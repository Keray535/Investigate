// T-024: EBU Tech 3341 true-peak cases 20-23 (docs/research/T-006-ebu-test-cases.md,
// Part A). These were left uncovered by T-007 (see the withdrawal comment block in
// tests/unit/TruePeakDetectorTest.cpp and docs/qa/T-007-unit.md's open question 1) -
// both earlier attempts compared an isolated fs/4 burst against a literal or
// self-referential expected value without first checking whether the *synthesis
// itself* reached 0dBFS after anti-alias filtering, so a synthesis bug could not be
// told apart from a TruePeakDetector bug.
//
// This file's design (per the T-024 task's own "Synthesis of cases 20 to 23"
// requirement, handed to unit-tester to design):
//   1. Build the literal EBU signal at a 4*fs working rate: a continuous fs/6,
//      0.5-amplitude background sine, with one full period of a fs/4, 1.0-amplitude
//      sine crossfaded in (phase-continuous by construction - see synthesizeCase2023
//      below), plus a short overall fade-in/out on the whole clip.
//   2. Anti-alias filter with a Kaiser-windowed-sinc low-pass, cutoff <= 0.45*fs
//      (of the target rate) in the working rate's own frequency scale, at least
//      90dB stopband attenuation - verified independently below (Test 0), not just
//      assumed from the design formula, because a Hann window (already used
//      elsewhere in this project, see MeterTestUtils.h's lowPassFilter()) cannot
//      reach anywhere near -90dB regardless of tap count (its first sidelobe floor
//      is a fixed ~-44dB) - a Kaiser window's beta is instead chosen for a target
//      attenuation, per Oppenheim & Schafer's/Crochiere & Rabiner's standard design
//      formula (see MeterTestUtils.h's designKaiserLowPass()).
//   3. Downsample to fs at offsets 0, 1, 2, 3 (every 4th working-rate sample,
//      starting at that offset).
//   4. Oracle validation FIRST (a separate assertion from the meter check): the
//      anti-alias-filtered 4*fs signal's own peak must be 0.0+-0.1dBFS. This is
//      exactly the check the two withdrawn T-007 attempts skipped.
//   5. Only then: TruePeakDetector's true-peak max on each downsampled-to-fs signal
//      must be within +0.2/-0.4dB of 0.0dBTP, at 48kHz.
//   6. libebur128's true peak on the same 4 downsampled signals, recorded for
//      information only (not a pass/fail gate, per the task).

#include "MeterTestUtils.h"
#include "dsp/DecibelUtils.h"
#include "dsp/TruePeakDetector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

extern "C"
{
#include <ebur128.h>
}

