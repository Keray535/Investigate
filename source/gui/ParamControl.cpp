#include "ParamControl.h"

#include "TooltipText.h"

#include "../params/ParameterIDs.h"

#include <cmath>

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

namespace
{
    constexpr int timerIntervalMs = 25;

    juce::uint32 deadlineIn (int ms) noexcept
    {
        const auto d = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
        return d == 0 ? 1u : d;
    }

    bool hasPassed (juce::uint32 deadline, juce::uint32 now) noexcept
    {
        return (juce::int32) (now - deadline) >= 0;
    }

    bool isChoiceLike (const juce::RangedAudioParameter& p) noexcept
    {
        return dynamic_cast<const juce::AudioParameterChoice*> (&p) != nullptr
            || dynamic_cast<const juce::AudioParameterBool*> (&p) != nullptr;
    }
}

//==============================================================================
class ParamControl::EntryEditor final : public juce::TextEditor
{
public:
    EntryEditor() = default;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EntryEditor)
};

class ParamControl::AccessibilityValue final : public juce::AccessibilityTextValueInterface
{
public:
    explicit AccessibilityValue (ParamControl& o) : owner (o) {}

    bool isReadOnly() const override { return false; }
    juce::String getCurrentValueAsString() const override { return owner.getDisplayString(); }

    void setValueAsString (const juce::String& newValue) override
    {
        if (! owner.isChoice)
        {
            owner.commitText (newValue);
            return;
        }

        const auto& range = owner.param.getNormalisableRange();
        const int count = (int) std::lround (range.end - range.start) + 1;

        for (int i = 0; i < count; ++i)
            if (owner.param.getText (owner.param.convertTo0to1 ((float) i), 64).equalsIgnoreCase (newValue))
                owner.applyComplete ((float) i);
    }

private:
    ParamControl& owner;
};

//==============================================================================
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                            Style styleToUse, const juce::String& labelText, const juce::String& accessibleName,
                            DisplayContext* displayContext)
    : id (paramID),
      param (*apvts.getParameter (paramID)),
      style (styleToUse),
      label (labelText),
      context (displayContext),
      isChoice (isChoiceLike (*apvts.getParameter (paramID))),
      behaviour (ControlMath::behaviourFor (paramID, isChoiceLike (*apvts.getParameter (paramID)))),
      attachment (*apvts.getParameter (paramID), [this] (float v) { handleParameterChanged (v); }, nullptr)
{
    setTitle (accessibleName);
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    // The initial value is not an "external change": no automation dot.
    const juce::ScopedValueSetter<bool> guard (localChange, true);
    attachment.sendInitialUpdate();
}

ParamControl::~ParamControl()
{
    stopTimer();

    if (entry != nullptr)
    {
        // The editor's focus-lost callback must not run into a half-destroyed
        // control.
        entry->onReturnKey = nullptr;
        entry->onEscapeKey = nullptr;
        entry->onFocusLost = nullptr;
    }

    if (gestureOpen)
        attachment.endGesture();
}

void ParamControl::setTooltipSource (std::function<juce::String()> firstLine)
{
    tooltipSource = std::move (firstLine);
}

//==============================================================================
void ParamControl::handleParameterChanged (float newValue)
{
    value = newValue;

    if (! localChange)
    {
        dotUntil = deadlineIn (tk::motion::externalChangeDotMs);
        updateTimerState();

        if (onExternalChange != nullptr)
            onExternalChange();
    }

    repaint();

    if (onValueChanged != nullptr)
        onValueChanged();
}

void ParamControl::applyComplete (float newValue)
{
    const juce::ScopedValueSetter<bool> guard (localChange, true);
    attachment.setValueAsCompleteGesture (newValue);
}

void ParamControl::applyPartOfGesture (float newValue)
{
    const juce::ScopedValueSetter<bool> guard (localChange, true);
    attachment.setValueAsPartOfGesture (newValue);
}

void ParamControl::resetToDefault()
{
    applyComplete (param.convertFrom0to1 (param.getDefaultValue()));
}

void ParamControl::stepBy (int direction, bool fine, int steps)
{
    const auto next = ControlMath::applyStep (param.getNormalisableRange(), behaviour, value, direction, fine, steps);

    if (next != value)
        applyComplete (next);
}

//==============================================================================
ValueFormat::ChipText ParamControl::getChipText() const
{
    if (id == ParamIDs::dbLow || id == ParamIDs::dbHigh)
    {
        // Built from the numeric value, not getText(), so the Level chip's
        // two halves can share one "dB" unit (T-008 open question 2).
        const bool unitless = style == Style::half && id == ParamIDs::dbLow;
        return { ValueFormat::formatSignedDb (value), unitless ? juce::String() : juce::String ("dB") };
    }

    return ValueFormat::splitChipText (id, param.getText (param.convertTo0to1 (value), 64));
}

