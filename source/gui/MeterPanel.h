#pragma once

#include "DisplayContext.h"
#include "HeaderButton.h"
#include "LayoutMath.h"
#include "ParamControl.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace investigate
{

class InvestigateProcessor;

/**
    The meter strip (ui-spec 4.4, 8). T-011 provides the final class name,
    the strip chrome (surface fill, left border), the three controls in the
    strip header (Scale, RMS, Reset statistics) and the layout of the two
    content columns. T-013 fills the bars column and the right column
    (table, loudness, correlation) using `getLayout()`, whose rectangles are
    strip-relative, i.e. in this component's own coordinates.

    The statistics reset calls `InvestigateProcessor::requestStatsReset()`;
    T-013 clears its own held values through `onStatisticsReset`.
*/
class MeterPanel final : public juce::Component
{
public:
    MeterPanel (InvestigateProcessor& processorToView, DisplayContext& context);
    ~MeterPanel() override;

    /** The strip-relative layout of ui-spec 4.4 for the current height. */
    const LayoutMath::MeterStripLayout& getLayout() const noexcept { return layout; }

    /** Fired after the processor was asked to reset the statistics, so the
        meter content can clear its held values (peak-hold ticks, TP hold,
        crest window) at once (ui-spec 6, Actions). */
    std::function<void()> onStatisticsReset;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    InvestigateProcessor& processor;
    LayoutMath::MeterStripLayout layout;

    ParamControl scale;
    ParamControl rms;
    HeaderButton resetButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterPanel)
};

} // namespace investigate
