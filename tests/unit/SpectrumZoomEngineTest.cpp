// T-035: SpectrumEngine re-publish, freeze snapshot, unfreeze, epoch,
// allocation freedom, overview through the engine, and the re-map timing.

#include "AllocGuard.h"
#include "SpectrumZoomTestUtils.h"
#include "dsp/DisplayRange.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace investigate;
using namespace investigate::test;

namespace
{
    constexpr int kInitialFrames = 40000;

    /** A rig after kInitialFrames of audio with one frame published and read. */
    struct Warm
    {
        explicit Warm (const SpectrumSettings& s, uint32_t seed = 1) : rig (s, seed)
        {
            rig.run (kInitialFrames);
            first = rig.read();
        }
        Rig rig;
        FramePtr first;
    };
}

// ---------------------------------------------------------------------
// AC4: re-publish.
// ---------------------------------------------------------------------
TEST_CASE ("Re-publish: a view change with no new samples publishes within one call", "[T-035][republish][AC5]")
{
    Warm w (makeSettings());
    REQUIRE (w.first != nullptr);

    w.rig.s.viewFreqLow = 200.0f;
    w.rig.s.viewFreqHigh = 800.0f;
    CHECK (w.rig.process());

    auto f = w.rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->viewFreqLow == 200.0);
    CHECK (f->viewFreqHigh == 800.0);
    CHECK (f->sequence > w.first->sequence);
    CHECK (f->isDual);
    CHECK (f->hasSecondary);
    CHECK (f->hasSidechain);
    CHECK (f->overviewNumPoints == 256);
}

TEST_CASE ("Re-publish: no view change and no hop publishes nothing", "[T-035][republish][AC5]")
{
    Warm w (makeSettings());
    REQUIRE (w.first != nullptr);

    CHECK_FALSE (w.rig.process());
    CHECK (w.rig.read() == nullptr);

    // A view change publishes once; the same view again publishes nothing.
    w.rig.s.viewFreqLow = 300.0f;
    w.rig.s.viewFreqHigh = 900.0f;
    CHECK (w.rig.process());
    CHECK (w.rig.read() != nullptr);
    CHECK_FALSE (w.rig.process());
    CHECK (w.rig.read() == nullptr);

    // Smoothing, slope and point count changes alone are not triggers (documented: only view/home/freeze state).
    w.rig.s.smoothing = SmoothingMode::oct1_3;
    w.rig.s.slopeDbPerOct = 0.0f;
    CHECK_FALSE (w.rig.process());
    CHECK (w.rig.read() == nullptr);
}

TEST_CASE ("Re-publish: a home-range change publishes, a clamped-equal view change does not", "[T-035][republish][AC5]")
{
    Warm w (makeSettings());
    w.rig.s.freqHigh = 10000.0f;
    CHECK (w.rig.process());
    auto f = w.rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->viewFreqHigh == 10000.0);

    // Two different requests that clamp to the same view publish nothing the second time.
    w.rig.s.viewFreqLow = 0.0f;
    w.rig.s.viewFreqHigh = 50000.0f; // invalid for the pair (0, x): home range
    CHECK_FALSE (w.rig.process());
}

TEST_CASE ("Re-publish: nothing to publish before the analyser has produced a frame", "[T-035][republish][AC5]")
{
    Rig rig (makeSettings());
    rig.s.viewFreqLow = 200.0f;
    rig.s.viewFreqHigh = 800.0f;
    CHECK_FALSE (rig.process());
    CHECK (rig.read() == nullptr);
}

