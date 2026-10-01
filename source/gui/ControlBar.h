#pragma once

#include "LayoutMath.h"
#include "ParamControl.h"
#include "RangeChip.h"

#include <functional>

namespace investigate
{

/**
    The control bar under the spectrum (ui-spec 4.3): group A (Block, Overlap,
    Window, Avg time, Smooth), group B (Slope, 2nd curve) and the right-aligned
    group C (Frequency, Level). Groups B and C hide when the bar is too narrow;
    the Settings panel keeps every control reachable, and the editor shows the
    Settings dot while anything is hidden.
*/
class ControlBar final : public juce::Component
{
public:
    ControlBar (juce::AudioProcessorValueTreeState& apvts, DisplayContext& context);
    ~ControlBar() override;

    bool hasHiddenGroups() const noexcept { return hiddenGroups; }

    /** Called when `hasHiddenGroups()` changes. */
    std::function<void()> onHiddenGroupsChanged;

    /** Repaints the controls whose warning colour depends on the range. */
    void refreshClampState();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ParamControl block, overlap, window, avgTime, smoothing, slope, secondary;
    RangeChip frequency, level;

    LayoutMath::ControlBarLayout layout;
    bool hiddenGroups = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlBar)
};

} // namespace investigate
