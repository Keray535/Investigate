#include "LoudnessMeter.h"

#include <algorithm>
#include <cmath>

namespace investigate
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;
}

LoudnessMeter::BiquadCoeffs LoudnessMeter::deriveShelfCoeffs (double rate) noexcept
{
    // BS.1770-4 Annex 1 "pre-filter" (a high shelf modelling the acoustic
    // effect of the head), expressed as an analog-prototype shelving filter
    // (f0, gain, Q) and converted with the bilinear transform. These
    // prototype parameter values are the ones libebur128 uses to derive its
    // own per-sample-rate coefficients (third_party/libebur128-src/ebur128/
    // ebur128.c, function ebur128_init_filter, read for T-006); at 48 kHz
    // this reproduces ITU-R BS.1770-4's published 48 kHz table
    // (b0=1.53512485958697, b1=-2.69169618940638, b2=1.19839281085285,
    // a1=-1.69065929318241, a2=0.73248077421585) to double-precision
    // rounding (verified numerically for T-006, difference < 1e-15).
    constexpr double f0 = 1681.9744509555319;
    constexpr double gainDb = 3.999843853973347;
    constexpr double q = 0.7071752369554196;

    const double k = std::tan (kPi * f0 / rate);
    const double vh = std::pow (10.0, gainDb / 20.0);
    const double vb = std::pow (vh, 0.4996667741545416);
    const double a0 = 1.0 + k / q + k * k;

    BiquadCoeffs c;
    c.b0 = (vh + vb * k / q + k * k) / a0;
    c.b1 = 2.0 * (k * k - vh) / a0;
    c.b2 = (vh - vb * k / q + k * k) / a0;
    c.a1 = 2.0 * (k * k - 1.0) / a0;
    c.a2 = (1.0 - k / q + k * k) / a0;
    return c;
}

LoudnessMeter::BiquadCoeffs LoudnessMeter::deriveRlbCoeffs (double rate) noexcept
{
    // BS.1770-4 Annex 1 RLB (revised low-frequency B) high-pass, same
    // analog-prototype-plus-bilinear-transform derivation as the shelf
    // above. At 48 kHz reproduces the published table (b0=1, b1=-2, b2=1,
    // a1=-1.99004745483398, a2=0.99007225036621) to double-precision
    // rounding.
    constexpr double f0 = 38.13547087602444;
    constexpr double q = 0.5003270373238773;

    const double k = std::tan (kPi * f0 / rate);
    const double a0 = 1.0 + k / q + k * k;

    BiquadCoeffs c;
    c.b0 = 1.0;
    c.b1 = -2.0;
    c.b2 = 1.0;
    c.a1 = 2.0 * (k * k - 1.0) / a0;
    c.a2 = (1.0 - k / q + k * k) / a0;
    return c;
}

double LoudnessMeter::energyToLoudness (double energy) noexcept
{
    // BS.1770-4 eq. (2): L_K = -0.691 + 10*log10(sum of mean-square per
    // channel). Guard log(0) for exact digital silence rather than letting
    // it produce -inf via std::log10(0.0) implicitly (well-defined, but be
    // explicit that this is the intended "silence" answer, not a NaN risk).
    if (energy <= 0.0)
        return -std::numeric_limits<double>::infinity();
    return -0.691 + 10.0 * std::log10 (energy);
}

int LoudnessMeter::histogramBinForLoudness (double loudnessLufs) noexcept
{
    // 1000 bins, 0.1 LU wide, spanning [-70.0, +30.0) LUFS - the same
    // layout libebur128's EBUR128_MODE_HISTOGRAM uses, so the two
    // implementations' gating decisions land in the same bin for the same
    // input, keeping the cross-check well inside its tolerance.
    const double scaled = (loudnessLufs - kHistogramLowLufs) / kHistogramLuStep;
    const int bin = static_cast<int> (std::floor (scaled));
    return std::clamp (bin, 0, kHistogramBins - 1);
}

double LoudnessMeter::histogramBinCenterEnergy (int bin) noexcept
{
    const double centerLoudness = kHistogramLowLufs + (static_cast<double> (bin) + 0.5) * kHistogramLuStep;
    // Inverse of energyToLoudness(): energy = 10^((L + 0.691) / 10).
    return std::pow (10.0, (centerLoudness + 0.691) / 10.0);
}

