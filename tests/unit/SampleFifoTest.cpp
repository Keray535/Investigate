// T-004 acceptance criterion 12: SampleFifo stress test (SPSC, no
// corruption), overflow counter exactness, and push() allocation-free.
//
// This file installs a global operator new/delete override so
// SampleFifo::push() can be checked for zero allocation. The override is
// link-wide (applies to the whole InvestigateUnitTests binary, not just this
// translation unit), so it is only *armed* (via ScopedAllocGuard) around the
// specific push() calls under test - the rest of the suite pays a negligible
// atomic-increment overhead per allocation and is otherwise unaffected.
//
// The counter/flag/guard themselves live in AllocGuard.h (T-005) with
// external linkage, so other test files (e.g. FrameExchangeTest.cpp,
// SpectrumEngineTest.cpp) can reuse the same ScopedAllocGuard without each
// defining their own operator new/delete, which would be a multiple-
// definition link error. This file remains the single definition point for
// the global operator new/delete overloads.

#include "AllocGuard.h"
#include "dsp/SampleFifo.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <new>
#include <thread>
#include <vector>

namespace investigate::test
{
    std::atomic<uint64_t> gAllocCount { 0 };
    std::atomic<bool> gAllocGuardArmed { false };
}

using investigate::test::gAllocCount;
using investigate::test::gAllocGuardArmed;
using investigate::test::ScopedAllocGuard;

void* operator new (std::size_t sz)
{
    if (gAllocGuardArmed.load (std::memory_order_relaxed))
        gAllocCount.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (sz == 0 ? 1 : sz))
        return p;
    throw std::bad_alloc();
}

