// T-035: bit-identical no-regression against the pre-zoom mapper, view
// mapping, edge smoothing against an independent double-precision
// reference, and the overview.

#include "LegacySpectrumMapper.h"
#include "SpectrumZoomTestUtils.h"
#include "dsp/DisplayRange.h"
#include "dsp/SpectrumMapper.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    // ---- Legacy (HEAD) pipeline: standalone analyser + frozen mapper copy ----

    void legacyFrame (const SpectrumSettings& s, const std::vector<Samples>& rounds, SpectrumFrame& out)
    {
        SpectrumAnalyser analyser;
        SpectrumAnalyser::Settings initial;
        initial.sampleRate = s.sampleRate;
        initial.numMainChannels = s.numMainChannels;
        initial.numSidechainChannels = s.numSidechainChannels;
        analyser.prepare (initial);
        analyser.setSettings (s.toAnalyserSettings());

        for (const auto& smp : rounds)
        {
            // The engine drains its FIFO in chunks of 8192 frames.
            for (int off = 0; off < smp.size(); off += 8192)
            {
                const int n = std::min (8192, smp.size() - off);
                const float* main[2] = { smp.l.data() + off, smp.r.data() + off };
                const float* sc[2] = { smp.sl.data() + off, smp.sr.data() + off };
                analyser.pushBlock (main, s.numSidechainChannels > 0 ? sc : nullptr, n);
            }
        }

        LegacySpectrumMapper mapper;
        mapper.prepare();
        const int n = std::clamp (s.numPoints, 1, SpectrumFrame::maxPoints);
        out.numPoints = n;
        mapper.computeFrequencies (s, out.freqHz.data(), n);

        const int numBins = analyser.getNumBins();
        const double binHz = analyser.getSettings().sampleRate / static_cast<double> (analyser.getSettings().fftSize);
        mapper.mapCurve (analyser.getPrimaryPower(), numBins, binHz, s, out.freqHz.data(), n, out.primaryDb.data());
        out.isDual = analyser.isDual();
        if (out.isDual)
            mapper.mapCurve (analyser.getPrimaryBPower(), numBins, binHz, s, out.freqHz.data(), n, out.primaryBDb.data());
        out.hasSecondary = analyser.getSecondaryPower (false) != nullptr;
        if (out.hasSecondary)
        {
            mapper.mapCurve (analyser.getSecondaryPower (false), numBins, binHz, s, out.freqHz.data(), n, out.secondaryDb.data());
            if (out.isDual)
                mapper.mapCurve (analyser.getSecondaryPower (true), numBins, binHz, s, out.freqHz.data(), n, out.secondaryBDb.data());
        }
        out.hasSidechain = analyser.hasSidechain();
        if (out.hasSidechain)
            mapper.mapCurve (analyser.getSidechainPower(), numBins, binHz, s, out.freqHz.data(), n, out.sidechainDb.data());
    }

    bool legacyMatches (const SpectrumFrame& a, const SpectrumFrame& ref)
    {
        const int n = ref.numPoints;
        if (a.numPoints != n || a.isDual != ref.isDual || a.hasSecondary != ref.hasSecondary || a.hasSidechain != ref.hasSidechain)
            return false;
        if (! sameFloats (a.freqHz.data(), ref.freqHz.data(), n) || ! sameFloats (a.primaryDb.data(), ref.primaryDb.data(), n))
            return false;
        if (ref.isDual && ! sameFloats (a.primaryBDb.data(), ref.primaryBDb.data(), n))
            return false;
        if (ref.hasSecondary)
        {
            if (! sameFloats (a.secondaryDb.data(), ref.secondaryDb.data(), n))
                return false;
            if (ref.isDual && ! sameFloats (a.secondaryBDb.data(), ref.secondaryBDb.data(), n))
                return false;
        }
        if (ref.hasSidechain && ! sameFloats (a.sidechainDb.data(), ref.sidechainDb.data(), n))
            return false;
        return true;
    }

    /** Engine versus legacy over three publish rounds. Returns the number of rounds that mismatched. */
    int compareToLegacy (const SpectrumSettings& s)
    {
        Rig rig (s, 3);
        Source copy (3);
        std::vector<Samples> history;
        int mismatches = 0;

        for (int round = 0; round < 3; ++round)
        {
            const Samples smp = copy.next (std::max (19384, s.fftSize + s.fftSize / 2));
            history.push_back (smp);
            rig.push (smp);
            const bool published = rig.process();
            if (! published)
            {
                ++mismatches;
                continue;
            }
            auto frame = rig.read();
            auto ref = newFrame();
            legacyFrame (s, history, *ref);
            if (frame == nullptr || ! legacyMatches (*frame, *ref))
                ++mismatches;
        }
        return mismatches;
    }

    // ---- A realistic bin spectrum: pink noise plus three tones ----

    struct BinSpectrum
    {
        std::vector<float> power;
        double binHz = 1.0;
        int numBins = 0;
    };

    BinSpectrum makeSpectrum (int fftSize, double sampleRate = 48000.0)
    {
        SpectrumAnalyser analyser;
        SpectrumAnalyser::Settings st;
        st.sampleRate = sampleRate;
        st.fftSize = fftSize;
        st.numMainChannels = 1;
        st.channelMode = ChannelMode::mid;
        st.avgTimeMs = 1000.0f;
        st.secondary = SecondaryMode::off;
        analyser.prepare (st);
        analyser.setSettings (st);

        Source src (7);
        const Samples smp = src.next (static_cast<int> (3.0 * sampleRate));
        std::vector<float> mono (smp.l);
        pushMono (analyser, mono, 4096);

        BinSpectrum out;
        out.numBins = analyser.getNumBins();
        out.binHz = sampleRate / static_cast<double> (fftSize);
        out.power.assign (analyser.getPrimaryPower(), analyser.getPrimaryPower() + out.numBins);
        return out;
    }

    // ---- Independent double-precision reference mapper (T-002 section 2) ----
    // Maps `count` visible points starting at point 0 = exp2 (logLo), `padLow`
    // / `padHigh` extra aligned grid points around them, window half width
    // `half` points, boxcar truncated at the ends of the padded array.

    std::vector<double> referenceMap (const BinSpectrum& sp, double logLo, double step, int padLow, int count,
                                      int padHigh, int half, double slopeDbPerOct)
    {
        const int total = padLow + count + padHigh;
        const int lastBin = sp.numBins - 1;
        std::vector<float> raw (static_cast<size_t> (total));

        for (int k = 0; k < total; ++k)
        {
            const double e = static_cast<double> (k - padLow);
            const double f = std::exp2 (logLo + step * e);
            const double loEdge = std::exp2 (logLo + step * (e - 0.5));
            const double hiEdge = std::exp2 (logLo + step * (e + 0.5));
            float v;
            if (hiEdge - loEdge <= sp.binHz)
            {
                const double bin = f / sp.binHz;
                const double fl = std::floor (bin);
                const int b0 = std::clamp (static_cast<int> (fl), 0, lastBin);
                const int b1 = std::clamp (b0 + 1, 0, lastBin);
                const float frac = static_cast<float> (bin - fl);
                v = sp.power[static_cast<size_t> (b0)] + (sp.power[static_cast<size_t> (b1)] - sp.power[static_cast<size_t> (b0)]) * frac;
            }
            else
            {
                const int b0 = std::clamp (static_cast<int> (std::lround (loEdge / sp.binHz)), 0, lastBin);
                int b1 = std::clamp (static_cast<int> (std::lround (hiEdge / sp.binHz)), 0, lastBin);
                b1 = std::max (b1, b0);
                float peak = sp.power[static_cast<size_t> (b0)];
                for (int b = b0 + 1; b <= b1; ++b)
                    peak = std::max (peak, sp.power[static_cast<size_t> (b)]);
                v = peak;
            }
            raw[static_cast<size_t> (k)] = v;
        }

        std::vector<double> db (static_cast<size_t> (count));
        for (int i = 0; i < count; ++i)
        {
            const int c = padLow + i;
            double p;
            if (half > 0)
            {
                const int lo = std::max (0, c - half);
                const int hi = std::min (total - 1, c + half);
                double sum = 0.0;
                for (int k = lo; k <= hi; ++k)
                    sum += static_cast<double> (raw[static_cast<size_t> (k)]);
                p = sum / static_cast<double> (hi - lo + 1);
            }
            else
            {
                p = static_cast<double> (raw[static_cast<size_t> (c)]);
            }
            p = std::max (1.0e-37, p);
            const double f = std::exp2 (logLo + step * static_cast<double> (i));
            db[static_cast<size_t> (i)] = std::max (-200.0, 10.0 * std::log10 (p) + slopeDbPerOct * std::log2 (f / 1000.0));
        }
        return db;
    }

    double smoothingOct (SmoothingMode m)
    {
        switch (m)
        {
            case SmoothingMode::off: return 0.0;
            case SmoothingMode::oct1_48: return 1.0 / 48.0;
            case SmoothingMode::oct1_24: return 1.0 / 24.0;
            case SmoothingMode::oct1_12: return 1.0 / 12.0;
            case SmoothingMode::oct1_6: return 1.0 / 6.0;
            case SmoothingMode::oct1_3: return 1.0 / 3.0;
        }
        return 0.0;
    }
}

