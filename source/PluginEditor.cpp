#include "PluginEditor.h"

#include "gui/LayoutMath.h"
#include "gui/TooltipText.h"
#include "gui/ValueFormat.h"
#include "gui/ViewStateStore.h"
#include "params/ParameterIDs.h"

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

namespace
{
    // Header tab order: header controls, then the control bar, then the meter
    // strip, then the popover (ui-spec 7.2).
    constexpr int focusOrderModeSelector = 1;
    constexpr int focusOrderControlBar = 7;
    constexpr int focusOrderMeterPanel = 8;
    constexpr int focusOrderPopover = 9;

    juce::String makeVersionLine (const InvestigateProcessor& processor)
    {
        const auto format = processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone ? "Standalone"
                          : processor.wrapperType == juce::AudioProcessor::wrapperType_VST3       ? "VST3"
                                                                                                  : "Plugin";

        // U+00B7 between the parts, as in ui-spec 6.2.
        const auto dot = juce::String::fromUTF8 (" \xC2\xB7 ");
        return "Version " + juce::String (JucePlugin_VersionString) + dot + format + dot + "64-bit";
    }
}

InvestigateEditor::InvestigateEditor (InvestigateProcessor& processorToEdit)
    : juce::AudioProcessorEditor (processorToEdit),
      processor (processorToEdit),
      displayContext (processorToEdit.getApvts()),
      tooltipWindow (this, tk::motion::tooltipDelayMs),
      modeSelector (processorToEdit.getApvts()),
      scButton (Icon::sidechain, "SC", HeaderButton::Kind::toggle, "Sidechain trace"),
      freezeButton (Icon::freeze, "Freeze", HeaderButton::Kind::toggle, "Freeze display"),
      resetCurvesButton (Icon::reset, "Reset curves", HeaderButton::Kind::action, "Reset spectrum curves"),
      metersButton (Icon::meters, {}, HeaderButton::Kind::toggle, "Show meters"),
      settingsButton (Icon::settings, {}, HeaderButton::Kind::action, "Settings"),
      spectrumView (processorToEdit, displayContext),
      controlBar (processorToEdit.getApvts(), displayContext),
      meterPanel (processorToEdit, displayContext),
      settingsPopover (processorToEdit.getApvts(), displayContext, makeVersionLine (processorToEdit)),
      cornerResizer (this, getConstrainer()),
      scAttachment (*processorToEdit.getApvts().getParameter (ParamIDs::showSidechain),
                    [this] (float v) { scButton.setToggled (v >= 0.5f); }, nullptr),
      vblank (this, [this] { pollProcessor(); })
{
    setLookAndFeel (&lookAndFeel);
    setTitle ("Investigate");
    setOpaque (true);

    // The size and the collapse flag come from ViewState, tolerating missing
    // or corrupt properties (ui-spec 2).
    const auto& apvts = processorToEdit.getApvts();
    const auto size = ViewStateStore::readEditorSize (apvts.state);
    metersCollapsed = ViewStateStore::readMetersCollapsed (apvts.state);

    setResizeLimits (L::minWidth, L::minHeight, L::maxWidth, L::maxHeight);
    setResizable (true, false);

    // Header
    modeSelector.onModeChanged = [this] { setFrozen (false); };

    scButton.setTooltipSource ([this]
    {
        return TooltipText::sidechain (processor.isSidechainActive()) + "\nClick to toggle. Double-click to reset (on).";
    });
    scButton.onClick = [this] { scAttachment.setValueAsCompleteGesture (scButton.isToggled() ? 0.0f : 1.0f); };
    scButton.onDoubleClick = [this] { scAttachment.setValueAsCompleteGesture (1.0f); };
    scButton.onAltClick = [this] { scAttachment.setValueAsCompleteGesture (1.0f); };
    scButton.onWheel = [this] (int direction) { scAttachment.setValueAsCompleteGesture (direction > 0 ? 1.0f : 0.0f); };

    freezeButton.setLook (HeaderButton::Look::frozen);
    freezeButton.setTooltipSource ([] { return TooltipText::freeze(); });
    freezeButton.onClick = [this] { setFrozen (! frozen); };

    resetCurvesButton.setTooltipSource ([] { return TooltipText::resetCurves(); });
    resetCurvesButton.onClick = [this] { processor.requestSpectrumReset(); };

    metersButton.setTooltipSource ([this] { return TooltipText::metersToggle (metersCollapsed); });
    metersButton.onClick = [this] { setMetersCollapsed (! metersCollapsed); };

    settingsButton.setTooltipSource ([this] { return TooltipText::settings (controlBar.hasHiddenGroups()); });
    settingsButton.onClick = [this] { togglePopover(); };

    controlBar.onHiddenGroupsChanged = [this] { settingsButton.setShowDot (controlBar.hasHiddenGroups()); };

    settingsPopover.setIgnoredComponent (&settingsButton);
    settingsPopover.onCloseRequested = [this] { closePopover(); };

    modeSelector.setExplicitFocusOrder (focusOrderModeSelector);
    scButton.setExplicitFocusOrder (focusOrderModeSelector + 1);
    freezeButton.setExplicitFocusOrder (focusOrderModeSelector + 2);
    resetCurvesButton.setExplicitFocusOrder (focusOrderModeSelector + 3);
    metersButton.setExplicitFocusOrder (focusOrderModeSelector + 4);
    settingsButton.setExplicitFocusOrder (focusOrderModeSelector + 5);
    controlBar.setExplicitFocusOrder (focusOrderControlBar);
    meterPanel.setExplicitFocusOrder (focusOrderMeterPanel);
    settingsPopover.setExplicitFocusOrder (focusOrderPopover);

    for (auto* c : std::initializer_list<juce::Component*> { &modeSelector, &scButton, &freezeButton, &resetCurvesButton,
                                                              &metersButton, &settingsButton, &spectrumView,
                                                              &controlBar, &meterPanel, &settingsPopover })
        addAndMakeVisible (*c);

    settingsPopover.setVisible (false);
    addAndMakeVisible (cornerResizer);

    updateHeaderButtons();
    scAttachment.sendInitialUpdate();

    // Analysis runs only while an editor is open (roadmap Threading model).
    processor.setAnalysisActive (true);

    setSize (size.width, size.height);
}

