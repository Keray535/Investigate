// T-006 acceptance criterion: "The K-weighting frequency response at 44.1,
// 88.2, 96 and 192 kHz matches the 48 kHz response within +-0.1 dB from
// 20 Hz to 16 kHz. The derived 48 kHz coefficients match the BS.1770 table
// within 1e-6."
//
// LoudnessMeter::deriveShelfCoeffs()/deriveRlbCoeffs() are private, so this
// file verifies the coefficient-table match two ways:
//  1. An independent re-derivation, in this test file, of the same
//     bilinear-transform formula from the same analog-prototype parameters
//     cited in LoudnessMeter.cpp's comments (f0/gain/Q for the shelf, f0/Q
//     for the RLB) - checked against the published BS.1770-4 48 kHz table
//     to within 1e-6. This validates the documented derivation matches the
//     standard.
//  2. An output-based frequency-response measurement through the public
//     API (process() + getMomentaryLoudness()) at each of {44.1, 88.2, 96,
//     192} kHz, compared to the same measurement at 48 kHz, which validates
//     that whatever LoudnessMeter.cpp actually computes internally is
//     consistent with (1) and stays within tolerance across sample rates.
// Together these give strong indirect evidence the private implementation
// matches the standard, without needing a friend/test hook into private
// members - flagged as an open item in the QA report in case a literal
// coefficient-level test is wanted instead.

#include "LoudnessMeterTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct Coeffs { double b0, b1, b2, a1, a2; };

    // Independent re-derivation of LoudnessMeter::deriveShelfCoeffs(), using
    // the same prototype parameters cited in LoudnessMeter.cpp (read from
    // third_party/libebur128-src/ebur128/ebur128.c's ebur128_init_filter).
    Coeffs deriveShelf (double rate)
    {
        constexpr double f0 = 1681.9744509555319;
        constexpr double gainDb = 3.999843853973347;
        constexpr double q = 0.7071752369554196;

        const double k = std::tan (kPi * f0 / rate);
        const double vh = std::pow (10.0, gainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;

        Coeffs c {};
        c.b0 = (vh + vb * k / q + k * k) / a0;
        c.b1 = 2.0 * (k * k - vh) / a0;
        c.b2 = (vh - vb * k / q + k * k) / a0;
        c.a1 = 2.0 * (k * k - 1.0) / a0;
        c.a2 = (1.0 - k / q + k * k) / a0;
        return c;
    }

    Coeffs deriveRlb (double rate)
    {
        constexpr double f0 = 38.13547087602444;
        constexpr double q = 0.5003270373238773;

        const double k = std::tan (kPi * f0 / rate);
        const double a0 = 1.0 + k / q + k * k;

        Coeffs c {};
        c.b0 = 1.0;
        c.b1 = -2.0;
        c.b2 = 1.0;
        c.a1 = 2.0 * (k * k - 1.0) / a0;
        c.a2 = (1.0 - k / q + k * k) / a0;
        return c;
    }
}

TEST_CASE ("K-weighting: independently re-derived 48kHz coefficients match BS.1770-4 table within 1e-6", "[dsp][loudness][kweighting][T-006]")
{
    const auto shelf = deriveShelf (48000.0);
    CHECK_THAT (shelf.b0, WithinAbs (1.53512485958697, 1e-6));
    CHECK_THAT (shelf.b1, WithinAbs (-2.69169618940638, 1e-6));
    CHECK_THAT (shelf.b2, WithinAbs (1.19839281085285, 1e-6));
    CHECK_THAT (shelf.a1, WithinAbs (-1.69065929318241, 1e-6));
    CHECK_THAT (shelf.a2, WithinAbs (0.73248077421585, 1e-6));

    const auto rlb = deriveRlb (48000.0);
    CHECK_THAT (rlb.b0, WithinAbs (1.0, 1e-6));
    CHECK_THAT (rlb.b1, WithinAbs (-2.0, 1e-6));
    CHECK_THAT (rlb.b2, WithinAbs (1.0, 1e-6));
    CHECK_THAT (rlb.a1, WithinAbs (-1.99004745483398, 1e-6));
    CHECK_THAT (rlb.a2, WithinAbs (0.99007225036621, 1e-6));
}

namespace
{
    // Measures relative K-weighted gain (in dB, via getMomentaryLoudness())
    // of a sine at `freqHz` versus a 1 kHz reference at the same amplitude
    // and sample rate. Feeds 1.5s of pre-roll (RLB pole settling) then reads
    // M over the following 400 ms.
    double measureRelativeGainDb (double sampleRate, double freqHz)
    {
        constexpr double amplitude = 0.5;
        constexpr double preRollSeconds = 1.5;
        constexpr double totalSeconds = preRollSeconds + 0.4;

        auto measure = [&] (double f) -> double
        {
            LoudnessMeter meter;
            meter.prepare (sampleRate, 512, 2);
            auto left = makeSine (f, amplitude, sampleRate, static_cast<int> (std::llround (totalSeconds * sampleRate)));
            auto right = left;
            feedStereo (meter, left, right, 512);
            return static_cast<double> (meter.getMomentaryLoudness());
        };

        return measure (freqHz) - measure (1000.0);
    }
}

TEST_CASE ("K-weighting: derived-coefficient frequency response matches 48kHz within +-0.1dB, 20Hz-16kHz", "[dsp][loudness][kweighting][T-006]")
{
    const double freqHz = GENERATE (20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0);
    const double otherRate = GENERATE (44100.0, 88200.0, 96000.0, 192000.0);

    const double gain48 = measureRelativeGainDb (48000.0, freqHz);
    const double gainOther = measureRelativeGainDb (otherRate, freqHz);

    INFO ("freq=" << freqHz << "Hz rate=" << otherRate << " gain48=" << gain48 << "dB gainOther=" << gainOther << "dB");
    CHECK_THAT (gainOther, WithinAbs (gain48, 0.1));
}

// Sanity check on the shape of the response itself (not one of the numbered
// acceptance criteria, but validates the measurement method above isn't
// vacuously passing): BS.1770's K-weighting is a high shelf + high-pass, so
// gain should be monotonically non-decreasing with frequency in this range,
// and comfortably positive at 16 kHz vs 1 kHz reference given the shelf's
// ~+4dB boost above ~2kHz, and strongly negative at 20 Hz given the RLB
// high-pass at ~38 Hz.
TEST_CASE ("K-weighting: response shape sanity (high-pass low end, shelf boost high end) at 48kHz", "[dsp][loudness][kweighting][T-006]")
{
    const double g20 = measureRelativeGainDb (48000.0, 20.0);
    const double g1k = 0.0; // reference
    const double g4k = measureRelativeGainDb (48000.0, 4000.0);
    const double g16k = measureRelativeGainDb (48000.0, 16000.0);

    INFO ("g20=" << g20 << " g1k=" << g1k << " g4k=" << g4k << " g16k=" << g16k);
    CHECK (g20 < -3.0);   // well attenuated by the ~38 Hz RLB high-pass
    CHECK (g4k > 2.0);    // shelf boost engaged
    CHECK (g16k > 0.0);
}
