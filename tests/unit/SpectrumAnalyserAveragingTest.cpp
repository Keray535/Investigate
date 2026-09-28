// T-004 acceptance criteria 8, 9, 10: averaging decay, secondary
// accumulators, and reset semantics.
//
// Decay methodology (criteria 8 and 9): prime the averaged curve to a known
// reference power with one full frame of a full-scale bin-centred sine,
// then push enough silence to fully flush the ring buffer of any sine
// content (one more fftSize worth of samples), and take *that* post-flush
// reading as the t=0 reference - this sidesteps the ambiguity of exactly
// how long the transition from "some sine, some silence in the window"
// takes. From there, push an exact, known number of additional hops of
// silence, compute the *actual* elapsed time from the exact hop count
// (rather than assuming avgTimeMs divides evenly into it), and compare the
// measured dB drop against the analytically exact -20*(elapsed/avgTime)
// prediction. The implementation's own alpha = 1-exp(-hop/tau) makes this
// an exact discrete formula (not an approximation) at hop granularity, so a
// tight 0.5 dB tolerance is meaningful.

#include "SpectrumTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    SpectrumAnalyser::Settings monoSettings (int fftSize, int overlap, double sampleRate, float avgTimeMs)
    {
        SpectrumAnalyser::Settings s;
        s.sampleRate = sampleRate;
        s.fftSize = fftSize;
        s.overlap = overlap;
        s.window = WindowFunctions::Type::hann;
        s.channelMode = ChannelMode::mid;
        s.avgTimeMs = avgTimeMs;
        s.secondary = SecondaryMode::realTimeMax;
        s.numMainChannels = 1;
        s.numSidechainChannels = 0;
        return s;
    }

    // Primes avg (and rtMax) to ~0 dBFS at `bin`, then flushes the ring
    // buffer clean with `fftSize` samples of silence, and returns the
    // (avgDb, rtMaxDb) reading right after the flush - the t=0 reference.
    struct DecayReference
    {
        double avgDb;
        double rtMaxDb;
    };

    DecayReference primeAndFlush (SpectrumAnalyser& analyser, const SpectrumAnalyser::Settings& s, int bin)
    {
        const double freq = analyser.getBinFrequency (bin);
        auto sine = makeSine (1.0, freq, s.sampleRate, s.fftSize);
        pushMono (analyser, sine); // exactly 1 frame -> avg primed to ~1.0 (0 dBFS)

        auto silenceFlush = makeSilence (s.fftSize);
        pushMono (analyser, silenceFlush); // fftSize/hop more frames -> ring now pure silence

        return { powerToDb (analyser.getPrimaryPower()[bin]), powerToDb (analyser.getRealTimeMaxPower()[bin]) };
    }
}

TEST_CASE ("Averaged level falls 20 dB at t = avgTime after a steady tone stops", "[dsp][spectrum][averaging][T-004]")
{
    const float avgTimeMs = GENERATE (100.0f, 1000.0f, 5000.0f);
    const double sampleRate = GENERATE (44100.0, 96000.0);
    const int overlap = GENERATE (2, 8);
    CAPTURE (avgTimeMs, sampleRate, overlap);

    const int fftSize = 2048;
    const int hop = fftSize / overlap;

    SpectrumAnalyser analyser;
    auto s = monoSettings (fftSize, overlap, sampleRate, avgTimeMs);
    analyser.prepare (s);

    const int bin = fftSize / 4;
    const auto ref = primeAndFlush (analyser, s, bin);
    CAPTURE (ref.avgDb);

    const double avgTimeSeconds = static_cast<double> (avgTimeMs) / 1000.0;
    const int64_t numHops = std::max<int64_t> (1, std::llround (avgTimeSeconds * sampleRate / hop));
    const auto extraSilence = makeSilence (static_cast<int> (numHops * hop));
    pushMono (analyser, extraSilence);

    const double actualElapsedSeconds = static_cast<double> (numHops * hop) / sampleRate;
    const double expectedDropDb = -20.0 * (actualElapsedSeconds / avgTimeSeconds);

    const double afterDb = powerToDb (analyser.getPrimaryPower()[bin]);
    const double actualDropDb = afterDb - ref.avgDb;
    CAPTURE (expectedDropDb, actualDropDb);

    REQUIRE_THAT (actualDropDb, WithinAbs (expectedDropDb, 0.5));
}

