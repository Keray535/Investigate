// T-008 acceptance criterion 2: "for every parameter, getText(getValueForText(s))
// returns the same displayed value for 10 sample values each"
// (docs/tasks/T-008-params-buses-state.md). Exercised through the real
// RangedAudioParameter objects (not the bare ParameterFormatting functions
// directly), because getText()/getValueForText() go through the parameter's
// NormalisableRange as well as the string conversion functions, and that is
// what a host actually calls.

#include "PluginProcessor.h"
#include "params/ParameterFormatting.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace investigate;

namespace
{
    /// Round-trips one non-normalised value through a float parameter's
    /// text conversion and checks the displayed text is stable. getText()
    /// and getValueForText() are private on the concrete subclasses (only
    /// reachable virtually through the juce::AudioProcessorParameter
    /// interface a host actually holds), so this takes the base pointer.
    void checkRoundTrip (juce::AudioParameterFloat& param, float value)
    {
        INFO ("value = " << value);
        auto& base = static_cast<juce::AudioProcessorParameter&> (param);
        const auto norm1 = param.range.convertTo0to1 (value);
        const auto text1 = base.getText (norm1, 0);
        const auto norm2 = base.getValueForText (text1);
        const auto text2 = base.getText (norm2, 0);
        CHECK (text1 == text2);
    }

    void checkRoundTripChoice (juce::AudioParameterChoice& param, int index)
    {
        INFO ("index = " << index);
        auto& base = static_cast<juce::AudioProcessorParameter&> (param);
        const auto norm1 = param.getNormalisableRange().convertTo0to1 ((float) index);
        const auto text1 = base.getText (norm1, 0);
        const auto norm2 = base.getValueForText (text1);
        const auto text2 = base.getText (norm2, 0);
        CHECK (text1 == text2);
    }
}

TEST_CASE ("avgTime text round trip across all display buckets", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::avgTime));

    // 10 values: 0 (Off), <1000 ms bucket, [1000,10000) ms bucket, and 10000 exactly.
    const float values[] { 0.0f, 1.0f, 250.0f, 500.0f, 999.6f, 1000.0f, 1500.0f, 5000.0f, 9999.0f, 10000.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

// T-008 round-2 regression (roadmap Decisions, 2026-09-29): plugin-validator
// found that any positive avgTime strictly inside (0, 0.5) ms rounds down to
// "0 ms" as an integer, which re-parses to exactly 0.0f and then reformats as
// "Off" - not idempotent. The array above's smallest nonzero value is 1.0f,
// which already rounds to "1 ms" and never touched this bucket, so it does
// not actually re-exercise the fixed defect. This case closes that gap
// directly against ParameterFormatting (not just the round-trip helper),
// independent of T-008's own re-verification.
TEST_CASE ("avgTime: values strictly inside (0, 0.5) ms round-trip to Off, not a false nonzero reading", "[params][formatting][T-009]")
{
    using namespace investigate::ParameterFormatting;

    for (float ms : { 0.05f, 0.1f, 0.2f, 0.3f, 0.49f })
    {
        INFO ("ms = " << ms);
        const auto text1 = avgTimeToText (ms, 0);
        CHECK (text1 == "Off");
        const auto reparsed = textToAvgTimeMs (text1);
        CHECK (reparsed == 0.0f);
        const auto text2 = avgTimeToText (reparsed, 0);
        CHECK (text2 == text1);
    }

    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::avgTime));
    for (float v : { 0.05f, 0.1f, 0.2f, 0.3f, 0.49f })
        checkRoundTrip (p, v);
}

