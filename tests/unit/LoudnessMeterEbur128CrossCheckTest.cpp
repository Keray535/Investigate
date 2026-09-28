// T-006 test oracle: cross-check LoudnessMeter's M, S, I and LRA against
// libebur128 (investigate_libebur128, test-only per docs/dependencies.md)
// on 60s of pink noise and a stepped-level signal, at 44.1/48/96 kHz.
// Required tolerance (task file, acceptance criteria): +-0.05 LU for I and
// M, +-0.1 LU for S and LRA.

#include "LoudnessMeterTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

extern "C"
{
#include <ebur128.h>
}

#include <cmath>
#include <limits>
#include <vector>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    struct Ebur128Result
    {
        double m = -std::numeric_limits<double>::infinity();
        double s = -std::numeric_limits<double>::infinity();
        double i = -std::numeric_limits<double>::infinity();
        double lra = 0.0;
    };

    // Feeds an interleaved stereo buffer (built from `left`/`right`) to a
    // fresh libebur128 state and returns its final M/S/I/LRA.
    Ebur128Result runEbur128 (const std::vector<float>& left, const std::vector<float>& right, double sampleRate)
    {
        ebur128_state* st = ebur128_init (2, static_cast<unsigned long> (sampleRate),
                                           EBUR128_MODE_I | EBUR128_MODE_LRA | EBUR128_MODE_HISTOGRAM);
        REQUIRE (st != nullptr);

        const size_t n = std::min (left.size(), right.size());
        std::vector<float> interleaved (n * 2);
        for (size_t idx = 0; idx < n; ++idx)
        {
            interleaved[idx * 2] = left[idx];
            interleaved[idx * 2 + 1] = right[idx];
        }

        const int addResult = ebur128_add_frames_float (st, interleaved.data(), n);
        REQUIRE (addResult == EBUR128_SUCCESS);

        Ebur128Result result;
        double v = 0.0;
        if (ebur128_loudness_momentary (st, &v) == EBUR128_SUCCESS) result.m = v;
        if (ebur128_loudness_shortterm (st, &v) == EBUR128_SUCCESS) result.s = v;
        if (ebur128_loudness_global (st, &v) == EBUR128_SUCCESS) result.i = v;
        if (ebur128_loudness_range (st, &v) == EBUR128_SUCCESS) result.lra = v;

        ebur128_destroy (&st);
        return result;
    }

    Ebur128Result runOwnMeter (const std::vector<float>& left, const std::vector<float>& right, double sampleRate)
    {
        LoudnessMeter meter;
        meter.prepare (sampleRate, 512, 2);
        feedStereo (meter, left, right, 512);

        Ebur128Result result;
        result.m = meter.getMomentaryLoudness();
        result.s = meter.getShortTermLoudness();
        result.i = meter.getIntegratedLoudness();
        result.lra = meter.getLoudnessRange();
        return result;
    }

    void buildSteppedSignal (std::vector<float>& left, std::vector<float>& right, double sampleRate)
    {
        double phase = 0.0;
        for (double db : { -40.0, -25.0, -18.0, -30.0, -22.0, -35.0 }) // 6 segments, 10s each = 60s
            appendStereoSineSegment (left, right, 1000.0, db, sampleRate, 10.0, phase);
    }
}

TEST_CASE ("libebur128 cross-check: 60s pink noise", "[dsp][loudness][ebur128][T-006]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    const int numSamples = static_cast<int> (sampleRate * 60.0);

    auto left = makePinkNoise (0.4, numSamples, 101);
    auto right = makePinkNoise (0.4, numSamples, 202);

    const auto ref = runEbur128 (left, right, sampleRate);
    const auto own = runOwnMeter (left, right, sampleRate);

    INFO ("rate=" << sampleRate << " ref(M,S,I,LRA)=" << ref.m << "," << ref.s << "," << ref.i << "," << ref.lra
                  << " own=" << own.m << "," << own.s << "," << own.i << "," << own.lra);
    CHECK_THAT (own.m, WithinAbs (ref.m, 0.05));
    CHECK_THAT (own.i, WithinAbs (ref.i, 0.05));
    CHECK_THAT (own.s, WithinAbs (ref.s, 0.1));
    CHECK_THAT (own.lra, WithinAbs (ref.lra, 0.1));
}

TEST_CASE ("libebur128 cross-check: 60s stepped-level signal", "[dsp][loudness][ebur128][T-006]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);

    std::vector<float> left, right;
    buildSteppedSignal (left, right, sampleRate);

    const auto ref = runEbur128 (left, right, sampleRate);
    const auto own = runOwnMeter (left, right, sampleRate);

    INFO ("rate=" << sampleRate << " ref(M,S,I,LRA)=" << ref.m << "," << ref.s << "," << ref.i << "," << ref.lra
                  << " own=" << own.m << "," << own.s << "," << own.i << "," << own.lra);
    CHECK_THAT (own.m, WithinAbs (ref.m, 0.05));
    CHECK_THAT (own.i, WithinAbs (ref.i, 0.05));
    CHECK_THAT (own.s, WithinAbs (ref.s, 0.1));
    CHECK_THAT (own.lra, WithinAbs (ref.lra, 0.1));
}