TEST_CASE ("Re-publish leaves analyser state untouched (twin comparison after more audio)", "[T-035][republish][AC5]")
{
    // A is re-published (view toggled repeatedly) between hops; B never is.
    // Averages, accumulators and the frame counter would diverge if a
    // re-publish changed any state.
    SpectrumSettings s = makeSettings (ChannelMode::lrDual, SecondaryMode::allTimeAverage, 2);
    Warm a (s, 5);
    Warm b (s, 5);
    REQUIRE (sameCurves (*a.first, *b.first));

    for (int i = 0; i < 6; ++i)
    {
        a.rig.s.viewFreqLow = (i % 2 == 0) ? 150.0f : 0.0f;
        a.rig.s.viewFreqHigh = (i % 2 == 0) ? 3000.0f : 0.0f;
        CHECK (a.rig.process());
        CHECK (a.rig.read() != nullptr);
    }
    a.rig.s.viewFreqLow = 0.0f;
    a.rig.s.viewFreqHigh = 0.0f;

    Source srcA (77, 1), srcB (77, 1);
    for (int round = 0; round < 4; ++round)
    {
        a.rig.run (20000, srcA);
        b.rig.run (20000, srcB);
        auto fa = a.rig.read();
        auto fb = b.rig.read();
        REQUIRE (fa != nullptr);
        REQUIRE (fb != nullptr);
        INFO ("round " << round);
        CHECK (sameCurves (*fa, *fb));
    }
}

TEST_CASE ("Re-publish with new audio pending publishes the live hop mapped on the new view", "[T-035][republish][AC5]")
{
    Warm w (makeSettings());
    w.rig.s.viewFreqLow = 500.0f;
    w.rig.s.viewFreqHigh = 2000.0f;
    w.rig.feed (8192);
    CHECK (w.rig.process());
    auto f = w.rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->viewFreqLow == 500.0);
    CHECK (f->viewFreqHigh == 2000.0);
}

// ---------------------------------------------------------------------
// AC6: freeze.
// ---------------------------------------------------------------------
TEST_CASE ("Freeze: entry publishes the snapshot with the new epoch; 10 s of new audio changes nothing", "[T-035][freeze][AC6]")
{
    Warm w (makeSettings());
    REQUIRE (w.first != nullptr);
    CHECK (w.first->freezeEpoch == 0);

    w.rig.s.frozen = true;
    w.rig.s.freezeEpoch = 7;
    CHECK (w.rig.process());
    auto entry = w.rig.read();
    REQUIRE (entry != nullptr);
    CHECK (entry->freezeEpoch == 7);
    CHECK (sameCurves (*entry, *w.first)); // what the user saw when freezing

    Source loud (321, 1);
    for (int second = 0; second < 10; ++second)
    {
        w.rig.feed (48000, loud);
        CHECK_FALSE (w.rig.process());
        CHECK (w.rig.read() == nullptr);
    }

    // A call with no new audio publishes nothing either.
    CHECK_FALSE (w.rig.process());
    CHECK (w.rig.read() == nullptr);
}

TEST_CASE ("Freeze: view changes re-map the snapshot (equal to a live re-map of the same state)", "[T-035][freeze][AC6]")
{
    const SpectrumSettings s = makeSettings();
    Warm frozen (s, 9);
    Warm twin (s, 9);
    REQUIRE (sameCurves (*frozen.first, *twin.first));

    frozen.rig.s.frozen = true;
    frozen.rig.s.freezeEpoch = 1;
    REQUIRE (frozen.rig.process());
    REQUIRE (frozen.rig.read() != nullptr);

    Source loud (5, 1);
    for (int i = 0; i < 4; ++i)
    {
        frozen.rig.feed (30000, loud);
        frozen.rig.process();
    }
    REQUIRE (frozen.rig.read() == nullptr);

    struct View { float lo, hi; };
    for (const View v : { View { 100.0f, 400.0f }, View { 1000.0f, 1100.0f }, View { 0.0f, 0.0f }, View { 8000.0f, 20000.0f } })
    {
        frozen.rig.s.viewFreqLow = v.lo;
        frozen.rig.s.viewFreqHigh = v.hi;
        twin.rig.s.viewFreqLow = v.lo;
        twin.rig.s.viewFreqHigh = v.hi;

        INFO ("view " << v.lo << ".." << v.hi);
        REQUIRE (frozen.rig.process());
        REQUIRE (twin.rig.process());
        auto ff = frozen.rig.read();
        auto tf = twin.rig.read();
        REQUIRE (ff != nullptr);
        REQUIRE (tf != nullptr);
        CHECK (ff->freezeEpoch == 1);
        CHECK (sameCurves (*ff, *tf));
    }
}

