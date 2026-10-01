#pragma once

#include "InvestigateLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace investigate
{

/**
    The channel-mode segmented control in the header (ui-spec 4.2, 6, 7.1):
    two segment groups, single (Mid, Side, L, R, L+R) and dual (L/R, M/S),
    bound to `channelMode`. A click selects at once; double-click or Alt+click
    resets to Mid; the wheel and arrow keys step; Delete resets.
*/
class ModeSelector final : public juce::Component,
                           public juce::TooltipClient
{
public:
    explicit ModeSelector (juce::AudioProcessorValueTreeState& apvts);
    ~ModeSelector() override;

    int getSelectedIndex() const noexcept { return selected; }

    /** Called when the mode changes from any source after construction
        (the editor uses it to turn Freeze off, ui-spec 5.7). */
    std::function<void()> onModeChanged;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    juce::String getTooltip() override;

    static constexpr int numModes = 7;

private:
    class AccessibilityValue;

    int indexAt (juce::Point<int> p) const noexcept;
    void select (int index);
    void resetToDefault();
    void stepBy (int direction);
    void handleParameterChanged (float newValue);

    juce::RangedAudioParameter& param;
    std::array<juce::Rectangle<int>, numModes> segments {};
    std::array<juce::Rectangle<int>, 2> tracks {};

    int selected = 0;
    int hoveredIndex = -1;
    bool keyboardFocusRing = false;

    // Declared last: destroyed first.
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModeSelector)
};

} // namespace investigate
