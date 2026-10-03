// T-013 MeterMath: scale mapping, number formatting and the display ballistics
// (docs/design/ui-spec.md 8.2 to 8.7). Pure functions, no GUI.

#include "gui/MeterMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <string>
#include <vector>

using namespace investigate;
using namespace investigate::MeterMath;
using Catch::Matchers::WithinAbs;

namespace
{
constexpr const char* minus = "\xE2\x88\x92";
constexpr const char* dash = "\xE2\x80\x94";

std::string s (const FixedText& t) { return std::string (t.view()); }
std::string m (const char* rest) { return std::string (minus) + rest; }

constexpr float posInf = std::numeric_limits<float>::infinity();
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();

// A sine at -20 dBFS peak has an RMS of -23.0103 dBFS (reported unreferenced).
const float sineMinus20Rms = -20.0f - 3.0103f;
}

//==============================================================================
TEST_CASE ("MeterMath: -20 dBFS peak sine on the K scales and in dBFS", "[meter][metermath]")
{
    const float reported = -23.01f;

    SECTION ("display units")
    {
        CHECK_THAT (rmsDisplay (reported, MeterScale::dBFS), WithinAbs (-23.01, 0.01));
        CHECK_THAT (rmsDisplay (reported, MeterScale::k20), WithinAbs (0.0, 0.01));
        CHECK_THAT (rmsDisplay (reported, MeterScale::k14), WithinAbs (-6.0, 0.01));   // 0 K sits at -14 dBFS
        CHECK_THAT (rmsDisplay (reported, MeterScale::k12), WithinAbs (-8.0, 0.01));   // 0 K sits at -12 dBFS
        CHECK_THAT (rmsDisplay (sineMinus20Rms, MeterScale::k20), WithinAbs (0.0, 0.001));
    }

    SECTION ("bar position includes the AES17 offset only in the K modes")
    {
        CHECK_THAT (rmsBarDb (reported, MeterScale::dBFS), WithinAbs (-23.01, 1e-4));

        for (auto scale : { MeterScale::k20, MeterScale::k14, MeterScale::k12 })
            CHECK_THAT (rmsBarDb (reported, scale), WithinAbs (-20.0, 0.01));
    }

    SECTION ("silence stays silence")
    {
        for (auto scale : { MeterScale::dBFS, MeterScale::k20, MeterScale::k14, MeterScale::k12 })
        {
            CHECK (std::isinf (rmsBarDb (-posInf, scale)));
            CHECK (rmsBarDb (-posInf, scale) < 0.0f);
        }
    }

    SECTION ("scale index mapping is clamped")
    {
        CHECK (scaleFromIndex (-5) == MeterScale::dBFS);
        CHECK (scaleFromIndex (0) == MeterScale::dBFS);
        CHECK (scaleFromIndex (1) == MeterScale::k20);
        CHECK (scaleFromIndex (2) == MeterScale::k14);
        CHECK (scaleFromIndex (3) == MeterScale::k12);
        CHECK (scaleFromIndex (99) == MeterScale::k12);
        CHECK (kReference (MeterScale::dBFS) == 0);
        CHECK (kReference (MeterScale::k20) == 20);
        CHECK (kReference (MeterScale::k14) == 14);
        CHECK (kReference (MeterScale::k12) == 12);
    }
}

TEST_CASE ("MeterMath: zones follow ui-spec 8.2", "[meter][metermath]")
{
    CHECK (zonesFor (MeterScale::dBFS).warmFrom == -12.0f);
    CHECK (zonesFor (MeterScale::dBFS).hotFrom == -3.0f);
    CHECK (zonesFor (MeterScale::k20).warmFrom == -20.0f);
    CHECK (zonesFor (MeterScale::k20).hotFrom == -16.0f);
    CHECK (zonesFor (MeterScale::k14).warmFrom == -14.0f);
    CHECK (zonesFor (MeterScale::k14).hotFrom == -10.0f);
    CHECK (zonesFor (MeterScale::k12).warmFrom == -12.0f);
    CHECK (zonesFor (MeterScale::k12).hotFrom == -8.0f);
}

