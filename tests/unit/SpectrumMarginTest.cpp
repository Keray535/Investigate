// T-039: pan margin and bin spacing in the spectrum frame.
//
// Mapper level (synthetic bin spectra): visible points bit-identical to the
// frozen T-035 mapper copy, margin geometry, margin values against a frame
// mapped for a shifted view, T-035 API unchanged. Engine level: Fit frames,
// views, clamp of numPoints, binHz while live and frozen, allocation, timing.

#include "AllocGuard.h"
#include "LegacySpectrumMapper.h"
#include "SpectrumZoomTestUtils.h"
#include "T035SpectrumMapper.h"
#include "dsp/DisplayRange.h"
#include "dsp/SpectrumMapper.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace investigate;
using namespace investigate::test;

// ---------------------------------------------------------------------
// Frame constants (compile time).
// ---------------------------------------------------------------------
static_assert (SpectrumFrame::maxVisiblePoints == 2048);
static_assert (SpectrumFrame::maxPoints == 3072);
static_assert (SpectrumFrame::maxPoints == SpectrumFrame::maxVisiblePoints + 2 * SpectrumFrame::maxMarginPoints);
static_assert (std::is_trivially_copyable_v<SpectrumFrame>);
static_assert (std::is_same_v<decltype (SpectrumFrame::binHz), double>);

namespace
{
    // ---- Synthetic smooth bin spectrum: slope plus broad bumps (>= 8 bins wide) ----

    struct Spectrum
    {
        std::vector<float> power;
        double binHz = 1.0;
        int numBins = 0;
    };

    Spectrum makeSyntheticSpectrum (double sampleRate, int fftSize)
    {
        Spectrum sp;
        sp.numBins = fftSize / 2 + 1;
        sp.binHz = sampleRate / static_cast<double> (fftSize);
        sp.power.resize (static_cast<size_t> (sp.numBins));
        const double centres[4] = { 110.0, 440.0, 1800.0, 7000.0 };
        for (int b = 0; b < sp.numBins; ++b)
        {
            const double f = std::max (1.0, b * sp.binHz);
            double db = -50.0 - 3.0 * std::log2 (f / 100.0);
            for (const double c : centres)
            {
                // Width: at least 10 bins and at least 6 % of the centre frequency.
                const double sigma = std::max (10.0 * sp.binHz, 0.06 * c);
                db += 25.0 * std::exp (-0.5 * (f - c) * (f - c) / (sigma * sigma));
            }
            sp.power[static_cast<size_t> (b)] = static_cast<float> (std::pow (10.0, db / 10.0));
        }
        return sp;
    }

    struct View
    {
        float lo = 0.0f, hi = 0.0f;
        std::string name;
    };

    /** Zoomed views: spans from 1/6 octave to the home range, at the low edge,
        the high edge and inside, plus Fit. */
    std::vector<View> makeViews (double homeLo, double homeHi)
    {
        std::vector<View> views;
        views.push_back ({ 0.0f, 0.0f, "fit" });
        const double homeSpan = std::log2 (homeHi / homeLo);
        const double spans[] = { 1.0 / 6.0, 0.25, 0.5, 1.0, 2.0, 4.0, 6.0, 8.0, homeSpan };
        const double positions[] = { 0.0, 0.37, 1.0 };
        for (const double span : spans)
            for (const double pos : positions)
            {
                const double lo = homeLo * std::exp2 (pos * std::max (0.0, homeSpan - span));
                std::ostringstream n;
                n << "span " << span << " pos " << pos;
                views.push_back ({ static_cast<float> (lo), static_cast<float> (lo * std::exp2 (std::min (span, homeSpan))), n.str() });
            }
        return views;
    }

    bool bitEqual (const float* a, const float* b, int n)
    {
        return std::memcmp (a, b, static_cast<size_t> (n) * sizeof (float)) == 0;
    }

    SpectrumSettings mapperSettings (double sampleRate, SmoothingMode smoothing, int numPoints, float homeLo = 20.0f, float homeHi = 20000.0f)
    {
        SpectrumSettings s;
        s.sampleRate = sampleRate;
        s.smoothing = smoothing;
        s.numPoints = numPoints;
        s.freqLow = homeLo;
        s.freqHigh = homeHi;
        s.slopeDbPerOct = 4.5f;
        return s;
    }

    constexpr SmoothingMode kSmoothingModes[] = { SmoothingMode::off, SmoothingMode::oct1_48, SmoothingMode::oct1_24,
                                                  SmoothingMode::oct1_12, SmoothingMode::oct1_6, SmoothingMode::oct1_3 };

    int smoothingIndex (SmoothingMode m) { return static_cast<int> (m); }

    double smoothingOctaves (SmoothingMode m)
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

    struct Failure
    {
        int count = 0;
        std::string first;
        void note (const std::string& what)
        {
            if (count++ == 0)
                first = what;
        }
    };
}

// ---------------------------------------------------------------------
// Frame struct.
// ---------------------------------------------------------------------
TEST_CASE ("Frame: capacity, defaults and margin point helper", "[T-039][frame]")
{
    CHECK (SpectrumFrame::maxVisiblePoints == 2048);
    CHECK (SpectrumFrame::maxMarginPoints == 512);
    CHECK (SpectrumFrame::maxPoints == 3072);

    const SpectrumFrame f;
    CHECK (f.numPoints == 0);
    CHECK (f.firstVisiblePoint == 0);
    CHECK (f.numVisiblePoints == 0);
    CHECK (f.binHz == 0.0);
    CHECK (f.freqHz.size() == 3072);
    CHECK (f.primaryDb.size() == 3072);
    CHECK (f.primaryBDb.size() == 3072);
    CHECK (f.secondaryDb.size() == 3072);
    CHECK (f.secondaryBDb.size() == 3072);
    CHECK (f.sidechainDb.size() == 3072);

    // spectrumMarginPointsFor is the integer form of lround (0.25 (N - 1)).
    int mismatches = 0;
    for (int n = 1; n <= SpectrumFrame::maxVisiblePoints; ++n)
        if (spectrumMarginPointsFor (n) != static_cast<int> (std::lround (0.25 * (n - 1))))
            ++mismatches;
    CHECK (mismatches == 0);
    CHECK (spectrumMarginPointsFor (2048) == 512);
    CHECK (SpectrumFrame::marginPointsFor (2048) == 512);
    CHECK (spectrumMarginPointsFor (1) == 0);
}

