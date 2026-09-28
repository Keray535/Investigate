// T-006 acceptance criteria that are not one of the transcribed EBU
// conformance cases: the -23/-26 dBFS stereo/mono criterion, sentinel
// values before enough data exists, reset()/determinism, real-time-safety
// (no allocation in process()), and the 1-hour simulated-run stability
// check.

#include "AllocGuard.h"
#include "LoudnessMeterTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSr = 48000.0;
}

// Task acceptance criterion: "A 1 kHz sine at -23 dBFS on both channels of
// a stereo input reads -23.0 +-0.1 LUFS for M, S and I after 3 s."
TEST_CASE ("Criterion: stereo -23 dBFS sine -> M,S,I = -23.0 LUFS after 3s", "[dsp][loudness][criteria][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    auto left = makeSine (1000.0, dbfsToAmplitude (-23.0), kSr, static_cast<int> (kSr * 3.0));
    auto right = left; // in phase, identical
    feedStereo (meter, left, right);

    REQUIRE_THAT (meter.getMomentaryLoudness(), WithinAbs (-23.0, 0.1));
    REQUIRE_THAT (meter.getShortTermLoudness(), WithinAbs (-23.0, 0.1));
    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-23.0, 0.1));
}

// "On a mono input with the same sine it reads -26.0 +-0.1 LUFS, following
// BS.1770's single-channel weight" (sum, not average, of per-channel mean
// square -> mono is ~3 dB / 10*log10(2) quieter than the same signal
// duplicated to stereo).
TEST_CASE ("Criterion: mono -23 dBFS sine -> M,S,I = -26.0 LUFS after 3s", "[dsp][loudness][criteria][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 1);

    auto mono = makeSine (1000.0, dbfsToAmplitude (-23.0), kSr, static_cast<int> (kSr * 3.0));
    feedMono (meter, mono);

    REQUIRE_THAT (meter.getMomentaryLoudness(), WithinAbs (-26.0, 0.1));
    REQUIRE_THAT (meter.getShortTermLoudness(), WithinAbs (-26.0, 0.1));
    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-26.0, 0.1));
}

TEST_CASE ("Sentinels: all getters read the documented sentinel before/after prepare(), never NaN", "[dsp][loudness][sentinel][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    CHECK (meter.getMomentaryLoudness() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getShortTermLoudness() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getIntegratedLoudness() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getMaxMomentary() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getMaxShortTerm() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getLoudnessRange() == 0.0f); // documented deviation, class comment

    CHECK_FALSE (std::isnan (meter.getMomentaryLoudness()));
    CHECK_FALSE (std::isnan (meter.getShortTermLoudness()));
    CHECK_FALSE (std::isnan (meter.getIntegratedLoudness()));
    CHECK_FALSE (std::isnan (meter.getLoudnessRange()));
    CHECK_FALSE (std::isnan (meter.getMaxMomentary()));
    CHECK_FALSE (std::isnan (meter.getMaxShortTerm()));

    // Silence never becomes NaN, and momentary/short-term reach their
    // "no data ever passed the absolute gate" negative-infinity state, not
    // NaN, once their window fills.
    auto silence = makeSilence (static_cast<int> (kSr * 4.0));
    std::vector<float> silenceR = silence;
    feedStereo (meter, silence, silenceR);
    CHECK_FALSE (std::isnan (meter.getMomentaryLoudness()));
    CHECK_FALSE (std::isnan (meter.getShortTermLoudness()));
    CHECK (meter.getIntegratedLoudness() == LoudnessMeter::negativeInfinity()); // absolute-gated out
    CHECK (meter.getLoudnessRange() == 0.0f);
}

