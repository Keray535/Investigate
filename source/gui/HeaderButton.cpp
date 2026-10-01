#include "HeaderButton.h"

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

class HeaderButton::Handler final : public juce::AccessibilityHandler
{
public:
    explicit Handler (HeaderButton& b)
        : juce::AccessibilityHandler (b,
                                      b.kind == Kind::toggle ? juce::AccessibilityRole::toggleButton
                                                             : juce::AccessibilityRole::button,
                                      makeActions (b)),
          owner (b)
    {
    }

    juce::AccessibleState getCurrentState() const override
    {
        auto state = juce::AccessibilityHandler::getCurrentState();

        if (owner.kind == Kind::toggle)
        {
            state = state.withCheckable();

            if (owner.toggled)
                state = state.withChecked();
        }

        return state;
    }

private:
    static juce::AccessibilityActions makeActions (HeaderButton& b)
    {
        auto actions = juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [&b] { b.trigger(); });

        if (b.kind == Kind::toggle)
            actions = actions.addAction (juce::AccessibilityActionType::toggle, [&b] { b.trigger(); });

        return actions;
    }

    HeaderButton& owner;
};

HeaderButton::HeaderButton (Icon iconToUse, const juce::String& labelText, Kind kindToUse,
                            const juce::String& accessibleName)
    : icon (iconToUse), text (labelText), kind (kindToUse)
{
    setTitle (accessibleName);
    setWantsKeyboardFocus (true);
}

HeaderButton::~HeaderButton() = default;

void HeaderButton::setToggled (bool shouldBeOn)
{
    if (toggled == shouldBeOn)
        return;

    toggled = shouldBeOn;
    repaint();
}

void HeaderButton::setLook (Look newLook)
{
    look = newLook;
    repaint();
}

void HeaderButton::setBusInactive (bool inactive)
{
    if (busInactive == inactive)
        return;

    busInactive = inactive;
    repaint();
}

void HeaderButton::setShowDot (bool show)
{
    if (showDot == show)
        return;

    showDot = show;
    repaint();
}

void HeaderButton::setMirrored (bool shouldMirror)
{
    if (mirrored == shouldMirror)
        return;

    mirrored = shouldMirror;
    repaint();
}

int HeaderButton::getIdealWidth() const
{
    auto& laf = InvestigateLookAndFeel::from (*this);
    return 2 * L::buttonPadX + L::iconSize + L::buttonIconGap + (int) std::ceil (laf.getTextWidth (text, tk::Type::body));
}