// ---------------------------------------------------------------------
// AC2: no regression when not zoomed. Engine versus the frozen HEAD mapper
// in every channel mode and smoothing mode.
// ---------------------------------------------------------------------
TEST_CASE ("No regression: unzoomed engine output is bit-identical to the HEAD mapper, every channel and smoothing mode", "[T-035][regression][AC2]")
{
    const auto mode = GENERATE (ChannelMode::mid, ChannelMode::side, ChannelMode::left, ChannelMode::right,
                                ChannelMode::lrPowerMean, ChannelMode::lrDual, ChannelMode::msDual);
    const auto smoothing = GENERATE (SmoothingMode::off, SmoothingMode::oct1_48, SmoothingMode::oct1_24,
                                     SmoothingMode::oct1_12, SmoothingMode::oct1_6, SmoothingMode::oct1_3);

    SpectrumSettings s = makeSettings (mode, SecondaryMode::allTimeAverage, 2);
    s.smoothing = smoothing;
    s.numPoints = 2048;

    INFO ("mode " << static_cast<int> (mode) << " smoothing " << static_cast<int> (smoothing));
    CHECK (compareToLegacy (s) == 0);
}

TEST_CASE ("No regression: sample rate, FFT size, range, point count and slope sweep", "[T-035][regression][AC2]")
{
    struct Case { double sr; int fft; float lo; float hi; int points; SmoothingMode sm; float slope; SecondaryMode sec; };
    const auto c = GENERATE (Case { 44100.0, 1024, 20.0f, 20000.0f, 2048, SmoothingMode::oct1_3, 0.0f, SecondaryMode::realTimeMax },
                             Case { 44100.0, 4096, 20.0f, 40000.0f, 777, SmoothingMode::oct1_6, 3.0f, SecondaryMode::allTimeMax }, // Nyquist-limited
                             Case { 48000.0, 512, 100.0f, 8000.0f, 513, SmoothingMode::oct1_12, -4.5f, SecondaryMode::off },
                             Case { 48000.0, 65536, 20.0f, 20000.0f, 2048, SmoothingMode::oct1_24, 4.5f, SecondaryMode::allTimeAverage },
                             Case { 96000.0, 4096, 5000.0f, 90000.0f, 64, SmoothingMode::oct1_48, 4.5f, SecondaryMode::realTimeMax }, // freqLow above fHi/2
                             Case { 48000.0, 4096, 20.0f, 20000.0f, 1, SmoothingMode::oct1_3, 4.5f, SecondaryMode::realTimeMax },
                             Case { 48000.0, 4096, 20.0f, 20000.0f, 2, SmoothingMode::oct1_3, 4.5f, SecondaryMode::realTimeMax });

    SpectrumSettings s = makeSettings (ChannelMode::lrDual, c.sec, 2);
    s.sampleRate = c.sr;
    s.fftSize = c.fft;
    s.freqLow = c.lo;
    s.freqHigh = c.hi;
    s.numPoints = c.points;
    s.smoothing = c.sm;
    s.slopeDbPerOct = c.slope;

    INFO ("sr " << c.sr << " fft " << c.fft << " points " << c.points);
    CHECK (compareToLegacy (s) == 0);
}

