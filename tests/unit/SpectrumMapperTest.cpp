// T-005 acceptance criteria 1-4 and (mapper-level) 9: SpectrumMapper
// bin-to-point mapping, fractional-octave smoothing, slope and dB
// conversion.

#include "SpectrumTestUtils.h"
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

// ---------------------------------------------------------------------
// AC1: a -6 dBFS sine at any frequency from 20 Hz to 20 kHz shows a peak
// point value between -6.0 and -7.5 dB (Hann, fftSize 4096, 2048 points,
// slope 0, smoothing off). The upper bound allows for Hann scalloping.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumMapper: -6 dBFS sine peak reads -6.0..-7.5 dB, 20 Hz..20 kHz", "[T-005][mapper][AC1]")
{
    const double sampleRate = 48000.0;
    const int fftSize = 4096;

    const double toneFreq = GENERATE (20.0, 31.5, 50.0, 100.0, 250.0, 500.0, 1000.0,
                                       2000.0, 4000.0, 8000.0, 12000.0, 16000.0, 19000.0, 19980.0);

    SpectrumAnalyser::Settings aSettings;
    aSettings.sampleRate = sampleRate;
    aSettings.fftSize = fftSize;
    aSettings.overlap = 4;
    aSettings.window = WindowFunctions::Type::hann;
    aSettings.channelMode = ChannelMode::mid;
    aSettings.avgTimeMs = 1000.0f;
    aSettings.secondary = SecondaryMode::off;
    aSettings.numMainChannels = 1;
    aSettings.numSidechainChannels = 0;

    SpectrumAnalyser analyser;
    analyser.prepare (aSettings);

    const double amplitude = std::pow (10.0, -6.0 / 20.0); // -6 dBFS
    auto sine = makeSine (amplitude, toneFreq, sampleRate, fftSize);
    const float* ch[1] = { sine.data() };
    analyser.pushBlock (ch, nullptr, fftSize);
    REQUIRE (analyser.getFramesProduced() >= 1);

    SpectrumSettings settings;
    settings.sampleRate = sampleRate;
    settings.freqLow = 20.0f;
    settings.freqHigh = 20000.0f;
    settings.smoothing = SmoothingMode::off;
    settings.slopeDbPerOct = 0.0f;
    settings.numPoints = 2048;

    SpectrumMapper mapper;
    mapper.prepare();

    const int numPoints = 2048;
    std::vector<float> freqPts (static_cast<size_t> (numPoints)), outDb (static_cast<size_t> (numPoints));
    mapper.computeFrequencies (settings, freqPts.data(), numPoints);

    const double binHz = sampleRate / static_cast<double> (fftSize);
    mapper.mapCurve (analyser.getPrimaryPower(), analyser.getNumBins(), binHz, settings,
                      freqPts.data(), numPoints, outDb.data());

    const float peak = *std::max_element (outDb.begin(), outDb.end());
    INFO ("toneFreq = " << toneFreq << " Hz, peak = " << peak << " dB");
    CHECK (peak >= -7.5f);
    CHECK (peak <= -6.0f);
}

// ---------------------------------------------------------------------
// AC2: low-frequency interpolation never produces a point higher than the
// maximum of its two neighbouring bins plus 0.1 dB.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumMapper: interpolated points never exceed max(neighbour bins) + 0.1 dB", "[T-005][mapper][AC2]")
{
    SpectrumMapper mapper;
    mapper.prepare();

    SpectrumSettings settings;
    settings.sampleRate = 48000.0;
    settings.freqLow = 20.0f;
    // A narrow low-frequency band with 2048 points guarantees "points
    // denser than bins" (interpolation) across the whole range: point
    // spacing here is a small fraction of a Hz, far finer than the 11.7 Hz
    // bin spacing used below.
    settings.freqHigh = 200.0f;
    settings.smoothing = SmoothingMode::off;
    settings.slopeDbPerOct = 0.0f;
    settings.numPoints = 2048;

    const int numPoints = 2048;
    std::vector<float> freqPts (static_cast<size_t> (numPoints)), outDb (static_cast<size_t> (numPoints));
    mapper.computeFrequencies (settings, freqPts.data(), numPoints);

    const int fftSize = 4096;
    const double binHz = 48000.0 / static_cast<double> (fftSize);
    const int numBins = fftSize / 2 + 1;

    const unsigned seed = GENERATE (1u, 2u, 3u);
    juce::Random rng (static_cast<juce::int64> (seed));
    std::vector<float> binPower (static_cast<size_t> (numBins));
    for (auto& p : binPower)
        p = static_cast<float> (rng.nextDouble()); // arbitrary positive power values in [0, 1)

    mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, outDb.data());

    for (int i = 0; i < numPoints; ++i)
    {
        const double bin = static_cast<double> (freqPts[static_cast<size_t> (i)]) / binHz;
        const int b0 = juce::jlimit (0, numBins - 1, static_cast<int> (std::floor (bin)));
        const int b1 = juce::jlimit (0, numBins - 1, b0 + 1);
        const double maxNeighbourPower = std::max<double> (binPower[static_cast<size_t> (b0)], binPower[static_cast<size_t> (b1)]);
        const double maxNeighbourDb = 10.0 * std::log10 (std::max (maxNeighbourPower, 1e-300));

        INFO ("point " << i << " f=" << freqPts[static_cast<size_t> (i)] << " Hz, b0=" << b0 << " b1=" << b1);
        CHECK (outDb[static_cast<size_t> (i)] <= static_cast<float> (maxNeighbourDb + 0.1));
    }
}

