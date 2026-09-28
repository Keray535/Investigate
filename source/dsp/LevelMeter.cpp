#include "LevelMeter.h"
#include "DecibelUtils.h"

#include <algorithm>
#include <cmath>

namespace investigate
{

void LevelMeter::prepare (double sampleRateIn, int maxBlockSizeIn, int numChannelsIn)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;
    maxBlockSize = std::max (1, maxBlockSizeIn);
    preparedChannels = std::clamp (numChannelsIn, 1, maxChannels);

    // round(sampleRate * 0.01) - a 10 ms sub-block, matching LoudnessMeter's
    // technique - never below 1 sample even at pathological rates, so the
    // sub-block loop below never divides or shifts by 0.
    subBlockSamples = std::max (1, static_cast<int> (std::llround (sampleRate * 0.01)));

    reset();
}

void LevelMeter::reset() noexcept
{
    subBlockSampleCounter = 0;
    rmsRingWritePos = 0;
    completedSubBlocks = 0;

    for (auto& c : channels)
    {
        c.displayRing.reset();
        c.subBlockPeakLinear = 0.0f;
        c.rmsRing.fill (0.0);
        c.subBlockSumSquares = 0.0;
        c.peakMaxLinear = 0.0f;
        c.sampleClipCount = 0;
        c.cachedRmsDb = Decibels::negativeInfinityDb();
    }
}

void LevelMeter::resetStatistics() noexcept
{
    for (auto& c : channels)
    {
        c.peakMaxLinear = 0.0f;
        c.sampleClipCount = 0;
    }
}

void LevelMeter::setRmsWindowMs (float windowMs) noexcept
{
    rmsWindowMs.store (std::clamp (windowMs, 50.0f, 3000.0f), std::memory_order_relaxed);
}

void LevelMeter::process (const float* const* channelsIn, int numChannels, int numSamples) noexcept
{
    if (channelsIn == nullptr || numSamples <= 0 || preparedChannels <= 0)
        return;

    const int channelsToProcess = std::min (numChannels, preparedChannels);
    if (channelsToProcess <= 0)
        return;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int c = 0; c < channelsToProcess; ++c)
        {
            auto& st = channels[static_cast<size_t> (c)];
            const float x = channelsIn[c][n];
            const float ax = std::abs (x);

            st.subBlockPeakLinear = std::max (st.subBlockPeakLinear, ax);
            st.peakMaxLinear = std::max (st.peakMaxLinear, ax);
            st.subBlockSumSquares += static_cast<double> (x) * static_cast<double> (x);

            if (ax >= 1.0f)
                ++st.sampleClipCount;
        }

        if (++subBlockSampleCounter >= subBlockSamples)
        {
            subBlockSampleCounter = 0;
            onSubBlockComplete();
        }
    }
}

void LevelMeter::onSubBlockComplete() noexcept
{
    const size_t ringIndex = static_cast<size_t> (rmsRingWritePos % static_cast<uint64_t> (kRmsRingSlots));
    ++completedSubBlocks;
    ++rmsRingWritePos;

    // Real-time-safe: bounded by kRmsRingSlots (300), never by audio
    // content - see the class comment for why this is a full recompute
    // (drift-free) rather than an incremental running sum.
    const float windowMs = rmsWindowMs.load (std::memory_order_relaxed);
    const int windowSubBlocks = std::clamp (
        static_cast<int> (std::lround (windowMs / kSubBlockMs)), 1, kRmsRingSlots);
    const uint64_t available = std::min<uint64_t> (completedSubBlocks, static_cast<uint64_t> (windowSubBlocks));

    for (int c = 0; c < preparedChannels; ++c)
    {
        auto& st = channels[static_cast<size_t> (c)];

        st.rmsRing[ringIndex] = st.subBlockSumSquares;
        st.subBlockSumSquares = 0.0;

        st.displayRing.push (st.subBlockPeakLinear);
        st.subBlockPeakLinear = 0.0f;

        if (available == 0)
        {
            st.cachedRmsDb = Decibels::negativeInfinityDb();
            continue;
        }

        double sumOfSquares = 0.0;
        for (uint64_t i = 0; i < available; ++i)
        {
            const uint64_t idx = (rmsRingWritePos - 1 - i) % static_cast<uint64_t> (kRmsRingSlots);
            sumOfSquares += st.rmsRing[static_cast<size_t> (idx)];
        }

        const double meanSquare = sumOfSquares / (static_cast<double> (available) * static_cast<double> (subBlockSamples));
        const float rmsLinear = static_cast<float> (std::sqrt (meanSquare));
        st.cachedRmsDb = Decibels::linearToDb (rmsLinear);
    }
}

float LevelMeter::getSamplePeakDb (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return Decibels::negativeInfinityDb();
    const auto& st = channels[static_cast<size_t> (channel)];
    return Decibels::linearToDb (st.displayRing.currentMax (st.subBlockPeakLinear));
}

float LevelMeter::getPeakMaxDb (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return Decibels::negativeInfinityDb();
    return Decibels::linearToDb (channels[static_cast<size_t> (channel)].peakMaxLinear);
}

float LevelMeter::getRmsDb (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return Decibels::negativeInfinityDb();
    return channels[static_cast<size_t> (channel)].cachedRmsDb;
}

uint64_t LevelMeter::getSampleClipCount (int channel) const noexcept
{
    if (channel < 0 || channel >= preparedChannels)
        return 0;
    return channels[static_cast<size_t> (channel)].sampleClipCount;
}

} // namespace investigate