// ---------------------------------------------------------------------
// Mapper: the T-035 API is unchanged and visible-only; frame API keeps the
// visible points bit-identical to T-035 for the same view.
// ---------------------------------------------------------------------
TEST_CASE ("Mapper: T-035 API output is bit-identical to the T-035 mapper (visible only), also after the frame API", "[T-039][mapper][regression]")
{
    const double sampleRate = GENERATE (44100.0, 96000.0);
    const int fftSize = GENERATE (1024, 16384);
    const Spectrum sp = makeSyntheticSpectrum (sampleRate, fftSize);

    SpectrumMapper mapper;
    mapper.prepare();
    T035SpectrumMapper ref;
    ref.prepare();

    std::vector<float> fRef (SpectrumFrame::maxPoints), dRef (SpectrumFrame::maxPoints), fNew (SpectrumFrame::maxPoints),
        dNew (SpectrumFrame::maxPoints);
    Failure failures;
    int cases = 0;

    for (const int points : { 1, 2, 7, 257, 2048 })
        for (const auto smoothing : kSmoothingModes)
        {
            const auto s0 = mapperSettings (sampleRate, smoothing, points);
            for (const View& v : makeViews (20.0, 20000.0))
            {
                SpectrumSettings s = s0;
                s.viewFreqLow = v.lo;
                s.viewFreqHigh = v.hi;
                ++cases;

                ref.computeFrequencies (s, fRef.data(), points);
                ref.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, fRef.data(), points, dRef.data());

                // Frame API first (margin grid cached), then the T-035 API on the same view.
                mapper.computeFrameFrequencies (s, fNew.data(), points);
                mapper.mapCurveFrame (sp.power.data(), sp.numBins, sp.binHz, s, dNew.data());

                std::fill (fNew.begin(), fNew.end(), -1.0f);
                std::fill (dNew.begin(), dNew.end(), -1.0f);
                mapper.computeFrequencies (s, fNew.data(), points);
                mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, fNew.data(), points, dNew.data());

                const bool tailUntouched = fNew[static_cast<size_t> (points)] == -1.0f && dNew[static_cast<size_t> (points)] == -1.0f;
                if (! bitEqual (fRef.data(), fNew.data(), points) || ! bitEqual (dRef.data(), dNew.data(), points) || ! tailUntouched)
                    failures.note ("N " + std::to_string (points) + " smoothing " + std::to_string (smoothingIndex (smoothing)) + " " + v.name);
            }
        }
    INFO ("first failure: " << failures.first);
    CHECK (cases >= 5 * 6 * 20);
    CHECK (failures.count == 0);
}

TEST_CASE ("Mapper: frame API visible points are bit-identical to T-035 for 24+ views, every smoothing mode, several rates and home ranges", "[T-039][mapper][visible]")
{
    struct Home { float lo, hi; };
    const Home home = GENERATE (Home { 20.0f, 20000.0f }, Home { 50.0f, 8000.0f });
    const double sampleRate = GENERATE (44100.0, 48000.0, 192000.0);
    const int fftSize = GENERATE (512, 4096, 65536);
    const Spectrum sp = makeSyntheticSpectrum (sampleRate, fftSize);

    SpectrumMapper mapper;
    mapper.prepare();
    T035SpectrumMapper ref;
    ref.prepare();

    std::vector<float> fRef (SpectrumFrame::maxPoints), dRef (SpectrumFrame::maxPoints), fNew (SpectrumFrame::maxPoints),
        dNew (SpectrumFrame::maxPoints);
    Failure failures;
    int cases = 0;
    int withMargin = 0;

    for (const int points : { 2, 7, 64, 1000, 2048 })
        for (const auto smoothing : kSmoothingModes)
        {
            const auto s0 = mapperSettings (sampleRate, smoothing, points, home.lo, home.hi);
            for (const View& v : makeViews (home.lo, home.hi))
            {
                SpectrumSettings s = s0;
                s.viewFreqLow = v.lo;
                s.viewFreqHigh = v.hi;
                ++cases;

                ref.computeFrequencies (s, fRef.data(), points);
                ref.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, fRef.data(), points, dRef.data());

                const auto layout = mapper.computeFrameFrequencies (s, fNew.data(), points);
                mapper.mapCurveFrame (sp.power.data(), sp.numBins, sp.binHz, s, dNew.data());
                if (layout.numPoints > layout.numVisible)
                    ++withMargin;

                const std::string what = "N " + std::to_string (points) + " smoothing " + std::to_string (smoothingIndex (smoothing)) + " " + v.name;
                if (layout.numVisible != points)
                    failures.note (what + " numVisible " + std::to_string (layout.numVisible));
                else if (! bitEqual (fRef.data(), fNew.data() + layout.firstVisible, points)
                         || ! bitEqual (dRef.data(), dNew.data() + layout.firstVisible, points))
                    failures.note (what);
            }
        }
    INFO ("first failure: " << failures.first);
    CHECK (cases >= 5 * 6 * 25);
    CHECK (withMargin > cases / 2); // the sweep really produced margins
    CHECK (failures.count == 0);
}

