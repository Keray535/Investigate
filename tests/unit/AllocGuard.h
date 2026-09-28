#pragma once

// Shared declarations for the link-wide operator new/delete override defined
// in SampleFifoTest.cpp (T-004). The override applies to the whole
// InvestigateUnitTests binary; any test file may #include this header and
// use ScopedAllocGuard to check a code path for zero heap allocation, as
// long as SampleFifoTest.cpp (the single definition point for operator
// new/delete) stays linked into the same binary. Moved out of an anonymous
// namespace (T-005) so more than one translation unit can share the same
// counter/flag instead of each defining its own operator new/delete, which
// would be a multiple-definition link error.

#include <atomic>
#include <cstdint>

namespace investigate::test
{
    extern std::atomic<uint64_t> gAllocCount;
    extern std::atomic<bool> gAllocGuardArmed;

    /** While alive, every operator new/new[] call anywhere in the process
        increments `gAllocCount`. Construct one around the exact code path
        under test; read `count()` after the scope you want checked. */
    struct ScopedAllocGuard
    {
        ScopedAllocGuard() noexcept
        {
            gAllocCount.store (0, std::memory_order_relaxed);
            gAllocGuardArmed.store (true, std::memory_order_relaxed);
        }
        ~ScopedAllocGuard() noexcept
        {
            gAllocGuardArmed.store (false, std::memory_order_relaxed);
        }
        uint64_t count() const noexcept { return gAllocCount.load (std::memory_order_relaxed); }
    };
}