#include <cmath>
#include <iostream>
#include <vector>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kFs = 48000.0;              // target rate, matching the task's "at 48kHz".
    constexpr double kWorkingRate = 4.0 * kFs;    // 192kHz.
    constexpr double kFBackground = kFs / 6.0;    // 8000Hz - well below kWorkingRate's Nyquist (96kHz).
    constexpr double kFBurst = kFs / 4.0;         // 12000Hz.

    // Kaiser anti-alias filter design: cutoff at 0.1125 of the working rate
    // (= 0.45*fs, the task's own suggested bound, in the working rate's frequency
    // scale: 0.45*fs / (4*fs) = 0.1125), transition band out to 0.15 (27.6kHz,
    // still well inside the working rate's 96kHz Nyquist), 90dB stopband floor.
    constexpr double kCutoffNormalised = 0.1125;
    constexpr double kTransitionWidthNormalised = 0.0375;
    constexpr double kAttenuationDb = 90.0;

    /** Builds the literal EBU cases 20-23 signal at the 4*fs working rate, before
        anti-alias filtering: a continuous fs/6, 0.5-amplitude background sine, with
        one full period of a fs/4, 1.0-amplitude sine crossfaded in around its
        centre. Phase continuity at both splice edges is achieved by construction,
        not by searching for a coincidental zero-crossing alignment (the two
        frequencies chosen, fs/6 and fs/4, don't share one in general): the burst is
        evaluated as an infinite (not one-period-windowed-to-zero) sinusoid, and a
        raised-cosine crossfade blends continuously from the (continuous) background
        function to the (continuous) burst function and back - a weighted sum of two
        continuous functions with a continuous weight is itself continuous, which is
        what "phase-continuous... plus short fade-in/out" is checking for: no abrupt
        step at the splice, not any particular slope-matching. A short overall
        Hann-shaped fade-in/out is then applied to the whole clip, matching the
        pattern already used for cases 15-19 (applyFade() in TruePeakDetectorTest.cpp),
        so the extreme ends of the clip don't excite an FIR step-response transient
        unrelated to the case under test. */
    std::vector<float> synthesizeCase2023AtWorkingRate()
    {
        // 10ms clip at the working rate: long enough for ~13 full burst periods'
        // worth of neighbourhood around the splice (for the anti-alias filter's
        // finite width) and ~80 background cycles either side, short enough to
        // keep the test fast.
        const int totalSamples = static_cast<int> (std::llround (0.010 * kWorkingRate)); // 1920 at 192kHz.
        const int burstPeriodSamples = static_cast<int> (std::llround (kWorkingRate / kFBurst)); // 16, exact.
        const int burstStart = totalSamples / 2 - burstPeriodSamples / 2;                        // centred.
        const int fadeLen = burstPeriodSamples;                                                   // 16 samples: a full period's worth of crossfade, smooth enough to avoid Gibbs-ringing overshoot after the anti-alias filter.

        std::vector<double> composite (static_cast<size_t> (totalSamples));
        for (int n = 0; n < totalSamples; ++n)
        {
            const double bg = 0.5 * std::sin (2.0 * kMeterTestPi * kFBackground * n / kWorkingRate);
            const double burst = std::sin (2.0 * kMeterTestPi * kFBurst * (n - burstStart) / kWorkingRate);

            // Crossfade weight: 0 well before/after the burst, ramps 0->1 over
            // fadeLen samples ending exactly at burstStart, stays 1 for the full
            // burstPeriodSamples (the literal "single period... at amplitude
            // 1.00"), then ramps 1->0 over fadeLen samples.
            double w = 0.0;
            if (n >= burstStart - fadeLen && n < burstStart)
            {
                const double t = static_cast<double> (n - (burstStart - fadeLen)) / fadeLen;
                w = 0.5 * (1.0 - std::cos (kMeterTestPi * t));
            }
            else if (n >= burstStart && n < burstStart + burstPeriodSamples)
            {
                w = 1.0;
            }
            else if (n >= burstStart + burstPeriodSamples && n < burstStart + burstPeriodSamples + fadeLen)
            {
                const double t = static_cast<double> (n - (burstStart + burstPeriodSamples)) / fadeLen;
                w = 0.5 * (1.0 + std::cos (kMeterTestPi * t));
            }

            composite[static_cast<size_t> (n)] = (1.0 - w) * bg + w * burst;
        }

        std::vector<float> out (composite.size());
        for (size_t n = 0; n < composite.size(); ++n)
            out[n] = static_cast<float> (composite[n]);

        // Short (1ms) overall clip fade-in/out, matching cases 15-19's pattern
        // (10ms fade over an ~83ms clip there; 1ms over this 10ms clip is the
        // same rough proportion, hence "short" here).
        const int fadeSamples = std::min (static_cast<int> (out.size()) / 2,
                                           static_cast<int> (std::llround (kWorkingRate * 0.001)));
        for (int i = 0; i < fadeSamples; ++i)
        {
            const double g = 0.5 * (1.0 - std::cos (kMeterTestPi * static_cast<double> (i) / fadeSamples));
            out[static_cast<size_t> (i)] = static_cast<float> (out[static_cast<size_t> (i)] * g);
            out[out.size() - 1 - static_cast<size_t> (i)] = static_cast<float> (out[out.size() - 1 - static_cast<size_t> (i)] * g);
        }
        return out;
    }

    /** Every 4th sample of `in`, starting at `offset` (0..3) - the "downsample to fs
        with offsets 0-3" step. */
    std::vector<float> downsampleByFour (const std::vector<float>& in, int offset)
    {
        std::vector<float> out;
        out.reserve (in.size() / 4 + 1);
        for (size_t n = static_cast<size_t> (offset); n < in.size(); n += 4)
            out.push_back (in[n]);
        return out;
    }

    double runEbur128TruePeakMono (const std::vector<float>& mono, double sampleRate)
    {
        ebur128_state* st = ebur128_init (1, static_cast<unsigned long> (sampleRate), EBUR128_MODE_TRUE_PEAK);
        REQUIRE (st != nullptr);
        REQUIRE (ebur128_add_frames_float (st, mono.data(), mono.size()) == EBUR128_SUCCESS);
        double tp = 0.0;
        ebur128_true_peak (st, 0, &tp);
        ebur128_destroy (&st);
        return 20.0 * std::log10 (std::max (tp, 1.0e-12));
    }

    float measureTruePeakMaxDb (const std::vector<float>& mono, double sampleRate)
    {
        TruePeakDetector det;
        det.prepare (sampleRate, 512, 1);
        feedMono (det, mono, 512);
        return det.getTruePeakMaxDb (0);
    }
}

