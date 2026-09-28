// T-009 / roadmap Budgets: "processBlock takes <= 20us mean and <= 50us p99
// per 128-sample block at 48kHz, with a stereo main and a stereo sidechain
// and analysis active." Catch2's hidden `[.perf]` tag (T-026 / the roadmap's
// 2026-09-29 Decision), so this is excluded from the default `ctest` run and
// is run on purpose via `InvestigateUnitTests.exe "[perf]"`, matching
// MeterBankTest.cpp's equivalent T-024 timing test.

#include "MeterTestUtils.h"
#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <vector>

using namespace investigate;
using namespace investigate::test;

TEST_CASE ("Performance: processBlock() <= 20us mean, <= 50us p99 per 128-sample block at 48kHz, stereo main+sidechain, analysis active (Release)",
           "[processor][performance][.perf][T-009]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 20000; // 20000 * 128 / 48000 ~= 53.3s of audio

    InvestigateProcessor processor;

    auto layout = processor.getBusesLayout();
    layout.getChannelSet (true, 0) = juce::AudioChannelSet::stereo();
    layout.getChannelSet (false, 0) = juce::AudioChannelSet::stereo();
    REQUIRE (layout.inputBuses.size() > 1);
    layout.getChannelSet (true, 1) = juce::AudioChannelSet::stereo();
    REQUIRE (processor.setBusesLayout (layout));

    processor.prepareToPlay (sampleRate, blockSize);
    processor.setAnalysisActive (true);

    auto mainL = makeSineAt (1000.0, dbfsToAmplitude (-12.0), sampleRate, blockSize * numBlocks, 0.0);
    auto mainR = makeSineAt (1000.0, dbfsToAmplitude (-12.0), sampleRate, blockSize * numBlocks, 1.234);
    auto sideL = makeSineAt (500.0, dbfsToAmplitude (-18.0), sampleRate, blockSize * numBlocks, 0.5);
    auto sideR = makeSineAt (500.0, dbfsToAmplitude (-18.0), sampleRate, blockSize * numBlocks, 2.0);

    juce::AudioBuffer<float> buffer (4, blockSize);
    juce::MidiBuffer midi;

    auto fill = [&] (int b)
    {
        const int offset = b * blockSize;
        buffer.copyFrom (0, 0, mainL.data() + offset, blockSize);
        buffer.copyFrom (1, 0, mainR.data() + offset, blockSize);
        buffer.copyFrom (2, 0, sideL.data() + offset, blockSize);
        buffer.copyFrom (3, 0, sideR.data() + offset, blockSize);
    };

    fill (0);
    processor.processBlock (buffer, midi); // warm-up

    std::vector<double> samplesUs;
    samplesUs.reserve (numBlocks - 1);
    for (int b = 1; b < numBlocks; ++b)
    {
        fill (b);
        const auto start = std::chrono::steady_clock::now();
        processor.processBlock (buffer, midi);
        const auto end = std::chrono::steady_clock::now();
        samplesUs.push_back (std::chrono::duration<double, std::micro> (end - start).count());
    }

    processor.setAnalysisActive (false);

    double total = 0.0;
    for (double v : samplesUs) total += v;
    const double meanUs = total / static_cast<double> (samplesUs.size());

    auto sorted = samplesUs;
    std::sort (sorted.begin(), sorted.end());
    const double p99Us = sorted[static_cast<size_t> (sorted.size() * 0.99)];

    INFO ("mean=" << meanUs << "us (roadmap budget 20us), p99=" << p99Us << "us (roadmap budget 50us)");
#ifdef NDEBUG
    CHECK (meanUs <= 20.0);
    CHECK (p99Us <= 50.0);
#else
    WARN ("Debug build: budget not enforced. mean=" << meanUs << "us p99=" << p99Us << "us.");
#endif
}
