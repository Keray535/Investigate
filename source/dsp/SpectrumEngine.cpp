#include "SpectrumEngine.h"

#include <algorithm>
#include <cstring>

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

    // The freeze snapshot is sized for the largest FFT (32769 bins x 5
    // curves, about 0.66 MB) so freezing never allocates.
    snapshot.data.assign (static_cast<size_t> (5) * SpectrumAnalyser::maxNumBins, 0.0f);
    snapshot.valid = false;
    snapshot.epoch = 0;
    frozenActive = false;
    published = PublishedState {};

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

    // Freeze entry happens before the drain below: the analyser then still
    // holds exactly the state the last published frame was mapped from, so
    // the snapshot is what the user was looking at when they froze. (A full
    // analyser reset in setSettings() just above leaves no valid state, and
    // takeSnapshot() records that.)
    if (settings.frozen)
    {
        if (! frozenActive || settings.freezeEpoch != snapshot.epoch)
        {
            takeSnapshot (settings);
            frozenActive = true;
        }
    }
    else
    {
        frozenActive = false;
        snapshot.valid = false;
    }

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

    if (settings.frozen)
    {
        // Live frames are not published while frozen; only a snapshot
        // re-map after a view or home-range change (or at freeze entry).
        if (! snapshot.valid || ! differsFromPublished (settings, true, snapshot.epoch))
            return false;

        SpectrumSettings frozenSettings = settings;
        frozenSettings.smoothing = snapshot.smoothing;
        frozenSettings.slopeDbPerOct = snapshot.slopeDbPerOct;

        publishFrame (frozenSettings, snapshotSource(), true, snapshot.epoch);
        return true;
    }

    // Live. A new hop always publishes; without one, publish only when the
    // view, the home range or the freeze state changed and the analyser holds
    // a valid frame to re-map.
    if (! producedAny)
    {
        if (analyser.getFramesProduced() <= 0 || ! differsFromPublished (settings, false, settings.freezeEpoch))
            return false;
    }

    publishFrame (settings, liveSource(), false, settings.freezeEpoch);
    return true;
}

SpectrumEngine::BinSource SpectrumEngine::liveSource() const noexcept
{
    BinSource src;
    src.numBins = analyser.getNumBins();
    src.binHz = analyser.getSettings().sampleRate / static_cast<double> (analyser.getSettings().fftSize);
    src.primary = analyser.getPrimaryPower();
    src.isDual = analyser.isDual();
    src.primaryB = src.isDual ? analyser.getPrimaryBPower() : nullptr;
    src.secondary = analyser.getSecondaryPower (false);
    src.hasSecondary = src.secondary != nullptr;
    src.secondaryB = (src.isDual && src.hasSecondary) ? analyser.getSecondaryPower (true) : nullptr;
    src.hasSidechain = analyser.hasSidechain();
    src.sidechain = src.hasSidechain ? analyser.getSidechainPower() : nullptr;
    return src;
}

SpectrumEngine::BinSource SpectrumEngine::snapshotSource() const noexcept
{
    BinSource src;
    src.numBins = snapshot.numBins;
    src.binHz = snapshot.binHz;
    src.isDual = snapshot.isDual;
    src.hasSecondary = snapshot.hasSecondary;
    src.hasSidechain = snapshot.hasSidechain;
    src.primary = snapshot.curve (0);
    src.primaryB = snapshot.curve (1);
    src.secondary = snapshot.curve (2);
    src.secondaryB = snapshot.curve (3);
    src.sidechain = snapshot.curve (4);
    return src;
}