// ---------------------------------------------------------------------
// AC3: view mapping.
// ---------------------------------------------------------------------
TEST_CASE ("View mapping: ends, monotonic points, frame view equals clamped view", "[T-035][view][AC3]")
{
    struct View { float lo, hi; };
    const auto v = GENERATE (View { 0.0f, 0.0f }, View { 100.0f, 400.0f }, View { 1000.0f, 1000.0f * 1.1225f },
                             View { 1000.0f, 1000.5f }, View { 20.0f, 25.0f }, View { 15000.0f, 20000.0f },
                             View { 5.0f, 90.0f }, View { 15000.0f, 90000.0f }, View { 800.0f, 200.0f },
                             View { 10.0f, 100000.0f }, View { 50.0f, 0.0f });
    const int points = GENERATE (2048, 257, 2);

    SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    s.viewFreqLow = v.lo;
    s.viewFreqHigh = v.hi;
    s.numPoints = points;

    const auto ranges = SpectrumMapper::computeRanges (s);
    const auto expected = DisplayRange::clampViewFrequency (v.lo, v.hi, ranges.homeLow, ranges.homeHigh);
    CHECK (ranges.viewLow == expected.low);
    CHECK (ranges.viewHigh == expected.high);

    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (2048);
    mapper.computeFrequencies (s, freq.data(), points);

    INFO ("view " << v.lo << ".." << v.hi << " points " << points << " clamped " << expected.low << ".." << expected.high);
    CHECK (std::abs (freq[0] / expected.low - 1.0) < 1e-4);
    CHECK (std::abs (freq[static_cast<size_t> (points - 1)] / expected.high - 1.0) < 1e-4);
    for (int i = 1; i < points; ++i)
        REQUIRE (freq[static_cast<size_t> (i)] > freq[static_cast<size_t> (i - 1)]);
}

