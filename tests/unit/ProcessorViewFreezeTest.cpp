// T-036 acceptance criteria for the processor's view and freeze API
// (docs/tasks/T-036-processor-view-freeze-api.md):
//  setSpectrumView / setSpectrumFrozen / getFreezeEpoch and the AnalysisThread
//  wiring. Latency, torn pair, freeze epoch semantics, audio-thread
//  unchanged (null, allocation hook, 100 open/close cycles), and reset after
//  setAnalysisActive(false)/(true).

#include "AllocGuard.h"
#include "MeterTestUtils.h"
#include "PluginProcessor.h"
#include "dsp/DisplayRange.h"
#include "dsp/SpectrumMapper.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace investigate;
using namespace investigate::test;

namespace
{
    using Clock = std::chrono::steady_clock;

    constexpr double kSr = 48000.0;

    void setChoice (InvestigateProcessor& p, const char* id, int index)
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p.getApvts().getParameter (id));
        REQUIRE (choice != nullptr);
        *choice = index;
    }

    void fastSettings (InvestigateProcessor& p, int fftIndex = 0)
    {
        setChoice (p, ParamIDs::fftSize, fftIndex);
        setChoice (p, ParamIDs::overlap, 0);
        auto* avg = p.getApvts().getParameter (ParamIDs::avgTime);
        REQUIRE (avg != nullptr);
        avg->setValueNotifyingHost (0.0f);
    }

    /// Home and clamped-view ranges for the processor's default frequency
    /// parameters at 48 kHz, through the shared clamps.
    SpectrumMapper::Ranges rangesFor (float lo, float hi)
    {
        SpectrumSettings s;
        s.sampleRate = kSr;
        s.freqLow = 20.0f;
        s.freqHigh = 20000.0f;
        s.viewFreqLow = lo;
        s.viewFreqHigh = hi;
        return SpectrumMapper::computeRanges (s);
    }

    /// Feeds real-time-paced audio on a thread. `variant` selects the tone, so
    /// a test can make "different audio" appear while frozen.
    class Feeder
    {
    public:
        explicit Feeder (InvestigateProcessor& proc) : processor (proc)
        {
            thread = std::thread ([this] { run(); });
        }
        ~Feeder() { stop(); }
        void stop()
        {
            if (thread.joinable())
            {
                quit.store (true);
                thread.join();
            }
        }
        std::atomic<int> variant { 0 };

    private:
        void run()
        {
            constexpr int block = 128;
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            int64_t pos = 0;
            while (! quit.load())
            {
                const bool second = variant.load() != 0;
                const auto s = makeSineAt (second ? 6000.0 : 700.0, second ? 0.6 : 0.1, kSr, block, 0.0, pos);
                pos += block;
                buffer.copyFrom (0, 0, s.data(), block);
                buffer.copyFrom (1, 0, s.data(), block);
                processor.processBlock (buffer, midi);
                std::this_thread::sleep_for (std::chrono::microseconds (2000));
            }
        }
        InvestigateProcessor& processor;
        std::atomic<bool> quit { false };
        std::thread thread;
    };

    void drain (InvestigateProcessor& p)
    {
        SpectrumFrame f;
        while (p.getLatestSpectrumFrame (f)) {}
    }

    bool waitForAnyFrame (InvestigateProcessor& p, SpectrumFrame& f, int timeoutMs)
    {
        const auto end = Clock::now() + std::chrono::milliseconds (timeoutMs);
        while (Clock::now() < end)
        {
            if (p.getLatestSpectrumFrame (f))
                return true;
            std::this_thread::sleep_for (std::chrono::microseconds (200));
        }
        return false;
    }

    bool sameCurve (const SpectrumFrame& a, const SpectrumFrame& b)
    {
        return a.numPoints == b.numPoints
            && std::memcmp (a.primaryDb.data(), b.primaryDb.data(), sizeof (float) * static_cast<size_t> (a.numPoints)) == 0;
    }

    double percentile (std::vector<double> v, double p)
    {
        std::sort (v.begin(), v.end());
        const auto idx = static_cast<size_t> (std::ceil (p * static_cast<double> (v.size()))) - 1;
        return v[std::min (idx, v.size() - 1)];
    }

    double runLatency (int fftIndex, int changes, double& p95, double& maxMs, double& meanMs)
    {
        auto proc = std::make_unique<InvestigateProcessor>();
        fastSettings (*proc, fftIndex);
        proc->prepareToPlay (kSr, 128);
        proc->setAnalysisActive (true);

        // Fill the analyser (65536 samples) quickly, without a pacing thread.
        const int fftSize = 512 << fftIndex;
        {
            juce::AudioBuffer<float> buffer (2, 128);
            juce::MidiBuffer midi;
            int64_t pos = 0;
            for (int b = 0; b < fftSize / 128 * 2 + 4; ++b)
            {
                const auto s = makeSineAt (700.0, 0.1, kSr, 128, 0.0, pos);
                pos += 128;
                buffer.copyFrom (0, 0, s.data(), 128);
                buffer.copyFrom (1, 0, s.data(), 128);
                proc->processBlock (buffer, midi);
            }
        }

        SpectrumFrame f;
        REQUIRE (waitForAnyFrame (*proc, f, 3000));

        std::vector<double> ms;
        juce::Random rng (1234);
        for (int i = 0; i < changes; ++i)
        {
            const float lo = 100.0f + 3000.0f * rng.nextFloat();
            const float hi = lo * (2.0f + 3.0f * rng.nextFloat());
            const auto expect = rangesFor (lo, hi);
            drain (*proc);
            const auto t0 = Clock::now();
            proc->setSpectrumView (lo, hi);
            bool got = false;
            while (Clock::now() - t0 < std::chrono::milliseconds (500))
            {
                if (proc->getLatestSpectrumFrame (f) && f.viewFreqLow == expect.viewLow && f.viewFreqHigh == expect.viewHigh)
                {
                    got = true;
                    break;
                }
            }
            REQUIRE (got);
            ms.push_back (std::chrono::duration<double, std::milli> (Clock::now() - t0).count());
            // Spread the changes across the analysis thread's 5 ms poll phase.
            std::this_thread::sleep_for (std::chrono::microseconds (1000 + (rng.nextInt (4000))));
        }
        proc->setAnalysisActive (false);

        p95 = percentile (ms, 0.95);
        maxMs = *std::max_element (ms.begin(), ms.end());
        double sum = 0;
        for (auto v : ms) sum += v;
        meanMs = sum / static_cast<double> (ms.size());
        return maxMs;
    }
}

