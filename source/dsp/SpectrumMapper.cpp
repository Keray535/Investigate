#include "SpectrumMapper.h"
#include "DisplayRange.h"

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

void SpectrumMapper::Grid::allocate (int maxTotal, int maxVisible)
{
    freqHz.assign (static_cast<size_t> (maxTotal), 0.0f);
    edgesHz.assign (static_cast<size_t> (maxTotal) + 1, 0.0);
    slopeLog2.assign (static_cast<size_t> (maxVisible), 0.0);
    visible = padLow = total = halfWidth = 0;
    valid = false;
}

SpectrumMapper::Ranges SpectrumMapper::computeRanges (const SpectrumSettings& settings) noexcept
{
    Ranges r;
    r.homeHigh = DisplayRange::effectiveFreqHigh (static_cast<double> (settings.freqHigh), settings.sampleRate);
    r.homeLow = DisplayRange::effectiveFreqLow (static_cast<double> (settings.freqLow), r.homeHigh);

    const auto view = DisplayRange::clampViewFrequency (static_cast<double> (settings.viewFreqLow),
                                                         static_cast<double> (settings.viewFreqHigh),
                                                         r.homeLow, r.homeHigh);
    r.viewLow = view.low;
    r.viewHigh = view.high;
    return r;
}

void SpectrumMapper::prepare()
{
    rawPower.assign (static_cast<size_t> (maxPaddedPoints), 0.0f);
    smoothedPower.assign (static_cast<size_t> (maxPaddedPoints), 0.0f);
    cumSum.assign (static_cast<size_t> (maxPaddedPoints) + 1, 0.0);

    viewGrid.allocate (maxPaddedPoints, maxPoints);
    overviewGrid.allocate (maxOverviewPoints, maxOverviewPoints);
}

void SpectrumMapper::reset() noexcept
{
    std::fill (rawPower.begin(), rawPower.end(), 0.0f);
    std::fill (smoothedPower.begin(), smoothedPower.end(), 0.0f);
    std::fill (cumSum.begin(), cumSum.end(), 0.0);

    viewGrid.valid = false;
    viewGrid.visible = viewGrid.padLow = viewGrid.total = viewGrid.halfWidth = 0;
    overviewGrid.valid = false;
    overviewGrid.visible = overviewGrid.padLow = overviewGrid.total = overviewGrid.halfWidth = 0;
}

double SpectrumMapper::smoothingOctaves (SmoothingMode mode) noexcept
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

int SpectrumMapper::smoothingHalfWidthPoints (SmoothingMode mode, double stepOctaves, int numPoints) noexcept
{
    const double octaves = smoothingOctaves (mode);
    if (octaves <= 0.0 || stepOctaves <= 0.0)
        return 0;

    // A window of `octaves` total width, centred on each point, spread over
    // points spaced `stepOctaves` octaves apart. At most numPoints - 1, which
    // bounds the padding (1/3-octave smoothing at the 1/6-octave minimum view
    // span) to about 3 x numPoints in total.
    const int half = static_cast<int> (std::lround ((octaves * 0.5) / stepOctaves));
    return juce::jlimit (0, numPoints - 1, half);
}

