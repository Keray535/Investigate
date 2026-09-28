// T-006 acceptance criterion: process() <= 5 us mean per 128-sample stereo
// block at 48 kHz in a Release build on the development machine. Same
// Release-only enforcement pattern as SpectrumAnalyserPerformanceTest.cpp
// (T-004): measured and reported in Debug too, but only asserted under
// NDEBUG.

#include "LoudnessMeterTestUtils.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace investigate;
using namespace investigate::test;

TEST_CASE ("Performance: process() <= 5us mean per 128-sample stereo block at 48kHz (Release)", "[dsp][loudness][performance][.perf][T-006]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 20000; // 20000 * 128 / 48000 ~= 53.3s of audio

    LoudnessMeter meter;
    meter.prepare (sampleRate, blockSize, 2);

    auto left = makeSine (1000.0, dbfsToAmplitude (-18.0), sampleRate, blockSize * numBlocks);
    auto right = left;

    // Warm-up (excluded from timing): first-touch page faults, filter
    // transient.
    {
        const float* chans[2] = { left.data(), right.data() };
        meter.process (chans, 2, blockSize);
    }

    const auto start = std::chrono::steady_clock::now();
    for (int b = 1; b < numBlocks; ++b)
    {
        const float* chans[2] = { left.data() + static_cast<size_t> (b) * blockSize,
                                   right.data() + static_cast<size_t> (b) * blockSize };
        meter.process (chans, 2, blockSize);
    }
    const auto end = std::chrono::steady_clock::now();

    const double totalUs = std::chrono::duration<double, std::micro> (end - start).count();
    const double meanUsPerBlock = totalUs / static_cast<double> (numBlocks - 1);

    INFO ("mean = " << meanUsPerBlock << " us/block (budget 5 us), " << (numBlocks - 1) << " blocks");
#ifdef NDEBUG
    REQUIRE (meanUsPerBlock <= 5.0);
#else
    WARN ("Debug build: budget not enforced, measured " << meanUsPerBlock << " us/block against a 5 us Release budget.");
#endif
}

// Real-time safety rule: denormal-range input (~1e-40) must not slow
// process() down materially. The class does not engage
// juce::ScopedNoDenormals itself (documented assumption that the caller
// does - see the class comment); this test does not engage it either, so a
// slowdown here would reflect the class's own behaviour under denormals
// with no host-level FTZ/DAZ protection.
TEST_CASE ("Performance: denormal-range input does not slow process() down", "[dsp][loudness][performance][.perf][T-006]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 5000;

    auto timeRun = [&] (float amplitude) -> double
    {
        LoudnessMeter meter;
        meter.prepare (sampleRate, blockSize, 2);
        std::vector<float> left (static_cast<size_t> (blockSize) * numBlocks, amplitude);
        std::vector<float> right = left;

        const float* warm[2] = { left.data(), right.data() };
        meter.process (warm, 2, blockSize);

        const auto start = std::chrono::steady_clock::now();
        for (int b = 1; b < numBlocks; ++b)
        {
            const float* chans[2] = { left.data() + static_cast<size_t> (b) * blockSize,
                                       right.data() + static_cast<size_t> (b) * blockSize };
            meter.process (chans, 2, blockSize);
        }
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::micro> (end - start).count() / (numBlocks - 1);
    };

    const double normalUs = timeRun (1.0e-3f);
    const double denormalUs = timeRun (1.0e-40f);

    INFO ("normal-amplitude = " << normalUs << " us/block, denormal-amplitude = " << denormalUs << " us/block");
#ifdef NDEBUG
    // Generous factor (10x): the point is to catch the classic
    // orders-of-magnitude denormal-stall pathology, not to re-assert the
    // 5us budget (already covered by the test above).
    REQUIRE (denormalUs <= normalUs * 10.0 + 5.0);
#else
    WARN ("Debug build: not enforced. normal=" << normalUs << " denormal=" << denormalUs << " us/block.");
#endif
}
