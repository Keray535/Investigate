#include "RangeChip.h"

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

RangeChip::RangeChip (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& lowID, const juce::String& highID,
                      const juce::String& labelText,
                      const juce::String& lowName, const juce::String& highName,
                      int lowHalfWidthToUse, DisplayContext& context)
    : label (labelText),
      lowHalfWidth (lowHalfWidthToUse),
      low (apvts, lowID, ParamControl::Style::half, {}, lowName, &context),
      high (apvts, highID, ParamControl::Style::half, {}, highName, &context)
{
    low.setTextInset (L::chipPadX);
    high.setTextInset (L::rangeSepW / 2);

    low.onExternalChange = [this] { armDot(); };
    high.onExternalChange = [this] { armDot(); };

    addAndMakeVisible (low);
    addAndMakeVisible (high);
}

RangeChip::~RangeChip()
{
    stopTimer();
}

void RangeChip::resized()
{
    // Split at the centre of the "…" separator.
    const int split = L::chipPadX + lowHalfWidth + L::rangeSepW / 2;

    low.setBounds (0, 0, split, getHeight());
    high.setBounds (split, 0, getWidth() - split, getHeight());
}

void RangeChip::paint (juce::Graphics& g)
{
    auto& laf = InvestigateLookAndFeel::from (*this);

    g.setColour (tk::colour::surface2);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), tk::radius::control);

    laf.drawText (g, label, { L::chipPadX, L::chipPadTop, getWidth() - 2 * L::chipPadX, L::chipLabelH },
                  tk::Type::caps, tk::colour::textSecondary);

    laf.drawText (g, juce::String::fromUTF8 ("\xE2\x80\xA6"),
                  { L::chipPadX + lowHalfWidth, L::chipValueY, L::rangeSepW, L::chipValueH },
                  tk::Type::body, tk::colour::textTertiary, juce::Justification::centred);

    if (dotUntil != 0)
    {
        g.setColour (tk::colour::accent);
        g.fillEllipse ((float) (getWidth() - L::automationDotInset - L::automationDotSize), (float) L::automationDotInset,
                       (float) L::automationDotSize, (float) L::automationDotSize);
    }
}

void RangeChip::armDot()
{
    dotUntil = juce::Time::getMillisecondCounter() + (juce::uint32) tk::motion::externalChangeDotMs;

    if (dotUntil == 0)
        dotUntil = 1;

    startTimer (tk::motion::externalChangeDotMs / 4);
    repaint();
}

void RangeChip::timerCallback()
{
    if (dotUntil != 0 && (juce::int32) (juce::Time::getMillisecondCounter() - dotUntil) >= 0)
    {
        dotUntil = 0;
        stopTimer();
        repaint();
    }
}

} // namespace investigate