TEST_CASE ("Mapper: Fit gives no margin, firstVisible 0, and a frame bit-identical to the pre-zoom mapper", "[T-039][mapper][regression]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 192000.0);
    const int fftSize = GENERATE (512, 8192);
    const Spectrum sp = makeSyntheticSpectrum (sampleRate, fftSize);

    SpectrumMapper mapper;
    mapper.prepare();
    LegacySpectrumMapper legacy;
    legacy.prepare();
    std::vector<float> fRef (2048), dRef (2048), fNew (SpectrumFrame::maxPoints), dNew (SpectrumFrame::maxPoints);

    Failure failures;
    for (const int points : { 1, 2, 64, 1024, 2048 })
        for (const auto smoothing : kSmoothingModes)
        {
            const auto s = mapperSettings (sampleRate, smoothing, points);
            legacy.computeFrequencies (s, fRef.data(), points);
            legacy.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, fRef.data(), points, dRef.data());
            const auto layout = mapper.computeFrameFrequencies (s, fNew.data(), points);
            mapper.mapCurveFrame (sp.power.data(), sp.numBins, sp.binHz, s, dNew.data());
            const std::string what = "N " + std::to_string (points) + " smoothing " + std::to_string (smoothingIndex (smoothing));
            if (layout.firstVisible != 0 || layout.numVisible != points || layout.numPoints != points)
                failures.note (what + " layout");
            else if (! bitEqual (fRef.data(), fNew.data(), points) || ! bitEqual (dRef.data(), dNew.data(), points))
                failures.note (what + " values");
        }
    INFO ("first failure: " << failures.first);
    CHECK (failures.count == 0);

    // An explicit view equal to the home range is also Fit: no margin.
    SpectrumSettings s = mapperSettings (sampleRate, SmoothingMode::oct1_12, 512);
    s.viewFreqLow = 20.0f;
    s.viewFreqHigh = 20000.0f;
    const auto layout = mapper.computeFrameFrequencies (s, fNew.data(), 512);
    CHECK (layout.firstVisible == 0);
    CHECK (layout.numPoints == 512);
}

// ---------------------------------------------------------------------
// Margin geometry.
// ---------------------------------------------------------------------
TEST_CASE ("Margin geometry: span, step, home range, ordering and capacity", "[T-039][mapper][geometry]")
{
    struct Home { float lo, hi; };
    const Home home = GENERATE (Home { 20.0f, 20000.0f }, Home { 100.0f, 5000.0f });
    const double sampleRate = GENERATE (44100.0, 192000.0);

    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (SpectrumFrame::maxPoints);
    Failure failures;
    int zoomed = 0, lowMargins = 0, highMargins = 0, clippedLow = 0, clippedHigh = 0;

    for (const int points : { 2, 3, 7, 64, 257, 1000, 2047, 2048 })
        for (const auto smoothing : { SmoothingMode::off, SmoothingMode::oct1_3 })
            for (const View& v : makeViews (home.lo, home.hi))
            {
                SpectrumSettings s = mapperSettings (sampleRate, smoothing, points, home.lo, home.hi);
                s.viewFreqLow = v.lo;
                s.viewFreqHigh = v.hi;
                const auto layout = mapper.computeFrameFrequencies (s, freq.data(), points);
                const auto r = SpectrumMapper::computeRanges (s);
                const std::string what = "N " + std::to_string (points) + " " + v.name;

                const int m = spectrumMarginPointsFor (points);
                const int mLo = layout.firstVisible;
                const int mHi = layout.numPoints - layout.firstVisible - layout.numVisible;
                const double logLo = std::log2 (r.viewLow), logHi = std::log2 (r.viewHigh);
                const double span = logHi - logLo;
                const double step = points > 1 ? span / static_cast<double> (points - 1) : 0.0;
                const double logHomeLo = std::log2 (r.homeLow), logHomeHi = std::log2 (r.homeHigh);

                if (layout.numPoints > SpectrumFrame::maxPoints || layout.numPoints < 1)
                    failures.note (what + " numPoints " + std::to_string (layout.numPoints));
                if (layout.numVisible != points || mLo < 0 || mHi < 0 || mLo > m || mHi > m)
                    failures.note (what + " layout " + std::to_string (mLo) + "/" + std::to_string (mHi));
                if (layout.numPoints != mLo + layout.numVisible + mHi)
                    failures.note (what + " layout sum");

                if (r.viewLow > r.homeLow * 1.0000001 || r.viewHigh < r.homeHigh * 0.9999999)
                    ++zoomed;
                lowMargins += mLo > 0;
                highMargins += mHi > 0;
                clippedLow += (mLo < m);
                clippedHigh += (mHi < m);

                // Strictly increasing.
                for (int i = 1; i < layout.numPoints; ++i)
                    if (! (freq[static_cast<size_t> (i)] > freq[static_cast<size_t> (i - 1)]))
                    {
                        failures.note (what + " not increasing at " + std::to_string (i));
                        break;
                    }

                if (points < 2)
                    continue;

                // Step: every point is the same grid, 2^(logLo + step k) rounded to float, k = -mLo ..
                // (compared in the double domain; a float freqHz cannot show 1e-9).
                int gridMismatch = 0;
                for (int i = 0; i < layout.numPoints; ++i)
                {
                    const int k = i - mLo;
                    const float expected = static_cast<float> (std::exp2 (logLo + step * static_cast<double> (k)));
                    const double rel = std::abs (static_cast<double> (freq[static_cast<size_t> (i)]) / static_cast<double> (expected) - 1.0);
                    if (rel > 1.2e-7) // one float ulp
                        ++gridMismatch;
                }
                if (gridMismatch != 0)
                    failures.note (what + " grid mismatches " + std::to_string (gridMismatch));

                // No point outside the effective home range (float rounding: one ulp).
                const double f0 = freq[0], fn = freq[static_cast<size_t> (layout.numPoints - 1)];
                if (f0 < r.homeLow * (1.0 - 1.2e-7) || fn > r.homeHigh * (1.0 + 1.2e-7))
                    failures.note (what + " outside home range");

                // Span of the published points within one step of the target.
                const double targetLo = std::max (logHomeLo, logLo - 0.25 * span);
                const double targetHi = std::min (logHomeHi, logHi + 0.25 * span);
                if (std::abs (std::log2 (f0) - targetLo) > step * (1.0 + 1e-6) + 1e-6
                    || std::abs (std::log2 (fn) - targetHi) > step * (1.0 + 1e-6) + 1e-6)
                    failures.note (what + " span: low " + std::to_string (std::log2 (f0) - targetLo) + " high "
                                   + std::to_string (std::log2 (fn) - targetHi) + " step " + std::to_string (step));

                // Margin count: the full m unless the home edge clips it, then maximal.
                const double distLo = (logLo - logHomeLo) / step, distHi = (logHomeHi - logHi) / step;
                if (distLo >= m + 1e-6 && mLo != m)
                    failures.note (what + " unclipped low margin " + std::to_string (mLo) + " != " + std::to_string (m));
                if (distHi >= m + 1e-6 && mHi != m)
                    failures.note (what + " unclipped high margin " + std::to_string (mHi) + " != " + std::to_string (m));
                if (mLo < m && std::abs (distLo - mLo) > 1.0 + 1e-6)
                    failures.note (what + " low margin not maximal: " + std::to_string (mLo) + " dist " + std::to_string (distLo));
                if (mHi < m && std::abs (distHi - mHi) > 1.0 + 1e-6)
                    failures.note (what + " high margin not maximal: " + std::to_string (mHi) + " dist " + std::to_string (distHi));
                // A view on a home edge has no margin on that side.
                if (distLo < 1e-6 && mLo != 0)
                    failures.note (what + " margin at home low edge");
                if (distHi < 1e-6 && mHi != 0)
                    failures.note (what + " margin at home high edge");
            }

    INFO ("first failure: " << failures.first);
    CHECK (failures.count == 0);
    CHECK (zoomed > 100);
    CHECK (lowMargins > 50);
    CHECK (highMargins > 50);
    CHECK (clippedLow > 20);
    CHECK (clippedHigh > 20);
}

