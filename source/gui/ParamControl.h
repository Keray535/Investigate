#pragma once

#include "ControlMath.h"
#include "DisplayContext.h"
#include "InvestigateLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace investigate
{

/**
    One parameter control: a "chip" (label over value), an inline chip, a
    settings-panel field, or one half of a range chip (ui-spec 6, 7 and 9).

    It binds to its parameter with a `juce::ParameterAttachment`, which
    carries host automation and state loads to the UI within a frame and
    wraps each user change in begin/end gesture calls. Interaction follows
    ui-spec 7.1: vertical drag (Shift for fine), wheel steps, a 250 ms
    deferred click that opens a menu (choices) or text entry (floats),
    double-click or Alt+click to reset to the default, and the keyboard map
    of 7.2.

    Message-thread only.
*/
class ParamControl : public juce::Component,
                     public juce::TooltipClient,
                     private juce::Timer
{
public:
    enum class Style
    {
        stacked,    // 40 px chip: label above value
        inlineChip, // 28 px chip: label left, value right
        field,      // 128 x 24 settings field: value only
        half        // one side of a range chip: value only, no chrome
    };

    /** `label` is the visible caps label (stacked and inline chips);
        `accessibleName` is the full name exposed to assistive technology. */
    ParamControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                  Style style, const juce::String& label, const juce::String& accessibleName,
                  DisplayContext* context = nullptr);
    ~ParamControl() override;

    /** Overrides the tooltip's first line (the how-to line is appended). */
    void setTooltipSource (std::function<juce::String()> firstLine);

    /** Left edge of the value text, for halves. */
    void setTextInset (int inset) noexcept { textInset = inset; }

    /** Called after any value change (user, automation or state load), on
        the message thread. */
    std::function<void()> onValueChanged;

    /** Called for a change that did not come from this control's own
        gesture (automation, state load, a twin control). */
    std::function<void()> onExternalChange;

    const juce::String& getParamID() const noexcept { return id; }
    float getValue() const noexcept { return value; }

    /** The value as displayed ("4.5 dB/oct", "−90 … " is not used here:
        halves return their own half), for accessibility. */
    juce::String getDisplayString() const;

    /** Commits typed text as one gesture. Returns false (and flashes the
        error outline) if it cannot be parsed. */
    bool commitText (const juce::String& text);

    /** True while the external-change dot should show. */
    bool isShowingExternalDot() const noexcept { return dotUntil != 0; }

    // juce::Component
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    // juce::TooltipClient
    juce::String getTooltip() override;

private:
    class EntryEditor;
    class AccessibilityValue;

    void timerCallback() override;
    void updateTimerState();

    void handleParameterChanged (float newValue);
    void applyComplete (float newValue);
    void applyPartOfGesture (float newValue);
    void resetToDefault();
    void stepBy (int direction, bool fine, int steps);

    void performPrimaryAction();
    void showChoiceMenu();
    void openEntry();
    void closeEntry (bool commit);
    void flashError();

    juce::Rectangle<int> getValueRect() const;
    ValueFormat::ChipText getChipText() const;
    tokens::Type getValueType() const noexcept;
    juce::Colour getValueColour() const;

    juce::String id;
    juce::RangedAudioParameter& param;
    Style style;
    juce::String label;
    DisplayContext* context;
    bool isChoice;
    ControlMath::Behaviour behaviour;

    float value = 0.0f;
    int textInset = 0;
    std::function<juce::String()> tooltipSource;

    bool hovering = false;
    bool pressed = false;
    bool dragging = false;
    bool altHandled = false;
    bool fineAtAnchor = false;
    bool keyboardFocusRing = false;
    bool localChange = false;
    bool gestureOpen = false;
    float dragStartValue = 0.0f;
    int dragAnchorScreenY = 0;

    // One 25 ms timer runs only while one of these is pending (0 = idle).
    juce::uint32 clickDeadline = 0;
    juce::uint32 dotUntil = 0;
    juce::uint32 errorUntil = 0;

    std::unique_ptr<EntryEditor> entry;

    // Declared last so it is destroyed first and stops calling back.
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamControl)
};

} // namespace investigate