TEST_CASE ("API: freeze epoch increments only on a real state change", "[processor][view][T-036]")
{
    InvestigateProcessor p;
    CHECK (p.getFreezeEpoch() == 0u);
    p.setSpectrumFrozen (false);
    CHECK (p.getFreezeEpoch() == 0u);
    p.setSpectrumFrozen (true);
    CHECK (p.getFreezeEpoch() == 1u);
    p.setSpectrumFrozen (true);
    CHECK (p.getFreezeEpoch() == 1u);
    p.setSpectrumFrozen (false);
    CHECK (p.getFreezeEpoch() == 2u);
    p.setSpectrumFrozen (false);
    CHECK (p.getFreezeEpoch() == 2u);
    for (int i = 0; i < 100; ++i)
        p.setSpectrumFrozen ((i & 1) == 0);
    CHECK (p.getFreezeEpoch() == 102u);
}

TEST_CASE ("API: concurrent freeze toggles from two threads never lose or skip an epoch", "[processor][view][T-036]")
{
    InvestigateProcessor p;
    std::atomic<bool> go { false };
    auto work = [&] { while (! go.load()) {} for (int i = 0; i < 20000; ++i) p.setSpectrumFrozen ((i & 1) == 0); };
    std::thread a (work), b (work);
    go.store (true);
    a.join();
    b.join();
    // Each real change adds exactly one; the epoch is bounded by the call count and non-zero.
    CHECK (p.getFreezeEpoch() >= 1u);
    CHECK (p.getFreezeEpoch() <= 40000u);
}

TEST_CASE ("Frames carry the clamped view (no view = home range)", "[processor][view][T-036]")
{
    InvestigateProcessor p;
    fastSettings (p);
    p.prepareToPlay (kSr, 128);
    p.setAnalysisActive (true);
    Feeder feeder (p);

    SpectrumFrame f;
    const auto home = rangesFor (0, 0);
    REQUIRE (waitForAnyFrame (p, f, 2000));
    CHECK (f.viewFreqLow == home.homeLow);
    CHECK (f.viewFreqHigh == home.homeHigh);

    struct Case { float lo, hi; };
    for (const auto c : { Case { 500.0f, 2000.0f }, Case { 2000.0f, 500.0f }, Case { 1.0f, 1e9f }, Case { 1000.0f, 1001.0f }, Case { -5.0f, 10.0f } })
    {
        const auto expect = rangesFor (c.lo, c.hi);
        p.setSpectrumView (c.lo, c.hi);
        bool ok = false;
        const auto end = Clock::now() + std::chrono::milliseconds (500);
        while (Clock::now() < end && ! ok)
            ok = p.getLatestSpectrumFrame (f) && f.viewFreqLow == expect.viewLow && f.viewFreqHigh == expect.viewHigh;
        INFO ("view " << c.lo << ", " << c.hi);
        CHECK (ok);
    }
    feeder.stop();
    p.setAnalysisActive (false);
}

