// T-007 acceptance criteria for MeterBank: blockCounter increments exactly
// once per process() call, requestReset() takes effect at the next
// process() call including from another thread with no data race, no
// allocation in process(), performance ([performance]-tagged per the
// project's noise-avoidance convention), and no NaN/Inf in the published
// snapshot for silence, DC, full-scale noise or denormal-range input.

#include "AllocGuard.h"
#include "MeterTestUtils.h"
#include "dsp/MeterBank.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

TEST_CASE ("MeterBank: blockCounter increments by exactly 1 per process() call", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    bank.prepare (48000.0, 512, 2);

    auto silence = makeSilence (512);
    for (uint64_t expected = 1; expected <= 50; ++expected)
    {
        const float* chans[2] = { silence.data(), silence.data() };
        bank.process (chans, 2, 512);
        CHECK (bank.getSnapshot().blockCounter == expected);
    }
}

TEST_CASE ("MeterBank: prepare() resets blockCounter and the published snapshot", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    bank.prepare (48000.0, 512, 2);

    auto loud = makeSineAt (1000.0, 1.0, 48000.0, 512, 0.0);
    const float* chans[2] = { loud.data(), loud.data() };
    for (int i = 0; i < 10; ++i)
        bank.process (chans, 2, 512);
    CHECK (bank.getSnapshot().blockCounter == 10);

    bank.prepare (48000.0, 512, 2);
    const auto snap = bank.getSnapshot();
    CHECK (snap.blockCounter == 0);
    CHECK (snap.samplePeak[0] == -std::numeric_limits<float>::infinity());
    CHECK (snap.peakMax[0] == -std::numeric_limits<float>::infinity());
}

TEST_CASE ("MeterBank: requestReset() from another thread takes effect at the next process() call, no data race", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    bank.prepare (48000.0, 512, 2);

    auto loud = makeSineAt (1000.0, 1.0, 48000.0, 512, 0.0);
    const float* chans[2] = { loud.data(), loud.data() };

    // Build up a non-trivial peakMax/clip count.
    for (int i = 0; i < 5; ++i)
        bank.process (chans, 2, 512);
    REQUIRE_THAT (bank.getSnapshot().peakMax[0], WithinAbs (0.0f, 0.05f));
    REQUIRE (bank.getSnapshot().sampleClipCount[0] > 0);

    std::atomic<bool> resetSent { false };
    std::thread resetter ([&bank, &resetSent]
    {
        bank.requestReset();
        resetSent.store (true, std::memory_order_release);
    });
    while (! resetSent.load (std::memory_order_acquire))
        std::this_thread::yield();
    resetter.join();

    // The reset flag was set before the next process() call below; that
    // call must consume it.
    auto quiet = makeSilence (512);
    const float* quietChans[2] = { quiet.data(), quiet.data() };
    bank.process (quietChans, 2, 512);

    const auto snap = bank.getSnapshot();
    CHECK (snap.peakMax[0] == -std::numeric_limits<float>::infinity());
    CHECK (snap.sampleClipCount[0] == 0);
}

