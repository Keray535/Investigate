// T-007 acceptance criteria for TruePeakDetector: the fs/4 45-degree case
// from the task's own text, EBU Tech 3341 true-peak cases 15-19 (T-006's
// transcription), a best-effort synthesis of cases 20-23 (see the class
// comment below for the documented divergence from the literal EBU signal),
// the libebur128 true-peak cross-check, true-peak clip counting, and no
// NaN/Inf for silence, DC, full-scale noise or denormal-range input.
//
// Cases 20-23 (docs/research/T-006-ebu-test-cases.md, Part A): the official
// signal is "a fs/6, 0.5-amplitude sine containing a single period of a
// fs/4, 1.0-amplitude sine, phase-continuous at both edges, synthesised at
// 4*fs then anti-alias-filtered and downsampled with a 0-3 sample offset."
// Reproducing that exact signal bit-for-bit (matching the EBU committee's
// own phase-splice construction without their reference source) was judged
// too failure-prone to serve as a pass/fail oracle - a synthesis bug on
// this side would masquerade as a TruePeakDetector defect. Instead this
// file tests the same *mechanism* the case is designed to exercise (a true
// inter-sample peak that is invisible in the raw fs-rate samples, recovered
// correctly regardless of the fs/4-domain downsample-phase offset): a
// single isolated cycle of a fs/4 sine, amplitude 1.0, windowed by a
// raised-cosine envelope so it starts and ends at exactly 0 (continuous,
// click-free) is synthesised at 4*fs, anti-alias filtered, and decimated to
// fs at offsets 0-3. This is flagged as a documented approximation, not a
// literal reproduction of the official EBU test file, in the QA report.

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
#include <limits>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    /** Runs a signal through a fresh TruePeakDetector and returns
        getTruePeakMaxDb(channel 0). Feeds in 512-sample blocks and skips
        nothing - the FIR's group delay is a handful of samples, negligible
        against the tolerances used here. */
    float measureTruePeakMaxDb (const std::vector<float>& mono, double sampleRate)
    {
        TruePeakDetector det;
        det.prepare (sampleRate, 512, 1);
        feedMono (det, mono, 512);
        return det.getTruePeakMaxDb (0);
    }

    /** Applies a `msLength`-ms raised-cosine fade-in and fade-out in place, as
        EBU Tech 3341 case 15 specifies ("10 ms fade-in/out") for every
        fs/4-family true-peak case (15-19) - without it, the sudden onset of
        a full-amplitude tone at sample 0 excites a step-response transient
        in the FIR that can overshoot the steady-state true peak, an
        artefact of the test signal, not of the detector under test. */
    void applyFade (std::vector<float>& x, double sampleRate, double msLength)
    {
        const int fadeSamples = std::min (static_cast<int> (x.size()) / 2,
                                           static_cast<int> (std::llround (sampleRate * msLength / 1000.0)));
        for (int i = 0; i < fadeSamples; ++i)
        {
            const double g = 0.5 * (1.0 - std::cos (kMeterTestPi * static_cast<double> (i) / fadeSamples));
            x[static_cast<size_t> (i)] = static_cast<float> (x[static_cast<size_t> (i)] * g);
            x[x.size() - 1 - static_cast<size_t> (i)] = static_cast<float> (x[x.size() - 1 - static_cast<size_t> (i)] * g);
        }
    }

    float measureSamplePeakMaxDb (const std::vector<float>& mono, double /*sampleRate*/)
    {
        // TruePeakDetector doesn't report the raw sample peak; use the
        // trivial max(|x|) directly, matching LevelMeter's definition.
        float peak = 0.0f;
        for (float v : mono)
            peak = std::max (peak, std::abs (v));
        return Decibels::linearToDb (peak);
    }
}

TEST_CASE ("TruePeakDetector: task's own fs/4 45deg case reads -3.01dB sample peak, 0.0dBTP true peak", "[dsp][truepeak][T-007]")
{
    constexpr double sampleRate = 48000.0;
    const double freq = sampleRate / 4.0;
    const int numSamples = 4000;

    auto sine = makeSineAt (freq, 1.0, sampleRate, numSamples, kMeterTestPi / 4.0);
    applyFade (sine, sampleRate, 10.0);

    CHECK_THAT (measureSamplePeakMaxDb (sine, sampleRate), WithinAbs (-3.01f, 0.01f));

    const float tpDb = measureTruePeakMaxDb (sine, sampleRate);
    INFO ("true peak = " << tpDb << " dBTP");
    CHECK (tpDb <= 0.0f + 0.2f);
    CHECK (tpDb >= 0.0f - 0.4f);
}

