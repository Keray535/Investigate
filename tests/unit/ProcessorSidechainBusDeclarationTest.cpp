// T-060: the sidechain bus is declared with a real stereo default layout and
// isActivatedByDefault = false (JUCE asserts "Your default layout cannot be
// disabled" otherwise; see docs/research/T-044-host-platform-constraints.md).
// Verifies: construction in Debug raises no jassert, the bus is inactive by
// default, and enabling it as mono or stereo feeds the sidechain as before.

#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <chrono>
#include <cmath>
#include <thread>

using namespace investigate;


TEST_CASE ("Construction: sidechain bus declared stereo default, inactive by default",
           "[processor][buses][sidechain][T-060]")
{
    InvestigateProcessor processor; // Debug: would jassert on a disabled default layout

    REQUIRE (processor.getBusCount (true) == 2);
    REQUIRE (processor.getBusCount (false) == 1);

    auto* sc = processor.getBus (true, 1);
    REQUIRE (sc != nullptr);
    CHECK (sc->getName() == "Sidechain");
    CHECK_FALSE (sc->isEnabled());
    CHECK_FALSE (sc->isEnabledByDefault());
    CHECK (sc->getDefaultLayout() == juce::AudioChannelSet::stereo());
    CHECK (sc->getLastEnabledLayout() == juce::AudioChannelSet::stereo());
    CHECK (sc->getNumberOfChannels() == 0);

    CHECK (processor.getBus (true, 0)->isEnabled());
    CHECK (processor.getBus (false, 0)->isEnabled());
    CHECK_FALSE (processor.isSidechainActive());

    processor.prepareToPlay (48000.0, 256);
    CHECK_FALSE (processor.isSidechainActive());
    CHECK (processor.getChannelCountOfBus (true, 1) == 0);

    // Not enabled: a process call on a 2-channel buffer must succeed.
    juce::AudioBuffer<float> buffer (2, 256);
    buffer.clear();
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    SpectrumFrame frame;
    // No sidechain was prepared, so no frame may report one.
    for (int i = 0; i < 20; ++i)
    {
        if (processor.getLatestSpectrumFrame (frame))
            CHECK_FALSE (frame.hasSidechain);
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}

TEST_CASE ("Sidechain bus enable: mono and stereo through setBusesLayout / enableAllBuses",
           "[processor][buses][sidechain][T-060]")
{
    SECTION ("setBusesLayout")
    {
        const auto sidechainSet = GENERATE (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());
        INFO ("sidechain = " << sidechainSet.getDescription());

        InvestigateProcessor processor;
        auto layout = processor.getBusesLayout();
        layout.getChannelSet (true, 1) = sidechainSet;
        REQUIRE (processor.setBusesLayout (layout));
        processor.prepareToPlay (48000.0, 256);

        CHECK (processor.isSidechainActive());
        CHECK (processor.getChannelCountOfBus (true, 1) == sidechainSet.size());
        CHECK (processor.getBus (true, 1)->isEnabled());

        // Main bus untouched.
        CHECK (processor.getMainBusNumInputChannels() == 2);
        CHECK (processor.getMainBusNumOutputChannels() == 2);

        // Disable again -> inactive.
        layout.getChannelSet (true, 1) = juce::AudioChannelSet::disabled();
        REQUIRE (processor.setBusesLayout (layout));
        processor.prepareToPlay (48000.0, 256);
        CHECK_FALSE (processor.isSidechainActive());
        CHECK (processor.getChannelCountOfBus (true, 1) == 0);
    }

    SECTION ("enableAllBuses")
    {
        InvestigateProcessor processor;
        REQUIRE (processor.enableAllBuses());
        processor.prepareToPlay (48000.0, 256);
        CHECK (processor.isSidechainActive());
        CHECK (processor.getChannelCountOfBus (true, 1) == 2);
    }
}

TEST_CASE ("Enabled sidechain is fed to the spectrum engine (frame.hasSidechain), mono and stereo",
           "[processor][buses][sidechain][analysis][T-060]")
{
    const auto sidechainSet = GENERATE (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo());
    INFO ("sidechain = " << sidechainSet.getDescription());

    InvestigateProcessor processor;
    auto layout = processor.getBusesLayout();
    layout.getChannelSet (true, 1) = sidechainSet;
    REQUIRE (processor.setBusesLayout (layout));
    processor.prepareToPlay (48000.0, 256);
    REQUIRE (processor.isSidechainActive());

    processor.setAnalysisActive (true);

    const int totalCh = 2 + sidechainSet.size();
    juce::AudioBuffer<float> buffer (totalCh, 256);
    juce::MidiBuffer midi;

    bool sawSidechain = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (10);
    double phase = 0.0;
    while (! sawSidechain && std::chrono::steady_clock::now() < deadline)
    {
        for (int ch = 0; ch < totalCh; ++ch)
            for (int i = 0; i < 256; ++i)
                buffer.setSample (ch, i, 0.25f * static_cast<float> (std::sin (phase + 0.0654 * i)));
        phase += 0.0654 * 256;
        processor.processBlock (buffer, midi);

        SpectrumFrame frame;
        if (processor.getLatestSpectrumFrame (frame) && frame.hasSidechain)
            sawSidechain = true;
        else
            std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }
    processor.setAnalysisActive (false);
    CHECK (sawSidechain);
}

TEST_CASE ("isBusesLayoutSupported accepts disabled/mono/stereo sidechain after the new declaration",
           "[processor][buses][sidechain][T-060]")
{
    InvestigateProcessor processor;
    const juce::AudioChannelSet mains[] { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo() };
    const juce::AudioChannelSet ok[] { juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(),
                                       juce::AudioChannelSet::stereo() };
    for (auto m : mains)
        for (auto sc : ok)
        {
            INFO ("main = " << m.getDescription() << ", sidechain = " << sc.getDescription());
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (m);
            l.inputBuses.add (sc);
            l.outputBuses.add (m);
            CHECK (processor.isBusesLayoutSupported (l));
        }

    juce::AudioProcessor::BusesLayout bad;
    bad.inputBuses.add (juce::AudioChannelSet::stereo());
    bad.inputBuses.add (juce::AudioChannelSet::createLCR());
    bad.outputBuses.add (juce::AudioChannelSet::stereo());
    CHECK_FALSE (processor.isBusesLayoutSupported (bad));
}
