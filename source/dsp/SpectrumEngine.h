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
        `SpectrumAnalyser::setSettings()`); never allocates otherwise.

        Zoom and freeze (M3b, docs/plans/spectrum-zoom-pan.md):

         - Live (`frozen == false`): a new hop maps the newest bin-domain
           state onto the clamped view range and publishes it. With no new
           hop, a change of the clamped view range, of the effective home
           range, or of the freeze state (an unfreeze) re-maps the latest
           bin-domain state and publishes at once. A re-publish reads the
           analyser and changes none of its state. If the analyser has no
           valid frame (nothing produced since its last full reset) there is
           nothing to re-map and nothing is published.
         - Freeze: the first call with `frozen == true` for a new
           `freezeEpoch` copies the bin-domain state, as the last published
           frame was made from it (before this call drains new samples),
           into a preallocated snapshot together with `binHz`, the curve
           flags, the smoothing and the slope, and publishes it as a frame
           stamped with the new epoch. Analysis continues internally (so
           `resetSecondary()` still works) but no live frame is published.
           A view or home-range change re-maps the snapshot and publishes.
           The call's `smoothing` and `slopeDbPerOct` are ignored for the
           snapshot; the home range is honoured. A change of `fftSize`,
           `window`, `channelMode` or the layout resets the live analyser
           and leaves the snapshot untouched.
         - Every frame carries `SpectrumSettings::freezeEpoch` (live frames
           the current one, frozen frames the epoch of the freeze).

        Returns true if a frame was published. */
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
    /** Pointers to one set of bin-domain curves plus what describes them:
        either the live analyser or the freeze snapshot. */
    struct BinSource
    {
        const float* primary = nullptr;
        const float* primaryB = nullptr;
        const float* secondary = nullptr;
        const float* secondaryB = nullptr;
        const float* sidechain = nullptr;
        int numBins = 0;
        double binHz = 1.0;
        bool isDual = false;
        bool hasSecondary = false;
        bool hasSidechain = false;
    };

    /** The bin-domain freeze snapshot. `data` holds five curves
        (primary, primary B, secondary, secondary B, sidechain) of
        `SpectrumAnalyser::maxNumBins` floats each, sized in prepare(). */
    struct Snapshot
    {
        std::vector<float> data;
        bool valid = false;
        uint32_t epoch = 0;
        int numBins = 0;
        double binHz = 1.0;
        bool isDual = false;
        bool hasSecondary = false;
        bool hasSidechain = false;
        SmoothingMode smoothing = SmoothingMode::off;
        float slopeDbPerOct = 0.0f;

        float* curve (int index) noexcept { return data.data() + static_cast<size_t> (index) * SpectrumAnalyser::maxNumBins; }
        const float* curve (int index) const noexcept { return data.data() + static_cast<size_t> (index) * SpectrumAnalyser::maxNumBins; }
    };

    /** What the most recently published frame was mapped for. */
    struct PublishedState
    {
        bool valid = false;
        SpectrumMapper::Ranges ranges;
        bool frozen = false;
        uint32_t epoch = 0;
    };

    BinSource liveSource() const noexcept;
    BinSource snapshotSource() const noexcept;
    void takeSnapshot (const SpectrumSettings& settings) noexcept;
    bool differsFromPublished (const SpectrumSettings& settings, bool frozen, uint32_t epoch) const noexcept;
    void publishFrame (const SpectrumSettings& settings, const BinSource& source, bool frozen, uint32_t epoch) noexcept;

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

    Snapshot snapshot;
    bool frozenActive = false;
    PublishedState published;
};

} // namespace investigate