TEST_CASE ("View mapping through the engine: frame carries the clamped view", "[T-035][view][AC3]")
{
    SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    Rig rig (s);
    rig.run (30000);
    auto first = rig.read();
    REQUIRE (first != nullptr);
    CHECK (first->viewFreqLow == 20.0);
    CHECK (first->viewFreqHigh == 20000.0);

    struct View { float lo, hi; };
    for (const View v : { View { 100.0f, 400.0f }, View { 1000.0f, 1000.5f }, View { 10.0f, 30.0f }, View { 15000.0f, 90000.0f }, View { 0.0f, 0.0f } })
    {
        rig.s.viewFreqLow = v.lo;
        rig.s.viewFreqHigh = v.hi;
        REQUIRE (rig.process());
        auto f = rig.read();
        REQUIRE (f != nullptr);

        const auto expected = DisplayRange::clampViewFrequency (v.lo, v.hi, 20.0, 20000.0);
        INFO ("view " << v.lo << ".." << v.hi);
        CHECK (f->viewFreqLow == expected.low);
        CHECK (f->viewFreqHigh == expected.high);
        CHECK (std::abs (f->freqHz[0] / expected.low - 1.0) < 1e-4);
        CHECK (std::abs (f->freqHz[static_cast<size_t> (f->numPoints - 1)] / expected.high - 1.0) < 1e-4);
        for (int i = 1; i < f->numPoints; ++i)
            REQUIRE (f->freqHz[static_cast<size_t> (i)] > f->freqHz[static_cast<size_t> (i - 1)]);
    }
}

