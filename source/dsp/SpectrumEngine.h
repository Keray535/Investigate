#pragma once

#include "FrameExchange.h"
#include "SampleFifo.h"
#include "SpectrumAnalyser.h"
#include "SpectrumMapper.h"
#include "SpectrumSettings.h"

#include <array>
#include <cstdint>
#include <vector>

namespace investigate
{

/** Facade tying `SampleFifo`, `SpectrumAnalyser`, `SpectrumMapper` and
    `FrameExchange` together into the three-thread API the roadmap's
    threading model calls for:

      - Audio thread: `pushMain()` / `pushSidechain()`.
      - Analysis thread: `processPending()`, `resetSecondary()`.
      - Message thread: `readLatest()`.

    `prepare()` may be called from any non-audio thread before the other
    methods are used (and again later, e.g. on a bus-layout change - see
    the `numMainChannels`/`numSidechainChannels` note below).
*/
class SpectrumEngine
{
public:
    SpectrumEngine();
    ~SpectrumEngine();

    SpectrumEngine (const SpectrumEngine&) = delete;
    SpectrumEngine& operator= (const SpectrumEngine&) = delete;

    /** Allocates the sample FIFOs (capacity >= 2^17 frames per channel,
        roadmap risk 5), the analyser's buffers, the mapper's scratch, and
        the frame-exchange slots. `numMainChannels` (1 or 2) and
        `numSidechainChannels` (0, 1 or 2) fix the FIFOs' channel counts:
        every `SpectrumSettings` passed to `processPending()` afterwards
        should report the same counts until this is called again. Call
        once, off the audio thread, before the producer/consumer methods
        are used, and again (also off the audio thread, with the analysis
        thread stopped) if the host's bus layout changes the channel
        counts. `maxBlockSize` is accepted for the caller's own sizing
        symmetry with other `prepare()` calls in this codebase; the FIFOs'
        internal drain granularity is independent of it. May allocate. */
    void prepare (double sampleRate, int maxBlockSize, int numMainChannels, int numSidechainChannels);

    // ---- Producer API (audio thread) ----------------------------------

    /** Pushes `numSamples` frames of main input. Real-time-safe: forwards
        directly to the main `SampleFifo`, which never allocates, blocks or
        throws; a full FIFO drops the block and counts an overflow (see
        `getOverflowCount()`). */
    void pushMain (const float* const* channels, int numChannels, int numSamples) noexcept;

    /** Pushes `numSamples` frames of sidechain input. A no-op if `prepare()`
        was last called with `numSidechainChannels == 0`. Real-time-safe,
        same contract as `pushMain()`. */
    void pushSidechain (const float* const* channels, int numChannels, int numSamples) noexcept;

    // ---- Consumer API (analysis thread) --------------------------------

    /** Applies `settings` to the bin-domain analyser, drains every buffered
        sample through it (running the FFT/averaging pipeline at every hop
        boundary crossed), maps the newest resulting bin-domain state
        through `SpectrumMapper`, and publishes at most one `SpectrumFrame`
        (the newest) via `FrameExchange`. Returns true if a frame was
        published, i.e. at least one new analysis frame was produced this
        call. May allocate only when `settings` changes `fftSize`, `window`,
        `sampleRate` or the channel counts (via
        `SpectrumAnalyser::setSettings()`); never allocates otherwise. */
    bool processPending (const SpectrumSettings& settings) noexcept;

    // ---- Consumer API (message thread) ---------------------------------

    /** Returns the newest published frame, if any arrived since the last
        call. Real-time-safe (wait-free apart from a rare bounded CAS
        retry): no allocation. */
    bool readLatest (SpectrumFrame& dest) noexcept { return exchange.read (dest); }

    // ---- Control ---------------------------------------------------------

    /** Clears the real-time max, all-time max and all-time average
        accumulators. Intended to be called from the analysis thread (it
        forwards to `SpectrumAnalyser::resetSecondary()`, documented as
        analysis-thread-only), in response to a message-thread request
        (e.g. an atomic flag `processPending()`'s caller checks before each
        call - T-009's wiring). Never allocates. */
    void resetSecondary() noexcept { analyser.resetSecondary(); }

    /** Total number of dropped blocks across both the main and sidechain
        FIFOs since `prepare()`. Real-time-safe. */
    uint64_t getOverflowCount() const noexcept
    {
        return mainFifo.getOverflowCount() + sidechainFifo.getOverflowCount();
    }

private:
    void publishFrame (const SpectrumSettings& settings) noexcept;

    static constexpr int kFifoCapacityFrames = 1 << 17; ///< Roadmap risk 5 mitigation.
    static constexpr int kDrainChunkFrames = 8192;       ///< Bounded per-iteration drain size.

    SampleFifo mainFifo;
    SampleFifo sidechainFifo;
    SpectrumAnalyser analyser;
    SpectrumMapper mapper;
    FrameExchange exchange;

    // Drain scratch, sized to kDrainChunkFrames in prepare(); reused every
    // processPending() call, never resized.
    std::array<std::vector<float>, 2> mainPopBuffers;
    std::array<std::vector<float>, 2> sidechainPopBuffers;
    std::array<float*, 2> mainPopPtrs {};
    std::array<float*, 2> sidechainPopPtrs {};

    int preparedNumSidechainChannels = 0;
    uint64_t frameSequence = 0;
};

} // namespace investigate