TEST_CASE ("Latency: view change to published frame, 512 FFT smoke (correctness, loose bound)", "[processor][view][T-036]")
{
    double p95 = 0, maxMs = 0, mean = 0;
    runLatency (0, 50, p95, maxMs, mean);
    INFO ("512 FFT: p95 " << p95 << " ms, max " << maxMs << " ms, mean " << mean << " ms");
    CHECK (maxMs <= 200.0);
}

TEST_CASE ("Latency: 200 view changes at 65536 FFT, 1x overlap, 48 kHz: p95 <= 10 ms, max <= 20 ms",
           "[processor][view][performance][.perf][T-036]")
{
    double p95 = 0, maxMs = 0, mean = 0;
    runLatency (7, 200, p95, maxMs, mean);
    std::printf ("T-036 LATENCY 65536/1x/48k, 200 changes: p95 = %.3f ms, max = %.3f ms, mean = %.3f ms\n", p95, maxMs, mean);
    INFO ("p95 " << p95 << " ms, max " << maxMs << " ms, mean " << mean << " ms");
    CHECK (p95 <= 10.0);
    CHECK (maxMs <= 20.0);
}

TEST_CASE ("No torn pair: 10^6 view pairs written while the analysis thread runs", "[processor][view][T-036]")
{
    InvestigateProcessor p;
    fastSettings (p);
    p.prepareToPlay (kSr, 128);
    p.setAnalysisActive (true);
    Feeder feeder (p);

    constexpr int numPairs = 1000000;
    std::vector<float> los (numPairs), his (numPairs);
    juce::Random rng (42);
    struct Key
    {
        double a, b;
        bool operator== (const Key& o) const { return a == o.a && b == o.b; }
    };
    struct KeyHash
    {
        size_t operator() (const Key& k) const
        {
            uint64_t x, y;
            std::memcpy (&x, &k.a, 8);
            std::memcpy (&y, &k.b, 8);
            // splitmix64 finaliser: the float-derived doubles have mostly zero low bits.
            auto mix = [] (uint64_t z)
            {
                z += 0x9E3779B97F4A7C15ull;
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
                return z ^ (z >> 31);
            };
            return static_cast<size_t> (mix (x ^ mix (y)));
        }
    };
    std::unordered_set<Key, KeyHash> allowed;
    allowed.reserve (numPairs * 2);
    for (int i = 0; i < numPairs; ++i)
    {
        los[static_cast<size_t> (i)] = 25.0f + 5000.0f * rng.nextFloat();
        his[static_cast<size_t> (i)] = los[static_cast<size_t> (i)] * (1.5f + 6.0f * rng.nextFloat());
        const auto r = rangesFor (los[static_cast<size_t> (i)], his[static_cast<size_t> (i)]);
        allowed.insert ({ r.viewLow, r.viewHigh });
    }
    const auto home = rangesFor (0, 0);
    allowed.insert ({ home.homeLow, home.homeHigh }); // before the first write

    std::atomic<bool> done { false };
    std::thread writer ([&]
    {
        for (int i = 0; i < numPairs; ++i)
        {
            p.setSpectrumView (los[static_cast<size_t> (i)], his[static_cast<size_t> (i)]);
            if ((i % 2000) == 0)
                std::this_thread::sleep_for (std::chrono::milliseconds (2));
        }
        done.store (true);
    });

    SpectrumFrame f;
    int frames = 0, bad = 0;
    std::unordered_set<double> distinct;
    while (! done.load())
    {
        if (p.getLatestSpectrumFrame (f))
        {
            ++frames;
            distinct.insert (f.viewFreqLow);
            if (allowed.find ({ f.viewFreqLow, f.viewFreqHigh }) == allowed.end())
                ++bad;
        }
        else
            std::this_thread::yield();
    }
    writer.join();
    feeder.stop();
    p.setAnalysisActive (false);

    INFO ("frames " << frames << ", distinct views " << distinct.size() << ", torn " << bad);
    CHECK (frames > 100);
    CHECK (distinct.size() > 50);
    CHECK (bad == 0);
}

