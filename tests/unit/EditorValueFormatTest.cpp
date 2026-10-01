// T-011 unit tests for source/gui/ValueFormat.h and NoticesText.h: Level chip
// text (built from numbers, U+2212 minus), chip splitting, sample-rate text,
// effective-range clamp tooltips (ui-spec 5.4, 6, 8.8), and the About notices.

#include "gui/NoticesText.h"
#include "gui/ValueFormat.h"
#include "params/ParameterFormatting.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using namespace investigate;
using namespace investigate::ValueFormat;

namespace
{
juce::String u (const char* utf8) { return juce::String::fromUTF8 (utf8); }
const juce::String minus = u ("\xE2\x88\x92");
const juce::String ellipsis = u ("\xE2\x80\xA6");
}

TEST_CASE ("formatSignedDb uses U+2212, plus and a bare zero", "[gui][format]")
{
    CHECK (formatSignedDb (-90.0f) == minus + "90");
    CHECK (formatSignedDb (6.0f) == "+6");
    CHECK (formatSignedDb (0.0f) == "0");
    CHECK (formatSignedDb (-150.0f) == minus + "150");
    CHECK (formatSignedDb (30.0f) == "+30");
    CHECK (formatSignedDb (-30.0f) == minus + "30");
    CHECK (formatSignedDb (-1.0f) == minus + "1");
    CHECK_FALSE (formatSignedDb (-90.0f).containsChar ('-'));
}

TEST_CASE ("formatSignedDb rounds half away from zero", "[gui][format]")
{
    CHECK (formatSignedDb (0.5f) == "+1");
    CHECK (formatSignedDb (-0.5f) == minus + "1");
    CHECK (formatSignedDb (0.4f) == "0");
    CHECK (formatSignedDb (-0.4f) == "0");
    CHECK (formatSignedDb (5.5f) == "+6");
    CHECK (formatSignedDb (-5.5f) == minus + "6");
    CHECK (formatSignedDb (-29.5f) == minus + "30");
    CHECK (formatSignedDb (-89.6f) == minus + "90");
}

TEST_CASE ("formatLevelRange shares one dB unit between both halves", "[gui][format]")
{
    CHECK (formatLevelRange (-90.0f, 6.0f) == minus + "90 " + ellipsis + " +6 dB");
    CHECK (formatLevelRange (-60.0f, 0.0f) == minus + "60 " + ellipsis + " 0 dB");
    CHECK (formatLevelRange (-150.0f, 30.0f) == minus + "150 " + ellipsis + " +30 dB");
    CHECK (formatLevelRange (-30.0f, -30.0f) == minus + "30 " + ellipsis + " " + minus + "30 dB");

    // Exactly one "dB", at the end (getText() would have added one per half).
    for (const auto& text : { formatLevelRange (-90.0f, 6.0f), formatLevelRange (-100.0f, -20.0f) })
    {
        CHECK (text.endsWith (" dB"));
        CHECK (text.indexOf ("dB") == text.lastIndexOf ("dB"));
        CHECK (text.contains (ellipsis));
    }
}

