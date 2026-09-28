#pragma once

#include "SpectrumFrame.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace investigate
{

/** Lock-free triple buffer, single producer (the analysis thread) / single
    consumer (the message thread), for handing off `SpectrumFrame`s (T-002
    research section 7; the classic non-blocking triple-buffer algorithm,
    sometimes attributed to Dmitry Vyukov / described as "Triple Buffering
    as a Concurrency Mechanism").

    Three fixed `SpectrumFrame` slots are partitioned, at all times, among
    three roles: the producer's private write slot, one shared "ready" slot
    (encoded in `state`), and the consumer's private read slot. Publishing
    or reading is a single atomic exchange (occasionally retried as a CAS
    loop on the read side, to keep the "clear the dirty flag only if we
    actually claimed this exact ready slot" step atomic) that swaps the
    caller's private slot with the shared one - never blocking the other
    side and never touching the other side's private slot. The consumer may
    skip frames if it reads slower than the producer publishes; only the
    newest published frame is ever visible to a `read()` call. All memory
    (the three slots) is allocated once in `prepare()`, so steady-state use
    never allocates.
*/
class FrameExchange
{
public:
    /** Resets the three slots to their default-constructed state and the
        initial role assignment (write = 0, shared = 1, read = 2). Not
        real-time-safe by contract (it default-constructs three
        `SpectrumFrame`s, ~49 KB each); call once, off the audio thread,
        before the producer or consumer start. */
    void prepare() noexcept;

    /** Producer only (analysis thread). Returns a mutable reference to the
        producer's current private write slot. Fill in every field, then
        call `publish()`. The reference is invalidated by the next
        `publish()` call (it may return a different slot afterwards).
        Real-time-safe. */
    SpectrumFrame& getWriteFrame() noexcept { return slots[static_cast<size_t> (writeIndex)]; }

    /** Producer only. Publishes the frame last returned by
        `getWriteFrame()` as the newest frame, and recycles whichever slot
        was previously "ready" (read or not) as the new private write slot.
        Real-time-safe: one atomic exchange, no allocation, no blocking. */
    void publish() noexcept;

    /** Consumer only (message thread). If a frame newer than the one most
        recently returned by `read()` is available, copies it into `dest`
        and returns true. Otherwise leaves `dest` untouched and returns
        false. Never returns the same published frame twice. Real-time-safe
        (wait-free apart from a rare, bounded CAS retry against a
        concurrent `publish()`): no allocation, no blocking. */
    bool read (SpectrumFrame& dest) noexcept;

private:
    static constexpr uint32_t kIndexMask = 0x3u;
    static constexpr uint32_t kDirtyBit = 0x4u;

    std::array<SpectrumFrame, 3> slots;

    // The shared "ready" slot index (bits 0-1) plus a dirty flag (bit 2)
    // meaning "this slot holds a published frame the consumer has not yet
    // claimed" - packed into one atomic so a publish/read never observes a
    // torn combination of the two.
    std::atomic<uint32_t> state { 1u };

    int writeIndex = 0; ///< Producer-private, never touched by the consumer.
    int readIndex = 2;  ///< Consumer-private, never touched by the producer.
};

} // namespace investigate