TEST_CASE ("Freeze: smoothing and slope changes do not alter frozen frames; home-range changes do", "[T-035][freeze][AC6]")
{
    const SpectrumSettings s = makeSettings();
    Warm frozen (s, 11);
    Warm twin (s, 11);

    frozen.rig.s.frozen = true;
    frozen.rig.s.freezeEpoch = 3;
    REQUIRE (frozen.rig.process());
    REQUIRE (frozen.rig.read() != nullptr);

    // Smoothing and slope alone: nothing published.
    frozen.rig.s.smoothing = SmoothingMode::oct1_3;
    frozen.rig.s.slopeDbPerOct = -6.0f;
    CHECK_FALSE (frozen.rig.process());
    CHECK (frozen.rig.read() == nullptr);

    // With a view change the new smoothing and slope are still ignored: the
    // twin keeps the original smoothing and slope.
    frozen.rig.s.viewFreqLow = 250.0f;
    frozen.rig.s.viewFreqHigh = 700.0f;
    twin.rig.s.viewFreqLow = 250.0f;
    twin.rig.s.viewFreqHigh = 700.0f;
    REQUIRE (frozen.rig.process());
    REQUIRE (twin.rig.process());
    {
        auto ff = frozen.rig.read();
        auto tf = twin.rig.read();
        REQUIRE (ff != nullptr);
        REQUIRE (tf != nullptr);
        CHECK (sameCurves (*ff, *tf));
    }

    // Home range change re-maps the frozen snapshot over the new home range.
    frozen.rig.s.freqLow = 50.0f;
    frozen.rig.s.freqHigh = 10000.0f;
    frozen.rig.s.viewFreqLow = 0.0f;
    frozen.rig.s.viewFreqHigh = 0.0f;
    twin.rig.s.freqLow = 50.0f;
    twin.rig.s.freqHigh = 10000.0f;
    twin.rig.s.viewFreqLow = 0.0f;
    twin.rig.s.viewFreqHigh = 0.0f;
    REQUIRE (frozen.rig.process());
    REQUIRE (twin.rig.process());
    auto ff = frozen.rig.read();
    auto tf = twin.rig.read();
    REQUIRE (ff != nullptr);
    REQUIRE (tf != nullptr);
    CHECK (ff->viewFreqLow == 50.0);
    CHECK (ff->viewFreqHigh == 10000.0);
    CHECK (ff->freezeEpoch == 3);
    CHECK (sameCurves (*ff, *tf));
}

TEST_CASE ("Freeze: fftSize, window and channelMode changes do not corrupt the snapshot", "[T-035][freeze][AC6]")
{
    const SpectrumSettings s = makeSettings();
    Warm frozen (s, 13);
    Warm twin (s, 13);

    frozen.rig.s.frozen = true;
    frozen.rig.s.freezeEpoch = 2;
    REQUIRE (frozen.rig.process());
    REQUIRE (frozen.rig.read() != nullptr);

    Source other (31, 1);

    frozen.rig.s.fftSize = 1024;
    frozen.rig.feed (20000, other);
    frozen.rig.process();
    frozen.rig.s.window = WindowFunctions::Type::hiRes;
    frozen.rig.feed (20000, other);
    frozen.rig.process();
    frozen.rig.s.channelMode = ChannelMode::mid;
    frozen.rig.feed (20000, other);
    frozen.rig.process();
    frozen.rig.s.fftSize = 16384;
    frozen.rig.s.channelMode = ChannelMode::msDual;
    frozen.rig.feed (20000, other);
    frozen.rig.process();
    CHECK (frozen.rig.read() == nullptr);

    frozen.rig.s.viewFreqLow = 400.0f;
    frozen.rig.s.viewFreqHigh = 1600.0f;
    twin.rig.s.viewFreqLow = 400.0f;
    twin.rig.s.viewFreqHigh = 1600.0f;
    REQUIRE (frozen.rig.process());
    REQUIRE (twin.rig.process());
    auto ff = frozen.rig.read();
    auto tf = twin.rig.read();
    REQUIRE (ff != nullptr);
    REQUIRE (tf != nullptr);
    CHECK (ff->freezeEpoch == 2);
    CHECK (sameCurves (*ff, *tf));
}