// ---------------------------------------------------------------------
// AC3: slope S = +4.5 dB/oct on flat (white) noise, averaged: point at
// 1 kHz unchanged within +-0.01 dB, point at 2 kHz is +4.5 +-0.01 dB, point
// at 250 Hz is -9.0 +-0.01 dB, all relative to slope 0.
//
// A flat linear-power spectrum stands in for "flat white noise, averaged"
// (in the power domain an exact average of white noise converges to exactly
// this), which lets the +-0.01 dB tolerance be met by construction rather
// than by chance: freqLow=250, freqHigh=32000 and numPoints=449 are chosen
// so log2(32000/250) = 7 octaves divides evenly into 448 steps of exactly
// 1/64 octave, putting display points exactly on 250 Hz (i=0), 1000 Hz
// (i=128) and 2000 Hz (i=192).
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumMapper: slope +4.5 dB/oct matches the analytic dB delta exactly on a flat spectrum", "[T-005][mapper][AC3]")
{
    SpectrumMapper mapper;
    mapper.prepare();

    SpectrumSettings settings;
    settings.sampleRate = 96000.0;
    settings.freqLow = 250.0f;
    settings.freqHigh = 32000.0f;
    settings.smoothing = SmoothingMode::off;
    settings.numPoints = 449;

    const int numPoints = 449;
    std::vector<float> freqPts (static_cast<size_t> (numPoints));
    mapper.computeFrequencies (settings, freqPts.data(), numPoints);

    REQUIRE_THAT (freqPts[0], WithinAbs (250.0f, 0.01f));
    REQUIRE_THAT (freqPts[128], WithinAbs (1000.0f, 0.05f));
    REQUIRE_THAT (freqPts[192], WithinAbs (2000.0f, 0.1f));

    const int fftSize = 8192;
    const double binHz = 96000.0 / static_cast<double> (fftSize);
    const int numBins = fftSize / 2 + 1;
    std::vector<float> binPower (static_cast<size_t> (numBins), 0.001f); // flat power spectrum

    std::vector<float> outZero (static_cast<size_t> (numPoints)), outSlope (static_cast<size_t> (numPoints));

    settings.slopeDbPerOct = 0.0f;
    mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, outZero.data());

    settings.slopeDbPerOct = 4.5f;
    mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, outSlope.data());

    CHECK_THAT ((double) (outSlope[128] - outZero[128]), WithinAbs (0.0, 0.01));
    CHECK_THAT ((double) (outSlope[192] - outZero[192]), WithinAbs (4.5, 0.01));
    CHECK_THAT ((double) (outSlope[0] - outZero[0]), WithinAbs (-9.0, 0.01));
}

