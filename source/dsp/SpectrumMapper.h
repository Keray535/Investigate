#pragma once

#include "SpectrumFrame.h"
#include "SpectrumSettings.h"

#include <vector>

namespace investigate
{

/** Maps a bin-domain power spectrum (linear power, 1.0 == 0 dBFS, as
    produced by `SpectrumAnalyser`) onto log-spaced display points, then
    applies fractional-octave smoothing, slope around a 1 kHz pivot, and a
    floored dB conversion. Pure DSP: no dependency on the processor, the
    editor or APVTS.

    Usage per published frame: call `computeFrequencies()` once (it also
    caches the bin-to-point edge table used by `mapCurve()`), then call
    `mapCurve()` once per curve (primary, primary B, the one or two
    secondary curves, the sidechain curve) with the *same* `settings` and
    the `freqHz` array `computeFrequencies()` just filled in.

    Threading: intended to run on the analysis thread only, like
    `SpectrumAnalyser`. `prepare()` allocates; `reset()` and `mapCurve()`
    never do. Not safe to call concurrently from multiple threads.
*/
class SpectrumMapper
{
public:
    static constexpr int maxPoints = SpectrumFrame::maxPoints;

    /** Allocates every scratch buffer at `maxPoints`, so no `mapCurve()` or
        `computeFrequencies()` call ever allocates. Call once, off the audio
        thread, before first use. */
    void prepare();

    /** Clears scratch state. Not required for correctness (every element
        used by `mapCurve()` is fully overwritten before being read on every
        call), but kept for interface consistency with the other DSP
        classes and as a defensive reset between test cases. Never
        allocates. */
    void reset() noexcept;

    /** Computes the log-spaced display-point frequencies for the effective
        range (`DisplayRange::effectiveFreqLow/High`, from `settings.freqLow`
        /`freqHigh`/`sampleRate`) into `outFreqHz[0 .. numPoints)`, and caches
        the per-point bin-domain edge table that `mapCurve()` uses. Must be
        called before any `mapCurve()` calls for this frame; those calls
        must use the same `numPoints` and the `outFreqHz` array filled in
        here. `numPoints` is clamped to `[1, maxPoints]`. Never allocates. */
    void computeFrequencies (const SpectrumSettings& settings, float* outFreqHz, int numPoints) noexcept;

    /** Maps one bin-domain power curve to the `numPoints` display points
        set up by the most recent `computeFrequencies()` call (same
        `numPoints`, same `freqHz` array), writing the result in dB into
        `outDb[0 .. numPoints)`. `binHz` is the FFT bin spacing in Hz
        (`sampleRate / fftSize`) for the curve being mapped.

        Pipeline (roadmap / T-002 section 2):
         1. Bin -> point mapping: linear power-domain interpolation between
            the two neighbouring bins where points are denser than bins
            (low frequencies), peak-preserving aggregation over the
            enclosed bins where bins are denser than points (high
            frequencies).
         2. Fractional-octave smoothing: a power-domain boxcar mean over a
            fixed number of neighbouring points (constant in index space,
            since points are log-spaced), computed via a prefix sum so the
            cost is O(numPoints) regardless of the smoothing width. Skipped
            when `settings.smoothing == SmoothingMode::off`.
         3. dB conversion, then slope: `+slopeDbPerOct * log2 (f / 1000)`
            added post-smoothing (T-002 section 2), then a final -200 dB
            floor (the roadmap's "convert to dB with a -200 dB floor" is the
            last step, after slope, matching T-002's "pre-final-dB-clamp"
            wording - this makes the slope test's dB deltas exact, since the
            underlying power values in that test are never near the floor).

        Real-time-safe: no allocation. `outDb` may alias none of the mapper's
        internal buffers (it never does when called through
        `SpectrumEngine`). */
    void mapCurve (const float* binPower, int numBins, double binHz,
                    const SpectrumSettings& settings,
                    const float* freqHz, int numPoints,
                    float* outDb) noexcept;

private:
    static double smoothingOctaves (SmoothingMode mode) noexcept;
    int smoothingHalfWidthPoints (SmoothingMode mode, int numPoints) const noexcept;

    // Scratch, sized to maxPoints (+1 for the edge/prefix-sum tables) in
    // prepare(); reused across every mapCurve() call, never resized.
    std::vector<float> rawPower;      ///< Bin->point mapped power, linear.
    std::vector<float> smoothedPower; ///< Boxcar-smoothed power, linear.
    std::vector<double> cumSum;       ///< Prefix sum of rawPower, size maxPoints + 1.
    std::vector<double> edgesHz;      ///< Per-point frequency edges, size maxPoints + 1.

    int cachedNumPoints = 0;     ///< numPoints from the last computeFrequencies() call.
    double pointStepLog2 = 0.0;  ///< log2-frequency spacing between adjacent points.
};

} // namespace investigate