TEST_CASE ("reset() restarts integrated/LRA/max to sentinels; same input after reset gives identical output", "[dsp][loudness][reset][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    auto left = makeSine (1000.0, dbfsToAmplitude (-23.0), kSr, static_cast<int> (kSr * 3.0));
    auto right = left;
    feedStereo (meter, left, right);

    REQUIRE (meter.getIntegratedLoudness() != LoudnessMeter::negativeInfinity());
    REQUIRE (meter.getMaxMomentary() != LoudnessMeter::negativeInfinity());
    REQUIRE (meter.getMaxShortTerm() != LoudnessMeter::negativeInfinity());

    const float firstIntegrated = meter.getIntegratedLoudness();
    const float firstMomentary = meter.getMomentaryLoudness();
    const float firstShortTerm = meter.getShortTermLoudness();
    const float firstMaxM = meter.getMaxMomentary();
    const float firstMaxS = meter.getMaxShortTerm();

    meter.reset();
    CHECK (meter.getMomentaryLoudness() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getShortTermLoudness() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getIntegratedLoudness() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getMaxMomentary() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getMaxShortTerm() == LoudnessMeter::negativeInfinity());
    CHECK (meter.getLoudnessRange() == 0.0f);

    // Determinism: same input after reset() gives identical output.
    feedStereo (meter, left, right);
    CHECK (meter.getIntegratedLoudness() == firstIntegrated);
    CHECK (meter.getMomentaryLoudness() == firstMomentary);
    CHECK (meter.getShortTermLoudness() == firstShortTerm);
    CHECK (meter.getMaxMomentary() == firstMaxM);
    CHECK (meter.getMaxShortTerm() == firstMaxS);
}

TEST_CASE ("prepare() can be called repeatedly and re-derives filters for the new sample rate", "[dsp][loudness][reset][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (44100.0, 512, 2);
    meter.prepare (96000.0, 4096, 1);
    meter.prepare (48000.0, 512, 2);

    auto left = makeSine (1000.0, dbfsToAmplitude (-23.0), kSr, static_cast<int> (kSr * 3.0));
    auto right = left;
    feedStereo (meter, left, right);
    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-23.0, 0.1));
}

TEST_CASE ("process() performs no heap allocation", "[dsp][loudness][realtime][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    auto left = makeSine (1000.0, dbfsToAmplitude (-18.0), kSr, static_cast<int> (kSr * 5.0));
    auto right = left;

    {
        ScopedAllocGuard guard;
        feedStereo (meter, left, right, 512);
        CHECK (guard.count() == 0);
    }
}

TEST_CASE ("process() performs no heap allocation across mono and odd block sizes", "[dsp][loudness][realtime][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (44100.0, 512, 1);

    auto mono = makeSine (997.0, dbfsToAmplitude (-10.0), 44100.0, 44100 * 2);

    {
        ScopedAllocGuard guard;
        for (int chunk : { 1, 17, 64, 4096 })
        {
            meter.reset();
            feedMono (meter, mono, chunk);
        }
        CHECK (guard.count() == 0);
    }
}

// 1-hour simulated run (fed far faster than real time - a tight loop, not
// wall-clock), stationary -23 dBFS 1 kHz stereo tone. Reuses a single
// pre-generated 1-second buffer (exactly 48 cycles at 1 kHz/48 kHz, so
// phase-continuous when looped) instead of generating 3600 s of samples, to
// keep this test's own runtime reasonable while still exercising 3600
// seconds of meter state. Checks: no allocation across the whole run
// (structurally guarantees the fixed-size histograms/ring buffers never
// grow), and the integrated value stays within +-0.01 LU of its settled
// value for the remainder of the run.
TEST_CASE ("1-hour simulated run: no allocation, integrated loudness stable within +-0.01 LU", "[dsp][loudness][longrun][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    const int oneSecond = static_cast<int> (kSr);
    auto left = makeSine (1000.0, dbfsToAmplitude (-23.0), kSr, oneSecond);
    auto right = left;

    constexpr int kSecondsToSimulate = 3600;
    float settledIntegrated = LoudnessMeter::negativeInfinity();
    double maxAbsDeviation = 0.0;

    {
        ScopedAllocGuard guard;
        for (int s = 0; s < kSecondsToSimulate; ++s)
        {
            feedStereo (meter, left, right, 512);
            if (s == 9) // first ~10s: past both the M/S fill delay and a few gating blocks
                settledIntegrated = meter.getIntegratedLoudness();
            else if (s > 9)
                maxAbsDeviation = std::max (maxAbsDeviation,
                    std::abs (static_cast<double> (meter.getIntegratedLoudness()) - static_cast<double> (settledIntegrated)));
        }
        CHECK (guard.count() == 0);
    }

    INFO ("settled I=" << settledIntegrated << " final I=" << meter.getIntegratedLoudness()
                       << " maxAbsDeviation=" << maxAbsDeviation);
    REQUIRE (settledIntegrated != LoudnessMeter::negativeInfinity());
    REQUIRE (maxAbsDeviation <= 0.01);
}