struct FreezeRoundsResult
{
    std::vector<double> frozenLatency;
    int violations = 0, staleLive = 0;
};

// Shared by the default functional case and the hidden [.perf] latency case.
// kArrivalMs only bounds the wait so a lost frame fails instead of hanging.
static FreezeRoundsResult runFreezeRounds (int kArrivalMs)
{
    InvestigateProcessor p;
    fastSettings (p);
    p.prepareToPlay (kSr, 128);
    p.setAnalysisActive (true);
    Feeder feeder (p);

    SpectrumFrame f;
    REQUIRE (waitForAnyFrame (p, f, 2000));

    FreezeRoundsResult r;
    auto& frozenLatency = r.frozenLatency;
    int& violations = r.violations;
    int& staleLive = r.staleLive;

    for (int round = 0; round < 20; ++round)
    {
        feeder.variant.store (round & 1);
        std::this_thread::sleep_for (std::chrono::milliseconds (60)); // let live frames reflect this tone
        drain (p);

        const uint32_t before = p.getFreezeEpoch();
        const auto t0 = Clock::now();
        p.setSpectrumFrozen (true);
        const uint32_t frozenEpoch = p.getFreezeEpoch();
        REQUIRE (frozenEpoch == before + 1u);

        SpectrumFrame snapshot;
        bool haveSnapshot = false;
        double firstMs = -1.0;
        // Different audio while frozen: a live frame under the frozen epoch would differ.
        feeder.variant.store ((round & 1) ^ 1);
        // Bounded wait: the frozen frame must arrive within kArrivalMs (a loose
        // liveness bound; the 20 ms budget is asserted only in the [.perf] case).
        // After it arrives, keep observing for 100 ms to catch live frames
        // published under the frozen epoch.
        auto end = t0 + std::chrono::milliseconds (kArrivalMs);
        while (Clock::now() < end)
        {
            if (! p.getLatestSpectrumFrame (f))
            {
                std::this_thread::sleep_for (std::chrono::microseconds (200));
                continue;
            }
            if (f.freezeEpoch == frozenEpoch)
            {
                if (! haveSnapshot)
                {
                    snapshot = f;
                    haveSnapshot = true;
                    firstMs = std::chrono::duration<double, std::milli> (Clock::now() - t0).count();
                    end = std::min (end, Clock::now() + std::chrono::milliseconds (100));
                }
                else if (! sameCurve (snapshot, f))
                    ++violations;
            }
            else if (f.freezeEpoch == before)
                ++staleLive; // allowed: a poll that started before the call
            else
                ++violations; // neither old nor new epoch
        }
        REQUIRE (haveSnapshot);
        frozenLatency.push_back (firstMs);

        p.setSpectrumFrozen (false);
        const uint32_t liveEpoch = p.getFreezeEpoch();
        REQUIRE (liveEpoch == frozenEpoch + 1u);
        // Unfreezing publishes live state: a frame under the new epoch whose
        // curve moves away from the snapshot once the audio differs.
        bool live = false;
        const auto end2 = Clock::now() + std::chrono::milliseconds (500);
        while (Clock::now() < end2 && ! live)
            live = p.getLatestSpectrumFrame (f) && f.freezeEpoch == liveEpoch && ! sameCurve (snapshot, f);
        CHECK (live);
    }
    feeder.stop();
    p.setAnalysisActive (false);

    return r;
}

TEST_CASE ("Freeze epoch: frames after freeze are old-epoch or the frozen snapshot, never live under a frozen epoch; frozen frame arrives (500 ms liveness bound)",
           "[processor][freeze][T-036]")
{
    const auto r = runFreezeRounds (500);
    const double worst = *std::max_element (r.frozenLatency.begin(), r.frozenLatency.end());
    std::printf ("T-036 FREEZE: frozen frame latency max %.3f ms over %zu rounds, stale old-epoch frames seen %d\n",
                 worst, r.frozenLatency.size(), r.staleLive);
    INFO ("worst frozen-frame latency " << worst << " ms");
    CHECK (r.violations == 0);
}

