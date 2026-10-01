// T-011 unit tests for source/gui/LayoutMath.h: the resolved positions of
// docs/design/ui-spec.md sections 2, 4.1, 4.3 and 4.4 at 1200x700 and 920x540,
// the meters-collapsed case, the size clamp and the popover anchor. Pure
// arithmetic, no display needed.

#include "gui/LayoutMath.h"

#include <catch2/catch_test_macros.hpp>

using namespace investigate::LayoutMath;
using Rect = juce::Rectangle<int>;

TEST_CASE ("clampEditorSize enforces ui-spec 2 limits", "[gui][layout]")
{
    CHECK (clampEditorSize (1200, 700) == EditorSize { 1200, 700 });
    CHECK (clampEditorSize (920, 540) == EditorSize { 920, 540 });
    CHECK (clampEditorSize (2560, 1600) == EditorSize { 2560, 1600 });
    CHECK (clampEditorSize (919, 539) == EditorSize { 920, 540 });
    CHECK (clampEditorSize (2561, 1601) == EditorSize { 2560, 1600 });
    CHECK (clampEditorSize (0, 0) == EditorSize { 920, 540 });
    CHECK (clampEditorSize (-50, -1) == EditorSize { 920, 540 });
    CHECK (clampEditorSize (1000000, 1000000) == EditorSize { 2560, 1600 });
    // Width and height are independent (free aspect ratio).
    CHECK (clampEditorSize (100, 1000) == EditorSize { 920, 1000 });
    CHECK (clampEditorSize (3000, 600) == EditorSize { 2560, 600 });
}

TEST_CASE ("EditorSize default is 1200x700", "[gui][layout]")
{
    const EditorSize s;
    CHECK (s.width == 1200);
    CHECK (s.height == 700);
}

TEST_CASE ("computeRegions at the default size (ui-spec 4.1)", "[gui][layout]")
{
    const auto r = computeRegions (1200, 700, false);
    CHECK (r.meterStripWidth == 296);
    CHECK (r.header == Rect (0, 0, 1200, 44));
    CHECK (r.plot == Rect (0, 44, 904, 600));
    CHECK (r.controlBar == Rect (0, 644, 904, 56));
    CHECK (r.meterStrip == Rect (904, 44, 296, 656));
}

TEST_CASE ("computeRegions at the minimum size", "[gui][layout]")
{
    const auto r = computeRegions (920, 540, false);
    CHECK (r.header == Rect (0, 0, 920, 44));
    CHECK (r.plot == Rect (0, 44, 624, 440));
    CHECK (r.controlBar == Rect (0, 484, 624, 56));
    CHECK (r.meterStrip == Rect (624, 44, 296, 496));
}

TEST_CASE ("computeRegions with meters collapsed gives the full width and no strip", "[gui][layout]")
{
    const auto r = computeRegions (1200, 700, true);
    CHECK (r.meterStripWidth == 0);
    CHECK (r.plot == Rect (0, 44, 1200, 600));
    CHECK (r.controlBar == Rect (0, 644, 1200, 56));
    CHECK (r.meterStrip.isEmpty());
}

TEST_CASE ("regions tile the editor without gaps at several sizes", "[gui][layout]")
{
    for (const auto& size : { EditorSize { 1200, 700 }, EditorSize { 920, 540 }, EditorSize { 2560, 1600 }, EditorSize { 1333, 777 } })
        for (const bool collapsed : { false, true })
        {
            const auto r = computeRegions (size.width, size.height, collapsed);
            CHECK (r.plot.getBottom() == r.controlBar.getY());
            CHECK (r.controlBar.getBottom() == size.height);
            CHECK (r.plot.getRight() + r.meterStripWidth == size.width);
            CHECK (r.header.getBottom() == r.plot.getY());

            if (! collapsed)
            {
                CHECK (r.meterStrip.getBottom() == size.height);
                CHECK (r.meterStrip.getRight() == size.width);
                CHECK (r.meterStrip.getX() == r.plot.getRight());
            }
        }
}

TEST_CASE ("computeDataRect matches ui-spec 4.1", "[gui][layout]")
{
    // Default: plot 904 x 600 -> data rect 840 x 564 (ui-spec 2 rationale).
    CHECK (computeDataRect (904, 600) == Rect (48, 12, 840, 564));
    CHECK (computeDataRect (624, 440) == Rect (48, 12, 560, 404));
    CHECK (computeDataRect (1200, 600) == Rect (48, 12, 1136, 564));
}

TEST_CASE ("control bar at 1200x700 (ui-spec 4.3 resolved positions)", "[gui][layout]")
{
    const auto l = computeControlBarLayout (904);
    CHECK (l.showGroupB);
    CHECK (l.showGroupC);
    CHECK_FALSE (l.anyHidden());
    CHECK (l.groupAX == std::array<int, 5> { 12, 80, 156, 240, 320 });
    CHECK (l.dividerABX == 398);
    CHECK (l.groupBX == std::array<int, 2> { 408, 508 });
    CHECK (l.dividerBCX == 614);
    CHECK (l.groupCX == std::array<int, 2> { 624, 772 });
    CHECK (l.groupCX[1] + 120 == 892);
}

