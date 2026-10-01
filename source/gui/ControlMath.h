#pragma once

#include <juce_core/juce_core.h>

/** Pure value arithmetic for chip drag, wheel and keyboard steps (ui-spec 6
    control inventory and 7.1). Values are real (denormalised) parameter
    values; the functions take the parameter's NormalisableRange so they can
    be tested without an APVTS. */
namespace investigate::ControlMath
{

enum class Kind
{
    choice,      // index steps, 20 px per step
    skewed,      // avgTime, rmsWindow: normalised drag, ms snap
    linearStep,  // slope, dbLow, dbHigh: fixed units per pixel / per wheel step
    logFrequency // freqLow, freqHigh: normalised drag, 3 significant figures
};

struct Behaviour
{
    Kind kind = Kind::choice;
    float dragUnitsPerPx = 0.0f;      // linearStep
    float dragUnitsPerPxFine = 0.0f;  // linearStep
    float wheelStep = 0.0f;           // linearStep (also used for Shift)
};

/** The behaviour of the parameter `paramID` for float controls, or `choice`
    for anything else. `isChoice` forces Kind::choice (for choice and bool
    parameters). */
Behaviour behaviourFor (const juce::String& paramID, bool isChoice);

/** 10 ms below 1 s, 100 ms at or above (ui-spec 6, avgTime and rmsWindow);
    0 stays 0. */
float snapMilliseconds (float ms) noexcept;

/** Rounds to three significant figures. */
float roundToThreeSignificantFigures (float value) noexcept;

/** A drag of `deltaYPixels` (up is positive, measured from the press point)
    applied to the value the drag started from. */
float applyDrag (const juce::NormalisableRange<float>& range, const Behaviour& behaviour,
                 float startValue, float deltaYPixels, bool fine) noexcept;

/** One wheel notch, arrow key or page step in `direction` (+1 or -1);
    `steps` multiplies it (PageUp and PageDown use 5). The result always
    differs from `currentValue` unless the range edge is reached. */
float applyStep (const juce::NormalisableRange<float>& range, const Behaviour& behaviour,
                 float currentValue, int direction, bool fine, int steps = 1) noexcept;

} // namespace investigate::ControlMath
