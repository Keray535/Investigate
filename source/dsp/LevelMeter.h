#pragma once

#include "DecibelUtils.h"
#include "PeakHoldRing.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace investigate
{

/** Per-channel sample peak, RMS over an adjustable window, and a sample
    clip counter (task T-007). Pure DSP: depends only on the standard
    library plus PeakHoldRing.h, so it is testable without a plugin
    instance and has no editor/AudioProcessor dependency.

    Algorithm summary:
    - A single 10 ms sub-block cadence (subBlockSamples = round(sampleRate
      * 0.01), the same technique LoudnessMeter (T-006) uses) drives two
      independent, always-exactly-recomputed mechanisms - never an
      incrementally-maintained running sum, so neither can drift:
        1. samplePeak: a 5-slot PeakHoldRing (5 * 10 ms = 50 ms) of
           per-sub-block linear peak maxima (roadmap "Display-rate peaks").
        2. rms: a 300-slot ring of per-sub-block sum-of-squares (300 *
           10 ms = 3000 ms, the maximum rmsWindow). getRmsDb() sums exactly
           the last round(rmsWindow / 10 ms) ring entries fresh every
           sub-block boundary (bounded <= 300 double additions, cheap) -
           this is a *true* sliding window (T-002 section 6's
           recommendation), not an exponential approximation, and every
           report is a full recompute, so 1 hour of simulated audio cannot
           accumulate floating-point drift the way a running
           add-new/subtract-oldest sum could.
    - peakMax and sampleClipCount update every sample directly (no extra
      per-sample cost: both reuse the |x| already computed for the
      sub-block peak) and are held until resetStatistics() - never cleared
      by the sub-block/ring machinery above, which keeps rolling
      regardless (the roadmap treats RMS and the 50 ms peaks as continuous
      readouts, not "since reset" quantities).
    - setRmsWindowMs() is a single std::atomic<float> store: real-time-safe
      from any thread, read once per sub-block boundary with relaxed
      ordering (only ever used to pick how many ring entries to sum, so a
      torn or stale read for one sub-block has no observable effect beyond
      that sub-block's RMS value).

    Threading and denormals: intended for the audio thread, one instance
    per LevelMeter (MeterBank owns exactly one, prepared for up to 2
    channels). Not safe to call concurrently from multiple threads, except
    setRmsWindowMs() which may be called from any thread. Like the other
    source/dsp/ classes, this class assumes the caller has already engaged
    juce::ScopedNoDenormals for the duration of process(). */
class LevelMeter
{
public:
    static constexpr int maxChannels = 2;

    LevelMeter() = default;

    /** Allocates per-channel state sized for `numChannels` (clamped to
        1..maxChannels) and derives the 10 ms sub-block length for
        `sampleRate`. Also resets every measurement, as if reset() had been
        called. May allocate. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Full reset: clears the display-peak ring, the RMS ring, peakMax and
        sampleClipCount. Never allocates. Used by prepare() and available
        for a full re-initialisation; MeterBank's requestReset() uses
        resetStatistics() instead (see its comment for why). */
    void reset() noexcept;

    /** Clears only peakMax and sampleClipCount (what MeterBank's
        requestReset() resets, per the roadmap: peak max and clip counts
        are "since reset" quantities, RMS and the 50 ms display peak are
        not). Never allocates. */
    void resetStatistics() noexcept;

    /** Sets the RMS averaging window in milliseconds, clamped to [50,
        3000] (the roadmap's rmsWindow parameter range). Real-time-safe: a
        single atomic store, callable from any thread. */
    void setRmsWindowMs (float windowMs) noexcept;

    /** Accumulates `numSamples` new samples on each channel of `channels`.
        `numChannels` is clamped to the channel count passed to prepare().
        Real-time-safe: no allocation, no locks, every loop bounded by
        numSamples or a fixed constant (<= 300 ring slots), never by the
        audio content itself. */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Maximum |sample| over the last 50 ms, dBFS. Decibels::negativeInfinityDb()
        before any data has been processed. */
    float getSamplePeakDb (int channel) const noexcept;

    /** Maximum getSamplePeakDb() reached since the last reset()/
        resetStatistics(). Decibels::negativeInfinityDb() before any data. */
    float getPeakMaxDb (int channel) const noexcept;

    /** RMS over the current rmsWindow, dBFS, unreferenced (AES17/K-system
        offsets are applied by the editor, never here). Decibels::negativeInfinityDb()
        before at least one sub-block has completed. */
    float getRmsDb (int channel) const noexcept;

    /** Count of samples with |x| >= 1.0 (0 dBFS) since the last reset()/
        resetStatistics(). */
    uint64_t getSampleClipCount (int channel) const noexcept;

private:
    static constexpr int kDisplayRingSlots = 5;   ///< 5 * 10 ms = 50 ms, see the class comment.
    static constexpr int kRmsRingSlots = 300;     ///< 300 * 10 ms = 3000 ms, the maximum rmsWindow.
    static constexpr double kSubBlockMs = 10.0;

    struct ChannelState
    {
        PeakHoldRing<kDisplayRingSlots> displayRing;
        float subBlockPeakLinear = 0.0f;

        std::array<double, kRmsRingSlots> rmsRing {};   ///< per-sub-block sum-of-squares.
        double subBlockSumSquares = 0.0;

        float peakMaxLinear = 0.0f;
        uint64_t sampleClipCount = 0;

        float cachedRmsDb = Decibels::negativeInfinityDb();
    };

    void onSubBlockComplete() noexcept;

    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    int preparedChannels = 2;

    int subBlockSamples = 480;   ///< round(sampleRate * 0.01), >= 1.
    int subBlockSampleCounter = 0;
    uint64_t rmsRingWritePos = 0;   ///< monotonically increasing; index into rmsRing via % kRmsRingSlots.
    uint64_t completedSubBlocks = 0;

    std::atomic<float> rmsWindowMs { 300.0f };

    std::array<ChannelState, maxChannels> channels;
};

} // namespace investigate