void LoudnessMeter::prepare (double sampleRateIn, int maxBlockSizeIn, int numChannelsIn)
{
    sampleRate = sampleRateIn > 0.0 ? sampleRateIn : 48000.0;
    maxBlockSize = std::max (1, maxBlockSizeIn);
    preparedChannels = std::clamp (numChannelsIn, 1, maxChannels);

    shelfCoeffs = deriveShelfCoeffs (sampleRate);
    rlbCoeffs = deriveRlbCoeffs (sampleRate);

    shelfState.assign (static_cast<size_t> (preparedChannels), BiquadState {});
    rlbState.assign (static_cast<size_t> (preparedChannels), BiquadState {});
    subBlockSumSquares.assign (static_cast<size_t> (preparedChannels), 0.0);

    // round(sampleRate * 0.01) - a 10 ms sub-block (see the class comment
    // for why momentary/short-term use a finer grid than the 100 ms hop
    // Integrated Loudness/LRA gating requires) - never below 1 sample even
    // at pathological rates, so the sub-block/loop logic below never
    // divides or shifts by 0.
    subBlockSamples = std::max (1, static_cast<int> (std::llround (sampleRate * 0.01)));

    reset();
}

void LoudnessMeter::reset() noexcept
{
    for (auto& s : shelfState) s.reset();
    for (auto& s : rlbState) s.reset();
    std::fill (subBlockSumSquares.begin(), subBlockSumSquares.end(), 0.0);
    subBlockSampleCounter = 0;
    gatingSubBlockCounter = 0;

    momentaryRing.fill (0.0);
    momentaryWritePos = 0;
    momentaryCount = 0;
    momentarySum = 0.0;

    shortTermRing.fill (0.0);
    shortTermWritePos = 0;
    shortTermCount = 0;
    shortTermSum = 0.0;

    integratedHistogram.fill (0);
    integratedHistogramTotal = 0;
    lraHistogram.fill (0);
    lraHistogramTotal = 0;

    momentaryLoudness = negativeInfinity();
    shortTermLoudness = negativeInfinity();
    integratedLoudness = negativeInfinity();
    loudnessRange = 0.0f;
    maxMomentary = negativeInfinity();
    maxShortTerm = negativeInfinity();
}

void LoudnessMeter::resetStatistics() noexcept
{
    integratedHistogram.fill (0);
    integratedHistogramTotal = 0;
    lraHistogram.fill (0);
    lraHistogramTotal = 0;

    integratedLoudness = negativeInfinity();
    loudnessRange = 0.0f;
    maxMomentary = negativeInfinity();
    maxShortTerm = negativeInfinity();
}

void LoudnessMeter::process (const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numSamples <= 0 || preparedChannels <= 0)
        return;

    const int channelsToProcess = std::min (numChannels, preparedChannels);
    if (channelsToProcess <= 0)
        return;

    for (int n = 0; n < numSamples; ++n)
    {
        for (int c = 0; c < channelsToProcess; ++c)
        {
            const auto ch = static_cast<size_t> (c);
            const double x = static_cast<double> (channels[c][n]);
            const double shelfOut = shelfState[ch].process (x, shelfCoeffs);
            const double kWeighted = rlbState[ch].process (shelfOut, rlbCoeffs);
            subBlockSumSquares[ch] += kWeighted * kWeighted;
        }

        if (++subBlockSampleCounter >= subBlockSamples)
        {
            subBlockSampleCounter = 0;
            onSubBlockComplete();
        }
    }
}