void SpectrumMapper::buildGrid (Grid& grid, double viewLow, double viewHigh, double homeLow, double homeHigh,
                                 int numPoints, SmoothingMode smoothing) noexcept
{
    if (grid.valid
        && grid.keyViewLow == viewLow && grid.keyViewHigh == viewHigh
        && grid.keyHomeLow == homeLow && grid.keyHomeHigh == homeHigh
        && grid.keyNumPoints == numPoints && grid.keySmoothing == smoothing)
        return;

    const double logLo = std::log2 (viewLow);
    const double logHi = std::log2 (viewHigh);
    const double step = (numPoints > 1) ? (logHi - logLo) / static_cast<double> (numPoints - 1) : 0.0;

    // Smoothing padding: half a window on each side, but never past the
    // effective home range. At the home range edge the window is truncated,
    // exactly as in the unzoomed mapping, which keeps the unzoomed output
    // unchanged (padLow == padHigh == 0 whenever the view is the home range).
    const int half = smoothingHalfWidthPoints (smoothing, step, numPoints);
    int padLow = 0, padHigh = 0;

    if (half > 0)
    {
        constexpr double slack = 1.0e-9; // do not lose a point to rounding when the view edge sits on a grid line
        const double stepsBelow = (logLo - std::log2 (homeLow)) / step;
        const double stepsAbove = (std::log2 (homeHigh) - logHi) / step;
        padLow = juce::jlimit (0, half, static_cast<int> (std::floor (stepsBelow + slack)));
        padHigh = juce::jlimit (0, half, static_cast<int> (std::floor (stepsAbove + slack)));
    }

    const int total = padLow + numPoints + padHigh;

    // Point k of the padded grid is view point (k - padLow). Computing the
    // exponent as logLo + step * (k - padLow) keeps the visible points
    // identical, bit for bit, to the unpadded grid.
    for (int k = 0; k < total; ++k)
        grid.freqHz[static_cast<size_t> (k)] = static_cast<float> (std::exp2 (logLo + step * static_cast<double> (k - padLow)));

    // Per-point edges in Hz: the log-frequency midpoint between adjacent
    // points, extended by half a step at both ends so every point owns a span
    // of exactly `step` octaves. mapOnGrid() uses these to decide, per point,
    // whether to interpolate or aggregate.
    if (numPoints > 1)
    {
        grid.edgesHz[0] = std::exp2 (logLo + step * (-static_cast<double> (padLow) - 0.5));
        for (int k = 1; k < total; ++k)
            grid.edgesHz[static_cast<size_t> (k)] = std::exp2 (logLo + step * (static_cast<double> (k - padLow) - 0.5));
        grid.edgesHz[static_cast<size_t> (total)] = std::exp2 (logHi + step * (static_cast<double> (padHigh) + 0.5));
    }
    else
    {
        // A single point covers the whole view.
        grid.edgesHz[0] = viewLow;
        grid.edgesHz[1] = viewHigh;
    }

    for (int i = 0; i < numPoints; ++i)
        grid.slopeLog2[static_cast<size_t> (i)] = std::log2 (static_cast<double> (grid.freqHz[static_cast<size_t> (padLow + i)]) / 1000.0);

    grid.visible = numPoints;
    grid.padLow = padLow;
    grid.total = total;
    grid.halfWidth = half;

    grid.valid = true;
    grid.keyViewLow = viewLow;
    grid.keyViewHigh = viewHigh;
    grid.keyHomeLow = homeLow;
    grid.keyHomeHigh = homeHigh;
    grid.keyNumPoints = numPoints;
    grid.keySmoothing = smoothing;
}

void SpectrumMapper::computeFrequencies (const SpectrumSettings& settings, float* outFreqHz, int numPoints) noexcept
{
    numPoints = juce::jlimit (1, maxPoints, numPoints);

    const Ranges r = computeRanges (settings);
    buildGrid (viewGrid, r.viewLow, r.viewHigh, r.homeLow, r.homeHigh, numPoints, settings.smoothing);

    std::copy_n (viewGrid.freqHz.data() + viewGrid.padLow, numPoints, outFreqHz);
}

void SpectrumMapper::mapCurve (const float* binPower, int numBins, double binHz,
                                 const SpectrumSettings& settings,
                                 const float* freqHz, int numPoints,
                                 float* outDb) noexcept
{
    juce::ignoreUnused (freqHz, numPoints);
    jassert (numPoints == viewGrid.visible);
    jassert (viewGrid.valid);

    // The padding depends on the smoothing width, which the caller may change
    // between computeFrequencies() and mapCurve() (the grid keeps its view,
    // so this is a no-op in the normal case).
    buildGrid (viewGrid, viewGrid.keyViewLow, viewGrid.keyViewHigh, viewGrid.keyHomeLow, viewGrid.keyHomeHigh,
               viewGrid.keyNumPoints, settings.smoothing);

    mapOnGrid (viewGrid, binPower, numBins, binHz, settings, outDb);
}