// ---------------------------------------------------------------------
// AC4: edge smoothing against an aligned, widened reference.
// ---------------------------------------------------------------------
TEST_CASE ("Edge smoothing: first and last 5 % match a view widened 2x per side (aligned grid), within 0.05 dB", "[T-035][edge][AC4]")
{
    const int fftSize = GENERATE (1024, 8192);
    const auto smoothing = GENERATE (SmoothingMode::off, SmoothingMode::oct1_48, SmoothingMode::oct1_24,
                                     SmoothingMode::oct1_12, SmoothingMode::oct1_6, SmoothingMode::oct1_3);
    const int points = GENERATE (512, 2048);
    const double spanOct = GENERATE (1.0 / 6.0, 0.5, 1.0, 3.0, 1000.0 /* full home range */);
    const double position = GENERATE (0.0, 0.37, 1.0);

    static const BinSpectrum spectra[2] = { makeSpectrum (1024), makeSpectrum (8192) };
    const BinSpectrum& sp = spectra[fftSize == 1024 ? 0 : 1];

    SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    s.smoothing = smoothing;
    s.numPoints = points;
    s.slopeDbPerOct = 4.5f;

    const double homeSpan = std::log2 (20000.0 / 20.0);
    const double span = std::min (spanOct, homeSpan);
    if (span >= homeSpan)
    {
        s.viewFreqLow = 20.0f;
        s.viewFreqHigh = 20000.0f;
    }
    else
    {
        const double loOct = position * (homeSpan - span);
        s.viewFreqLow = static_cast<float> (20.0 * std::exp2 (loOct));
        s.viewFreqHigh = static_cast<float> (20.0 * std::exp2 (loOct + span));
    }

    const auto r = SpectrumMapper::computeRanges (s);
    const double logLo = std::log2 (r.viewLow);
    const double logHi = std::log2 (r.viewHigh);
    const double step = (logHi - logLo) / static_cast<double> (points - 1);

    // Reference: widened by 2x the view span in octaves on each side, in whole
    // grid steps, clamped to the effective home range.
    const int wide = 2 * (points - 1);
    const int padLow = std::clamp (static_cast<int> (std::floor ((logLo - std::log2 (r.homeLow)) / step + 1e-9)), 0, wide);
    const int padHigh = std::clamp (static_cast<int> (std::floor ((std::log2 (r.homeHigh) - logHi) / step + 1e-9)), 0, wide);
    const int half = std::clamp (static_cast<int> (std::lround ((smoothingOct (smoothing) * 0.5) / step)), 0, points - 1);
    const auto ref = referenceMap (sp, logLo, step, padLow, points, padHigh, half, 4.5);

    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (2048), out (2048);
    mapper.computeFrequencies (s, freq.data(), points);
    mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, freq.data(), points, out.data());

    const int edge = static_cast<int> (std::ceil (0.05 * points));
    double worst = 0.0;
    for (int i = 0; i < points; ++i)
        if (i < edge || i >= points - edge)
            worst = std::max (worst, std::abs (static_cast<double> (out[static_cast<size_t> (i)]) - ref[static_cast<size_t> (i)]));

    INFO ("fft " << fftSize << " smoothing " << static_cast<int> (smoothing) << " points " << points << " span " << span
          << " pos " << position << " view " << r.viewLow << ".." << r.viewHigh << " worst edge diff " << worst);
    CHECK (worst <= 0.05);
}

TEST_CASE ("Edge smoothing: zoomed curve interior also tracks the reference; home-edge truncation unchanged", "[T-035][edge][AC4]")
{
    static const BinSpectrum sp = makeSpectrum (8192);

    SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    s.smoothing = SmoothingMode::oct1_3;
    s.numPoints = 1024;

    // A view touching the lower home edge: the low end keeps the truncated
    // window, the high end is padded.
    s.viewFreqLow = 20.0f;
    s.viewFreqHigh = 160.0f;
    const auto r = SpectrumMapper::computeRanges (s);
    CHECK (r.viewLow == 20.0);

    const double logLo = std::log2 (r.viewLow);
    const double step = (std::log2 (r.viewHigh) - logLo) / 1023.0;
    const int half = static_cast<int> (std::lround ((1.0 / 6.0) / step));
    const auto ref = referenceMap (sp, logLo, step, 0, 1024, half, half, 4.5);

    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (2048), out (2048);
    mapper.computeFrequencies (s, freq.data(), 1024);
    mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, freq.data(), 1024, out.data());

    double worst = 0.0;
    for (int i = 0; i < 1024; ++i)
        worst = std::max (worst, std::abs (static_cast<double> (out[static_cast<size_t> (i)]) - ref[static_cast<size_t> (i)]));
    CHECK (worst <= 0.05);
}

