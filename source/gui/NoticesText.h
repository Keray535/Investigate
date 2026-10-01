#pragma once

#include <juce_core/juce_core.h>

#include <vector>

/** The About view's "Credits and licenses" content (ui-spec 6.2), built once
    from the notice draft in docs/research/T-023-third-party-notices.md
    section 4 and the license texts compiled into the InvestigateAssets
    binary data. Nothing is retyped in C++ except the short notice lines. */
namespace investigate::NoticesText
{

struct Block
{
    enum class Kind { heading, notice, licenseText };

    Kind kind = Kind::notice;
    juce::String text;
};

/** `juceVersion` is the running JUCE version string, e.g. "JUCE v8.0.15". A
    missing license resource yields "License text missing: <file name>" so a
    test can catch the omission. */
std::vector<Block> build (const juce::String& juceVersion);

/** The same blocks as plain text for tests and for the accessible value. */
juce::String toPlainText (const std::vector<Block>& blocks);

} // namespace investigate::NoticesText
