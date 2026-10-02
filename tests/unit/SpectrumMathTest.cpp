// T-012 unit tests for source/gui/SpectrumMath.h (ui-spec 5.1 to 5.6): note
// names, axis mapping round trips, frequency/level/clipboard text, hover-level
// interpolation, hover-card placement, grid label/step selection and curve
// column reduction. No component is constructed.

#include "gui/SpectrumMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

using namespace investigate::SpectrumMath;

namespace
{
juce::String u (const char* utf8) { return juce::String::fromUTF8 (utf8); }
const juce::String minus = u ("\xE2\x88\x92");

Mapping makeMapping (double fLo, double fHi, double dbLo, double dbHi)
{
    Mapping m;
    m.fLo = fLo; m.fHi = fHi; m.dbLo = dbLo; m.dbHi = dbHi;
    m.x0 = 37.0; m.y0 = 11.0; m.w = 840.0; m.h = 564.0;
    return m;
}
}

TEST_CASE ("noteName acceptance examples (T-012)", "[gui][spectrummath]")
{
    CHECK (noteName (440.0) == "A4 +0c");
    CHECK (noteName (261.626) == "C4 +0c");
    CHECK (noteName (27.5) == "A0 +0c");
    CHECK (noteName (452.6) == "A4 +49c");
    CHECK (noteName (452.9) == "A#4 " + minus + "50c");
}

TEST_CASE ("noteName edge cases", "[gui][spectrummath]")
{
    CHECK (noteName (0.0).isEmpty());
    CHECK (noteName (-5.0).isEmpty());
    CHECK (noteName (std::numeric_limits<double>::quiet_NaN()).isEmpty());
    CHECK (noteName (std::numeric_limits<double>::infinity()).isEmpty());
    CHECK (noteName (880.0) == "A5 +0c");
    CHECK (noteName (20.0).startsWith ("D#0"));
    CHECK (noteName (16.3516).startsWith ("C0"));
    CHECK_FALSE (noteName (440.0).containsChar ('-'));

    // A flat note shows the U+2212 sign, never an ASCII hyphen-minus.
    const auto flat = noteName (440.0 * std::pow (2.0, -20.0 / 1200.0));
    CHECK (flat == "A4 " + minus + "20c");
    CHECK_FALSE (flat.containsChar ('-'));
}

TEST_CASE ("Mapping x and y round trips are exact to 1e-6 relative", "[gui][spectrummath]")
{
    const auto [fLo, fHi] = GENERATE (std::pair<double, double> { 20.0, 20000.0 },
                                      std::pair<double, double> { 20.0, 40.0 },
                                      std::pair<double, double> { 100.0, 24000.0 },
                                      std::pair<double, double> { 5.0, 96000.0 });
    const auto [dbLo, dbHi] = GENERATE (std::pair<double, double> { -90.0, 6.0 },
                                        std::pair<double, double> { -150.0, 30.0 },
                                        std::pair<double, double> { -18.0, -6.0 });
    const auto m = makeMapping (fLo, fHi, dbLo, dbHi);

    CHECK (m.freqToX (fLo) == Catch::Approx (m.x0).margin (1e-9));
    CHECK (m.freqToX (fHi) == Catch::Approx (m.x0 + m.w).epsilon (1e-9));
    CHECK (m.dbToY (dbHi) == Catch::Approx (m.y0).margin (1e-9));
    CHECK (m.dbToY (dbLo) == Catch::Approx (m.y0 + m.h).epsilon (1e-9));

    for (int i = 0; i <= 200; ++i)
    {
        const double f = fLo * std::pow (fHi / fLo, i / 200.0);
        CHECK (m.xToFreq (m.freqToX (f)) == Catch::Approx (f).epsilon (1e-6));

        const double db = dbLo + (dbHi - dbLo) * i / 200.0;
        CHECK (m.yToDb (m.dbToY (db)) == Catch::Approx (db).margin (1e-6 * std::max (1.0, std::abs (db))));
    }

    for (int px = 0; px <= 840; px += 7)
    {
        const double x = m.x0 + px;
        CHECK (m.freqToX (m.xToFreq (x)) == Catch::Approx (x).epsilon (1e-6));
    }
}