TEST_CASE ("MeterMath: tick sets", "[meter][metermath]")
{
    SECTION ("dBFS, tall track: every 6 dB from +6 to -60")
    {
        const auto t = computeTicks (MeterScale::dBFS, 600.0);
        REQUIRE (t.count == 12);
        CHECK (t.ticks[0].dbfs == 6.0f);
        CHECK (t.ticks[0].label == 6);
        CHECK (t.ticks[1].label == 0);
        CHECK (t.ticks[11].label == -60);
    }

    SECTION ("dBFS, short track: 6 dB under 20 px gives 12 dB steps")
    {
        const auto t = computeTicks (MeterScale::dBFS, 100.0);
        REQUIRE (t.count == 6);
        CHECK (t.ticks[0].label == 6);
        CHECK (t.ticks[1].label == -6);
        CHECK (t.ticks[5].label == -54);
    }

    SECTION ("the 20 px threshold is inclusive")
    {
        // 6 dB * px/dB = 20 px at 66 dB * 20/6 = 220 px track
        CHECK (computeTicks (MeterScale::dBFS, 220.0).count == 12);
        CHECK (computeTicks (MeterScale::dBFS, 219.0).count == 6);
    }

    SECTION ("K-20: labelled in K units, +24 down to -40, placed at k - 20 dBFS")
    {
        const auto t = computeTicks (MeterScale::k20, 600.0);
        REQUIRE (t.count == 17);
        CHECK (t.ticks[0].label == 24);
        CHECK (t.ticks[0].dbfs == 4.0f);
        CHECK (t.ticks[16].label == -40);
        CHECK (t.ticks[16].dbfs == -60.0f);

        bool hasZero = false;

        for (int i = 0; i < t.count; ++i)
            if (t.ticks[(size_t) i].label == 0)
            {
                hasZero = true;
                CHECK (t.ticks[(size_t) i].dbfs == -20.0f);
            }

        CHECK (hasZero);
    }

    SECTION ("K modes use 8 K steps when 4 K is under 20 px")
    {
        const auto t = computeTicks (MeterScale::k14, 100.0);
        REQUIRE (t.count >= 2);
        CHECK (t.ticks[0].label - t.ticks[1].label == 8);
    }

    SECTION ("no tick falls outside the bar axis")
    {
        for (auto scale : { MeterScale::dBFS, MeterScale::k20, MeterScale::k14, MeterScale::k12 })
            for (double h : { 60.0, 200.0, 600.0 })
            {
                const auto t = computeTicks (scale, h);

                for (int i = 0; i < t.count; ++i)
                {
                    CHECK (t.ticks[(size_t) i].dbfs <= barCeilDb);
                    CHECK (t.ticks[(size_t) i].dbfs >= barFloorDb);
                }
            }
    }
}

TEST_CASE ("MeterMath: BarGeometry::yFor", "[meter][metermath]")
{
    const BarGeometry g { 10, 76 };   // 66 px for 66 dB: 1 px per dB
    CHECK (g.heightPx() == 66);
    CHECK (g.yFor (6.0f) == 10);
    CHECK (g.yFor (0.0f) == 16);
    CHECK (g.yFor (-30.0f) == 46);
    CHECK (g.yFor (-59.0f) == 75);
    CHECK (g.yFor (-60.0f) == 76);
    CHECK (g.yFor (-61.0f) == 76);
    CHECK (g.yFor (50.0f) == 10);
    CHECK (g.yFor (posInf) == 10);
    CHECK (g.yFor (-posInf) == 76);
    CHECK (g.yFor (qNaN) == 76);

    // Monotonic: a louder level is never lower on screen.
    int previous = 100000;

    for (float db = -70.0f; db <= 10.0f; db += 0.25f)
    {
        const int y = g.yFor (db);
        CHECK (y <= previous);
        previous = y;
    }
}