TEST_CASE ("MeterBank: stress - concurrent process()/getSnapshot()/requestReset() never yields a torn or non-finite snapshot", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    bank.prepare (48000.0, 4096, 2);

    std::atomic<bool> stop { false };
    std::atomic<bool> sawBadSnapshot { false };

    // Writer: the audio thread, hammering process() with the smallest
    // possible block size (1 sample) to make the writer as fast as possible
    // relative to the reader, per the task's own stress-test requirement.
    std::thread writer ([&]
    {
        auto sine = makeSineAt (1000.0, 0.8, 48000.0, 1, 0.0);
        int64_t n = 0;
        while (! stop.load (std::memory_order_relaxed))
        {
            const float v = static_cast<float> (0.8 * std::sin (2.0 * kMeterTestPi * 1000.0 * static_cast<double> (n) / 48000.0));
            const float chan[1] = { v };
            const float* chans[2] = { chan, chan };
            bank.process (chans, 2, 1);
            ++n;
            if ((n % 10000) == 0)
                bank.requestReset();
        }
    });

    // Reader: hammers getSnapshot() concurrently and checks every field is
    // finite/-inf (never NaN) and every dB field is <= a generous sanity
    // bound - a torn seqlock read would very likely produce a value outside
    // these bounds or a NaN, since the writer's per-field stores are not
    // atomic and are only made safe by the seqlock's retry protocol.
    std::thread reader ([&]
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (1500);
        while (std::chrono::steady_clock::now() < deadline)
        {
            const auto snap = bank.getSnapshot();
            auto checkDbField = [&] (float v)
            {
                if (std::isnan (v) || v > 20.0f)
                    sawBadSnapshot.store (true, std::memory_order_relaxed);
            };
            for (int c = 0; c < 2; ++c)
            {
                checkDbField (snap.samplePeak[static_cast<size_t> (c)]);
                checkDbField (snap.peakMax[static_cast<size_t> (c)]);
                checkDbField (snap.rms[static_cast<size_t> (c)]);
                checkDbField (snap.truePeak[static_cast<size_t> (c)]);
                checkDbField (snap.truePeakMax[static_cast<size_t> (c)]);
            }
            if (std::isnan (snap.correlation) || std::abs (snap.correlation) > 1.0001f)
                sawBadSnapshot.store (true, std::memory_order_relaxed);
            if (std::isnan (snap.balance))
                sawBadSnapshot.store (true, std::memory_order_relaxed);
        }
    });

    reader.join();
    stop.store (true, std::memory_order_relaxed);
    writer.join();

    CHECK_FALSE (sawBadSnapshot.load());
}

TEST_CASE ("MeterBank: process() performs no heap allocation", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    bank.prepare (48000.0, 128, 2);

    auto sine = makeSineAt (1000.0, 0.8, 48000.0, 128, 0.0);
    const float* chans[2] = { sine.data(), sine.data() };

    // Warm-up: allow any lazy first-touch behaviour outside the guarded
    // region (there should be none, but keep the guard tight regardless).
    bank.process (chans, 2, 128);

    investigate::test::ScopedAllocGuard guard;
    for (int i = 0; i < 100; ++i)
        bank.process (chans, 2, 128);
    bank.setRmsWindowMs (500.0f);
    bank.requestReset();
    bank.process (chans, 2, 128);
    (void) bank.getSnapshot();

    CHECK (guard.count() == 0);
}

TEST_CASE ("Performance: MeterBank::process() <= 15us mean, <= 40us p99 per 128-sample stereo block at 48kHz (Release)", "[dsp][meterbank][performance][.perf][T-024]")
{
    // Asserts the roadmap Budgets table value for "MeterBank alone"
    // (mean <= 15us, p99 <= 40us per block), per T-026: timing tests must
    // assert the roadmap budget, not a task-level stretch target. T-024
    // additionally measured a tighter stretch target on the dev machine
    // (mean <= 10us, p99 <= 30us), now that TruePeakDetector's 4x tier
    // (44.1/48kHz, the rate this test exercises) uses its own exact
    // 12-tap-per-phase kernel instead of the T-007 25-tap-padded shared one
    // (see TruePeakDetector's class comment, "Polyphase structure,
    // per-tier kernel length (T-024)"). That stretch target is printed as
    // information only below, not asserted, since gating on it caused
    // intermittent failures under full-suite contention (T-025 open
    // question 2). Tagged `[performance]` (T-007's original tag, kept for
    // any tooling still filtering on it) and Catch2's hidden `[.perf]` tag
    // (T-026 / the roadmap's 2026-09-29 Decision), so this test is excluded
    // from the default run and from `catch_discover_tests`' CTest
    // registration, and is run on purpose via
    // `InvestigateUnitTests.exe "[perf]"` or by T-016.
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    constexpr int numBlocks = 20000;

    MeterBank bank;
    bank.prepare (sampleRate, blockSize, 2);

    auto sine = makeSineAt (1000.0, dbfsToAmplitude (-18.0), sampleRate, blockSize * numBlocks, 0.0);

    {
        const float* chans[2] = { sine.data(), sine.data() };
        bank.process (chans, 2, blockSize); // warm-up
    }

    std::vector<double> samplesUs;
    samplesUs.reserve (numBlocks - 1);
    for (int b = 1; b < numBlocks; ++b)
    {
        const float* chans[2] = { sine.data() + static_cast<size_t> (b) * blockSize,
                                   sine.data() + static_cast<size_t> (b) * blockSize };
        const auto start = std::chrono::steady_clock::now();
        bank.process (chans, 2, blockSize);
        const auto end = std::chrono::steady_clock::now();
        samplesUs.push_back (std::chrono::duration<double, std::micro> (end - start).count());
    }

    double total = 0.0;
    for (double v : samplesUs) total += v;
    const double meanUs = total / static_cast<double> (samplesUs.size());

    auto sorted = samplesUs;
    std::sort (sorted.begin(), sorted.end());
    const double p99Us = sorted[static_cast<size_t> (sorted.size() * 0.99)];

    INFO ("mean=" << meanUs << "us (roadmap budget 15us), p99=" << p99Us << "us (roadmap budget 40us)");
    // Information only, not asserted: T-024's tighter stretch target on this
    // dev machine.
    INFO ("stretch target (T-024, informational only): mean <= 10us ("
          << (meanUs <= 10.0 ? "met" : "missed") << "), p99 <= 30us ("
          << (p99Us <= 30.0 ? "met" : "missed") << ")");
#ifdef NDEBUG
    CHECK (meanUs <= 15.0);
    CHECK (p99Us <= 40.0);
#else
    WARN ("Debug build: budget not enforced. mean=" << meanUs << "us p99=" << p99Us << "us.");
#endif
}