InvestigateEditor::~InvestigateEditor()
{
    processor.setAnalysisActive (false);
    setLookAndFeel (nullptr);
}

//==============================================================================
void InvestigateEditor::updateHeaderButtons()
{
    freezeButton.setToggled (frozen);
    metersButton.setToggled (! metersCollapsed);
    metersButton.setMirrored (metersCollapsed);
}

void InvestigateEditor::setFrozen (bool shouldBeFrozen)
{
    if (frozen == shouldBeFrozen)
        return;

    frozen = shouldBeFrozen;
    spectrumView.setFrozen (frozen);
    freezeButton.setToggled (frozen);
}

void InvestigateEditor::setMetersCollapsed (bool shouldCollapse)
{
    if (metersCollapsed == shouldCollapse)
        return;

    metersCollapsed = shouldCollapse;
    ViewStateStore::writeMetersCollapsed (processor.getApvts().state, metersCollapsed);
    updateHeaderButtons();

    // The editor size does not change; the plot and control bar take the
    // freed width (ui-spec 4.5).
    resized();
}

void InvestigateEditor::togglePopover()
{
    if (settingsPopover.isOpen())
        closePopover();
    else
        settingsPopover.open (getWidth());
}

void InvestigateEditor::closePopover()
{
    settingsPopover.close();
    settingsButton.grabKeyboardFocus();
}

void InvestigateEditor::pollProcessor()
{
    // The snapshot copy is lock-free; only its sample rate is needed here.
    // The spectrum view (T-012) and, later, the meter panel (T-013) pull their
    // own data from this same vblank callback.
    const auto sampleRate = processor.getMeterSnapshot().sampleRate;

    if (sampleRate > 0.0 && sampleRate != displayContext.getSampleRate())
    {
        displayContext.setSampleRate (sampleRate);

        const auto text = ValueFormat::formatSampleRate (sampleRate);

        if (text != sampleRateText)
        {
            sampleRateText = text;
            repaint (getWidth() - L::sampleRateFromRight, L::headerItemY, L::sampleRateW, L::headerItemH);
        }
    }

    if (displayContext.refresh())
    {
        controlBar.refreshClampState();
        settingsPopover.refreshClampState();
    }

    scButton.setBusInactive (! processor.isSidechainActive());

    // After the context refresh, so the view maps frames with the current
    // range and sample rate. One vblank callback serves the whole editor.
    spectrumView.pollFrame();
}

//==============================================================================
void InvestigateEditor::paint (juce::Graphics& g)
{
    g.fillAll (tk::colour::bgApp);

    // Header: 1 px border along its bottom edge.
    g.setColour (tk::colour::border);
    g.fillRect (0, L::headerH - 1, getWidth(), 1);
    g.fillRect (getWidth() - L::headerDividerFromRight, L::headerDividerY, 1, L::headerDividerH);

    if (const auto* wordmark = lookAndFeel.getIcon (Icon::logoWordmark, tk::colour::textPrimary, false))
        wordmark->drawWithin (g, { (float) L::wordmarkX, (float) L::wordmarkY, (float) L::wordmarkW, (float) L::wordmarkH },
                              juce::RectanglePlacement::centred, 1.0f);

    lookAndFeel.drawText (g, sampleRateText,
                          { getWidth() - L::sampleRateFromRight, L::headerItemY, L::sampleRateW, L::headerItemH },
                          tk::Type::axis, tk::colour::textTertiary, juce::Justification::centredRight);
}

void InvestigateEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();
    const auto regions = LayoutMath::computeRegions (w, h, metersCollapsed);

    spectrumView.setBounds (regions.plot);
    controlBar.setBounds (regions.controlBar);
    meterPanel.setVisible (! metersCollapsed);

    if (! metersCollapsed)
        meterPanel.setBounds (regions.meterStrip);

    modeSelector.setBounds (L::modeSingleX, L::headerItemY, L::modeDualX - L::modeSingleX + L::modeDualW, L::headerItemH);
    scButton.setBounds (w - L::scFromRight, L::headerItemY, L::scW, L::headerItemH);
    freezeButton.setBounds (w - L::freezeFromRight, L::headerItemY, L::freezeW, L::headerItemH);
    resetCurvesButton.setBounds (w - L::resetCurvesFromRight, L::headerItemY, L::resetCurvesW, L::headerItemH);
    metersButton.setBounds (w - L::metersBtnFromRight, L::headerItemY, L::iconBtnW, L::headerItemH);
    settingsButton.setBounds (w - L::settingsBtnFromRight, L::headerItemY, L::iconBtnW, L::headerItemH);

    cornerResizer.setBounds (w - L::resizeGrip, h - L::resizeGrip, L::resizeGrip, L::resizeGrip);
    cornerResizer.toFront (false);

    if (settingsPopover.isOpen())
        settingsPopover.updateForEditorWidth (w);

    // Persist on every resize (ui-spec 2).
    ViewStateStore::writeEditorSize (processor.getApvts().state, { w, h });

    repaint (0, 0, w, L::headerH);
}

} // namespace investigate
