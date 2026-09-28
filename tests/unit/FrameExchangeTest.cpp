// T-005 acceptance criterion 6: FrameExchange lock-free triple buffer -
// publish/read semantics, allocation-free steady state, and a concurrent
// stress test (no torn frames, no out-of-order sequence numbers).

#include "AllocGuard.h"
#include "dsp/FrameExchange.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <thread>

using namespace investigate;
using investigate::test::ScopedAllocGuard;

namespace
{
    // Fills every field of a SpectrumFrame with a value deterministically
    // derived from `seq`, so a reader can detect a "torn" frame (fields from
    // two different publishes mixed together) by checking the fields are
    // still mutually consistent with a single sequence number.
    void fillFrame (SpectrumFrame& f, uint64_t seq)
    {
        f.numPoints = 4;
        f.sequence = seq;
        f.sampleRate = 48000.0 + static_cast<double> (seq % 1000);
        f.isDual = (seq % 2) == 0;
        f.hasSecondary = (seq % 3) == 0;
        f.hasSidechain = (seq % 5) == 0;

        for (int i = 0; i < f.numPoints; ++i)
        {
            const auto s = static_cast<size_t> (i);
            f.freqHz[s] = static_cast<float> (seq);
            f.primaryDb[s] = static_cast<float> (seq) * 2.0f;
            f.primaryBDb[s] = static_cast<float> (seq) * 3.0f;
            f.secondaryDb[s] = static_cast<float> (seq) * 5.0f;
            f.secondaryBDb[s] = static_cast<float> (seq) * 7.0f;
            f.sidechainDb[s] = static_cast<float> (seq) * 11.0f;
        }
    }

    bool isConsistent (const SpectrumFrame& f)
    {
        const uint64_t seq = f.sequence;
        if (f.isDual != ((seq % 2) == 0)) return false;
        if (f.hasSecondary != ((seq % 3) == 0)) return false;
        if (f.hasSidechain != ((seq % 5) == 0)) return false;
        if (std::abs (f.sampleRate - (48000.0 + static_cast<double> (seq % 1000))) > 1e-9) return false;

        for (int i = 0; i < f.numPoints; ++i)
        {
            const auto s = static_cast<size_t> (i);
            if (f.freqHz[s] != static_cast<float> (seq)) return false;
            if (f.primaryDb[s] != static_cast<float> (seq) * 2.0f) return false;
            if (f.primaryBDb[s] != static_cast<float> (seq) * 3.0f) return false;
            if (f.secondaryDb[s] != static_cast<float> (seq) * 5.0f) return false;
            if (f.secondaryBDb[s] != static_cast<float> (seq) * 7.0f) return false;
            if (f.sidechainDb[s] != static_cast<float> (seq) * 11.0f) return false;
        }
        return true;
    }
}

TEST_CASE ("FrameExchange: read() on an unpublished exchange returns false", "[T-005][frame-exchange]")
{
    FrameExchange fx;
    fx.prepare();

    SpectrumFrame dest;
    CHECK_FALSE (fx.read (dest));
}

TEST_CASE ("FrameExchange: basic publish/read round trip", "[T-005][frame-exchange]")
{
    FrameExchange fx;
    fx.prepare();

    fillFrame (fx.getWriteFrame(), 1);
    fx.publish();

    SpectrumFrame dest;
    REQUIRE (fx.read (dest));
    CHECK (dest.sequence == 1);
    CHECK (isConsistent (dest));

    // No new publish since the last successful read.
    CHECK_FALSE (fx.read (dest));
}

TEST_CASE ("FrameExchange: only the newest of several publishes survives to the next read", "[T-005][frame-exchange]")
{
    FrameExchange fx;
    fx.prepare();

    for (uint64_t s = 1; s <= 5; ++s)
    {
        fillFrame (fx.getWriteFrame(), s);
        fx.publish();
    }

    SpectrumFrame dest;
    REQUIRE (fx.read (dest));
    CHECK (dest.sequence == 5);
    CHECK (isConsistent (dest));
    CHECK_FALSE (fx.read (dest));
}