TEST_CASE ("Real-time max falls linearly in dB at 20 dB per avgTime after a tone stops", "[dsp][spectrum][averaging][T-004]")
{
    const float avgTimeMs = GENERATE (500.0f, 2000.0f);
    CAPTURE (avgTimeMs);

    const int fftSize = 2048;
    const int overlap = 4;
    const double sampleRate = 48000.0;
    const int hop = fftSize / overlap;

    SpectrumAnalyser analyser;
    auto s = monoSettings (fftSize, overlap, sampleRate, avgTimeMs);
    analyser.prepare (s);

    const int bin = fftSize / 4;
    const auto ref = primeAndFlush (analyser, s, bin);
    CAPTURE (ref.rtMaxDb);
    // rtMax reached 0 dBFS at the priming frame (frame 1) and can only have
    // fallen since - the flush period (overlap hops of partial-silence
    // frames) already applies some decay before this reference point is
    // taken, which is fine: the decay-rate assertion below is relative to
    // this reference, not to an assumed absolute 0 dB. Loose bound here
    // just guards against a grossly wrong (e.g. floor-clamped) starting point.
    REQUIRE (ref.rtMaxDb > -20.0);

    const double avgTimeSeconds = static_cast<double> (avgTimeMs) / 1000.0;
    const int64_t numHops = std::max<int64_t> (1, std::llround (avgTimeSeconds * sampleRate / hop));
    const auto extraSilence = makeSilence (static_cast<int> (numHops * hop));
    pushMono (analyser, extraSilence);

    const double actualElapsedSeconds = static_cast<double> (numHops * hop) / sampleRate;
    const double expectedDropDb = -20.0 * (actualElapsedSeconds / avgTimeSeconds);

    const double afterDb = powerToDb (analyser.getRealTimeMaxPower()[bin]);
    const double actualDropDb = afterDb - ref.rtMaxDb;
    CAPTURE (expectedDropDb, actualDropDb);

    REQUIRE_THAT (actualDropDb, WithinAbs (expectedDropDb, 0.5));
}

TEST_CASE ("All-time max never decreases until resetSecondary()", "[dsp][spectrum][averaging][T-004]")
{
    const int fftSize = 1024;
    SpectrumAnalyser analyser;
    auto s = monoSettings (fftSize, 4, 48000.0, 50.0f); // fast averaging so raw amplitude changes show up promptly
    analyser.prepare (s);

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);

    juce::Random rng (99);
    double runningMax = -1.0;

    for (int i = 0; i < 40; ++i)
    {
        const double amplitude = 0.05 + 0.9 * rng.nextDouble(); // varying level, never full-scale to leave headroom
        auto sine = makeSine (amplitude, freq, s.sampleRate, fftSize);
        pushMono (analyser, sine);

        const double current = analyser.getAllTimeMaxPower()[bin];
        REQUIRE (current >= runningMax - 1e-12); // monotonic non-decreasing
        runningMax = std::max (runningMax, current);
    }

    REQUIRE (runningMax > 1e-10); // sanity: some real signal was captured, not just the floor

    analyser.resetSecondary();
    const double afterReset = powerToDb (analyser.getAllTimeMaxPower()[bin]);
    REQUIRE (afterReset <= -199.0); // back at the -200 dB floor
}

TEST_CASE ("All-time average of 1s tone then 1s silence reads -3.01 dB relative to the tone", "[dsp][spectrum][averaging][T-004]")
{
    const int fftSize = 1024;
    const int overlap = 4;
    const double sampleRate = 48000.0;

    SpectrumAnalyser analyser;
    auto s = monoSettings (fftSize, overlap, sampleRate, 5.0f); // fast averaging: avg tracks raw almost immediately, per the task's own guidance
    s.secondary = SecondaryMode::allTimeAverage;
    analyser.prepare (s);

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);

    auto sine = makeSine (1.0, freq, sampleRate, static_cast<int> (sampleRate)); // 1 s
    pushMono (analyser, sine);
    const double refDb = powerToDb (analyser.getPrimaryPower()[bin]);
    CAPTURE (refDb);
    REQUIRE_THAT (refDb, WithinAbs (0.0, 0.5));

    auto silence = makeSilence (static_cast<int> (sampleRate)); // 1 s
    pushMono (analyser, silence);

    const double allTimeAvgDb = powerToDb (analyser.getAllTimeAveragePower()[bin]);
    CAPTURE (allTimeAvgDb);
    REQUIRE_THAT (allTimeAvgDb - refDb, WithinAbs (-3.01, 0.1));
}

TEST_CASE ("setSettings full-reset triggers: channelMode/fftSize/window/sampleRate clear secondary accumulators", "[dsp][spectrum][reset][T-004]")
{
    SECTION ("channelMode change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        s.numMainChannels = 2;
        s.channelMode = ChannelMode::mid;
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushStereo (analyser, sine, sine);
        REQUIRE (analyser.getAllTimeMaxPower()[512] > 1e-10);

        s.channelMode = ChannelMode::side;
        analyser.setSettings (s);
        REQUIRE_THAT (powerToDb (analyser.getAllTimeMaxPower()[512]), WithinAbs (-200.0, 1.0));
    }

    SECTION ("fftSize change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushMono (analyser, sine);
        REQUIRE (analyser.getAllTimeMaxPower()[512] > 1e-10);

        s.fftSize = 4096;
        analyser.setSettings (s);
        REQUIRE_THAT (powerToDb (analyser.getAllTimeMaxPower()[0]), WithinAbs (-200.0, 1.0));
    }

    SECTION ("window change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushMono (analyser, sine);
        REQUIRE (analyser.getAllTimeMaxPower()[512] > 1e-10);

        s.window = WindowFunctions::Type::blackman;
        analyser.setSettings (s);
        REQUIRE_THAT (powerToDb (analyser.getAllTimeMaxPower()[512]), WithinAbs (-200.0, 1.0));
    }

    SECTION ("sampleRate change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushMono (analyser, sine);
        REQUIRE (analyser.getAllTimeMaxPower()[512] > 1e-10);

        s.sampleRate = 44100.0;
        analyser.setSettings (s);
        REQUIRE_THAT (powerToDb (analyser.getAllTimeMaxPower()[512]), WithinAbs (-200.0, 1.0));
    }
}

