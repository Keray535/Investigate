#include "CorrelationMeter.h"

#include <algorithm>
#include <cmath>

namespace investigate
{

void CorrelationMeter::prepare (double sampleRateIn, int /*maxBlockSize*/, int /*numChannels*/)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;
    subBlockSamples = std::max (1, static_cast<int> (std::llround (sampleRate * 0.01)));
    reset();
}

void CorrelationMeter::reset() noexcept
{
    subBlockSampleCounter = 0;
    ring.fill (SubBlockSums {});
    completedSubBlocks = 0;
    currentSubBlock = SubBlockSums {};
    monoFlag = false;
    correlationValue = 0.0f;
    balanceValue = 0.0f;
}

void CorrelationMeter::process (const float* const* channelsIn, int numChannels, int numSamples) noexcept
{
    if (channelsIn == nullptr || numSamples <= 0)
        return;

    monoFlag = numChannels < 2;
    if (monoFlag)
    {
        // Roadmap: "A mono input reports a correlation of +1... and sets
        // isMono" - no accumulation needed, and nothing to recompute.
        correlationValue = 1.0f;
        balanceValue = 0.0f;
        return;
    }

    const float* l = channelsIn[0];
    const float* r = channelsIn[1];

    for (int n = 0; n < numSamples; ++n)
    {
        const double lv = static_cast<double> (l[n]);
        const double rv = static_cast<double> (r[n]);

        currentSubBlock.sumLR += lv * rv;
        currentSubBlock.sumL2 += lv * lv;
        currentSubBlock.sumR2 += rv * rv;

        if (++subBlockSampleCounter >= subBlockSamples)
        {
            subBlockSampleCounter = 0;
            ring[static_cast<size_t> (completedSubBlocks % static_cast<uint64_t> (kRingSlots))] = currentSubBlock;
            currentSubBlock = SubBlockSums {};
            ++completedSubBlocks;
        }
    }

    recomputeCorrelationAndBalance();
}

void CorrelationMeter::recomputeCorrelationAndBalance() noexcept
{
    // Bounded by kRingSlots (300) regardless of numSamples - a full,
    // drift-free recompute of both rectangular windows, done once per
    // process() call. See the class comment for why this beats an
    // incrementally-maintained running sum.
    const auto sumWindow = [this] (int blocks) noexcept -> SubBlockSums
    {
        SubBlockSums total;
        const uint64_t available = std::min<uint64_t> (completedSubBlocks, static_cast<uint64_t> (blocks));
        for (uint64_t i = 0; i < available; ++i)
        {
            const uint64_t idx = (completedSubBlocks - 1 - i) % static_cast<uint64_t> (kRingSlots);
            const auto& s = ring[static_cast<size_t> (idx)];
            total.sumLR += s.sumLR;
            total.sumL2 += s.sumL2;
            total.sumR2 += s.sumR2;
        }
        return total;
    };

    const auto corrSums = sumWindow (kCorrelationWindowBlocks);
    const double denomCorr = std::sqrt (std::max (corrSums.sumL2 * corrSums.sumR2, kEpsilon));
    correlationValue = static_cast<float> (std::clamp (corrSums.sumLR / denomCorr, -1.0, 1.0));

    const auto balSums = sumWindow (kBalanceWindowBlocks);
    const double ratio = (balSums.sumR2 + kEpsilon) / (balSums.sumL2 + kEpsilon);
    balanceValue = static_cast<float> (10.0 * std::log10 (ratio));
}

} // namespace investigate
