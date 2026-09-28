#include "MeterBank.h"

#include <algorithm>

namespace investigate
{

void MeterBank::prepare (double sampleRateIn, int maxBlockSize, int numChannelsIn)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;
    preparedChannels = std::clamp (numChannelsIn, 1, MeterSnapshot::maxChannels);

    levelMeter.prepare (sampleRate, maxBlockSize, preparedChannels);
    truePeakDetector.prepare (sampleRate, maxBlockSize, preparedChannels);
    loudnessMeter.prepare (sampleRate, maxBlockSize, preparedChannels);
    correlationMeter.prepare (sampleRate, maxBlockSize, preparedChannels);

    resetRequested.store (false, std::memory_order_relaxed);
    blockCounter = 0;

    snapshot = MeterSnapshot {};
    snapshot.sampleRate = sampleRate;
    version.store (0, std::memory_order_relaxed);
}

void MeterBank::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (resetRequested.exchange (false, std::memory_order_acq_rel))
    {
        // Exactly the roadmap's reset list: peak max, TP max, both clip
        // counts, integrated loudness, LRA, M max and S max. RMS,
        // momentary/short-term and correlation/balance are deliberately
        // left untouched (see each meter's class comment).
        levelMeter.resetStatistics();
        truePeakDetector.resetStatistics();
        loudnessMeter.resetStatistics();
    }

    levelMeter.process (channels, numChannels, numSamples);
    truePeakDetector.process (channels, numChannels, numSamples);
    loudnessMeter.process (channels, numChannels, numSamples);
    correlationMeter.process (channels, numChannels, numSamples);

    ++blockCounter;
    publishSnapshot();
}

void MeterBank::setRmsWindowMs (float windowMs) noexcept
{
    levelMeter.setRmsWindowMs (windowMs);
}

void MeterBank::requestReset() noexcept
{
    resetRequested.store (true, std::memory_order_release);
}

void MeterBank::publishSnapshot() noexcept
{
    const uint32_t v = version.load (std::memory_order_relaxed);
    version.store (v + 1, std::memory_order_release);   // odd: write in progress.

    snapshot.sampleRate = sampleRate;
    snapshot.blockCounter = blockCounter;

    for (int c = 0; c < MeterSnapshot::maxChannels; ++c)
    {
        snapshot.samplePeak[static_cast<size_t> (c)] = levelMeter.getSamplePeakDb (c);
        snapshot.peakMax[static_cast<size_t> (c)] = levelMeter.getPeakMaxDb (c);
        snapshot.rms[static_cast<size_t> (c)] = levelMeter.getRmsDb (c);
        snapshot.truePeak[static_cast<size_t> (c)] = truePeakDetector.getTruePeakDb (c);
        snapshot.truePeakMax[static_cast<size_t> (c)] = truePeakDetector.getTruePeakMaxDb (c);
        snapshot.sampleClipCount[static_cast<size_t> (c)] = levelMeter.getSampleClipCount (c);
        snapshot.truePeakClipCount[static_cast<size_t> (c)] = truePeakDetector.getTruePeakClipCount (c);
    }

    snapshot.momentary = loudnessMeter.getMomentaryLoudness();
    snapshot.shortTerm = loudnessMeter.getShortTermLoudness();
    snapshot.integrated = loudnessMeter.getIntegratedLoudness();
    snapshot.loudnessRange = loudnessMeter.getLoudnessRange();
    snapshot.maxMomentary = loudnessMeter.getMaxMomentary();
    snapshot.maxShortTerm = loudnessMeter.getMaxShortTerm();

    snapshot.correlation = correlationMeter.getCorrelation();
    snapshot.balance = correlationMeter.getBalanceDb();
    snapshot.isMono = correlationMeter.isMono();

    version.store (v + 2, std::memory_order_release);   // even: write complete.
}

MeterSnapshot MeterBank::getSnapshot() const noexcept
{
    MeterSnapshot result;
    uint32_t v1, v2;
    do
    {
        v1 = version.load (std::memory_order_acquire);
        while ((v1 & 1u) != 0u)
            v1 = version.load (std::memory_order_acquire);   // writer mid-publish; spin (reader is non-real-time).

        result = snapshot;   // plain copy, ordered after the acquire load above.

        v2 = version.load (std::memory_order_acquire);
    } while (v1 != v2);

    return result;
}

} // namespace investigate
