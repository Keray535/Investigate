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

    Zoom (M3b): the display points are spread over the clamped view range
    (`SpectrumSettings::viewFreqLow/High`, see `DisplayRange::
    clampViewFrequency`) instead of the whole home range. Smoothing is
    computed over a range padded by half a smoothing window on each side,
    clipped to the effective home range, and only the visible points are
    output. A zoomed curve therefore equals the unzoomed curve at the same
    frequencies; only the home range edges keep the truncated window of the
    unzoomed mapping, so with no view set the output is bit-identical to the
    pre-zoom implementation.

    Threading: intended to run on the analysis thread only, like
    `SpectrumAnalyser`. `prepare()` allocates; `reset()` and `mapCurve()`
    never do. Not safe to call concurrently from multiple threads.
*/
class SpectrumMapper
{
public:
    /** Largest visible point count (`SpectrumFrame::maxVisiblePoints`). */
    static constexpr int maxPoints = SpectrumFrame::maxVisiblePoints;
    static constexpr int maxOverviewPoints = SpectrumFrame::overviewPoints;

    /** Worst case of visible, margin and padding points: the minimum view
        span (1/6 octave) with 1/3-octave smoothing pads by a full
        `numPoints - 1` per side beyond the margin, so 3 x 2048 + 2 x 512. */
    static constexpr int maxPaddedPoints = 3 * maxPoints + 2 * SpectrumFrame::maxMarginPoints;

    /** Largest padded count of the visible points alone (no margin). */
    static constexpr int maxVisiblePaddedPoints = 3 * maxPoints;

    /** The point layout of one published frame. */
    struct FrameLayout
    {
        int numPoints = 0;         ///< Published points, margin included.
        int firstVisible = 0;      ///< Margin points before the view.
        int numVisible = 0;        ///< Visible points.
    };

    /** The effective frequency ranges a set of settings maps over. */
    struct Ranges
    {
        double homeLow = 0.0;  ///< Effective home range (`DisplayRange`).
        double homeHigh = 0.0;
        double viewLow = 0.0;  ///< The view clamped into the home range.
        double viewHigh = 0.0;
    };

    /** Derives the effective home range from `freqLow`/`freqHigh`/
        `sampleRate` and clamps the requested view into it. Pure; the engine
        uses it to detect view and home-range changes. */
    static Ranges computeRanges (const SpectrumSettings& settings) noexcept;

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

    /** Computes the log-spaced display-point frequencies for the clamped
        view range (`computeRanges()`; the whole effective home range when no
        view is set) into `outFreqHz[0 .. numPoints)`, and caches the
        per-point bin-domain edge table, including the smoothing padding,
        that `mapCurve()` uses. The first and last point sit exactly on the
        view ends. Must be
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

        Smoothing runs over the visible points plus up to half a smoothing
        window of padding on each side (clipped to the effective home range),
        so zooming does not bend the curve at the view edges. Only the
        visible `numPoints` values are written. `freqHz` is accepted for API
        compatibility; the mapper uses the frequencies it cached itself in
        `computeFrequencies()` (they include the padding points).

        Real-time-safe: no allocation. `outDb` may alias none of the mapper's
        internal buffers (it never does when called through
        `SpectrumEngine`). */
    void mapCurve (const float* binPower, int numBins, double binHz,
                    const SpectrumSettings& settings,
                    const float* freqHz, int numPoints,
                    float* outDb) noexcept;

    /** Frame variant of `computeFrequencies()`: also extends the grid by the
        pan margin. For `numVisible` visible points over the clamped view it
        adds `SpectrumFrame::marginPointsFor (numVisible)` points on each side
        (25 % of the visible span in octaves), each side clipped to the
        effective home range, on the same grid (same step, the visible points
        are computed exactly as in `computeFrequencies()`). Writes all
        `layout.numPoints` frequencies to `outFreqHz` (at most
        `SpectrumFrame::maxPoints`) and returns the layout. Pair with
        `mapCurveFrame()`. Never allocates. */
    FrameLayout computeFrameFrequencies (const SpectrumSettings& settings, float* outFreqHz, int numVisible) noexcept;

