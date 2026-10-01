#include "ControlBar.h"

#include "TooltipText.h"

#include "../params/ParameterIDs.h"

#include <cmath>

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

namespace
{
    using Style = ParamControl::Style;

    // The control bar's chips sit 8 px below the bar's top edge (the chip row
    // is at H - 48 in editor coordinates, the bar starts at H - 56).
    constexpr int chipY = tk::space::s8;
    constexpr int dividerY = (L::controlBarH - L::dividerH) / 2;
}

ControlBar::ControlBar (juce::AudioProcessorValueTreeState& apvts, DisplayContext& context)
    : block (apvts, ParamIDs::fftSize, Style::stacked, "Block", "Block size", &context),
      overlap (apvts, ParamIDs::overlap, Style::stacked, "Overlap", "Overlap", &context),
      window (apvts, ParamIDs::window, Style::stacked, "Window", "Window", &context),
      avgTime (apvts, ParamIDs::avgTime, Style::stacked, "Avg time", "Average time", &context),
      smoothing (apvts, ParamIDs::smoothing, Style::stacked, "Smooth", "Smoothing", &context),
      slope (apvts, ParamIDs::slope, Style::stacked, "Slope", "Slope", &context),
      secondary (apvts, ParamIDs::secondary, Style::stacked, "2nd curve", "Secondary curve", &context),
      frequency (apvts, ParamIDs::freqLow, ParamIDs::freqHigh, "Frequency", "Frequency low", "Frequency high",
                 L::rangeHalfFreqLow, context),
      level (apvts, ParamIDs::dbLow, ParamIDs::dbHigh, "Level", "Range low", "Range high",
             L::rangeHalfDbLow, context)
{
    // The Block tooltip is dynamic: bin spacing and block time depend on the
    // sample rate (ui-spec 6).
    block.setTooltipSource ([&apvts, &context]
    {
        const int index = (int) std::lround (apvts.getRawParameterValue (ParamIDs::fftSize)->load());
        return TooltipText::blockSize (512 << juce::jlimit (0, 7, index), context.getSampleRateOrDefault());
    });

    for (auto* c : { &block, &overlap, &window, &avgTime, &smoothing, &slope, &secondary })
        addAndMakeVisible (*c);

    addAndMakeVisible (frequency);
    addAndMakeVisible (level);
}

ControlBar::~ControlBar() = default;

void ControlBar::refreshClampState()
{
    frequency.repaint();
    level.repaint();
}

void ControlBar::resized()
{
    layout = LayoutMath::computeControlBarLayout (getWidth());

    const std::array<ParamControl*, 5> groupA { &block, &overlap, &window, &avgTime, &smoothing };
    const std::array<int, 5> widthsA { L::chipWBlock, L::chipWOverlap, L::chipWWindow, L::chipWAvg, L::chipWSmooth };

    for (size_t i = 0; i < groupA.size(); ++i)
        groupA[i]->setBounds (layout.groupAX[i], chipY, widthsA[i], L::chipH);

    slope.setBounds (layout.groupBX[0], chipY, L::chipWSlope, L::chipH);
    secondary.setBounds (layout.groupBX[1], chipY, L::chipWSecondary, L::chipH);
    slope.setVisible (layout.showGroupB);
    secondary.setVisible (layout.showGroupB);

    frequency.setBounds (layout.groupCX[0], chipY, L::chipWFreqRange, L::chipH);
    level.setBounds (layout.groupCX[1], chipY, L::chipWDbRange, L::chipH);
    frequency.setVisible (layout.showGroupC);
    level.setVisible (layout.showGroupC);

    const bool nowHidden = layout.anyHidden();

    if (nowHidden != hiddenGroups)
    {
        hiddenGroups = nowHidden;

        if (onHiddenGroupsChanged != nullptr)
            onHiddenGroupsChanged();
    }

    repaint();
}

void ControlBar::paint (juce::Graphics& g)
{
    g.fillAll (tk::colour::bgApp);

    g.setColour (tk::colour::border);
    g.fillRect (0, 0, getWidth(), 1);

    if (layout.showGroupB)
        g.fillRect (layout.dividerABX, dividerY, 1, L::dividerH);

    if (layout.showGroupC)
        g.fillRect (layout.dividerBCX, dividerY, 1, L::dividerH);
}

} // namespace investigate
