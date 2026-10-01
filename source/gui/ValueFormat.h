#pragma once

#include <juce_core/juce_core.h>

/** Pure display-text helpers for the editor (docs/design/ui-spec.md sections
    5.4, 6 and 8.8). They take plain values or strings, so they are unit
    testable without any UI. */
namespace investigate::ValueFormat
{

/** A chip value split into its number or choice text and its trailing unit
    ("4.5" + "dB/oct"). The unit is drawn smaller, 3 px after the value. */
struct ChipText
{
    juce::String value;
    juce::String unit;
};

/** Splits `paramText` (a parameter's getText() string) at its first space
    for the parameters whose display carries a unit (avgTime, smoothing,
    slope, freqLow, freqHigh, dbLow, dbHigh, rmsWindow). Other parameters
    (pure choices such as "All-time max", or "Off") are returned whole. */
ChipText splitChipText (const juce::String& paramID, const juce::String& paramText);

/** A dB value rounded half away from zero to a whole number, with U+2212 for
    negatives, "+" for positives and a bare "0" for zero ("-90", "+6", "0").
    Used by both halves of the Level chip, built from numeric values rather
    than getText(), which always appends " dB". */
juce::String formatSignedDb (float db);

/** The two-halves Level chip as one string for accessibility and tests:
    "-90 ... +6 dB" with "…" (U+2026) and U+2212. */
juce::String formatLevelRange (float dbLow, float dbHigh);

/** ui-spec 8.8: kHz with one decimal and a trailing ".0" dropped:
    "44.1 kHz", "48 kHz", "176.4 kHz". Empty for a non-positive rate. */
juce::String formatSampleRate (double sampleRateHz);

/** True if `text` contains something a numeric parameter parser can use: at
    least one digit, or the word "off" when `allowOff` is set. Text entry uses
    it to decide between committing and the error flash. */
bool isParsableNumericText (const juce::String& text, bool allowOff);

//==============================================================================
/** The effective display range after the shared DisplayRange.h clamps
    (ui-spec 5.4), plus which set values are being overridden. */
struct EffectiveRange
{
    double freqLow = 20.0, freqHigh = 20000.0, dbLow = -90.0, dbHigh = 6.0;
    double nyquist = 0.0; // 0 when the sample rate is unknown
    bool freqLowClamped = false, freqHighClamped = false, dbLowClamped = false;
};

/** `sampleRate <= 0` means unknown (no analysis frame yet): no Nyquist limit. */
EffectiveRange computeEffectiveRange (float freqLow, float freqHigh, float dbLow, float dbHigh, double sampleRate);

/** True when the control for `paramID` shows its set value in the warning
    colour. */
bool isClamped (const EffectiveRange& range, const juce::String& paramID);

/** The tooltip sentence for a clamped value, e.g. "Effective 24.0 kHz:
    limited by Nyquist (24.0 kHz)." Empty when `paramID` is not clamped. */
juce::String clampTooltip (const EffectiveRange& range, const juce::String& paramID);

} // namespace investigate::ValueFormat
