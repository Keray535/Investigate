#pragma once

#include <array>
#include <cstdint>

namespace investigate
{

/** Margin points added to each side of `numVisible` visible points:
    `lround (0.25 * (numVisible - 1))`, i.e. 25 % of the visible span in
    octaves (one zoom-out notch exposes at most that much on one side). This
    is the integer form of the same rounding (halves round up). A namespace
    function, because a member function cannot be used in the class's own
    constants. */
constexpr int spectrumMarginPointsFor (int numVisible) noexcept
{
    return numVisible > 1 ? (numVisible + 1) / 4 : 0;
}

/** One published spectrum frame: up to `maxPoints` log-spaced display
    points plus the curves derived from them, exactly as described in the
    roadmap's "SpectrumFrame contents" section. Fixed-size (no heap
    pointers, no dynamic containers) so it is trivially copyable and safe to
    hold in `FrameExchange`'s three preallocated slots - a whole frame is
    75848 bytes (5 curve arrays + `freqHz`, 6 x 3072 floats, plus 2 x 256
    overview floats and the scalars), which is cheap next to the project's
    64 MB per-instance budget.

    Only the first `numPoints` entries of every array are valid for a given
    frame; the rest are leftover data from a previous, possibly
    differently-sized frame and must be ignored.

    Pan margin (T-039): a zoomed frame carries extra points beyond the
    visible view, on the same log-frequency grid, so a fast pan or a zoom-out
    has curve data to draw while the editor waits for the re-mapped frame.
    The visible view occupies `[firstVisiblePoint, firstVisiblePoint +
    numVisiblePoints)`; the rest are margin. At no zoom there is no margin. */
struct SpectrumFrame
{
    /// Largest number of visible display points a frame can be mapped with
    /// (roadmap: "up to 2048 display points"); `SpectrumSettings::numPoints`
    /// is clamped to it.
    static constexpr int maxVisiblePoints = 2048;

    /// Margin points per side for `numVisible` visible points, see
    /// `spectrumMarginPointsFor()`.
    static constexpr int marginPointsFor (int numVisible) noexcept { return spectrumMarginPointsFor (numVisible); }

    /// Largest margin per side.
    static constexpr int maxMarginPoints = spectrumMarginPointsFor (maxVisiblePoints);

    /// Fixed point capacity: the largest visible count with both margins
    /// (2048 + 2 x 512 = 3072).
    static constexpr int maxPoints = maxVisiblePoints + 2 * maxMarginPoints;

    static_assert (maxMarginPoints == 512, "the margin is 25 % of the visible span per side");
    static_assert (maxPoints == 3072, "capacity of 2048 visible points plus two margins of 512");
    static_assert (maxVisiblePoints + 2 * spectrumMarginPointsFor (maxVisiblePoints) <= maxPoints,
                   "the capacity must hold the maximum visible count with both margins");

    /// Fixed point count of the overview curve (zoom-and-pan mini-map).
    static constexpr int overviewPoints = 256;

    /// Number of valid entries in every array below, 1 .. maxPoints: the
    /// visible points plus the margin points.
    int numPoints = 0;

    /// The visible view is `[firstVisiblePoint, firstVisiblePoint +
    /// numVisiblePoints)` of the arrays below. The margin before it has
    /// `firstVisiblePoint` points, the margin after it
    /// `numPoints - firstVisiblePoint - numVisiblePoints`. With no zoom (or
    /// a view at both home range edges) `firstVisiblePoint` is 0 and
    /// `numVisiblePoints == numPoints`.
    int firstVisiblePoint = 0;
    int numVisiblePoints = 0;

    /// Frequency of each display point in Hz, log-spaced and strictly
    /// increasing across [0, numPoints). The visible points run from the view
    /// low to the view high; the margin points continue the same grid (same
    /// log step), clipped to the effective home range (DisplayRange.h).
    std::array<float, maxPoints> freqHz {};

    /// The frequency view the display points were mapped onto, in Hz: the
    /// requested view after `DisplayRange::clampViewFrequency`. Equals
    /// `freqHz[firstVisiblePoint]` and `freqHz[firstVisiblePoint +
    /// numVisiblePoints - 1]` (up to float rounding). At no zoom this is the
    /// effective home range.
    double viewFreqLow = 0.0;
    double viewFreqHigh = 0.0;

    /// The primary curve in dB: single-mode curve, or L/Mid in dual modes.
    /// -200 dB floor, smoothing and slope already applied, not clamped to
    /// the display dB range (the editor does that).
    std::array<float, maxPoints> primaryDb {};

    /// The second primary curve (R or Side), valid only when `isDual`.
    std::array<float, maxPoints> primaryBDb {};

    /// The secondary curve (RT max / all-time max / all-time average) for
    /// `primaryDb`, valid only when `hasSecondary`.
    std::array<float, maxPoints> secondaryDb {};

    /// The secondary curve for `primaryBDb`, valid only when `isDual &&
    /// hasSecondary` (two secondary curves in the dual modes, per the
    /// roadmap).
    std::array<float, maxPoints> secondaryBDb {};

    /// The sidechain reference curve, valid only when `hasSidechain`. In
    /// the dual modes this is the power mean of the sidechain channels
    /// (roadmap SpectrumFrame note); in single modes it uses the same
    /// channel combination as the main curve.
    std::array<float, maxPoints> sidechainDb {};

    /// Overview of the primary curve (A) over the whole effective home range,
    /// log-spaced, with the same smoothing and slope as `primaryDb`. Only the
    /// first `overviewNumPoints` entries are valid.
    int overviewNumPoints = 0;
    std::array<float, overviewPoints> overviewFreqHz {};
    std::array<float, overviewPoints> overviewDb {};

    /// True when `primaryBDb`/`secondaryBDb` carry a second curve (L/R dual
    /// or M/S dual channel mode with a stereo main input).
    bool isDual = false;

    /// True when `secondaryDb` (and `secondaryBDb` if `isDual`) is valid,
    /// i.e. `secondary != SecondaryMode::off`.
    bool hasSecondary = false;

    /// True when `sidechainDb` is valid, i.e. the analyser has an active
    /// sidechain input. Independent of the editor's `showSidechain` toggle.
    bool hasSidechain = false;

    /// The sample rate this frame was analysed at.
    double sampleRate = 48000.0;

    /// Bin spacing of the data the frame was mapped from, in Hz (sample rate
    /// / FFT size). A frame mapped from the freeze snapshot carries the
    /// snapshot's value, so a block-size change while frozen does not change
    /// it. 0 when unset.
    double binHz = 0.0;

    /// Monotonically increasing across every frame published by one
    /// `SpectrumEngine` instance; never decreases. Used by `FrameExchange`
    /// and its tests to confirm no frame is ever delivered out of order.
    uint64_t sequence = 0;

    /// The freeze epoch of the settings this frame was published under
    /// (`SpectrumSettings::freezeEpoch`). While frozen it is the epoch of the
    /// freeze whose snapshot the frame was mapped from; the editor drops
    /// frames whose epoch is not the current one.
    uint32_t freezeEpoch = 0;
};

} // namespace investigate
