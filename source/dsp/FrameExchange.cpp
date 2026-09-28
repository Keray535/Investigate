#include "FrameExchange.h"

namespace investigate
{

void FrameExchange::prepare() noexcept
{
    for (auto& s : slots)
        s = SpectrumFrame {};

    writeIndex = 0;
    readIndex = 2;
    state.store (1u, std::memory_order_relaxed); // slot 1 is "ready", not dirty (nothing published yet).
}

void FrameExchange::publish() noexcept
{
    // Hand the just-filled write slot to the "ready" position, marked
    // dirty, and take back whatever was there before as the new write
    // slot. release: every write into slots[writeIndex] up to this point
    // must be visible to whichever thread's exchange/CAS observes this
    // value with acquire semantics.
    const uint32_t newState = static_cast<uint32_t> (writeIndex) | kDirtyBit;
    const uint32_t oldState = state.exchange (newState, std::memory_order_acq_rel);
    writeIndex = static_cast<int> (oldState & kIndexMask);
}

bool FrameExchange::read (SpectrumFrame& dest) noexcept
{
    uint32_t oldState = state.load (std::memory_order_acquire);

    for (;;)
    {
        if ((oldState & kDirtyBit) == 0)
            return false; // Nothing published since the last successful read.

        // Claim the ready slot for ourselves (clearing the dirty bit) and
        // give back our current read slot in its place. A CAS loop, not a
        // plain exchange, because a concurrent publish() between the load
        // above and this point must not be silently overwritten - if that
        // happens, oldState is refreshed to the new value and we retry
        // (single producer means this can race at most once in practice).
        const uint32_t newState = static_cast<uint32_t> (readIndex);
        if (state.compare_exchange_weak (oldState, newState,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire))
            break;
    }

    readIndex = static_cast<int> (oldState & kIndexMask);
    dest = slots[static_cast<size_t> (readIndex)];
    return true;
}

} // namespace investigate