TEST_CASE ("splitChipText splits unit-carrying parameters at the first space", "[gui][format]")
{
    using namespace ParamIDs;

    SECTION ("with units")
    {
        const auto avg = splitChipText (avgTime, "350 ms");
        CHECK (avg.value == "350");
        CHECK (avg.unit == "ms");

        const auto avgS = splitChipText (avgTime, "1.00 s");
        CHECK (avgS.value == "1.00");
        CHECK (avgS.unit == "s");

        const auto sl = splitChipText (slope, "+4.5 dB/oct");
        CHECK (sl.value == "+4.5");
        CHECK (sl.unit == "dB/oct");

        const auto sm = splitChipText (smoothing, "1/12 oct");
        CHECK (sm.value == "1/12");
        CHECK (sm.unit == "oct");

        const auto fl = splitChipText (freqLow, "20.0 Hz");
        CHECK (fl.value == "20.0");
        CHECK (fl.unit == "Hz");

        const auto fh = splitChipText (freqHigh, "20.0 kHz");
        CHECK (fh.value == "20.0");
        CHECK (fh.unit == "kHz");

        const auto dl = splitChipText (dbLow, minus + "90 dB");
        CHECK (dl.value == minus + "90");
        CHECK (dl.unit == "dB");

        const auto dh = splitChipText (dbHigh, "+6 dB");
        CHECK (dh.value == "+6");
        CHECK (dh.unit == "dB");

        const auto rms = splitChipText (rmsWindow, "1.50 s");
        CHECK (rms.value == "1.50");
        CHECK (rms.unit == "s");
    }

    SECTION ("values without a unit are returned whole")
    {
        CHECK (splitChipText (avgTime, "Off").value == "Off");
        CHECK (splitChipText (avgTime, "Off").unit.isEmpty());
        CHECK (splitChipText (smoothing, "Off").unit.isEmpty());
        CHECK (splitChipText (secondary, "All-time max").value == "All-time max");
        CHECK (splitChipText (secondary, "All-time max").unit.isEmpty());
        CHECK (splitChipText (fftSize, "4096").value == "4096");
        CHECK (splitChipText (window, "Flat-top").value == "Flat-top");
        CHECK (splitChipText (meterScale, "K-20").unit.isEmpty());
    }

    SECTION ("degenerate text does not crash")
    {
        CHECK (splitChipText (avgTime, "").value.isEmpty());
        CHECK (splitChipText (avgTime, "").unit.isEmpty());
        CHECK (splitChipText (avgTime, " ms").value == " ms"); // leading space: returned whole
        CHECK (splitChipText ("unknownParam", "1 ms").value == "1 ms");
        CHECK (splitChipText (dbHigh, "+6 dB extra words").unit == "dB extra words");
    }
}

TEST_CASE ("formatSampleRate follows ui-spec 8.8", "[gui][format]")
{
    CHECK (formatSampleRate (44100.0) == "44.1 kHz");
    CHECK (formatSampleRate (48000.0) == "48 kHz");
    CHECK (formatSampleRate (88200.0) == "88.2 kHz");
    CHECK (formatSampleRate (96000.0) == "96 kHz");
    CHECK (formatSampleRate (176400.0) == "176.4 kHz");
    CHECK (formatSampleRate (192000.0) == "192 kHz");
    CHECK (formatSampleRate (8000.0) == "8 kHz");
}

TEST_CASE ("formatSampleRate is empty for an unusable rate", "[gui][format]")
{
    CHECK (formatSampleRate (0.0).isEmpty());
    CHECK (formatSampleRate (-48000.0).isEmpty());
    CHECK (formatSampleRate (std::numeric_limits<double>::quiet_NaN()).isEmpty());
}

TEST_CASE ("isParsableNumericText", "[gui][format]")
{
    CHECK (isParsableNumericText ("250", false));
    CHECK (isParsableNumericText ("1.5 s", false));
    CHECK (isParsableNumericText ("0.5k", false));
    CHECK (isParsableNumericText (minus + "96 dB", false));
    CHECK_FALSE (isParsableNumericText ("", false));
    CHECK_FALSE (isParsableNumericText ("abc", false));
    CHECK_FALSE (isParsableNumericText ("off", false));
    CHECK (isParsableNumericText ("off", true));
    CHECK (isParsableNumericText ("  OFF ", true));
    CHECK_FALSE (isParsableNumericText ("offf", true));
}

//==============================================================================
TEST_CASE ("computeEffectiveRange leaves legal defaults untouched", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (20.0f, 20000.0f, -90.0f, 6.0f, 48000.0);
    CHECK (r.freqLow == 20.0);
    CHECK (r.freqHigh == 20000.0);
    CHECK (r.dbLow == -90.0);
    CHECK (r.dbHigh == 6.0);
    CHECK (r.nyquist == 24000.0);
    CHECK_FALSE (r.freqLowClamped);
    CHECK_FALSE (r.freqHighClamped);
    CHECK_FALSE (r.dbLowClamped);
}