juce::String ParamControl::getDisplayString() const
{
    const auto t = getChipText();
    return t.unit.isEmpty() ? t.value : t.value + " " + t.unit;
}

tokens::Type ParamControl::getValueType() const noexcept
{
    return isChoice ? tk::Type::choice : tk::Type::value;
}

juce::Colour ParamControl::getValueColour() const
{
    if (dragging || entry != nullptr)
        return tk::colour::accent;

    if (context != nullptr && ValueFormat::isClamped (context->effective(), id))
        return tk::colour::stateWarn;

    return tk::colour::textPrimary;
}

juce::Rectangle<int> ParamControl::getValueRect() const
{
    const int w = getWidth();

    switch (style)
    {
        case Style::stacked:    return { L::chipPadX, L::chipValueY, w - 2 * L::chipPadX, L::chipValueH };
        case Style::inlineChip: return { L::chipPadX, 0, w - 2 * L::chipPadX, getHeight() };
        case Style::field:      return { L::fieldPadX, 0, w - 2 * L::fieldPadX, getHeight() };
        case Style::half:       return { textInset, L::chipValueY, w - textInset, L::chipValueH };
    }

    return getLocalBounds();
}

//==============================================================================
void ParamControl::paint (juce::Graphics& g)
{
    auto& laf = InvestigateLookAndFeel::from (*this);
    const auto bounds = getLocalBounds().toFloat();
    const bool active = dragging || entry != nullptr;
    const auto now = juce::Time::getMillisecondCounter();

    if (style != Style::half)
    {
        g.setColour (active || hovering ? tk::colour::surface3 : tk::colour::surface2);
        g.fillRoundedRectangle (bounds, tk::radius::control);

        if (active)
        {
            g.setColour (tk::colour::accent);
            g.drawRoundedRectangle (bounds.reduced (0.5f), tk::radius::control, tk::stroke::hairline);
        }

        if (label.isNotEmpty() && style == Style::stacked)
            laf.drawText (g, label, { L::chipPadX, L::chipPadTop, getWidth() - 2 * L::chipPadX, L::chipLabelH },
                          tk::Type::caps, tk::colour::textSecondary);
        else if (label.isNotEmpty() && style == Style::inlineChip)
            laf.drawText (g, label, { L::chipPadX, 0, getWidth() - 2 * L::chipPadX, getHeight() },
                          tk::Type::caps, tk::colour::textSecondary);
    }

    const auto text = getChipText();
    const auto type = getValueType();
    const auto rect = getValueRect();
    const int valueW = (int) std::ceil (laf.getTextWidth (text.value, type));
    const int unitW = text.unit.isNotEmpty() ? (int) std::ceil (laf.getTextWidth (text.unit, tk::Type::unit)) : 0;
    const int total = valueW + (unitW > 0 ? L::unitGap + unitW : 0);
    const int x0 = style == Style::inlineChip ? rect.getRight() - total : rect.getX();

    if (style == Style::half && (hovering || active))
    {
        g.setColour (tk::colour::surface3);
        g.fillRoundedRectangle (juce::Rectangle<int> (x0 - L::chipInset, rect.getY() - L::halfHighlightPadY,
                                                       total + 2 * L::chipInset, rect.getHeight() + 2 * L::halfHighlightPadY).toFloat(),
                                tk::radius::small);
    }

    laf.drawText (g, text.value, { x0, rect.getY(), valueW + 2, rect.getHeight() }, type, getValueColour());

    if (unitW > 0)
        laf.drawText (g, text.unit, { x0 + valueW + L::unitGap, rect.getY(), unitW + 2, rect.getHeight() },
                      tk::Type::unit, tk::colour::textSecondary);

    if (keyboardFocusRing)
    {
        g.setColour (tk::colour::accent);

        if (style == Style::half)
            g.drawRoundedRectangle (juce::Rectangle<int> (x0 - L::chipInset, rect.getY() - L::halfHighlightPadY,
                                                           total + 2 * L::chipInset, rect.getHeight() + 2 * L::halfHighlightPadY).toFloat(),
                                    tk::radius::small, tk::stroke::focus);
        else
            g.drawRoundedRectangle (bounds.reduced (tk::stroke::focus * 0.5f), tk::radius::control, tk::stroke::focus);
    }

    if (errorUntil != 0 && ! hasPassed (errorUntil, now))
    {
        g.setColour (tk::colour::stateError);
        g.drawRoundedRectangle (bounds.reduced (tk::stroke::focus * 0.5f), tk::radius::control, tk::stroke::focus);
    }

    if (style != Style::half && dotUntil != 0 && ! hasPassed (dotUntil, now))
    {
        g.setColour (tk::colour::accent);
        g.fillEllipse ((float) (getWidth() - L::automationDotInset - L::automationDotSize), (float) L::automationDotInset,
                       (float) L::automationDotSize, (float) L::automationDotSize);
    }
}

