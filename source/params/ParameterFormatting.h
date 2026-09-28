#pragma once

#include <juce_core/juce_core.h>

namespace investigate::ParameterFormatting
{

/** Glyphs the UI spec requires in every displayed value (docs/design/ui-spec.md
    top matter: "Minus signs in displayed text are U+2212 ... Multiplication
    signs are U+00D7"). Host automation lanes use `getText()`, which must
    match what the editor draws, so these string-conversion functions use the
    same glyphs as the GUI rather than ASCII "-" or "x". */
const juce::String minusSign    = juce::String::fromUTF8 ("\xE2\x88\x92");
const juce::String multiplySign = juce::String::fromUTF8 ("\xC3\x97");

// avgTime: 0 -> "Off"; 1-999 ms -> "350 ms"; 1000-9999 ms -> "1.00 s";
// 10000 ms -> "10.0 s" (docs/design/ui-spec.md section 6 control inventory).
juce::String avgTimeToText (float ms, int maximumStringLength);
float textToAvgTimeMs (const juce::String& text);

// rmsWindow: below 1000 ms -> "300 ms"; at or above -> "1.50 s".
juce::String rmsWindowToText (float ms, int maximumStringLength);
float textToRmsWindowMs (const juce::String& text);

// freqLow / freqHigh: round to 3 significant figures, then format per
// docs/design/ui-spec.md section 5.5: below 100 Hz 1 decimal, 100-999 Hz
// 0 decimals, 1-9.99 kHz 2 decimals, >= 10 kHz 1 decimal.
juce::String frequencyToText (float hz, int maximumStringLength);
float textToFrequencyHz (const juce::String& text);

// slope: signed, 1 decimal, "0.0" at zero, e.g. "+4.5 dB/oct", "-1.5 dB/oct".
juce::String slopeToText (float dbPerOctave, int maximumStringLength);
float textToSlope (const juce::String& text);

// dbLow: always negative in its [-150, -30] range, e.g. "-90 dB".
juce::String dbLowToText (float db, int maximumStringLength);
float textToDbLow (const juce::String& text);

// dbHigh: signed, zero shows "0 dB" with no sign, e.g. "+6 dB", "-12 dB".
juce::String dbHighToText (float db, int maximumStringLength);
float textToDbHigh (const juce::String& text);

} // namespace investigate::ParameterFormatting