TEST_CASE ("freqHigh above Nyquist is clamped with the spec's tooltip", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (20.0f, 40000.0f, -90.0f, 6.0f, 48000.0);
    CHECK (r.freqHigh == 24000.0);
    CHECK (r.freqHighClamped);
    CHECK_FALSE (r.freqLowClamped);
    CHECK (isClamped (r, ParamIDs::freqHigh));
    CHECK_FALSE (isClamped (r, ParamIDs::freqLow));
    CHECK (clampTooltip (r, ParamIDs::freqHigh) == "Effective 24.0 kHz: limited by Nyquist (24.0 kHz).");
    CHECK (clampTooltip (r, ParamIDs::freqLow).isEmpty());
    CHECK (clampTooltip (r, ParamIDs::dbLow).isEmpty());
}

TEST_CASE ("Nyquist tooltip scales with the sample rate", "[gui][format][range]")
{
    const auto r44 = computeEffectiveRange (20.0f, 40000.0f, -90.0f, 6.0f, 44100.0);
    CHECK (r44.freqHighClamped);
    CHECK (clampTooltip (r44, ParamIDs::freqHigh) == "Effective 22.1 kHz: limited by Nyquist (22.1 kHz).");

    const auto r96 = computeEffectiveRange (20.0f, 40000.0f, -90.0f, 6.0f, 96000.0);
    CHECK_FALSE (r96.freqHighClamped); // 40 kHz is below the 48 kHz Nyquist
    CHECK (r96.freqHigh == 40000.0);
    CHECK (clampTooltip (r96, ParamIDs::freqHigh).isEmpty());
}

TEST_CASE ("freqLow too close to freqHigh is clamped to one octave", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (1000.0f, 1000.0f, -90.0f, 6.0f, 48000.0);
    CHECK (r.freqLow == 500.0);
    CHECK (r.freqHigh == 1000.0);
    CHECK (r.freqLowClamped);
    CHECK_FALSE (r.freqHighClamped);
    CHECK (clampTooltip (r, ParamIDs::freqLow) == "Effective 500 Hz: the range must span at least one octave.");
}

TEST_CASE ("freqLow clamp uses the Nyquist-limited freqHigh", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (1000.0f, 40000.0f, -90.0f, 6.0f, 48000.0);
    CHECK (r.freqHigh == 24000.0);
    CHECK (r.freqLow == 1000.0); // 1000 <= 12000, fine
    CHECK_FALSE (r.freqLowClamped);

    const auto c = computeEffectiveRange (1000.0f, 1000.0f, -90.0f, 6.0f, 48000.0);
    CHECK (c.freqLowClamped);
    // The octave constraint is freqHigh / freqLow >= 2 on the effective values.
    CHECK (c.freqHigh / c.freqLow >= 2.0);
}

TEST_CASE ("dbLow too close to dbHigh is clamped to 12 dB", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (20.0f, 20000.0f, -30.0f, -30.0f, 48000.0);
    CHECK (r.dbLow == -42.0);
    CHECK (r.dbHigh == -30.0);
    CHECK (r.dbLowClamped);
    CHECK (isClamped (r, ParamIDs::dbLow));
    CHECK_FALSE (isClamped (r, ParamIDs::dbHigh));
    CHECK (clampTooltip (r, ParamIDs::dbLow) == "Effective " + minus + "42 dB: the range must span at least 12 dB.");
    CHECK (clampTooltip (r, ParamIDs::dbHigh).isEmpty());

    const auto edge = computeEffectiveRange (20.0f, 20000.0f, -42.0f, -30.0f, 48000.0);
    CHECK_FALSE (edge.dbLowClamped); // exactly 12 dB apart is legal
    CHECK (edge.dbHigh - edge.dbLow >= 12.0);
}

