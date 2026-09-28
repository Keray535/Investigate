// T-008 acceptance criterion 3: isBusesLayoutSupported accepts exactly
// mono/mono and stereo/stereo main (each with sidechain disabled/mono/
// stereo), and rejects mono-in/stereo-out, 3+ channel layouts, and mismatched
// main in/out (docs/roadmap.md, Architecture > Buses and channel layouts;
// docs/tasks/T-008-params-buses-state.md).

#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

using namespace investigate;

namespace
{
    juce::AudioProcessor::BusesLayout makeLayout (juce::AudioChannelSet mainIn,
                                                   juce::AudioChannelSet mainOut,
                                                   juce::AudioChannelSet sidechain)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (mainIn);
        layout.inputBuses.add (sidechain);
        layout.outputBuses.add (mainOut);
        return layout;
    }
}

TEST_CASE ("Accepted layouts: mono/mono and stereo/stereo main, any sidechain", "[processor][buses]")
{
    InvestigateProcessor processor;

    const juce::AudioChannelSet mains[] { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() };
    const juce::AudioChannelSet sidechains[] {
        juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()
    };

    for (auto main : mains)
    {
        for (auto sc : sidechains)
        {
            INFO ("main = " << main.getDescription() << ", sidechain = " << sc.getDescription());
            CHECK (processor.isBusesLayoutSupported (makeLayout (main, main, sc)));
        }
    }
}

TEST_CASE ("Rejected: mono in, stereo out", "[processor][buses]")
{
    InvestigateProcessor processor;
    const auto layout = makeLayout (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo(),
                                     juce::AudioChannelSet::disabled());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout));
}

TEST_CASE ("Rejected: stereo in, mono out", "[processor][buses]")
{
    InvestigateProcessor processor;
    const auto layout = makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono(),
                                     juce::AudioChannelSet::disabled());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout));
}

TEST_CASE ("Rejected: 3 or more main channels", "[processor][buses]")
{
    InvestigateProcessor processor;

    const auto layout1 = makeLayout (juce::AudioChannelSet::createLCR(), juce::AudioChannelSet::createLCR(),
                                      juce::AudioChannelSet::disabled());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout1));

    const auto layout2 = makeLayout (juce::AudioChannelSet::quadraphonic(), juce::AudioChannelSet::quadraphonic(),
                                      juce::AudioChannelSet::disabled());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout2));

    const auto layout3 = makeLayout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1(),
                                      juce::AudioChannelSet::disabled());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout3));
}

TEST_CASE ("Rejected: 3+ channel sidechain", "[processor][buses]")
{
    InvestigateProcessor processor;
    const auto layout = makeLayout (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo(),
                                     juce::AudioChannelSet::createLCR());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout));
}

TEST_CASE ("Rejected: disabled main output", "[processor][buses]")
{
    InvestigateProcessor processor;
    const auto layout = makeLayout (juce::AudioChannelSet::disabled(), juce::AudioChannelSet::disabled(),
                                     juce::AudioChannelSet::disabled());
    CHECK_FALSE (processor.isBusesLayoutSupported (layout));
}
