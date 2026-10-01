#pragma once

#include "LayoutMath.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace investigate
{

class InvestigateProcessor;

/**
    The spectrum plot region (ui-spec 4.1, 5). T-011 provides the final class
    name, the region bounds, the `bg-plot` fill, the data-rect frame and the
    Freeze view flag; T-012 fills in the grid, curves, legend, hover readout,
    toasts and interactions without changing the layout the editor gives it.

    The component covers exactly the plot region (below the header, above the
    control bar). `getDataRect()` is the plotted area in this component's own
    coordinates.
*/
class SpectrumView final : public juce::Component
{
public:
    explicit SpectrumView (InvestigateProcessor& processorToView);
    ~SpectrumView() override;

    /** The Freeze view flag (ui-spec 5.7). It is not a parameter and is not
        persisted. T-012 implements what frozen means for the frames; here it
        only switches the frame colour. */
    void setFrozen (bool shouldBeFrozen);
    bool isFrozen() const noexcept { return frozen; }

    /** The plotted area inside the gutters, in this component's coordinates. */
    juce::Rectangle<int> getDataRect() const noexcept;

    void paint (juce::Graphics&) override;

private:
    [[maybe_unused]] InvestigateProcessor& processor;
    bool frozen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};

} // namespace investigate