void SpectrumEngine::takeSnapshot (const SpectrumSettings& settings) noexcept
{
    snapshot.epoch = settings.freezeEpoch;

    // Nothing to freeze before the first frame, or right after a full
    // analyser reset (the averaged arrays are stale until the next hop).
    if (analyser.getFramesProduced() <= 0)
    {
        snapshot.valid = false;
        return;
    }

    const BinSource live = liveSource();
    const size_t bytes = static_cast<size_t> (live.numBins) * sizeof (float);

    std::memcpy (snapshot.curve (0), live.primary, bytes);
    if (live.isDual)
        std::memcpy (snapshot.curve (1), live.primaryB, bytes);
    if (live.hasSecondary)
    {
        std::memcpy (snapshot.curve (2), live.secondary, bytes);
        if (live.isDual)
            std::memcpy (snapshot.curve (3), live.secondaryB, bytes);
    }
    if (live.hasSidechain)
        std::memcpy (snapshot.curve (4), live.sidechain, bytes);

    snapshot.numBins = live.numBins;
    snapshot.binHz = live.binHz;
    snapshot.isDual = live.isDual;
    snapshot.hasSecondary = live.hasSecondary;
    snapshot.hasSidechain = live.hasSidechain;
    snapshot.smoothing = settings.smoothing;
    snapshot.slopeDbPerOct = settings.slopeDbPerOct;
    snapshot.valid = true;
}

bool SpectrumEngine::differsFromPublished (const SpectrumSettings& settings, bool frozen, uint32_t epoch) const noexcept
{
    if (! published.valid || published.frozen != frozen || published.epoch != epoch)
        return true;

    const SpectrumMapper::Ranges r = SpectrumMapper::computeRanges (settings);
    return r.viewLow != published.ranges.viewLow || r.viewHigh != published.ranges.viewHigh
        || r.homeLow != published.ranges.homeLow || r.homeHigh != published.ranges.homeHigh;
}

void SpectrumEngine::publishFrame (const SpectrumSettings& settings, const BinSource& src, bool frozen, uint32_t epoch) noexcept
{
    SpectrumFrame& frame = exchange.getWriteFrame();

    // `settings.numPoints` is the visible point count; a zoomed frame adds the
    // pan margin on top of it (up to `SpectrumFrame::maxPoints` in total).
    const int numVisible = juce::jlimit (1, SpectrumFrame::maxVisiblePoints, settings.numPoints);
    const SpectrumMapper::FrameLayout layout = mapper.computeFrameFrequencies (settings, frame.freqHz.data(), numVisible);
    frame.numPoints = layout.numPoints;
    frame.firstVisiblePoint = layout.firstVisible;
    frame.numVisiblePoints = layout.numVisible;

    const SpectrumMapper::Ranges ranges = SpectrumMapper::computeRanges (settings);
    frame.viewFreqLow = ranges.viewLow;
    frame.viewFreqHigh = ranges.viewHigh;

    const int numBins = src.numBins;
    const double binHz = src.binHz;
    frame.binHz = binHz;

    mapper.mapCurveFrame (src.primary, numBins, binHz, settings, frame.primaryDb.data());

    frame.isDual = src.isDual;
    if (frame.isDual)
        mapper.mapCurveFrame (src.primaryB, numBins, binHz, settings, frame.primaryBDb.data());

    frame.hasSecondary = src.hasSecondary;
    if (frame.hasSecondary)
    {
        mapper.mapCurveFrame (src.secondary, numBins, binHz, settings, frame.secondaryDb.data());
        if (frame.isDual)
            mapper.mapCurveFrame (src.secondaryB, numBins, binHz, settings, frame.secondaryBDb.data());
    }

    frame.hasSidechain = src.hasSidechain;
    if (frame.hasSidechain)
        mapper.mapCurveFrame (src.sidechain, numBins, binHz, settings, frame.sidechainDb.data());

    frame.overviewNumPoints = mapper.mapOverview (src.primary, numBins, binHz, settings,
                                                   frame.overviewFreqHz.data(), frame.overviewDb.data());

    frame.sampleRate = settings.sampleRate;
    frame.sequence = ++frameSequence;
    frame.freezeEpoch = epoch;

    exchange.publish();

    published.valid = true;
    published.ranges = ranges;
    published.frozen = frozen;
    published.epoch = epoch;
}

} // namespace investigate
