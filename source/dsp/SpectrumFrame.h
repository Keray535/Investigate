#pragma once

#include <array>
#include <cstdint>

namespace investigate
{

/** One published spectrum frame: up to `maxPoints` log-spaced display
    points plus the curves derived from them, exactly as described in the
    roadmap's "SpectrumFrame contents" section. Fixed-size (no heap
    pointers, no dynamic containers) so it is trivially copyable and safe to
    hold in `FrameExchange`'s three preallocated slots - a whole frame is
    about 49 KB (6 arrays x 2048 floats), which is cheap next to the
    project's 64 MB per-instance budget.

    Only the first `numPoints` entries of every array are valid for a given
    frame; the rest are leftover data from a previous, possibly
    differently-sized frame and must be ignored. */
struct SpectrumFrame
{
    /// Fixed maximum point count (roadmap: "up to 2048 display points").
    static constexpr int maxPoints = 2048;

    /// Number of valid entries in every array below, 1 .. maxPoints.
    int numPoints = 0;

    /// Frequency of each display point in Hz, log-spaced from fLoEff to
    /// fHiEff (DisplayRange.h). Strictly increasing across [0, numPoints).
    std::array<float, maxPoints> freqHz {};

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

    /// Monotonically increasing across every frame published by one
    /// `SpectrumEngine` instance; never decreases. Used by `FrameExchange`
    /// and its tests to confirm no frame is ever delivered out of order.
    uint64_t sequence = 0;
};

} // namespace investigate
