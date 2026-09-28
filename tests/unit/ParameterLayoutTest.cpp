// T-008 acceptance criterion 1: "Every roadmap parameter exists with the
// exact ID, range, default, skew and labels" (docs/roadmap.md, Architecture >
// Parameter table), cross-checked against the current source/dsp/ enums for
// the four choice parameters that map onto them (docs/tasks/T-008-params-
// buses-state.md).

#include "PluginProcessor.h"
#include "params/ParameterFormatting.h"
#include "params/ParameterIDs.h"

#include "dsp/SpectrumAnalyser.h"
#include "dsp/SpectrumSettings.h"
#include "dsp/WindowFunctions.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using Catch::Matchers::WithinAbs;

namespace
{
    template <typename T>
    T& requireParam (juce::AudioProcessorValueTreeState& apvts, const char* id)
    {
        auto* p = apvts.getParameter (id);
        REQUIRE (p != nullptr);
        auto* cast = dynamic_cast<T*> (p);
        REQUIRE (cast != nullptr);
        return *cast;
    }
}

TEST_CASE ("Parameter layout has exactly the 15 roadmap parameters", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& apvts = processor.getApvts();

    const char* ids[] = {
        ParamIDs::channelMode, ParamIDs::fftSize, ParamIDs::overlap, ParamIDs::window,
        ParamIDs::avgTime, ParamIDs::smoothing, ParamIDs::slope, ParamIDs::secondary,
        ParamIDs::freqLow, ParamIDs::freqHigh, ParamIDs::dbLow, ParamIDs::dbHigh,
        ParamIDs::showSidechain, ParamIDs::meterScale, ParamIDs::rmsWindow
    };

    CHECK (apvts.processor.getParameters().size() == 15);

    for (auto* id : ids)
        CHECK (apvts.getParameter (id) != nullptr);
}

TEST_CASE ("channelMode matches ChannelMode enum order and roadmap defaults", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::channelMode);

    const juce::StringArray expected { "Mid", "Side", "Left", "Right", "L+R", "L/R", "M/S" };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 0); // Mid, roadmap default
    CHECK (static_cast<int> (ChannelMode::mid) == 0);
    CHECK (static_cast<int> (ChannelMode::msDual) == 6);
    CHECK (p.choices.size() == 7);
}

TEST_CASE ("fftSize choices and default match the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::fftSize);

    const juce::StringArray expected { "512", "1024", "2048", "4096", "8192", "16384", "32768", "65536" };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 3); // "4096"
}

TEST_CASE ("overlap uses the multiplication sign and matches the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::overlap);

    const juce::StringArray expected {
        "1" + ParameterFormatting::multiplySign, "2" + ParameterFormatting::multiplySign,
        "4" + ParameterFormatting::multiplySign, "8" + ParameterFormatting::multiplySign
    };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 2); // "4x"

    // The UI spec requires U+00D7, not ASCII 'x' (top matter, section 6).
    CHECK (p.choices[2] == juce::String::fromUTF8 ("4\xC3\x97"));
}

TEST_CASE ("window matches WindowFunctions::Type enum order", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::window);

    const juce::StringArray expected { "Hann", "Blackman", "Hi-Res", "Flat-top" };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 0); // Hann

    CHECK (static_cast<int> (WindowFunctions::Type::hann) == 0);
    CHECK (static_cast<int> (WindowFunctions::Type::blackman) == 1);
    CHECK (static_cast<int> (WindowFunctions::Type::hiRes) == 2);
    CHECK (static_cast<int> (WindowFunctions::Type::flatTop) == 3);
}

TEST_CASE ("avgTime range, default and skew centre match the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::avgTime);

    CHECK_THAT (p.range.start, WithinAbs (0.0f, 1.0e-6f));
    CHECK_THAT (p.range.end, WithinAbs (10000.0f, 1.0e-6f));
    CHECK_THAT (p.get(), WithinAbs (1000.0f, 1.0e-6f));
    // setSkewForCentre(500) means the normalised midpoint (0.5) demaps to 500 ms.
    CHECK_THAT (p.range.convertFrom0to1 (0.5f), WithinAbs (500.0f, 1.0f));
}

TEST_CASE ("smoothing matches SmoothingMode enum order", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::smoothing);

    const juce::StringArray expected { "Off", "1/48 oct", "1/24 oct", "1/12 oct", "1/6 oct", "1/3 oct" };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 3); // 1/12 oct

    CHECK (static_cast<int> (SmoothingMode::off) == 0);
    CHECK (static_cast<int> (SmoothingMode::oct1_48) == 1);
    CHECK (static_cast<int> (SmoothingMode::oct1_24) == 2);
    CHECK (static_cast<int> (SmoothingMode::oct1_12) == 3);
    CHECK (static_cast<int> (SmoothingMode::oct1_6) == 4);
    CHECK (static_cast<int> (SmoothingMode::oct1_3) == 5);
}