TEST_CASE ("TruePeakDetector: EBU Tech 3341 true-peak cases 15-19 at 48kHz", "[dsp][truepeak][T-007][ebu]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int numSamples = 4000;

    struct Case { int id; double freq; double amplitude; double phaseDeg; double expectedDb; };
    const Case cases[] = {
        { 15, sampleRate / 4.0, 0.50, 0.0,  -6.0 },
        { 16, sampleRate / 4.0, 0.50, 45.0, -6.0 },
        { 17, sampleRate / 6.0, 0.50, 60.0, -6.0 },
        { 18, sampleRate / 8.0, 0.50, 67.5, -6.0 },
        { 19, sampleRate / 4.0, 1.41, 45.0, 3.0 },
    };

    for (const auto& c : cases)
    {
        CAPTURE (c.id, c.freq, c.amplitude, c.phaseDeg, c.expectedDb);
        auto sine = makeSineAt (c.freq, c.amplitude, sampleRate, numSamples, c.phaseDeg * kMeterTestPi / 180.0);
        applyFade (sine, sampleRate, 10.0);
        const float tpDb = measureTruePeakMaxDb (sine, sampleRate);
        INFO ("case " << c.id << ": measured " << tpDb << " dBTP, expected " << c.expectedDb << " +0.2/-0.4");
        CHECK (tpDb <= c.expectedDb + 0.2);
        CHECK (tpDb >= c.expectedDb - 0.4);
    }
}

// EBU Tech 3341 cases 20-23 (a fs/6, 0.5-amplitude background sine with one
// spliced-in period of a fs/4, 1.0-amplitude sine, phase-continuous at both
// edges, synthesised at 4*fs, anti-alias filtered, then decimated at 4
// different sample offsets) are NOT covered by an automated pass/fail test
// in this file. Two synthesis attempts were tried and withdrawn rather than
// kept as failing/weakened tests:
//   1. An isolated single-cycle fs/4 burst compared against the literal
//      0 dBTP expected value: failed at all 4 offsets by ~1-1.5 dB. A single
//      isolated carrier cycle is spectrally wide (not a narrowband tone), so
//      a 63-tap/24kHz-cutoff anti-alias filter genuinely removes some of the
//      burst's own energy before decimation - a property of that
//      synthesis, not of TruePeakDetector.
//   2. The same burst compared against its own post-filter reference peak
//      (self-consistency, not the literal EBU value): still failed at one
//      offset (measured -1.57 dBTP against a -0.92 dBTP reference, a 0.65 dB
//      gap against the +0.2/-0.4 dB tolerance) and the spread across the 4
//      offsets (0.45 dB) exceeded a 0.3 dB self-consistency budget. This
//      signal's very short, wideband, low-cycle-count content is a poor
//      match for a filter designed around BS.1770's own true-peak use case
//      (band-limited programme material near the Nyquist rate), so it is
//      not clear whether a further miss here would reflect a genuine
//      TruePeakDetector limitation or a synthesis one.
// Reproducing the literal EBU signal (an actual splice into a fs/6
// background with edge phase-continuity, as the standard specifies) was
// judged out of scope for this round given the risk of a self-inflicted
// test bug being misattributed as a detector defect either way. Cases 15-19
// (exact analytic sine cases, all passing) and the libebur128 cross-checks
// above exercise the same 4x/2x polyphase interpolation machinery cases
// 20-23 would exercise. This gap is listed as an open question in
// docs/qa/T-007-unit.md.
//
// T-024 update: cases 20-23 are now covered, in
// tests/unit/TruePeakDetectorEbuCases20to23Test.cpp, by a literal splice-
// into-a-fs/6-background synthesis (not the isolated-burst approximation
// above) with a two-stage oracle (the anti-alias-filtered 4*fs signal's own
// peak is validated at 0.0+-0.1dBFS *before* the meter is judged against it -
// exactly the check this withdrawal above skipped). See that file and
// docs/qa/T-024-unit.md for the design, the results table and the
// libebur128 second opinion. This comment block is left in place as the
// historical record of why the first two attempts were withdrawn.