TEST_CASE ("Margin geometry: N = 2048 inside the home range publishes exactly 3072 points", "[T-039][mapper][geometry]")
{
    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (SpectrumFrame::maxPoints);
    SpectrumSettings s = mapperSettings (48000.0, SmoothingMode::oct1_3, 2048);
    s.viewFreqLow = 500.0f;
    s.viewFreqHigh = 1000.0f;
    const auto layout = mapper.computeFrameFrequencies (s, freq.data(), 2048);
    CHECK (layout.numPoints == 3072);
    CHECK (layout.firstVisible == 512);
    CHECK (layout.numVisible == 2048);
}

TEST_CASE ("Margin geometry: single point and two points", "[T-039][mapper][geometry]")
{
    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (SpectrumFrame::maxPoints), db (SpectrumFrame::maxPoints);
    const Spectrum sp = makeSyntheticSpectrum (48000.0, 4096);
    for (const int points : { 1, 2, 3 })
    {
        SpectrumSettings s = mapperSettings (48000.0, SmoothingMode::oct1_12, points);
        s.viewFreqLow = 500.0f;
        s.viewFreqHigh = 1000.0f;
        const auto layout = mapper.computeFrameFrequencies (s, freq.data(), points);
        mapper.mapCurveFrame (sp.power.data(), sp.numBins, sp.binHz, s, db.data());
        INFO ("points " << points);
        CHECK (layout.numVisible == points);
        CHECK (layout.numPoints == points + 2 * spectrumMarginPointsFor (points));
        for (int i = 0; i < layout.numPoints; ++i)
            REQUIRE (std::isfinite (db[static_cast<size_t> (i)]));
    }
}