TEST_CASE ("FrameExchange: sequence numbers never decrease across repeated publish/read pairs", "[T-005][frame-exchange]")
{
    FrameExchange fx;
    fx.prepare();

    SpectrumFrame dest;
    uint64_t lastSeq = 0;
    for (uint64_t s = 1; s <= 200; ++s)
    {
        fillFrame (fx.getWriteFrame(), s);
        fx.publish();
        REQUIRE (fx.read (dest));
        CHECK (dest.sequence >= lastSeq);
        lastSeq = dest.sequence;
    }
    CHECK (lastSeq == 200);
}

TEST_CASE ("FrameExchange: prepare() may allocate, publish()/read() never do", "[T-005][frame-exchange][rt-safety]")
{
    FrameExchange fx;
    fx.prepare();
    fillFrame (fx.getWriteFrame(), 1);

    ScopedAllocGuard guard;

    for (int i = 0; i < 1000; ++i)
    {
        fillFrame (fx.getWriteFrame(), static_cast<uint64_t> (i) + 2);
        fx.publish();
        SpectrumFrame dest;
        fx.read (dest);
    }

    CHECK (guard.count() == 0);
}

// ---------------------------------------------------------------------
// AC6: a stress test with a real producer thread and a real consumer
// thread shows no torn frames (every field carries the same sequence
// stamp), no allocation after prepare(), and a sequence number that never
// decreases. Bounded by iteration count rather than wall-clock time (per
// CLAUDE.md's "no wall-clock dependence" rule for deterministic tests) -
// this still exercises the fast-producer/slow-consumer skip path the
// roadmap's "1 kHz producer / 60 Hz consumer, 10 s" scenario is about,
// since a tight-loop producer publishing whole ~49 KB frames vastly
// outpaces a tight-loop consumer that copies whole frames out.
// ---------------------------------------------------------------------
TEST_CASE ("FrameExchange: concurrent stress - no torn frames, no out-of-order sequence, no allocation", "[T-005][frame-exchange][AC6]")
{
    FrameExchange fx;
    fx.prepare();

    constexpr uint64_t kNumPublishes = 200000;

    std::atomic<bool> producerDone { false };
    std::atomic<uint64_t> tornCount { 0 };
    std::atomic<uint64_t> outOfOrderCount { 0 };
    std::atomic<uint64_t> readCount { 0 };
    std::atomic<uint64_t> allocCount { 0 };

    std::thread producer ([&]
    {
        ScopedAllocGuard guard;
        for (uint64_t s = 1; s <= kNumPublishes; ++s)
        {
            fillFrame (fx.getWriteFrame(), s);
            fx.publish();
        }
        allocCount.store (guard.count(), std::memory_order_relaxed);
        producerDone.store (true, std::memory_order_release);
    });

    std::thread consumer ([&]
    {
        SpectrumFrame dest;
        uint64_t lastSeq = 0;
        for (;;)
        {
            if (fx.read (dest))
            {
                readCount.fetch_add (1, std::memory_order_relaxed);
                if (! isConsistent (dest))
                    tornCount.fetch_add (1, std::memory_order_relaxed);
                if (dest.sequence < lastSeq)
                    outOfOrderCount.fetch_add (1, std::memory_order_relaxed);
                lastSeq = dest.sequence;
                if (lastSeq == kNumPublishes)
                    break;
            }
            else if (producerDone.load (std::memory_order_acquire))
            {
                // One last drain attempt in case the final publish() raced
                // the producerDone flag.
                if (fx.read (dest))
                {
                    readCount.fetch_add (1, std::memory_order_relaxed);
                    if (! isConsistent (dest))
                        tornCount.fetch_add (1, std::memory_order_relaxed);
                    if (dest.sequence < lastSeq)
                        outOfOrderCount.fetch_add (1, std::memory_order_relaxed);
                }
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    CHECK (allocCount.load() == 0);
    CHECK (tornCount.load() == 0);
    CHECK (outOfOrderCount.load() == 0);
    CHECK (readCount.load() >= 1);
    CHECK (readCount.load() <= kNumPublishes);
    INFO ("delivered " << readCount.load() << " of " << kNumPublishes
          << " published frames (skipping is expected and allowed)");
}
