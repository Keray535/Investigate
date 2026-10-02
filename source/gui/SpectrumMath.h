#pragma once

#include "DesignTokens.h"

#include <juce_graphics/juce_graphics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

/** Pure arithmetic and text formatting for the spectrum view (docs/design/
    ui-spec.md sections 5.1 to 5.6). No component state and no painting, so
    the unit tests can pin every resolved value without constructing a UI.

    The effective ranges (`fLo`, `fHi`, `dbLo`, `dbHi`) are always passed in:
    the caller gets them from `DisplayContext`, which calls the shared
    `source/dsp/DisplayRange.h`. Nothing here re-derives those formulas. */
namespace investigate::SpectrumMath
{

namespace sp = investigate::tokens::spectrum;

//==============================================================================
/** Maps frequency to x and level to y over the data rect (ui-spec 5.3, 5.5).
    x is log in frequency, y is linear in dB with `dbHi` at the top. */
struct Mapping
{
    double fLo = 20.0, fHi = 20000.0;
    double dbLo = -90.0, dbHi = 6.0;
    double x0 = 0.0, y0 = 0.0, w = 1.0, h = 1.0;

    double freqToX (double f) const noexcept
    {
        return x0 + std::log (f / fLo) / std::log (fHi / fLo) * w;
    }

    double xToFreq (double x) const noexcept
    {
        return fLo * std::pow (fHi / fLo, (x - x0) / w);
    }

    double dbToY (double db) const noexcept
    {
        return y0 + (dbHi - db) / (dbHi - dbLo) * h;
    }

    double yToDb (double y) const noexcept
    {
        return dbHi - (y - y0) / h * (dbHi - dbLo);
    }