void ParamControl::resized()
{
    if (entry == nullptr)
        return;

    switch (style)
    {
        case Style::stacked:
            entry->setBounds (L::chipInset, L::chipValueY - L::chipInset, getWidth() - 2 * L::chipInset,
                              L::chipValueH + 2 * L::chipInset);
            break;
        case Style::half:
        {
            const int x0 = std::max (tk::space::s2, textInset - tk::space::s6);
            entry->setBounds (x0, L::chipValueY - tk::space::s6, getWidth() - x0 - tk::space::s2,
                              L::chipValueH + 2 * tk::space::s6);
            break;
        }
        default:
            entry->setBounds (getLocalBounds().reduced (tk::space::s2));
            break;
    }
}

//==============================================================================
void ParamControl::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void ParamControl::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void ParamControl::mouseDown (const juce::MouseEvent& e)
{
    clickDeadline = 0;

    if (entry != nullptr)
    {
        closeEntry (true);
        return;
    }

    pressed = true;
    dragging = false;
    altHandled = false;

    // Alt+click resets immediately: the accessibility alternative to a
    // double-click (ui-spec 7.1).
    if (e.mods.isAltDown())
    {
        altHandled = true;
        resetToDefault();
        return;
    }

    dragStartValue = value;
    dragAnchorScreenY = e.getMouseDownScreenPosition().y;
    fineAtAnchor = e.mods.isShiftDown();
}

void ParamControl::mouseDrag (const juce::MouseEvent& e)
{
    if (! pressed || altHandled || entry != nullptr)
        return;

    if (! dragging)
    {
        if (e.getDistanceFromDragStart() <= L::clickSlopPx)
            return;

        dragging = true;
        gestureOpen = true;
        attachment.beginGesture();
        repaint();
    }

    const bool fine = e.mods.isShiftDown();

    // Switching Shift mid-drag re-anchors, so the value does not jump.
    if (fine != fineAtAnchor)
    {
        fineAtAnchor = fine;
        dragStartValue = value;
        dragAnchorScreenY = e.getScreenY();
    }

    const auto dy = (float) (dragAnchorScreenY - e.getScreenY());
    const auto next = ControlMath::applyDrag (param.getNormalisableRange(), behaviour, dragStartValue, dy, fine);

    if (next != value)
        applyPartOfGesture (next);
}

void ParamControl::mouseUp (const juce::MouseEvent& e)
{
    if (! pressed)
        return;

    pressed = false;

    if (dragging)
    {
        dragging = false;

        if (gestureOpen)
        {
            gestureOpen = false;
            attachment.endGesture();
        }

        repaint();
        return;
    }

    if (altHandled || e.getNumberOfClicks() > 1 || ! getLocalBounds().contains (e.getPosition()))
        return;

    // Defer so a double-click can reset without a menu or text field
    // flashing up first (ui-spec 7.1).
    clickDeadline = deadlineIn (tk::motion::clickDeferMs);
    updateTimerState();
}

void ParamControl::mouseDoubleClick (const juce::MouseEvent&)
{
    clickDeadline = 0;
    resetToDefault();
}

void ParamControl::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (entry != nullptr || wheel.deltaY == 0.0f)
        return;

    stepBy (wheel.deltaY > 0.0f ? 1 : -1, e.mods.isShiftDown(), 1);
}

bool ParamControl::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    const bool fine = key.getModifiers().isShiftDown();

    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey)
    {
        stepBy (1, fine, 1);
        return true;
    }

    if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
    {
        stepBy (-1, fine, 1);
        return true;
    }

    if (code == juce::KeyPress::pageUpKey)
    {
        stepBy (1, fine, L::pageSteps);
        return true;
    }

    if (code == juce::KeyPress::pageDownKey)
    {
        stepBy (-1, fine, L::pageSteps);
        return true;
    }

    if (code == juce::KeyPress::returnKey || code == juce::KeyPress::spaceKey)
    {
        performPrimaryAction();
        return true;
    }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        resetToDefault();
        return true;
    }

    return false;
}

void ParamControl::focusGained (FocusChangeType cause)
{
    keyboardFocusRing = cause == focusChangedByTabKey;
    repaint();
}

void ParamControl::focusLost (FocusChangeType)
{
    keyboardFocusRing = false;
    repaint();
}

//==============================================================================
void ParamControl::performPrimaryAction()
{
    if (isChoice)
        showChoiceMenu();
    else
        openEntry();
}