//==============================================================================
TEST_CASE ("MeterMath: level formatting (ui-spec 8.3 rounding)", "[meter][metermath][format]")
{
    CHECK (s (formatLevel (-posInf)) == m ("inf"));
    CHECK (s (formatLevel (qNaN)) == m ("inf"));
    CHECK (s (formatLevel (posInf)) == m ("inf"));
    CHECK (s (formatLevel (-120.0f)) == m ("inf"));
    CHECK (s (formatLevel (-119.9f)) == m ("119.9"));
    CHECK (s (formatLevel (-0.04f)) == "0.0");
    CHECK (s (formatLevel (0.04f)) == "0.0");
    CHECK (s (formatLevel (0.0f)) == "0.0");
    CHECK (s (formatLevel (0.05f)) == "+0.1");
    CHECK (s (formatLevel (-0.05f)) == m ("0.1"));
    CHECK (s (formatLevel (-6.0f)) == m ("6.0"));
    CHECK (s (formatLevel (0.3f)) == "+0.3");
    CHECK (s (formatLevel (12.34f)) == "+12.3");
}

TEST_CASE ("MeterMath: RMS formatting per scale", "[meter][metermath][format]")
{
    CHECK (s (formatRms (-23.01f, MeterScale::dBFS)) == m ("23.0"));
    CHECK (s (formatRms (-23.0103f, MeterScale::k20)) == "0.0");
    CHECK (s (formatRms (-23.0103f, MeterScale::k14)) == m ("6.0"));
    CHECK (s (formatRms (-23.0103f, MeterScale::k12)) == m ("8.0"));
    CHECK (s (formatRms (-posInf, MeterScale::k20)) == m ("inf"));
    CHECK (s (formatRms (-130.0f, MeterScale::k12)) == m ("inf"));
}

TEST_CASE ("MeterMath: headroom", "[meter][metermath][format]")
{
    bool negative = true;
    CHECK (s (formatHeadroom (-1.2f, negative)) == "1.2");
    CHECK_FALSE (negative);

    CHECK (s (formatHeadroom (0.4f, negative)) == m ("0.4"));
    CHECK (negative);

    CHECK (s (formatHeadroom (0.0f, negative)) == "0.0");
    CHECK_FALSE (negative);

    // +0.04 dBTP rounds to 0.0: not shown as an error.
    CHECK (s (formatHeadroom (0.04f, negative)) == "0.0");
    CHECK_FALSE (negative);

    negative = true;
    CHECK (s (formatHeadroom (-posInf, negative)) == dash);
    CHECK_FALSE (negative);
    CHECK (s (formatHeadroom (qNaN, negative)) == dash);
    CHECK (s (formatHeadroom (-125.0f, negative)) == dash);
}

TEST_CASE ("MeterMath: crest", "[meter][metermath][format]")
{
    // A sine: peak -6 dBFS, RMS -9.0103 dBFS.
    CHECK (s (formatCrest (-6.0f, -6.0f - 3.0103f)) == "3.0");
    CHECK (s (formatCrest (-posInf, -10.0f)) == dash);
    CHECK (s (formatCrest (-10.0f, -posInf)) == dash);
    CHECK (s (formatCrest (qNaN, -10.0f)) == dash);
    CHECK (s (formatCrest (-6.0f, -6.0f)) == "0.0");
    CHECK (s (formatCrest (-12.4f, -24.8f)) == "12.4");
}

TEST_CASE ("MeterMath: counts", "[meter][metermath][format]")
{
    CHECK (s (formatCount (0)) == "0");
    CHECK (s (formatCount (1)) == "1");
    CHECK (s (formatCount (9999)) == "9999");
    CHECK (s (formatCount (10000)) == "9999+");
    CHECK (s (formatCount (UINT64_MAX)) == "9999+");
}

