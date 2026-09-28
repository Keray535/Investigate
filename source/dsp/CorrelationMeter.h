#pragma once

#include <array>
#include <cstdint>

namespace investigate
{

/** Stereo phase correlation over a fixed 500 ms window, and an L/R balance
    readout over a fixed 3 s window (task T-007; roadmap Decisions
    2026-09-28 and Parameter table constraints: both windows are fixed,
    not user-adjustable). Pure DSP: depends only on the standard library.

    Algorithm summary:
    - corr = SUM(L*R) / sqrt(SUM(L^2) * SUM(R^2)), balance_dB =
      10*log10(SUM(R^2) / SUM(L^2)) (T-002 section 5's documented
      formulas; negative balance means the left channel is louder, per the
      roadmap).
    - **Rectangular window via sub-block partial sums** (the first of the
      two techniques the task allows, T-002 section 5 / roadmap Decisions):
      a single 10 ms sub-block cadence accumulates (SUM(L*R), SUM(L^2),
      SUM(R^2)) into a 300-slot ring (300 * 10 ms = 3 s, the longer of the
      two windows - correlation and balance share the same ring rather
      than keeping two). Both readouts are a **full recompute** every
      process() call (sum the last 50 ring entries for the 500 ms
      correlation window, all 300 for the 3 s balance window - at most 350
      double additions, done once per call, not per sample or per
      sub-block) - like LevelMeter's RMS, this is deliberately not an
      incrementally-maintained running sum, so it cannot drift.
    - **Epsilon-clamped denominators** (T-002 section 5's recommendation):
      a tiny epsilon is added to both sides of the balance ratio and to the
      correlation denominator. For true silence (or before any data has
      arrived), every sum is exactly 0, so both ratios evaluate to exactly
      0/eps = 0 (correlation) and eps/eps = 1 -> 0 dB (balance) - no
      special-casing needed, and never NaN, for any input.
    - **Mono** (numChannels < 2, passed straight through from
      MeterBank::process()): short-circuits to correlation = +1, balance =
      0 dB, isMono() = true, with no accumulation at all (roadmap: "A mono
      input reports a correlation of +1... and sets isMono").
    - MeterBank's requestReset() does **not** reset this meter: correlation
      and balance are continuous rolling-window readouts, not "since
      reset" quantities (the roadmap's reset list is peak max, TP max,
      clip counts, integrated loudness, LRA, M max and S max - correlation
      is not in that list).

    Threading and denormals: intended for the audio thread, one instance
    per CorrelationMeter (MeterBank owns exactly one). Not safe to call
    concurrently from multiple threads. Like the other source/dsp/ classes,
    this class assumes the caller has already engaged
    juce::ScopedNoDenormals for the duration of process(). */
class CorrelationMeter
{
public:
    CorrelationMeter() = default;

    /** Derives the 10 ms sub-block length for `sampleRate` and resets
        every measurement, as if reset() had been called. `maxBlockSize`
        and `numChannels` are accepted for API symmetry with the other
        source/dsp/ classes; this meter always reads exactly the first two
        channel pointers process() is given, gated by the numChannels
        argument passed to process() itself. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Clears the sub-block ring, the in-progress sub-block accumulator,
        and the cached correlation/balance values. Never allocates. */
    void reset() noexcept;

    /** Accumulates `numSamples` new samples from `channels[0]`
        (left/only) and, when `numChannels >= 2`, `channels[1]` (right),
        then recomputes the cached correlation and balance values.
        `numChannels < 2` sets isMono() and skips accumulation entirely.
        Real-time-safe: no allocation, no locks, every loop bounded by
        numSamples or the fixed 300-slot ring, never by the audio content
        itself. */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** -1 to +1 over the fixed 500 ms window. 0 for silence, before any
        data, or when isMono(); never NaN. */
    float getCorrelation() const noexcept { return correlationValue; }

    /** L/R balance in dB over the fixed 3 s window; negative means the
        left channel is louder. 0 for silence, before any data, or when
        isMono(); never NaN. */
    float getBalanceDb() const noexcept { return balanceValue; }

    /** True when the most recent process() call was given fewer than 2
        channels. */
    bool isMono() const noexcept { return monoFlag; }

private:
    static constexpr int kRingSlots = 300;                ///< 300 * 10 ms = 3000 ms, the longer (balance) window.
    static constexpr int kCorrelationWindowBlocks = 50;   ///< 50 * 10 ms = 500 ms.
    static constexpr int kBalanceWindowBlocks = 300;      ///< 300 * 10 ms = 3000 ms.
    static constexpr double kSubBlockMs = 10.0;
    static constexpr double kEpsilon = 1e-20;

    struct SubBlockSums
    {
        double sumLR = 0.0;
        double sumL2 = 0.0;
        double sumR2 = 0.0;
    };

    void recomputeCorrelationAndBalance() noexcept;

    double sampleRate = 48000.0;
    int subBlockSamples = 480;
    int subBlockSampleCounter = 0;

    std::array<SubBlockSums, kRingSlots> ring {};
    uint64_t completedSubBlocks = 0;   ///< monotonically increasing; ring index via % kRingSlots.
    SubBlockSums currentSubBlock {};

    bool monoFlag = false;
    float correlationValue = 0.0f;
    float balanceValue = 0.0f;
};

} // namespace investigate
