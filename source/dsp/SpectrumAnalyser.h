#pragma once

#include "RealFft.h"
#include "WindowFunctions.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace investigate
{

/** How the main (and, independently, sidechain) input channels are combined
    into one or two analysed curves. Matches the roadmap's `channelMode`
    parameter choices exactly. With a mono main input every mode analyses
    the single input channel (T-004 design requirement); `channelMode` is
    otherwise ignored in that case. */
enum class ChannelMode
{
    mid,        ///< (L+R)/2, one curve.
    side,       ///< (L-R)/2, one curve.
    left,       ///< L only, one curve.
    right,      ///< R only, one curve.
    lrPowerMean,///< (|L|^2 + |R|^2)/2 combined post-FFT, one curve.
    lrDual,     ///< L and R analysed separately, two curves.
    msDual      ///< Mid and Side analysed separately, two curves.
};

/** Which secondary accumulator (if any) `getSecondaryPower()` exposes.
    Matches the roadmap's `secondary` parameter choices. */
enum class SecondaryMode
{
    off,
    realTimeMax,
    allTimeMax,
    allTimeAverage
};

/** Frames the incoming sample stream with overlap, combines channels
    according to `channelMode`, runs the FFT, and keeps averaged power
    spectra and the secondary accumulators - all in the bin domain (linear
    power, 1.0 == 0 dBFS). Pure DSP: no dependency on the processor, the
    editor or APVTS. `SpectrumMapper`/`SpectrumEngine` (T-005) build the
    display-point mapping and the thread hand-off on top of this class.

    Threading: every member function is intended to run on a single
    "analysis" thread (the consumer side of `SampleFifo`). `prepare()` and
    `setSettings()` may allocate; `pushBlock()`, `reset()` and
    `resetSecondary()` never do, so calling them from an audio-rate context
    would also be safe, even though the roadmap only ever calls them from
    the analysis thread. Not safe to call concurrently from multiple
    threads.
*/
class SpectrumAnalyser
{
public:
    /** Plain settings struct - no APVTS, no processor dependency, so the
        class is testable in isolation. */
    struct Settings
    {
        double sampleRate = 48000.0;
        int fftSize = 4096;              ///< Power of two, 512 .. 65536.
        int overlap = 4;                 ///< 1, 2, 4 or 8. Hop = fftSize / overlap.
        WindowFunctions::Type window = WindowFunctions::Type::hann;
        ChannelMode channelMode = ChannelMode::mid;
        float avgTimeMs = 1000.0f;       ///< 0 .. 10000 ms; time for the level to fall 20 dB. 0 = no averaging.
        SecondaryMode secondary = SecondaryMode::realTimeMax;
        int numMainChannels = 2;         ///< 1 or 2.
        int numSidechainChannels = 0;    ///< 0, 1 or 2.
    };

    static constexpr int maxFftSize = 65536;
    static constexpr int maxNumBins = maxFftSize / 2 + 1;

    explicit SpectrumAnalyser (FftBackend backend = FftBackend::pffft);
    ~SpectrumAnalyser();

    SpectrumAnalyser (const SpectrumAnalyser&) = delete;
    SpectrumAnalyser& operator= (const SpectrumAnalyser&) = delete;

    /** Allocates every buffer at the maximum supported size (65536-point
        FFT, stereo main + stereo sidechain) and applies `initial` as the
        first settings. Call once, off the audio thread, before the first
        `pushBlock()`. May allocate. */
    void prepare (const Settings& initial);

    /** Applies new settings. May allocate (recreates the FFT plan when
        `fftSize` changes - "reallocation is allowed" on the analysis
        thread per the task). Always resets the framing state (ring
        buffers). Additionally resets the secondary accumulators and
        re-primes the averaged primary/primaryB/sidechain arrays whenever
        `channelMode`, `fftSize`, `window`, `sampleRate`, or the channel
        counts differ from the previous settings (the roadmap's explicit
        reset triggers, extended to channel-count changes for the same
        reason: old ring-buffer content would otherwise mix samples
        combined under a different layout). `overlap`, `avgTimeMs` and
        `secondary` changes alone touch none of this state. */
    void setSettings (const Settings& newSettings);

    const Settings& getSettings() const noexcept { return settings; }

    /** Clears framing state (ring buffers, write position, hop countdown)
        and re-primes the averaged primary/primaryB/sidechain arrays, but
        leaves the secondary accumulators untouched. Intended for
        `SampleFifo` overflow resynchronisation, where a dropped-samples
        glitch shouldn't also discard the displayed max/average history.
        Never allocates. */
    void reset() noexcept;

    /** Clears the real-time max, all-time max and all-time average
        accumulators (and the all-time-average sample counts) for every
        curve. Never allocates. */
    void resetSecondary() noexcept;

    /** Feeds `numFrames` new samples per channel and runs the framing/
        window/FFT/averaging/secondary pipeline for every hop boundary
        crossed. `mainChannels` must have `getSettings().numMainChannels`
        valid pointers, each with `numFrames` valid samples;
        `sidechainChannels` must have `getSettings().numSidechainChannels`
        valid pointers (may be nullptr if that count is 0). Real-time-safe:
        no allocation, no locks, bounded work. */
    void pushBlock (const float* const* mainChannels,
                     const float* const* sidechainChannels,
                     int numFrames) noexcept;

    /** True when the current channel mode (and a stereo main input)
        produces two separate curves (L/R dual or M/S dual). Always false
        with a mono main input. */
    bool isDual() const noexcept { return dualOutput; }

    bool hasSidechain() const noexcept { return settings.numSidechainChannels > 0; }

    int getNumBins() const noexcept { return numBins; }
    double getBinFrequency (int bin) const noexcept { return static_cast<double> (bin) * settings.sampleRate / static_cast<double> (settings.fftSize); }

    /** Total number of analysis frames produced since `prepare()` (or the
        last `setSettings()` that reset framing). Exposed mainly for
        testing the frame-count acceptance criterion. */
    int64_t getFramesProduced() const noexcept { return framesProduced; }

    // Bin-domain power arrays (linear power, 1.0 == 0 dBFS), each
    // getNumBins() long. The returned pointers stay valid, and the arrays
    // are updated in place, until the next pushBlock()/setSettings()/
    // reset() call - callers must not hold them across those calls, but
    // never need to copy them on every frame either (T-005 reads directly).
    const float* getPrimaryPower() const noexcept { return primary.avg.data(); }
    const float* getPrimaryBPower() const noexcept { return primaryB.avg.data(); } ///< Valid only when isDual().
    const float* getSidechainPower() const noexcept { return sidechain.avg.data(); } ///< Valid only when hasSidechain().

    const float* getRealTimeMaxPower (bool secondCurve = false) const noexcept { return curve (secondCurve).rtMax.data(); }
    const float* getAllTimeMaxPower (bool secondCurve = false) const noexcept { return curve (secondCurve).allTimeMax.data(); }
    const float* getAllTimeAveragePower (bool secondCurve = false) const noexcept { return curve (secondCurve).allTimeAvg.data(); }

    /** Returns whichever of the three secondary accumulators matches
        `getSettings().secondary`, or nullptr when it is `SecondaryMode::off`. */
    const float* getSecondaryPower (bool secondCurve = false) const noexcept;

private:
    /** Per-curve state: the exponentially-averaged power spectrum plus its
        three secondary accumulators. One instance each for the primary
        curve, the (dual-mode-only) primary B curve, and the sidechain
        curve - though the sidechain curve only ever uses `avg` (T-004: no
        secondary accumulator is kept for the sidechain). */
    struct CurveState
    {
        std::vector<float> avg;
        std::vector<float> rtMax;
        std::vector<float> allTimeMax;
        std::vector<float> allTimeAvg;
        int64_t allTimeAvgCount = 0;
        bool primed = false;

        void allocate (int size);
        void primeOrBlend (const float* rawPower, int size, bool avgTimeIsZero, float alpha) noexcept;
        void updateSecondary (int size, bool avgTimeIsZero, float rtDecayMultiplier) noexcept;
        void resetSecondaryOnly (int size) noexcept;
        void reprimeAveraging() noexcept { primed = false; }
    };

    const CurveState& curve (bool secondCurve) const noexcept { return secondCurve ? primaryB : primary; }

    void recomputeDerivedConstants() noexcept;
    void produceFrame() noexcept;
    void computeStreamPower (const std::vector<float>& ring, std::vector<float>& destPower) noexcept;

    FftBackend fftBackend;
    Settings settings;

    std::unique_ptr<RealFft> fft;
    int fftSize = 0;
    int numBins = 0;
    int hopSize = 0;
    bool dualOutput = false;
    bool needsBufferB = false;       ///< bufferB participates (lrPowerMean, lrDual, msDual), even when not dual output.
    bool combinePrimaryInPower = false; ///< lrPowerMean: bufferA/B combine post-FFT into one curve.

    double coherentGain = 1.0;
    float binScale = 1.0f;
    float dcNyquistScale = 1.0f;

    // Averaging / secondary decay constants, recomputed whenever overlap,
    // avgTimeMs, fftSize or sampleRate change.
    bool avgTimeIsZero = true;
    float averagingAlpha = 1.0f;
    float rtMaxDecayMultiplier = 1.0f;

    // Window table, sized at the maximum FFT size; only the first fftSize
    // entries are valid for the current settings.
    std::vector<float> windowTable;

    // Ring buffers (framing), sized at the maximum FFT size so no fftSize
    // change ever reallocates them - only the FFT plan and window table are
    // recreated. Capacity == current fftSize (a power of two); writePos
    // wraps with a bitmask.
    std::array<std::vector<float>, 2> ringMain;
    std::array<std::vector<float>, 2> ringSidechain;
    int writePos = 0;
    int fftSizeMask = 0;
    int64_t samplesIngested = 0;
    int countdownToNextFrame = 0;
    int64_t framesProduced = 0;

    // Scratch, reused every frame - no allocation outside prepare()/setSettings().
    std::vector<float> frameScratch;
    std::vector<float> realScratch, imagScratch;
    std::array<std::vector<float>, 2> powerMain;
    std::array<std::vector<float>, 2> powerSidechain;
    std::vector<float> rawPrimary, rawPrimaryB, rawSidechain;

    CurveState primary, primaryB, sidechain;
};

} // namespace investigate