TEST_CASE ("MeterMath: loudness formatting", "[meter][metermath][format]")
{
    SECTION ("momentary and short-term")
    {
        CHECK (s (formatLoudnessLive (-14.2f)) == m ("14.2"));
        CHECK (s (formatLoudnessLive (-99.9f)) == m ("99.9"));
        CHECK (s (formatLoudnessLive (-99.95f)) == m ("inf"));
        CHECK (s (formatLoudnessLive (-100.0f)) == m ("inf"));
        CHECK (s (formatLoudnessLive (-posInf)) == m ("inf"));
        CHECK (s (formatLoudnessLive (qNaN)) == m ("inf"));
        CHECK (s (formatLoudnessLive (posInf)) == m ("inf"));
        CHECK (s (formatLoudnessLive (-23.0f)) == m ("23.0"));
    }

    SECTION ("integrated and maxima: dash when non-finite")
    {
        CHECK (s (formatLoudnessHeld (-23.0f)) == m ("23.0"));
        CHECK (s (formatLoudnessHeld (-posInf)) == dash);
        CHECK (s (formatLoudnessHeld (posInf)) == dash);
        CHECK (s (formatLoudnessHeld (qNaN)) == dash);
        CHECK (s (formatLoudnessHeld (-0.04f)) == "0.0");
    }

    SECTION ("LRA")
    {
        CHECK (s (formatLra (2.5f, -23.0f)) == "2.5");
        CHECK (s (formatLra (2.5f, -posInf)) == dash);
        CHECK (s (formatLra (2.5f, qNaN)) == dash);
        CHECK (s (formatLra (qNaN, -23.0f)) == dash);
        CHECK (s (formatLra (0.0f, -23.0f)) == "0.0");
        CHECK (s (formatLra (-1.0f, -23.0f)) == "0.0");
    }
}

TEST_CASE ("MeterMath: correlation and balance", "[meter][metermath][format]")
{
    SECTION ("correlation")
    {
        CHECK (s (formatCorrelation (0.82f)) == "+0.82");
        CHECK (s (formatCorrelation (-0.15f)) == m ("0.15"));
        CHECK (s (formatCorrelation (0.0f)) == "0.00");
        CHECK (s (formatCorrelation (0.004f)) == "0.00");
        CHECK (s (formatCorrelation (-0.004f)) == "0.00");
        CHECK (s (formatCorrelation (1.0f)) == "+1.00");
        CHECK (s (formatCorrelation (-1.0f)) == m ("1.00"));
        CHECK (s (formatCorrelation (3.0f)) == "+1.00");
        CHECK (s (formatCorrelation (qNaN)) == "0.00");
        CHECK (s (formatCorrelation (posInf)) == "0.00");
    }

    SECTION ("balance")
    {
        CHECK (s (formatBalance (-1.2f)) == "L 1.2 dB");
        CHECK (s (formatBalance (0.8f)) == "R 0.8 dB");
        CHECK (s (formatBalance (0.0f)) == "C 0.0 dB");
        CHECK (s (formatBalance (0.04f)) == "C 0.0 dB");
        CHECK (s (formatBalance (-0.04f)) == "C 0.0 dB");
        CHECK (s (formatBalance (-6.0f)) == "L 6.0 dB");
        CHECK (s (formatBalance (-61.0f)) == "L >60 dB");
        CHECK (s (formatBalance (60.5f)) == "R >60 dB");
        CHECK (s (formatBalance (60.0f)) == "R 60.0 dB");
        CHECK (s (formatBalance (qNaN)) == "C 0.0 dB");
        CHECK (s (formatBalance (-posInf)) == "C 0.0 dB");
    }
}

TEST_CASE ("MeterMath: no formatter ever prints NaN, posInf or an ASCII minus", "[meter][metermath][format]")
{
    const float value = GENERATE (qNaN, posInf, -posInf, 1.0e30f, -1.0e30f, std::numeric_limits<float>::max(),
                                  std::numeric_limits<float>::lowest(), std::numeric_limits<float>::denorm_min(),
                                  1.0e-40f, -1.0e-40f, 0.0f, -0.0f, 99999.9f, -99999.9f);
    INFO ("value " << value);

    bool negative = false;

    const FixedText texts[] {
        formatLevel (value),
        formatRms (value, MeterScale::dBFS),
        formatRms (value, MeterScale::k20),
        formatRms (value, MeterScale::k12),
        formatUnsigned1 ((double) value),
        formatHeadroom (value, negative),
        formatCrest (value, value),
        formatCrest (value, -20.0f),
        formatCrest (-3.0f, value),
        formatLoudnessLive (value),
        formatLoudnessHeld (value),
        formatLra (value, value),
        formatLra (value, -23.0f),
        formatCorrelation (value),
        formatBalance (value),
    };

    for (const auto& t : texts)
    {
        const std::string text = s (t);
        INFO ("text '" << text << "'");
        CHECK_FALSE (text.empty());
        CHECK (text.find ("nan") == std::string::npos);
        CHECK (text.find ("NaN") == std::string::npos);
        CHECK (text.find ('-') == std::string::npos);
        CHECK (text.find ("e+") == std::string::npos);

        // "inf" appears only as the dedicated "-inf" text.
        if (text.find ("inf") != std::string::npos)
            CHECK (text == std::string (negInfText));
    }
}

