#pragma once

#include <algorithm>
#include <array>

namespace investigate
{

/** A small fixed-size ring of per-sub-block linear peak maxima. LevelMeter
    and TruePeakDetector both need to report "the maximum over the last
    50 ms" (roadmap "Display-rate peaks": samplePeak and truePeak are the
    maximum over the last 50 ms, not the current block, so a 30-60 Hz reader
    can never miss a block's peak) without storing per-sample history - this
    is the "small fixed ring of sub-block maxima" the task names directly.

    Usage: the owner accumulates a running peak for the sample currently in
    progress and calls push() with that value once every sub-block boundary
    (typically every 10 ms - see LevelMeter/TruePeakDetector), then resets
    its own in-progress accumulator to 0. currentMax() combines the ring
    (completed sub-blocks) with the caller's still-accumulating in-progress
    peak, so the very latest, not-yet-flushed samples are never missed.

    With Slots = 5 and a 10 ms sub-block, the ring covers the last 5
    *completed* sub-blocks (50 ms), and currentMax() adds up to one more
    partial sub-block on top - so a peak is visible for at least 50 ms after
    it is pushed (worst case: pushed at the exact instant a sub-block
    completes, evicted only after 5 further pushes = 5*10 ms later),
    satisfying the "at least 50 ms" acceptance criterion with the
    "+-1 block of granularity" the task allows. */
template <int Slots>
class PeakHoldRing
{
public:
    void reset() noexcept
    {
        ring.fill (0.0f);
        pos = 0;
        cachedMax = 0.0f;
    }

    /** Called once per completed sub-block with that sub-block's linear
        peak. Recomputes the cached ring maximum by scanning all Slots
        entries (Slots is small - 5 for the 50 ms display ring - so this is
        cheap and, unlike an incrementally-maintained running max, cannot
        drift or go stale when a value leaves the window). */
    void push (float subBlockPeakLinear) noexcept
    {
        ring[static_cast<size_t> (pos)] = subBlockPeakLinear;
        pos = (pos + 1) % Slots;
        cachedMax = *std::max_element (ring.begin(), ring.end());
    }

    /** The maximum over the ring's completed sub-blocks and the caller's
        still-accumulating in-progress peak. */
    float currentMax (float inProgressPeakLinear) const noexcept
    {
        return std::max (cachedMax, inProgressPeakLinear);
    }

private:
    std::array<float, Slots> ring {};
    int pos = 0;
    float cachedMax = 0.0f;
};

} // namespace investigate
