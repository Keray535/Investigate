#include "MeterPanel.h"

#include "TooltipText.h"

#include "../PluginProcessor.h"
#include "../params/ParameterIDs.h"

namespace investigate
{

namespace tk = tokens;

MeterPanel::MeterPanel (InvestigateProcessor& processorToView, DisplayContext& context)
    : processor (processorToView),
      scale (processorToView.getApvts(), ParamIDs::meterScale, ParamControl::Style::inlineChip, "Scale", "Meter scale", &context),
      rms (processorToView.getApvts(), ParamIDs::rmsWindow, ParamControl::Style::inlineChip, "RMS", "RMS window", &context),
      resetButton (Icon::reset, "Reset", HeaderButton::Kind::action, "Reset statistics")
{
    setOpaque (true);

    resetButton.setTooltipSource ([] { return TooltipText::resetStatistics(); });
    resetButton.onClick = [this]
    {
        processor.requestStatsReset();

        if (onStatisticsReset != nullptr)
            onStatisticsReset();
    };

    addAndMakeVisible (scale);
    addAndMakeVisible (rms);
    addAndMakeVisible (resetButton);
}

MeterPanel::~MeterPanel() = default;

void MeterPanel::resized()
{
    layout = LayoutMath::computeMeterStripLayout (getHeight());

    scale.setBounds (layout.scaleChip);
    rms.setBounds (layout.rmsChip);
    resetButton.setBounds (layout.resetButton);
}

void MeterPanel::paint (juce::Graphics& g)
{
    g.fillAll (tk::colour::surface);

    // 1 px border on the left edge.
    g.setColour (tk::colour::border);
    g.fillRect (0, 0, 1, getHeight());

    // T-013 placeholder: outline the two content columns so the layout is
    // visible until the bars, table, loudness and correlation blocks exist.
    g.setColour (tk::colour::borderSubtle);
    g.drawRect (layout.barsColumn, 1);
    g.drawRect (layout.rightColumn, 1);
}

} // namespace investigate
