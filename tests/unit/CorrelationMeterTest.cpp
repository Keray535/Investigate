// T-007 acceptance criteria for CorrelationMeter: correlation (L=R, L=-R,
// independent noise, silence, 90deg phase shift, L=R-to-L=-R crossing
// timing) and balance (asymmetric levels, equal levels, silence), plus the
// mono short-circuit.

#include "MeterTestUtils.h"
#include "dsp/CorrelationMeter.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

TEST_CASE ("CorrelationMeter: L=R gives +1.00, L=-R gives -1.00", "[dsp][correlation][T-007]")
{
    constexpr double sampleRate = 48000.0;
    CorrelationMeter meter;
    meter.prepare (sampleRate, 512, 2);

    auto sine = makeSineAt (1000.0, dbfsToAmplitude (-6.0), sampleRate, static_cast<int> (sampleRate * 1.0), 0.0);

    SECTION ("L = R")
    {
        feedStereo (meter, sine, sine, 512);
        CHECK_THAT (meter.getCorrelation(), WithinAbs (1.0f, 0.01f));
        CHECK_FALSE (meter.isMono());
    }
    SECTION ("L = -R")
    {
        std::vector<float> inverted (sine.size());
        for (size_t i = 0; i < sine.size(); ++i) inverted[i] = -sine[i];
        feedStereo (meter, sine, inverted, 512);
        CHECK_THAT (meter.getCorrelation(), WithinAbs (-1.0f, 0.01f));
    }
}

TEST_CASE ("CorrelationMeter: independent white noise gives |r| <= 0.05 after 1s", "[dsp][correlation][T-007]")
{
    constexpr double sampleRate = 48000.0;
    CorrelationMeter meter;
    meter.prepare (sampleRate, 512, 2);

    auto left = makeWhiteNoise (0.5, static_cast<int> (sampleRate * 1.0), 501);
    auto right = makeWhiteNoise (0.5, static_cast<int> (sampleRate * 1.0), 907);
    feedStereo (meter, left, right, 512);

    INFO ("correlation = " << meter.getCorrelation());
    CHECK (std::abs (meter.getCorrelation()) <= 0.05f);
}

TEST_CASE ("CorrelationMeter: silence gives 0.00 and never NaN", "[dsp][correlation][T-007]")
{
    constexpr double sampleRate = 48000.0;
    CorrelationMeter meter;
    meter.prepare (sampleRate, 512, 2);

    auto silence = makeSilence (static_cast<int> (sampleRate * 1.0));
    feedStereo (meter, silence, silence, 512);

    CHECK_FALSE (std::isnan (meter.getCorrelation()));
    CHECK_FALSE (std::isnan (meter.getBalanceDb()));
    CHECK_THAT (meter.getCorrelation(), WithinAbs (0.0f, 0.0001f));
    CHECK_THAT (meter.getBalanceDb(), WithinAbs (0.0f, 0.0001f));
}

TEST_CASE ("CorrelationMeter: before any data, correlation and balance are 0.00, never NaN", "[dsp][correlation][T-007]")
{
    CorrelationMeter meter;
    meter.prepare (48000.0, 512, 2);
    CHECK_FALSE (std::isnan (meter.getCorrelation()));
    CHECK_THAT (meter.getCorrelation(), WithinAbs (0.0f, 0.0001f));
    CHECK_THAT (meter.getBalanceDb(), WithinAbs (0.0f, 0.0001f));
}

TEST_CASE ("CorrelationMeter: 90deg phase-shifted sine pair gives 0.00 +-0.02", "[dsp][correlation][T-007]")
{
    constexpr double sampleRate = 48000.0;
    CorrelationMeter meter;
    meter.prepare (sampleRate, 512, 2);

    const int numSamples = static_cast<int> (sampleRate * 1.0);
    auto left = makeSineAt (1000.0, dbfsToAmplitude (-6.0), sampleRate, numSamples, 0.0);
    auto right = makeSineAt (1000.0, dbfsToAmplitude (-6.0), sampleRate, numSamples, kMeterTestPi / 2.0); // 90deg

    feedStereo (meter, left, right, 512);
    CHECK_THAT (meter.getCorrelation(), WithinAbs (0.0f, 0.02f));
}