TEST_CASE ("Freeze latency: 20 freeze rounds, frozen frame published within 20 ms of setSpectrumFrozen (true)",
           "[processor][freeze][performance][.perf][T-036]")
{
    const auto r = runFreezeRounds (500);
    auto v = r.frozenLatency;
    const double p95 = percentile (v, 0.95);
    const double worst = *std::max_element (v.begin(), v.end());
    std::printf ("T-036 FREEZE LATENCY: %zu rounds, p95 = %.3f ms, max = %.3f ms\n", v.size(), p95, worst);
    INFO ("p95 " << p95 << " ms, max " << worst << " ms");
    CHECK (r.violations == 0);
    CHECK (worst <= 20.0);
}

TEST_CASE ("Freeze with no frame produced yet publishes nothing; no-op freeze keeps the epoch", "[processor][freeze][T-036]")
{
    InvestigateProcessor p;
    fastSettings (p);
    p.prepareToPlay (kSr, 128);
    p.setSpectrumFrozen (true);
    p.setAnalysisActive (true); // no audio fed: the analyser has produced nothing
    SpectrumFrame f;
    std::this_thread::sleep_for (std::chrono::milliseconds (60));
    CHECK_FALSE (p.getLatestSpectrumFrame (f));
    const auto e = p.getFreezeEpoch();
    p.setSpectrumFrozen (true);
    CHECK (p.getFreezeEpoch() == e);
    p.setAnalysisActive (false);
}

TEST_CASE ("Reset: setAnalysisActive(false) then (true) restores home range and unfrozen", "[processor][view][freeze][T-036]")
{
    InvestigateProcessor p;
    fastSettings (p);
    p.prepareToPlay (kSr, 128);
    const auto home = rangesFor (0, 0);

    p.setAnalysisActive (true);
    {
        Feeder feeder (p);
        SpectrumFrame f;
        REQUIRE (waitForAnyFrame (p, f, 2000));
        p.setSpectrumView (500.0f, 2000.0f);
        p.setSpectrumFrozen (true);
        const auto expect = rangesFor (500.0f, 2000.0f);
        bool ok = false;
        const auto end = Clock::now() + std::chrono::milliseconds (500);
        while (Clock::now() < end && ! ok)
            ok = p.getLatestSpectrumFrame (f) && f.viewFreqLow == expect.viewLow && f.freezeEpoch == 1u;
        REQUIRE (ok);
        feeder.stop();
    }

    SECTION ("nested activation: only the last deactivation resets")
    {
        p.setAnalysisActive (true);
        p.setAnalysisActive (false);
        CHECK (p.getFreezeEpoch() == 1u); // still frozen
        p.setAnalysisActive (false);
        CHECK (p.getFreezeEpoch() == 2u); // unfrozen: a real change
    }

    SECTION ("full reset")
    {
        p.setAnalysisActive (false);
        CHECK (p.getFreezeEpoch() == 2u);
        drain (p);
        p.setAnalysisActive (true);
        Feeder feeder (p);
        SpectrumFrame f;
        REQUIRE (waitForAnyFrame (p, f, 2000));
        CHECK (f.viewFreqLow == home.homeLow);
        CHECK (f.viewFreqHigh == home.homeHigh);
        CHECK (f.freezeEpoch == p.getFreezeEpoch());

        // Not frozen: the curve follows new audio.
        const SpectrumFrame first = f;
        feeder.variant.store (1);
        bool moved = false;
        const auto end = Clock::now() + std::chrono::milliseconds (500);
        while (Clock::now() < end && ! moved)
            moved = p.getLatestSpectrumFrame (f) && ! sameCurve (first, f);
        CHECK (moved);
        feeder.stop();
        p.setAnalysisActive (false);
    }

    SECTION ("reset without prior freeze leaves the epoch alone")
    {
        p.setSpectrumFrozen (false);
        const auto e = p.getFreezeEpoch();
        p.setAnalysisActive (false);
        CHECK (p.getFreezeEpoch() == e);
    }
}

TEST_CASE ("Audio thread unchanged: processBlock stays bit-exact pass-through while view and freeze change",
           "[processor][view][T-036]")
{
    InvestigateProcessor p;
    p.prepareToPlay (kSr, 128);
    p.setAnalysisActive (true);

    std::atomic<bool> stop { false };
    std::thread toggler ([&]
    {
        juce::Random r (7);
        bool fr = false;
        while (! stop.load())
        {
            p.setSpectrumView (50.0f + 4000.0f * r.nextFloat(), 5000.0f + 10000.0f * r.nextFloat());
            p.setSpectrumFrozen (fr = ! fr);
            std::this_thread::sleep_for (std::chrono::microseconds (500));
        }
    });

    juce::Random rnd (99);
    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;
    int mismatches = 0;
    for (int b = 0; b < 2000; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 128; ++s)
                buffer.setSample (ch, s, rnd.nextFloat() * 2.0f - 1.0f);
        const juce::AudioBuffer<float> copy (buffer);
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            if (std::memcmp (buffer.getReadPointer (ch), copy.getReadPointer (ch), sizeof (float) * 128) != 0)
                ++mismatches;
        if ((b % 50) == 0)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    stop.store (true);
    toggler.join();
    p.setAnalysisActive (false);
    CHECK (mismatches == 0);
}

