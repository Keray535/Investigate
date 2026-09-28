#pragma once

#include "PeakHoldRing.h"

#include <array>
#include <cstdint>

namespace investigate
{

/** BS.1770 Annex 2 polyphase-FIR true-peak detector: oversamples each
    channel by a sample-rate-dependent factor, reports the interpolated
    true peak and a true-peak maximum since reset, and a true-peak clip
    counter (task T-007). Pure DSP: depends only on the standard library
    plus PeakHoldRing.h.

    Algorithm summary:
    - **Filter.** ITU-R BS.1770-5 Annex 2, Attachment 1 publishes a single
      order-48, 4-phase FIR interpolating filter, printed already split
      into 4 columns ("Phase 0".."Phase 3") of 12 taps each - equivalently
      one 48-tap linear-phase prototype h[n] de-interleaved by
      phase_p[r] = h[4r+p]. Exact coefficients, transcription method and
      citation are in docs/research/T-007-true-peak-coefficients.md
      (fetched directly from the ITU-R BS.1770-5 PDF for this task, not
      typed from memory or copied from a third party's reimplementation,
      per the task's citation requirement). Phase 0 is the near-identity
      phase (its 12 coefficients sum to ~1.0016, and it carries a single
      dominant "center tap" coefficient rather than spreading energy across
      two taps like phases 1-3) - i.e. phase p approximates the
      fractional-sample offset p/4.
    - **Polyphase structure, per-tier kernel length (T-024).** For each new
      input sample, a tap-count-sized delay line holds the last N samples at
      the *original* sample rate, and each active phase's sub-filter is
      applied directly to that same delay line - the standard polyphase-
      interpolator computational shortcut: it produces the same oversampled
      output points a naive "zero-stuff then filter with the full
      prototype" implementation would, without ever materialising the
      oversampled signal. Earlier (T-007) both filter tables shared one
      25-element delay line and computePhase() loop (25 = ceil(49/2), sized
      for the 88.2/96 kHz filter below), so the ITU table's 12 real taps per
      phase were zero-padded to 25 - correct, but every one of the 13 added
      per-phase multiply-adds is a `coeff * anything` with `coeff` an exact
      `0.0f`, contributing nothing to the sum, at 44.1/48 kHz, the two most
      common sample rates and therefore the tier that runs on nearly every
      real-world block. That wasted work was the direct, measured cause of
      MeterBank::process()'s thin p99 margin against its 40 us budget in
      T-007 (up to 38.8 us measured, see the roadmap's T-007 decision row
      and docs/tasks/T-007-meters-meterbank.md's final log entries). T-024
      decouples the two tiers instead: kItuTapsPerPhase (12, exact, no
      padding) sizes `delay4x`/`computePhase4x` for the ITU table, and
      kHannSincTapsPerPhase (25, unchanged) sizes `delay2x`/`computePhase2x`
      for the Hann-sinc table below - `kPhaseCoeffs` itself is now a literal
      12-taps-per-phase table (the 13 trailing zero columns are deleted, not
      just left at 0, so "no zero-padded taps are multiplied" holds by
      construction). Since `x + 0.0f == x` and `0.0f * y == 0.0f` exactly for
      every finite x/y, removing those 13 exact-zero addends from each
      phase's sum cannot change its value or rounding at any intermediate
      step - the 4x tier's results are bit-for-bit identical to the T-007
      25-tap-padded computation, not merely close to it. The 2x tier's own
      table, delay line and loop are untouched (its phase-0 column being 24
      exact zeros plus one 1.0 is a genuine mathematical property of an
      odd-length/factor-2 polyphase design - see "The 88.2/96 kHz filter is
      not a subset of the ITU table" below - not a padding artefact; T-024's
      design requirement scopes the "own kernel length" fix to the 4x tier
      and keeps the 2x tier as "its current 49-tap, 2-phase design").
    - **No fixed-point attenuation/compensation.** Annex 2's algorithm
      description (docs/research/T-007-true-peak-coefficients.md,
      "Filter structure summary") attenuates the input by 12.04 dB before
      filtering (headroom for a fixed-point implementation) and adds
      12.04 dB back after taking the peak - explicitly noted in the
      standard as skippable in a floating-point implementation. Since the
      coefficients already sum to ~1.0 (unity gain) per phase, skipping
      *both* the attenuation and the compensation (rather than skipping
      only one) is the correct floating-point-equivalent behaviour: a
      full-scale DC input filtered by the near-identity phase yields
      ~1.0016, i.e. ~0 dBTP, with no extra offset needed. Adding the
      12.04 dB compensation without the matching input attenuation would
      overshoot every reading by 12 dB.
    - **Oversampling factor by sample rate** (T-002 section 4 / roadmap
      Decisions 2026-09-28, matching libebur128/ffmpeg's convention): 4x
      using the literal ITU 4-phase table above at 44.1/48 kHz (where
      BS.1770-5 asks for 4x, and where EBU Tech 3341 cases 15-23 are
      evaluated, "at 48 kHz" per the task); a *different*, independently
      derived 2-phase filter (below) at 88.2/96 kHz; and 1x (no filtering -
      the true peak equals the sample peak, matching BS.1770's own
      rationale that very high native rates already resolve inter-sample
      peaks) above that.
    - **The 88.2/96 kHz filter is not a subset of the ITU table.** Three
      earlier designs were tried and measured against unit-tester's
      libebur128 EBUR128_MODE_TRUE_PEAK cross-check (T-007 rounds 1-3)
      before this one:
        1. Reusing phases 0+2 of the ITU table: missed the task's +-0.2 dB
           tolerance by 0.02 dB on a wideband 20 Hz-20 kHz swept sine at
           96 kHz.
        2. All 4 ITU phases at 96 kHz: made no measurable difference to the
           swept sine (the global peak over a 5 s sweep differed by ~3e-7
           between the 2- and 4-phase ITU subsets) - so the swept-sine gap
           was never a resolution/tap-count problem with the ITU table
           itself.
        3. A from-scratch 24-tap/factor-2 Hann-windowed-sinc filter (2
           phases of 12 taps): closed the swept-sine gap to ~0.005 dB, but
           *broke* the fs/4 45-degree case at 96 kHz badly (a 0.69 dB miss,
           far worse than design 1's 0.02 dB overage on the swept sine) -
           it under-detects genuine fs/4-type inter-sample overshoot, the
           one thing BS.1770's own filter exists specifically to catch.
      Reading libebur128's own source
      (third_party/libebur128-src/ebur128/ebur128.c,
      ebur128_init_resampler() and interp_create()) explained why: it does
      not use the ITU table at any sample rate - it always generates its
      own Hann-windowed-sinc polyphase interpolator, with **49 taps**
      (factor 4 below 96 kHz, factor 2 from 96 kHz to below 192 kHz), not
      the 24-tap approximation design 3 used. A side-by-side sweep of tap
      counts (12, 16, 20, 24, 28, 32, 48 - all even) against the *same*
      formula showed every even tap count under-detects the fs/4 case by
      about the same 0.6-0.7 dB, while taps=49 (odd) detects it correctly,
      matching libebur128 to 5+ significant figures on *both* the fs/4 case
      and the swept sine. The reason is structural, not a tuning accident:
      with factor 2 and an *odd* total tap count, the centre tap (m=0 in
      the formula below) lands exactly on phase 0, and every other phase-0
      coefficient is an exact zero (sin(m*pi/2) is exactly 0 for every
      non-zero integer m) - so phase 0 of an odd-length, factor-2 design is
      always a pure identity (pass the original sample straight through),
      exactly mirroring the ITU table's own near-identity phase 0. An
      *even* tap count never produces that exact identity phase, and it is
      that identity phase which correctly resolves fs/4-type alternating
      peaks. This project's filter therefore uses the same **taps=49,
      factor=2** parametrisation as libebur128 (chosen for this structural
      reason, confirmed by the sweep above - not an attempt to copy
      libebur128's own coefficient values, which were never read; only its
      choice of taps/factor was, after which this project's own copy of
      the coefficients was independently generated from the public-domain
      formula below and verified against both required signal classes
      before being transcribed into the .cpp file): a 25-sample delay
      (ceil(49/2)), giving phase 0 as a pure identity (a single 1.0 at
      index 12, exact) and phase 1 as a 24-tap symmetric Hann-windowed-sinc
      kernel (index 24 is an exact zero-pad, since 49 is odd), from
      h[j] = sinc((j - (taps-1)/2) / factor) * 0.5*(1-cos(2*pi*j/(taps-1))),
      phase = j % factor, tap index = j / factor (Crochiere & Rabiner,
      "Multirate Digital Signal Processing," Prentice-Hall, 1983; also
      Smith, "Digital Audio Resampling Home Page," CCRMA - the standard,
      public-domain windowed-sinc fractional-delay-interpolator formula).
      This matches libebur128 to 5+ significant figures on both the fs/4
      case and the swept sine (see the task log for the full before/after
      numbers across all four designs). 44.1/48 kHz keeps the literal ITU
      table untouched throughout, since it already passes every criterion
      there.
    - **True-peak neighbourhood, per input sample.** For each new input
      sample, all active phases are evaluated immediately (using the
      delay line as it stands right after that sample was pushed); their
      max |value| is that sample's "true-peak neighbourhood" value. This
      feeds the same PeakHoldRing-based 50 ms display-peak mechanism
      LevelMeter uses (roadmap "Display-rate peaks"), the running
      truePeakMax, and the true-peak clip counter (documented rule, T-001
      section 3's "True Peak Clippings" equivalent): the counter increments
      **once per input sample** (not once per oversampled point) whenever
      that sample's neighbourhood value reaches or exceeds 1.0 (0 dBTP).

    Threading and denormals: intended for the audio thread, one instance
    per TruePeakDetector (MeterBank owns exactly one, prepared for up to 2
    channels). Not safe to call concurrently from multiple threads. Like
    the other source/dsp/ classes, this class assumes the caller has
    already engaged juce::ScopedNoDenormals for the duration of process(). */
class TruePeakDetector
{
public:
    static constexpr int maxChannels = 2;
    static constexpr int kItuTapsPerPhase = 12;       ///< Exact: the ITU table's real tap count per phase (4x tier, 44.1/48 kHz). No padding (T-024).
    static constexpr int kHannSincTapsPerPhase = 25;  ///< ceil(49/2): the 88.2/96 kHz Hann-sinc filter's own delay length (2x tier), unchanged from T-007.

