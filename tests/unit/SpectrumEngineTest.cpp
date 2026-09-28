// T-005 acceptance criteria 5, 7, 8 and (engine-level) 9: SpectrumEngine
// facade - settings changes that must not reset averaging/secondary state,
// a producer/consumer overflow budget, mapping performance, and NaN/Inf
// safety through the full pipeline.

#include "SpectrumTestUtils.h"
#include "dsp/SpectrumEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <cmath>
#include <functional>
#include <vector>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    int nearestPointIndex (const std::array<float, SpectrumFrame::maxPoints>& freqHz, int numPoints, float target)
    {
        int best = 0;
        float bestDiff = 1.0e30f;
        for (int i = 0; i < numPoints; ++i)
        {
            const float d = std::abs (freqHz[static_cast<size_t> (i)] - target);
            if (d < bestDiff) { bestDiff = d; best = i; }
        }
        return best;
    }

    double millisecondsFor (const std::function<void()>& work)
    {
        const auto start = std::chrono::steady_clock::now();
        work();
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli> (end - start).count();
    }
}

// ---------------------------------------------------------------------
// AC5: changing freqLow, freqHigh or the number of points between frames
// does not reset the averaging or secondary state. Verified through
// SpectrumEngine::processPending (not just by reading toAnalyserSettings()),
// per the task's explicit instruction.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumEngine: freqLow/freqHigh/numPoints changes never reset averaging or secondary state", "[T-005][engine][AC5]")
{
    const double sampleRate = 48000.0;
    const int fftSize = 4096;
    const int overlap = 4;
    const int hop = fftSize / overlap;
    const float avgTimeMs = 1000.0f;

    SpectrumEngine engine;
    engine.prepare (sampleRate, 512, 1, 0);

    SpectrumSettings settingsA;
    settingsA.sampleRate = sampleRate;
    settingsA.fftSize = fftSize;
    settingsA.overlap = overlap;
    settingsA.window = WindowFunctions::Type::hann;
    settingsA.channelMode = ChannelMode::mid;
    settingsA.avgTimeMs = avgTimeMs;
    settingsA.secondary = SecondaryMode::allTimeMax;
    settingsA.numMainChannels = 1;
    settingsA.numSidechainChannels = 0;
    settingsA.freqLow = 20.0f;
    settingsA.freqHigh = 20000.0f;
    settingsA.smoothing = SmoothingMode::off;
    settingsA.slopeDbPerOct = 0.0f;
    settingsA.numPoints = 2048;

    SpectrumSettings settingsB = settingsA;
    settingsB.freqLow = 100.0f;
    settingsB.freqHigh = 8000.0f;
    settingsB.numPoints = 513;

    // Bin-centred-ish tone near 2 kHz at close to full scale, so its peak is
    // unmistakable against the later silence floor.
    const double toneFreq = 171.0 * (sampleRate / fftSize); // ~2004.7 Hz
    const double amplitude = std::pow (10.0, -3.0 / 20.0);
    auto tone = makeSine (amplitude, toneFreq, sampleRate, fftSize * 3);
    const float* toneCh[1] = { tone.data() };
    engine.pushMain (toneCh, 1, static_cast<int> (tone.size()));

    REQUIRE (engine.processPending (settingsA));

    SpectrumFrame frame;
    REQUIRE (engine.readLatest (frame));
    const int idxA = nearestPointIndex (frame.freqHz, frame.numPoints, 2000.0f);
    const float primaryV1 = frame.primaryDb[static_cast<size_t> (idxA)];
    const float secondaryV1 = frame.secondaryDb[static_cast<size_t> (idxA)];
    INFO ("primaryV1 = " << primaryV1 << " dB, secondaryV1 = " << secondaryV1 << " dB");
    CHECK (secondaryV1 > -20.0f); // the tone must actually register as a peak

    // Alternate freqLow/freqHigh/numPoints on every subsequent call while
    // feeding silence, ending on settingsA so the final grid matches the one
    // used to measure V1 exactly. 41 iterations (odd) guarantees the last
    // (index 40, even) uses settingsA.
    const auto silenceHop = makeSilence (hop);
    constexpr int kIterations = 41;
    bool producedFinal = false;
    for (int i = 0; i < kIterations; ++i)
    {
        const float* silCh[1] = { silenceHop.data() };
        engine.pushMain (silCh, 1, hop);
        const bool useA = (i % 2) == 0;
        producedFinal = engine.processPending (useA ? settingsA : settingsB);
    }
    REQUIRE (producedFinal);

    REQUIRE (engine.readLatest (frame));
    REQUIRE (frame.numPoints == settingsA.numPoints);
    const int idxFinal = nearestPointIndex (frame.freqHz, frame.numPoints, 2000.0f);
    REQUIRE (idxFinal == idxA); // same grid as the V1 measurement

    // Secondary (all-time max) must retain the tone's peak: unaffected by
    // silence, and NOT reset by the freqLow/freqHigh/numPoints churn.
    const float secondaryFinal = frame.secondaryDb[static_cast<size_t> (idxFinal)];
    CHECK_THAT (secondaryFinal, WithinAbs (secondaryV1, 0.1f));

    // Primary average must have decayed smoothly (continuously) from V1,
    // not been re-primed to a near-floor value by any of the intervening
    // setSettings() calls. Analytic prediction for alpha = 1 - exp(-hopTime
    // / tau), tau = avgTimeMs / ln(100), decaying with 0-power silence for
    // kIterations hops: dB_N = V1 + N * 10*log10(1 - alpha).
    const double tau = (avgTimeMs / 1000.0) / std::log (100.0);
    const double hopTime = static_cast<double> (hop) / sampleRate;
    const double alpha = 1.0 - std::exp (-hopTime / tau);
    const double predicted = static_cast<double> (primaryV1) + kIterations * 10.0 * std::log10 (1.0 - alpha);
    const float primaryFinal = frame.primaryDb[static_cast<size_t> (idxFinal)];
    INFO ("predicted (no reset) = " << predicted << " dB, measured = " << primaryFinal << " dB");
    // A reset would re-prime the average to near the -200 dB floor at the
    // very next processed frame, a >100 dB deviation from the analytic
    // no-reset prediction - 2 dB is generous slack for point-grid/interp
    // rounding while still failing hard on any reset.
    CHECK_THAT (static_cast<double> (primaryFinal), WithinAbs (predicted, 2.0));
}

