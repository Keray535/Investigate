#include "ControlMath.h"

#include "DesignTokens.h"
#include "../params/ParameterIDs.h"

#include <algorithm>
#include <cmath>

namespace investigate::ControlMath
{

namespace L = investigate::tokens::layout;

namespace
{
    float snapStepMs (float ms) noexcept
    {
        return ms < 1000.0f ? 10.0f : 100.0f;
    }

    float clampToRange (const juce::NormalisableRange<float>& range, float v) noexcept
    {
        return juce::jlimit (range.start, range.end, v);
    }

    /** The real value `delta` normalised units away from `value`. */
    float offsetNormalised (const juce::NormalisableRange<float>& range, float value, float delta) noexcept
    {
        const auto norm = juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (value) + delta);
        return range.convertFrom0to1 (norm);
    }
}

Behaviour behaviourFor (const juce::String& paramID, bool isChoice)
{
    Behaviour b;

    if (isChoice)
        return b;

    if (paramID == ParamIDs::avgTime || paramID == ParamIDs::rmsWindow)
    {
        b.kind = Kind::skewed;
    }
    else if (paramID == ParamIDs::freqLow || paramID == ParamIDs::freqHigh)
    {
        b.kind = Kind::logFrequency;
    }
    else if (paramID == ParamIDs::slope)
    {
        b.kind = Kind::linearStep;
        b.dragUnitsPerPx = L::slopeDragUnitsPerPx;
        b.dragUnitsPerPxFine = L::slopeDragUnitsPerPxFine;
        b.wheelStep = 0.5f;
    }
    else if (paramID == ParamIDs::dbLow || paramID == ParamIDs::dbHigh)
    {
        b.kind = Kind::linearStep;
        b.dragUnitsPerPx = L::dbDragUnitsPerPx;
        b.dragUnitsPerPxFine = L::dbDragUnitsPerPxFine;
        b.wheelStep = 1.0f;
    }

    return b;
}

float snapMilliseconds (float ms) noexcept
{
    if (ms <= 0.0f)
        return 0.0f;

    const auto step = snapStepMs (ms);
    auto snapped = std::round (ms / step) * step;

    // 995 ms snaps up to 1000 ms, which is already on the 100 ms grid.
    if (snapped >= 1000.0f && ms < 1000.0f)
        snapped = 1000.0f;

    return snapped;
}

float roundToThreeSignificantFigures (float value) noexcept
{
    if (value == 0.0f || ! std::isfinite (value))
        return 0.0f;

    const auto magnitudeDigits = std::ceil (std::log10 (std::abs ((double) value)));
    const auto scale = std::pow (10.0, 3.0 - magnitudeDigits);
    return (float) (std::round ((double) value * scale) / scale);
}

float applyDrag (const juce::NormalisableRange<float>& range, const Behaviour& behaviour,
                 float startValue, float deltaYPixels, bool fine) noexcept
{
    switch (behaviour.kind)
    {
        case Kind::choice:
        {
            const auto px = fine ? L::choicePixelsPerStepFine : L::choicePixelsPerStep;
            const auto steps = (float) std::lround (deltaYPixels / px);
            return clampToRange (range, range.snapToLegalValue (startValue + steps));
        }

        case Kind::skewed:
        {
            const auto delta = deltaYPixels / L::dragPixelsFullRange * (fine ? L::fineFactor : 1.0f);
            return clampToRange (range, snapMilliseconds (offsetNormalised (range, startValue, delta)));
        }

        case Kind::logFrequency:
        {
            const auto delta = deltaYPixels / L::dragPixelsFullRange * (fine ? L::fineFactor : 1.0f);
            return clampToRange (range, roundToThreeSignificantFigures (offsetNormalised (range, startValue, delta)));
        }

        case Kind::linearStep:
        {
            const auto perPx = fine ? behaviour.dragUnitsPerPxFine : behaviour.dragUnitsPerPx;
            return clampToRange (range, range.snapToLegalValue (startValue + deltaYPixels * perPx));
        }
    }

    return startValue;
}

float applyStep (const juce::NormalisableRange<float>& range, const Behaviour& behaviour,
                 float currentValue, int direction, bool fine, int steps) noexcept
{
    const auto dir = direction >= 0 ? 1.0f : -1.0f;
    const auto n = (float) std::max (1, steps);

    switch (behaviour.kind)
    {
        case Kind::choice:
            return clampToRange (range, range.snapToLegalValue (currentValue + dir * n));

        case Kind::skewed:
        {
            const auto delta = dir * n * (fine ? L::wheelNormalisedFine : L::wheelNormalised);
            auto next = snapMilliseconds (offsetNormalised (range, currentValue, delta));

            // "Always at least one snap step" (ui-spec 6, avgTime and rmsWindow).
            if (next == currentValue)
            {
                const auto step = snapStepMs (dir > 0.0f ? currentValue : currentValue - 1.0f);
                next = currentValue + dir * n * step;
            }

            return clampToRange (range, snapMilliseconds (next));
        }

        case Kind::logFrequency:
        {
            const auto exponent = (fine ? 1.0f / 24.0f : 1.0f / 6.0f) * dir * n;
            auto next = roundToThreeSignificantFigures (currentValue * std::pow (2.0f, exponent));

            if (next == currentValue)
                next = roundToThreeSignificantFigures (currentValue * (1.0f + 0.01f * dir));

            return clampToRange (range, next);
        }

        case Kind::linearStep:
            return clampToRange (range, range.snapToLegalValue (currentValue + dir * n * behaviour.wheelStep));
    }

    return currentValue;
}

} // namespace investigate::ControlMath
