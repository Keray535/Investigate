// Frozen copy of the pre-M3b (git HEAD 7130542) SpectrumMapper.cpp, renamed. Do not edit.
#include "LegacySpectrumMapper.h"
#include "dsp/DisplayRange.h"

#include <algorithm>
#include <cmath>

namespace investigate
{

namespace
{
    // Deliberately far below the -200 dB floor applied after slope (see
    // mapCurve()'s doc comment): only prevents log10(0) from producing
    // -inf, never determines the visible floor by itself.
    constexpr float kLogEpsilon = 1.0e-37f;
}

void LegacySpectrumMapper::prepare()
{
    rawPower.assign (static_cast<size_t> (maxPoints), 0.0f);
    smoothedPower.assign (static_cast<size_t> (maxPoints), 0.0f);
    cumSum.assign (static_cast<size_t> (maxPoints) + 1, 0.0);
    edgesHz.assign (static_cast<size_t> (maxPoints) + 1, 0.0);

    cachedNumPoints = 0;
    pointStepLog2 = 0.0;
}

void LegacySpectrumMapper::reset() noexcept
{
    std::fill (rawPower.begin(), rawPower.end(), 0.0f);
    std::fill (smoothedPower.begin(), smoothedPower.end(), 0.0f);
    std::fill (cumSum.begin(), cumSum.end(), 0.0);
    std::fill (edgesHz.begin(), edgesHz.end(), 0.0);

    cachedNumPoints = 0;
    pointStepLog2 = 0.0;
}

void LegacySpectrumMapper::computeFrequencies (const SpectrumSettings& settings, float* outFreqHz, int numPoints) noexcept
{
    numPoints = juce::jlimit (1, maxPoints, numPoints);
    cachedNumPoints = numPoints;

    const double fHiEff = DisplayRange::effectiveFreqHigh (static_cast<double> (settings.freqHigh), settings.sampleRate);
    const double fLoEff = DisplayRange::effectiveFreqLow (static_cast<double> (settings.freqLow), fHiEff);

    const double logLo = std::log2 (fLoEff);
    const double logHi = std::log2 (fHiEff);
    const double step = (numPoints > 1) ? (logHi - logLo) / static_cast<double> (numPoints - 1) : 0.0;
    pointStepLog2 = step;

    for (int i = 0; i < numPoints; ++i)
        outFreqHz[i] = static_cast<float> (std::exp2 (logLo + step * static_cast<double> (i)));

    // Per-point edges in Hz: the log-frequency midpoint between adjacent
    // points, extended by half a step at both ends so every point (including
    // the first and last) owns a span of exactly `step` octaves. mapCurve()
    // uses these to decide, per point, whether to interpolate or aggregate.
    if (numPoints > 1)
    {
        edgesHz[0] = std::exp2 (logLo - 0.5 * step);
        for (int i = 1; i < numPoints; ++i)
            edgesHz[static_cast<size_t> (i)] = std::exp2 (logLo + step * (static_cast<double> (i) - 0.5));
        edgesHz[static_cast<size_t> (numPoints)] = std::exp2 (logHi + 0.5 * step);
    }
    else
    {
        // A single point covers the whole effective range.
        edgesHz[0] = fLoEff;
        edgesHz[1] = fHiEff;
    }
}

double LegacySpectrumMapper::smoothingOctaves (SmoothingMode mode) noexcept
{
    switch (mode)
    {
        case SmoothingMode::off:     return 0.0;
        case SmoothingMode::oct1_48: return 1.0 / 48.0;
        case SmoothingMode::oct1_24: return 1.0 / 24.0;
        case SmoothingMode::oct1_12: return 1.0 / 12.0;
        case SmoothingMode::oct1_6:  return 1.0 / 6.0;
        case SmoothingMode::oct1_3:  return 1.0 / 3.0;
        default:                     return 0.0;
    }
}

int LegacySpectrumMapper::smoothingHalfWidthPoints (SmoothingMode mode, int numPoints) const noexcept
{
    const double octaves = smoothingOctaves (mode);
    if (octaves <= 0.0 || pointStepLog2 <= 0.0)
        return 0;

    // A window of `octaves` total width, centred on each point, spread over
    // points spaced `pointStepLog2` octaves apart.
    const int half = static_cast<int> (std::lround ((octaves * 0.5) / pointStepLog2));
    return juce::jlimit (0, numPoints - 1, half);
}

void LegacySpectrumMapper::mapCurve (const float* binPower, int numBins, double binHz,
                                 const SpectrumSettings& settings,
                                 const float* freqHz, int numPoints,
                                 float* outDb) noexcept
{
    jassert (numPoints == cachedNumPoints);
    jassert (numPoints <= maxPoints);
    jassert (numBins > 0);
    jassert (binHz > 0.0);

    const int lastBin = numBins - 1;

    // 1. Bin -> point mapping (T-002 section 2).
    for (int i = 0; i < numPoints; ++i)
    {
        const double loEdgeHz = edgesHz[static_cast<size_t> (i)];
        const double hiEdgeHz = edgesHz[static_cast<size_t> (i + 1)];

        float value;

        if (hiEdgeHz - loEdgeHz <= binHz)
        {
            // Points denser than bins here: linearly interpolate (power
            // domain) between the two bins bracketing the point's own
            // frequency. A convex combination of the two neighbours, so the
            // result can never exceed max(bin0, bin1) - the acceptance
            // criterion's "+0.1 dB" margin is satisfied with room to spare.
            const double bin = static_cast<double> (freqHz[i]) / binHz;
            const double binFloor = std::floor (bin);
            const int b0 = juce::jlimit (0, lastBin, static_cast<int> (binFloor));
            const int b1 = juce::jlimit (0, lastBin, b0 + 1);
            const float frac = static_cast<float> (bin - binFloor);
            value = binPower[b0] + (binPower[b1] - binPower[b0]) * frac;
        }
        else
        {
            // Bins denser than points here: peak-preserving aggregation
            // over every bin whose centre falls in the point's span, so a
            // narrow peak between two display points is never missed.
            const int b0 = juce::jlimit (0, lastBin, static_cast<int> (std::lround (loEdgeHz / binHz)));
            int b1 = juce::jlimit (0, lastBin, static_cast<int> (std::lround (hiEdgeHz / binHz)));
            if (b1 < b0)
                b1 = b0;

            float peak = binPower[b0];
            for (int b = b0 + 1; b <= b1; ++b)
                peak = juce::jmax (peak, binPower[b]);
            value = peak;
        }

        rawPower[static_cast<size_t> (i)] = value;
    }

    // 2. Fractional-octave smoothing: power-domain boxcar mean over a fixed
    // number of neighbouring points, via a prefix sum so the per-point cost
    // is O(1) regardless of the window width (Hatziantoniou & Mourjopoulos
    // 2000, simplified per T-002 section 2 to operate directly on the
    // already log-spaced display-point array).
    const int halfWidth = smoothingHalfWidthPoints (settings.smoothing, numPoints);
    const float* smoothedSource = rawPower.data();

    if (halfWidth > 0)
    {
        cumSum[0] = 0.0;
        for (int i = 0; i < numPoints; ++i)
            cumSum[static_cast<size_t> (i + 1)] = cumSum[static_cast<size_t> (i)] + static_cast<double> (rawPower[static_cast<size_t> (i)]);

        for (int i = 0; i < numPoints; ++i)
        {
            const int lo = juce::jmax (0, i - halfWidth);
            const int hi = juce::jmin (numPoints - 1, i + halfWidth);
            const double sum = cumSum[static_cast<size_t> (hi + 1)] - cumSum[static_cast<size_t> (lo)];
            smoothedPower[static_cast<size_t> (i)] = static_cast<float> (sum / static_cast<double> (hi - lo + 1));
        }

        smoothedSource = smoothedPower.data();
    }

    // 3. dB conversion + slope (1 kHz pivot) + the final -200 dB floor.
    const double slope = static_cast<double> (settings.slopeDbPerOct);

    for (int i = 0; i < numPoints; ++i)
    {
        const float p = juce::jmax (kLogEpsilon, smoothedSource[static_cast<size_t> (i)]);
        double db = 10.0 * std::log10 (static_cast<double> (p));
        db += slope * std::log2 (static_cast<double> (freqHz[i]) / 1000.0);
        outDb[i] = static_cast<float> (juce::jmax (-200.0, db));
    }
}

} // namespace investigate
