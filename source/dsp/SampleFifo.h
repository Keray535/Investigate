#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace investigate
{

/** Lock-free single-producer/single-consumer multichannel ring buffer.

    The audio thread is the sole producer (`push`); the analysis thread is
    the sole consumer (`pop`). `push` never blocks, allocates or throws: if
    there is not enough free space for an incoming block, the whole block is
    dropped (never partially written) and an atomic overflow counter is
    incremented, so the consumer can detect the discontinuity and
    resynchronise (e.g. `SpectrumAnalyser::reset()`) instead of silently
    continuing with a corrupted framing offset.

    Implementation: one `juce::AbstractFifo` (an allocation-free SPSC index
    pair with correct acquire/release ordering - see T-002 research section
    7) drives a matching read/write cursor across N parallel per-channel
    `std::vector<float>` arrays, all sized once in `prepare()` and never
    resized afterwards. Main and sidechain audio use two separate
    `SampleFifo` instances (per the roadmap and T-004 design requirements),
    not channel indices within one instance.
*/
class SampleFifo
{
public:
    SampleFifo() = default;

    /** Allocates `numChannels` arrays of `capacityFrames` floats each and
        resets all cursors and the overflow counter. `capacityFrames` should
        be at least 1 << 17 per the roadmap's risk-5 mitigation (checked
        with `jassert` in debug builds; the caller decides the exact value).
        Not real-time-safe: call from the message or analysis thread, before
        the producer starts pushing. */
    void prepare (int numChannels, int capacityFrames);

    /** Drops all buffered samples and clears the overflow counter. Not
        safe to call concurrently with `push()`; call only while the
        producer is quiescent (e.g. before the analysis thread is started). */
    void reset() noexcept;

    /** Producer (audio thread only). `channelData` must have at least
        `min(numChannels, numChannelsPrepared)` valid pointers, each with at
        least `numFrames` valid samples. If fewer than `numFrames` frames of
        free space remain, the entire block is dropped (not partially
        written) and the overflow counter is incremented by 1. Never
        allocates, blocks, locks or throws. */
    void push (const float* const* channelData, int numChannels, int numFrames) noexcept;

    /** Consumer (analysis thread only). Copies up to `numFrames` frames
        into `dest` (at least `min(numChannels, numChannelsPrepared)`
        pointers, each with at least `numFrames` capacity) and returns the
        number of frames actually popped (0 if the FIFO was empty). Never
        allocates, blocks, locks or throws. */
    int pop (float* const* dest, int numChannels, int numFrames) noexcept;

    /** Consumer-side: number of frames currently buffered and ready to
        pop. Real-time-safe. */
    int getNumReady() const noexcept { return fifo.getNumReady(); }

    int getCapacity() const noexcept { return capacityFrames; }
    int getNumChannels() const noexcept { return numChannelsPrepared; }

    /** Total number of dropped blocks since `prepare()`/`reset()`.
        Real-time-safe; the consumer polls this to detect overflow and
        decide whether to resynchronise. */
    uint64_t getOverflowCount() const noexcept { return overflowCount.load (std::memory_order_relaxed); }

private:
    juce::AbstractFifo fifo { 1 };
    std::vector<std::vector<float>> channels;
    int numChannelsPrepared = 0;
    int capacityFrames = 0;
    std::atomic<uint64_t> overflowCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleFifo)
};

} // namespace investigate
