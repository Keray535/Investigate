#include "SpectrumEngine.h"

#include <algorithm>

namespace investigate
{

SpectrumEngine::SpectrumEngine() = default;
SpectrumEngine::~SpectrumEngine() = default;

void SpectrumEngine::prepare (double sampleRate, int maxBlockSize, int numMainChannels, int numSidechainChannels)
{
    juce::ignoreUnused (maxBlockSize);

    numMainChannels = juce::jlimit (1, 2, numMainChannels);
    numSidechainChannels = juce::jlimit (0, 2, numSidechainChannels);
    preparedNumSidechainChannels = numSidechainChannels;

    mainFifo.prepare (numMainChannels, kFifoCapacityFrames);

    // SampleFifo::prepare() asserts numChannels > 0, and an inactive
    // sidechain (0 channels) should behave as a safe no-op everywhere
    // (SampleFifo::push()/pop() already no-op when numChannelsPrepared ==
    // 0, their default-constructed state) - so simply leave it unprepared
    // rather than preparing it with a channel count of zero.
    if (numSidechainChannels > 0)
        sidechainFifo.prepare (numSidechainChannels, kFifoCapacityFrames);

    for (auto& b : mainPopBuffers)
        b.assign (static_cast<size_t> (kDrainChunkFrames), 0.0f);
    for (auto& b : sidechainPopBuffers)
        b.assign (static_cast<size_t> (kDrainChunkFrames), 0.0f);

    for (int c = 0; c < 2; ++c)
    {
        mainPopPtrs[static_cast<size_t> (c)] = mainPopBuffers[static_cast<size_t> (c)].data();
        sidechainPopPtrs[static_cast<size_t> (c)] = sidechainPopBuffers[static_cast<size_t> (c)].data();
    }

    SpectrumAnalyser::Settings initial;
    initial.sampleRate = sampleRate;
    initial.numMainChannels = numMainChannels;
    initial.numSidechainChannels = numSidechainChannels;
    analyser.prepare (initial);

    mapper.prepare();
    exchange.prepare();

    frameSequence = 0;
}

void SpectrumEngine::pushMain (const float* const* channels, int numChannels, int numSamples) noexcept
{
    mainFifo.push (channels, numChannels, numSamples);
}

void SpectrumEngine::pushSidechain (const float* const* channels, int numChannels, int numSamples) noexcept
{
    sidechainFifo.push (channels, numChannels, numSamples);
}

bool SpectrumEngine::processPending (const SpectrumSettings& settings) noexcept
{
    analyser.setSettings (settings.toAnalyserSettings());

    const bool sidechainActive = settings.numSidechainChannels > 0 && preparedNumSidechainChannels > 0;
    bool producedAny = false;

    // Bounded defensively: the FIFO capacity is fixed, so the number of
    // chunk-sized pops needed to drain it fully is bounded too. This is not
    // a hard real-time path (the analysis thread, not processBlock), but a
    // finite bound is still cheap insurance against a logic error turning
    // into a stuck thread.
    const int maxIterations = (kFifoCapacityFrames / kDrainChunkFrames) + 2;

    for (int iter = 0; iter < maxIterations; ++iter)
    {
        const int mainReady = mainFifo.getNumReady();
        if (mainReady <= 0)
            break;

        const int toPop = juce::jmin (mainReady, kDrainChunkFrames);
        const int mainPopped = mainFifo.pop (mainPopPtrs.data(), settings.numMainChannels, toPop);
        if (mainPopped <= 0)
            break;

        const float* const* scPtrs = nullptr;

        if (sidechainActive)
        {
            // Zero-pad first: main and sidechain are normally pushed with
            // matching sample counts every processBlock() call, so this
            // only matters if the two FIFOs ever desynchronise (e.g. a
            // dropped-block overflow on one side but not the other).
            for (auto& b : sidechainPopBuffers)
                std::fill (b.begin(), b.begin() + mainPopped, 0.0f);
            sidechainFifo.pop (sidechainPopPtrs.data(), settings.numSidechainChannels, mainPopped);
            scPtrs = sidechainPopPtrs.data();
        }

        const int64_t framesBefore = analyser.getFramesProduced();
        analyser.pushBlock (mainPopPtrs.data(), scPtrs, mainPopped);
        if (analyser.getFramesProduced() != framesBefore)
            producedAny = true;
    }

    if (! producedAny)
        return false;

    publishFrame (settings);
    return true;
}

void SpectrumEngine::publishFrame (const SpectrumSettings& settings) noexcept
{
    SpectrumFrame& frame = exchange.getWriteFrame();

    const int numPoints = juce::jlimit (1, SpectrumFrame::maxPoints, settings.numPoints);
    frame.numPoints = numPoints;
    mapper.computeFrequencies (settings, frame.freqHz.data(), numPoints);

    const int numBins = analyser.getNumBins();
    const double binHz = settings.sampleRate / static_cast<double> (settings.fftSize);

    mapper.mapCurve (analyser.getPrimaryPower(), numBins, binHz, settings, frame.freqHz.data(), numPoints, frame.primaryDb.data());

    frame.isDual = analyser.isDual();
    if (frame.isDual)
        mapper.mapCurve (analyser.getPrimaryBPower(), numBins, binHz, settings, frame.freqHz.data(), numPoints, frame.primaryBDb.data());

    frame.hasSecondary = settings.secondary != SecondaryMode::off;
    if (frame.hasSecondary)
    {
        mapper.mapCurve (analyser.getSecondaryPower (false), numBins, binHz, settings, frame.freqHz.data(), numPoints, frame.secondaryDb.data());
        if (frame.isDual)
            mapper.mapCurve (analyser.getSecondaryPower (true), numBins, binHz, settings, frame.freqHz.data(), numPoints, frame.secondaryBDb.data());
    }

    frame.hasSidechain = analyser.hasSidechain();
    if (frame.hasSidechain)
        mapper.mapCurve (analyser.getSidechainPower(), numBins, binHz, settings, frame.freqHz.data(), numPoints, frame.sidechainDb.data());

    frame.sampleRate = settings.sampleRate;
    frame.sequence = ++frameSequence;

    exchange.publish();
}

} // namespace investigate
