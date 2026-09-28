// T-004 acceptance criterion 13: performance budgets on the full
// SpectrumAnalyser::pushBlock pipeline (framing + windowing + FFT +
// averaging + secondary), Release build:
//   - 1 s of stereo 48 kHz audio, fftSize 4096, 4x overlap: <= 15 ms.
//   - fftSize 65536, 8x overlap, dual mode with sidechain: <= 60 ms.
//
// The budget is only meaningful (and only enforced as a hard failure) in a
// Release/NDEBUG build - the task explicitly scopes the numbers to
// "Performance, Release". In a Debug build these still run (for basic
// smoke/crash coverage and to report actual numbers) but do not fail the
// suite, since Debug is off by an order of magnitude or more and was never
// the measured configuration.

#include "SpectrumTestUtils.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <functional>

using namespace investigate;
using namespace investigate::test;

namespace
{
    double millisecondsFor (std::function<void()> work)
    {
        const auto start = std::chrono::steady_clock::now();
        work();
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli> (end - start).count();
    }
}

TEST_CASE ("Performance: fftSize 4096, 4x overlap, stereo 48 kHz, 1 s <= 15 ms (Release)", "[dsp][spectrum][performance][.perf][T-004]")
{
    const int fftSize = 4096;
    const double sampleRate = 48000.0;
    const int numSamples = static_cast<int> (sampleRate); // 1 s

    SpectrumAnalyser::Settings s;
    s.sampleRate = sampleRate;
    s.fftSize = fftSize;
    s.overlap = 4;
    s.window = WindowFunctions::Type::hann;
    s.channelMode = ChannelMode::mid;
    s.avgTimeMs = 300.0f;
    s.secondary = SecondaryMode::realTimeMax;
    s.numMainChannels = 2;
    s.numSidechainChannels = 0;

    SpectrumAnalyser analyser;
    analyser.prepare (s);

    auto left = makeWhiteNoise (0.5, numSamples, 21);
    auto right = makeWhiteNoise (0.5, numSamples, 22);

    // Warm-up push (excluded from the timing) so the very first FFT plan
    // use / page faults on freshly-allocated buffers don't skew the budget.
    // Deliberately NOT followed by reset(): this simulates steady-state
    // streaming (ring already full, framing mid-stream), which is the
    // realistic scenario the budget targets - a fresh reset() would empty
    // the ring and make the very next pushBlock's frame count (and cost)
    // depend on how much of fftSize the measured chunk alone provides.
    {
        auto warmL = makeWhiteNoise (0.1, fftSize, 1);
        auto warmR = makeWhiteNoise (0.1, fftSize, 2);
        pushStereo (analyser, warmL, warmR);
    }

    const double elapsedMs = millisecondsFor ([&] { pushStereo (analyser, left, right); });

    INFO ("elapsed = " << elapsedMs << " ms (budget 15 ms)");
#ifdef NDEBUG
    REQUIRE (elapsedMs <= 15.0);
#else
    WARN ("Debug build: budget not enforced, measured " << elapsedMs << " ms against a 15 ms Release budget.");
#endif
}

TEST_CASE ("Performance: fftSize 65536, 8x overlap, dual mode + stereo sidechain, 1 s @ 48 kHz <= 60 ms (Release)", "[dsp][spectrum][performance][.perf][T-004]")
{
    const int fftSize = 65536;
    const double sampleRate = 48000.0;
    const int numSamples = static_cast<int> (sampleRate); // 1 s

    SpectrumAnalyser::Settings s;
    s.sampleRate = sampleRate;
    s.fftSize = fftSize;
    s.overlap = 8;
    s.window = WindowFunctions::Type::hann;
    s.channelMode = ChannelMode::lrDual;
    s.avgTimeMs = 300.0f;
    s.secondary = SecondaryMode::realTimeMax;
    s.numMainChannels = 2;
    s.numSidechainChannels = 2;

    SpectrumAnalyser analyser;
    analyser.prepare (s);
    REQUIRE (analyser.isDual());
    REQUIRE (analyser.hasSidechain());

    auto left = makeWhiteNoise (0.5, numSamples, 31);
    auto right = makeWhiteNoise (0.5, numSamples, 32);
    auto scL = makeWhiteNoise (0.5, numSamples, 33);
    auto scR = makeWhiteNoise (0.5, numSamples, 34);

    {
        auto warmL = makeWhiteNoise (0.1, fftSize, 1);
        auto warmR = makeWhiteNoise (0.1, fftSize, 2);
        auto warmScL = makeWhiteNoise (0.1, fftSize, 3);
        auto warmScR = makeWhiteNoise (0.1, fftSize, 4);
        const float* mainCh[2] = { warmL.data(), warmR.data() };
        const float* scCh[2] = { warmScL.data(), warmScR.data() };
        analyser.pushBlock (mainCh, scCh, fftSize); // fills the ring; no reset() after - see comment above
    }

    const double elapsedMs = millisecondsFor ([&]
    {
        const float* mainCh[2] = { left.data(), right.data() };
        const float* scCh[2] = { scL.data(), scR.data() };
        analyser.pushBlock (mainCh, scCh, numSamples);
    });

    INFO ("elapsed = " << elapsedMs << " ms (budget 60 ms)");
#ifdef NDEBUG
    REQUIRE (elapsedMs <= 60.0);
#else
    WARN ("Debug build: budget not enforced, measured " << elapsedMs << " ms against a 60 ms Release budget.");
#endif
}
