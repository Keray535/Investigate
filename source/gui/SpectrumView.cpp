#include "SpectrumView.h"

#include "DesignTokens.h"

#include "../PluginProcessor.h"

namespace investigate
{

namespace tk = tokens;

SpectrumView::SpectrumView (InvestigateProcessor& processorToView)
    : processor (processorToView)
{
    setOpaque (true);
    setTitle ("Spectrum");
    setDescription ("Spectrum display. Hover for a readout; click to reset the max and average curves; "
                    "right-click to copy the frequency.");
}

SpectrumView::~SpectrumView() = default;

void SpectrumView::setFrozen (bool shouldBeFrozen)
{
    if (frozen == shouldBeFrozen)
        return;

    frozen = shouldBeFrozen;
    repaint();
}

juce::Rectangle<int> SpectrumView::getDataRect() const noexcept
{
    return LayoutMath::computeDataRect (getWidth(), getHeight());
}

void SpectrumView::paint (juce::Graphics& g)
{
    g.fillAll (tk::colour::bgPlot);

    // The data-rect frame: border-subtle, or state-frozen while frozen
    // (ui-spec 4.1, 5.7).
    g.setColour (frozen ? tk::colour::stateFrozen : tk::colour::borderSubtle);
    g.drawRect (getDataRect(), 1);
}

} // namespace investigate
