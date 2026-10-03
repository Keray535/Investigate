#pragma once

#include <algorithm>
#include <cmath>

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
    // ---- View window (zoom and pan, milestone M3b) ------------------------

    /** Smallest visible frequency span, in octaves, and smallest visible
        level span, in dB. Shared by the mapper (analysis thread) and the
        editor so both clamp a view identically. The home range keeps its own
        1-octave / 12 dB rules above. */
    inline constexpr double minViewSpanOctaves = 1.0 / 6.0;
    inline constexpr double minViewSpanDb = 6.0;

    /** A low/high pair: a frequency view in Hz or a level view in dB. */
    struct Range
    {
        double low = 0.0;
        double high = 0.0;
    };

    namespace detail
    {
        /** Relative tolerance under which a span counts as "at least the
            minimum". It makes the clamps idempotent: an output whose span is
            the minimum up to rounding passes through a second clamp
            untouched, bit for bit. */
        inline constexpr double spanTolerance = 1.0e-9;
    }

    /** Clamps a frequency view `(lo, hi)` into the home range
        `(homeLo, homeHi)` (the effective home range from the functions
        above). The result lies inside the home range, spans at least
        `minViewSpanOctaves` (or the whole home range if that is smaller), and
        keeps the view's logarithmic centre where it can: a view that would
        poke out of the home range is slid back in, not squashed. `(0, 0)`,
        or any input that is not a positive finite pair, means "no view" and
        returns the home range. The function is pure and idempotent
        (`clamp (clamp (x)) == clamp (x)` exactly). Edges that reach the home
        range are returned as exactly the home range values. */
    inline Range clampViewFrequency (double lo, double hi, double homeLo, double homeHi) noexcept
    {
        const Range home { homeLo, homeHi };

        if (! (homeLo > 0.0 && homeHi > homeLo))
            return home;

        if (! (std::isfinite (lo) && std::isfinite (hi) && lo > 0.0 && hi > 0.0))
            return home;

        if (hi < lo)
            std::swap (lo, hi);

        const double logHomeLo = std::log2 (homeLo);
        const double logHomeHi = std::log2 (homeHi);
        const double homeSpan = logHomeHi - logHomeLo;
        const double minSpan = std::min (minViewSpanOctaves, homeSpan);

        // Already a legal view: pass it through unchanged.
        if (lo >= homeLo && hi <= homeHi && std::log2 (hi / lo) >= minSpan * (1.0 - detail::spanTolerance))
            return { lo, hi };

        const double logLo = std::log2 (lo);
        const double logHi = std::log2 (hi);
        const double span = std::clamp (logHi - logLo, minSpan, homeSpan);

        if (span >= homeSpan)
            return home;

        const double centre = 0.5 * (logLo + logHi);
        double newLo = centre - 0.5 * span;
        double newHi = centre + 0.5 * span;

        if (newLo < logHomeLo)
        {
            newHi += logHomeLo - newLo;
            newLo = logHomeLo;
        }
        else if (newHi > logHomeHi)
        {
            newLo -= newHi - logHomeHi;
            newHi = logHomeHi;
        }

        Range out { std::exp2 (newLo), std::exp2 (newHi) };

        // Snap edges that reached the home range exactly, and guard against
        // exp2 rounding stepping outside it.
        if (newLo <= logHomeLo)
            out.low = homeLo;
        if (newHi >= logHomeHi)
            out.high = homeHi;

        out.low = std::clamp (out.low, homeLo, homeHi);
        out.high = std::clamp (out.high, homeLo, homeHi);
        return out;
    }

    /** The level-axis counterpart of `clampViewFrequency`: linear in dB, with
        `minViewSpanDb` as the smallest span. `(0, 0)` or non-finite input
        returns the home range. Pure and idempotent. */
    inline Range clampViewDb (double lo, double hi, double homeLo, double homeHi) noexcept
    {
        const Range home { homeLo, homeHi };

        if (! (std::isfinite (homeLo) && std::isfinite (homeHi) && homeHi > homeLo))
            return home;

        if (! (std::isfinite (lo) && std::isfinite (hi)) || (lo == 0.0 && hi == 0.0))
            return home;

        if (hi < lo)
            std::swap (lo, hi);

        const double homeSpan = homeHi - homeLo;
        const double minSpan = std::min (minViewSpanDb, homeSpan);

        if (lo >= homeLo && hi <= homeHi && (hi - lo) >= minSpan * (1.0 - detail::spanTolerance))
            return { lo, hi };

        const double span = std::clamp (hi - lo, minSpan, homeSpan);

        if (span >= homeSpan)
            return home;

        const double centre = 0.5 * (lo + hi);
        double newLo = centre - 0.5 * span;
        double newHi = centre + 0.5 * span;

        if (newLo < homeLo)
        {
            newHi += homeLo - newLo;
            newLo = homeLo;
        }
        else if (newHi > homeHi)
        {
            newLo -= newHi - homeHi;
            newHi = homeHi;
        }

        return { std::clamp (newLo, homeLo, homeHi), std::clamp (newHi, homeLo, homeHi) };
    }
}

} // namespace investigate
