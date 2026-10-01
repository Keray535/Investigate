#pragma once

#include "LayoutMath.h"

#include <juce_data_structures/juce_data_structures.h>

/** Reads and writes the `ViewState` child of the APVTS state (roadmap State
    format; ui-spec 2 Persistence). Message-thread only. Every read tolerates
    a missing child, a missing property or a property of the wrong type, and
    falls back to the T-010 defaults; a size is clamped to the spec limits. A
    write creates the child if a state load removed it. The functions take the
    APVTS root state each time, never a cached child, because
    `replaceState` swaps the whole tree. */
namespace investigate::ViewStateStore
{

inline constexpr const char* viewStateType     = "ViewState";
inline constexpr const char* editorWidthProp   = "editorWidth";
inline constexpr const char* editorHeightProp  = "editorHeight";
inline constexpr const char* metersCollapsedProp = "metersCollapsed";

LayoutMath::EditorSize readEditorSize (const juce::ValueTree& rootState);
void writeEditorSize (juce::ValueTree rootState, LayoutMath::EditorSize size);

bool readMetersCollapsed (const juce::ValueTree& rootState);
void writeMetersCollapsed (juce::ValueTree rootState, bool collapsed);

} // namespace investigate::ViewStateStore