void ParamControl::showChoiceMenu()
{
    const auto& range = param.getNormalisableRange();
    const int count = (int) std::lround (range.end - range.start) + 1;
    const int current = (int) std::lround (value);
    const int defaultIndex = (int) std::lround (param.convertFrom0to1 (param.getDefaultValue()));

    juce::PopupMenu menu;

    for (int i = 0; i < count; ++i)
    {
        juce::PopupMenu::Item item (param.getText (param.convertTo0to1 ((float) i), 64));
        item.itemID = i + 1;
        item.isTicked = i == current;

        if (i == defaultIndex)
            item.shortcutKeyDescription = "default";

        menu.addItem (std::move (item));
    }

    // The menu opens below the control with a 4 px gap, or above it when
    // there is no room (JUCE flips it).
    auto area = getScreenBounds();
    area.setHeight (area.getHeight() + L::menuGap);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withTargetScreenArea (area)
                            .withMinimumWidth (getWidth())
                            .withStandardItemHeight (L::menuItemH),
                        [safe = juce::Component::SafePointer<ParamControl> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->applyComplete ((float) (result - 1));
                        });
}

void ParamControl::openEntry()
{
    if (entry != nullptr)
        return;

    auto& laf = InvestigateLookAndFeel::from (*this);

    entry = std::make_unique<EntryEditor>();
    entry->setMultiLine (false);
    entry->setReturnKeyStartsNewLine (false);
    entry->setScrollbarsShown (false);
    entry->setPopupMenuEnabled (false);
    entry->setSelectAllWhenFocused (true);
    entry->setFont (laf.getFont (getValueType()));
    entry->setJustification (juce::Justification::centredLeft);
    entry->setBorder (juce::BorderSize<int> (0, tk::space::s6, 0, tk::space::s6));
    entry->setText (getDisplayString(), false);
    entry->onReturnKey = [this] { closeEntry (true); };
    entry->onEscapeKey = [this] { closeEntry (false); };
    entry->onFocusLost = [this] { closeEntry (true); };

    addAndMakeVisible (*entry);
    resized();
    entry->grabKeyboardFocus();
    entry->selectAll();
    repaint();
}

void ParamControl::closeEntry (bool commit)
{
    if (entry == nullptr)
        return;

    const auto text = entry->getText();

    entry->onReturnKey = nullptr;
    entry->onEscapeKey = nullptr;
    entry->onFocusLost = nullptr;
    removeChildComponent (entry.get());

    // Deleting a TextEditor from inside its own callback is unsafe, so the
    // retired editor is destroyed on the next message-loop turn.
    auto* retired = entry.release();
    juce::MessageManager::callAsync ([retired] { delete retired; });

    repaint();

    if (commit)
        commitText (text);
}

bool ParamControl::commitText (const juce::String& text)
{
    if (! ValueFormat::isParsableNumericText (text, id == ParamIDs::avgTime))
    {
        flashError();
        return false;
    }

    // The parameter's own text parser handles ASCII "-" and U+2212, ","
    // decimals and unit suffixes; out-of-range values clamp in
    // convertTo0to1, and the range snaps to its step.
    applyComplete (param.convertFrom0to1 (param.getValueForText (text)));
    return true;
}

void ParamControl::flashError()
{
    errorUntil = deadlineIn (tk::motion::errorFlashMs);
    updateTimerState();
    repaint();
}

//==============================================================================
void ParamControl::updateTimerState()
{
    if (clickDeadline != 0 || dotUntil != 0 || errorUntil != 0)
    {
        if (! isTimerRunning())
            startTimer (timerIntervalMs);
    }
    else
    {
        stopTimer();
    }
}

void ParamControl::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();

    if (clickDeadline != 0 && hasPassed (clickDeadline, now))
    {
        clickDeadline = 0;
        performPrimaryAction();
    }

    if (dotUntil != 0 && hasPassed (dotUntil, now))
    {
        dotUntil = 0;
        repaint();
    }

    if (errorUntil != 0 && hasPassed (errorUntil, now))
    {
        errorUntil = 0;
        repaint();
    }

    updateTimerState();
}

//==============================================================================
juce::String ParamControl::getTooltip()
{
    auto first = tooltipSource != nullptr ? tooltipSource() : TooltipText::forParameter (id);

    if (context != nullptr)
    {
        const auto clamp = ValueFormat::clampTooltip (context->effective(), id);

        if (clamp.isNotEmpty())
            first = clamp + " " + first;
    }

    return first + "\n" + (isChoice ? TooltipText::howToChoice() : TooltipText::howToFloat());
}

std::unique_ptr<juce::AccessibilityHandler> ParamControl::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces {
                                                             std::make_unique<AccessibilityValue> (*this) });
}

} // namespace investigate