TEST_CASE ("CorrelationMeter: switching L=R to L=-R crosses 0 at 250+-50ms and reaches <=-0.95 by 600ms", "[dsp][correlation][T-007]")
{
    constexpr double sampleRate = 48000.0;
    CorrelationMeter meter;
    meter.prepare (sampleRate, 512, 2);

    const double freq = 1000.0;
    const double amplitude = dbfsToAmplitude (-6.0);

    // Stabilise correlation = +1 for well over the 500 ms window.
    const int stableSamples = static_cast<int> (sampleRate * 1.0);
    auto stableL = makeSineAt (freq, amplitude, sampleRate, stableSamples, 0.0);
    feedStereo (meter, stableL, stableL, 512);
    REQUIRE_THAT (meter.getCorrelation(), WithinAbs (1.0f, 0.01f));

    // Feed L=-R in small (10 ms) steps, phase-continuous from the switch
    // point, and record correlation at each elapsed time.
    constexpr int stepSamples = 480; // 10 ms at 48kHz - matches the meter's own sub-block cadence
    int elapsedSamples = 0;
    double crossingMs = -1.0;
    float lastCorrelation = 1.0f;
    float correlationAt600ms = 1.0f;

    while (elapsedSamples < static_cast<int> (sampleRate * 0.7))
    {
        auto l = makeSineAt (freq, amplitude, sampleRate, stepSamples, 0.0, stableSamples + elapsedSamples);
        std::vector<float> r (l.size());
        for (size_t i = 0; i < l.size(); ++i) r[i] = -l[i];

        const float* chans[2] = { l.data(), r.data() };
        meter.process (chans, 2, stepSamples);
        elapsedSamples += stepSamples;

        const double elapsedMs = 1000.0 * elapsedSamples / sampleRate;
        const float corr = meter.getCorrelation();

        if (crossingMs < 0.0 && lastCorrelation >= 0.0f && corr < 0.0f)
            crossingMs = elapsedMs;
        lastCorrelation = corr;

        if (elapsedMs >= 599.0 && elapsedMs <= 601.0)
            correlationAt600ms = corr;
    }

    INFO ("crossingMs=" << crossingMs << " correlationAt600ms=" << correlationAt600ms);
    REQUIRE (crossingMs > 0.0);
    CHECK (crossingMs >= 200.0);
    CHECK (crossingMs <= 300.0);
    CHECK (correlationAt600ms <= -0.95f);
}

TEST_CASE ("CorrelationMeter: balance", "[dsp][correlation][T-007]")
{
    constexpr double sampleRate = 48000.0;

    SECTION ("L at -6dBFS, R at -12dBFS (noise) gives -6.0 +-0.2dB after 3s")
    {
        CorrelationMeter meter;
        meter.prepare (sampleRate, 512, 2);

        // Calibrated-RMS uniform noise: RMS = amplitude/sqrt(3), so choosing
        // amplitude = targetLinearRms*sqrt(3) gives an exact target RMS in
        // the large-N limit.
        const double leftRms = dbfsToAmplitude (-6.0);
        const double rightRms = dbfsToAmplitude (-12.0);
        const int numSamples = static_cast<int> (sampleRate * 3.0);
        auto left = makeWhiteNoise (leftRms * std::sqrt (3.0), numSamples, 111);
        auto right = makeWhiteNoise (rightRms * std::sqrt (3.0), numSamples, 222);

        feedStereo (meter, left, right, 512);
        INFO ("balance = " << meter.getBalanceDb());
        CHECK_THAT (meter.getBalanceDb(), WithinAbs (-6.0f, 0.2f));
    }
    SECTION ("Equal levels give 0.0 +-0.2dB")
    {
        CorrelationMeter meter;
        meter.prepare (sampleRate, 512, 2);

        const double rms = dbfsToAmplitude (-9.0);
        const int numSamples = static_cast<int> (sampleRate * 3.0);
        auto left = makeWhiteNoise (rms * std::sqrt (3.0), numSamples, 333);
        auto right = makeWhiteNoise (rms * std::sqrt (3.0), numSamples, 444);

        feedStereo (meter, left, right, 512);
        CHECK_THAT (meter.getBalanceDb(), WithinAbs (0.0f, 0.2f));
    }
    SECTION ("Silence gives 0.0, never NaN")
    {
        CorrelationMeter meter;
        meter.prepare (sampleRate, 512, 2);
        auto silence = makeSilence (static_cast<int> (sampleRate * 3.0));
        feedStereo (meter, silence, silence, 512);
        CHECK_FALSE (std::isnan (meter.getBalanceDb()));
        CHECK_THAT (meter.getBalanceDb(), WithinAbs (0.0f, 0.0001f));
    }
}

TEST_CASE ("CorrelationMeter: mono (numChannels < 2) reports correlation +1, balance 0, isMono true", "[dsp][correlation][T-007]")
{
    CorrelationMeter meter;
    meter.prepare (48000.0, 512, 2);

    auto mono = makeSineAt (1000.0, 0.5, 48000.0, 4800, 0.0);
    const float* chans[1] = { mono.data() };
    meter.process (chans, 1, 4800);

    CHECK_THAT (meter.getCorrelation(), WithinAbs (1.0f, 0.0001f));
    CHECK_THAT (meter.getBalanceDb(), WithinAbs (0.0f, 0.0001f));
    CHECK (meter.isMono());
}

TEST_CASE ("CorrelationMeter: no NaN or Inf for DC, full-scale noise or denormal input", "[dsp][correlation][T-007]")
{
    CorrelationMeter meter;
    meter.prepare (48000.0, 512, 2);

    auto checkFinite = [] (float v)
    {
        CHECK_FALSE (std::isnan (v));
        CHECK (std::isfinite (v));
    };

    SECTION ("DC full-scale")
    {
        auto d = makeDc (48000, 1.0f);
        feedStereo (meter, d, d, 512);
    }
    SECTION ("full-scale noise")
    {
        auto n1 = makeWhiteNoise (1.0, 48000, 71);
        auto n2 = makeWhiteNoise (1.0, 48000, 72);
        feedStereo (meter, n1, n2, 512);
    }
    SECTION ("denormal-range input")
    {
        auto d = makeDc (48000, 1.0e-40f);
        feedStereo (meter, d, d, 512);
    }

    checkFinite (meter.getCorrelation());
    checkFinite (meter.getBalanceDb());
}