// ---------------------------------------------------------------------
// AC7: a producer at 48 kHz in 128-sample blocks plus a consumer polling
// roughly every 5 ms, for 30 s (simulated by sample/block count, not
// wall-clock), gives 0 overflows at default settings, and the frames
// reflect the injected signal.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumEngine: 30 s at 48 kHz / 128-sample blocks with ~5 ms polling gives 0 overflows and reflects the signal", "[T-005][engine][AC7]")
{
    const double sampleRate = 48000.0;
    const int blockSize = 128;
    const int totalBlocks = 11250; // 30 s * 48000 / 128, exact
    const int blocksPerPoll = 2;   // 256 samples ~= 5.33 ms, close to the roadmap's ~5 ms poll

    SpectrumEngine engine;
    engine.prepare (sampleRate, blockSize, 1, 0);

    SpectrumSettings settings;
    settings.sampleRate = sampleRate;
    settings.fftSize = 4096;
    settings.overlap = 4;
    settings.window = WindowFunctions::Type::hann;
    settings.channelMode = ChannelMode::mid;
    settings.avgTimeMs = 1000.0f;
    settings.secondary = SecondaryMode::realTimeMax;
    settings.numMainChannels = 1;
    settings.numSidechainChannels = 0;
    settings.freqLow = 20.0f;
    settings.freqHigh = 20000.0f;
    settings.smoothing = SmoothingMode::off;
    settings.slopeDbPerOct = 0.0f;
    settings.numPoints = 2048;

    const double toneFreq = 1000.0;
    const double amplitude = std::pow (10.0, -6.0 / 20.0);

    SpectrumFrame lastFrame;
    bool gotAnyFrame = false;
    double phaseAccum = 0.0;

    for (int b = 0; b < totalBlocks; ++b)
    {
        std::vector<float> block (static_cast<size_t> (blockSize));
        const double w = 2.0 * 3.14159265358979323846 * toneFreq / sampleRate;
        for (int n = 0; n < blockSize; ++n)
            block[static_cast<size_t> (n)] = static_cast<float> (amplitude * std::sin (phaseAccum + w * n));
        phaseAccum += w * blockSize;

        const float* ch[1] = { block.data() };
        engine.pushMain (ch, 1, blockSize);

        if ((b + 1) % blocksPerPoll == 0)
        {
            if (engine.processPending (settings))
            {
                REQUIRE (engine.readLatest (lastFrame));
                gotAnyFrame = true;
            }
        }
    }

    // Final drain in case buffered samples remain.
    if (engine.processPending (settings))
        if (engine.readLatest (lastFrame))
            gotAnyFrame = true;

    CHECK (engine.getOverflowCount() == 0);
    REQUIRE (gotAnyFrame);

    const float peak = *std::max_element (lastFrame.primaryDb.begin(),
                                           lastFrame.primaryDb.begin() + lastFrame.numPoints);
    INFO ("peak = " << peak << " dB");
    CHECK (peak >= -7.5f);
    CHECK (peak <= -6.0f);
}

