// T-009 acceptance criteria covering the analysis thread's lifecycle
// (docs/roadmap.md Budgets and Acceptance criteria; docs/tasks/T-009-processor-wiring.md):
//  - "Opening and closing (setAnalysisActive true then false) 100 times while
//    audio runs on another thread gives no crash, no deadlock, and a thread
//    stop within 50ms each time."
//  - "Changing the sample rate 44.1 -> 96 -> 48kHz while analysis is active
//    makes the frames report the new sample rate within 200ms. Meters
//    restart cleanly."
//  - "isSidechainActive() is true only when the sidechain bus is enabled
//    with at least 1 channel", exercised together with a live bus-layout
//    change while analysis is active (prepareToPlay re-prepares
//    SpectrumEngine with the thread stopped, per the roadmap's threading
//    model / T-005 log).

#include "MeterTestUtils.h"
#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace investigate;
using namespace investigate::test;

namespace
{
    void setChoiceIndex (juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
        REQUIRE (choice != nullptr);
        *choice = index;
    }

    // Small fftSize / 1x overlap / no averaging, so a fresh frame is produced
    // quickly once enough samples are buffered - keeps the "within 200ms"
    // criteria fast and deterministic rather than dependent on the (much
    // larger) roadmap defaults.
    void useFastAnalysisSettings (InvestigateProcessor& processor)
    {
        auto& apvts = processor.getApvts();
        setChoiceIndex (apvts, ParamIDs::fftSize, 0);  // 512
        setChoiceIndex (apvts, ParamIDs::overlap, 0);  // 1x
        auto* avgTime = apvts.getParameter (ParamIDs::avgTime);
        REQUIRE (avgTime != nullptr);
        avgTime->setValueNotifyingHost (0.0f); // avgTime = 0 -> no averaging (roadmap: RT max == current frame)
    }

    void feedBlocks (InvestigateProcessor& processor, double sampleRate, int numMainChannels,
                      int blockSize, int numBlocks, double freqHz)
    {
        auto mono = makeSineAt (freqHz, dbfsToAmplitude (-12.0), sampleRate, blockSize * numBlocks, 0.0);
        juce::AudioBuffer<float> buffer (numMainChannels, blockSize);
        juce::MidiBuffer midi;
        for (int b = 0; b < numBlocks; ++b)
        {
            for (int ch = 0; ch < numMainChannels; ++ch)
                buffer.copyFrom (ch, 0, mono.data() + b * blockSize, blockSize);
            processor.processBlock (buffer, midi);
        }
    }

    /// Polls getLatestSpectrumFrame() for up to `timeoutMs` of *wall-clock*
    /// time (the analysis thread runs on its own real-time schedule,
    /// independent of how fast the test feeds samples - roadmap: "polls
    /// every ~5ms"), returning the first frame for which `pred` is true.
    template <typename Pred>
    bool waitForFrame (InvestigateProcessor& processor, SpectrumFrame& dest, int timeoutMs, Pred pred)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            SpectrumFrame frame;
            if (processor.getLatestSpectrumFrame (frame) && pred (frame))
            {
                dest = frame;
                return true;
            }
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        return false;
    }
}

TEST_CASE ("setAnalysisActive(true)/(false) 100 times while audio runs concurrently: no crash, no deadlock, stop <= 50ms each time",
           "[processor][analysis][T-009]")
{
    InvestigateProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    std::atomic<bool> stopAudio { false };
    std::thread audioThread ([&]
    {
        auto sine = makeSineAt (1000.0, 0.5, 48000.0, 128, 0.0);
        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, sine.data(), 128);
        while (! stopAudio.load (std::memory_order_relaxed))
            processor.processBlock (buffer, midi);
    });

    double worstStopMs = 0.0;
    double totalStopMs = 0.0;
    constexpr int iterations = 100;

    for (int i = 0; i < iterations; ++i)
    {
        processor.setAnalysisActive (true);
        std::this_thread::sleep_for (std::chrono::milliseconds (2)); // let the analysis thread actually run a bit

        const auto start = std::chrono::steady_clock::now();
        processor.setAnalysisActive (false);
        const auto end = std::chrono::steady_clock::now();

        const double stopMs = std::chrono::duration<double, std::milli> (end - start).count();
        worstStopMs = std::max (worstStopMs, stopMs);
        totalStopMs += stopMs;

        INFO ("iteration " << i << ": stop took " << stopMs << " ms");
        CHECK (stopMs <= 50.0);
    }

    stopAudio.store (true, std::memory_order_relaxed);
    audioThread.join();

    INFO ("100x open/close: worst stop = " << worstStopMs << " ms, mean stop = " << (totalStopMs / iterations) << " ms");
    CHECK (worstStopMs <= 50.0);
}