    /** Maps one bin-domain power curve onto every point of the layout set up
        by the last `computeFrameFrequencies()` call, margin included, into
        `outDb[0 .. layout.numPoints)`. The visible points are bit-identical
        to what `mapCurve()` writes for the same view. The margin points use
        the same smoothing and slope; their smoothing window is padded beyond
        the margin and clipped to the home range, so they equal a frame
        mapped for a shifted view in which they are visible. Real-time-safe:
        no allocation. */
    void mapCurveFrame (const float* binPower, int numBins, double binHz,
                         const SpectrumSettings& settings, float* outDb) noexcept;

    /** Maps primary curve A over the whole effective home range onto
        `settings.overviewPoints` (clamped to `maxOverviewPoints`) log-spaced
        points, with the same smoothing and slope as `mapCurve()`, for the
        zoom overview strip. Writes the frequencies and dB values and returns
        the number of points. Independent of the view and of the state
        `computeFrequencies()` caches. Real-time-safe: no allocation. */
    int mapOverview (const float* binPower, int numBins, double binHz,
                      const SpectrumSettings& settings,
                      float* outFreqHz, float* outDb) noexcept;

private:
    /** One log-spaced point grid: the published points (visible plus margin)
        and the smoothing padding around them, with the bin-domain edge table
        and the slope term for the published points. Two exist: the view grid
        and the overview grid.

        Index spaces. The "array" runs over `total` points, `padLow` of them
        before view point 0. View point `v` (0 .. visible - 1) sits at array
        index `padLow + v`; the published points are the array range
        `[padLow - marginLow, padLow + visible + marginHigh)`. The smoothing
        of the visible points runs over its own sub-range (the T-035 padded
        range) so it stays bit-identical to the grid without margin. */
    struct Grid
    {
        std::vector<float> freqHz;       ///< Array points (total entries).
        std::vector<double> edgesHz;     ///< total + 1 edges.
        std::vector<double> slopeLog2;   ///< log2 (f / 1 kHz), published points only.

        int visible = 0;                 ///< Number of visible points.
        int padLow = 0;                  ///< Array points before view point 0 (margin plus padding).
        int total = 0;                   ///< Array size.
        int halfWidth = 0;               ///< Smoothing half width, in points.
        int marginLow = 0;               ///< Margin points before the view.
        int marginHigh = 0;              ///< Margin points after the view.
        int visPadLow = 0;               ///< Smoothing padding of the visible points, low side.
        int visPadHigh = 0;              ///< ... and high side.

        // Cache key: the grid is rebuilt only when one of these changes.
        bool valid = false;
        double keyViewLow = 0.0, keyViewHigh = 0.0, keyHomeLow = 0.0, keyHomeHigh = 0.0;
        int keyNumPoints = 0;
        bool keyWithMargin = false;
        SmoothingMode keySmoothing = SmoothingMode::off;

        int numPublished() const noexcept { return marginLow + visible + marginHigh; }
        void allocate (int maxTotal, int maxPublished);
    };

    static double smoothingOctaves (SmoothingMode mode) noexcept;
    static int smoothingHalfWidthPoints (SmoothingMode mode, double stepOctaves, int numPoints) noexcept;

    static void buildGrid (Grid& grid, double viewLow, double viewHigh, double homeLow, double homeHigh,
                            int numPoints, SmoothingMode smoothing, bool withMargin) noexcept;

    void mapOnGrid (const Grid& grid, const float* binPower, int numBins, double binHz,
                     const SpectrumSettings& settings, float* outDb) noexcept;

    // Scratch, sized to maxPaddedPoints (+1 for the prefix-sum tables) in
    // prepare(); reused across every mapCurve() call, never resized.
    std::vector<float> rawPower;      ///< Bin->point mapped power, linear.
    std::vector<float> smoothedPower; ///< Boxcar-smoothed power, linear.
    std::vector<double> cumSum;       ///< Prefix sum of rawPower over the whole array, size maxPaddedPoints + 1.
    std::vector<double> cumSumVisible; ///< Prefix sum over the visible points' padded range.

    Grid viewGrid;
    Grid overviewGrid;
};

} // namespace investigate
