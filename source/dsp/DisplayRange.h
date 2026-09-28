#pragma once

#include <algorithm>

namespace investigate
{

/** The effective spectrum display range, computed with exactly the formulas
    in the roadmap's parameter-table constraints section. `SpectrumMapper`
    (analysis thread) and the editor's grid/axis/hover code (T-012, message
    thread) both call these functions instead of re-deriving the formulas,
    so the two stay identical by construction. Parameters themselves are
    never rewritten; only these derived "effective" values are clamped.

    All four formulas operate on plain `double` values, independent of any
    JUCE or APVTS type, so this header has no dependency beyond the standard
    library and can be included from both `source/dsp/` and `source/gui/`.
*/
namespace DisplayRange
{
    /** fHiEff = min (freqHigh, sampleRate / 2): the display never shows above
        Nyquist, regardless of the `freqHigh` parameter value. */
    inline double effectiveFreqHigh (double freqHigh, double sampleRate) noexcept
    {
        return std::min (freqHigh, sampleRate * 0.5);
    }

    /** fLoEff = min (freqLow, fHiEff / 2): guarantees the display spans at
        least one octave, even if `freqLow` is set above `fHiEff / 2`. Must
        be computed from `fHiEff`, not from `freqHigh` directly. */
    inline double effectiveFreqLow (double freqLow, double freqHighEffective) noexcept
    {
        return std::min (freqLow, freqHighEffective * 0.5);
    }

    /** dbHiEff = dbHigh: the top of the dB range is never clamped. Provided
        for symmetry with the other three formulas and so callers never need
        to special-case it. */
    inline double effectiveDbHigh (double dbHigh) noexcept
    {
        return dbHigh;
    }

    /** dbLoEff = min (dbLow, dbHigh - 12): guarantees the display spans at
        least 12 dB, even if `dbLow` is set above `dbHigh - 12`. Must be
        computed from `dbHigh`, not from `dbHiEff` (they are equal, but this
        keeps the formula's dependency explicit). */
    inline double effectiveDbLow (double dbLow, double dbHigh) noexcept
    {
        return std::min (dbLow, dbHigh - 12.0);
    }
}

} // namespace investigate