int SpectrumMapper::mapOverview (const float* binPower, int numBins, double binHz,
                                  const SpectrumSettings& settings,
                                  float* outFreqHz, float* outDb) noexcept
{
    const int numPoints = juce::jlimit (1, maxOverviewPoints, settings.overviewPoints);

    const Ranges r = computeRanges (settings);
    buildGrid (overviewGrid, r.homeLow, r.homeHigh, r.homeLow, r.homeHigh, numPoints, settings.smoothing);

    std::copy_n (overviewGrid.freqHz.data(), numPoints, outFreqHz);
    mapOnGrid (overviewGrid, binPower, numBins, binHz, settings, outDb);
    return numPoints;
}

void SpectrumMapper::mapOnGrid (const Grid& grid, const float* binPower, int numBins, double binHz,
                                 const SpectrumSettings& settings, float* outDb) noexcept
{
    jassert (grid.total > 0 && grid.total <= maxPaddedPoints);
    jassert (numBins > 0);
    jassert (binHz > 0.0);

    const int lastBin = numBins - 1;
    const int total = grid.total;

    // 1. Bin -> point mapping (T-002 section 2), over the visible points and
    // the smoothing padding.
    for (int i = 0; i < total; ++i)
    {
        const double loEdgeHz = grid.edgesHz[static_cast<size_t> (i)];
        const double hiEdgeHz = grid.edgesHz[static_cast<size_t> (i + 1)];

        float value;

        if (hiEdgeHz - loEdgeHz <= binHz)
        {
            // Points denser than bins here: linearly interpolate (power
            // domain) between the two bins bracketing the point's own
            // frequency. A convex combination of the two neighbours, so the
            // result can never exceed max(bin0, bin1) - the acceptance
            // criterion's "+0.1 dB" margin is satisfied with room to spare.
            const double bin = static_cast<double> (grid.freqHz[static_cast<size_t> (i)]) / binHz;
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
    // already log-spaced display-point array). The window is truncated at the
    // ends of the padded array: the home range edges, or the end of the
    // padding half a window beyond the view.
    const int halfWidth = grid.halfWidth;
    const float* smoothedSource = rawPower.data();

    if (halfWidth > 0)
    {
        cumSum[0] = 0.0;
        for (int i = 0; i < total; ++i)
            cumSum[static_cast<size_t> (i + 1)] = cumSum[static_cast<size_t> (i)] + static_cast<double> (rawPower[static_cast<size_t> (i)]);

        // Only the visible points are needed.
        for (int i = grid.padLow; i < grid.padLow + grid.visible; ++i)
        {
            const int lo = juce::jmax (0, i - halfWidth);
            const int hi = juce::jmin (total - 1, i + halfWidth);
            const double sum = cumSum[static_cast<size_t> (hi + 1)] - cumSum[static_cast<size_t> (lo)];
            smoothedPower[static_cast<size_t> (i)] = static_cast<float> (sum / static_cast<double> (hi - lo + 1));
        }

        smoothedSource = smoothedPower.data();
    }

    // 3. dB conversion + slope (1 kHz pivot) + the final -200 dB floor, for
    // the visible points only.
    const double slope = static_cast<double> (settings.slopeDbPerOct);

    for (int i = 0; i < grid.visible; ++i)
    {
        const float p = juce::jmax (kLogEpsilon, smoothedSource[static_cast<size_t> (grid.padLow + i)]);
        double db = 10.0 * std::log10 (static_cast<double> (p));
        db += slope * grid.slopeLog2[static_cast<size_t> (i)];
        outDb[i] = static_cast<float> (juce::jmax (-200.0, db));
    }
}

} // namespace investigate