TEST_CASE ("slope range, step, default are linear and match the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::slope);

    CHECK_THAT (p.range.start, WithinAbs (-12.0f, 1.0e-6f));
    CHECK_THAT (p.range.end, WithinAbs (12.0f, 1.0e-6f));
    CHECK_THAT (p.range.interval, WithinAbs (0.5f, 1.0e-6f));
    CHECK_THAT (p.range.skew, WithinAbs (1.0f, 1.0e-6f)); // linear
    CHECK_THAT (p.get(), WithinAbs (4.5f, 1.0e-6f));
}

TEST_CASE ("secondary matches SecondaryMode enum order", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::secondary);

    const juce::StringArray expected { "Off", "RT max", "All-time max", "All-time avg" };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 1); // RT max

    CHECK (static_cast<int> (SecondaryMode::off) == 0);
    CHECK (static_cast<int> (SecondaryMode::realTimeMax) == 1);
    CHECK (static_cast<int> (SecondaryMode::allTimeMax) == 2);
    CHECK (static_cast<int> (SecondaryMode::allTimeAverage) == 3);
}

TEST_CASE ("freqLow / freqHigh ranges, defaults and skew centres match the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& low = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::freqLow);
    auto& high = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::freqHigh);

    CHECK_THAT (low.range.start, WithinAbs (10.0f, 1.0e-6f));
    CHECK_THAT (low.range.end, WithinAbs (1000.0f, 1.0e-6f));
    CHECK_THAT (low.get(), WithinAbs (20.0f, 1.0e-6f));
    CHECK_THAT (low.range.convertFrom0to1 (0.5f), WithinAbs (100.0f, 1.0f));

    CHECK_THAT (high.range.start, WithinAbs (1000.0f, 1.0e-6f));
    CHECK_THAT (high.range.end, WithinAbs (40000.0f, 1.0e-6f));
    CHECK_THAT (high.get(), WithinAbs (20000.0f, 1.0e-6f));
    CHECK_THAT (high.range.convertFrom0to1 (0.5f), WithinAbs (6325.0f, 5.0f));
}

TEST_CASE ("dbLow / dbHigh ranges, step and defaults match the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& low = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::dbLow);
    auto& high = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::dbHigh);

    CHECK_THAT (low.range.start, WithinAbs (-150.0f, 1.0e-6f));
    CHECK_THAT (low.range.end, WithinAbs (-30.0f, 1.0e-6f));
    CHECK_THAT (low.range.interval, WithinAbs (1.0f, 1.0e-6f));
    CHECK_THAT (low.range.skew, WithinAbs (1.0f, 1.0e-6f));
    CHECK_THAT (low.get(), WithinAbs (-90.0f, 1.0e-6f));

    CHECK_THAT (high.range.start, WithinAbs (-30.0f, 1.0e-6f));
    CHECK_THAT (high.range.end, WithinAbs (30.0f, 1.0e-6f));
    CHECK_THAT (high.range.interval, WithinAbs (1.0f, 1.0e-6f));
    CHECK_THAT (high.range.skew, WithinAbs (1.0f, 1.0e-6f));
    CHECK_THAT (high.get(), WithinAbs (6.0f, 1.0e-6f));
}

TEST_CASE ("showSidechain is a bool parameter, default on", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterBool> (processor.getApvts(), ParamIDs::showSidechain);
    CHECK (p.get() == true);
}

TEST_CASE ("meterScale matches MeterScale enum order", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterChoice> (processor.getApvts(), ParamIDs::meterScale);

    const juce::StringArray expected { "dBFS", "K-20", "K-14", "K-12" };
    CHECK (p.choices == expected);
    CHECK (p.getIndex() == 0); // dBFS

    CHECK (static_cast<int> (MeterScale::dBFS) == 0);
    CHECK (static_cast<int> (MeterScale::k20) == 1);
    CHECK (static_cast<int> (MeterScale::k14) == 2);
    CHECK (static_cast<int> (MeterScale::k12) == 3);
}

TEST_CASE ("rmsWindow range, default and skew centre match the roadmap", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& p = requireParam<juce::AudioParameterFloat> (processor.getApvts(), ParamIDs::rmsWindow);

    CHECK_THAT (p.range.start, WithinAbs (50.0f, 1.0e-6f));
    CHECK_THAT (p.range.end, WithinAbs (3000.0f, 1.0e-6f));
    CHECK_THAT (p.get(), WithinAbs (300.0f, 1.0e-6f));
    CHECK_THAT (p.range.convertFrom0to1 (0.5f), WithinAbs (300.0f, 1.0f));
}

TEST_CASE ("Every parameter uses version hint 1", "[params][layout]")
{
    InvestigateProcessor processor;
    auto& apvts = processor.getApvts();

    const char* ids[] = {
        ParamIDs::channelMode, ParamIDs::fftSize, ParamIDs::overlap, ParamIDs::window,
        ParamIDs::avgTime, ParamIDs::smoothing, ParamIDs::slope, ParamIDs::secondary,
        ParamIDs::freqLow, ParamIDs::freqHigh, ParamIDs::dbLow, ParamIDs::dbHigh,
        ParamIDs::showSidechain, ParamIDs::meterScale, ParamIDs::rmsWindow
    };

    for (auto* id : ids)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id));
        REQUIRE (p != nullptr);
        CHECK (p->paramID == juce::String (id));
        CHECK (p->getVersionHint() == 1);
    }
}