TEST_CASE ("setSettings does NOT clear secondary accumulators for overlap/avgTimeMs/secondary-only changes", "[dsp][spectrum][reset][T-004]")
{
    SECTION ("overlap change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushMono (analyser, sine);
        const double before = analyser.getAllTimeMaxPower()[512];
        REQUIRE (before > 1e-10);

        s.overlap = 8;
        analyser.setSettings (s);
        REQUIRE (analyser.getAllTimeMaxPower()[512] == before);
    }

    SECTION ("avgTimeMs change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushMono (analyser, sine);
        const double before = analyser.getAllTimeMaxPower()[512];
        REQUIRE (before > 1e-10);

        s.avgTimeMs = 500.0f;
        analyser.setSettings (s);
        REQUIRE (analyser.getAllTimeMaxPower()[512] == before);
    }

    SECTION ("secondary mode change")
    {
        SpectrumAnalyser analyser;
        auto s = monoSettings (2048, 4, 48000.0, 50.0f);
        analyser.prepare (s);

        auto sine = makeSine (1.0, analyser.getBinFrequency (512), 48000.0, 2048);
        pushMono (analyser, sine);
        const double before = analyser.getAllTimeMaxPower()[512];
        REQUIRE (before > 1e-10);

        s.secondary = SecondaryMode::allTimeAverage;
        analyser.setSettings (s);
        REQUIRE (analyser.getAllTimeMaxPower()[512] == before); // underlying accumulator untouched, only which one is exposed changes
    }
}

TEST_CASE ("Public reset() clears framing/re-primes averaging but leaves secondary accumulators alone", "[dsp][spectrum][reset][T-004]")
{
    const int fftSize = 2048;
    SpectrumAnalyser analyser;
    auto s = monoSettings (fftSize, 4, 48000.0, 5000.0f); // slow averaging so a stale blend would be obviously wrong if not re-primed
    analyser.prepare (s);

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);

    auto loud = makeSine (1.0, freq, s.sampleRate, fftSize);
    pushMono (analyser, loud);
    const double allTimeMaxBefore = analyser.getAllTimeMaxPower()[bin];
    REQUIRE (allTimeMaxBefore > 1e-10);

    analyser.reset();

    // Secondary accumulators must survive reset() untouched.
    REQUIRE (analyser.getAllTimeMaxPower()[bin] == allTimeMaxBefore);

    // Averaging must be re-primed (hard-set), not blended: pushing a much
    // quieter tone right after reset() should read (close to) its own raw
    // level immediately, not a slow blend from the previous loud level.
    const double quietAmplitude = dbToAmplitude (-40.0);
    auto quiet = makeSine (quietAmplitude, freq, s.sampleRate, fftSize);
    pushMono (analyser, quiet);

    const double db = powerToDb (analyser.getPrimaryPower()[bin]);
    CAPTURE (db);
    REQUIRE_THAT (db, WithinAbs (-40.0, 0.5));
}

TEST_CASE ("Public resetSecondary() clears only the secondary accumulators, leaving the averaged curve alone", "[dsp][spectrum][reset][T-004]")
{
    const int fftSize = 2048;
    SpectrumAnalyser analyser;
    auto s = monoSettings (fftSize, 4, 48000.0, 5000.0f);
    analyser.prepare (s);

    const int bin = fftSize / 4;
    auto sine = makeSine (1.0, analyser.getBinFrequency (bin), s.sampleRate, fftSize);
    pushMono (analyser, sine);

    const double avgBefore = analyser.getPrimaryPower()[bin];
    REQUIRE (analyser.getAllTimeMaxPower()[bin] > 1e-10);

    analyser.resetSecondary();

    REQUIRE (analyser.getPrimaryPower()[bin] == avgBefore); // averaged curve untouched
    REQUIRE_THAT (powerToDb (analyser.getAllTimeMaxPower()[bin]), WithinAbs (-200.0, 1.0));
    REQUIRE_THAT (powerToDb (analyser.getRealTimeMaxPower()[bin]), WithinAbs (-200.0, 1.0));
    REQUIRE_THAT (powerToDb (analyser.getAllTimeAveragePower()[bin]), WithinAbs (-200.0, 1.0));
}