TEST_CASE ("MeterBank: no NaN or Inf in the snapshot for silence, DC, full-scale noise or denormal input", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    bank.prepare (48000.0, 512, 2);

    SECTION ("silence")
    {
        auto s = makeSilence (48000);
        feedStereo (bank, s, s, 512);
    }
    SECTION ("DC full-scale")
    {
        auto d = makeDc (48000, 1.0f);
        feedStereo (bank, d, d, 512);
    }
    SECTION ("full-scale noise")
    {
        auto n1 = makeWhiteNoise (1.0, 48000, 55);
        auto n2 = makeWhiteNoise (1.0, 48000, 66);
        feedStereo (bank, n1, n2, 512);
    }
    SECTION ("denormal-range input")
    {
        auto d = makeDc (48000, 1.0e-40f);
        feedStereo (bank, d, d, 512);
    }

    const auto snap = bank.getSnapshot();
    auto checkFinite = [] (float v)
    {
        CHECK_FALSE (std::isnan (v));
        CHECK ((v == -std::numeric_limits<float>::infinity() || std::isfinite (v)));
    };
    for (int c = 0; c < 2; ++c)
    {
        checkFinite (snap.samplePeak[static_cast<size_t> (c)]);
        checkFinite (snap.peakMax[static_cast<size_t> (c)]);
        checkFinite (snap.rms[static_cast<size_t> (c)]);
        checkFinite (snap.truePeak[static_cast<size_t> (c)]);
        checkFinite (snap.truePeakMax[static_cast<size_t> (c)]);
    }
    checkFinite (snap.momentary);
    checkFinite (snap.shortTerm);
    checkFinite (snap.integrated);
    checkFinite (snap.loudnessRange);
    checkFinite (snap.maxMomentary);
    checkFinite (snap.maxShortTerm);
    checkFinite (snap.correlation);
    checkFinite (snap.balance);
}

TEST_CASE ("MeterBank: prepare() is repeatable and reset()+same input is deterministic", "[dsp][meterbank][T-007]")
{
    MeterBank bank;
    auto sine = makeSineAt (1000.0, 0.7, 48000.0, 4800, 0.0);

    MeterSnapshot first, second;
    for (int i = 0; i < 2; ++i)
    {
        bank.prepare (48000.0, 512, 2);
        feedStereo (bank, sine, sine, 512);
        (i == 0 ? first : second) = bank.getSnapshot();
    }

    CHECK_THAT (first.peakMax[0], WithinAbs (second.peakMax[0], 0.0001f));
    CHECK_THAT (first.rms[0], WithinAbs (second.rms[0], 0.0001f));
    CHECK_THAT (first.truePeakMax[0], WithinAbs (second.truePeakMax[0], 0.0001f));
    CHECK (first.blockCounter == second.blockCounter);
}
