#include "ViewStateStore.h"

#include <cmath>

namespace investigate::ViewStateStore
{

namespace
{
    // A state saved through XML (createXml / fromXml) returns every property
    // as a string, so numeric strings count as numbers.
    bool isNumeric (const juce::var& v) noexcept
    {
        if (v.isInt() || v.isInt64() || v.isDouble())
            return true;

        return v.isString() && v.toString().trim().containsOnly ("0123456789.-+eE")
            && v.toString().containsAnyOf ("0123456789");
    }

    int toClampedInt (const juce::var& v) noexcept
    {
        const double d = v.isString() ? v.toString().getDoubleValue() : (double) v;

        if (! std::isfinite (d))
            return 0;

        return (int) juce::jlimit (-1.0e6, 1.0e6, d);
    }

    int readDimension (const juce::ValueTree& view, const char* name, int fallback)
    {
        if (! view.isValid() || ! view.hasProperty (name))
            return fallback;

        const auto& v = view.getProperty (name);
        return isNumeric (v) ? toClampedInt (v) : fallback;
    }
}

LayoutMath::EditorSize readEditorSize (const juce::ValueTree& rootState)
{
    const auto view = rootState.getChildWithName (viewStateType);

    return LayoutMath::clampEditorSize (readDimension (view, editorWidthProp, LayoutMath::L::defaultWidth),
                                        readDimension (view, editorHeightProp, LayoutMath::L::defaultHeight));
}

void writeEditorSize (juce::ValueTree rootState, LayoutMath::EditorSize size)
{
    if (! rootState.isValid())
        return;

    auto view = rootState.getOrCreateChildWithName (viewStateType, nullptr);
    const auto clamped = LayoutMath::clampEditorSize (size.width, size.height);

    // Avoid notifying listeners when nothing changed.
    if (readDimension (view, editorWidthProp, -1) != clamped.width)
        view.setProperty (editorWidthProp, clamped.width, nullptr);

    if (readDimension (view, editorHeightProp, -1) != clamped.height)
        view.setProperty (editorHeightProp, clamped.height, nullptr);
}

bool readMetersCollapsed (const juce::ValueTree& rootState)
{
    const auto view = rootState.getChildWithName (viewStateType);

    if (! view.isValid() || ! view.hasProperty (metersCollapsedProp))
        return false;

    const auto& v = view.getProperty (metersCollapsedProp);

    if (v.isString())
    {
        const auto t = v.toString().trim();
        return t.equalsIgnoreCase ("true") || (isNumeric (v) && t.getDoubleValue() != 0.0);
    }

    return (v.isBool() || isNumeric (v)) ? (bool) v : false;
}

void writeMetersCollapsed (juce::ValueTree rootState, bool collapsed)
{
    if (! rootState.isValid())
        return;

    rootState.getOrCreateChildWithName (viewStateType, nullptr)
             .setProperty (metersCollapsedProp, collapsed, nullptr);
}

} // namespace investigate::ViewStateStore
