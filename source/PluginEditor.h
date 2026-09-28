#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace investigate
{

/**
    Placeholder editor for the Investigate analyser (T-003). It hosts a
    generic parameter editor at a fixed size. The real UI lands in T-010
    through T-013; this class reverts to gui-engineer ownership once T-003
    is DONE.
*/
class InvestigateEditor final : public juce::AudioProcessorEditor
{
public:
    explicit InvestigateEditor (InvestigateProcessor& processorToEdit);
    ~InvestigateEditor() override = default;

    void resized() override;

private:
    juce::GenericAudioProcessorEditor genericEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InvestigateEditor)
};

} // namespace investigate