TEST_CASE ("MeterMath: FixedText never overflows", "[meter][metermath][format]")
{
    FixedText t;

    for (int i = 0; i < 50; ++i)
        t.append ("abcdef");

    CHECK ((int) t.view().size() == FixedText::capacity);
    CHECK (FixedText().empty());
    CHECK (FixedText() == FixedText());
    CHECK (FixedText().append ("a") != FixedText().append ("b"));
}

//==============================================================================
TEST_CASE ("MeterMath: sanitizeLevel", "[meter][metermath]")
{
    CHECK (sanitizeLevel (qNaN) == -posInf);
    CHECK (sanitizeLevel (-posInf) == -posInf);
    CHECK (sanitizeLevel (-200.0f) == -posInf);
    CHECK (sanitizeLevel (-1000.0f) == -posInf);
    CHECK (sanitizeLevel (-199.0f) == -199.0f);
    CHECK (sanitizeLevel (-6.0f) == -6.0f);
    CHECK (sanitizeLevel (3.0f) == 3.0f);
    CHECK (sanitizeLevel (posInf) == 200.0f);
    CHECK (sanitizeLevel (1.0e20f) == 200.0f);
}

TEST_CASE ("MeterMath: FallingLevel peak bar", "[meter][metermath][ballistics]")
{
    FallingLevel f;
    CHECK (f.value == -posInf);

    f.update (-6.0f, 0.0);
    CHECK (f.value == -6.0f);                       // instant rise from nothing

    f.update (-posInf, 0.5);
    CHECK_THAT (f.value, WithinAbs (-16.0, 1e-4));   // 20 dB/s

    f.update (-40.0f, 0.5);
    CHECK_THAT (f.value, WithinAbs (-26.0, 1e-4));   // input below the fall: keep falling

    f.update (-3.0f, 0.001);
    CHECK (f.value == -3.0f);                       // instant rise

    f.reset();
    CHECK (f.value == -posInf);

    // Many small steps equal one big step (linear fall).
    FallingLevel a, b;
    a.update (0.0f, 0.0);
    b.update (0.0f, 0.0);

    for (int i = 0; i < 60; ++i)
        a.update (-posInf, 1.0 / 60.0);

    b.update (-posInf, 1.0);
    CHECK_THAT (a.value, WithinAbs (b.value, 1e-3));
    CHECK_THAT (a.value, WithinAbs (-20.0, 1e-3));
}

