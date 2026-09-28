// T-009 acceptance criterion: "No allocation, lock or system call happens in
// processBlock. unit-tester verifies this with an allocation-hook test over
// 10s of audio with parameter changes, and by audit." (docs/roadmap.md
// Budgets / docs/tasks/T-009-processor-wiring.md).
//
// This exercises InvestigateProcessor::processBlock() itself (not the
// individual dsp/ classes it wires together, which already have their own
// allocation-hook tests from T-004/T-005/T-007), with:
//  - a stereo main bus and a stereo sidechain bus, both enabled;
//  - analysis active (setAnalysisActive(true)), so both the MeterBank and
//    the SpectrumEngine producer-side calls run every block;
//  - parameter changes interleaved between blocks (simulating the message
//    thread automating parameters while audio runs), with the alloc-guard
//    deliberately disarmed for the parameter-set call itself: APVTS/
//    ValueTree and juce::AudioProcessorParameter::setValueNotifyingHost()
//    are message-thread operations with no real-time-safety contract of
//    their own (CLAUDE.md's rule is about code reachable from processBlock/
//    the audio thread) - only the processBlock() calls themselves are
//    required to be allocation-free.
//  - 10s of audio at 48kHz/128-sample blocks (3750 blocks).

#include "AllocGuard.h"
#include "MeterTestUtils.h"
#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

using namespace investigate;
using namespace investigate::test;

namespace
{
    void enableStereoSidechain (InvestigateProcessor& processor)
    {
        auto layout = processor.getBusesLayout();
        layout.getChannelSet (true, 0) = juce::AudioChannelSet::stereo();
        layout.getChannelSet (false, 0) = juce::AudioChannelSet::stereo();
        if (layout.inputBuses.size() > 1)
            layout.getChannelSet (true, 1) = juce::AudioChannelSet::stereo();
        REQUIRE (processor.setBusesLayout (layout));
    }
}

TEST_CASE ("processBlock() performs zero heap allocation over 10s of audio, stereo main+sidechain, analysis active, with parameter changes interleaved",
           "[processor][realtime][T-009]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 3750; // 3750 * 128 / 48000 = 10.0s

    InvestigateProcessor processor;
    enableStereoSidechain (processor);
    processor.prepareToPlay (sampleRate, blockSize);

    // Starting the analysis thread is explicitly documented as possibly
    // allocating (SpectrumEngine may reallocate on a settings change) - kept
    // outside the guarded region, matching the roadmap's own scope ("No
    // allocation ... happens in processBlock").
    processor.setAnalysisActive (true);

    auto mainL = makeSineAt (1000.0, dbfsToAmplitude (-12.0), sampleRate, blockSize * numBlocks, 0.0);
    auto mainR = makeSineAt (1000.0, dbfsToAmplitude (-12.0), sampleRate, blockSize * numBlocks, 1.234);
    auto sideL = makeSineAt (500.0, dbfsToAmplitude (-18.0), sampleRate, blockSize * numBlocks, 0.5);
    auto sideR = makeSineAt (500.0, dbfsToAmplitude (-18.0), sampleRate, blockSize * numBlocks, 2.0);

    juce::AudioBuffer<float> buffer (4, blockSize); // 2 main + 2 sidechain channels
    juce::MidiBuffer midi;

    auto* rmsWindowParam = processor.getApvts().getParameter (ParamIDs::rmsWindow);
    auto* slopeParam = processor.getApvts().getParameter (ParamIDs::slope);
    REQUIRE (rmsWindowParam != nullptr);
    REQUIRE (slopeParam != nullptr);

    // Warm-up outside the guard: first-touch page faults, any lazy
    // initialisation.
    {
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, (ch == 0 ? mainL : mainR).data(), blockSize);
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (2 + ch, 0, (ch == 0 ? sideL : sideR).data(), blockSize);
        processor.processBlock (buffer, midi);
    }

    ScopedAllocGuard guard;

    for (int b = 1; b < numBlocks; ++b)
    {
        const int offset = b * blockSize;
        buffer.copyFrom (0, 0, mainL.data() + offset, blockSize);
        buffer.copyFrom (1, 0, mainR.data() + offset, blockSize);
        buffer.copyFrom (2, 0, sideL.data() + offset, blockSize);
        buffer.copyFrom (3, 0, sideR.data() + offset, blockSize);

        if ((b % 500) == 0)
        {
            // Simulate the message thread automating a parameter between
            // blocks. Disarm the guard for this call only: it is not
            // processBlock, and setValueNotifyingHost() has no allocation
            // contract of its own.
            gAllocGuardArmed.store (false, std::memory_order_relaxed);
            rmsWindowParam->setValueNotifyingHost (rmsWindowParam->getValue() > 0.5f ? 0.1f : 0.9f);
            slopeParam->setValueNotifyingHost (slopeParam->getValue() > 0.5f ? 0.2f : 0.8f);
            gAllocGuardArmed.store (true, std::memory_order_relaxed);
        }

        processor.processBlock (buffer, midi);
    }

    const auto allocations = guard.count();
    processor.setAnalysisActive (false); // outside the guard; stopThread() is not processBlock either.

    INFO ("heap allocations observed across " << (numBlocks - 1) << " processBlock() calls (10s of audio): " << allocations);
    CHECK (allocations == 0);
}