void LoudnessMeter::onSubBlockComplete() noexcept
{
    // BS.1770 sums (not averages) the per-channel mean square, each
    // weighted at 1.0 for the L/R/mono channels Investigate supports (see
    // the class comment: this is why an identical mono signal reads about
    // 3 dB quieter than the same signal duplicated to both channels of a
    // stereo input).
    double weightedPower = 0.0;
    for (int c = 0; c < preparedChannels; ++c)
    {
        const auto ch = static_cast<size_t> (c);
        const double meanSquare = subBlockSumSquares[ch] / static_cast<double> (subBlockSamples);
        weightedPower += meanSquare;
        subBlockSumSquares[ch] = 0.0;
    }

    // --- Momentary: sliding sum over the last 40 sub-blocks (400 ms). ---
    momentarySum += weightedPower;
    if (momentaryCount >= kMomentaryBlocks)
        momentarySum -= momentaryRing[static_cast<size_t> (momentaryWritePos)];
    momentaryRing[static_cast<size_t> (momentaryWritePos)] = weightedPower;
    momentaryWritePos = (momentaryWritePos + 1) % kMomentaryBlocks;
    if (momentaryCount < kMomentaryBlocks)
        ++momentaryCount;

    double momentaryMeanPower = 0.0;
    const bool momentaryValid = (momentaryCount == kMomentaryBlocks);
    if (momentaryValid)
    {
        momentaryMeanPower = momentarySum / static_cast<double> (kMomentaryBlocks);
        momentaryLoudness = static_cast<float> (energyToLoudness (momentaryMeanPower));
        maxMomentary = std::max (maxMomentary, momentaryLoudness);
    }

    // --- Short-term: sliding sum over the last 300 sub-blocks (3 s). ---
    shortTermSum += weightedPower;
    if (shortTermCount >= kShortTermBlocks)
        shortTermSum -= shortTermRing[static_cast<size_t> (shortTermWritePos)];
    shortTermRing[static_cast<size_t> (shortTermWritePos)] = weightedPower;
    shortTermWritePos = (shortTermWritePos + 1) % kShortTermBlocks;
    if (shortTermCount < kShortTermBlocks)
        ++shortTermCount;

    double shortTermMeanPower = 0.0;
    const bool shortTermValid = (shortTermCount == kShortTermBlocks);
    if (shortTermValid)
    {
        shortTermMeanPower = shortTermSum / static_cast<double> (kShortTermBlocks);
        shortTermLoudness = static_cast<float> (energyToLoudness (shortTermMeanPower));
        maxShortTerm = std::max (maxShortTerm, shortTermLoudness);
    }

    // --- Gating (Integrated Loudness / LRA): BS.1770 Annex 2 mandates an
    // exact 100 ms hop (75% overlap of the 400 ms block) for gating blocks
    // - unlike Momentary/Short-term's internal grid above, this cadence is
    // part of the standard's own algorithm, not an implementation choice,
    // so it is kept at exactly 100 ms (every 10th 10 ms sub-block) even
    // though Momentary/Short-term now update on a finer grid. Sampling the
    // sliding-window values computed above keeps this identical in effect
    // to feeding "the current momentary/short-term value" into the gate
    // every 100 ms, matching BS.1770 Annex 2 and libebur128. ---
    if (++gatingSubBlockCounter >= kGatingHopSubBlocks)
    {
        gatingSubBlockCounter = 0;
        if (momentaryValid)
            addIntegratedGatingBlock (momentaryMeanPower);
        if (shortTermValid)
            addLraSample (shortTermMeanPower);
    }
}

void LoudnessMeter::addIntegratedGatingBlock (double meanPower) noexcept
{
    const double loudness = energyToLoudness (meanPower);
    if (loudness < kAbsoluteGateLufs) // absolute gate: -70 LUFS.
        return;

    const int bin = histogramBinForLoudness (loudness);
    ++integratedHistogram[static_cast<size_t> (bin)];
    ++integratedHistogramTotal;
    recomputeIntegratedLoudness();
}

void LoudnessMeter::recomputeIntegratedLoudness() noexcept
{
    // Bounded O(kHistogramBins) work, run at most once per 100 ms - never
    // per sample, never data-dependent in iteration count.
    if (integratedHistogramTotal == 0)
    {
        integratedLoudness = negativeInfinity();
        return;
    }

    // Pass 1: absolute-gate-only ("ungated") mean, over the whole
    // histogram (it already excludes blocks below -70 LUFS).
    double sumEnergy = 0.0;
    for (int i = 0; i < kHistogramBins; ++i)
        sumEnergy += static_cast<double> (integratedHistogram[static_cast<size_t> (i)]) * histogramBinCenterEnergy (i);
    const double ungatedMeanEnergy = sumEnergy / static_cast<double> (integratedHistogramTotal);

    // Relative gate: -10 LU below the ungated mean. In the energy domain
    // that is a multiply by 0.1 (10*log10(0.1) == -10 dB == -10 LU).
    const double relativeThresholdEnergy = ungatedMeanEnergy * 0.1;
    int startBin = histogramBinForLoudness (energyToLoudness (relativeThresholdEnergy));
    // Match libebur128: advance to the next bin if the threshold sits
    // above this bin's own centre representative value.
    if (relativeThresholdEnergy > histogramBinCenterEnergy (startBin))
        ++startBin;
    if (startBin >= kHistogramBins)
    {
        integratedLoudness = negativeInfinity();
        return;
    }

    // Pass 2: mean of everything at or above the relative gate.
    double gatedSumEnergy = 0.0;
    uint64_t gatedCount = 0;
    for (int i = startBin; i < kHistogramBins; ++i)
    {
        const uint64_t n = integratedHistogram[static_cast<size_t> (i)];
        if (n == 0)
            continue;
        gatedSumEnergy += static_cast<double> (n) * histogramBinCenterEnergy (i);
        gatedCount += n;
    }
    if (gatedCount == 0)
    {
        integratedLoudness = negativeInfinity();
        return;
    }
    integratedLoudness = static_cast<float> (energyToLoudness (gatedSumEnergy / static_cast<double> (gatedCount)));
}