//==============================================================================
void HeaderButton::paint (juce::Graphics& g)
{
    auto& laf = InvestigateLookAndFeel::from (*this);
    const auto bounds = getLocalBounds().toFloat();
    const bool frozenOn = toggled && look == Look::frozen;
    const bool on = toggled;

    juce::Colour fill = juce::Colours::transparentBlack;
    juce::Colour border = juce::Colours::transparentBlack;
    juce::Colour content = tk::colour::textSecondary;

    if (on)
    {
        fill = frozenOn ? tk::colour::stateFrozenTint : tk::colour::accentTint;
        border = (frozenOn ? tk::colour::stateFrozen.withAlpha (tk::alpha::frozenBorder)
                           : tk::colour::accent.withAlpha (tk::alpha::toggleOnBorder));
        content = frozenOn ? tk::colour::stateFrozen : tk::colour::accent;
    }
    else if (pressed || hovering)
    {
        fill = tk::colour::surface3;
        content = tk::colour::textPrimary;
    }

    if (busInactive)
    {
        content = tk::colour::textDisabled;

        if (on)
            border = tk::colour::border;
    }

    if (! fill.isTransparent())
    {
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, tk::radius::control);
    }

    if (! border.isTransparent())
    {
        g.setColour (border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), tk::radius::control, tk::stroke::hairline);
    }

    // Icon and label are centred as one group.
    const int labelW = text.isEmpty() ? 0 : (int) std::ceil (laf.getTextWidth (text, tk::Type::body));
    const int ringW = busInactive && text.isNotEmpty() ? (tk::space::s4 + L::settingsDotSize) : 0;
    const int groupW = L::iconSize + (labelW > 0 ? L::buttonIconGap + labelW : 0) + ringW;
    const int x0 = (getWidth() - groupW) / 2;
    const int iconY = (getHeight() - L::iconSize) / 2;
    const auto iconRect = juce::Rectangle<float> ((float) x0, (float) iconY, (float) L::iconSize, (float) L::iconSize);

    if (const auto* drawable = laf.getIcon (icon, content, true))
    {
        if (mirrored)
        {
            juce::Graphics::ScopedSaveState state (g);
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (iconRect.getX() * 2.0f + iconRect.getWidth(), 0.0f));
            drawable->drawWithin (g, iconRect, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            drawable->drawWithin (g, iconRect, juce::RectanglePlacement::centred, 1.0f);
        }
    }

    if (labelW > 0)
    {
        const int tx = x0 + L::iconSize + L::buttonIconGap;
        laf.drawText (g, text, { tx, 0, labelW + 2, getHeight() }, tk::Type::body, content);

        if (busInactive)
        {
            // Hollow ring after the label: "no input" (ui-spec 9).
            g.setColour (content);
            g.drawEllipse ((float) (tx + labelW + tk::space::s4) + 0.5f,
                           (float) (getHeight() - L::settingsDotSize) * 0.5f + 0.5f,
                           (float) L::settingsDotSize - 1.0f, (float) L::settingsDotSize - 1.0f, tk::stroke::hairline);
        }
    }

    if (showDot)
    {
        g.setColour (tk::colour::accent);
        g.fillEllipse ((float) (getWidth() - L::settingsDotInset - L::settingsDotSize), (float) L::settingsDotInset,
                       (float) L::settingsDotSize, (float) L::settingsDotSize);
    }

    if (keyboardFocusRing)
    {
        g.setColour (tk::colour::accent);
        g.drawRoundedRectangle (bounds.reduced (tk::stroke::focus * 0.5f), tk::radius::control, tk::stroke::focus);
    }
}

//==============================================================================
void HeaderButton::trigger()
{
    if (onClick != nullptr)
        onClick();
}

void HeaderButton::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void HeaderButton::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void HeaderButton::mouseDown (const juce::MouseEvent& e)
{
    pressed = true;
    repaint();

    if (e.mods.isAltDown() && onAltClick != nullptr)
    {
        onAltClick();
        pressed = false;
        return;
    }

    if (kind == Kind::toggle)
        trigger();
}

void HeaderButton::mouseUp (const juce::MouseEvent& e)
{
    const bool wasPressed = pressed;
    pressed = false;
    repaint();

    if (kind == Kind::action && wasPressed && getLocalBounds().contains (e.getPosition()) && ! e.mods.isAltDown())
        trigger();
}

void HeaderButton::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick != nullptr)
        onDoubleClick();
}

void HeaderButton::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (onWheel != nullptr && wheel.deltaY != 0.0f)
        onWheel (wheel.deltaY > 0.0f ? 1 : -1);
}

bool HeaderButton::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        trigger();
        return true;
    }

    return false;
}

void HeaderButton::focusGained (FocusChangeType cause)
{
    keyboardFocusRing = cause == focusChangedByTabKey;
    repaint();
}

void HeaderButton::focusLost (FocusChangeType)
{
    keyboardFocusRing = false;
    repaint();
}

juce::String HeaderButton::getTooltip()
{
    return tooltipSource != nullptr ? tooltipSource() : juce::String();
}

std::unique_ptr<juce::AccessibilityHandler> HeaderButton::createAccessibilityHandler()
{
    return std::make_unique<Handler> (*this);
}

} // namespace investigate