// Numerical safety: silence, DC, extreme amplitude jumps and denormal-range
// input never produce NaN/Inf-that-should-be-finite. (negativeInfinity() is
// itself -inf and is the *expected* sentinel value, not a defect - this
// test checks for NaN and for unexpected +inf/finite-but-wrong infinities.)
TEST_CASE ("Numerical safety: DC, denormals, amplitude jumps -> never NaN", "[dsp][loudness][safety][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    auto checkFiniteOrNegInf = [] (float v)
    {
        CHECK_FALSE (std::isnan (v));
        if (std::isinf (v))
            CHECK (v == LoudnessMeter::negativeInfinity()); // never +inf
    };

    // DC at full scale.
    std::vector<float> dc (static_cast<size_t> (kSr * 2), 1.0f);
    feedStereo (meter, dc, dc);
    checkFiniteOrNegInf (meter.getMomentaryLoudness());
    checkFiniteOrNegInf (meter.getShortTermLoudness());
    checkFiniteOrNegInf (meter.getIntegratedLoudness());
    CHECK_FALSE (std::isnan (meter.getLoudnessRange()));

    // Denormal-range input (~1e-40).
    meter.reset();
    std::vector<float> denorm (static_cast<size_t> (kSr * 2), 1e-40f);
    feedStereo (meter, denorm, denorm);
    checkFiniteOrNegInf (meter.getMomentaryLoudness());
    checkFiniteOrNegInf (meter.getShortTermLoudness());
    checkFiniteOrNegInf (meter.getIntegratedLoudness());

    // Sudden amplitude jump: silence -> full scale -> silence, repeatedly.
    meter.reset();
    for (int rep = 0; rep < 10; ++rep)
    {
        std::vector<float> loud (static_cast<size_t> (kSr * 0.2), 0.99f);
        std::vector<float> quiet (static_cast<size_t> (kSr * 0.2), 0.0f);
        feedStereo (meter, loud, loud);
        checkFiniteOrNegInf (meter.getMomentaryLoudness());
        feedStereo (meter, quiet, quiet);
        checkFiniteOrNegInf (meter.getMomentaryLoudness());
    }
    checkFiniteOrNegInf (meter.getIntegratedLoudness());
    CHECK_FALSE (std::isnan (meter.getLoudnessRange()));

    // Extreme out-of-range amplitude (beyond +-1.0 FS - a legal float
    // value even if not a legal audio sample).
    meter.reset();
    std::vector<float> extreme (static_cast<size_t> (kSr * 2), 1.0e6f);
    feedStereo (meter, extreme, extreme);
    checkFiniteOrNegInf (meter.getMomentaryLoudness());
    checkFiniteOrNegInf (meter.getShortTermLoudness());
    checkFiniteOrNegInf (meter.getIntegratedLoudness());
}

// numChannels passed to process() is clamped to the channel count passed to
// prepare() - a mono-prepared meter fed a "stereo" call (numChannels=2)
// must not read/write out of range, and should behave as if only the first
// (clamped) channel were processed.
TEST_CASE ("process() clamps numChannels to the prepared channel count", "[dsp][loudness][safety][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 1); // mono

    auto mono = makeSine (1000.0, dbfsToAmplitude (-23.0), kSr, static_cast<int> (kSr * 3.0));
    auto other = makeSine (1000.0, dbfsToAmplitude (-6.0), kSr, static_cast<int> (kSr * 3.0)); // would corrupt the reading if read out of range

    int pos = 0;
    const int numSamples = static_cast<int> (mono.size());
    while (pos < numSamples)
    {
        const int n = std::min (512, numSamples - pos);
        const float* chans[2] = { mono.data() + pos, other.data() + pos };
        meter.process (chans, 2, n); // numChannels=2 into a mono-prepared meter
        pos += n;
    }

    // Mono channel-weight criterion: -23 dBFS mono -> -26.0 LUFS. If the
    // second ("other", -6 dBFS) channel leaked in, this would read far
    // louder than -26.
    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-26.0, 0.1));
}
