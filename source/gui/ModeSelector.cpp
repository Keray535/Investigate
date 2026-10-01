#include "ModeSelector.h"

#include "TooltipText.h"

#include "../params/ParameterIDs.h"

#include <cmath>

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

namespace
{
    constexpr std::array<const char*, ModeSelector::numModes> segmentLabels { "Mid", "Side", "L", "R", "L+R", "L/R", "M/S" };
    constexpr std::array<int, ModeSelector::numModes> segmentWidths { L::modeSegWMid, L::modeSegWSide, L::modeSegWL,
                                                                      L::modeSegWR, L::modeSegWLR,
                                                                      L::modeSegWDual, L::modeSegWDual };
}

class ModeSelector::AccessibilityValue final : public juce::AccessibilityTextValueInterface
{
public:
    explicit AccessibilityValue (ModeSelector& o) : owner (o) {}

    bool isReadOnly() const override { return false; }
    juce::String getCurrentValueAsString() const override { return segmentLabels[(size_t) owner.selected]; }

    void setValueAsString (const juce::String& newValue) override
    {
        for (int i = 0; i < numModes; ++i)
            if (newValue.equalsIgnoreCase (segmentLabels[(size_t) i]))
                owner.select (i);
    }

private:
    ModeSelector& owner;
};

ModeSelector::ModeSelector (juce::AudioProcessorValueTreeState& apvts)
    : param (*apvts.getParameter (ParamIDs::channelMode)),
      attachment (*apvts.getParameter (ParamIDs::channelMode), [this] (float v) { handleParameterChanged (v); }, nullptr)
{
    setTitle ("Channel mode");
    setWantsKeyboardFocus (true);
    attachment.sendInitialUpdate();
}

ModeSelector::~ModeSelector() = default;

void ModeSelector::handleParameterChanged (float newValue)
{
    const int next = juce::jlimit (0, numModes - 1, (int) std::lround (newValue));

    if (next == selected)
        return;

    selected = next;
    repaint();

    if (onModeChanged != nullptr)
        onModeChanged();
}

void ModeSelector::resized()
{
    // Single group at x 0, dual group right of it (header coordinates 116 and
    // 324, ui-spec 4.2), each in a 2 px padded track.
    const int trackY = 0;
    const int segY = L::modeTrackPad;

    tracks[0] = { 0, trackY, L::modeSingleW, L::headerItemH };
    tracks[1] = { L::modeDualX - L::modeSingleX, trackY, L::modeDualW, L::headerItemH };

    int x = tracks[0].getX() + L::modeTrackPad;

    for (int i = 0; i < 5; ++i)
    {
        segments[(size_t) i] = { x, segY, segmentWidths[(size_t) i], L::modeSegH };
        x += segmentWidths[(size_t) i];
    }

    x = tracks[1].getX() + L::modeTrackPad;

    for (int i = 5; i < numModes; ++i)
    {
        segments[(size_t) i] = { x, segY, segmentWidths[(size_t) i], L::modeSegH };
        x += segmentWidths[(size_t) i];
    }
}

void ModeSelector::paint (juce::Graphics& g)
{
    auto& laf = InvestigateLookAndFeel::from (*this);

    for (const auto& track : tracks)
    {
        g.setColour (tk::colour::surface2);
        g.fillRoundedRectangle (track.toFloat(), tk::radius::control);
    }

    for (int i = 0; i < numModes; ++i)
    {
        const auto seg = segments[(size_t) i];
        const bool isSelected = i == selected;
        const bool isHovered = i == hoveredIndex;

        if (isSelected)
        {
            g.setColour (tk::colour::accent);
            g.fillRoundedRectangle (seg.toFloat(), tk::radius::small);
        }
        else if (isHovered)
        {
            g.setColour (tk::colour::surface3);
            g.fillRoundedRectangle (seg.toFloat(), tk::radius::small);
        }

        laf.drawText (g, segmentLabels[(size_t) i], seg,
                      isSelected ? tk::Type::bodySemi : tk::Type::body,
                      isSelected ? tk::colour::textOnAccent
                                 : (isHovered ? tk::colour::textPrimary : tk::colour::textSecondary),
                      juce::Justification::centred);
    }

    if (keyboardFocusRing)
    {
        g.setColour (tk::colour::accent);

        for (const auto& track : tracks)
            g.drawRoundedRectangle (track.toFloat().reduced (tk::stroke::focus * 0.5f), tk::radius::control, tk::stroke::focus);
    }
}

int ModeSelector::indexAt (juce::Point<int> p) const noexcept
{
    for (int i = 0; i < numModes; ++i)
        if (segments[(size_t) i].contains (p))
            return i;

    return -1;
}

void ModeSelector::select (int index)
{
    const auto clamped = juce::jlimit (0, numModes - 1, index);
    attachment.setValueAsCompleteGesture ((float) clamped);
}

void ModeSelector::resetToDefault()
{
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void ModeSelector::stepBy (int direction)
{
    const int next = juce::jlimit (0, numModes - 1, selected + direction);

    if (next != selected)
        select (next);
}

void ModeSelector::mouseMove (const juce::MouseEvent& e)
{
    const int index = indexAt (e.getPosition());

    if (index != hoveredIndex)
    {
        hoveredIndex = index;
        repaint();
    }
}

void ModeSelector::mouseExit (const juce::MouseEvent&)
{
    hoveredIndex = -1;
    repaint();
}

void ModeSelector::mouseDown (const juce::MouseEvent& e)
{
    const int index = indexAt (e.getPosition());

    if (e.mods.isAltDown())
    {
        resetToDefault();
        return;
    }

    if (index >= 0)
        select (index);
}

void ModeSelector::mouseDoubleClick (const juce::MouseEvent&)
{
    resetToDefault();
}

void ModeSelector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (wheel.deltaY != 0.0f)
        stepBy (wheel.deltaY > 0.0f ? 1 : -1);
}

bool ModeSelector::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();

    if (code == juce::KeyPress::rightKey || code == juce::KeyPress::upKey)   { stepBy (1);  return true; }
    if (code == juce::KeyPress::leftKey  || code == juce::KeyPress::downKey) { stepBy (-1); return true; }

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        resetToDefault();
        return true;
    }

    return false;
}

void ModeSelector::focusGained (FocusChangeType cause)
{
    keyboardFocusRing = cause == focusChangedByTabKey;
    repaint();
}

void ModeSelector::focusLost (FocusChangeType)
{
    keyboardFocusRing = false;
    repaint();
}

juce::String ModeSelector::getTooltip()
{
    const int index = indexAt (getMouseXYRelative());
    return TooltipText::forChannelMode (index >= 0 ? index : selected) + "\n" + TooltipText::channelModeHowTo();
}

std::unique_ptr<juce::AccessibilityHandler> ModeSelector::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this, juce::AccessibilityRole::slider,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces {
                                                             std::make_unique<AccessibilityValue> (*this) });
}

} // namespace investigate