// ---------------------------------------------------------------------
// Margin values against a frame mapped for a shifted, aligned view.
// ---------------------------------------------------------------------
TEST_CASE ("Margin values: margin points match a frame mapped for a shifted aligned view, within 0.05 dB", "[T-039][mapper][values]")
{
    const double sampleRate = GENERATE (48000.0, 96000.0);
    const int fftSize = GENERATE (1024, 8192);
    const Spectrum sp = makeSyntheticSpectrum (sampleRate, fftSize);

    SpectrumMapper mapper;
    mapper.prepare();
    T035SpectrumMapper ref;
    ref.prepare();
    std::vector<float> fNew (SpectrumFrame::maxPoints), dNew (SpectrumFrame::maxPoints), fRef (2048), dRef (2048);

    Failure failures;
    double worst = 0.0;
    long compared = 0;
    int skippedTies = 0;

    for (const int points : { 64, 512, 2048 })
        for (const auto smoothing : kSmoothingModes)
            for (const View& v : makeViews (20.0, 20000.0))
            {
                SpectrumSettings s = mapperSettings (sampleRate, smoothing, points);
                s.viewFreqLow = v.lo;
                s.viewFreqHigh = v.hi;
                const auto layout = mapper.computeFrameFrequencies (s, fNew.data(), points);
                mapper.mapCurveFrame (sp.power.data(), sp.numBins, sp.binHz, s, dNew.data());
                const int mLo = layout.firstVisible;
                const int mHi = layout.numPoints - layout.firstVisible - layout.numVisible;
                if (mLo == 0 && mHi == 0)
                    continue;

                const auto r = SpectrumMapper::computeRanges (s);
                const double logLo = std::log2 (r.viewLow), logHi = std::log2 (r.viewHigh);
                const double step = (logHi - logLo) / static_cast<double> (points - 1);

                // The smoothing half width is lround (octaves / 2 / step): when it sits on a rounding tie
                // (for example a 1 octave view at N = 64) the float-rounded shifted view can land on the other
                // side of the tie, which is a reference artefact, not a margin defect. Skip those cases.
                const double halfRaw = smoothingOctaves (smoothing) * 0.5 / step;
                if (smoothing != SmoothingMode::off && std::abs (halfRaw - std::floor (halfRaw) - 0.5) < 2e-3)
                {
                    ++skippedTies;
                    continue;
                }

                auto mapShifted = [&] (int shiftSteps)
                {
                    SpectrumSettings t = s;
                    t.viewFreqLow = static_cast<float> (std::exp2 (logLo + step * shiftSteps));
                    t.viewFreqHigh = static_cast<float> (std::exp2 (logHi + step * shiftSteps));
                    ref.computeFrequencies (t, fRef.data(), points);
                    ref.mapCurve (sp.power.data(), sp.numBins, sp.binHz, t, fRef.data(), points, dRef.data());
                };

                if (mLo > 0)
                {
                    mapShifted (-mLo); // margin low points are the first mLo points of this view
                    for (int j = 0; j < mLo; ++j)
                    {
                        const double d = std::abs (static_cast<double> (dNew[static_cast<size_t> (j)]) - dRef[static_cast<size_t> (j)]);
                        worst = std::max (worst, d);
                        ++compared;
                        if (d > 0.05)
                            failures.note ("low N " + std::to_string (points) + " sm " + std::to_string (smoothingIndex (smoothing)) + " "
                                           + v.name + " j " + std::to_string (j) + " diff " + std::to_string (d));
                    }
                }
                if (mHi > 0)
                {
                    mapShifted (mHi); // margin high points are the last mHi points of this view
                    for (int j = 0; j < mHi; ++j)
                    {
                        const double d = std::abs (static_cast<double> (dNew[static_cast<size_t> (layout.firstVisible + points + j)])
                                                   - dRef[static_cast<size_t> (points - mHi + j)]);
                        worst = std::max (worst, d);
                        ++compared;
                        if (d > 0.05)
                            failures.note ("high N " + std::to_string (points) + " sm " + std::to_string (smoothingIndex (smoothing)) + " "
                                           + v.name + " j " + std::to_string (j) + " diff " + std::to_string (d));
                    }
                }
            }
    INFO ("first failure: " << failures.first);
    WARN ("worst margin deviation " << worst << " dB over " << compared << " points, " << skippedTies << " half-width ties skipped");
    CHECK (compared > 100000);
    CHECK (failures.count == 0);
}

// ---------------------------------------------------------------------
// Engine level.
// ---------------------------------------------------------------------
namespace
{
    /** A standalone analyser fed exactly like the engine (chunks of 8192), as the T-035 reference source. */
    struct RefAnalyser
    {
        RefAnalyser (const SpectrumSettings& s, const std::vector<Samples>& history)
        {
            SpectrumAnalyser::Settings initial;
            initial.sampleRate = s.sampleRate;
            initial.numMainChannels = s.numMainChannels;
            initial.numSidechainChannels = s.numSidechainChannels;
            analyser.prepare (initial);
            analyser.setSettings (s.toAnalyserSettings());
            for (const auto& smp : history)
                for (int off = 0; off < smp.size(); off += 8192)
                {
                    const int n = std::min (8192, smp.size() - off);
                    const float* main[2] = { smp.l.data() + off, smp.r.data() + off };
                    const float* sc[2] = { smp.sl.data() + off, smp.sr.data() + off };
                    analyser.pushBlock (main, s.numSidechainChannels > 0 ? sc : nullptr, n);
                }
        }

        SpectrumAnalyser analyser;
    };

    /** Maps the analyser state with the T-035 mapper for `s` and compares with
        the visible part of `f` (and the overview). Returns an error text, empty when equal. */
    std::string compareVisibleToT035 (RefAnalyser& ra, const SpectrumSettings& s, const SpectrumFrame& f, T035SpectrumMapper& mapper)
    {
        auto& an = ra.analyser;
        const int n = std::clamp (s.numPoints, 1, 2048);
        std::vector<float> freq (2048), db (2048);
        mapper.computeFrequencies (s, freq.data(), n);
        const int numBins = an.getNumBins();
        const double binHz = an.getSettings().sampleRate / static_cast<double> (an.getSettings().fftSize);

        if (f.numVisiblePoints != n)
            return "numVisiblePoints " + std::to_string (f.numVisiblePoints) + " != " + std::to_string (n);
        if (f.firstVisiblePoint < 0 || f.firstVisiblePoint + f.numVisiblePoints > f.numPoints)
            return "layout out of range";
        const int o = f.firstVisiblePoint;
        if (! bitEqual (freq.data(), f.freqHz.data() + o, n))
            return "freqHz";

        auto check = [&] (const float* power, const std::array<float, SpectrumFrame::maxPoints>& got, const char* name) -> std::string
        {
            mapper.mapCurve (power, numBins, binHz, s, freq.data(), n, db.data());
            return bitEqual (db.data(), got.data() + o, n) ? std::string() : std::string (name);
        };

        std::string e;
        if ((e = check (an.getPrimaryPower(), f.primaryDb, "primary")).size()) return e;
        if (an.isDual() != f.isDual) return "isDual";
        if (f.isDual && (e = check (an.getPrimaryBPower(), f.primaryBDb, "primaryB")).size()) return e;
        if ((an.getSecondaryPower (false) != nullptr) != f.hasSecondary) return "hasSecondary";
        if (f.hasSecondary)
        {
            if ((e = check (an.getSecondaryPower (false), f.secondaryDb, "secondary")).size()) return e;
            if (f.isDual && (e = check (an.getSecondaryPower (true), f.secondaryBDb, "secondaryB")).size()) return e;
        }
        if (an.hasSidechain() != f.hasSidechain) return "hasSidechain";
        if (f.hasSidechain && (e = check (an.getSidechainPower(), f.sidechainDb, "sidechain")).size()) return e;

        std::vector<float> of (256), od (256);
        const int on = mapper.mapOverview (an.getPrimaryPower(), numBins, binHz, s, of.data(), od.data());
        if (on != f.overviewNumPoints || ! bitEqual (of.data(), f.overviewFreqHz.data(), on) || ! bitEqual (od.data(), f.overviewDb.data(), on))
            return "overview";
        return {};
    }
}