TEST_CASE ("dbToYClamped pins out-of-range and non-finite levels", "[gui][spectrummath]")
{
    const auto m = makeMapping (20.0, 20000.0, -90.0, 6.0);
    CHECK (m.dbToYClamped (50.0) == Catch::Approx (m.y0));
    CHECK (m.dbToYClamped (-500.0) == Catch::Approx (m.y0 + m.h));
    CHECK (m.dbToYClamped (-std::numeric_limits<double>::infinity()) == Catch::Approx (m.y0 + m.h));
    CHECK (m.dbToYClamped (std::numeric_limits<double>::quiet_NaN()) == Catch::Approx (m.y0 + m.h));
    CHECK (m.dbToYClamped (-42.0) == Catch::Approx (m.dbToY (-42.0)));
}

TEST_CASE ("formatFrequency follows ui-spec 5.5", "[gui][spectrummath]")
{
    CHECK (formatFrequency (43.7) == "43.7 Hz");
    CHECK (formatFrequency (20.0) == "20.0 Hz");
    CHECK (formatFrequency (437.0) == "437 Hz");
    CHECK (formatFrequency (437.4) == "437 Hz");
    CHECK (formatFrequency (4370.0) == "4.37 kHz");
    CHECK (formatFrequency (14300.0) == "14.3 kHz");
    CHECK (formatFrequency (1250.0) == "1.25 kHz");
    CHECK (formatFrequency (20000.0) == "20.0 kHz");

    // Rounding to 3 significant figures happens first.
    CHECK (formatFrequency (999.6) == "1.00 kHz");
    CHECK (formatFrequency (99.96) == "100 Hz");
    CHECK (formatFrequency (9996.0) == "10.0 kHz");
    CHECK (formatFrequency (99.94) == "99.9 Hz");
}

TEST_CASE ("formatFrequencyForClipboard follows ui-spec 5.6", "[gui][spectrummath]")
{
    CHECK (formatFrequencyForClipboard (43.7) == "43.7");
    CHECK (formatFrequencyForClipboard (43.74) == "43.7");
    CHECK (formatFrequencyForClipboard (1250.3) == "1250");
    CHECK (formatFrequencyForClipboard (14312.0) == "14300");
    CHECK (formatFrequencyForClipboard (437.2) == "437");
    CHECK (formatFrequencyForClipboard (999.6) == "1000");
    CHECK (formatFrequencyForClipboard (99.96) == "100");
    CHECK_FALSE (formatFrequencyForClipboard (1250.3).containsAnyOf (" Hz"));
}

TEST_CASE ("formatLevel follows ui-spec 5.5", "[gui][spectrummath]")
{
    CHECK (formatLevel (-23.4) == minus + "23.4 dB");
    CHECK (formatLevel (-23.44) == minus + "23.4 dB");
    CHECK (formatLevel (0.0) == "0.0 dB");
    CHECK (formatLevel (0.04) == "0.0 dB");
    CHECK (formatLevel (-0.04) == "0.0 dB");
    CHECK (formatLevel (-0.25) == minus + "0.3 dB");
    CHECK (formatLevel (12.25) == "12.3 dB");
    CHECK (formatLevel (5.0) == "5.0 dB");
    CHECK (formatLevel (-199.9) == minus + "199.9 dB");
    CHECK (formatLevel (-200.0) == minus + "inf dB");
    CHECK (formatLevel (-1000.0) == minus + "inf dB");
    CHECK (formatLevel (-std::numeric_limits<double>::infinity()) == minus + "inf dB");
    CHECK (formatLevel (std::numeric_limits<double>::quiet_NaN()) == minus + "inf dB");
    CHECK (formatLevel (std::numeric_limits<double>::infinity()) == minus + "inf dB");
    CHECK_FALSE (formatLevel (-23.4).containsChar ('-'));
    CHECK_FALSE (formatLevel (6.0).containsChar ('+'));
}

