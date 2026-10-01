#include "ValueFormat.h"

#include "../dsp/DisplayRange.h"
#include "../params/ParameterFormatting.h"
#include "../params/ParameterIDs.h"

#include <cmath>

namespace investigate::ValueFormat
{

namespace
{
    bool hasUnit (const juce::String& id) noexcept
    {
        return id == ParamIDs::avgTime || id == ParamIDs::smoothing || id == ParamIDs::slope
            || id == ParamIDs::freqLow || id == ParamIDs::freqHigh
            || id == ParamIDs::dbLow || id == ParamIDs::dbHigh || id == ParamIDs::rmsWindow;
    }
}

ChipText splitChipText (const juce::String& paramID, const juce::String& paramText)
{
    if (! hasUnit (paramID))
        return { paramText, {} };

    const auto space = paramText.indexOfChar (' ');

    if (space <= 0)
        return { paramText, {} };

    return { paramText.substring (0, space), paramText.substring (space + 1).trim() };
}

juce::String formatSignedDb (float db)
{
    // roundToInt rounds half up; the spec asks for half away from zero.
    const int v = (int) std::lround ((double) db);

    if (v > 0)
        return "+" + juce::String (v);

    if (v < 0)
        return ParameterFormatting::minusSign + juce::String (-v);

    return "0";
}

juce::String formatLevelRange (float dbLow, float dbHigh)
{
    return formatSignedDb (dbLow) + " " + juce::String::fromUTF8 ("\xE2\x80\xA6") + " "
         + formatSignedDb (dbHigh) + " dB";
}

juce::String formatSampleRate (double sampleRateHz)
{
    if (! (sampleRateHz > 0.0))
        return {};

    auto text = juce::String (sampleRateHz / 1000.0, 1);

    if (text.endsWith (".0"))
        text = text.dropLastCharacters (2);

    return text + " kHz";
}

bool isParsableNumericText (const juce::String& text, bool allowOff)
{
    if (allowOff && text.trim().equalsIgnoreCase ("off"))
        return true;

    return text.containsAnyOf ("0123456789");
}

//==============================================================================
EffectiveRange computeEffectiveRange (float freqLow, float freqHigh, float dbLow, float dbHigh, double sampleRate)
{
    EffectiveRange r;
    const double sr = sampleRate > 0.0 ? sampleRate : 1.0e12;

    r.nyquist  = sampleRate > 0.0 ? sampleRate * 0.5 : 0.0;
    r.freqHigh = DisplayRange::effectiveFreqHigh (freqHigh, sr);
    r.freqLow  = DisplayRange::effectiveFreqLow (freqLow, r.freqHigh);
    r.dbHigh   = DisplayRange::effectiveDbHigh (dbHigh);
    r.dbLow    = DisplayRange::effectiveDbLow (dbLow, dbHigh);

    r.freqHighClamped = r.freqHigh != (double) freqHigh;
    r.freqLowClamped  = r.freqLow != (double) freqLow;
    r.dbLowClamped    = r.dbLow != (double) dbLow;
    return r;
}

bool isClamped (const EffectiveRange& range, const juce::String& paramID)
{
    if (paramID == ParamIDs::freqLow)  return range.freqLowClamped;
    if (paramID == ParamIDs::freqHigh) return range.freqHighClamped;
    if (paramID == ParamIDs::dbLow)    return range.dbLowClamped;
    return false;
}

juce::String clampTooltip (const EffectiveRange& range, const juce::String& paramID)
{
    using ParameterFormatting::frequencyToText;

    if (paramID == ParamIDs::freqHigh && range.freqHighClamped)
        return "Effective " + frequencyToText ((float) range.freqHigh, 0)
             + ": limited by Nyquist (" + frequencyToText ((float) range.nyquist, 0) + ").";

    if (paramID == ParamIDs::freqLow && range.freqLowClamped)
        return "Effective " + frequencyToText ((float) range.freqLow, 0)
             + ": the range must span at least one octave.";

    if (paramID == ParamIDs::dbLow && range.dbLowClamped)
        return "Effective " + formatSignedDb ((float) range.dbLow)
             + " dB: the range must span at least 12 dB.";

    return {};
}

} // namespace investigate::ValueFormat