TEST_CASE ("MeterMath: HeldLevel hold then 30 dB/s", "[meter][metermath][ballistics]")
{
    const float floorDb = -60.0f;

    SECTION ("holds for 1500 ms exactly, then falls at 30 dB/s")
    {
        HeldLevel h;
        h.update (-6.0f, 0.0, 0.0, floorDb);
        CHECK (h.value == -6.0f);

        h.update (-posInf, 1000.0, 1.0, floorDb);
        CHECK (h.value == -6.0f);

        h.update (-posInf, 1500.0, 0.5, floorDb);
        CHECK (h.value == -6.0f);                   // not yet: 1500 ms is not more than the hold

        h.update (-posInf, 1500.5, 0.1, floorDb);
        CHECK_THAT (h.value, WithinAbs (-9.0, 1e-4));

        h.update (-posInf, 1600.5, 1.0, floorDb);
        CHECK_THAT (h.value, WithinAbs (-39.0, 1e-4));
    }

    SECTION ("a higher input restarts the hold")
    {
        HeldLevel h;
        h.update (-20.0f, 0.0, 0.0, floorDb);
        h.update (-10.0f, 1000.0, 1.0, floorDb);
        CHECK (h.value == -10.0f);
        h.update (-posInf, 2400.0, 0.1, floorDb);     // 1400 ms after the new peak
        CHECK (h.value == -10.0f);
        h.update (-posInf, 2600.0, 0.1, floorDb);
        CHECK_THAT (h.value, WithinAbs (-13.0, 1e-4));
    }

    SECTION ("never falls below the floor argument")
    {
        HeldLevel h;
        h.update (-6.0f, 0.0, 0.0, -12.0f);
        h.update (-posInf, 2000.0, 100.0, -12.0f);
        CHECK (h.value == -12.0f);
    }

    SECTION ("a floor above the held value lifts it (hold stays on or above the bar)")
    {
        HeldLevel h;
        h.update (-30.0f, 0.0, 0.0, -60.0f);
        h.update (-30.0f, 10.0, 0.01, -20.0f);
        CHECK (h.value == -20.0f);
    }

    SECTION ("the TP hold uses the input as the floor")
    {
        HeldLevel h;
        h.update (-3.0f, 0.0, 0.0, -3.0f);
        h.update (-10.0f, 100.0, 0.1, -10.0f);
        CHECK (h.value == -3.0f);
        h.update (-10.0f, 1700.0, 0.1, -10.0f);
        CHECK_THAT (h.value, WithinAbs (-6.0, 1e-4));
        h.update (-10.0f, 3000.0, 1.0, -10.0f);
        CHECK (h.value == -10.0f);
    }

    SECTION ("reset")
    {
        HeldLevel h;
        h.update (-3.0f, 0.0, 0.0, -60.0f);
        h.reset();
        CHECK (h.value == -posInf);
    }
}

TEST_CASE ("MeterMath: CrestRing window", "[meter][metermath][ballistics]")
{
    CrestRing ring;
    CHECK (ring.maxWithin (0.0, 300.0) == -posInf);

    ring.push (0.0, -6.0f, 300.0);
    ring.push (100.0, -20.0f, 300.0);
    ring.push (200.0, -30.0f, 300.0);

    CHECK (ring.maxWithin (200.0, 300.0) == -6.0f);
    CHECK (ring.maxWithin (300.0, 300.0) == -6.0f);   // entry at 0 is exactly 300 ms old: inside
    CHECK (ring.maxWithin (301.0, 300.0) == -20.0f);
    CHECK (ring.maxWithin (350.0, 300.0) == -20.0f);
    CHECK (ring.maxWithin (401.0, 300.0) == -30.0f);      // the entry at 200 is 201 ms old
    CHECK (ring.maxWithin (501.0, 300.0) == -posInf);

    SECTION ("pushes closer than the spacing are dropped")
    {
        CrestRing r;
        r.push (0.0, -40.0f, 300.0);
        r.push (3.0, -1.0f, 300.0);       // under 8 ms apart
        CHECK (r.maxWithin (3.0, 300.0) == -40.0f);
    }

    SECTION ("reset empties the ring")
    {
        ring.reset();
        CHECK (ring.maxWithin (200.0, 300.0) == -posInf);
        ring.push (0.0, -9.0f, 300.0);
        CHECK (ring.maxWithin (0.0, 300.0) == -9.0f);
    }

    SECTION ("256 entries always cover the longest window (3 s)")
    {
        CrestRing r;
        constexpr double window = 3000.0;

        for (double t = 0.0; t <= 15000.0; t += 1.0)
        {
            // a 60 ms plateau at -3 around t = 12000 (a sample peak is itself a 50 ms maximum)
            const float v = (t >= 12000.0 && t < 12060.0) ? -3.0f : -50.0f;
            r.push (t, v, window);
        }

        CHECK (r.maxWithin (14000.0, window) == -3.0f);     // 2000 ms old: inside
        CHECK (r.maxWithin (15000.0, window) == -3.0f);     // 3000 ms old: inside
        CHECK (r.maxWithin (15100.0, window) == -50.0f);    // older than the window
        CHECK (r.maxWithin (15000.0 + 3100.0, window) == -posInf);   // everything is older than the window
    }

    SECTION ("a 60 Hz vblank feed over the 50 ms window")
    {
        CrestRing r;

        for (int i = 0; i <= 1010; ++i)
            r.push ((double) i * 16.6667, i == 1000 ? -2.0f : -40.0f, 50.0);

        CHECK (r.maxWithin (1000 * 16.6667 + 33.0, 50.0) == -2.0f);
        CHECK (r.maxWithin (1000 * 16.6667 + 60.0, 50.0) == -40.0f);
    }
}