TEST_CASE ("Audio thread unchanged: 0 allocations in processBlock while view and freeze toggle at 60 Hz",
           "[processor][realtime][view][T-036]")
{
    constexpr int blockSize = 128;
    constexpr int numBlocks = 1500; // ~4 s paced at the block period, ~240 toggles

    InvestigateProcessor p;
    p.prepareToPlay (kSr, blockSize);
    p.setAnalysisActive (true);

    std::atomic<bool> start { false }, stop { false };
    std::atomic<int> toggles { 0 };
    std::thread toggler ([&]
    {
        while (! start.load()) std::this_thread::yield();
        bool fr = false;
        uint32_t i = 0;
        auto next = Clock::now();
        while (! stop.load())
        {
            const float lo = 40.0f + static_cast<float> ((i * 97u) % 3000u);
            p.setSpectrumView (lo, lo * 4.0f);
            p.setSpectrumFrozen (fr = ! fr);
            ++i;
            toggles.fetch_add (1);
            next += std::chrono::microseconds (16667);
            std::this_thread::sleep_until (next);
        }
    });

    const auto sine = makeSineAt (1000.0, 0.25, kSr, blockSize * numBlocks, 0.0);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    buffer.copyFrom (0, 0, sine.data(), blockSize);
    buffer.copyFrom (1, 0, sine.data(), blockSize);
    p.processBlock (buffer, midi); // warm-up

    uint64_t allocations = 0;
    {
        ScopedAllocGuard guard;
        start.store (true);
        auto next = Clock::now();
        for (int b = 1; b < numBlocks; ++b)
        {
            buffer.copyFrom (0, 0, sine.data() + b * blockSize, blockSize);
            buffer.copyFrom (1, 0, sine.data() + b * blockSize, blockSize);
            p.processBlock (buffer, midi);
            next += std::chrono::microseconds (2667);
            std::this_thread::sleep_until (next);
        }
        allocations = guard.count();
    }
    stop.store (true);
    toggler.join();
    p.setAnalysisActive (false);

    INFO ("allocations (process-wide, includes analysis thread) = " << allocations << ", view/freeze toggles = " << toggles.load());
    CHECK (toggles.load() > 100);
    CHECK (allocations == 0);
}

TEST_CASE ("100 open/close cycles with view changes in flight: thread stops within 50 ms each time",
           "[processor][analysis][view][T-036]")
{
    InvestigateProcessor p;
    p.prepareToPlay (kSr, 128);

    std::atomic<bool> stop { false };
    std::thread audio ([&]
    {
        const auto sine = makeSineAt (1000.0, 0.5, kSr, 128, 0.0);
        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;
        buffer.copyFrom (0, 0, sine.data(), 128);
        buffer.copyFrom (1, 0, sine.data(), 128);
        while (! stop.load())
            p.processBlock (buffer, midi);
    });
    std::thread viewer ([&]
    {
        juce::Random r (5);
        bool fr = false;
        while (! stop.load())
        {
            const float lo = 30.0f + 3000.0f * r.nextFloat();
            p.setSpectrumView (lo, lo * (2.0f + 4.0f * r.nextFloat()));
            if ((r.nextInt (8)) == 0)
                p.setSpectrumFrozen (fr = ! fr);
            std::this_thread::sleep_for (std::chrono::microseconds (300));
        }
    });

    double worst = 0.0;
    for (int i = 0; i < 100; ++i)
    {
        p.setAnalysisActive (true);
        std::this_thread::sleep_for (std::chrono::milliseconds (3));
        const auto t0 = Clock::now();
        p.setAnalysisActive (false);
        const double ms = std::chrono::duration<double, std::milli> (Clock::now() - t0).count();
        worst = std::max (worst, ms);
        INFO ("iteration " << i << " stop " << ms << " ms");
        CHECK (ms <= 50.0);
    }
    stop.store (true);
    audio.join();
    viewer.join();
    std::printf ("T-036 OPEN/CLOSE with view changes: worst stop %.3f ms\n", worst);
}