TEST_CASE ("Engine: Fit frames and 25 zoomed views match the T-035 mapper in every channel mode and smoothing mode", "[T-039][engine][regression][visible]")
{
    const auto mode = GENERATE (ChannelMode::mid, ChannelMode::side, ChannelMode::left, ChannelMode::right, ChannelMode::lrPowerMean,
                                ChannelMode::lrDual, ChannelMode::msDual);
    const auto smoothing = GENERATE (SmoothingMode::off, SmoothingMode::oct1_12, SmoothingMode::oct1_3);
    const auto secondary = mode == ChannelMode::mid ? SecondaryMode::off : SecondaryMode::allTimeMax;

    SpectrumSettings s = makeSettings (mode, secondary, 2);
    s.smoothing = smoothing;
    s.numPoints = 1000;

    Rig rig (s, 3);
    Source copy (3);
    const Samples smp = copy.next (19384);
    rig.push (smp);
    REQUIRE (rig.process());

    RefAnalyser ra (s, { smp });
    T035SpectrumMapper t035;
    t035.prepare();

    int views = 0, margins = 0;
    Failure failures;
    for (const View& v : makeViews (20.0, 20000.0))
    {
        rig.s.viewFreqLow = v.lo;
        rig.s.viewFreqHigh = v.hi;
        if (views > 0 && ! rig.process())
            continue; // the clamped view equals the previous one: nothing to re-publish (T-035 publish key)
        auto f = rig.read();
        REQUIRE (f != nullptr);
        ++views;
        margins += f->numPoints > f->numVisiblePoints;

        if (v.lo == 0.0f)
        {
            // Fit: no margin at all, and the whole frame is visible.
            CHECK (f->firstVisiblePoint == 0);
            CHECK (f->numVisiblePoints == f->numPoints);
            CHECK (f->numPoints == 1000);
        }
        SpectrumSettings t = rig.s;
        const std::string err = compareVisibleToT035 (ra, t, *f, t035);
        if (! err.empty())
            failures.note (v.name + ": " + err);
    }
    INFO ("first failure: " << failures.first);
    CHECK (views >= 25);
    CHECK (margins >= 15);
    CHECK (failures.count == 0);
}

TEST_CASE ("Engine: Fit frame of a processor-style request (numPoints = 3072) clamps to 2048 visible points, bit-identical to a 2048 request", "[T-039][engine][clamp]")
{
    // numPoints is not part of the republish key (T-035), so each request gets a fresh rig with the same audio.
    auto fitFrame = [] (int requested, float viewLo = 0.0f, float viewHi = 0.0f)
    {
        SpectrumSettings s = makeSettings (ChannelMode::lrDual, SecondaryMode::allTimeAverage, 2);
        s.numPoints = requested;
        s.viewFreqLow = viewLo;
        s.viewFreqHigh = viewHi;
        Rig rig (s, 5);
        rig.run (30000);
        return rig.read();
    };

    const auto big = fitFrame (SpectrumFrame::maxPoints);
    const auto huge = fitFrame (100000);
    const auto exact = fitFrame (2048);
    REQUIRE (big != nullptr);
    REQUIRE (huge != nullptr);
    REQUIRE (exact != nullptr);
    CHECK (big->numPoints == 2048);
    CHECK (big->numVisiblePoints == 2048);
    CHECK (big->firstVisiblePoint == 0);
    CHECK (huge->numPoints == 2048);
    CHECK (sameCurves (*big, *exact));
    CHECK (sameCurves (*huge, *exact));

    // Zoomed: visible still 2048, total 3072 with both margins.
    const auto zoomed = fitFrame (SpectrumFrame::maxPoints, 500.0f, 1000.0f);
    REQUIRE (zoomed != nullptr);
    CHECK (zoomed->numVisiblePoints == 2048);
    CHECK (zoomed->numPoints == 3072);
    CHECK (zoomed->firstVisiblePoint == 512);

    // numPoints below 1 is clamped to 1 visible point.
    const auto one = fitFrame (0);
    REQUIRE (one != nullptr);
    CHECK (one->numVisiblePoints == 1);
    CHECK (one->numPoints == 1);
}