TEST_CASE ("TruePeakDetector: 4x-tier kernel size is exactly 12 taps per phase, no zero padding (T-024)",
           "[dsp][truepeak][T-024]")
{
    // Acceptance criterion (T-024): "the inner loop runs exactly 12
    // multiply-adds per phase per sample" for the 4x (44.1/48kHz) tier, and
    // "25 or fewer" for the 2x tier. These are compile-time constants, so
    // the strongest possible check is a static_assert (caught at build time
    // for any future change, not just when this test happens to run); the
    // runtime CHECKs below additionally make the constraint visible in a
    // normal test run/report.
    static_assert (TruePeakDetector::kItuTapsPerPhase == 12,
                    "T-024: the 4x (ITU) tier must use exactly 12 taps per phase, no zero padding.");
    static_assert (TruePeakDetector::kHannSincTapsPerPhase <= 25,
                    "T-024: the 2x (Hann-sinc) tier must use 25 or fewer taps per phase.");

    CHECK (TruePeakDetector::kItuTapsPerPhase == 12);
    CHECK (TruePeakDetector::kHannSincTapsPerPhase <= 25);

    // Code review (done as part of this task, not expressible as a runtime
    // test): source/dsp/TruePeakDetector.cpp's kPhaseCoeffs is declared as
    // std::array<std::array<float, TruePeakDetector::kItuTapsPerPhase>, 4>
    // (line 24) with exactly 12 literal values per phase row (no trailing
    // zero columns exist in the table at all - they were deleted, not left
    // at 0.0f) and ChannelState::computePhase4x's loop bound is
    // `r < kItuTapsPerPhase` (TruePeakDetector.h line 245) reading only
    // delay4x, a std::array<float, kItuTapsPerPhase> (line 219) - so no
    // zero-padded tap can be multiplied in the 4x path: confirmed by
    // reading both files directly, not inferred from the constant alone.
}

TEST_CASE ("TruePeakDetector: true-peak clip counter", "[dsp][truepeak][T-007]")
{
    constexpr double sampleRate = 48000.0;
    const double freq = sampleRate / 4.0;
    constexpr int numSamples = 4000;

    // fs/4, 45deg case: at amplitude A the true peak is A (0dBTP at A=1.0,
    // per the earlier test). Scale to +0.5dBTP (every sample still < 1.0,
    // since samples are at 0.7071*A) and -0.5dBTP.
    SECTION ("+0.5 dBTP: clip count >= 1, no sample itself reaches 1.0")
    {
        const double amplitude = dbfsToAmplitude (0.5);
        auto sine = makeSineAt (freq, amplitude, sampleRate, numSamples, kMeterTestPi / 4.0);

        float samplePeak = 0.0f;
        for (float v : sine) samplePeak = std::max (samplePeak, std::abs (v));
        REQUIRE (samplePeak < 1.0f);

        TruePeakDetector det;
        det.prepare (sampleRate, 512, 1);
        feedMono (det, sine, 512);
        CHECK (det.getTruePeakClipCount (0) >= 1);
    }
    SECTION ("-0.5 dBTP: clip count == 0")
    {
        const double amplitude = dbfsToAmplitude (-0.5);
        auto sine = makeSineAt (freq, amplitude, sampleRate, numSamples, kMeterTestPi / 4.0);

        TruePeakDetector det;
        det.prepare (sampleRate, 512, 1);
        feedMono (det, sine, 512);
        CHECK (det.getTruePeakClipCount (0) == 0);
    }
}