// ---------------------------------------------------------------------
// AC4: smoothing at 1/3 octave applied to a single-bin impulse spectrum
// spreads it over a width of 1/3 octave +-10%. With smoothing Off, values
// are identical to the unsmoothed input.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumMapper: 1/3-octave smoothing spreads a point impulse over ~1/3 octave; Off leaves it unspread", "[T-005][mapper][AC4]")
{
    SpectrumMapper mapper;
    mapper.prepare();

    SpectrumSettings settings;
    settings.sampleRate = 48000.0;
    settings.freqLow = 20.0f;
    settings.freqHigh = 20000.0f;
    settings.numPoints = 512;
    settings.slopeDbPerOct = 0.0f;

    const int numPoints = 512;
    std::vector<float> freqPts (static_cast<size_t> (numPoints));
    mapper.computeFrequencies (settings, freqPts.data(), numPoints);

    const int fftSize = 65536; // fine bin spacing => deep in the "bins denser than points" (aggregation) regime near 1 kHz
    const double binHz = 48000.0 / static_cast<double> (fftSize);
    const int numBins = fftSize / 2 + 1;

    int i0 = 0;
    float best = 1.0e30f;
    for (int i = 0; i < numPoints; ++i)
    {
        const float d = std::abs (freqPts[static_cast<size_t> (i)] - 1000.0f);
        if (d < best) { best = d; i0 = i; }
    }
    REQUIRE (i0 > 20);
    REQUIRE (i0 < numPoints - 20);

    const int impulseBin = juce::jlimit (0, numBins - 1,
        static_cast<int> (std::lround (static_cast<double> (freqPts[static_cast<size_t> (i0)]) / binHz)));
    std::vector<float> binPower (static_cast<size_t> (numBins), 0.0f);
    binPower[static_cast<size_t> (impulseBin)] = 1.0f;

    // Off: unsmoothed. The point nearest the impulse reads its power exactly.
    settings.smoothing = SmoothingMode::off;
    std::vector<float> outOff (static_cast<size_t> (numPoints));
    mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, outOff.data());
    CHECK_THAT (outOff[static_cast<size_t> (i0)], WithinAbs (0.0f, 0.02f)); // 10*log10(1.0) == 0 dB exactly

    int offNonzero = 0;
    for (int i = 0; i < numPoints; ++i)
        if (outOff[static_cast<size_t> (i)] > -100.0f)
            ++offNonzero;
    CHECK (offNonzero <= 2); // essentially unspread (at most a rounding-boundary neighbour)

    // 1/3 octave: spread over a contiguous band whose width is 1/3 octave +-10%.
    settings.smoothing = SmoothingMode::oct1_3;
    std::vector<float> outSmoothed (static_cast<size_t> (numPoints));
    mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, outSmoothed.data());

    int lo = -1, hi = -1;
    for (int i = 0; i < numPoints; ++i)
    {
        if (outSmoothed[static_cast<size_t> (i)] > -190.0f) // well above the -200 dB floor
        {
            if (lo < 0) lo = i;
            hi = i;
        }
    }
    REQUIRE (lo >= 0);

    const double widthOctaves = std::log2 (static_cast<double> (freqPts[static_cast<size_t> (hi)])
                                          / static_cast<double> (freqPts[static_cast<size_t> (lo)]));
    const double target = 1.0 / 3.0;
    INFO ("measured width = " << widthOctaves << " octaves, target = " << target);
    CHECK (widthOctaves >= target * 0.9);
    CHECK (widthOctaves <= target * 1.1);

    // A boxcar mean of a single impulse is constant wherever the window
    // includes the impulse, i.e. the plateau should be flat.
    for (int i = lo; i <= hi; ++i)
        CHECK_THAT (outSmoothed[static_cast<size_t> (i)], WithinAbs (outSmoothed[static_cast<size_t> (lo)], 0.05f));
}

// ---------------------------------------------------------------------
// AC9 (mapper level): no NaN/Inf for silence, full-scale/DC-like, denormal
// or very large bin-power inputs, across every smoothing mode and a range
// of slope values.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumMapper: no NaN/Inf for extreme bin-power inputs", "[T-005][mapper][AC9]")
{
    SpectrumMapper mapper;
    mapper.prepare();

    SpectrumSettings settings;
    settings.sampleRate = 48000.0;
    settings.freqLow = 20.0f;
    settings.freqHigh = 20000.0f;
    settings.numPoints = 2048;
    settings.slopeDbPerOct = GENERATE (-12.0f, 0.0f, 4.5f, 12.0f);
    settings.smoothing = GENERATE (SmoothingMode::off, SmoothingMode::oct1_48, SmoothingMode::oct1_3);

    const int numPoints = 2048;
    std::vector<float> freqPts (static_cast<size_t> (numPoints)), outDb (static_cast<size_t> (numPoints));
    mapper.computeFrequencies (settings, freqPts.data(), numPoints);

    const int fftSize = 4096;
    const double binHz = 48000.0 / static_cast<double> (fftSize);
    const int numBins = fftSize / 2 + 1;

    SECTION ("silence")
    {
        std::vector<float> p (static_cast<size_t> (numBins), 0.0f);
        mapper.mapCurve (p.data(), numBins, binHz, settings, freqPts.data(), numPoints, outDb.data());
        CHECK (allFinite (outDb.data(), numPoints));
    }
    SECTION ("full-scale / DC-like")
    {
        std::vector<float> p (static_cast<size_t> (numBins), 1.0f);
        mapper.mapCurve (p.data(), numBins, binHz, settings, freqPts.data(), numPoints, outDb.data());
        CHECK (allFinite (outDb.data(), numPoints));
    }
    SECTION ("denormal-range (1e-40)")
    {
        std::vector<float> p (static_cast<size_t> (numBins), 1.0e-40f);
        mapper.mapCurve (p.data(), numBins, binHz, settings, freqPts.data(), numPoints, outDb.data());
        CHECK (allFinite (outDb.data(), numPoints));
    }
    SECTION ("very large")
    {
        std::vector<float> p (static_cast<size_t> (numBins), 1.0e30f);
        mapper.mapCurve (p.data(), numBins, binHz, settings, freqPts.data(), numPoints, outDb.data());
        CHECK (allFinite (outDb.data(), numPoints));
    }
}