TEST_CASE ("Freeze: unfreeze publishes the live state on the next call", "[T-035][freeze][AC6]")
{
    const SpectrumSettings s = makeSettings();
    Warm frozen (s, 17);
    Warm live (s, 17); // never frozen, same cadence

    frozen.rig.s.frozen = true;
    frozen.rig.s.freezeEpoch = 1;
    REQUIRE (frozen.rig.process());
    REQUIRE (frozen.rig.read() != nullptr);

    Source srcF (99, 1), srcL (99, 1);
    for (int i = 0; i < 3; ++i)
    {
        frozen.rig.feed (16384, srcF);
        CHECK_FALSE (frozen.rig.process());
        live.rig.feed (16384, srcL);
        live.rig.process();
    }
    CHECK (frozen.rig.read() == nullptr);
    auto liveFrame = live.rig.read();
    REQUIRE (liveFrame != nullptr);

    frozen.rig.s.frozen = false;
    frozen.rig.s.freezeEpoch = 2;
    REQUIRE (frozen.rig.process()); // no new audio, no hop: published anyway
    auto f = frozen.rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->freezeEpoch == 2);
    CHECK (sameCurves (*f, *liveFrame));

    // And it goes back to normal: nothing without a change.
    CHECK_FALSE (frozen.rig.process());
}

TEST_CASE ("Freeze: every frame published while frozen carries the frozen epoch; a new epoch retakes the snapshot", "[T-035][freeze][AC6]")
{
    Warm w (makeSettings(), 19);
    w.rig.s.frozen = true;
    w.rig.s.freezeEpoch = 4;

    std::vector<uint32_t> epochs;
    auto collect = [&]
    {
        if (auto f = w.rig.read())
            epochs.push_back (f->freezeEpoch);
    };

    w.rig.process(); collect();
    Source other (4, 1);
    for (int i = 0; i < 5; ++i)
    {
        w.rig.s.viewFreqLow = 100.0f + 50.0f * static_cast<float> (i);
        w.rig.s.viewFreqHigh = 2000.0f + 100.0f * static_cast<float> (i);
        w.rig.feed (12000, other);
        w.rig.process();
        collect();
    }
    REQUIRE (epochs.size() == 6);
    for (auto e : epochs)
        CHECK (e == 4);

    // New epoch while still frozen: a fresh snapshot of the (meanwhile advanced) analysis.
    w.rig.s.freezeEpoch = 5;
    CHECK (w.rig.process());
    auto f = w.rig.read();
    REQUIRE (f != nullptr);
    CHECK (f->freezeEpoch == 5);
}

TEST_CASE ("Freeze: live frames carry the current epoch; freezing before any frame publishes nothing", "[T-035][freeze][AC6]")
{
    SECTION ("live frames")
    {
        SpectrumSettings s = makeSettings();
        s.freezeEpoch = 12;
        Rig rig (s);
        rig.run (kInitialFrames);
        auto f = rig.read();
        REQUIRE (f != nullptr);
        CHECK (f->freezeEpoch == 12);
    }
    SECTION ("freeze on an empty analyser")
    {
        SpectrumSettings s = makeSettings();
        s.frozen = true;
        s.freezeEpoch = 1;
        Rig rig (s);
        CHECK_FALSE (rig.process());
        rig.feed (30000);
        CHECK_FALSE (rig.process());
        CHECK (rig.read() == nullptr);
    }
}