TEST_CASE ("Edge smoothing: a zoomed curve equals the Fit curve at the same frequencies (dense grid, 1/3 octave)", "[T-035][edge][AC4]")
{
    static const BinSpectrum sp = makeSpectrum (8192);

    SpectrumSettings fit = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    fit.smoothing = SmoothingMode::oct1_3;
    fit.numPoints = 2048;

    SpectrumSettings zoom = fit;
    zoom.viewFreqLow = 300.0f;
    zoom.viewFreqHigh = 1200.0f;
    zoom.numPoints = 2048;

    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> fFit (2048), dFit (2048), fZoom (2048), dZoom (2048);
    mapper.computeFrequencies (fit, fFit.data(), 2048);
    mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, fit, fFit.data(), 2048, dFit.data());
    mapper.computeFrequencies (zoom, fZoom.data(), 2048);
    mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, zoom, fZoom.data(), 2048, dZoom.data());

    // The Fit curve is smoother per point; compare on a coarse level: both are
    // 1/3-octave smoothed, so their dB values at the same frequency should agree
    // within a few tenths of a dB away from narrow tones.
    double sumAbs = 0.0;
    int count = 0;
    for (int i = 0; i < 2048; ++i)
    {
        const float f = fZoom[static_cast<size_t> (i)];
        int best = 0;
        double bd = 1e30;
        for (int k = 0; k < 2048; ++k)
        {
            const double d = std::abs (std::log2 (static_cast<double> (fFit[static_cast<size_t> (k)]) / f));
            if (d < bd) { bd = d; best = k; }
        }
        sumAbs += std::abs (static_cast<double> (dFit[static_cast<size_t> (best)]) - dZoom[static_cast<size_t> (i)]);
        ++count;
    }
    CHECK (sumAbs / count < 0.25);
}

// ---------------------------------------------------------------------
// Overview (mapper level).
// ---------------------------------------------------------------------
TEST_CASE ("Overview: 256 points over the effective home range, independent of the view, matches the full-range curve", "[T-035][overview][AC8]")
{
    static const BinSpectrum sp = makeSpectrum (8192);
    const auto smoothing = GENERATE (SmoothingMode::off, SmoothingMode::oct1_12, SmoothingMode::oct1_3);

    SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    s.smoothing = smoothing;
    s.numPoints = 256;
    s.freqLow = 30.0f;
    s.freqHigh = 16000.0f;

    SpectrumMapper mapper;
    mapper.prepare();

    std::vector<float> fFull (2048), dFull (2048);
    mapper.computeFrequencies (s, fFull.data(), 256);
    mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, fFull.data(), 256, dFull.data());

    SpectrumSettings zoomed = s;
    zoomed.viewFreqLow = 200.0f;
    zoomed.viewFreqHigh = 260.0f;
    zoomed.numPoints = 2048;
    std::vector<float> fz (2048), dz (2048);
    mapper.computeFrequencies (zoomed, fz.data(), 2048);
    mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, zoomed, fz.data(), 2048, dz.data());

    std::vector<float> oFreq (256), oDb (256);
    const int n = mapper.mapOverview (sp.power.data(), sp.numBins, sp.binHz, zoomed, oFreq.data(), oDb.data());
    REQUIRE (n == 256);

    CHECK (std::abs (oFreq[0] / 30.0 - 1.0) < 1e-4);
    CHECK (std::abs (oFreq[255] / 16000.0 - 1.0) < 1e-4);
    for (int i = 1; i < n; ++i)
        REQUIRE (oFreq[static_cast<size_t> (i)] > oFreq[static_cast<size_t> (i - 1)]);

    double worst = 0.0;
    for (int i = 0; i < n; ++i)
    {
        CHECK (oFreq[static_cast<size_t> (i)] == fFull[static_cast<size_t> (i)]);
        worst = std::max (worst, std::abs (static_cast<double> (oDb[static_cast<size_t> (i)]) - dFull[static_cast<size_t> (i)]));
    }
    CHECK (worst <= 0.1);
}

TEST_CASE ("Overview: overviewPoints setting is honoured and clamped", "[T-035][overview][AC8]")
{
    static const BinSpectrum sp = makeSpectrum (4096);
    SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::off, 0);
    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> f (256), d (256);

    s.overviewPoints = 100;
    CHECK (mapper.mapOverview (sp.power.data(), sp.numBins, sp.binHz, s, f.data(), d.data()) == 100);
    s.overviewPoints = 1000;
    CHECK (mapper.mapOverview (sp.power.data(), sp.numBins, sp.binHz, s, f.data(), d.data()) == 256);
    s.overviewPoints = 0;
    CHECK (mapper.mapOverview (sp.power.data(), sp.numBins, sp.binHz, s, f.data(), d.data()) == 1);
}
