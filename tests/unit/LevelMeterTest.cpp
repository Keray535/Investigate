// T-007 acceptance criteria for LevelMeter: 50 ms display-peak behaviour,
// peak-max-holds-until-reset, RMS accuracy (50/300/3000 ms windows at 44.1
// and 192 kHz), 1-hour RMS no-drift, sample clip counting, block-size/
// sample-rate-matrix invariance, and no NaN/Inf for silence, DC, full-scale
// noise or denormal-range input.

#include "MeterTestUtils.h"
#include "dsp/LevelMeter.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

TEST_CASE ("LevelMeter: single full-scale click stays visible in samplePeak for >= 50ms", "[dsp][level][T-007]")
{
    constexpr double sampleRate = 48000.0;
    LevelMeter meter;
    meter.prepare (sampleRate, 512, 1);

    // 200 ms of silence, then a 128-sample block containing one full-scale
    // sample, then more silence. Feed in the exact 128-sample block the
    // acceptance criterion names.
    auto silenceBefore = makeSilence (static_cast<int> (0.2 * sampleRate));
    feedMono (meter, silenceBefore, 512);

    std::vector<float> clickBlock (128, 0.0f);
    clickBlock[64] = 1.0f;
    const float* chans[1] = { clickBlock.data() };
    meter.process (chans, 1, 128);

    // Immediately after the block: still visible.
    REQUIRE_THAT (meter.getSamplePeakDb (0), WithinAbs (0.0f, 0.01f));

    // Feed silence in small steps, checking the peak stays visible for at
    // least 50 ms (roadmap: "+-1 block of granularity" allowed), then falls.
    const int stepSamples = static_cast<int> (sampleRate * 0.005); // 5 ms
    int elapsedSamplesSinceClick = 128;
    bool sawFall = false;
    for (int step = 0; step < 40; ++step) // up to 200 ms
    {
        auto silence = makeSilence (stepSamples);
        feedMono (meter, silence, 512);
        elapsedSamplesSinceClick += stepSamples;

        const double elapsedMs = 1000.0 * elapsedSamplesSinceClick / sampleRate;
        const float peakDb = meter.getSamplePeakDb (0);

        if (elapsedMs <= 50.0)
        {
            INFO ("elapsedMs=" << elapsedMs);
            CHECK_THAT (peakDb, WithinAbs (0.0f, 0.01f));
        }
        else if (peakDb < -0.01f)
        {
            sawFall = true;
            break;
        }
    }
    CHECK (sawFall);

    // Peak max holds regardless.
    CHECK_THAT (meter.getPeakMaxDb (0), WithinAbs (0.0f, 0.01f));
}

TEST_CASE ("LevelMeter: peakMax holds until resetStatistics(), reset() clears everything", "[dsp][level][T-007]")
{
    LevelMeter meter;
    meter.prepare (48000.0, 512, 1);

    auto loud = makeSineAt (1000.0, 1.0, 48000.0, 4800, 0.0);
    feedMono (meter, loud, 512);
    CHECK_THAT (meter.getPeakMaxDb (0), WithinAbs (0.0f, 0.05f));

    auto quiet = makeSineAt (1000.0, dbfsToAmplitude (-40.0), 48000.0, 48000, 0.0);
    feedMono (meter, quiet, 512);
    // peakMax must still reflect the earlier louder signal.
    CHECK_THAT (meter.getPeakMaxDb (0), WithinAbs (0.0f, 0.05f));

    meter.resetStatistics();
    CHECK (meter.getPeakMaxDb (0) == -std::numeric_limits<float>::infinity());
    CHECK (meter.getSampleClipCount (0) == 0);

    // resetStatistics() must NOT touch the RMS window (still rolling).
    feedMono (meter, quiet, 512);
    CHECK (meter.getRmsDb (0) > -std::numeric_limits<float>::infinity());

    meter.reset();
    CHECK (meter.getPeakMaxDb (0) == -std::numeric_limits<float>::infinity());
    CHECK (meter.getRmsDb (0) == -std::numeric_limits<float>::infinity());
}

TEST_CASE ("LevelMeter: RMS of a -6dBFS sine reads -9.03 +-0.05dB after window*1.1", "[dsp][level][T-007]")
{
    const double sampleRate = GENERATE (44100.0, 192000.0);
    const double windowMs = GENERATE (50.0, 300.0, 3000.0);

    CAPTURE (sampleRate, windowMs);

    LevelMeter meter;
    meter.prepare (sampleRate, 512, 1);
    meter.setRmsWindowMs (static_cast<float> (windowMs));

    const int numSamples = static_cast<int> (sampleRate * (windowMs / 1000.0) * 1.1);
    auto sine = makeSineAt (1000.0, dbfsToAmplitude (-6.0), sampleRate, numSamples, 0.0);
    feedMono (meter, sine, 512);

    CHECK_THAT (meter.getRmsDb (0), WithinAbs (-9.03f, 0.05f));
}

