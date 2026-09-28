// T-008 acceptance criterion 7: setStateInformation never crashes on
// nullptr/0-length data, an empty MemoryBlock, 1 KB of random bytes, a saved
// state with a parameter ID missing, and a state with stateVersion=2 plus an
// unknown extra property; known parameters still load and the rest keep
// their defaults (docs/tasks/T-008-params-buses-state.md; docs/roadmap.md,
// Architecture > State format: "Loading tolerates missing properties ...,
// unknown properties ... and corrupt data").

#include "PluginProcessor.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using Catch::Matchers::WithinAbs;

TEST_CASE ("setStateInformation tolerates nullptr and non-positive size", "[processor][state][robustness]")
{
    InvestigateProcessor processor;
    CHECK_NOTHROW (processor.setStateInformation (nullptr, 0));
    CHECK_NOTHROW (processor.setStateInformation (nullptr, -1));

    int dummy = 0;
    CHECK_NOTHROW (processor.setStateInformation (&dummy, 0));
}

TEST_CASE ("setStateInformation tolerates an empty MemoryBlock", "[processor][state][robustness]")
{
    InvestigateProcessor processor;
    juce::MemoryBlock empty;
    CHECK_NOTHROW (processor.setStateInformation (empty.getData(), (int) empty.getSize()));
}

TEST_CASE ("setStateInformation tolerates 1 KB of random bytes and keeps defaults", "[processor][state][robustness]")
{
    InvestigateProcessor processor;
    auto* slope = dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::slope));
    REQUIRE (slope != nullptr);
    const auto defaultSlope = slope->get();

    juce::Random random (42);
    juce::MemoryBlock junk (1024);
    for (size_t i = 0; i < junk.getSize(); ++i)
        junk[i] = (char) random.nextInt (256);

    CHECK_NOTHROW (processor.setStateInformation (junk.getData(), (int) junk.getSize()));

    // Random bytes practically never parse as valid XML: the state stays at
    // its defaults rather than crashing or corrupting.
    CHECK_THAT (slope->get(), WithinAbs (defaultSlope, 1.0e-6f));
}

TEST_CASE ("setStateInformation tolerates a state missing a parameter ID", "[processor][state][robustness]")
{
    InvestigateProcessor source;
    auto& apvts = source.getApvts();

    // Give every parameter a non-default value so a "kept the default" check
    // is meaningful for the removed one and a "loaded" check is meaningful
    // for the rest.
    auto* slope = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (ParamIDs::slope));
    auto* dbHigh = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (ParamIDs::dbHigh));
    REQUIRE (slope != nullptr);
    REQUIRE (dbHigh != nullptr);
    slope->setValueNotifyingHost (slope->range.convertTo0to1 (-3.0f));
    dbHigh->setValueNotifyingHost (dbHigh->range.convertTo0to1 (18.0f));

    auto state = apvts.copyState();

    // Remove the PARAM child for "slope" before serialising, simulating an
    // older save that predates this parameter.
    for (int i = state.getNumChildren(); --i >= 0;)
    {
        auto child = state.getChild (i);
        if (child.hasType ("PARAM") && child.getProperty ("id").toString() == ParamIDs::slope)
        {
            state.removeChild (i, nullptr);
            break;
        }
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    REQUIRE (xml != nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary (*xml, saved);

    InvestigateProcessor destination;
    auto* destSlope = dynamic_cast<juce::AudioParameterFloat*> (destination.getApvts().getParameter (ParamIDs::slope));
    auto* destDbHigh = dynamic_cast<juce::AudioParameterFloat*> (destination.getApvts().getParameter (ParamIDs::dbHigh));
    const auto slopeDefault = destSlope->get();

    CHECK_NOTHROW (destination.setStateInformation (saved.getData(), (int) saved.getSize()));

    // The parameter with no saved entry keeps its default; every other
    // parameter still loads correctly.
    CHECK_THAT (destSlope->get(), WithinAbs (slopeDefault, 1.0e-6f));
    CHECK_THAT (destDbHigh->get(), WithinAbs (18.0f, 1.0e-6f));
}

TEST_CASE ("setStateInformation tolerates stateVersion=2 with an unknown property", "[processor][state][robustness]")
{
    InvestigateProcessor source;
    auto& apvts = source.getApvts();

    auto* dbLow = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (ParamIDs::dbLow));
    REQUIRE (dbLow != nullptr);
    dbLow->setValueNotifyingHost (dbLow->range.convertTo0to1 (-45.0f));

    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 2, nullptr);
    state.setProperty ("futureUnknownProperty", "some value from a newer build", nullptr);

    // Also add an unknown future child, e.g. a hypothetical PresetState.
    juce::ValueTree futureChild ("FutureState");
    futureChild.setProperty ("someFutureFlag", true, nullptr);
    state.addChild (futureChild, -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    REQUIRE (xml != nullptr);
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary (*xml, saved);

    InvestigateProcessor destination;
    auto* destDbLow = dynamic_cast<juce::AudioParameterFloat*> (destination.getApvts().getParameter (ParamIDs::dbLow));

    CHECK_NOTHROW (destination.setStateInformation (saved.getData(), (int) saved.getSize()));

    // The known parameter loads its saved value despite the unknown extras.
    CHECK_THAT (destDbLow->get(), WithinAbs (-45.0f, 1.0e-6f));

    // The ViewState child still has its three properties (constructor default
    // plus ensureViewStateDefaults() called again from setStateInformation).
    auto viewState = destination.getApvts().state.getChildWithName ("ViewState");
    REQUIRE (viewState.isValid());
    CHECK (viewState.hasProperty ("editorWidth"));
    CHECK (viewState.hasProperty ("editorHeight"));
    CHECK (viewState.hasProperty ("metersCollapsed"));
}

TEST_CASE ("setStateInformation ignores a payload with the wrong root element", "[processor][state][robustness]")
{
    InvestigateProcessor processor;
    auto* slope = dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::slope));
    const auto defaultSlope = slope->get();

    juce::ValueTree wrongRoot ("SomeOtherPluginState");
    wrongRoot.setProperty ("id", "not investigate", nullptr);
    std::unique_ptr<juce::XmlElement> xml (wrongRoot.createXml());
    juce::MemoryBlock saved;
    juce::AudioProcessor::copyXmlToBinary (*xml, saved);

    CHECK_NOTHROW (processor.setStateInformation (saved.getData(), (int) saved.getSize()));
    CHECK_THAT (slope->get(), WithinAbs (defaultSlope, 1.0e-6f));
}
