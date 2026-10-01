// T-011 unit tests for source/gui/ControlMath.h: drag, wheel/keyboard step and
// snapping rules from docs/design/ui-spec.md section 6 (control inventory).
// Ranges are rebuilt exactly as source/params/ParameterLayout.cpp builds them.

#include "gui/ControlMath.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

using namespace investigate;
using namespace investigate::ControlMath;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
using Range = juce::NormalisableRange<float>;

Range skewed (float start, float end, float centre)
{
    Range r (start, end);
    r.setSkewForCentre (centre);
    return r;
}

Range avgRange()  { return skewed (0.0f, 10000.0f, 500.0f); }
Range rmsRange()  { return skewed (50.0f, 3000.0f, 300.0f); }
Range fLowRange() { return skewed (10.0f, 1000.0f, 100.0f); }
Range fHighRange() { return skewed (1000.0f, 40000.0f, 6325.0f); }
Range slopeRange() { return Range (-12.0f, 12.0f, 0.5f); }
Range dbLowRange() { return Range (-150.0f, -30.0f, 1.0f); }
Range dbHighRange() { return Range (-30.0f, 30.0f, 1.0f); }
Range choiceRange (int n) { return Range (0.0f, (float) (n - 1), 1.0f); }

bool onSnapGrid (float ms)
{
    const float step = ms < 1000.0f ? 10.0f : 100.0f;
    return std::fmod (ms, step) == 0.0f;
}
}

TEST_CASE ("behaviourFor maps parameters to control behaviours", "[gui][control]")
{
    CHECK (behaviourFor (ParamIDs::avgTime, false).kind == Kind::skewed);
    CHECK (behaviourFor (ParamIDs::rmsWindow, false).kind == Kind::skewed);
    CHECK (behaviourFor (ParamIDs::freqLow, false).kind == Kind::logFrequency);
    CHECK (behaviourFor (ParamIDs::freqHigh, false).kind == Kind::logFrequency);

    const auto slope = behaviourFor (ParamIDs::slope, false);
    CHECK (slope.kind == Kind::linearStep);
    CHECK_THAT (slope.dragUnitsPerPx, WithinAbs (0.1, 1e-6));
    CHECK_THAT (slope.dragUnitsPerPxFine, WithinAbs (0.01, 1e-6));
    CHECK_THAT (slope.wheelStep, WithinAbs (0.5, 1e-6));

    for (const auto* id : { ParamIDs::dbLow, ParamIDs::dbHigh })
    {
        const auto b = behaviourFor (id, false);
        CHECK (b.kind == Kind::linearStep);
        CHECK_THAT (b.dragUnitsPerPx, WithinAbs (0.5, 1e-6));      // 2 px per dB
        CHECK_THAT (b.dragUnitsPerPxFine, WithinAbs (0.1, 1e-6));  // 10 px per dB
        CHECK_THAT (b.wheelStep, WithinAbs (1.0, 1e-6));
    }

    // isChoice overrides everything; unknown IDs are choices.
    CHECK (behaviourFor (ParamIDs::slope, true).kind == Kind::choice);
    CHECK (behaviourFor (ParamIDs::fftSize, true).kind == Kind::choice);
    CHECK (behaviourFor ("nope", false).kind == Kind::choice);
}

TEST_CASE ("snapMilliseconds: 10 ms below 1 s, 100 ms at or above", "[gui][control]")
{
    CHECK (snapMilliseconds (0.0f) == 0.0f);
    CHECK (snapMilliseconds (-5.0f) == 0.0f);
    CHECK (snapMilliseconds (4.0f) == 0.0f);
    CHECK (snapMilliseconds (5.0f) == 10.0f);
    CHECK (snapMilliseconds (14.0f) == 10.0f);
    CHECK (snapMilliseconds (15.0f) == 20.0f);
    CHECK (snapMilliseconds (350.0f) == 350.0f);
    CHECK (snapMilliseconds (994.0f) == 990.0f);
    CHECK (snapMilliseconds (995.0f) == 1000.0f);
    CHECK (snapMilliseconds (999.0f) == 1000.0f);
    CHECK (snapMilliseconds (1000.0f) == 1000.0f);
    CHECK (snapMilliseconds (1049.0f) == 1000.0f);
    CHECK (snapMilliseconds (1051.0f) == 1100.0f);
    CHECK (snapMilliseconds (10000.0f) == 10000.0f);
}

