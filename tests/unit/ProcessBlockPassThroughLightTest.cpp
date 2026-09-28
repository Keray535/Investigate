// T-008 acceptance criterion: processBlock keeps the main buffer untouched
// (docs/roadmap.md, Architecture > Buses and channel layouts; "Pass-through"
// design requirement). This is a light sanity check across a couple of
// layouts and block sizes; the full null-test matrix across block sizes and
// sample rates is plugin-validator's job (T-008 unit-tester brief), not
// unit-tester's.

#include "PluginProcessor.h"

#include <catch2/catch_test_macros.hpp>

using namespace investigate;

namespace
{
    void checkPassThrough (int numChannels, int numSamples, double sampleRate, juce::uint32 seed)
    {
        InvestigateProcessor processor;

        // The processor's default bus layout (its constructor's
        // BusesProperties) is stereo main in/out. getBusBuffer(), which
        // processBlock uses (T-009), returns a sub-buffer sized to the
        // *bus's* current channel count, aliasing pointers out of the
        // AudioBuffer passed to processBlock - it assumes, as every real
        // host guarantees, that the buffer actually has at least that many
        // channels (JUCE's own getBusBuffer() contract). A "mono" scenario
        // must therefore negotiate a real mono layout first, or a 1-channel
        // buffer read through a bus the processor still believes is stereo
        // walks off the end of the buffer's channel-pointer array. This
        // mirrors what a host actually does (query isBusesLayoutSupported /
        // call setBusesLayout before prepareToPlay), and is required for
        // every layout this helper is asked to exercise, not only mono.
        auto layout = processor.getBusesLayout();
        const auto channelSet = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        layout.getChannelSet (true, 0) = channelSet;
        layout.getChannelSet (false, 0) = channelSet;
        REQUIRE (processor.setBusesLayout (layout));

        processor.prepareToPlay (sampleRate, numSamples > 0 ? numSamples : 1);

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        juce::Random random (seed);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < numSamples; ++s)
                buffer.setSample (ch, s, random.nextFloat() * 2.0f - 1.0f);

        const juce::AudioBuffer<float> inputCopy (buffer);
        juce::MidiBuffer midi;

        processor.processBlock (buffer, midi);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int s = 0; s < numSamples; ++s)
                REQUIRE (buffer.getSample (ch, s) == inputCopy.getSample (ch, s));

        processor.releaseResources();
    }
}

TEST_CASE ("processBlock is bit-exact pass-through: mono, 1-sample block", "[processor][passthrough]")
{
    checkPassThrough (1, 1, 44100.0, 1);
}

TEST_CASE ("processBlock is bit-exact pass-through: stereo, 512-sample block", "[processor][passthrough]")
{
    checkPassThrough (2, 512, 48000.0, 2);
}

TEST_CASE ("processBlock is bit-exact pass-through: stereo, 4096-sample block at 96 kHz", "[processor][passthrough]")
{
    checkPassThrough (2, 4096, 96000.0, 3);
}

TEST_CASE ("processBlock is bit-exact pass-through: mono, 64-sample block", "[processor][passthrough]")
{
    checkPassThrough (1, 64, 44100.0, 4);
}

TEST_CASE ("Latency and tail are always zero", "[processor][passthrough]")
{
    InvestigateProcessor processor;
    CHECK (processor.getLatencySamples() == 0);
    CHECK (processor.getTailLengthSeconds() == 0.0);

    processor.prepareToPlay (48000.0, 512);
    CHECK (processor.getLatencySamples() == 0);
    CHECK (processor.getTailLengthSeconds() == 0.0);
}