TEST_CASE ("unknown sample rate means no Nyquist limit", "[gui][format][range]")
{
    for (const double sr : { 0.0, -1.0 })
    {
        const auto r = computeEffectiveRange (20.0f, 40000.0f, -90.0f, 6.0f, sr);
        CHECK (r.nyquist == 0.0);
        CHECK (r.freqHigh == 40000.0);
        CHECK_FALSE (r.freqHighClamped);
        CHECK (clampTooltip (r, ParamIDs::freqHigh).isEmpty());
    }
}

TEST_CASE ("chained clamp: freqLow follows the clamped freqHigh", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (20000.0f, 40000.0f, -90.0f, 6.0f, 48000.0);
    CHECK (r.freqHigh == 24000.0);
    CHECK (r.freqLow == 12000.0);
    CHECK (r.freqLowClamped);
    CHECK (r.freqHighClamped);
    CHECK (clampTooltip (r, ParamIDs::freqLow) == "Effective 12.0 kHz: the range must span at least one octave.");
}

TEST_CASE ("isClamped is false for parameters that are never clamped", "[gui][format][range]")
{
    const auto r = computeEffectiveRange (1000.0f, 1000.0f, -30.0f, -30.0f, 8000.0);
    CHECK_FALSE (isClamped (r, ParamIDs::dbHigh));
    CHECK_FALSE (isClamped (r, ParamIDs::avgTime));
    CHECK_FALSE (isClamped (r, "nonsense"));
    CHECK (clampTooltip (r, "nonsense").isEmpty());
}

//==============================================================================
TEST_CASE ("NoticesText::build has no missing license text", "[gui][notices]")
{
    const auto blocks = NoticesText::build ("JUCE v8.0.15");
    REQUIRE_FALSE (blocks.empty());

    int licenseBlocks = 0;

    for (const auto& b : blocks)
    {
        CHECK_FALSE (b.text.containsIgnoreCase ("License text missing"));
        CHECK (b.text.isNotEmpty());

        if (b.kind == NoticesText::Block::Kind::licenseText)
        {
            ++licenseBlocks;
            CHECK (b.text.length() > 200);
        }
    }

    CHECK (licenseBlocks == 3);
    CHECK_FALSE (NoticesText::toPlainText (blocks).containsIgnoreCase ("License text missing"));
}

TEST_CASE ("NoticesText content follows T-023 section 4", "[gui][notices]")
{
    const auto text = NoticesText::toPlainText (NoticesText::build ("JUCE v8.0.15"));
    CHECK (text.contains ("JUCE v8.0.15"));
    CHECK (text.contains ("Inter 4.1"));
    CHECK (text.contains ("JetBrains Mono 2.304"));
    CHECK (text.contains ("VST 3 SDK"));
    CHECK (text.contains ("registered trademark of Steinberg Media Technologies GmbH"));
    CHECK (text.contains ("pffft"));
    CHECK (text.containsIgnoreCase ("SIL Open Font License"));
    CHECK (text.contains ("zlib"));
    CHECK (text.contains ("HarfBuzz"));
    CHECK (text.contains ("SheenBidi"));
}

TEST_CASE ("NoticesText: the embedded licenses are the real texts", "[gui][notices]")
{
    const auto blocks = NoticesText::build ("x");
    juce::StringArray licenses;

    for (const auto& b : blocks)
        if (b.kind == NoticesText::Block::Kind::licenseText)
            licenses.add (b.text);

    REQUIRE (licenses.size() == 3);
    CHECK (licenses[0].containsIgnoreCase ("SIL OPEN FONT LICENSE"));
    CHECK (licenses[0].contains ("Inter Project Authors"));
    CHECK (licenses[1].containsIgnoreCase ("SIL OPEN FONT LICENSE"));
    CHECK (licenses[1].contains ("JetBrains"));
    CHECK (licenses[2].contains ("Dario Mambro"));
    CHECK (licenses[2].containsIgnoreCase ("Redistribution"));
}