TEST_CASE ("MeterMath: CorrelationFilter tau = 50 ms", "[meter][metermath][ballistics]")
{
    CorrelationFilter f;
    CHECK (f.value == 0.0f);

    SECTION ("step response reaches 1 - 1/e after one time constant")
    {
        for (int i = 0; i < 50; ++i)
            f.update (1.0f, 0.001);

        CHECK_THAT (f.value, WithinAbs (1.0 - std::exp (-1.0), 0.002));

        for (int i = 0; i < 150; ++i)
            f.update (1.0f, 0.001);

        CHECK_THAT (f.value, WithinAbs (1.0 - std::exp (-4.0), 0.002));
    }

    SECTION ("independent of the frame rate")
    {
        CorrelationFilter coarse;

        for (int i = 0; i < 3; ++i)
            coarse.update (1.0f, 1.0 / 60.0);       // 50 ms in three frames

        CHECK_THAT (coarse.value, WithinAbs (1.0 - std::exp (-1.0), 0.002));
    }

    SECTION ("negative step and clamping")
    {
        f.reset (1.0f);
        f.update (-1.0f, 10.0);
        CHECK_THAT (f.value, WithinAbs (-1.0, 1e-4));

        f.reset (0.0f);
        f.update (50.0f, 10.0);
        CHECK_THAT (f.value, WithinAbs (1.0, 1e-4));
    }

    SECTION ("non-finite targets read as 0 and the state stays finite")
    {
        f.reset (0.5f);
        f.update (qNaN, 10.0);
        CHECK_THAT (f.value, WithinAbs (0.0, 1e-4));
        f.update (posInf, 0.01);
        CHECK (std::isfinite (f.value));
    }

    SECTION ("zero or negative dt leaves the value alone")
    {
        f.reset (0.3f);
        f.update (1.0f, 0.0);
        CHECK (f.value == 0.3f);
        f.update (1.0f, -1.0);
        CHECK (f.value == 0.3f);
    }
}

TEST_CASE ("MeterMath: StaleDetector (250 ms, != compare)", "[meter][metermath][ballistics]")
{
    StaleDetector d;
    CHECK_FALSE (d.update (5, 1000.0));            // first sight is never stale
    CHECK_FALSE (d.update (5, 1100.0));
    CHECK_FALSE (d.update (5, 1249.0));
    CHECK (d.update (5, 1250.0));                  // 250 ms without a change
    CHECK (d.update (5, 5000.0));

    SECTION ("the first counter change ends it at once")
    {
        CHECK_FALSE (d.update (6, 5001.0));
        CHECK_FALSE (d.update (6, 5200.0));
        CHECK (d.update (6, 5251.0));
    }

    SECTION ("a counter that goes backwards also counts as a change (not <)")
    {
        CHECK_FALSE (d.update (4, 5001.0));
    }

    SECTION ("wrap-around is harmless")
    {
        StaleDetector w;
        CHECK_FALSE (w.update (UINT64_MAX, 0.0));
        CHECK_FALSE (w.update (0, 16.0));
        CHECK_FALSE (w.update (1, 33.0));
        CHECK (w.update (1, 300.0));
    }

    SECTION ("reset forgets the history")
    {
        d.reset();
        CHECK_FALSE (d.update (5, 9000.0));
        CHECK_FALSE (d.stale);
    }

    SECTION ("stale-free while the counter keeps changing")
    {
        StaleDetector live;

        for (uint64_t i = 0; i < 200; ++i)
            CHECK_FALSE (live.update (i, (double) i * 16.7));
    }
}

TEST_CASE ("MeterMath: thicknessPx", "[meter][metermath]")
{
    CHECK (thicknessPx (1.0f, 1.0f) == 1);
    CHECK (thicknessPx (1.0f, 0.5f) == 1);   // never under 1 px
    CHECK (thicknessPx (2.0f, 1.0f) == 2);
    CHECK (thicknessPx (2.0f, 1.5f) == 3);
    CHECK (thicknessPx (0.0f, 2.0f) == 1);
}

