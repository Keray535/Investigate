#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

TEST_CASE ("InvestigateProcessor passes audio through unchanged", "[processor][smoke]")
{
    investigate::InvestigateProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    constexpr int numChannels = 2;
    constexpr int numSamples = 512;

    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    juce::Random random (1234);

    for (int channel = 0; channel < numChannels; ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            buffer.setSample (channel, sample, random.nextFloat() * 2.0f - 1.0f);

    const juce::AudioBuffer<float> inputCopy (buffer);
    juce::MidiBuffer midi;

    processor.processBlock (buffer, midi);

    for (int channel = 0; channel < numChannels; ++channel)
        for (int sample = 0; sample < numSamples; ++sample)
            REQUIRE (buffer.getSample (channel, sample) == inputCopy.getSample (channel, sample));

    processor.releaseResources();
}