TEST_CASE ("rmsWindow text round trip across both display buckets", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::rmsWindow));

    const float values[] { 50.0f, 100.0f, 300.0f, 500.0f, 999.0f, 999.6f, 1000.0f, 1200.0f, 1500.0f, 3000.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

TEST_CASE ("freqLow text round trip across all four frequency-format buckets", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::freqLow));

    // Range is 10 to 1000 Hz, so only the "below 100" and "100 to 999" buckets
    // are reachable; both are covered along with the range edges and default.
    const float values[] { 10.0f, 20.0f, 43.7f, 99.0f, 100.0f, 250.0f, 437.0f, 750.0f, 999.0f, 1000.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

TEST_CASE ("freqHigh text round trip across all four frequency-format buckets", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::freqHigh));

    // Range is 1000 to 40000 Hz: covers the "100-999" boundary at 1000, the
    // "1-9.99k" and ">=10k" buckets, plus the range edges and default.
    const float values[] { 1000.0f, 1250.0f, 4370.0f, 9990.0f, 9999.0f, 10000.0f, 14300.0f, 20000.0f, 35000.0f, 40000.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

TEST_CASE ("slope text round trip across the signed range", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::slope));

    const float values[] { -12.0f, -6.5f, -1.5f, -0.5f, 0.0f, 0.5f, 1.5f, 4.5f, 6.0f, 12.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

TEST_CASE ("dbLow text round trip across the negative range", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::dbLow));

    const float values[] { -150.0f, -140.0f, -120.0f, -100.0f, -96.0f, -90.0f, -60.0f, -45.0f, -31.0f, -30.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

TEST_CASE ("dbHigh text round trip across the signed range including zero", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& p = *dynamic_cast<juce::AudioParameterFloat*> (processor.getApvts().getParameter (ParamIDs::dbHigh));

    const float values[] { -30.0f, -20.0f, -12.0f, -5.0f, 0.0f, 1.0f, 6.0f, 12.0f, 20.0f, 30.0f };
    for (auto v : values)
        checkRoundTrip (p, v);
}

TEST_CASE ("Every choice parameter's text round-trips for every option", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto& apvts = processor.getApvts();

    const char* choiceIds[] = {
        ParamIDs::channelMode, ParamIDs::fftSize, ParamIDs::overlap, ParamIDs::window,
        ParamIDs::smoothing, ParamIDs::secondary, ParamIDs::meterScale
    };

    for (auto* id : choiceIds)
    {
        auto& p = *dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id));
        INFO ("param = " << id);
        for (int i = 0; i < p.choices.size(); ++i)
            checkRoundTripChoice (p, i);
    }
}

TEST_CASE ("showSidechain bool text round trip", "[params][formatting]")
{
    InvestigateProcessor processor;
    auto* p = dynamic_cast<juce::AudioProcessorParameter*> (processor.getApvts().getParameter (ParamIDs::showSidechain));
    REQUIRE (p != nullptr);

    for (bool v : { false, true })
    {
        const auto norm1 = v ? 1.0f : 0.0f;
        const auto text1 = p->getText (norm1, 0);
        const auto norm2 = p->getValueForText (text1);
        const auto text2 = p->getText (norm2, 0);
        CHECK (text1 == text2);
    }
}

//==============================================================================
// Direct checks against the ParameterFormatting functions for the exact
// display strings the ui-spec calls for (docs/design/ui-spec.md section 6 and
// 5.5), independent of the parameter wrapper.

TEST_CASE ("ParameterFormatting produces the exact glyphs the ui-spec requires", "[params][formatting]")
{
    using namespace investigate::ParameterFormatting;

    CHECK (avgTimeToText (0.0f, 0) == "Off");
    CHECK (avgTimeToText (350.0f, 0) == "350 ms");
    CHECK (avgTimeToText (1000.0f, 0) == "1.00 s");
    CHECK (avgTimeToText (10000.0f, 0) == "10.0 s");

    CHECK (rmsWindowToText (300.0f, 0) == "300 ms");
    CHECK (rmsWindowToText (1500.0f, 0) == "1.50 s");

    CHECK (frequencyToText (43.7f, 0) == "43.7 Hz");
    CHECK (frequencyToText (437.0f, 0) == "437 Hz");
    CHECK (frequencyToText (4370.0f, 0) == "4.37 kHz");
    CHECK (frequencyToText (14300.0f, 0) == "14.3 kHz");
    CHECK (frequencyToText (20.0f, 0) == "20.0 Hz");
    CHECK (frequencyToText (20000.0f, 0) == "20.0 kHz");

    CHECK (slopeToText (4.5f, 0) == "+4.5 dB/oct");
    CHECK (slopeToText (-1.5f, 0) == (minusSign + "1.5 dB/oct"));
    CHECK (slopeToText (0.0f, 0) == "0.0 dB/oct");

    CHECK (dbLowToText (-90.0f, 0) == (minusSign + "90 dB"));

    CHECK (dbHighToText (6.0f, 0) == "+6 dB");
    CHECK (dbHighToText (-12.0f, 0) == (minusSign + "12 dB"));
    CHECK (dbHighToText (0.0f, 0) == "0 dB");
}