TEST_CASE ("T-024 EBU cases 20-23: Kaiser anti-alias filter reaches >=90dB stopband attenuation",
           "[dsp][truepeak][T-024][ebu]")
{
    // Verifies the filter DESIGN independently of the signal it will be applied
    // to (a synthesis defect in the filter itself must not be confused with a
    // synthesis defect in the signal, or a TruePeakDetector defect).
    const auto coeffs = designKaiserLowPass (kCutoffNormalised, kTransitionWidthNormalised, kAttenuationDb);
    INFO ("taps=" << coeffs.size());

    // Passband: near 0dB close to DC and just below the cutoff.
    CHECK_THAT (filterMagnitudeDb (coeffs, 0.0), WithinAbs (0.0, 0.05));
    CHECK_THAT (filterMagnitudeDb (coeffs, kFBackground / kWorkingRate), WithinAbs (0.0, 0.1));
    CHECK_THAT (filterMagnitudeDb (coeffs, kFBurst / kWorkingRate), WithinAbs (0.0, 0.1));

    // Stopband: at and beyond the transition edge, magnitude must be <= -90dB.
    const double stopbandEdge = kCutoffNormalised + kTransitionWidthNormalised;
    for (double f = stopbandEdge; f < 0.5; f += 0.01)
    {
        INFO ("f=" << f);
        CHECK (filterMagnitudeDb (coeffs, f) <= -kAttenuationDb);
    }
}

TEST_CASE ("T-024 EBU cases 20-23: oracle validation - anti-alias-filtered 4*fs signal peaks at 0.0+-0.1dBFS",
           "[dsp][truepeak][T-024][ebu]")
{
    // This must pass BEFORE the meter is judged against these signals (see the
    // file comment): if it fails, the defect is in this file's synthesis, not in
    // TruePeakDetector.
    const auto raw = synthesizeCase2023AtWorkingRate();
    const auto coeffs = designKaiserLowPass (kCutoffNormalised, kTransitionWidthNormalised, kAttenuationDb);
    const auto filtered = applyFirFilterCentered (raw, coeffs);

    float peak = 0.0f;
    for (float v : filtered)
        peak = std::max (peak, std::abs (v));
    const double peakDb = Decibels::linearToDb (peak);

    INFO ("oracle peak = " << peakDb << " dBFS (expect 0.0 +-0.1)");
    CHECK_THAT (peakDb, WithinAbs (0.0, 0.1));
}

TEST_CASE ("T-024 EBU cases 20-23: TruePeakDetector reads 0.0dBTP (+0.2/-0.4) at all 4 downsample offsets",
           "[dsp][truepeak][T-024][ebu]")
{
    const auto raw = synthesizeCase2023AtWorkingRate();
    const auto coeffs = designKaiserLowPass (kCutoffNormalised, kTransitionWidthNormalised, kAttenuationDb);
    const auto filtered = applyFirFilterCentered (raw, coeffs);

    float oraclePeak = 0.0f;
    for (float v : filtered)
        oraclePeak = std::max (oraclePeak, std::abs (v));
    const double oraclePeakDb = Decibels::linearToDb (oraclePeak);
    REQUIRE_THAT (oraclePeakDb, WithinAbs (0.0, 0.1)); // re-gate: don't judge the meter if the oracle itself is broken.

    // Cases 20/21/22/23 = offsets 0/1/2/3.
    const int offset = GENERATE (0, 1, 2, 3);
    const int caseId = 20 + offset;
    CAPTURE (caseId, offset);

    const auto downsampled = downsampleByFour (filtered, offset);
    const double tpDb = static_cast<double> (measureTruePeakMaxDb (downsampled, kFs));
    const double ref = runEbur128TruePeakMono (downsampled, kFs);

    INFO ("case " << caseId << ": own=" << tpDb << "dBTP, libebur128 (informational)=" << ref << "dBTP, oracle="
                   << oraclePeakDb << "dBFS");
    // Recorded for the QA report's results table via -s/--reporter console; also
    // print directly so it survives a filtered/quiet run.
    std::cout << "[T-024 case " << caseId << "] oracle=" << oraclePeakDb << "dBFS own=" << tpDb
               << "dBTP libebur128=" << ref << "dBTP\n";

    CHECK (tpDb <= 0.0 + 0.2);
    CHECK (tpDb >= 0.0 - 0.4);
}