TEST_CASE ("roundToThreeSignificantFigures", "[gui][control]")
{
    CHECK (roundToThreeSignificantFigures (20.04f) == 20.0f);
    CHECK_THAT (roundToThreeSignificantFigures (22.449f), WithinRel (22.4f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (1234.0f), WithinRel (1230.0f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (123456.0f), WithinRel (123000.0f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (0.5f), WithinRel (0.5f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (999.6f), WithinRel (1000.0f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (100.0f), WithinRel (100.0f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (1000.0f), WithinRel (1000.0f, 1e-5f));
    CHECK_THAT (roundToThreeSignificantFigures (-1234.0f), WithinRel (-1230.0f, 1e-5f));
    CHECK (roundToThreeSignificantFigures (0.0f) == 0.0f);
    CHECK (roundToThreeSignificantFigures (std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    CHECK (roundToThreeSignificantFigures (std::numeric_limits<float>::infinity()) == 0.0f);
}

//==============================================================================
TEST_CASE ("choice drag: one step per 20 px, 60 px with Shift", "[gui][control][drag]")
{
    const auto r = choiceRange (4);
    const auto b = behaviourFor (ParamIDs::window, true);

    CHECK (applyDrag (r, b, 0.0f, 0.0f, false) == 0.0f);
    CHECK (applyDrag (r, b, 0.0f, 9.0f, false) == 0.0f);
    CHECK (applyDrag (r, b, 0.0f, 19.0f, false) == 1.0f);
    CHECK (applyDrag (r, b, 0.0f, 20.0f, false) == 1.0f);
    CHECK (applyDrag (r, b, 0.0f, 40.0f, false) == 2.0f);
    CHECK (applyDrag (r, b, 2.0f, -20.0f, false) == 1.0f);
    CHECK (applyDrag (r, b, 0.0f, -100.0f, false) == 0.0f);  // clamped low
    CHECK (applyDrag (r, b, 0.0f, 1000.0f, false) == 3.0f);  // clamped high
    CHECK (applyDrag (r, b, 1.0f, 20.0f, true) == 1.0f);     // fine: 60 px per step
    CHECK (applyDrag (r, b, 1.0f, 60.0f, true) == 2.0f);
    CHECK (applyDrag (r, b, 1.0f, -60.0f, true) == 0.0f);
}

TEST_CASE ("slope drag: 0.1 units per px, snapped to 0.5, clamped", "[gui][control][drag]")
{
    const auto r = slopeRange();
    const auto b = behaviourFor (ParamIDs::slope, false);

    CHECK_THAT (applyDrag (r, b, 4.5f, 10.0f, false), WithinAbs (5.5, 1e-5));
    CHECK_THAT (applyDrag (r, b, 4.5f, -45.0f, false), WithinAbs (0.0, 1e-5));
    CHECK_THAT (applyDrag (r, b, 4.5f, 5.0f, false), WithinAbs (5.0, 1e-5));   // 5 px = one 0.5 step
    CHECK_THAT (applyDrag (r, b, 4.5f, 240.0f, false), WithinAbs (12.0, 1e-5)); // full range
    CHECK_THAT (applyDrag (r, b, 4.5f, 100000.0f, false), WithinAbs (12.0, 1e-5));
    CHECK_THAT (applyDrag (r, b, 4.5f, -100000.0f, false), WithinAbs (-12.0, 1e-5));
    // Fine: 0.01 per px, the step grid is still 0.5.
    CHECK_THAT (applyDrag (r, b, 4.5f, 10.0f, true), WithinAbs (4.5, 1e-5));
    CHECK_THAT (applyDrag (r, b, 4.5f, 50.0f, true), WithinAbs (5.0, 1e-5));
}

TEST_CASE ("dB range drag: 2 px per dB, 10 px per dB with Shift", "[gui][control][drag]")
{
    const auto b = behaviourFor (ParamIDs::dbLow, false);

    CHECK_THAT (applyDrag (dbLowRange(), b, -90.0f, 20.0f, false), WithinAbs (-80.0, 1e-5));
    CHECK_THAT (applyDrag (dbLowRange(), b, -90.0f, -20.0f, false), WithinAbs (-100.0, 1e-5));
    CHECK_THAT (applyDrag (dbLowRange(), b, -90.0f, 10.0f, true), WithinAbs (-89.0, 1e-5));
    CHECK_THAT (applyDrag (dbLowRange(), b, -90.0f, 1000.0f, false), WithinAbs (-30.0, 1e-5));
    CHECK_THAT (applyDrag (dbLowRange(), b, -90.0f, -1000.0f, false), WithinAbs (-150.0, 1e-5));

    const auto bh = behaviourFor (ParamIDs::dbHigh, false);
    CHECK_THAT (applyDrag (dbHighRange(), bh, 6.0f, 12.0f, false), WithinAbs (12.0, 1e-5));
    CHECK_THAT (applyDrag (dbHighRange(), bh, 6.0f, 1000.0f, false), WithinAbs (30.0, 1e-5));
    CHECK_THAT (applyDrag (dbHighRange(), bh, 6.0f, -1000.0f, false), WithinAbs (-30.0, 1e-5));
}

TEST_CASE ("avgTime and rmsWindow drag: normalised, snapped, clamped", "[gui][control][drag]")
{
    SECTION ("avgTime")
    {
        const auto r = avgRange();
        const auto b = behaviourFor (ParamIDs::avgTime, false);

        CHECK (applyDrag (r, b, 1000.0f, 0.0f, false) == 1000.0f);
        CHECK (applyDrag (r, b, 1000.0f, 240.0f, false) == 10000.0f);
        CHECK (applyDrag (r, b, 1000.0f, -240.0f, false) == 0.0f);
        CHECK (applyDrag (r, b, 1000.0f, 100000.0f, false) == 10000.0f);
        CHECK (applyDrag (r, b, 1000.0f, -100000.0f, false) == 0.0f);

        float last = -1.0f;

        for (int dy = -240; dy <= 240; dy += 3)
        {
            const auto v = applyDrag (r, b, 1000.0f, (float) dy, false);
            CHECK (v >= 0.0f);
            CHECK (v <= 10000.0f);
            CHECK (onSnapGrid (v));
            CHECK (v >= last); // monotonic in the drag distance
            last = v;
        }

        // Shift moves less than the coarse drag.
        const auto coarse = applyDrag (r, b, 1000.0f, 30.0f, false);
        const auto fine = applyDrag (r, b, 1000.0f, 30.0f, true);
        CHECK (fine < coarse);
        CHECK (fine >= 1000.0f);
    }

    SECTION ("rmsWindow")
    {
        const auto r = rmsRange();
        const auto b = behaviourFor (ParamIDs::rmsWindow, false);

        CHECK (applyDrag (r, b, 300.0f, 240.0f, false) == 3000.0f);
        CHECK (applyDrag (r, b, 300.0f, -240.0f, false) == 50.0f);

        for (int dy = -240; dy <= 240; dy += 7)
        {
            const auto v = applyDrag (r, b, 300.0f, (float) dy, false);
            CHECK (v >= 50.0f);
            CHECK (v <= 3000.0f);
            CHECK (onSnapGrid (v));
        }
    }
}

TEST_CASE ("frequency drag: 3 significant figures and clamped", "[gui][control][drag]")
{
    const auto r = fLowRange();
    const auto b = behaviourFor (ParamIDs::freqLow, false);

    CHECK (applyDrag (r, b, 20.0f, 0.0f, false) == 20.0f);
    CHECK (applyDrag (r, b, 20.0f, 240.0f, false) == 1000.0f);
    CHECK (applyDrag (r, b, 20.0f, -240.0f, false) == 10.0f);
    CHECK (applyDrag (r, b, 20.0f, 99999.0f, false) == 1000.0f);

    for (int dy = -240; dy <= 240; dy += 5)
    {
        const auto v = applyDrag (r, b, 20.0f, (float) dy, false);
        CHECK (v >= 10.0f);
        CHECK (v <= 1000.0f);
        CHECK (v == roundToThreeSignificantFigures (v));
    }

    const auto rh = fHighRange();
    const auto bh = behaviourFor (ParamIDs::freqHigh, false);
    CHECK (applyDrag (rh, bh, 20000.0f, 240.0f, false) == 40000.0f);
    CHECK (applyDrag (rh, bh, 20000.0f, -240.0f, false) == 1000.0f);
}

//==============================================================================
TEST_CASE ("choice step: wheel, arrows and page steps", "[gui][control][step]")
{
    const auto r = choiceRange (7);
    const auto b = behaviourFor (ParamIDs::channelMode, true);

    CHECK (applyStep (r, b, 3.0f, 1, false) == 4.0f);
    CHECK (applyStep (r, b, 3.0f, -1, false) == 2.0f);
    CHECK (applyStep (r, b, 3.0f, 1, true) == 4.0f); // Shift does not change a choice step
    CHECK (applyStep (r, b, 6.0f, 1, false) == 6.0f); // clamped at the end
    CHECK (applyStep (r, b, 0.0f, -1, false) == 0.0f);
    CHECK (applyStep (r, b, 1.0f, 1, false, 5) == 6.0f);
    CHECK (applyStep (r, b, 5.0f, -1, false, 5) == 0.0f);
    CHECK (applyStep (r, b, 3.0f, 0, false) == 4.0f); // direction >= 0 counts as up
}

TEST_CASE ("slope and dB steps", "[gui][control][step]")
{
    const auto bs = behaviourFor (ParamIDs::slope, false);
    CHECK_THAT (applyStep (slopeRange(), bs, 4.5f, 1, false), WithinAbs (5.0, 1e-5));
    CHECK_THAT (applyStep (slopeRange(), bs, 4.5f, -1, false), WithinAbs (4.0, 1e-5));
    CHECK_THAT (applyStep (slopeRange(), bs, 4.5f, 1, true), WithinAbs (5.0, 1e-5)); // Shift: same step
    CHECK_THAT (applyStep (slopeRange(), bs, 4.5f, 1, false, 5), WithinAbs (7.0, 1e-5));
    CHECK_THAT (applyStep (slopeRange(), bs, 12.0f, 1, false), WithinAbs (12.0, 1e-5));
    CHECK_THAT (applyStep (slopeRange(), bs, -12.0f, -1, false), WithinAbs (-12.0, 1e-5));

    const auto bd = behaviourFor (ParamIDs::dbLow, false);
    CHECK_THAT (applyStep (dbLowRange(), bd, -90.0f, 1, false), WithinAbs (-89.0, 1e-5));
    CHECK_THAT (applyStep (dbLowRange(), bd, -90.0f, -1, false), WithinAbs (-91.0, 1e-5));
    CHECK_THAT (applyStep (dbLowRange(), bd, -30.0f, 1, false), WithinAbs (-30.0, 1e-5));
    CHECK_THAT (applyStep (dbLowRange(), bd, -150.0f, -1, false), WithinAbs (-150.0, 1e-5));
    CHECK_THAT (applyStep (dbLowRange(), bd, -90.0f, 1, false, 5), WithinAbs (-85.0, 1e-5));

    const auto bh = behaviourFor (ParamIDs::dbHigh, false);
    CHECK_THAT (applyStep (dbHighRange(), bh, 0.0f, 1, false), WithinAbs (1.0, 1e-5));
    CHECK_THAT (applyStep (dbHighRange(), bh, 30.0f, 1, false), WithinAbs (30.0, 1e-5));
}

TEST_CASE ("avgTime and rmsWindow step always move at least one snap step", "[gui][control][step]")
{
    SECTION ("avgTime from many starting values")
    {
        const auto r = avgRange();
        const auto b = behaviourFor (ParamIDs::avgTime, false);

        for (const float start : { 10.0f, 50.0f, 250.0f, 500.0f, 990.0f, 1000.0f, 1100.0f, 5000.0f, 9900.0f })
            for (const bool fine : { false, true })
            {
                const auto up = applyStep (r, b, start, 1, fine);
                const auto down = applyStep (r, b, start, -1, fine);
                INFO ("start " << start << " fine " << fine);
                CHECK (up > start);
                CHECK (down < start);
                CHECK (up <= 10000.0f);
                CHECK (down >= 0.0f);
                CHECK (onSnapGrid (up));
                CHECK (onSnapGrid (down));
            }

        CHECK (applyStep (r, b, 0.0f, 1, false) == 10.0f);
        CHECK (applyStep (r, b, 0.0f, -1, false) == 0.0f);
        CHECK (applyStep (r, b, 10000.0f, 1, false) == 10000.0f);
    }

    SECTION ("rmsWindow")
    {
        const auto r = rmsRange();
        const auto b = behaviourFor (ParamIDs::rmsWindow, false);

        for (const float start : { 60.0f, 300.0f, 990.0f, 1000.0f, 2000.0f })
        {
            INFO ("start " << start);
            CHECK (applyStep (r, b, start, 1, false) > start);
            CHECK (applyStep (r, b, start, -1, false) < start);
        }

        CHECK (applyStep (r, b, 50.0f, -1, false) == 50.0f);
        CHECK (applyStep (r, b, 3000.0f, 1, false) == 3000.0f);
        CHECK (applyStep (r, b, 50.0f, 1, true) > 50.0f);
    }

    SECTION ("Shift steps are no larger than coarse steps")
    {
        const auto r = avgRange();
        const auto b = behaviourFor (ParamIDs::avgTime, false);
        CHECK (applyStep (r, b, 2000.0f, 1, true) <= applyStep (r, b, 2000.0f, 1, false));
    }
}

TEST_CASE ("frequency step: 2^(1/6) per notch, 2^(1/24) with Shift", "[gui][control][step]")
{
    const auto r = fLowRange();
    const auto b = behaviourFor (ParamIDs::freqLow, false);

    CHECK_THAT (applyStep (r, b, 100.0f, 1, false), WithinRel (112.0f, 1e-4f));
    CHECK_THAT (applyStep (r, b, 100.0f, -1, false), WithinRel (89.1f, 1e-4f));
    CHECK_THAT (applyStep (r, b, 100.0f, 1, true), WithinRel (103.0f, 1e-4f));
    CHECK_THAT (applyStep (r, b, 20.0f, 1, false), WithinRel (22.4f, 1e-4f));
    CHECK (applyStep (r, b, 1000.0f, 1, false) == 1000.0f);
    CHECK (applyStep (r, b, 10.0f, -1, false) == 10.0f);

    // Always changes, and stays on 3 significant figures, throughout the range.
    for (float f = 10.0f; f < 990.0f; f *= 1.07f)
    {
        const auto up = applyStep (r, b, f, 1, true);
        INFO ("f " << f);
        CHECK (up > f);
        CHECK (up == roundToThreeSignificantFigures (up));
    }

    // 5 notches (PageUp) is 5/6 octave: 20 * 2^(5/6) = 35.64 -> 35.6.
    CHECK_THAT (applyStep (r, b, 20.0f, 1, false, 5), WithinRel (35.6f, 2e-3f));
}
