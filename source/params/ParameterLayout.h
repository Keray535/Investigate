#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace investigate
{

/** The single source of Investigate's parameter definitions
    (docs/roadmap.md, Architecture > Parameter table). Every parameter uses
    `juce::ParameterID { id, 1 }` (version hint 1); a future change adds a
    new ID and a higher version hint rather than editing an existing one. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace investigate
