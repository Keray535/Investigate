#include "ParameterFormatting.h"

#include <cmath>

namespace investigate::ParameterFormatting
{

namespace
{
    /// Normalises the two accepted minus-sign spellings and the accepted
    /// decimal-separator spelling (docs/design/ui-spec.md section 7.1:
    /// "Accepts ASCII '-' or U+2212, ',' as the decimal separator") so the
    /// rest of the parser can just call juce::String::getFloatValue().
    juce::String normaliseForParsing (const juce::String& text)
    {
        return text.trim().replace (minusSign, "-").replaceCharacter (',', '.');
    }

    float roundToSignificantFigures (float value, int sigFigs)
    {
        if (value == 0.0f || ! std::isfinite (value))
            return 0.0f;

        const auto magnitudeDigits = std::ceil (std::log10 (std::abs ((double) value)));
        const auto power = (double) sigFigs - magnitudeDigits;
        const auto scale = std::pow (10.0, power);
        return (float) (std::round ((double) value * scale) / scale);
    }
}

//==============================================================================
juce::String avgTimeToText (float ms, int)
{
    if (ms <= 0.0f)
        return "Off";

    // Pick the display bucket from the value *after* rounding to the
    // precision that bucket actually shows, not from the raw value -
    // otherwise a value near a bucket boundary (e.g. 9999 ms, which rounds
    // to "10.00 s") lands in the wrong bucket and the round trip through
    // textToAvgTimeMs is not idempotent (re-parsing "10.00 s" yields exactly
    // 10000 ms, which belongs in the next bucket and reformats as "10.0 s").
    // frequencyToText avoids the same trap by rounding first; this mirrors
    // that approach one bucket boundary at a time.
    if (ms < 1000.0f)
    {
        const auto wholeMs = juce::roundToInt (ms);

        // A positive value that rounds down to 0 ms must still format as
        // "Off", matching what re-parsing "0 ms" produces (textToAvgTimeMs
        // parses it to exactly 0.0f, which the ms <= 0.0f check above turns
        // back into "Off") - otherwise getText(getValueForText(s)) is not
        // stable for s == "0 ms".
        if (wholeMs <= 0)
            return "Off";

        if (wholeMs < 1000)
            return juce::String (wholeMs) + " ms";

        ms = (float) wholeMs; // rounded up into the seconds bucket
    }

    auto seconds = ms / 1000.0f;

    if (ms < 10000.0f)
    {
        const auto rounded = std::round (seconds * 100.0f) / 100.0f;

        if (rounded < 10.0f)
            return juce::String (rounded, 2) + " s";

        seconds = rounded; // rounded up to 10.0 s or more
    }

    return juce::String (seconds, 1) + " s";
}

float textToAvgTimeMs (const juce::String& textIn)
{
    auto text = textIn.trim();

    if (text.equalsIgnoreCase ("off"))
        return 0.0f;

    text = normaliseForParsing (text);
    const auto lower = text.toLowerCase();
    const auto value = text.getFloatValue();

    if (lower.contains ("ms"))
        return juce::jlimit (0.0f, 10000.0f, value);

    if (lower.contains ("s"))
        return juce::jlimit (0.0f, 10000.0f, value * 1000.0f);

    // A bare number is milliseconds (matches the ui-spec text-entry example "250").
    return juce::jlimit (0.0f, 10000.0f, value);
}

//==============================================================================
juce::String rmsWindowToText (float ms, int)
{
    // Same fix as avgTimeToText: pick the bucket from the rounded value
    // (e.g. 999.6 ms rounds to "1000 ms" as an integer, but belongs in the
    // seconds bucket once rounded, or the round trip through
    // textToRmsWindowMs is not idempotent at the 1000 ms boundary).
    if (ms < 1000.0f)
    {
        const auto wholeMs = juce::roundToInt (ms);

        if (wholeMs < 1000)
            return juce::String (wholeMs) + " ms";

        ms = (float) wholeMs;
    }

    return juce::String (ms / 1000.0f, 2) + " s";
}

float textToRmsWindowMs (const juce::String& textIn)
{
    const auto text = normaliseForParsing (textIn);
    const auto lower = text.toLowerCase();
    const auto value = text.getFloatValue();

    if (lower.contains ("ms"))
        return juce::jlimit (50.0f, 3000.0f, value);

    if (lower.contains ("s"))
        return juce::jlimit (50.0f, 3000.0f, value * 1000.0f);

    return juce::jlimit (50.0f, 3000.0f, value);
}

//==============================================================================
juce::String frequencyToText (float hz, int)
{
    const auto f = roundToSignificantFigures (hz, 3);

    if (f < 100.0f)
        return juce::String (f, 1) + " Hz";

    if (f < 1000.0f)
        return juce::String (juce::roundToInt (f)) + " Hz";

    if (f < 10000.0f)
        return juce::String (f / 1000.0f, 2) + " kHz";

    return juce::String (f / 1000.0f, 1) + " kHz";
}

float textToFrequencyHz (const juce::String& textIn)
{
    const auto text = normaliseForParsing (textIn);
    const auto lower = text.toLowerCase();
    auto value = text.getFloatValue();

    if (lower.contains ("k"))
        value *= 1000.0f;

    return value;
}

//==============================================================================
juce::String slopeToText (float dbPerOctave, int)
{
    const auto rounded = std::round (dbPerOctave * 10.0f) / 10.0f;

    if (rounded > 0.0f)
        return "+" + juce::String (rounded, 1) + " dB/oct";

    if (rounded < 0.0f)
        return minusSign + juce::String (std::abs (rounded), 1) + " dB/oct";

    return "0.0 dB/oct";
}

float textToSlope (const juce::String& textIn)
{
    const auto text = normaliseForParsing (textIn);
    return juce::jlimit (-12.0f, 12.0f, text.getFloatValue());
}

//==============================================================================
juce::String dbLowToText (float db, int)
{
    const auto v = juce::roundToInt (db);
    return (v < 0 ? minusSign : juce::String())
         + juce::String (std::abs (v)) + " dB";
}

float textToDbLow (const juce::String& textIn)
{
    const auto text = normaliseForParsing (textIn);
    return juce::jlimit (-150.0f, -30.0f, text.getFloatValue());
}

//==============================================================================
juce::String dbHighToText (float db, int)
{
    const auto v = juce::roundToInt (db);

    if (v > 0)
        return "+" + juce::String (v) + " dB";

    if (v < 0)
        return minusSign + juce::String (std::abs (v)) + " dB";

    return "0 dB";
}

float textToDbHigh (const juce::String& textIn)
{
    const auto text = normaliseForParsing (textIn);
    return juce::jlimit (-30.0f, 30.0f, text.getFloatValue());
}

} // namespace investigate::ParameterFormatting
