// T-008 acceptance criterion 6: "randomise every parameter, save, restore
// into a fresh instance, and all normalised values are equal within 1e-6.
// ViewState survives as well" (docs/tasks/T-008-params-buses-state.md;
// docs/roadmap.md, Architecture > State format).

#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using Catch::Matchers::WithinAbs;

TEST_CASE ("State round trip: every parameter survives save/restore within 1e-6", "[processor][state]")
{
    InvestigateProcessor source;
    auto& sourceApvts = source.getApvts();

    juce::Random random (987654321);
    const auto& params = sourceApvts.processor.getParameters();

    // Randomise every parameter's normalised value. What actually reaches
    // getStateInformation() is APVTS's own denormalised copy (synced through
    // AudioProcessorValueTreeState's internal listener, which snaps through
    // each parameter's NormalisableRange - e.g. the 0.5-step range every
    // choice parameter has, or AudioParameterBool's {0,1,1} range even though
    // AudioParameterBool::getValue() itself does not apply that range). So
    // the expected value for the round trip is read back through the same
    // getRawParameterValue()/convertTo0to1 path APVTS uses internally, not
    // the parameter's own (possibly unsnapped) getValue().
    std::vector<float> randomisedValues;
    randomisedValues.reserve ((size_t) params.size());
    for (auto* p : params)
    {
        p->setValueNotifyingHost (random.nextFloat());

        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        REQUIRE (ranged != nullptr);
        const auto* raw = sourceApvts.getRawParameterValue (ranged->paramID);
        REQUIRE (raw != nullptr);
        randomisedValues.push_back (ranged->convertTo0to1 (raw->load()));
    }

    // Also give ViewState non-default values so its round trip is meaningful.
    auto viewState = sourceApvts.state.getChildWithName ("ViewState");
    REQUIRE (viewState.isValid());
    viewState.setProperty ("editorWidth", 1600, nullptr);
    viewState.setProperty ("editorHeight", 900, nullptr);
    viewState.setProperty ("metersCollapsed", true, nullptr);

    juce::MemoryBlock saved;
    source.getStateInformation (saved);
    REQUIRE (saved.getSize() > 0);

    InvestigateProcessor destination;
    destination.setStateInformation (saved.getData(), (int) saved.getSize());

    auto& destApvts = destination.getApvts();
    const auto& destParams = destApvts.processor.getParameters();
    REQUIRE (destParams.size() == params.size());

    for (int i = 0; i < params.size(); ++i)
    {
        INFO ("param index " << i << " id " << dynamic_cast<juce::RangedAudioParameter*> (params[i])->paramID);
        CHECK_THAT (destParams[i]->getValue(), WithinAbs (randomisedValues[(size_t) i], 1.0e-6f));
    }

    auto destViewState = destApvts.state.getChildWithName ("ViewState");
    REQUIRE (destViewState.isValid());
    CHECK ((int) destViewState.getProperty ("editorWidth") == 1600);
    CHECK ((int) destViewState.getProperty ("editorHeight") == 900);
    CHECK ((bool) destViewState.getProperty ("metersCollapsed") == true);
}

TEST_CASE ("State round trip with default (unmodified) state preserves ViewState defaults", "[processor][state]")
{
    InvestigateProcessor source;

    juce::MemoryBlock saved;
    source.getStateInformation (saved);

    InvestigateProcessor destination;
    destination.setStateInformation (saved.getData(), (int) saved.getSize());

    auto destViewState = destination.getApvts().state.getChildWithName ("ViewState");
    REQUIRE (destViewState.isValid());
    CHECK ((int) destViewState.getProperty ("editorWidth") == 1200);
    CHECK ((int) destViewState.getProperty ("editorHeight") == 700);
    CHECK ((bool) destViewState.getProperty ("metersCollapsed") == false);
}

TEST_CASE ("prepare/getStateInformation can be called repeatedly without side effects", "[processor][state]")
{
    InvestigateProcessor processor;
    processor.prepareToPlay (44100.0, 256);
    processor.prepareToPlay (48000.0, 512);
    processor.prepareToPlay (96000.0, 64);

    juce::MemoryBlock first;
    processor.getStateInformation (first);
    juce::MemoryBlock second;
    processor.getStateInformation (second);

    CHECK (first.getSize() == second.getSize());
    CHECK (first == second);
}