void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void* operator new[] (std::size_t sz) { return operator new (sz); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace investigate;

namespace
{
    constexpr int kMinCapacity = 1 << 17; // roadmap risk-5 mitigation, documented in SampleFifo.h

    // SampleFifo wraps juce::AbstractFifo, which reserves one slot internally
    // to disambiguate full/empty (see juce_AbstractFifo.cpp:
    // getFreeSpace() == bufferSize - getNumReady() - 1).
    // SampleFifo::prepare(numChannels, capacityFrames) absorbs that
    // reservation by sizing the underlying storage at capacityFrames + 1
    // (source/dsp/SampleFifo.cpp), so the *usable* capacity - the most
    // frames that can ever be buffered at once - is exactly
    // SampleFifo::getCapacity(), with no off-by-one. This was defect #1 in
    // round 1 (see docs/qa/T-004-unit.md); the "getCapacity() is exactly
    // honest" test case below is the regression test for the fix.
}

TEST_CASE ("SampleFifo::push performs no heap allocation", "[dsp][fifo][T-004]")
{
    SampleFifo fifo;
    fifo.prepare (2, kMinCapacity);

    std::vector<float> l (256, 1.0f), r (256, -1.0f);
    const float* ch[2] = { l.data(), r.data() };

    // Warm up (first call to some JUCE/CRT paths can allocate lazily on
    // first use; we only assert on steady-state calls, matching how
    // processBlock is actually exercised after prepare()).
    fifo.push (ch, 2, 256);

    ScopedAllocGuard guard;
    for (int i = 0; i < 200; ++i)
        fifo.push (ch, 2, 256);

    REQUIRE (guard.count() == 0);
}

TEST_CASE ("SampleFifo::getCapacity() is exactly honest (regression: one-off-by-one fix)", "[dsp][fifo][T-004]")
{
    // Regression test for defect #1 (see docs/qa/T-004-unit.md, round 1):
    // SampleFifo::prepare() now sizes the underlying AbstractFifo/storage at
    // capacityFrames + 1, absorbing AbstractFifo's internal one-slot
    // reservation, so a freshly-prepared, empty SampleFifo can accept
    // exactly getCapacity() frames in a single push() without dropping.
    SampleFifo fifo;
    fifo.prepare (1, kMinCapacity);
    REQUIRE (fifo.getCapacity() == kMinCapacity);

    std::vector<float> full (static_cast<size_t> (kMinCapacity), 1.0f); // exactly getCapacity() frames
    const float* ch[1] = { full.data() };
    fifo.push (ch, 1, kMinCapacity);

    // A push of exactly getCapacity() frames into an empty FIFO now succeeds.
    REQUIRE (fifo.getOverflowCount() == 0);
    REQUIRE (fifo.getNumReady() == kMinCapacity);

    // The buffer is now completely full: even a 1-frame push must drop and count.
    std::vector<float> one (1, 1.0f);
    const float* chOne[1] = { one.data() };
    fifo.push (chOne, 1, 1);
    REQUIRE (fifo.getOverflowCount() == 1);
    REQUIRE (fifo.getNumReady() == kMinCapacity); // unchanged: dropped, not partially written
}

TEST_CASE ("SampleFifo overflow counter increments exactly on a full-buffer drop", "[dsp][fifo][T-004]")
{
    SampleFifo fifo;
    fifo.prepare (1, kMinCapacity);

    std::vector<float> block (static_cast<size_t> (kMinCapacity) - 1000, 3.0f);
    const float* ch[1] = { block.data() };

    fifo.push (ch, 1, static_cast<int> (block.size())); // fills to capacity - 1000
    REQUIRE (fifo.getOverflowCount() == 0);
    REQUIRE (fifo.getNumReady() == static_cast<int> (block.size()));

    std::vector<float> tooBig (2000, 9.0f);
    const float* chBig[1] = { tooBig.data() };
    fifo.push (chBig, 1, 2000); // only 1000 free -> whole block dropped
    REQUIRE (fifo.getOverflowCount() == 1);
    REQUIRE (fifo.getNumReady() == static_cast<int> (block.size())); // unchanged: not partially written

    std::vector<float> fits (1000, 5.0f);
    const float* chFits[1] = { fits.data() };
    fifo.push (chFits, 1, 1000); // exactly the remaining free space -> succeeds
    REQUIRE (fifo.getOverflowCount() == 1); // unchanged
    REQUIRE (fifo.getNumReady() == kMinCapacity);

    // Buffer is now completely full: even a 1-frame push must drop and count.
    std::vector<float> one (1, 1.0f);
    const float* chOne[1] = { one.data() };
    fifo.push (chOne, 1, 1);
    REQUIRE (fifo.getOverflowCount() == 2);
}

TEST_CASE ("SampleFifo::pop never returns partially-written or reordered data after a drop", "[dsp][fifo][T-004]")
{
    SampleFifo fifo;
    fifo.prepare (1, kMinCapacity);

    // Fill to full capacity with a known ramp so we can verify order.
    std::vector<float> ramp (static_cast<size_t> (kMinCapacity));
    for (size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<float> (i);
    const float* ch[1] = { ramp.data() };
    fifo.push (ch, 1, kMinCapacity);
    REQUIRE (fifo.getOverflowCount() == 0);

    // This push must be dropped entirely (buffer full).
    std::vector<float> intruder (10, -1.0f);
    const float* chIntruder[1] = { intruder.data() };
    fifo.push (chIntruder, 1, 10);
    REQUIRE (fifo.getOverflowCount() == 1);

    std::vector<float> out (static_cast<size_t> (kMinCapacity), -999.0f);
    float* dst[1] = { out.data() };
    const int popped = fifo.pop (dst, 1, kMinCapacity);
    REQUIRE (popped == kMinCapacity);

    for (int i = 0; i < kMinCapacity; ++i)
        REQUIRE (out[static_cast<size_t> (i)] == static_cast<float> (i)); // the dropped block never appears
}

TEST_CASE ("SampleFifo SPSC stress: order preserved, no corruption, across many random-sized pushes/pops", "[dsp][fifo][T-004][slow]")
{
    // Simulates roughly 10 s of 48 kHz two-channel audio (480,000 frames)
    // pushed and popped on separate threads with randomly-sized chunks, as
    // fast as the CPU allows (not throttled to wall-clock time - the
    // property under test is throughput-independent: sequence integrity
    // under concurrent SPSC access).
    SampleFifo fifo;
    fifo.prepare (2, kMinCapacity);

    constexpr int64_t totalFrames = 480'000;

    std::atomic<bool> producerDone { false };
    std::atomic<int64_t> producedCount { 0 };

    std::thread producer ([&]
    {
        juce::Random rng (1);
        int64_t counter = 0;
        std::vector<float> l, r;
        l.reserve (4096);
        r.reserve (4096);

        while (counter < totalFrames)
        {
            const int chunk = static_cast<int> (juce::jmin<int64_t> (1 + rng.nextInt (2048), totalFrames - counter));
            l.resize (static_cast<size_t> (chunk));
            r.resize (static_cast<size_t> (chunk));
            for (int i = 0; i < chunk; ++i)
            {
                const float v = static_cast<float> (counter + i);
                l[static_cast<size_t> (i)] = v;
                r[static_cast<size_t> (i)] = -v;
            }
            const float* ch[2] = { l.data(), r.data() };
            fifo.push (ch, 2, chunk);
            // Whether or not this push was dropped (overflow), the producer's
            // own notion of "next expected value" always advances - the
            // consumer independently re-derives which values it actually
            // received and checks only for reordering/corruption, not for
            // zero drops (drops are expected and allowed by design).
            counter += chunk;
            producedCount.store (counter, std::memory_order_release);
        }
        producerDone.store (true, std::memory_order_release);
    });

    // Catch2 assertions are not safe to call concurrently from multiple
    // threads (shared reporter/assertion-count state), so the consumer
    // thread only records plain local results; every REQUIRE runs on the
    // main test thread after both threads have joined.
    bool channelsDesynced = false;
    bool outOfOrder = false;
    int64_t framesChecked = 0;

    std::thread consumer ([&]
    {
        juce::Random rng (2);
        std::vector<float> l (4096), r (4096);
        float* dst[2] = { l.data(), r.data() };

        bool haveLast = false;
        float lastValue = 0.0f;

        while (! (producerDone.load (std::memory_order_acquire) && fifo.getNumReady() == 0))
        {
            const int chunk = 1 + rng.nextInt (2048);
            const int popped = fifo.pop (dst, 2, chunk);
            for (int i = 0; i < popped; ++i)
            {
                const float v = l[static_cast<size_t> (i)];
                if (r[static_cast<size_t> (i)] != -v) // channels stay in lockstep - no cross-channel corruption
                    channelsDesynced = true;

                if (haveLast && ! (v > lastValue)) // strictly increasing: no reordering, no repeats, no garbage
                    outOfOrder = true;
                lastValue = v;
                haveLast = true;
                ++framesChecked;
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE (producedCount.load() == totalFrames);
    REQUIRE_FALSE (channelsDesynced);
    REQUIRE_FALSE (outOfOrder);
    REQUIRE (framesChecked > 0);
}
