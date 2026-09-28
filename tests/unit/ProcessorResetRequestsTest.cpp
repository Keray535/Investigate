// T-009 acceptance criterion: "requestStatsReset and requestSpectrumReset,
// called from the message thread, take effect within one block or one frame
// respectively." (docs/roadmap.md Acceptance criteria / Processor API).
// Exercised end-to-end through InvestigateProcessor's public API (not
// MeterBank/SpectrumEngine directly, which already have their own T-007/
// T-005 tests for the same underlying mechanism) to confirm the wiring
// itself - the atomic request flags and where they are consumed - is
// correct.

#include "MeterTestUtils.h"
#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <limits>
#include <thread>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    void setChoiceIndex (juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
        REQUIRE (choice != nullptr);
        *choice = index;
    }

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

    void processStereoBlock (InvestigateProcessor& processor, const std::vector<float>& mono, int blockSize)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, mono.data(), blockSize);
        processor.processBlock (buffer, midi);
    }
}

TEST_CASE ("requestStatsReset() takes effect within one block", "[processor][reset][T-009]")
{
    InvestigateProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    // Build up a non-trivial peakMax and clip count with a full-scale signal.
    auto loud = makeSineAt (1000.0, 1.0, 48000.0, 512, 0.0);
    for (int i = 0; i < 5; ++i)
        processStereoBlock (processor, loud, 512);

    REQUIRE_THAT (processor.getMeterSnapshot().peakMax[0], WithinAbs (0.0f, 0.05f));
    REQUIRE (processor.getMeterSnapshot().sampleClipCount[0] > 0);

    // Called from "the message thread" (the calling thread here) per the
    // roadmap's documented contract.
    processor.requestStatsReset();

    // "Takes effect within one block": feed exactly one more block and check.
    auto quiet = makeSilence (512);
    processStereoBlock (processor, quiet, 512);

    const auto snap = processor.getMeterSnapshot();
    CHECK (snap.peakMax[0] == -std::numeric_limits<float>::infinity());
    CHECK (snap.sampleClipCount[0] == 0);
}

TEST_CASE ("requestSpectrumReset() takes effect within one analysis frame", "[processor][reset][T-009]")
{
    InvestigateProcessor processor;
    auto& apvts = processor.getApvts();
    setChoiceIndex (apvts, ParamIDs::fftSize, 0); // 512: fast turnover for this test
    setChoiceIndex (apvts, ParamIDs::overlap, 0); // 1x
    setChoiceIndex (apvts, ParamIDs::secondary, 2); // All-time max: monotonically non-decreasing until reset
    apvts.getParameter (ParamIDs::avgTime)->setValueNotifyingHost (0.0f);

    processor.prepareToPlay (48000.0, 512);
    processor.setAnalysisActive (true);

    // Feed a loud tone long enough to accumulate a clearly nonzero all-time
    // max secondary curve, but bounded to at most one SpectrumEngine::
    // processPending() drain chunk (kDrainChunkFrames == 8192 samples ==
    // 16 hops of 512 at 1x overlap - source/dsp/SpectrumEngine.h). This
    // keeps the loud backlog small enough that it cannot survive past a
    // couple of AnalysisThread polls (below).
    //
    // Bounding the pushed block count alone is *not* enough to guarantee no
    // leftover loud data behind the reset, though: the AnalysisThread polls
    // independently every 5 ms (PluginProcessor.cpp AnalysisThread::run()),
    // so it can race the push loop below and drain a *partial* backlog (say
    // the first 3 of 10 blocks) on one call, publish that as "the newest
    // frame with hasSecondary", and only pick up the remaining queued loud
    // blocks on its *next* call a few ms later. waitForFrame() returns on
    // the first satisfying frame, so naively treating that first frame's
    // max as final and immediately requesting the reset reopens exactly the
    // original bug (leftover loud hops draining right after the reset is
    // consumed, reviving the all-time max before a drop is ever observed) -
    // just with a smaller, still-reliably-reproducible backlog instead of
    // the original 40-block one. So below, after the first hasSecondary hit,
    // we keep polling for a short settle window and track the running max:
    // since allTimeMax is monotonically non-decreasing until reset, the
    // value at the end of that settle window is guaranteed to already
    // include any backlog the AnalysisThread was still draining, as long as
    // the settle window comfortably exceeds its 5 ms poll period (it does
    // here, at 60 ms).
    auto loud = makeSineAt (1000.0, dbfsToAmplitude (-6.0), 48000.0, 512, 0.0);
    for (int i = 0; i < 10; ++i)
        processStereoBlock (processor, loud, 512);

    SpectrumFrame beforeReset;
    REQUIRE (waitForFrame (processor, beforeReset, 200, [] (const SpectrumFrame& f) { return f.hasSecondary; }));
    REQUIRE (beforeReset.numPoints > 0);

    auto curveMax = [] (const SpectrumFrame& f)
    {
        float m = -1000.0f;
        for (int i = 0; i < f.numPoints; ++i)
            m = std::max (m, f.secondaryDb[static_cast<size_t> (i)]);
        return m;
    };

    float maxBefore = curveMax (beforeReset);

    // Settle window: let any still-in-flight backlog finish draining before
    // this value is treated as final (see comment above).
    const auto settleDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (60);
    while (std::chrono::steady_clock::now() < settleDeadline)
    {
        SpectrumFrame frame;
        if (processor.getLatestSpectrumFrame (frame) && frame.hasSecondary)
            maxBefore = std::max (maxBefore, curveMax (frame));
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    REQUIRE (maxBefore > -100.0f); // a real -6dBFS tone's all-time max is nowhere near the -200dB floor

    // Called from the message thread, per the roadmap's documented contract.
    processor.requestSpectrumReset();

    // Feed silence and confirm the secondary (all-time max) curve has
    // dropped, i.e. the reset was actually consumed by the analysis thread
    // "within one frame" rather than being ignored or delayed indefinitely.
    auto quiet = makeSilence (512);
    for (int i = 0; i < 20; ++i)
        processStereoBlock (processor, quiet, 512);

    SpectrumFrame afterReset;
    bool sawLowerMax = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (200);
    while (std::chrono::steady_clock::now() < deadline && ! sawLowerMax)
    {
        if (processor.getLatestSpectrumFrame (afterReset) && afterReset.hasSecondary)
        {
            float maxAfter = -1000.0f;
            for (int i = 0; i < afterReset.numPoints; ++i)
                maxAfter = std::max (maxAfter, afterReset.secondaryDb[static_cast<size_t> (i)]);
            if (maxAfter < maxBefore - 20.0f) // a clear, unambiguous drop
                sawLowerMax = true;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    CHECK (sawLowerMax);

    processor.setAnalysisActive (false);
}