void LoudnessMeter::addLraSample (double meanPower) noexcept
{
    const double loudness = energyToLoudness (meanPower);
    if (loudness < kAbsoluteGateLufs) // absolute gate: -70 LUFS.
        return;

    const int bin = histogramBinForLoudness (loudness);
    ++lraHistogram[static_cast<size_t> (bin)];
    ++lraHistogramTotal;
    recomputeLoudnessRange();
}

void LoudnessMeter::recomputeLoudnessRange() noexcept
{
    // Bounded O(kHistogramBins) work, run at most once per 100 ms.
    if (lraHistogramTotal == 0)
    {
        loudnessRange = 0.0f;
        return;
    }

    // Ungated (absolute-gate-only) mean energy of the short-term value
    // population (EBU Tech 3342 section 3.1).
    double sumEnergy = 0.0;
    for (int i = 0; i < kHistogramBins; ++i)
        sumEnergy += static_cast<double> (lraHistogram[static_cast<size_t> (i)]) * histogramBinCenterEnergy (i);
    const double ungatedMeanEnergy = sumEnergy / static_cast<double> (lraHistogramTotal);

    // Relative gate: -20 LU below the ungated mean (Tech 3342's own
    // constant - deliberately not the -10 LU Integrated Loudness uses; a
    // multiply by 0.01 in the energy domain, 10*log10(0.01) == -20 dB).
    const double relativeThresholdEnergy = ungatedMeanEnergy * 0.01;
    int startBin = histogramBinForLoudness (energyToLoudness (relativeThresholdEnergy));
    if (relativeThresholdEnergy > histogramBinCenterEnergy (startBin))
        ++startBin;
    if (startBin >= kHistogramBins)
    {
        loudnessRange = 0.0f;
        return;
    }

    uint64_t gatedTotal = 0;
    for (int i = startBin; i < kHistogramBins; ++i)
        gatedTotal += lraHistogram[static_cast<size_t> (i)];
    if (gatedTotal == 0)
    {
        loudnessRange = 0.0f;
        return;
    }

    // Rank-based percentiles (EBU Tech 3342 section 3.1's reference
    // MATLAB: stl_sorted_vec(round((n-1)*P/100 + 1)), 1-based indexing, so
    // the 0-based index is round((n-1)*P/100)). Applied here to the
    // histogram's implicit sorted order (bins are already ascending), one
    // rank per percentile, rounding half up via "+0.5 then truncate" -
    // matches libebur128's ebur128_loudness_range_multiple exactly.
    const auto rankIndex = [gatedTotal] (double percentile01) noexcept -> uint64_t
    {
        const double idx = (static_cast<double> (gatedTotal) - 1.0) * percentile01 + 0.5;
        return static_cast<uint64_t> (idx);
    };
    const uint64_t lowRank = rankIndex (0.10);
    const uint64_t highRank = rankIndex (0.95);

    uint64_t cumulative = 0;
    int bin = startBin;
    while (cumulative <= lowRank && bin < kHistogramBins)
    {
        cumulative += lraHistogram[static_cast<size_t> (bin)];
        ++bin;
    }
    const double lowEnergy = histogramBinCenterEnergy (bin - 1);

    while (cumulative <= highRank && bin < kHistogramBins)
    {
        cumulative += lraHistogram[static_cast<size_t> (bin)];
        ++bin;
    }
    const double highEnergy = histogramBinCenterEnergy (bin - 1);

    loudnessRange = static_cast<float> (energyToLoudness (highEnergy) - energyToLoudness (lowEnergy));
}

} // namespace investigate