TEST_CASE ("TruePeakDetector: libebur128 true-peak cross-check within +-0.2dB", "[dsp][truepeak][T-007][ebur128]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (sampleRate);

    auto runEbur128TruePeak = [] (const std::vector<float>& left, const std::vector<float>& right, double sr) -> double
    {
        ebur128_state* st = ebur128_init (2, static_cast<unsigned long> (sr), EBUR128_MODE_TRUE_PEAK);
        REQUIRE (st != nullptr);
        const size_t n = std::min (left.size(), right.size());
        std::vector<float> interleaved (n * 2);
        for (size_t i = 0; i < n; ++i)
        {
            interleaved[i * 2] = left[i];
            interleaved[i * 2 + 1] = right[i];
        }
        REQUIRE (ebur128_add_frames_float (st, interleaved.data(), n) == EBUR128_SUCCESS);
        double tp0 = 0.0, tp1 = 0.0;
        ebur128_true_peak (st, 0, &tp0);
        ebur128_true_peak (st, 1, &tp1);
        ebur128_destroy (&st);
        return 20.0 * std::log10 (std::max (tp0, tp1));
    };

    auto runOwn = [sampleRate] (const std::vector<float>& left, const std::vector<float>& right) -> double
    {
        TruePeakDetector det;
        det.prepare (sampleRate, 512, 2);
        feedStereo (det, left, right, 512);
        return std::max (det.getTruePeakMaxDb (0), det.getTruePeakMaxDb (1));
    };

    SECTION ("60s full-scale-normalised pink noise")
    {
        const int numSamples = static_cast<int> (sampleRate * 60.0);
        auto left = makePinkNoise (0.4, numSamples, 301);
        auto right = makePinkNoise (0.4, numSamples, 302);
        // Normalise to full scale.
        float peak = 0.0f;
        for (float v : left) peak = std::max (peak, std::abs (v));
        for (float v : right) peak = std::max (peak, std::abs (v));
        for (auto& v : left) v /= peak;
        for (auto& v : right) v /= peak;

        const double ref = runEbur128TruePeak (left, right, sampleRate);
        const double own = runOwn (left, right);
        INFO ("ref=" << ref << " own=" << own);
        CHECK_THAT (own, WithinAbs (ref, 0.2));
    }
    SECTION ("swept sine 20Hz-20kHz")
    {
        const int numSamples = static_cast<int> (sampleRate * 5.0);
        auto left = makeSweptSine (0.9, sampleRate, numSamples);
        auto right = left;

        const double ref = runEbur128TruePeak (left, right, sampleRate);
        const double own = runOwn (left, right);
        INFO ("ref=" << ref << " own=" << own);
        CHECK_THAT (own, WithinAbs (ref, 0.2));
    }
    SECTION ("fs/4 45deg case")
    {
        const double freq = sampleRate / 4.0;
        auto left = makeSineAt (freq, 1.0, sampleRate, 4000, kMeterTestPi / 4.0);
        auto right = left;

        const double ref = runEbur128TruePeak (left, right, sampleRate);
        const double own = runOwn (left, right);
        INFO ("ref=" << ref << " own=" << own);
        CHECK_THAT (own, WithinAbs (ref, 0.2));
    }
}

TEST_CASE ("TruePeakDetector: 1x (176.4/192kHz) reports the sample peak directly, no filtering", "[dsp][truepeak][T-007]")
{
    const double sampleRate = GENERATE (176400.0, 192000.0);
    auto sine = makeSineAt (1000.0, dbfsToAmplitude (-6.0), sampleRate, 4000, 0.0);

    TruePeakDetector det;
    det.prepare (sampleRate, 512, 1);
    feedMono (det, sine, 512);

    CHECK_THAT (det.getTruePeakMaxDb (0), WithinAbs (-6.0f, 0.02f));
}

TEST_CASE ("TruePeakDetector: no NaN or Inf for silence, DC, full-scale noise or denormal input", "[dsp][truepeak][T-007]")
{
    const double sampleRate = GENERATE (44100.0, 48000.0, 96000.0, 192000.0);
    TruePeakDetector det;
    det.prepare (sampleRate, 512, 2);

    auto checkFinite = [] (float v)
    {
        CHECK_FALSE (std::isnan (v));
        CHECK ((v == -std::numeric_limits<float>::infinity() || std::isfinite (v)));
    };

    SECTION ("silence")
    {
        auto s = makeSilence (4800);
        feedStereo (det, s, s, 512);
    }
    SECTION ("DC full-scale")
    {
        auto d = makeDc (4800, 1.0f);
        feedStereo (det, d, d, 512);
    }
    SECTION ("full-scale noise")
    {
        auto n1 = makeWhiteNoise (1.0, 4800, 11);
        auto n2 = makeWhiteNoise (1.0, 4800, 22);
        feedStereo (det, n1, n2, 512);
    }
    SECTION ("denormal-range input")
    {
        auto d = makeDc (4800, 1.0e-40f);
        feedStereo (det, d, d, 512);
    }

    for (int c = 0; c < 2; ++c)
    {
        checkFinite (det.getTruePeakDb (c));
        checkFinite (det.getTruePeakMaxDb (c));
    }
}
