#pragma once

#include "InvestigateLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace investigate
{

/**
    A header or meter-strip button (ui-spec 4.2, 7.1, 9): an icon with an
    optional label, either an action (acts on release) or a toggle (toggles
    on press). It covers the SC, Freeze, Reset curves, Meters, Settings and
    Reset statistics buttons, with their on, frozen, inactive and dot states.
*/
class HeaderButton final : public juce::Component,
                           public juce::TooltipClient
{
public:
    enum class Kind { action, toggle };
    enum class Look { accent, frozen };

    HeaderButton (Icon icon, const juce::String& text, Kind kind, const juce::String& accessibleName);
    ~HeaderButton() override;

    void setToggled (bool shouldBeOn);
    bool isToggled() const noexcept { return toggled; }

    void setLook (Look newLook);

    /** SC only: the sidechain bus is not connected (ui-spec 9). */
    void setBusInactive (bool inactive);

    /** The 6 px accent dot at the top-right (Settings, ui-spec 4.2). */
    void setShowDot (bool show);

    /** Draws the icon mirrored horizontally (Meters when collapsed). */
    void setMirrored (bool shouldMirror);

    void setTooltipSource (std::function<juce::String()> source) { tooltipSource = std::move (source); }

    std::function<void()> onClick;
    std::function<void()> onDoubleClick;
    std::function<void()> onAltClick;
    std::function<void (int)> onWheel;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

    juce::String getTooltip() override;

    /** Natural width for a text button: padding, icon, gap and label. */
    int getIdealWidth() const;

private:
    class Handler;

    void trigger();

    Icon icon;
    juce::String text;
    Kind kind;
    Look look = Look::accent;

    bool toggled = false;
    bool busInactive = false;
    bool showDot = false;
    bool mirrored = false;
    bool hovering = false;
    bool pressed = false;
    bool keyboardFocusRing = false;

    std::function<juce::String()> tooltipSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderButton)
};

} // namespace investigate