    /** y for a level, with non-finite values and values outside the range
        pinned to the data-rect edges (ui-spec 5.1: points outside the range
        are clamped to the edge; the mockup puts non-finite at the bottom). */
    double dbToYClamped (double db) const noexcept
    {
        if (! std::isfinite (db))
            return y0 + h;

        return std::clamp (dbToY (db), y0, y0 + h);
    }
};

//==============================================================================
namespace detail
{
    inline juce::String minus() { return juce::String::fromUTF8 ("\xE2\x88\x92"); }
}

/** Rounds to 3 significant figures, half away from zero (ui-spec 5.5). */
inline double roundToThreeSignificant (double v) noexcept
{
    if (! (v > 0.0) || ! std::isfinite (v))
        return v;

    const double magnitude = std::pow (10.0, std::floor (std::log10 (v)) - 2.0);
    return std::round (v / magnitude) * magnitude;
}

/** "43.7 Hz", "437 Hz", "4.37 kHz", "14.3 kHz". Rounds to 3 significant
    figures first, so 999.6 gives "1.00 kHz" (ui-spec 5.5). */
inline juce::String formatFrequency (double hz)
{
    const double r = roundToThreeSignificant (hz);

    if (r < 100.0)   return juce::String (r, 1) + " Hz";
    if (r < 1000.0)  return juce::String ((int) std::lround (r)) + " Hz";
    if (r < 10000.0) return juce::String (r / 1000.0, 2) + " kHz";

    return juce::String (r / 1000.0, 1) + " kHz";
}

/** The right-click clipboard text: Hz, no unit, 1 decimal below 100 Hz and
    none above ("43.7", "1250", "14300"), ui-spec 5.6. */
inline juce::String formatFrequencyForClipboard (double hz)
{
    const double r = roundToThreeSignificant (hz);
    return r < 100.0 ? juce::String (r, 1) : juce::String ((int) std::lround (r));
}

/** "A4 +0c", "A#4 U+2212 50c" (ui-spec 5.5). A4 = 440 Hz, sharps only. Empty
    for a non-positive or non-finite frequency. */
inline juce::String noteName (double hz)
{
    if (! (hz > 0.0) || ! std::isfinite (hz))
        return {};

    static constexpr std::array<const char*, 12> pitchClass { "C", "C#", "D", "D#", "E", "F",
                                                              "F#", "G", "G#", "A", "A#", "B" };

    const double s = 12.0 * std::log2 (hz / 440.0);
    const int n = (int) std::floor (s + 0.5);
    const int cents = (int) std::round ((s - n) * 100.0); // half away from zero
    const int midi = 69 + n;
    const int pc = ((midi % 12) + 12) % 12;
    const int octave = (int) std::floor ((double) midi / 12.0) - 1;

    return juce::String (pitchClass[(size_t) pc]) + juce::String (octave) + " "
         + (cents >= 0 ? juce::String ("+") : detail::minus()) + juce::String (std::abs (cents)) + "c";
}

/** "-23.4 dB" with U+2212, "0.0 dB", and "-inf dB" for non-finite values or
    values at or below -200 (ui-spec 5.5). */
inline juce::String formatLevel (double db)
{
    if (! std::isfinite (db) || db <= sp::floorDb)
        return detail::minus() + "inf dB";

    const double r = std::round (std::abs (db) * 10.0) / 10.0;

    if (r == 0.0)
        return "0.0 dB";

    return (db < 0.0 ? detail::minus() : juce::String()) + juce::String (r, 1) + " dB";
}

/** Level at `hz`: linear interpolation in dB between the two neighbouring
    display points, in log frequency (ui-spec 5.5). Frequencies outside the
    frame clamp to the end point. Returns -infinity when either neighbour is
    non-finite or at the -200 dB floor. `freq` must be strictly increasing. */
inline double interpolateLevel (const float* freq, const float* db, int n, double hz) noexcept
{
    constexpr double negInf = -std::numeric_limits<double>::infinity();
    constexpr double floorDb = sp::floorDb;

    if (n <= 0)
        return negInf;

    const auto valid = [] (float v) { return std::isfinite (v) && (double) v > floorDb; };

    if (n == 1 || hz <= (double) freq[0])
        return valid (db[0]) ? (double) db[0] : negInf;

    if (hz >= (double) freq[n - 1])
        return valid (db[n - 1]) ? (double) db[n - 1] : negInf;

    const float* upper = std::upper_bound (freq, freq + n, (float) hz);
    const int i1 = std::clamp ((int) (upper - freq), 1, n - 1);
    const int i0 = i1 - 1;

    if (! valid (db[i0]) || ! valid (db[i1]))
        return negInf;

    const double lo = std::log ((double) freq[i0]);
    const double hi = std::log ((double) freq[i1]);
    const double t = hi > lo ? std::clamp ((std::log (hz) - lo) / (hi - lo), 0.0, 1.0) : 0.0;
    return (double) db[i0] * (1.0 - t) + (double) db[i1] * t;
}

//==============================================================================
/** The hover card position (ui-spec 5.5): above and to the right of the
    pointer, flipped left when it would pass the right edge, flipped below when
    it would pass the top, then clamped inside the data rect with a 4 px inset.
    The card never sits under the pointer. */
inline juce::Point<int> placeHoverCard (juce::Point<float> pointer, int cardW, int cardH,
                                         juce::Rectangle<int> data) noexcept
{
    constexpr float offset = (float) sp::cardOffset;
    constexpr int inset = sp::cardInset;

    float x = pointer.x + offset;
    float y = pointer.y - offset - (float) cardH;

    if (x + (float) cardW > (float) (data.getRight() - inset))
        x = pointer.x - offset - (float) cardW;

    if (y < (float) (data.getY() + inset))
        y = pointer.y + offset;

    const int px = (int) std::lround (x);
    const int py = (int) std::lround (y);

    const int minX = data.getX() + inset, maxX = std::max (minX, data.getRight() - inset - cardW);
    const int minY = data.getY() + inset, maxY = std::max (minY, data.getBottom() - inset - cardH);

    return { std::clamp (px, minX, maxX), std::clamp (py, minY, maxY) };
}

//==============================================================================
// Grid and axis labels (ui-spec 5.3)

/** Axis label text for a frequency: "20", "300", "1k", "20k". */
inline juce::String formatFrequencyLabel (double hz)
{
    if (hz < 1000.0)
        return juce::String ((int) std::lround (hz));

    const double k = hz / 1000.0;
    const double nearest = std::round (k);
    return std::abs (k - nearest) < 1.0e-9 ? juce::String ((int) nearest) + "k" : juce::String (k, 1) + "k";
}

/** The multipliers (the m in m x 10^k) a label set labels: set 0 is
    {1,2,3,5,7}, set 1 is {1,2,5}, set 2 is decades only. */
inline bool isLabelledMultiple (int labelSet, int multiple) noexcept
{
    switch (labelSet)
    {
        case 0:  return multiple == 1 || multiple == 2 || multiple == 3 || multiple == 5 || multiple == 7;
        case 1:  return multiple == 1 || multiple == 2 || multiple == 5;
        default: return multiple == 1;
    }
}

/** The densest label set whose smallest gap between adjacent label centres is
    at least `minGapPx` (36 in the spec), falling back to decades only. */
inline int chooseFrequencyLabelSet (double fLo, double fHi, double widthPx, double minGapPx = sp::freqLabelMinGap) noexcept
{
    const double logRange = std::log (fHi / fLo);

    for (int set = 0; set < 2; ++set)
    {
        double previousX = -1.0e30;
        double smallestGap = 1.0e30;

        for (int k = (int) std::floor (std::log10 (fLo)); k <= (int) std::ceil (std::log10 (fHi)); ++k)
        {
            for (int m = 1; m <= 9; ++m)
            {
                const double f = (double) m * std::pow (10.0, k);

                if (! isLabelledMultiple (set, m) || f < fLo || f > fHi)
                    continue;

                const double x = std::log (f / fLo) / logRange * widthPx;
                smallestGap = std::min (smallestGap, x - previousX);
                previousX = x;
            }
        }

        if (smallestGap >= minGapPx)
            return set;
    }

    return 2;
}

/** True when the pixel gap between the 9 x 10^k and 10^(k+1) lines is at
    least 4 px; otherwise unlabelled minor lines are skipped (ui-spec 5.3). */
inline bool minorFrequencyLinesVisible (double fLo, double fHi, double widthPx) noexcept
{
    return std::log (10.0 / 9.0) / std::log (fHi / fLo) * widthPx >= sp::minorLineMinGap;
}

/** The smallest of {1, 2, 3, 6, 12, 24} dB whose pixel spacing is at least
    20 px; 24 when none qualifies. */
inline double chooseDbStep (double dbLo, double dbHi, double heightPx) noexcept
{
    constexpr std::array<double, 6> steps { 1.0, 2.0, 3.0, 6.0, 12.0, 24.0 };
    const double pxPerDb = heightPx / (dbHi - dbLo);

    for (const double s : steps)
        if (s * pxPerDb >= sp::dbStepMinPx)
            return s;

    return steps.back();
}

//==============================================================================
// Curve geometry (ui-spec 5.1, "Curve geometry")

/** The extremes of one physical pixel column: the highest-level point (smallest
    y) and the lowest-level point (largest y), with their x positions. */
struct Column
{
    float xTop = 0.0f, yTop = 0.0f;
    float xBottom = 0.0f, yBottom = 0.0f;
    bool used = false;
};

/** Reduces `n` points to per-column extremes. `xs` are the points' x
    positions (shared by every curve of a frame, so the logs are paid once).
    `pixelsPerUnit` is the physical-to-logical scale. Points outside
    [m.x0, m.x0 + m.w] are dropped (the data rect clips them). `cols` must
    hold `numCols` entries; every entry is reset first. */
inline void reduceToColumns (const float* xs, const float* db, int n, const Mapping& m,
                             float pixelsPerUnit, Column* cols, int numCols) noexcept
{
    for (int c = 0; c < numCols; ++c)
        cols[c].used = false;

    if (numCols <= 0)
        return;

    const float left = (float) m.x0;
    const float right = (float) (m.x0 + m.w);

    for (int i = 0; i < n; ++i)
    {
        const float x = xs[i];

        if (x < left || x > right)
            continue;

        const float y = (float) m.dbToYClamped ((double) db[i]);
        auto& col = cols[std::clamp ((int) ((x - left) * pixelsPerUnit), 0, numCols - 1)];

        if (! col.used)
        {
            col = { x, y, x, y, true };
            continue;
        }

        if (y < col.yTop)    { col.yTop = y;    col.xTop = x; }
        if (y > col.yBottom) { col.yBottom = y; col.xBottom = x; }
    }
}

/** For each physical pixel column, the topmost (smallest) y reached by the
    polyline through the points, so a fill drawn as one bar per column from
    that y down to the baseline lies under the same silhouette as the polygon
    under the polyline and stays hidden beneath the curve's stroke.

    `xs` and `ys` are the point positions (x ascending, the display grid is
    log-spaced); only segments with both ends inside [left, right] count, as
    in `reduceToColumns`. Columns the polyline never touches get +infinity.
    `tops` must hold `numCols` entries. O(points + columns). */
inline void computeColumnTops (const float* xs, const float* ys, int n, float left, float right,
                               float pixelsPerUnit, float* tops, int numCols) noexcept
{
    constexpr float none = std::numeric_limits<float>::infinity();

    for (int c = 0; c < numCols; ++c)
        tops[c] = none;

    if (numCols <= 0 || n < 2)
        return;

    const float colWidth = 1.0f / pixelsPerUnit;

    for (int i = 0; i + 1 < n; ++i)
    {
        float xa = xs[i], ya = ys[i], xb = xs[i + 1], yb = ys[i + 1];

        if (xb < xa)
        {
            std::swap (xa, xb);
            std::swap (ya, yb);
        }

        if (xa < left || xb > right)
            continue;

        const int c0 = std::clamp ((int) ((xa - left) * pixelsPerUnit), 0, numCols - 1);
        const int c1 = std::clamp ((int) ((xb - left) * pixelsPerUnit), 0, numCols - 1);

        if (c0 == c1 || xb <= xa)
        {
            for (int c = c0; c <= c1; ++c)
                tops[c] = std::min ({ tops[c], ya, yb });

            continue;
        }

        const float slope = (yb - ya) / (xb - xa);

        for (int c = c0; c <= c1; ++c)
        {
            const float colL = left + (float) c * colWidth;
            const float x0 = std::max (xa, colL);
            const float x1 = std::min (xb, colL + colWidth);
            const float y0 = ya + slope * (x0 - xa);
            const float y1 = ya + slope * (x1 - xa);
            tops[c] = std::min ({ tops[c], y0, y1 });
        }
    }
}

/** Streaming polyline simplifier (Reumann-Witkam strip). Feed vertices in
    order; it emits a subset such that every dropped vertex lies within
    `tolerance` of the line through the emitted neighbours (twice that in the
    worst case), and never drops a vertex that turns back along the line.
    Collinear runs, which dense points on a smooth curve form, collapse to
    their end points; a jagged curve passes through unchanged. At most one
    vertex is emitted per `add`, and one more from `finish`. */
class PolylineSimplifier
{
public:
    struct Vertex { float x = 0.0f, y = 0.0f; };