TEST_CASE ("Freeze: analysis continues internally, so resetSecondary still takes effect after unfreeze", "[T-035][freeze][AC6]")
{
    const SpectrumSettings s = makeSettings (ChannelMode::mid, SecondaryMode::allTimeMax, 0);
    Warm frozen (s, 23);
    Warm twin (s, 23);

    frozen.rig.s.frozen = true;
    frozen.rig.s.freezeEpoch = 1;
    frozen.rig.process();
    frozen.rig.read();

    Source a (2, 1), b (2, 1);
    frozen.rig.feed (20000, a);
    frozen.rig.process();
    frozen.rig.engine.resetSecondary();
    twin.rig.feed (20000, b);
    twin.rig.process();
    twin.rig.engine.resetSecondary();

    frozen.rig.s.frozen = false;
    frozen.rig.s.freezeEpoch = 2;
    frozen.rig.feed (8192, a);
    twin.rig.feed (8192, b);
    frozen.rig.process();
    twin.rig.process();
    auto ff = frozen.rig.read();
    auto tf = twin.rig.read();
    REQUIRE (ff != nullptr);
    REQUIRE (tf != nullptr);
    CHECK (sameCurves (*ff, *tf));
}

// ---------------------------------------------------------------------
// AC7: allocation.
// ---------------------------------------------------------------------
TEST_CASE ("Allocation: freeze, unfreeze, view changes and re-publishes allocate nothing", "[T-035][alloc][AC7]")
{
    Warm w (makeSettings());
    Source other (8, 1);
    const Samples extra = other.next (16384);
    auto frame = newFrame();

    int published = 0;
    uint64_t allocations = 0;
    {
        ScopedAllocGuard guard;

        for (int i = 0; i < 4; ++i)
        {
            w.rig.s.viewFreqLow = 100.0f * static_cast<float> (i + 1);
            w.rig.s.viewFreqHigh = 3000.0f;
            published += w.rig.process() ? 1 : 0;
            w.rig.engine.readLatest (*frame);
        }

        w.rig.s.frozen = true;
        w.rig.s.freezeEpoch = 1;
        published += w.rig.process() ? 1 : 0;
        w.rig.engine.readLatest (*frame);

        w.rig.push (extra);
        published += w.rig.process() ? 1 : 0;

        for (int i = 0; i < 4; ++i)
        {
            w.rig.s.viewFreqLow = 50.0f * static_cast<float> (i + 1);
            w.rig.s.viewFreqHigh = 1000.0f + 500.0f * static_cast<float>(i);
            published += w.rig.process() ? 1 : 0;
            w.rig.engine.readLatest (*frame);
        }

        w.rig.s.freqHigh = 12000.0f;
        published += w.rig.process() ? 1 : 0;

        w.rig.s.freezeEpoch = 2; // retake while frozen
        published += w.rig.process() ? 1 : 0;

        w.rig.s.frozen = false;
        w.rig.s.freezeEpoch = 3;
        published += w.rig.process() ? 1 : 0;
        w.rig.engine.readLatest (*frame);

        w.rig.s.viewFreqLow = 0.0f;
        w.rig.s.viewFreqHigh = 0.0f;
        published += w.rig.process() ? 1 : 0;

        allocations = guard.count();
    }

    CHECK (published >= 12); // the scenario really exercised the publish paths
    CHECK (allocations == 0);
}

