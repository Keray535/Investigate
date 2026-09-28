#include "SpectrumAnalyser.h"

#include <algorithm>
#include <cmath>

namespace investigate
{

namespace
{
    // -200 dB floor (10*log10(1e-20) == -200 exactly): keeps every
    // averaged/secondary bin finite and non-zero for silence, DC, or
    // denormal-range input, per the task's "clean output" requirement.
    constexpr float kFloorPower = 1e-20f;
}

// ============================================================================
// CurveState
// ============================================================================

void SpectrumAnalyser::CurveState::allocate (int size)
{
    avg.assign (static_cast<size_t> (size), kFloorPower);
    rtMax.assign (static_cast<size_t> (size), kFloorPower);
    allTimeMax.assign (static_cast<size_t> (size), kFloorPower);
    allTimeAvg.assign (static_cast<size_t> (size), kFloorPower);
    allTimeAvgCount = 0;
    primed = false;
}

void SpectrumAnalyser::CurveState::primeOrBlend (const float* rawPower, int size, bool avgTimeIsZero, float alpha) noexcept
{
    // "avgTime = 0 means no averaging" and "the primed first frame after a
    // reset is hard-set, not blended" both reduce to a direct copy; the
    // only difference is that priming happens exactly once, so a later
    // avgTime == 0 still forces a direct copy every frame via avgTimeIsZero.
    if (! primed || avgTimeIsZero)
    {
        std::copy (rawPower, rawPower + size, avg.begin());
        primed = true;
        return;
    }

    for (int k = 0; k < size; ++k)
        avg[static_cast<size_t> (k)] = alpha * rawPower[k] + (1.0f - alpha) * avg[static_cast<size_t> (k)];
}

void SpectrumAnalyser::CurveState::updateSecondary (int size, bool avgTimeIsZero, float rtDecayMultiplier) noexcept
{
    ++allTimeAvgCount;
    const float invN = 1.0f / static_cast<float> (allTimeAvgCount);

    for (int k = 0; k < size; ++k)
    {
        const auto idx = static_cast<size_t> (k);
        const float a = avg[idx];

        // Real-time max: instantly tracks the averaged curve upward, falls
        // linearly in dB at 20 dB per avgTime otherwise (task requirement,
        // roadmap Decisions 2026-09-28). avgTime == 0 has "no fixed release
        // constant" and equals the current (averaged) frame directly.
        rtMax[idx] = avgTimeIsZero ? a : juce::jmax (a, rtMax[idx] * rtDecayMultiplier);

        // All-time max: never decreases except via resetSecondaryOnly().
        allTimeMax[idx] = juce::jmax (allTimeMax[idx], a);

        // All-time average: Welford running mean in the power domain, so
        // there is no unbounded accumulator.
        allTimeAvg[idx] += (a - allTimeAvg[idx]) * invN;
    }
}

void SpectrumAnalyser::CurveState::resetSecondaryOnly (int size) noexcept
{
    std::fill (rtMax.begin(), rtMax.begin() + size, kFloorPower);
    std::fill (allTimeMax.begin(), allTimeMax.begin() + size, kFloorPower);
    std::fill (allTimeAvg.begin(), allTimeAvg.begin() + size, kFloorPower);
    allTimeAvgCount = 0;
}

// ============================================================================
// SpectrumAnalyser
// ============================================================================

SpectrumAnalyser::SpectrumAnalyser (FftBackend backend)
    : fftBackend (backend)
{
}

SpectrumAnalyser::~SpectrumAnalyser() = default;

void SpectrumAnalyser::prepare (const Settings& initial)
{
    windowTable.assign (static_cast<size_t> (maxFftSize), 0.0f);

    for (auto& r : ringMain)
        r.assign (static_cast<size_t> (maxFftSize), 0.0f);
    for (auto& r : ringSidechain)
        r.assign (static_cast<size_t> (maxFftSize), 0.0f);

    frameScratch.assign (static_cast<size_t> (maxFftSize), 0.0f);
    realScratch.assign (static_cast<size_t> (maxNumBins), 0.0f);
    imagScratch.assign (static_cast<size_t> (maxNumBins), 0.0f);

    for (auto& p : powerMain)
        p.assign (static_cast<size_t> (maxNumBins), kFloorPower);
    for (auto& p : powerSidechain)
        p.assign (static_cast<size_t> (maxNumBins), kFloorPower);

    rawPrimary.assign (static_cast<size_t> (maxNumBins), kFloorPower);
    rawPrimaryB.assign (static_cast<size_t> (maxNumBins), kFloorPower);
    rawSidechain.assign (static_cast<size_t> (maxNumBins), kFloorPower);

    primary.allocate (maxNumBins);
    primaryB.allocate (maxNumBins);
    sidechain.allocate (maxNumBins);

    // Force the setSettings() call below to take the "full reset" path
    // unconditionally: settings.fftSize == 0 never matches a valid
    // (>= 512) requested fftSize, and fft == nullptr forces plan creation.
    settings = Settings{};
    settings.fftSize = 0;
    fft.reset();

    setSettings (initial);
}

void SpectrumAnalyser::setSettings (const Settings& newSettings)
{
    Settings s = newSettings;
    s.fftSize = juce::jlimit (512, maxFftSize, s.fftSize);
    jassert (juce::isPowerOfTwo (s.fftSize));
    s.overlap = juce::jlimit (1, 8, s.overlap);
    jassert (juce::isPowerOfTwo (s.overlap));
    s.avgTimeMs = juce::jmax (0.0f, s.avgTimeMs);
    s.numMainChannels = juce::jlimit (1, 2, s.numMainChannels);
    s.numSidechainChannels = juce::jlimit (0, 2, s.numSidechainChannels);
    s.sampleRate = juce::jmax (1000.0, s.sampleRate);

    const bool needsFullReset = (s.channelMode != settings.channelMode)
                              || (s.fftSize != settings.fftSize)
                              || (s.window != settings.window)
                              || (s.sampleRate != settings.sampleRate)
                              || (s.numMainChannels != settings.numMainChannels)
                              || (s.numSidechainChannels != settings.numSidechainChannels);

    const bool fftPlanNeedsRebuild = (s.fftSize != settings.fftSize) || (fft == nullptr);
    const bool windowTableNeedsRebuild = (s.window != settings.window) || (s.fftSize != settings.fftSize);

    settings = s;
    fftSize = settings.fftSize;
    numBins = fftSize / 2 + 1;
    hopSize = fftSize / settings.overlap;
    fftSizeMask = fftSize - 1;

    const bool stereoMain = settings.numMainChannels == 2;
    dualOutput = stereoMain && (settings.channelMode == ChannelMode::lrDual || settings.channelMode == ChannelMode::msDual);
    needsBufferB = stereoMain && (settings.channelMode == ChannelMode::lrPowerMean
                                   || settings.channelMode == ChannelMode::lrDual
                                   || settings.channelMode == ChannelMode::msDual);
    combinePrimaryInPower = stereoMain && settings.channelMode == ChannelMode::lrPowerMean;

    if (fftPlanNeedsRebuild)
        fft = createRealFft (fftBackend, fftSize);

    if (windowTableNeedsRebuild)
    {
        coherentGain = WindowFunctions::generate (settings.window, windowTable.data(), fftSize);

        // Coherent-gain normalisation (Harris 1978 / T-002 section 2): an
        // unscaled real FFT of a CG-windowed, full-scale, bin-centred sine
        // has magnitude (1/2)*fftSize*CG at that bin; DC/Nyquist are not
        // mirrored so they omit the factor of 2. Dividing by these scales
        // makes a full-scale bin-centred sine read 0 dBFS (power == 1.0).
        const double denom = static_cast<double> (fftSize) * coherentGain;
        binScale = static_cast<float> (2.0 / denom);
        dcNyquistScale = static_cast<float> (1.0 / denom);
    }

    recomputeDerivedConstants();

    if (needsFullReset)
    {
        reset();
        resetSecondary();
    }
}

void SpectrumAnalyser::recomputeDerivedConstants() noexcept
{
    const double hopSeconds = static_cast<double> (hopSize) / settings.sampleRate;
    const double avgTimeSeconds = static_cast<double> (settings.avgTimeMs) / 1000.0;

    avgTimeIsZero = avgTimeSeconds <= 0.0;

    if (! avgTimeIsZero)
    {
        // avgTime is SPAN's "time to fall 20 dB" convention (roadmap
        // Decisions 2026-09-28, T-001 section 2/9): tau = avgTime / ln(100)
        // makes a power-domain single-pole filter's step response cross
        // -20 dB at t == avgTime. alpha is derived per-hop so the
        // behaviour is independent of fftSize/overlap/sample rate (T-002
        // section 2).
        const double tau = avgTimeSeconds / std::log (100.0);
        averagingAlpha = static_cast<float> (1.0 - std::exp (-hopSeconds / tau));

        // Real-time max release: a linear-in-dB fall of 20 dB per
        // avgTimeSeconds is, in the power domain, a fixed per-hop
        // multiplicative decay: 10*log10(decay) * (avgTimeSeconds/hopSeconds) == -20.
        rtMaxDecayMultiplier = static_cast<float> (std::pow (10.0, -2.0 * hopSeconds / avgTimeSeconds));
    }
    else
    {
        averagingAlpha = 1.0f;
        rtMaxDecayMultiplier = 0.0f;
    }
}

void SpectrumAnalyser::reset() noexcept
{
    for (auto& r : ringMain)
        std::fill (r.begin(), r.begin() + fftSize, 0.0f);
    for (auto& r : ringSidechain)
        std::fill (r.begin(), r.begin() + fftSize, 0.0f);

    writePos = 0;
    samplesIngested = 0;
    countdownToNextFrame = fftSize;
    framesProduced = 0;

    primary.reprimeAveraging();
    primaryB.reprimeAveraging();
    sidechain.reprimeAveraging();
}

void SpectrumAnalyser::resetSecondary() noexcept
{
    primary.resetSecondaryOnly (numBins);
    primaryB.resetSecondaryOnly (numBins);
    sidechain.resetSecondaryOnly (numBins);
}

void SpectrumAnalyser::pushBlock (const float* const* mainChannels,
                                   const float* const* sidechainChannels,
                                   int numFrames) noexcept
{
    const juce::ScopedNoDenormals noDenormals;

    if (numFrames <= 0 || fft == nullptr)
        return;

    const int numMain = settings.numMainChannels;
    const int numSc = settings.numSidechainChannels;
    const ChannelMode mode = settings.channelMode;

    float* ringA = ringMain[0].data();
    float* ringB = ringMain[1].data();
    float* ringScL = ringSidechain[0].data();
    float* ringScR = ringSidechain[1].data();

    for (int i = 0; i < numFrames; ++i)
    {
        const float L = mainChannels[0][i];
        const float R = (numMain >= 2) ? mainChannels[1][i] : 0.0f;

        float a = L;
        float b = 0.0f;

        // With a mono main input every mode analyses the single channel
        // (T-004 design requirement): channelMode is only consulted when
        // there are two main channels to combine.
        if (numMain == 2)
        {
            switch (mode)
            {
                case ChannelMode::mid:         a = 0.5f * (L + R); break;
                case ChannelMode::side:        a = 0.5f * (L - R); break;
                case ChannelMode::left:        a = L; break;
                case ChannelMode::right:       a = R; break;
                case ChannelMode::lrPowerMean: a = L; b = R; break;
                case ChannelMode::lrDual:      a = L; b = R; break;
                case ChannelMode::msDual:      a = 0.5f * (L + R); b = 0.5f * (L - R); break;
            }
        }

        ringA[writePos] = a;
        if (needsBufferB)
            ringB[writePos] = b;

        if (numSc >= 1)
            ringScL[writePos] = sidechainChannels[0][i];
        if (numSc >= 2)
            ringScR[writePos] = sidechainChannels[1][i];

        writePos = (writePos + 1) & fftSizeMask;
        ++samplesIngested;

        // Samples are combined and ring-written one at a time, and the hop
        // boundary is checked after every sample (rather than once per
        // pushBlock call), specifically so a single large pushBlock call
        // still produces the correct sequence of distinct, correctly
        // time-offset overlapping frames instead of repeating "the current
        // end of the buffer" for every hop that was crossed within the call.
        if (--countdownToNextFrame <= 0)
        {
            if (samplesIngested >= fftSize)
                produceFrame();

            countdownToNextFrame = hopSize;
        }
    }
}

void SpectrumAnalyser::computeStreamPower (const std::vector<float>& ring, std::vector<float>& destPower) noexcept
{
    const float* w = windowTable.data();
    const float* r = ring.data();
    float* frame = frameScratch.data();

    // writePos currently points at the oldest surviving sample in a full
    // ring (capacity == fftSize exactly), so reading fftSize samples
    // forward from writePos, wrapping, gives chronological oldest-to-newest
    // order - the standard STFT frame convention.
    for (int i = 0; i < fftSize; ++i)
        frame[i] = w[i] * r[(writePos + i) & fftSizeMask];

    fft->forward (frame, realScratch.data(), imagScratch.data());

    const float* re = realScratch.data();
    const float* im = imagScratch.data();
    float* dest = destPower.data();
    const int half = fftSize / 2;

    {
        const float amp = re[0] * dcNyquistScale;
        dest[0] = juce::jmax (kFloorPower, amp * amp);
    }
    {
        const float amp = re[half] * dcNyquistScale;
        dest[half] = juce::jmax (kFloorPower, amp * amp);
    }
    for (int k = 1; k < half; ++k)
    {
        const float reS = re[k] * binScale;
        const float imS = im[k] * binScale;
        dest[k] = juce::jmax (kFloorPower, reS * reS + imS * imS);
    }
}

void SpectrumAnalyser::produceFrame() noexcept
{
    computeStreamPower (ringMain[0], powerMain[0]);
    if (needsBufferB)
        computeStreamPower (ringMain[1], powerMain[1]);

    if (settings.numSidechainChannels >= 1)
        computeStreamPower (ringSidechain[0], powerSidechain[0]);
    if (settings.numSidechainChannels >= 2)
        computeStreamPower (ringSidechain[1], powerSidechain[1]);

    // L+R power mean combines two independent FFTs in the power domain
    // (T-004 design requirement: a time-domain (L+R)/2 average would just
    // be Mid again).
    if (combinePrimaryInPower)
    {
        for (int k = 0; k < numBins; ++k)
            rawPrimary[static_cast<size_t> (k)] = 0.5f * (powerMain[0][static_cast<size_t> (k)] + powerMain[1][static_cast<size_t> (k)]);
    }
    else
    {
        std::copy (powerMain[0].begin(), powerMain[0].begin() + numBins, rawPrimary.begin());
    }

    if (dualOutput)
        std::copy (powerMain[1].begin(), powerMain[1].begin() + numBins, rawPrimaryB.begin());

    if (settings.numSidechainChannels == 1)
    {
        std::copy (powerSidechain[0].begin(), powerSidechain[0].begin() + numBins, rawSidechain.begin());
    }
    else if (settings.numSidechainChannels == 2)
    {
        // Sidechain is always a single curve; stereo sidechain always uses
        // the power mean of its channels, independent of the main
        // channelMode (T-004 design requirement, roadmap SpectrumFrame note).
        for (int k = 0; k < numBins; ++k)
            rawSidechain[static_cast<size_t> (k)] = 0.5f * (powerSidechain[0][static_cast<size_t> (k)] + powerSidechain[1][static_cast<size_t> (k)]);
    }

    primary.primeOrBlend (rawPrimary.data(), numBins, avgTimeIsZero, averagingAlpha);
    primary.updateSecondary (numBins, avgTimeIsZero, rtMaxDecayMultiplier);

    if (dualOutput)
    {
        primaryB.primeOrBlend (rawPrimaryB.data(), numBins, avgTimeIsZero, averagingAlpha);
        primaryB.updateSecondary (numBins, avgTimeIsZero, rtMaxDecayMultiplier);
    }

    if (settings.numSidechainChannels > 0)
        sidechain.primeOrBlend (rawSidechain.data(), numBins, avgTimeIsZero, averagingAlpha);

    ++framesProduced;
}

const float* SpectrumAnalyser::getSecondaryPower (bool secondCurve) const noexcept
{
    const CurveState& c = curve (secondCurve);

    switch (settings.secondary)
    {
        case SecondaryMode::realTimeMax:    return c.rtMax.data();
        case SecondaryMode::allTimeMax:     return c.allTimeMax.data();
        case SecondaryMode::allTimeAverage: return c.allTimeAvg.data();

        case SecondaryMode::off:
        default:
            return nullptr;
    }
}

} // namespace investigate