    explicit PolylineSimplifier (float toleranceToUse) noexcept : tolerance (toleranceToUse) {}

    /** Returns true and sets `out` if a vertex is to be emitted. */
    bool add (float x, float y, Vertex& out) noexcept
    {
        if (count == 0)
        {
            anchor = { x, y };
            count = 1;
            out = anchor;
            return true;
        }

        if (count == 1)
        {
            startStrip ({ x, y });
            return false;
        }

        const float rx = x - anchor.x, ry = y - anchor.y;
        const float along = rx * dirX + ry * dirY;
        const float across = std::abs (rx * dirY - ry * dirX);

        if (across <= tolerance && along >= pendingAlong - tolerance)
        {
            pending = { x, y };
            pendingAlong = std::max (pendingAlong, along);
            return false;
        }

        out = pending;
        anchor = pending;
        startStrip ({ x, y });
        return true;
    }

    /** Emits the final pending vertex, if any. */
    bool finish (Vertex& out) noexcept
    {
        if (count != 2)
            return false;

        out = pending;
        count = 1;
        anchor = pending;
        return true;
    }

private:
    void startStrip (Vertex p) noexcept
    {
        const float rx = p.x - anchor.x, ry = p.y - anchor.y;
        const float len = std::sqrt (rx * rx + ry * ry);

        if (len < 1.0e-6f)
        {
            count = 1;
            return;
        }

        pending = p;
        pendingAlong = len;
        dirX = rx / len;
        dirY = ry / len;
        count = 2;
    }