TEST_CASE ("Engine: frozen re-maps carry the margin and match a live re-map of the same state", "[T-039][engine][freeze]")
{
    const SpectrumSettings s = makeSettings();
    Rig frozen (s, 9), live (s, 9);
    frozen.run (40000);
    live.run (40000);
    frozen.read();
    live.read();

    frozen.s.frozen = true;
    frozen.s.freezeEpoch = 1;
    REQUIRE (frozen.process());
    frozen.read();

    Source other (55, 1);
    frozen.feed (20000, other);
    frozen.process();

    for (const View& v : makeViews (20.0, 20000.0))
    {
        if (v.lo == 0.0f)
            continue;
        frozen.s.viewFreqLow = live.s.viewFreqLow = v.lo;
        frozen.s.viewFreqHigh = live.s.viewFreqHigh = v.hi;
        const bool republishedFrozen = frozen.process();
        const bool republishedLive = live.process();
        CHECK (republishedFrozen == republishedLive);
        if (! republishedFrozen || ! republishedLive)
            continue; // clamped view unchanged
        auto ff = frozen.read();
        auto lf = live.read();
        REQUIRE (ff != nullptr);
        REQUIRE (lf != nullptr);
        INFO (v.name);
        CHECK (ff->firstVisiblePoint == lf->firstVisiblePoint);
        CHECK (ff->numVisiblePoints == lf->numVisiblePoints);
        CHECK (sameCurves (*ff, *lf));
        CHECK (ff->numPoints <= SpectrumFrame::maxPoints);
    }
}

// ---------------------------------------------------------------------
// binHz.
// ---------------------------------------------------------------------
TEST_CASE ("binHz: live frames carry sample rate / FFT size", "[T-039][engine][binHz]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    const int fftSize = GENERATE (512, 4096, 32768);

    SpectrumSettings s = makeSettings (ChannelMode::lrDual, SecondaryMode::off, 0);
    s.sampleRate = sampleRate;
    s.fftSize = fftSize;
    Rig rig (s, 2);
    rig.run (std::max (40000, 3 * fftSize));
    auto f = rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->binHz == sampleRate / static_cast<double> (fftSize));
    CHECK (f->sampleRate == sampleRate);

    // A zoomed re-publish carries it too.
    rig.s.viewFreqLow = 300.0f;
    rig.s.viewFreqHigh = 900.0f;
    REQUIRE (rig.process());
    f = rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->binHz == sampleRate / static_cast<double> (fftSize));
}

TEST_CASE ("binHz: a frozen frame keeps the freeze-time value after an FFT size change; unfreeze publishes the new value", "[T-039][engine][binHz][freeze]")
{
    SpectrumSettings s = makeSettings();
    REQUIRE (s.fftSize == 4096);
    Rig rig (s, 21);
    rig.run (40000);
    auto live = rig.read();
    REQUIRE (live != nullptr);
    const double before = 48000.0 / 4096.0;
    CHECK (live->binHz == before);

    rig.s.frozen = true;
    rig.s.freezeEpoch = 1;
    REQUIRE (rig.process());
    auto entry = rig.read();
    REQUIRE (entry != nullptr);
    CHECK (entry->binHz == before);

    // Change the FFT size while frozen; keep feeding audio and re-mapping views.
    Source other (77, 1);
    rig.s.fftSize = 16384;
    rig.feed (60000, other);
    rig.process();
    CHECK (rig.read() == nullptr);
    for (const float lo : { 200.0f, 400.0f })
    {
        rig.s.viewFreqLow = lo;
        rig.s.viewFreqHigh = lo * 4.0f;
        REQUIRE (rig.process());
        auto f = rig.read();
        REQUIRE (f != nullptr);
        CHECK (f->binHz == before);
        CHECK (f->freezeEpoch == 1);
    }

    // Retaking the snapshot while frozen takes the new FFT size's value.
    rig.s.freezeEpoch = 2;
    if (rig.process())
    {
        auto f = rig.read();
        REQUIRE (f != nullptr);
        CHECK (f->binHz == 48000.0 / 16384.0);
    }

    // Unfreeze: the live state has the new bin spacing.
    rig.s.frozen = false;
    rig.s.freezeEpoch = 3;
    rig.process();
    rig.run (60000, other);
    auto after = rig.read();
    REQUIRE (after != nullptr);
    CHECK (after->freezeEpoch == 3);
    CHECK (after->binHz == 48000.0 / 16384.0);
}

TEST_CASE ("binHz: a frozen frame keeps its value while only the live FFT size changes, unfreeze to the same size restores live value", "[T-039][engine][binHz][freeze]")
{
    SpectrumSettings s = makeSettings();
    Rig rig (s, 23);
    rig.run (40000);
    rig.read();

    rig.s.frozen = true;
    rig.s.freezeEpoch = 1;
    REQUIRE (rig.process());
    REQUIRE (rig.read()->binHz == 48000.0 / 4096.0);

    rig.s.fftSize = 1024;
    rig.s.viewFreqLow = 100.0f;
    rig.s.viewFreqHigh = 2000.0f;
    REQUIRE (rig.process());
    auto f = rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->binHz == 48000.0 / 4096.0);
}