TEST_CASE ("interpolateLevel is linear in dB over log frequency", "[gui][spectrummath]")
{
    const float freq[] { 100.0f, 1000.0f, 10000.0f };
    const float db[] { -10.0f, -30.0f, -20.0f };

    CHECK (interpolateLevel (freq, db, 3, std::sqrt (100.0 * 1000.0)) == Catch::Approx (-20.0).margin (1e-4));
    CHECK (interpolateLevel (freq, db, 3, 1000.0) == Catch::Approx (-30.0).margin (1e-4));
    CHECK (interpolateLevel (freq, db, 3, std::sqrt (1000.0 * 10000.0)) == Catch::Approx (-25.0).margin (1e-4));

    // Outside the frame: clamp to the end points.
    CHECK (interpolateLevel (freq, db, 3, 20.0) == Catch::Approx (-10.0));
    CHECK (interpolateLevel (freq, db, 3, 100.0) == Catch::Approx (-10.0));
    CHECK (interpolateLevel (freq, db, 3, 20000.0) == Catch::Approx (-20.0));

    // Empty and single-point frames.
    CHECK (interpolateLevel (freq, db, 0, 500.0) == -std::numeric_limits<double>::infinity());
    CHECK (interpolateLevel (freq, db, 1, 500.0) == Catch::Approx (-10.0));
}

TEST_CASE ("interpolateLevel returns -inf when a neighbour is -inf, NaN or at the floor", "[gui][spectrummath]")
{
    const double negInf = -std::numeric_limits<double>::infinity();
    const float freq[] { 100.0f, 1000.0f, 10000.0f };

    SECTION ("-inf neighbour")
    {
        const float db[] { -10.0f, -std::numeric_limits<float>::infinity(), -20.0f };
        CHECK (interpolateLevel (freq, db, 3, 300.0) == negInf);
        CHECK (interpolateLevel (freq, db, 3, 3000.0) == negInf);
        CHECK (interpolateLevel (freq, db, 3, 1000.0) == negInf);
        CHECK (interpolateLevel (freq, db, 3, 100.0) == Catch::Approx (-10.0));
    }
    SECTION ("NaN neighbour")
    {
        const float db[] { -10.0f, std::numeric_limits<float>::quiet_NaN(), -20.0f };
        CHECK (interpolateLevel (freq, db, 3, 300.0) == negInf);
    }
    SECTION ("floor neighbour")
    {
        const float db[] { -10.0f, -200.0f, -20.0f };
        CHECK (interpolateLevel (freq, db, 3, 300.0) == negInf);
    }
    SECTION ("end clamp on a -inf end point")
    {
        const float db[] { -std::numeric_limits<float>::infinity(), -5.0f, -20.0f };
        CHECK (interpolateLevel (freq, db, 3, 50.0) == negInf);
    }
}

