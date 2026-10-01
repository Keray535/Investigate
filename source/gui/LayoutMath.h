#pragma once

#include "DesignTokens.h"

#include <juce_graphics/juce_graphics.h>

#include <algorithm>
#include <array>
#include <cmath>

/** Pure layout arithmetic for the editor (docs/design/ui-spec.md sections 2,
    4.1, 4.3, 4.4). No component state, so the unit tests can pin the spec's
    resolved positions without constructing any UI. */
namespace investigate::LayoutMath
{

namespace L = investigate::tokens::layout;

struct EditorSize
{
    int width = L::defaultWidth;
    int height = L::defaultHeight;

    bool operator== (const EditorSize&) const = default;
};

/** Clamps to the spec's minimum and maximum (ui-spec 2). */
inline EditorSize clampEditorSize (int width, int height) noexcept
{
    return { std::clamp (width, L::minWidth, L::maxWidth),
             std::clamp (height, L::minHeight, L::maxHeight) };
}

//==============================================================================
struct Regions
{
    juce::Rectangle<int> header;
    juce::Rectangle<int> plot;        // spectrum plot region (bg-plot)
    juce::Rectangle<int> controlBar;
    juce::Rectangle<int> meterStrip;  // empty when collapsed
    int meterStripWidth = 0;          // MS in the spec
};

/** ui-spec 4.1. */
inline Regions computeRegions (int w, int h, bool metersCollapsed) noexcept
{
    Regions r;
    r.meterStripWidth = metersCollapsed ? 0 : L::meterStripW;
    const int panelW = w - r.meterStripWidth;

    r.header     = { 0, 0, w, L::headerH };
    r.plot       = { 0, L::headerH, panelW, h - L::headerH - L::controlBarH };
    r.controlBar = { 0, h - L::controlBarH, panelW, L::controlBarH };
    r.meterStrip = metersCollapsed ? juce::Rectangle<int>() : juce::Rectangle<int> (w - L::meterStripW, L::headerH, L::meterStripW, h - L::headerH);
    return r;
}

/** The plotted area inside the plot region (ui-spec 4.1), in the plot
    region's own coordinates. */
inline juce::Rectangle<int> computeDataRect (int plotWidth, int plotHeight) noexcept
{
    return { L::plotGutterLeft, L::plotGutterTop,
             plotWidth - L::plotGutterLeft - L::plotGutterRight,
             plotHeight - L::plotGutterTop - L::plotGutterBottom };
}

//==============================================================================
/** ui-spec 4.3. Positions are x offsets inside the control-bar component
    (whose x origin is the editor's x origin). */
struct ControlBarLayout
{
    bool showGroupB = false;
    bool showGroupC = false;
    std::array<int, 5> groupAX {};
    std::array<int, 2> groupBX {};
    std::array<int, 2> groupCX {};
    int dividerABX = 0;
    int dividerBCX = 0;

    bool anyHidden() const noexcept { return ! showGroupB || ! showGroupC; }
};

inline ControlBarLayout computeControlBarLayout (int panelWidth) noexcept
{
    constexpr std::array<int, 5> widthsA { L::chipWBlock, L::chipWOverlap, L::chipWWindow, L::chipWAvg, L::chipWSmooth };
    constexpr std::array<int, 2> widthsB { L::chipWSlope, L::chipWSecondary };

    ControlBarLayout out;
    const int avail = panelWidth - 2 * L::regionPadding;

    out.showGroupB = L::groupAW + L::groupGap + L::groupBW <= avail;
    out.showGroupC = out.showGroupB && L::groupAW + L::groupGap + L::groupBW + L::groupGap + L::groupCW <= avail;

    int x = L::regionPadding;

    for (size_t i = 0; i < widthsA.size(); ++i)
    {
        out.groupAX[i] = x;
        x += widthsA[i] + L::chipGap;
    }

    const int aEnd = L::regionPadding + L::groupAW;
    const int bStart = aEnd + L::groupGap;
    out.dividerABX = aEnd + L::groupGap / 2;

    x = bStart;

    for (size_t i = 0; i < widthsB.size(); ++i)
    {
        out.groupBX[i] = x;
        x += widthsB[i] + L::chipGap;
    }

    const int bEnd = bStart + L::groupBW;
    const int cStart = L::regionPadding + avail - L::groupCW;
    out.groupCX[0] = cStart;
    out.groupCX[1] = cStart + L::chipWFreqRange + L::chipGap;
    out.dividerBCX = (bEnd + cStart) / 2;
    return out;
}

//==============================================================================
/** ui-spec 4.4, strip-relative rectangles. `stripHeight` is SH = H - 44. */
struct MeterStripLayout
{
    juce::Rectangle<int> scaleChip, rmsChip, resetButton;
    juce::Rectangle<int> barsColumn, rightColumn;
    int rowH = L::rowHMax;
    int tableY = 0, tableH = 0;
    int loudnessY = 0;
    int correlationY = 0;
    int gap = L::stripGapMin;
};

inline MeterStripLayout computeMeterStripLayout (int stripHeight) noexcept
{
    MeterStripLayout m;
    const int body = stripHeight - L::stripBodyColumnPad;

    m.scaleChip   = { L::scaleChipX, L::stripChipY, L::stripChipW, L::stripChipH };
    m.rmsChip     = { L::rmsChipX, L::stripChipY, L::stripChipW, L::stripChipH };
    m.resetButton = { L::resetStatsX, L::stripChipY, L::resetStatsW, L::stripChipH };
    m.barsColumn  = { L::barsColX, L::stripBodyY, L::barsColW, body };
    m.rightColumn = { L::rightColX, L::stripBodyY, L::rightColW, body };

    m.rowH = std::clamp ((body - L::tableRowReserve) / L::tableRows, L::rowHMin, L::rowHMax);
    m.tableY = L::stripBodyY;
    m.tableH = L::tableHeaderH + L::tableRows * m.rowH;
    m.gap = std::max (L::stripGapMin, (body - m.tableH - L::loudnessBlockH - L::correlationBlockH) / 2);
    m.loudnessY = L::stripBodyY + m.tableH + m.gap;
    m.correlationY = stripHeight - L::stripBodyBottomPad - L::correlationBlockH;
    return m;
}

//==============================================================================
/** Popover anchor (ui-spec 6.1, 6.2): x from the right edge, fixed y. */
inline juce::Rectangle<int> computePopoverBounds (int editorWidth, bool aboutView) noexcept
{
    return { editorWidth - L::popoverFromRight, L::popoverY, L::popoverW,
             aboutView ? L::popoverAboutH : L::popoverSettingsH };
}

} // namespace investigate::LayoutMath