TEST_CASE ("control bar at 920x540 hides group C", "[gui][layout]")
{
    const auto l = computeControlBarLayout (624);
    CHECK (l.showGroupB);
    CHECK_FALSE (l.showGroupC);
    CHECK (l.anyHidden());
    CHECK (l.groupAX == std::array<int, 5> { 12, 80, 156, 240, 320 });
    CHECK (l.groupBX == std::array<int, 2> { 408, 508 });
    CHECK (l.groupBX[1] + 96 == 604);
}

TEST_CASE ("control bar at 1200x700 with meters collapsed", "[gui][layout]")
{
    const auto l = computeControlBarLayout (1200);
    CHECK (l.showGroupB);
    CHECK (l.showGroupC);
    CHECK (l.groupAX == std::array<int, 5> { 12, 80, 156, 240, 320 });
    CHECK (l.groupBX == std::array<int, 2> { 408, 508 });
    CHECK (l.groupCX == std::array<int, 2> { 920, 1068 });
    CHECK (l.groupCX[1] + 120 == 1188);
}

TEST_CASE ("control bar group visibility thresholds are exact (ui-spec 4.3 rules 3 and 4)", "[gui][layout]")
{
    // avail = panelWidth - 24. B needs avail >= 592, C needs avail >= 880.
    CHECK_FALSE (computeControlBarLayout (615).showGroupB);
    CHECK (computeControlBarLayout (616).showGroupB);
    CHECK_FALSE (computeControlBarLayout (903).showGroupC);
    CHECK (computeControlBarLayout (904).showGroupC);
    // C is never shown without B.
    CHECK_FALSE (computeControlBarLayout (100).showGroupB);
    CHECK_FALSE (computeControlBarLayout (100).showGroupC);
    CHECK (computeControlBarLayout (100).anyHidden());
}

TEST_CASE ("group C is right-aligned and ends at x = 12 + avail", "[gui][layout]")
{
    for (const int panelW : { 904, 1000, 1500, 2560 })
    {
        const auto l = computeControlBarLayout (panelW);
        REQUIRE (l.showGroupC);
        CHECK (l.groupCX[1] + 120 == panelW - 12);
        CHECK (l.groupCX[0] + 140 + 8 == l.groupCX[1]);
    }
}

TEST_CASE ("meter strip layout at 1200x700 (ui-spec 4.4)", "[gui][layout]")
{
    constexpr int sh = 700 - 44;
    const auto m = computeMeterStripLayout (sh);
    CHECK (m.scaleChip == Rect (12, 12, 96, 28));
    CHECK (m.rmsChip == Rect (114, 12, 96, 28));
    CHECK (m.resetButton == Rect (216, 12, 68, 28));
    CHECK (m.barsColumn == Rect (12, 52, 80, sh - 64));
    CHECK (m.rightColumn == Rect (108, 52, 176, sh - 64));
    CHECK (m.rowH == 28);
    // Editor y = strip y + 44.
    CHECK (m.tableY + 44 == 96);
    CHECK (m.tableY + m.tableH + 44 == 364);
    CHECK (m.loudnessY + 44 == 443);
    CHECK (m.loudnessY + 44 + 98 == 541);
    CHECK (m.correlationY + 44 == 620);
    CHECK (m.correlationY + 44 + 68 == 688);
    // Bars track editor y 110 to 672 (562 px): column y 52+44, track 14 .. BH-16.
    CHECK (44 + m.barsColumn.getY() + 14 == 110);
    CHECK (44 + m.barsColumn.getY() + m.barsColumn.getHeight() - 16 == 672);
}

TEST_CASE ("meter strip layout at 920x540 (ui-spec 4.4)", "[gui][layout]")
{
    constexpr int sh = 540 - 44;
    const auto m = computeMeterStripLayout (sh);
    CHECK (m.rowH == 24);
    CHECK (m.tableY + 44 == 96);
    CHECK (m.tableY + m.tableH + 44 == 328);
    CHECK (m.loudnessY + 44 == 345);
    CHECK (m.loudnessY + 44 + 98 == 443);
    CHECK (m.correlationY + 44 == 460);
    CHECK (m.correlationY + 44 + 68 == 528);
    CHECK (44 + m.barsColumn.getY() + m.barsColumn.getHeight() - 16 == 512);
}

TEST_CASE ("meter strip rowH clamps to 22..28 and the gap never drops below 12", "[gui][layout]")
{
    for (int h = 540; h <= 1600; h += 7)
    {
        const auto m = computeMeterStripLayout (h - 44);
        CHECK (m.rowH >= 22);
        CHECK (m.rowH <= 28);
        CHECK (m.gap >= 12);
        CHECK (m.loudnessY == 52 + m.tableH + m.gap);
        // Blocks never overlap and stay inside the strip.
        CHECK (m.loudnessY + 98 <= m.correlationY);
        CHECK (m.correlationY + 68 == h - 44 - 12);
    }

    // Beyond rowH = 28 extra height goes into the gaps (ui-spec 4.5).
    const auto a = computeMeterStripLayout (800 - 44);
    const auto b = computeMeterStripLayout (900 - 44);
    CHECK (a.rowH == 28);
    CHECK (b.rowH == 28);
    CHECK (b.gap > a.gap);
}

TEST_CASE ("popover bounds anchor from the right edge (ui-spec 6.1, 6.2)", "[gui][layout]")
{
    CHECK (computePopoverBounds (1200, false) == Rect (628, 48, 560, 336));
    CHECK (computePopoverBounds (1200, true) == Rect (628, 48, 560, 440));
    CHECK (computePopoverBounds (920, false) == Rect (348, 48, 560, 336));
}