TEST_CASE ("placeHoverCard follows the four ui-spec 5.5 steps", "[gui][spectrummath]")
{
    const juce::Rectangle<int> data (50, 20, 800, 400);
    constexpr int w = 140, h = 80;

    SECTION ("default: above and right of the pointer")
    {
        const auto p = placeHoverCard ({ 300.0f, 250.0f }, w, h, data);
        CHECK (p.x == 318);
        CHECK (p.y == 250 - 18 - h);
    }
    SECTION ("flips left at the right edge")
    {
        const auto p = placeHoverCard ({ 800.0f, 250.0f }, w, h, data);
        CHECK (p.x == 800 - 18 - w);
        CHECK (p.y == 250 - 18 - h);
    }
    SECTION ("flips below at the top")
    {
        const auto p = placeHoverCard ({ 300.0f, 60.0f }, w, h, data);
        CHECK (p.x == 318);
        CHECK (p.y == 60 + 18);
    }
    SECTION ("flips both ways in the top-right corner")
    {
        const auto p = placeHoverCard ({ 840.0f, 30.0f }, w, h, data);
        CHECK (p.x == 840 - 18 - w);
        CHECK (p.y == 30 + 18);
    }
    SECTION ("clamps inside the data rect with a 4 px inset")
    {
        const auto p = placeHoverCard ({ 52.0f, 420.0f }, w, h, data);
        CHECK (p.x >= data.getX() + 4);
        CHECK (p.y >= data.getY() + 4);
        CHECK (p.x + w <= data.getRight() - 4);
        CHECK (p.y + h <= data.getBottom() - 4);
    }
    SECTION ("a card larger than the data rect pins to the top-left inset")
    {
        const auto p = placeHoverCard ({ 100.0f, 100.0f }, 2000, 2000, data);
        CHECK (p.x == data.getX() + 4);
        CHECK (p.y == data.getY() + 4);
    }
    SECTION ("never under the pointer and always inside, over a sweep")
    {
        for (int px = data.getX(); px < data.getRight(); px += 13)
            for (int py = data.getY(); py < data.getBottom(); py += 11)
            {
                const auto p = placeHoverCard ({ (float) px, (float) py }, w, h, data);
                const juce::Rectangle<int> card (p.x, p.y, w, h);
                INFO ("pointer " << px << "," << py);
                CHECK_FALSE (card.toFloat().contains ((float) px, (float) py));
                CHECK (data.reduced (4).contains (card));
            }
    }
}

TEST_CASE ("formatFrequencyLabel", "[gui][spectrummath]")
{
    CHECK (formatFrequencyLabel (20.0) == "20");
    CHECK (formatFrequencyLabel (300.0) == "300");
    CHECK (formatFrequencyLabel (999.0) == "999");
    CHECK (formatFrequencyLabel (1000.0) == "1k");
    CHECK (formatFrequencyLabel (2000.0) == "2k");
    CHECK (formatFrequencyLabel (20000.0) == "20k");
    CHECK (formatFrequencyLabel (1500.0) == "1.5k");
}

TEST_CASE ("chooseFrequencyLabelSet and chooseDbStep match the spec's resolved examples", "[gui][spectrummath]")
{
    // ui-spec 5.3 numbers the sets 1 to 3; the function returns 0 to 2.
    // Default size: 840 px, 20 Hz to 20 kHz (280 px per decade) = spec set 1.
    CHECK (chooseFrequencyLabelSet (20.0, 20000.0, 840.0) == 0);
    // Minimum size: 560 px (187 px per decade) = spec set 2.
    CHECK (chooseFrequencyLabelSet (20.0, 20000.0, 560.0) == 1);
    // Very narrow: decades only.
    CHECK (chooseFrequencyLabelSet (20.0, 20000.0, 150.0) == 2);

    CHECK (isLabelledMultiple (0, 7));
    CHECK_FALSE (isLabelledMultiple (1, 3));
    CHECK (isLabelledMultiple (1, 5));
    CHECK_FALSE (isLabelledMultiple (2, 2));
    CHECK (isLabelledMultiple (2, 1));

    // Default range: 6 dB at the default (564 px) and minimum (404 px) heights.
    CHECK (chooseDbStep (-90.0, 6.0, 564.0) == 6.0);
    CHECK (chooseDbStep (-90.0, 6.0, 404.0) == 6.0);
    // Widest range at the minimum height uses 12 dB (27 px per step).
    CHECK (chooseDbStep (-150.0, 30.0, 405.0) == 12.0);
    // Extremes.
    CHECK (chooseDbStep (-90.0, 6.0, 5000.0) == 1.0);
    CHECK (chooseDbStep (-150.0, 30.0, 20.0) == 24.0);
}

TEST_CASE ("minorFrequencyLinesVisible switches at 4 px", "[gui][spectrummath]")
{
    CHECK (minorFrequencyLinesVisible (20.0, 20000.0, 840.0));
    CHECK (minorFrequencyLinesVisible (20.0, 20000.0, 560.0));
    CHECK_FALSE (minorFrequencyLinesVisible (20.0, 20000.0, 100.0));
}