    float tolerance;
    int count = 0;   // 0: nothing yet, 1: anchor only, 2: anchor and a pending vertex
    Vertex anchor, pending;
    float dirX = 0.0f, dirY = 0.0f, pendingAlong = 0.0f;
};

//==============================================================================
/** Frame pacing (T-029). Direct2D's message-thread cost per frame grows with
    the repainted physical area and with the number of curve layers; above
    either limit the curves repaint at 30 fps instead of every vblank.
    Measured (Release, Direct2D, i7-12700H, new frame every 60 Hz vblank): the
    plot needs about 17 to 20 % of a core at 1280 x 720 with all seven layers
    and 22 to 25 % at 2560 x 1440, against gates of 18 % and 20 %. */
namespace pacing
{
    /** Physical pixels of the data rect above which frames are paced. */
    constexpr double minPlotAreaPx = 2.0e6;

    /** Layer count at which frames are paced at any size. */
    constexpr int minLayerCount = 7;

    constexpr double minIntervalMs = 30.0;

    /** Primary A stroke and fill, primary B stroke (dual), one or two
        secondary strokes, sidechain stroke and fill. */
    constexpr int countLayers (bool dual, bool secondary, bool sidechain) noexcept
    {
        return 2 + (dual ? 1 : 0) + (secondary ? (dual ? 2 : 1) : 0) + (sidechain ? 2 : 0);
    }

    constexpr bool isPaced (double plotAreaPx, int layerCount) noexcept
    {
        return plotAreaPx > minPlotAreaPx || layerCount >= minLayerCount;
    }
}

} // namespace investigate::SpectrumMath