//==============================================================================
// Round 3: O-3 (loudness maxima) and O-2 (update rate limiter)

TEST_CASE ("MeterMath::formatLoudnessMax: -inf below -99.9, dash when non-finite", "[meter][metermath]")
{
    const std::string negInf = m ("inf");

    CHECK (s (formatLoudnessMax (-153.7f)) == negInf);
    CHECK (s (formatLoudnessMax (-100.0f)) == negInf);
    CHECK (s (formatLoudnessMax (-99.95f)) == negInf);
    CHECK (s (formatLoudnessMax (-99.9f)) == m ("99.9"));
    CHECK (s (formatLoudnessMax (-14.2f)) == m ("14.2"));
    CHECK (s (formatLoudnessMax (0.0f)) == "0.0");
    CHECK (s (formatLoudnessMax (-posInf)) == s (FixedText().append (dash)));
    CHECK (s (formatLoudnessMax (posInf)) == s (FixedText().append (dash)));
    CHECK (s (formatLoudnessMax (qNaN)) == s (FixedText().append (dash)));

    // Same -inf text as the live values.
    CHECK (s (formatLoudnessMax (-153.7f)) == s (formatLoudnessLive (-153.7f)));
}

namespace
{
/** Counts accepted updates in [from, from + 1000) for a vblank train. */
int countIn (const std::vector<double>& accepted, double from)
{
    int n = 0;
    for (double t : accepted)
        if (t >= from && t < from + 1000.0)
            ++n;
    return n;
}
}

TEST_CASE ("UpdateRateLimiter: 144 Hz vblank is capped at 60 per second", "[meter][metermath][limiter]")
{
    UpdateRateLimiter lim;
    std::vector<double> accepted;
    const double dt = 1000.0 / 144.0;

    for (int i = 0; i < 144 * 10; ++i)
    {
        const double t = 5000.0 + i * dt;
        if (lim.shouldUpdate (t))
            accepted.push_back (t);
    }

    int worst = 0;
    for (double from = 5000.0; from <= 5000.0 + 9000.0; from += 7.3)
        worst = std::max (worst, countIn (accepted, from));

    INFO ("worst 1 s window " << worst << ", total " << accepted.size());
    CHECK (worst <= 60);
    CHECK (accepted.size() >= 595);
    CHECK (accepted.size() <= 600);
}

TEST_CASE ("UpdateRateLimiter: 60 Hz with +-1 ms jitter is not thinned", "[meter][metermath][limiter]")
{
    UpdateRateLimiter lim;
    int accepted = 0;
    const int n = 600;
    uint32_t rng = 12345u;

    for (int i = 0; i < n; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const double jitter = ((double) (rng >> 8) / (double) (1u << 24)) * 2.0 - 1.0;
        if (lim.shouldUpdate (2000.0 + i * (1000.0 / 60.0) + jitter))
            ++accepted;
    }

    INFO ("accepted " << accepted << " of " << n);
    CHECK (accepted >= n - 2);
}

TEST_CASE ("UpdateRateLimiter: 30 Hz passes everything, a long gap resets", "[meter][metermath][limiter]")
{
    SECTION ("30 Hz")
    {
        UpdateRateLimiter lim;
        for (int i = 0; i < 300; ++i)
            CHECK (lim.shouldUpdate (100.0 + i * (1000.0 / 30.0)));
    }

    SECTION ("long gap")
    {
        UpdateRateLimiter lim;
        CHECK (lim.shouldUpdate (1000.0));
        CHECK_FALSE (lim.shouldUpdate (1005.0));
        CHECK (lim.shouldUpdate (1017.0));

        // After a 10 s stall the deadline is not left in the past: the first
        // call passes, the next one 5 ms later does not (no burst catch-up).
        CHECK (lim.shouldUpdate (11000.0));
        CHECK_FALSE (lim.shouldUpdate (11005.0));
        CHECK_FALSE (lim.shouldUpdate (11010.0));
        CHECK (lim.shouldUpdate (11017.0));
    }
}