TEST_CASE ("reduceToColumns keeps the extremes of each pixel column", "[gui][spectrummath]")
{
    Mapping m;
    m.fLo = 20.0; m.fHi = 20000.0; m.dbLo = -100.0; m.dbHi = 0.0;
    m.x0 = 10.0; m.y0 = 0.0; m.w = 100.0; m.h = 100.0;

    constexpr int numCols = 100;
    std::vector<Column> cols (numCols);

    SECTION ("several points in one column: highest and lowest survive")
    {
        const float xs[] { 10.2f, 10.5f, 10.8f };
        const float db[] { -50.0f, -10.0f, -90.0f };
        reduceToColumns (xs, db, 3, m, 1.0f, cols.data(), numCols);

        CHECK (cols[0].used);
        CHECK (cols[0].yTop == Catch::Approx (10.0f));
        CHECK (cols[0].xTop == Catch::Approx (10.5f));
        CHECK (cols[0].yBottom == Catch::Approx (90.0f));
        CHECK (cols[0].xBottom == Catch::Approx (10.8f));
        CHECK_FALSE (cols[1].used);
    }
    SECTION ("a single point sets top and bottom equal")
    {
        const float xs[] { 50.0f };
        const float db[] { -25.0f };
        reduceToColumns (xs, db, 1, m, 1.0f, cols.data(), numCols);
        CHECK (cols[40].used);
        CHECK (cols[40].yTop == cols[40].yBottom);
        CHECK (cols[40].yTop == Catch::Approx (25.0f));
    }
    SECTION ("points outside the data rect are dropped; edges are kept")
    {
        const float xs[] { 9.9f, 10.0f, 110.0f, 110.1f };
        const float db[] { -10.0f, -20.0f, -30.0f, -40.0f };
        reduceToColumns (xs, db, 4, m, 1.0f, cols.data(), numCols);
        CHECK (cols[0].used);
        CHECK (cols[0].yTop == Catch::Approx (20.0f));
        CHECK (cols[numCols - 1].used);
        CHECK (cols[numCols - 1].yTop == Catch::Approx (30.0f));

        int used = 0;
        for (auto& c : cols)
            used += c.used ? 1 : 0;
        CHECK (used == 2);
    }
    SECTION ("levels clamp to the plot edges and non-finite goes to the bottom")
    {
        const float xs[] { 20.0f, 30.0f, 40.0f };
        const float db[] { 40.0f, -500.0f, -std::numeric_limits<float>::infinity() };
        reduceToColumns (xs, db, 3, m, 1.0f, cols.data(), numCols);
        CHECK (cols[10].yTop == Catch::Approx (0.0f));
        CHECK (cols[20].yTop == Catch::Approx (100.0f));
        CHECK (cols[30].yTop == Catch::Approx (100.0f));
    }
    SECTION ("every column is reset first")
    {
        const float xs[] { 50.0f };
        const float db[] { -25.0f };
        reduceToColumns (xs, db, 1, m, 1.0f, cols.data(), numCols);
        reduceToColumns (xs, db, 0, m, 1.0f, cols.data(), numCols);
        for (auto& c : cols)
            CHECK_FALSE (c.used);
    }
    SECTION ("high pixel density doubles the column count")
    {
        std::vector<Column> fine (200);
        const float xs[] { 10.2f, 10.7f };
        const float db[] { -10.0f, -20.0f };
        reduceToColumns (xs, db, 2, m, 2.0f, fine.data(), 200);
        CHECK (fine[0].used);
        CHECK (fine[1].used);
    }
    SECTION ("zero columns and the right edge do not overrun")
    {
        const float xs[] { 110.0f };
        const float db[] { -10.0f };
        reduceToColumns (xs, db, 1, m, 1.0f, cols.data(), 0);
        reduceToColumns (xs, db, 1, m, 1.0f, cols.data(), numCols);
        CHECK (cols[numCols - 1].used);
    }
}