TEST_CASE ("LevelMeter: RMS shows no drift after 1 hour of simulated audio", "[dsp][level][T-007]")
{
    constexpr double sampleRate = 48000.0;
    constexpr double windowMs = 300.0;

    LevelMeter meter;
    meter.prepare (sampleRate, 4096, 1);
    meter.setRmsWindowMs (static_cast<float> (windowMs));

    const double amplitude = dbfsToAmplitude (-6.0);
    const double freq = 997.0; // avoid exact sub-block-period alignment artefacts
    constexpr int blockSize = 4096;
    const int64_t totalSamples = static_cast<int64_t> (sampleRate) * 3600; // 1 hour

    float rmsAtStart = -1000.0f;
    float rmsAtEnd = -1000.0f;
    int64_t pos = 0;
    const double w = 2.0 * kMeterTestPi * freq / sampleRate;

    std::vector<float> block (blockSize);
    while (pos < totalSamples)
    {
        const int n = static_cast<int> (std::min<int64_t> (blockSize, totalSamples - pos));
        for (int i = 0; i < n; ++i)
            block[static_cast<size_t> (i)] = static_cast<float> (amplitude * std::sin (w * static_cast<double> (pos + i)));

        const float* chans[1] = { block.data() };
        meter.process (chans, 1, n);
        pos += n;

        // Window length * 1.1 has elapsed: capture the "start" reading once.
        if (rmsAtStart < -999.0f && pos >= static_cast<int64_t> (sampleRate * (windowMs / 1000.0) * 1.1))
            rmsAtStart = meter.getRmsDb (0);
    }
    rmsAtEnd = meter.getRmsDb (0);

    INFO ("rmsAtStart=" << rmsAtStart << " rmsAtEnd=" << rmsAtEnd);
    CHECK_THAT (rmsAtStart, WithinAbs (-9.03f, 0.05f));
    CHECK_THAT (rmsAtEnd, WithinAbs (rmsAtStart, 0.05f)); // no drift: end matches start
    CHECK_THAT (rmsAtEnd, WithinAbs (-9.03f, 0.05f));
}

TEST_CASE ("LevelMeter: sample clip count equals exact number of |x| >= 1.0 samples", "[dsp][level][T-007]")
{
    LevelMeter meter;
    meter.prepare (48000.0, 512, 1);

    std::vector<float> signal (10000, 0.5f);
    // Plant an exact, known number of clipping samples.
    int expectedClips = 0;
    for (int i = 0; i < 10000; i += 37)
    {
        signal[static_cast<size_t> (i)] = (i % 74 == 0) ? 1.0f : -1.0f; // both boundary and negative-boundary clips
        ++expectedClips;
    }

    feedMono (meter, signal, 333); // odd block size, exercises the sub-block loop across boundaries
    CHECK (meter.getSampleClipCount (0) == static_cast<uint64_t> (expectedClips));
}

TEST_CASE ("LevelMeter: no NaN or Inf for silence, DC, full-scale noise or denormal input", "[dsp][level][T-007]")
{
    LevelMeter meter;
    meter.prepare (48000.0, 512, 2);

    auto checkFinite = [] (float v)
    {
        CHECK_FALSE (std::isnan (v));
        CHECK ((v == -std::numeric_limits<float>::infinity() || std::isfinite (v)));
    };

    SECTION ("silence")
    {
        auto s = makeSilence (48000);
        feedStereo (meter, s, s, 512);
    }
    SECTION ("DC full-scale")
    {
        auto d = makeDc (48000, 1.0f);
        feedStereo (meter, d, d, 512);
    }
    SECTION ("full-scale noise")
    {
        auto n1 = makeWhiteNoise (1.0, 48000, 1);
        auto n2 = makeWhiteNoise (1.0, 48000, 2);
        feedStereo (meter, n1, n2, 512);
    }
    SECTION ("denormal-range input")
    {
        auto d = makeDc (48000, 1.0e-40f);
        feedStereo (meter, d, d, 512);
    }

    for (int c = 0; c < 2; ++c)
    {
        checkFinite (meter.getSamplePeakDb (c));
        checkFinite (meter.getPeakMaxDb (c));
        checkFinite (meter.getRmsDb (c));
    }
}

TEST_CASE ("LevelMeter: result is independent of how a fixed signal is split into blocks", "[dsp][level][T-007]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 88200.0, 96000.0, 192000.0);
    const int blockSize = GENERATE (1, 17, 64, 512, 4096);

    CAPTURE (sampleRate, blockSize);

    auto sine = makeSineAt (1000.0, dbfsToAmplitude (-6.0), sampleRate, static_cast<int> (sampleRate * 0.5), 0.0);

    LevelMeter reference;
    reference.prepare (sampleRate, 512, 1);
    feedMono (reference, sine, static_cast<int> (sine.size())); // one giant block

    LevelMeter split;
    split.prepare (sampleRate, 512, 1);
    feedMono (split, sine, blockSize);

    CHECK_THAT (split.getPeakMaxDb (0), WithinAbs (reference.getPeakMaxDb (0), 0.01f));
    CHECK_THAT (split.getRmsDb (0), WithinAbs (reference.getRmsDb (0), 0.1f));
}

TEST_CASE ("LevelMeter: prepare() is repeatable", "[dsp][level][T-007]")
{
    LevelMeter meter;
    for (int i = 0; i < 3; ++i)
    {
        meter.prepare (48000.0, 256, 2);
        CHECK (meter.getPeakMaxDb (0) == -std::numeric_limits<float>::infinity());
        auto sine = makeSineAt (1000.0, 1.0, 48000.0, 4800, 0.0);
        feedStereo (meter, sine, sine, 256);
        CHECK_THAT (meter.getPeakMaxDb (0), WithinAbs (0.0f, 0.05f));
    }
}