TEST_CASE ("Sample rate change 44.1 -> 96 -> 48 kHz while analysis is active: frames report the new sample rate within 200ms, no crash",
           "[processor][analysis][T-009]")
{
    InvestigateProcessor processor;
    useFastAnalysisSettings (processor);

    processor.prepareToPlay (44100.0, 512);
    processor.setAnalysisActive (true);

    feedBlocks (processor, 44100.0, 2, 512, 8, 1000.0);
    SpectrumFrame frame;
    REQUIRE (waitForFrame (processor, frame, 200, [] (const SpectrumFrame& f) { return f.sampleRate == 44100.0; }));

    // prepareToPlay() stops the analysis thread, re-prepares MeterBank and
    // SpectrumEngine, and restarts the thread because setAnalysisActive(true)
    // left analysisRefCount > 0 (roadmap / T-009 design requirement) - not
    // the audio thread, so this is a legitimate call site for it.
    processor.prepareToPlay (96000.0, 512);
    feedBlocks (processor, 96000.0, 2, 512, 8, 1000.0);
    REQUIRE (waitForFrame (processor, frame, 200, [] (const SpectrumFrame& f) { return f.sampleRate == 96000.0; }));

    processor.prepareToPlay (48000.0, 512);
    feedBlocks (processor, 48000.0, 2, 512, 8, 1000.0);
    REQUIRE (waitForFrame (processor, frame, 200, [] (const SpectrumFrame& f) { return f.sampleRate == 48000.0; }));

    // Meters "restart cleanly": a fresh block after the last prepareToPlay()
    // publishes a snapshot with blockCounter starting again from a small
    // number (prepare() resets it to 0), not a stale/garbage value.
    CHECK (processor.getMeterSnapshot().blockCounter >= 1);
    CHECK (processor.getMeterSnapshot().blockCounter <= 8);

    processor.setAnalysisActive (false);
}

TEST_CASE ("isSidechainActive() reflects a live sidechain bus-layout change while analysis is active, no crash",
           "[processor][analysis][sidechain][T-009]")
{
    InvestigateProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    CHECK_FALSE (processor.isSidechainActive()); // disabled by default (roadmap: buses and channel layouts)

    processor.setAnalysisActive (true);

    // Enable a stereo sidechain (mirrors what a host does: negotiate a new
    // layout, then call prepareToPlay again).
    auto layout = processor.getBusesLayout();
    REQUIRE (layout.inputBuses.size() > 1);
    layout.getChannelSet (true, 1) = juce::AudioChannelSet::stereo();
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (48000.0, 256);
    CHECK (processor.isSidechainActive());

    // Feed a block with a real sidechain signal present; must not crash
    // (SpectrumEngine::pushSidechain now has 2 channels to consume, and was
    // re-prepared with those channel counts by the prepareToPlay() above).
    {
        juce::AudioBuffer<float> buffer (4, 256);
        juce::MidiBuffer midi;
        auto sine = makeSineAt (500.0, 0.4, 48000.0, 256, 0.0);
        for (int ch = 0; ch < 4; ++ch)
            buffer.copyFrom (ch, 0, sine.data(), 256);
        processor.processBlock (buffer, midi);
    }

    // Disable it again.
    layout = processor.getBusesLayout();
    layout.getChannelSet (true, 1) = juce::AudioChannelSet::disabled();
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (48000.0, 256);
    CHECK_FALSE (processor.isSidechainActive());

    {
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        auto sine = makeSineAt (500.0, 0.4, 48000.0, 256, 0.0);
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, sine.data(), 256);
        processor.processBlock (buffer, midi);
    }

    processor.setAnalysisActive (false);
}