    TruePeakDetector() = default;

    /** Allocates nothing beyond fixed-size members; selects the
        oversampling factor (4, 2 or 1) for `sampleRate` and resets every
        measurement, as if reset() had been called. `maxBlockSize` is
        accepted for API symmetry with the other source/dsp/ classes. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    /** Full reset: clears the FIR delay line, the display-peak ring,
        truePeakMax and the true-peak clip counter. Never allocates. Used
        by prepare(); MeterBank's requestReset() uses resetStatistics()
        instead (see its comment for why). */
    void reset() noexcept;

    /** Clears only truePeakMax and the true-peak clip counter - never the
        FIR delay line, so a mid-stream statistics reset introduces no
        artificial filter transient. Never allocates. */
    void resetStatistics() noexcept;

    /** Filters and analyses `numSamples` new samples on each channel of
        `channels`. `numChannels` is clamped to the channel count passed to
        prepare(). Real-time-safe: no allocation, no locks, every loop
        bounded by numSamples or a fixed small tap/phase count, never by
        the audio content itself. */
    void process (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Maximum interpolated true peak over the last 50 ms, dBTP.
        Decibels::negativeInfinityDb() before any data has been processed. */
    float getTruePeakDb (int channel) const noexcept;

    /** Maximum getTruePeakDb() reached since the last reset()/
        resetStatistics(). Decibels::negativeInfinityDb() before any data. */
    float getTruePeakMaxDb (int channel) const noexcept;

    /** Count of input samples whose interpolated neighbourhood reached or
        exceeded 1.0 (0 dBTP) since the last reset()/resetStatistics(). */
    uint64_t getTruePeakClipCount (int channel) const noexcept;

private:
    static constexpr int kDisplayRingSlots = 5;   ///< 5 * 10 ms = 50 ms, see LevelMeter's class comment for the same technique.
    static constexpr double kSubBlockMs = 10.0;

    struct ChannelState
    {
        // Two independently-sized delay lines (T-024): the 4x (ITU) tier
        // reads only its own 12 real taps, the 2x (Hann-sinc) tier reads
        // its own 25 - neither tier shifts or multiplies against the
        // other's, unlike T-007's single shared, zero-padded 25-element
        // array. delay*[0] = newest sample, delay*[N-1] = oldest.
        std::array<float, kItuTapsPerPhase> delay4x {};
        std::array<float, kHannSincTapsPerPhase> delay2x {};

        PeakHoldRing<kDisplayRingSlots> displayRing;
        float subBlockPeakLinear = 0.0f;

        float truePeakMaxLinear = 0.0f;
        uint64_t clipCount = 0;

        void push4x (float x) noexcept
        {
            for (int i = kItuTapsPerPhase - 1; i > 0; --i)
                delay4x[static_cast<size_t> (i)] = delay4x[static_cast<size_t> (i - 1)];
            delay4x[0] = x;
        }

        void push2x (float x) noexcept
        {
            for (int i = kHannSincTapsPerPhase - 1; i > 0; --i)
                delay2x[static_cast<size_t> (i)] = delay2x[static_cast<size_t> (i - 1)];
            delay2x[0] = x;
        }

        float computePhase4x (const std::array<float, kItuTapsPerPhase>& coeffs) const noexcept
        {
            float sum = 0.0f;
            for (int r = 0; r < kItuTapsPerPhase; ++r)
                sum += coeffs[static_cast<size_t> (r)] * delay4x[static_cast<size_t> (r)];
            return sum;
        }

        float computePhase2x (const std::array<float, kHannSincTapsPerPhase>& coeffs) const noexcept
        {
            float sum = 0.0f;
            for (int r = 0; r < kHannSincTapsPerPhase; ++r)
                sum += coeffs[static_cast<size_t> (r)] * delay2x[static_cast<size_t> (r)];
            return sum;
        }
    };

    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    int preparedChannels = 2;

    int oversampleFactor = 4;     ///< 4, 2 or 1, chosen in prepare() from sampleRate (see the class comment): selects which coefficient table process() uses (4 => the ITU table, 2 => the independently-derived Hann-sinc table, 1 => no filtering).
    int activePhaseCount = 4;     ///< 4, 2 or 0 (0 = no filtering, direct sample peak).

    int subBlockSamples = 480;
    int subBlockSampleCounter = 0;

    std::array<ChannelState, maxChannels> channels;

    void onSubBlockComplete() noexcept;
};

} // namespace investigate
