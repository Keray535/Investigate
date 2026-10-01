#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#include "gui/ControlBar.h"
#include "gui/DisplayContext.h"
#include "gui/HeaderButton.h"
#include "gui/InvestigateLookAndFeel.h"
#include "gui/MeterPanel.h"
#include "gui/ModeSelector.h"
#include "gui/SettingsPopover.h"
#include "gui/SpectrumView.h"

namespace investigate
{

/**
    Investigate's editor frame (T-011; docs/design/ui-spec.md). It owns the
    LookAndFeel, lays out the header, the spectrum plot, the control bar and
    the meter strip, binds every settings control to the APVTS, and wires the
    Freeze, Reset curves, Reset statistics, Meters and Settings actions.

    Lifecycle (roadmap Threading model): the constructor calls
    `setAnalysisActive (true)` and the destructor `setAnalysisActive (false)`.
    The editor reads and writes only the message-thread-safe processor API;
    it never touches DSP objects. Size and the meter-collapse flag persist in
    the `ViewState` child of the APVTS state.
*/
class InvestigateEditor final : public juce::AudioProcessorEditor
{
public:
    explicit InvestigateEditor (InvestigateProcessor& processorToEdit);
    ~InvestigateEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The Freeze view flag (ui-spec 5.7), shared with the SpectrumView. */
    bool isFrozen() const noexcept { return frozen; }

private:
    void setFrozen (bool shouldBeFrozen);
    void setMetersCollapsed (bool shouldCollapse);
    void togglePopover();
    void closePopover();
    void updateHeaderButtons();
    void pollProcessor();

    InvestigateProcessor& processor;

    // Declared first, so it outlives every component that uses it.
    InvestigateLookAndFeel lookAndFeel;
    DisplayContext displayContext;

    juce::TooltipWindow tooltipWindow;

    ModeSelector modeSelector;
    HeaderButton scButton;
    HeaderButton freezeButton;
    HeaderButton resetCurvesButton;
    HeaderButton metersButton;
    HeaderButton settingsButton;

    SpectrumView spectrumView;
    ControlBar controlBar;
    MeterPanel meterPanel;
    SettingsPopover settingsPopover;
    juce::ResizableCornerComponent cornerResizer;

    bool frozen = false;
    bool metersCollapsed = false;
    juce::String sampleRateText;

    // Declared after the components they drive, so they are destroyed first.
    juce::ParameterAttachment scAttachment;
    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InvestigateEditor)
};

} // namespace investigate