// ---------------------------------------------------------------------
// Allocation.
// ---------------------------------------------------------------------
TEST_CASE ("Allocation: freeze, unfreeze, view changes and re-publishes with margins allocate nothing", "[T-039][alloc]")
{
    SpectrumSettings st = makeSettings();
    st.smoothing = SmoothingMode::oct1_3;
    st.numPoints = 2048;
    Rig rig (st, 31);
    rig.run (40000);
    auto frame = newFrame();
    rig.engine.readLatest (*frame);
    Source other (8, 1);
    const Samples extra = other.next (16384);

    int published = 0;
    int maxPoints = 0;
    uint64_t allocations = 0;
    {
        ScopedAllocGuard guard;
        auto note = [&]
        {
            if (rig.engine.readLatest (*frame))
                maxPoints = std::max (maxPoints, frame->numPoints);
        };

        for (int i = 0; i < 6; ++i)
        {
            rig.s.viewFreqLow = 100.0f * static_cast<float> (i + 1);
            rig.s.viewFreqHigh = 3000.0f + 100.0f * static_cast<float> (i);
            published += rig.process() ? 1 : 0;
            note();
        }
        rig.s.frozen = true;
        rig.s.freezeEpoch = 1;
        published += rig.process() ? 1 : 0;
        note();
        rig.push (extra);
        published += rig.process() ? 1 : 0;
        for (int i = 0; i < 6; ++i)
        {
            rig.s.viewFreqLow = 50.0f * static_cast<float> (i + 1);
            rig.s.viewFreqHigh = 600.0f + 500.0f * static_cast<float> (i);
            published += rig.process() ? 1 : 0;
            note();
        }
        rig.s.freqHigh = 12000.0f;
        published += rig.process() ? 1 : 0;
        rig.s.freezeEpoch = 2;
        published += rig.process() ? 1 : 0;
        rig.s.frozen = false;
        rig.s.freezeEpoch = 3;
        published += rig.process() ? 1 : 0;
        note();
        for (int i = 0; i < 3; ++i)
        {
            rig.s.viewFreqLow = 1000.0f;
            rig.s.viewFreqHigh = 1000.0f * (1.1225f + 0.01f * static_cast<float> (i));
            published += rig.process() ? 1 : 0;
            note();
        }
        rig.s.viewFreqLow = 0.0f;
        rig.s.viewFreqHigh = 0.0f;
        published += rig.process() ? 1 : 0;
        allocations = guard.count();
    }
    CHECK (published >= 20);
    CHECK (maxPoints > 2048); // margins really were published
    CHECK (allocations == 0);
}

TEST_CASE ("Allocation: mapper frame API after prepare allocates nothing", "[T-039][alloc][mapper]")
{
    SpectrumMapper mapper;
    mapper.prepare();
    const Spectrum sp = makeSyntheticSpectrum (48000.0, 65536);
    std::vector<float> freq (SpectrumFrame::maxPoints), db (SpectrumFrame::maxPoints);
    const std::vector<View> views = makeViews (20.0, 20000.0);
    uint64_t allocations = 0;
    {
        ScopedAllocGuard guard;
        for (const View& v : views)
        {
            SpectrumSettings s = mapperSettings (48000.0, SmoothingMode::oct1_3, 2048);
            s.viewFreqLow = v.lo;
            s.viewFreqHigh = v.hi;
            mapper.computeFrameFrequencies (s, freq.data(), 2048);
            mapper.mapCurveFrame (sp.power.data(), sp.numBins, sp.binHz, s, db.data());
            mapper.computeFrequencies (s, freq.data(), 2048);
            mapper.mapCurve (sp.power.data(), sp.numBins, sp.binHz, s, freq.data(), 2048, db.data());
        }
        allocations = guard.count();
    }
    CHECK (allocations == 0);
}

// ---------------------------------------------------------------------
// Numerical safety.
// ---------------------------------------------------------------------
TEST_CASE ("Margin values are finite at silence and denormal input, every smoothing mode", "[T-039][mapper][safety]")
{
    SpectrumMapper mapper;
    mapper.prepare();
    std::vector<float> freq (SpectrumFrame::maxPoints), db (SpectrumFrame::maxPoints);
    const auto level = GENERATE (0.0f, 1.0e-40f, 1.0f);
    std::vector<float> power (4097, level);
    int bad = 0;
    for (const auto smoothing : kSmoothingModes)
        for (const View& v : makeViews (20.0, 20000.0))
        {
            SpectrumSettings s = mapperSettings (48000.0, smoothing, 2048);
            s.viewFreqLow = v.lo;
            s.viewFreqHigh = v.hi;
            const auto layout = mapper.computeFrameFrequencies (s, freq.data(), 2048);
            mapper.mapCurveFrame (power.data(), 4097, 48000.0 / 8192.0, s, db.data());
            for (int i = 0; i < layout.numPoints; ++i)
                bad += ! std::isfinite (db[static_cast<size_t> (i)]) || db[static_cast<size_t> (i)] < -200.0f;
        }
    CHECK (bad == 0);
}

// ---------------------------------------------------------------------
// Timing (hidden; run explicitly in Release).
// ---------------------------------------------------------------------
TEST_CASE ("Performance: worst-case re-map with margin, every curve plus overview <= 1.0 ms median (Release)", "[T-039][.perf]")
{
    SpectrumSettings s = makeSettings (ChannelMode::lrDual, SecondaryMode::allTimeAverage, 2);
    s.fftSize = 65536;
    s.smoothing = SmoothingMode::oct1_3;
    s.numPoints = 2048;
    Rig rig (s, 41);
    rig.run (4 * 48000);
    REQUIRE (rig.read() != nullptr);

    std::vector<double> ms;
    int maxPointsSeen = 0;
    for (int i = 0; i < 100; ++i)
    {
        // Minimum span (1/6 octave) in the interior; the view moves every run so the grid is rebuilt.
        const float lo = 1000.0f * (1.0f + 0.01f * static_cast<float> (i % 50)) * (i % 2 == 0 ? 1.0f : 1.3f);
        rig.s.viewFreqLow = lo;
        rig.s.viewFreqHigh = lo * 1.1225f;

        const auto start = std::chrono::steady_clock::now();
        const bool published = rig.process();
        const auto end = std::chrono::steady_clock::now();
        REQUIRE (published);
        ms.push_back (std::chrono::duration<double, std::milli> (end - start).count());
        auto f = rig.read();
        REQUIRE (f != nullptr);
        maxPointsSeen = std::max (maxPointsSeen, f->numPoints);
    }
    std::sort (ms.begin(), ms.end());
    const double median = 0.5 * (ms[49] + ms[50]);
    WARN ("T-039 re-map median " << median << " ms, min " << ms.front() << " ms, max " << ms.back() << " ms, points " << maxPointsSeen);
    CHECK (maxPointsSeen == 3072);
    CHECK (median <= 1.0);
}