// ---------------------------------------------------------------------
// AC8: overview through the engine.
// ---------------------------------------------------------------------
TEST_CASE ("Overview: engine frame carries 256 points over the effective home range, independent of the view", "[T-035][overview][AC8]")
{
    SpectrumSettings s = makeSettings (ChannelMode::lrDual, SecondaryMode::allTimeAverage, 2);
    s.numPoints = 256;
    s.freqLow = 40.0f;
    s.freqHigh = 40000.0f; // limited to Nyquist (24000)
    Warm w (s, 29);
    REQUIRE (w.first != nullptr);

    CHECK (w.first->overviewNumPoints == 256);
    CHECK (std::abs (w.first->overviewFreqHz[0] / 40.0 - 1.0) < 1e-4);
    CHECK (std::abs (w.first->overviewFreqHz[255] / 24000.0 - 1.0) < 1e-4);
    for (int i = 1; i < 256; ++i)
        REQUIRE (w.first->overviewFreqHz[static_cast<size_t> (i)] > w.first->overviewFreqHz[static_cast<size_t> (i - 1)]);

    // Same density as the primary curve at Fit: matches within 0.1 dB.
    double worst = 0.0;
    for (int i = 0; i < 256; ++i)
        worst = std::max (worst, std::abs (static_cast<double> (w.first->overviewDb[static_cast<size_t> (i)]) - w.first->primaryDb[static_cast<size_t> (i)]));
    CHECK (worst <= 0.1);

    // Zooming leaves the overview untouched.
    w.rig.s.viewFreqLow = 1000.0f;
    w.rig.s.viewFreqHigh = 1300.0f;
    REQUIRE (w.rig.process());
    auto z = w.rig.read();
    REQUIRE (z != nullptr);
    CHECK (z->overviewNumPoints == 256);
    CHECK (sameFloats (z->overviewFreqHz.data(), w.first->overviewFreqHz.data(), 256));
    CHECK (sameFloats (z->overviewDb.data(), w.first->overviewDb.data(), 256));

    // A home-range change moves it.
    w.rig.s.freqHigh = 10000.0f;
    REQUIRE (w.rig.process());
    auto h = w.rig.read();
    REQUIRE (h != nullptr);
    CHECK (std::abs (h->overviewFreqHz[255] / 10000.0 - 1.0) < 1e-4);
}

TEST_CASE ("Overview: a frozen frame's overview is mapped from the snapshot", "[T-035][overview][freeze][AC8]")
{
    Warm w (makeSettings(), 31);
    w.rig.s.frozen = true;
    w.rig.s.freezeEpoch = 1;
    REQUIRE (w.rig.process());
    auto entry = w.rig.read();
    REQUIRE (entry != nullptr);
    CHECK (sameFloats (entry->overviewDb.data(), w.first->overviewDb.data(), 256));

    Source loud (6, 1);
    w.rig.feed (48000, loud);
    w.rig.s.viewFreqLow = 300.0f;
    w.rig.s.viewFreqHigh = 600.0f;
    REQUIRE (w.rig.process());
    auto z = w.rig.read();
    REQUIRE (z != nullptr);
    CHECK (sameFloats (z->overviewDb.data(), w.first->overviewDb.data(), 256));
}

// ---------------------------------------------------------------------
// AC9: timing (hidden; run in Release with [.perf]).
// ---------------------------------------------------------------------
TEST_CASE ("Performance: one re-map with every curve plus overview <= 1.0 ms median (Release)", "[T-035][.perf][AC9]")
{
    SpectrumSettings s = makeSettings (ChannelMode::lrDual, SecondaryMode::allTimeAverage, 2);
    s.fftSize = 65536;
    s.smoothing = SmoothingMode::oct1_3;
    s.numPoints = 2048;
    Rig rig (s, 41);
    rig.run (4 * 48000);
    REQUIRE (rig.read() != nullptr);

    std::vector<double> ms;
    for (int i = 0; i < 100; ++i)
    {
        // Minimum span (1/6 octave); the view moves every run so the grid is rebuilt.
        const float lo = 1000.0f * (1.0f + 0.01f * static_cast<float> (i % 50)) * (i % 2 == 0 ? 1.0f : 1.3f);
        rig.s.viewFreqLow = lo;
        rig.s.viewFreqHigh = lo * 1.1225f;

        const auto start = std::chrono::steady_clock::now();
        const bool published = rig.process();
        const auto end = std::chrono::steady_clock::now();
        REQUIRE (published);
        ms.push_back (std::chrono::duration<double, std::milli> (end - start).count());
        rig.read();
    }

    std::sort (ms.begin(), ms.end());
    const double median = 0.5 * (ms[49] + ms[50]);
    WARN ("re-map median " << median << " ms, min " << ms.front() << " ms, max " << ms.back() << " ms");
    CHECK (median <= 1.0);
}
