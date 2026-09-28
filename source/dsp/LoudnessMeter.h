#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace investigate
{

/** BS.1770-4/-5 and EBU R128 loudness meter: momentary (400 ms), short-term
    (3 s) and gated integrated loudness, EBU Tech 3342 loudness range (LRA),
    and the maximum momentary/short-term values reached since reset().

    Algorithm summary (see the task log in docs/tasks/T-006-loudness-meter.md
    for the full derivation and docs/research/T-006-ebu-test-cases.md for the
    conformance values this was validated against):

    - K-weighting: BS.1770 Annex 1's two-stage filter - a high-shelf
      "pre-filter" (models the acoustic effect of the head) cascaded with an
      RLB high-pass. Both stages are analog prototypes (f0, gain, Q)
      converted to digital biquads per sample rate with the bilinear
      transform - not by resampling to 48 kHz (architect decision, roadmap
      Decisions 2026-09-28: resampling adds audio-thread cost for no
      accuracy gain). The prototype parameter values are the ones libebur128
      (MIT, github.com/jiixyj/libebur128, read locally at
      third_party/libebur128-src/ebur128/ebur128.c for T-006) derives its
      own 48 kHz coefficients from; at 48 kHz this reproduces the
      coefficients tabulated in ITU-R BS.1770-4 Annex 1 / section 5.4 to
      better than 1e-6 (verified numerically for T-006).
    - Momentary loudness: 400 ms sliding window. Short-term: 3 s sliding
      window. Both are recomputed on a 10 ms internal sub-block grid, finer
      than the ~10 Hz minimum display-update rate BS.1770 itself asks for,
      and finer than the 100 ms hop Integrated Loudness's own algorithm
      requires (see below) - not a display-rate choice, a correctness one:
      EBU Tech 3341's conformance cases 13/14 place a 400 ms tone burst at
      every 20 ms-spaced offset and expect Max Momentary to read the
      tone's true level regardless of offset. A 100 ms recompute grid can
      only exactly align with offsets that are themselves multiples of
      100 ms, which left up to ~0.22 LU of edge error on 4 of every 5
      offsets (found by T-006's unit-tester, round 1, and confirmed to be
      shared bit-for-bit with libebur128, which uses the same 100 ms grid
      for Momentary). A 10 ms grid aligns exactly with every one of Tech
      3341's 20 ms-spaced offsets (400 ms and 20 ms are both multiples of
      10 ms), removing the error without touching Integrated Loudness's
      required algorithm at all.
    - Integrated loudness: BS.1770 Annex 2 / EBU R128 gating over 400 ms
      blocks with 75% overlap - an exact 100 ms hop, which *is* mandated by
      the standard's own algorithm (unlike Momentary/Short-term's internal
      grid, which is an implementation freedom). Implemented by sampling
      the current momentary/short-term sliding-window value once every ten
      10 ms sub-blocks, so the gating cadence matches BS.1770 Annex 2 and
      libebur128 exactly even though Momentary/Short-term update on a
      finer internal grid - absolute gate at -70 LUFS, relative gate at
      -10 LU below the absolute-gated mean.
    - Loudness range (EBU Tech 3342 section 3.1): built from the (ungated)
      3 s short-term values, sampled every 100 ms - absolute gate at
      -70 LUFS, relative gate at -20 LU below the absolute-gated mean (a
      different constant from Integrated Loudness's -10 LU - the two gates
      must not be conflated). LRA = 95th minus 10th percentile of what
      remains, using the standard's rank-based (not interpolated) indexing.
    - Both gated measurements share the same bounded-memory technique:
      counts in a fixed 1000-bin, 0.1 LU histogram spanning -70.0 to
      +30.0 LUFS, using each bin's *centre* energy as the representative
      value for every block that lands in it. This is exactly libebur128's
      EBUR128_MODE_HISTOGRAM technique (same bin grain, same bin-centre
      convention, same "round the relative-threshold bin up if the
      threshold sits above the bin's own centre" rule), replicated here to
      keep the cross-check against it well inside the required tolerance.
      No dynamic memory after prepare(): a programme of any length uses the
      same fixed 1000 x 2 counters.
    - Channel weights: BS.1770 weights L and R (and Investigate's mono case)
      at 1.0. Investigate never analyses the surround channels BS.1770
      weights at 1.41, so no per-channel weight table exists. Loudness sums
      (not averages) the per-channel mean square, which is why an identical
      mono signal reads about 3 dB (10*log10(2)) quieter than the same
      signal on both channels of a stereo input - that is BS.1770's defined
      behaviour, not a bug.

    Sentinels (never NaN for any legal input): getMomentaryLoudness(),
    getShortTermLoudness(), getIntegratedLoudness() and getMaxMomentary()/
    getMaxShortTerm() read negativeInfinity() before enough data exists.
    getLoudnessRange() reads 0.0 before any value has passed both of its
    gates (matching libebur128: LRA is a non-negative range, so negative
    infinity would be a misleading "no data" marker).

    Pure DSP: depends only on the standard library, so it is testable
    without a plugin instance. prepare() may allocate; reset() and
    process() never do. process() is real-time-safe: every loop it runs is
    bounded by prepare()-time constants (the sample loop by numSamples, the
    gating passes by the fixed 1000 histogram bins), never by the audio
    content itself.

    Threading and denormals: intended to run on the audio thread, one
    instance per meter. Not safe to call concurrently from multiple
    threads. Like the other source/dsp/ classes, this class assumes the
    caller has already engaged `juce::ScopedNoDenormals` for the duration
    of process() (the roadmap has the processor do this once per
    processBlock, ahead of the whole MeterBank) rather than doing so itself
    on every call. */
class LoudnessMeter
{
public:
    static constexpr int maxChannels = 2;

    LoudnessMeter() = default;

    /** Allocates per-channel filter and accumulator state sized for
        `numChannels` (clamped to 1..maxChannels - the roadmap's mono-mono
        or stereo-stereo main bus) and derives the K-weighting coefficients
        for `sampleRate` (any rate from 44.1 to 192 kHz). Also resets every
        measurement, as if reset() had been called. `maxBlockSize` is
        accepted for API symmetry with the other source/dsp/ classes;
        process() itself has no fixed block-size limit. May allocate. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Clears filter memory, the sub-block accumulators, the sliding
        momentary/short-term windows, both gating histograms, the cached
        integrated/LRA values and the max-momentary/max-short-term
        sentinels. Never allocates. */
    void reset() noexcept;

    /** Clears only the gated measurements: both histograms, integrated
        loudness, loudness range, and the max-momentary/max-short-term
        sentinels. Leaves the K-weighting filter state and the momentary/
        short-term sliding windows (and their current values) untouched -
        added for T-007's MeterBank::requestReset(), which the roadmap
        specifies resets "integrated loudness, LRA, and the max momentary
        and short-term values" but *not* the current momentary/short-term
        readouts themselves (those, like RMS and the correlation meter, are
        continuous rolling-window readouts, not "since reset" quantities).
        Never allocates. */
    void resetStatistics() noexcept;

    /** K-weights, accumulates and (at every 10 ms sub-block boundary)
        updates momentary and short-term, and (every tenth sub-block, i.e.
        every 100 ms) updates integrated and LRA, for `numSamples`
        new samples on each channel of `channels`. `numChannels` is
        clamped to the channel count passed to prepare() (a mismatch is
        not expected in normal use - the roadmap re-prepares every meter on
        a bus-layout change - but is handled without reading or writing
        outside the prepared channel range). `channels[c]` must have
        `numSamples` valid samples for every c < the clamped channel count.
        Real-time-safe: no allocation, no locks, no data-dependent
        unbounded loop. */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** The sentinel every "not enough data yet" / "nothing passed the
        gate" getter reports, except getLoudnessRange() (see the class
        comment). Never NaN. */
    static constexpr float negativeInfinity() noexcept { return -std::numeric_limits<float>::infinity(); }

    /** Momentary (400 ms sliding window) loudness in LUFS, ungated,
        updated every 10 ms. negativeInfinity() until 400 ms of audio has
        been processed since prepare()/reset(). */
    float getMomentaryLoudness() const noexcept { return momentaryLoudness; }

    /** Short-term (3 s sliding window) loudness in LUFS, ungated, updated
        every 10 ms. negativeInfinity() until 3 s of audio has been
        processed since prepare()/reset(). */
    float getShortTermLoudness() const noexcept { return shortTermLoudness; }

    /** Gated integrated loudness in LUFS (BS.1770 Annex 2 / EBU R128).
        negativeInfinity() until at least one 400 ms block has passed both
        the absolute and the relative gate. */
    float getIntegratedLoudness() const noexcept { return integratedLoudness; }

    /** Loudness range in LU (EBU Tech 3342). 0.0 until at least one 3 s
        short-term value has passed both gates. */
    float getLoudnessRange() const noexcept { return loudnessRange; }

    /** Maximum momentary/short-term loudness reached since the last
        reset(). negativeInfinity() until the first momentary/short-term
        value exists. */
    float getMaxMomentary() const noexcept { return maxMomentary; }
    float getMaxShortTerm() const noexcept { return maxShortTerm; }

private:
    // --- K-weighting biquads (double precision: BS.1770's high-pass pole
    // sits close to DC, so single precision coefficients/state would lose
    // meaningful accuracy at low frequencies - CLAUDE.md's "double where
    // precision matters" rule). Direct Form I: simple, and numerically fine
    // for this filter's pole radius (~0.99 at 48 kHz).
    struct BiquadCoeffs
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    };

    struct BiquadState
    {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

        double process (double x, const BiquadCoeffs& c) noexcept
        {
            const double y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }

        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }
    };

    static BiquadCoeffs deriveShelfCoeffs (double sampleRate) noexcept;
    static BiquadCoeffs deriveRlbCoeffs (double sampleRate) noexcept;

    static double energyToLoudness (double energy) noexcept;

    // --- Gating histogram: 1000 bins, 0.1 LU wide, spanning
    // [-70.0, +30.0) LUFS. Shared bin layout for the Integrated Loudness
    // histogram and the LRA histogram (they are separate arrays, but use
    // the same indexing and bin-centre-energy functions).
    static constexpr int kHistogramBins = 1000;
    static constexpr double kHistogramLuStep = 0.1;
    static constexpr double kHistogramLowLufs = -70.0;
    static constexpr double kAbsoluteGateLufs = -70.0;

    static int histogramBinForLoudness (double loudnessLufs) noexcept;
    static double histogramBinCenterEnergy (int bin) noexcept;

    void onSubBlockComplete() noexcept;
    void addIntegratedGatingBlock (double meanPower) noexcept;
    void addLraSample (double meanPower) noexcept;
    void recomputeIntegratedLoudness() noexcept;
    void recomputeLoudnessRange() noexcept;

    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    int preparedChannels = 2;

    BiquadCoeffs shelfCoeffs;
    BiquadCoeffs rlbCoeffs;
    std::vector<BiquadState> shelfState;
    std::vector<BiquadState> rlbState;

    // Per-channel sum of K-weighted squared samples within the current
    // 10 ms sub-block; reset to 0 every time the sub-block completes.
    std::vector<double> subBlockSumSquares;
    int subBlockSamples = 480;      ///< round(sampleRate * 0.01), >= 1.
    int subBlockSampleCounter = 0;

    // Momentary (400 ms = 40 sub-blocks) and short-term (3 s = 300
    // sub-blocks) sliding sums, each its own small fixed-size ring buffer
    // so the two windows don't need index arithmetic against a shared
    // array. The 10 ms sub-block grid (see the class comment) is finer
    // than the 100 ms hop Integrated Loudness/LRA gating requires -
    // kGatingHopSubBlocks below samples this sliding sum down to that
    // required cadence.
    static constexpr int kMomentaryBlocks = 40;
    static constexpr int kShortTermBlocks = 300;
    static constexpr int kGatingHopSubBlocks = 10; ///< 10 * 10 ms = 100 ms, BS.1770 Annex 2's required gating hop.
    int gatingSubBlockCounter = 0;

    std::array<double, kMomentaryBlocks> momentaryRing {};
    int momentaryWritePos = 0;
    int momentaryCount = 0;
    double momentarySum = 0.0;

    std::array<double, kShortTermBlocks> shortTermRing {};
    int shortTermWritePos = 0;
    int shortTermCount = 0;
    double shortTermSum = 0.0;

    std::array<uint64_t, kHistogramBins> integratedHistogram {};
    uint64_t integratedHistogramTotal = 0;

    std::array<uint64_t, kHistogramBins> lraHistogram {};
    uint64_t lraHistogramTotal = 0;

    float momentaryLoudness = negativeInfinity();
    float shortTermLoudness = negativeInfinity();
    float integratedLoudness = negativeInfinity();
    float loudnessRange = 0.0f;
    float maxMomentary = negativeInfinity();
    float maxShortTerm = negativeInfinity();
};

} // namespace investigate
