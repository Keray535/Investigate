#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "params/ParameterLayout.h"
#include "params/ParameterRefs.h"

#include "dsp/MeterBank.h"
#include "dsp/SpectrumEngine.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace investigate
{

/**
    Investigate's AudioProcessor (T-008, wired to the DSP pipeline in T-009).

    Buses, parameters, state and host integration are all host-facing
    surface owned by plugin-engineer (docs/roadmap.md, Architecture >
    Modules). This class is a bit-exact, zero-latency pass-through over the
    roadmap's bus layouts (main mono-to-mono or stereo-to-stereo, with an
    optional mono or stereo sidechain input): the main buffer is never
    written. `MeterBank` runs on the audio thread every block; `SpectrumEngine`
    additionally receives samples while analysis is active (an editor is
    open), consumed off the audio thread by a private polling analysis
    thread (roadmap Threading model).
*/
class InvestigateProcessor final : public juce::AudioProcessor
{
public:
    InvestigateProcessor();
    ~InvestigateProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Editor-facing API (docs/roadmap.md, "Processor API used by the
    //      editor"). Each method documents the thread(s) it is safe to call
    //      from. ----

    /** The editor calls this from its constructor (true) and its destructor
        (false). Message-thread only; never called from the audio thread
        (roadmap: "The audio thread never signals it"). Reference-counted,
        so it stays correct if the host briefly has two editors open at
        once: the analysis thread starts on the first activation and stops
        only after the matching last deactivation. Starting may allocate
        (SpectrumEngine may reallocate on a subsequent settings change) and
        starts a juce::Thread; stopping joins it within 50 ms (roadmap
        Budgets: "Analysis thread stop <= 50 ms"). */
    void setAnalysisActive (bool shouldBeActive);

    /** Message-thread only. Returns true and fills `dest` with the newest
        published frame if one arrived since the last call; otherwise
        returns false and leaves `dest` untouched. Only meaningful while
        analysis is active (setAnalysisActive(true)); harmless to call
        otherwise (simply returns false). */
    bool getLatestSpectrumFrame (SpectrumFrame& dest) noexcept { return spectrumEngine.readLatest (dest); }

    /** Message thread (any thread is safe). Tells the analysis thread which
        frequency range the spectrum view shows, in Hz; (0, 0) is the home
        range. Both floats travel in one atomic 64-bit word, so the analysis
        thread never reads a mixed pair. The next published frame carries the
        clamped view in `viewFreqLow/High`. Reset to (0, 0) by the last
        `setAnalysisActive (false)`; never persisted in the state. */
    void setSpectrumView (float fLo, float fHi) noexcept;

    /** Message thread. Freezes or unfreezes the published spectrum. The
        freeze flag and a 32-bit epoch share one atomic; the epoch increments
        only when the state really changes, so repeating the current state is
        a no-op. A frame from a poll that began before the call may still
        carry the older epoch (the editor drops those); live frames never
        carry a frozen epoch. Reset to unfrozen by the last
        `setAnalysisActive (false)`. */
    void setSpectrumFrozen (bool shouldBeFrozen) noexcept;

    /** Current freeze epoch (any thread). Compare with
        `SpectrumFrame::freezeEpoch` to discard stale frames. */
    uint32_t getFreezeEpoch() const noexcept;

    /** Lock-free and callable from any thread (roadmap): forwards to
        MeterBank's seqlock-protected getSnapshot(). Meters run on every
        block regardless of analysis activity, so this is always current. */
    MeterSnapshot getMeterSnapshot() const noexcept { return meterBank.getSnapshot(); }

    /** Callable from any thread; a single atomic store consumed by
        MeterBank::process() on its next call (one block), which resets peak
        max, true-peak max, both clip counts, integrated loudness, LRA, and
        the max momentary/short-term values (roadmap). */
    void requestStatsReset() noexcept { meterBank.requestReset(); }

    /** Callable from any thread (intended caller: the message thread). Sets
        an atomic flag that the analysis thread consumes before its next
        processPending() call (so within one analysis frame) and forwards to
        SpectrumEngine::resetSecondary(), which is documented
        analysis-thread-only - the audio thread never calls it directly. */
    void requestSpectrumReset() noexcept { spectrumResetRequested.store (true, std::memory_order_release); }

    /** True only when the sidechain bus (input bus 1) is enabled with at
        least one channel (roadmap). Safe to call from any thread: reads the
        bus layout only, never allocates or blocks. */
    bool isSidechainActive() const noexcept
    {
        const auto* bus = getBus (true, 1);
        return bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0;
    }

    /** The editor (T-011), the analysis thread and any future test code
        reach every parameter, and its saved `ViewState` child, through this
        single object (roadmap: "juce::AudioProcessorValueTreeState&
        getApvts() noexcept"). */
    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }

    /** The audio-thread/analysis-thread parameter access helper, exposed so
        it is constructed once, next to the APVTS it reads from. */
    const ParameterRefs& getParameterRefs() const noexcept { return paramRefs; }

private:
    /** Restores the `ViewState` child's three properties to their T-010
        defaults if they are missing (a fresh instance, an older saved state,
        or a state that predates a given property), without touching any
        property that is already present. Called from both the constructor
        and setStateInformation, so "missing properties keep their defaults"
        (roadmap State format) holds in every case. */
    void ensureViewStateDefaults();

    juce::AudioProcessorValueTreeState apvts;
    ParameterRefs paramRefs;

    // ---- T-009: DSP wiring ----------------------------------------------

    MeterBank meterBank;
    SpectrumEngine spectrumEngine;

    // Bus/sample-rate snapshot as of the last prepareToPlay(), read by the
    // analysis thread (AnalysisThread holds references to these). Written
    // only from prepareToPlay(), which always stops the analysis thread
    // first (see the .cpp), so the analysis thread never reads these while
    // they are being written - no atomics needed for these three.
    double preparedSampleRate = 48000.0;
    int preparedNumMainChannels = 2;
    int preparedNumSidechainChannels = 0;

    // Message-thread-written, analysis-thread-read request flag for
    // requestSpectrumReset() (see its comment above).
    std::atomic<bool> spectrumResetRequested { false };

    // Guards analysisRefCount and every start/stop/prepare of
    // analysisThread. Only ever taken from the message thread
    // (setAnalysisActive) and whatever non-audio thread the host calls
    // prepareToPlay()/releaseResources() from - never the audio thread - so
    // this is not a real-time-safety violation (CLAUDE.md's "no locks"
    // rule is about code reachable from processBlock/the audio thread).
    juce::CriticalSection analysisLock;
    int analysisRefCount = 0;

    // Packed view (fLo low 32 bits, fHi high 32 bits) and freeze state
    // (epoch in the low 32 bits, frozen flag in bit 32). One word each, so
    // the analysis thread reads a consistent pair with a single load.
    std::atomic<uint64_t> viewBits { 0 };
    std::atomic<uint64_t> freezeBits { 0 };

    // The audio thread's only view into analysis activity: a plain atomic
    // flag, read once per processBlock(), never a lock.
    std::atomic<bool> analysisActive { false };

    class AnalysisThread; // Fully defined in PluginProcessor.cpp (private,
                           // derives from juce::Thread - roadmap/T-009
                           // design requirement).
    std::unique_ptr<AnalysisThread> analysisThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InvestigateProcessor)
};

} // namespace investigate
