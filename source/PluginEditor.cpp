#include "PluginEditor.h"

namespace investigate
{

InvestigateEditor::InvestigateEditor (InvestigateProcessor& processorToEdit)
    : juce::AudioProcessorEditor (processorToEdit),
      genericEditor (processorToEdit)
{
    addAndMakeVisible (genericEditor);
    setSize (600, 400);
}

void InvestigateEditor::resized()
{
    genericEditor.setBounds (getLocalBounds());
}

} // namespace investigate