// ---------------------------------------------------------------------
// AC8: mapping, smoothing, slope and dB conversion for 2048 points and 3
// curves take <= 0.3 ms per frame in Release.
// ---------------------------------------------------------------------
TEST_CASE ("Performance: SpectrumMapper 2048 points x 3 curves <= 0.3 ms per frame (Release)", "[T-005][mapper][performance][.perf][AC8]")
{
    SpectrumMapper mapper;
    mapper.prepare();

    SpectrumSettings settings;
    settings.sampleRate = 48000.0;
    settings.freqLow = 20.0f;
    settings.freqHigh = 20000.0f;
    settings.numPoints = 2048;
    settings.smoothing = SmoothingMode::oct1_12;
    settings.slopeDbPerOct = 4.5f;

    const int numPoints = 2048;
    std::vector<float> freqPts (static_cast<size_t> (numPoints));
    mapper.computeFrequencies (settings, freqPts.data(), numPoints);

    const int fftSize = 65536; // worst-case bin count (roadmap's worst-case analysis settings)
    const double binHz = 48000.0 / static_cast<double> (fftSize);
    const int numBins = fftSize / 2 + 1;

    juce::Random rng (12345);
    std::vector<float> binPower (static_cast<size_t> (numBins));
    for (auto& p : binPower)
        p = static_cast<float> (rng.nextDouble());

    std::vector<float> out1 (static_cast<size_t> (numPoints)), out2 (static_cast<size_t> (numPoints)), out3 (static_cast<size_t> (numPoints));

    // Warm-up (excluded from timing).
    mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, out1.data());

    constexpr int kIterations = 200;
    const double totalMs = millisecondsFor ([&]
    {
        for (int i = 0; i < kIterations; ++i)
        {
            mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, out1.data());
            mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, out2.data());
            mapper.mapCurve (binPower.data(), numBins, binHz, settings, freqPts.data(), numPoints, out3.data());
        }
    });

    const double perFrameMs = totalMs / kIterations;
    INFO ("per-frame (3 curves) = " << perFrameMs << " ms (budget 0.3 ms)");
#ifdef NDEBUG
    REQUIRE (perFrameMs <= 0.3);
#else
    WARN ("Debug build: budget not enforced, measured " << perFrameMs << " ms against a 0.3 ms Release budget.");
#endif
}

// ---------------------------------------------------------------------
// AC9 (engine level): no NaN/Inf in any frame for silence, DC, full-scale
// noise or denormal-range input, through the full pushMain -> processPending
// -> readLatest pipeline.
// ---------------------------------------------------------------------
TEST_CASE ("SpectrumEngine: no NaN/Inf for silence, DC, full-scale noise or denormal input", "[T-005][engine][AC9]")
{
    const double sampleRate = 48000.0;
    const int fftSize = 4096;

    SpectrumSettings settings;
    settings.sampleRate = sampleRate;
    settings.fftSize = fftSize;
    settings.overlap = 4;
    settings.window = WindowFunctions::Type::hann;
    settings.channelMode = ChannelMode::lrDual;
    settings.avgTimeMs = 500.0f;
    settings.secondary = SecondaryMode::allTimeAverage;
    settings.numMainChannels = 2;
    settings.numSidechainChannels = 2;
    settings.freqLow = 20.0f;
    settings.freqHigh = 20000.0f;
    settings.smoothing = SmoothingMode::oct1_6;
    settings.slopeDbPerOct = -6.0f;
    settings.numPoints = 2048;

    SpectrumEngine engine;
    engine.prepare (sampleRate, 512, 2, 2);

    auto runSignal = [&] (std::vector<float> l, std::vector<float> r)
    {
        const float* mainCh[2] = { l.data(), r.data() };
        const float* scCh[2] = { r.data(), l.data() };
        engine.pushMain (mainCh, 2, static_cast<int> (l.size()));
        engine.pushSidechain (scCh, 2, static_cast<int> (l.size()));
        REQUIRE (engine.processPending (settings));
        SpectrumFrame frame;
        REQUIRE (engine.readLatest (frame));

        CHECK (allFinite (frame.primaryDb.data(), frame.numPoints));
        CHECK (allFinite (frame.primaryBDb.data(), frame.numPoints));
        CHECK (allFinite (frame.secondaryDb.data(), frame.numPoints));
        CHECK (allFinite (frame.secondaryBDb.data(), frame.numPoints));
        CHECK (allFinite (frame.sidechainDb.data(), frame.numPoints));
        CHECK (allFinite (frame.freqHz.data(), frame.numPoints));
    };

    SECTION ("silence")
    {
        runSignal (makeSilence (fftSize), makeSilence (fftSize));
    }
    SECTION ("DC")
    {
        runSignal (makeDc (1.0, fftSize), makeDc (-1.0, fftSize));
    }
    SECTION ("full-scale white noise")
    {
        runSignal (makeWhiteNoise (1.0, fftSize, 41), makeWhiteNoise (1.0, fftSize, 42));
    }
    SECTION ("denormal-range")
    {
        runSignal (makeDc (1.0e-40, fftSize), makeDc (-1.0e-40, fftSize));
    }
}
