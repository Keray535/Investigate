// EBU Tech 3341 v4.0 Table 1 conformance cases 10, 11, 13, 14 - Max
// Short-term / Max Momentary on file-based and live-meter stepped-level
// signals. Expected values transcribed from
// docs/research/T-006-ebu-test-cases.md Part A.
//
// Round 2 (T-006): cases 10/13 now run the full i=0..19 range (previously a
// representative 6-value subset, per round 1's open question 4), since the
// coder's round-2 fix moved Momentary/Short-term's internal recompute grid
// from 100 ms to 10 ms specifically to remove the offset-phase dependency
// these cases probe - full coverage is the direct regression check for that
// fix and is cheap to run. Cases 11/14 ("live meters", one continuous file)
// are run in full (all 20 segments) since that is what the case defines.

#include "LoudnessMeterTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSr = 48000.0;
}

// Case 10: 20 "files", each (i*0.15 s silence; 3 s @ -23 dBFS; 1 s silence).
// Expected: Max S = -23.0 LUFS for each segment/file.
TEST_CASE ("Tech 3341 case 10: file-based, i*0.15s silence + 3s/-23dBFS -> Max S = -23.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    for (int i = 0; i < 20; ++i)
    {
        LoudnessMeter meter;
        meter.prepare (kSr, 512, 2);

        std::vector<float> left, right;
        appendStereoSilenceSegment (left, right, kSr, i * 0.15);
        double phase = 0.0;
        appendStereoSineSegment (left, right, 1000.0, -23.0, kSr, 3.0, phase);
        appendStereoSilenceSegment (left, right, kSr, 1.0);
        feedStereo (meter, left, right);

        INFO ("i=" << i << " maxS=" << meter.getMaxShortTerm());
        REQUIRE_THAT (meter.getMaxShortTerm(), WithinAbs (-23.0, 0.1));
    }
}

// Case 13: 20 "files", each (i*20 ms silence; 400 ms @ -23 dBFS; 1 s silence).
// Expected: Max M = -23.0 LUFS for each segment/file.
TEST_CASE ("Tech 3341 case 13: file-based, i*20ms silence + 400ms/-23dBFS -> Max M = -23.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    for (int i = 0; i < 20; ++i)
    {
        LoudnessMeter meter;
        meter.prepare (kSr, 512, 2);

        std::vector<float> left, right;
        appendStereoSilenceSegment (left, right, kSr, i * 0.02);
        double phase = 0.0;
        appendStereoSineSegment (left, right, 1000.0, -23.0, kSr, 0.4, phase);
        appendStereoSilenceSegment (left, right, kSr, 1.0);
        feedStereo (meter, left, right);

        INFO ("i=" << i << " maxM=" << meter.getMaxMomentary());
        CHECK_THAT (meter.getMaxMomentary(), WithinAbs (-23.0, 0.1));
    }
}

// Case 11: live meters, 20 tones in one continuous file:
// (i*0.15 s silence; 3 s @ (-38+i) dBFS; (3 - i*0.15) s silence), i = 0..19.
// Expected: Max S sequence = -38, -37, ..., -19 LUFS (one value per segment,
// sampled at the instant each segment's 3 s tone has just finished, since
// the S window then exactly covers that tone).
TEST_CASE ("Tech 3341 case 11: live-meter stepped -38..-19 dBFS sequence -> Max S sequence", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    std::vector<double> segmentEndSeconds;
    double tCursor = 0.0;
    for (int i = 0; i < 20; ++i)
    {
        appendStereoSilenceSegment (left, right, kSr, i * 0.15);
        tCursor += i * 0.15;
        appendStereoSineSegment (left, right, 1000.0, -38.0 + i, kSr, 3.0, phase);
        tCursor += 3.0;
        appendStereoSilenceSegment (left, right, kSr, 3.0 - i * 0.15);
        tCursor += 3.0 - i * 0.15;
        segmentEndSeconds.push_back (tCursor);
    }

    // Sample the running getMaxShortTerm() (the case literally asks for the
    // "Max S sequence") right after each full segment (leading silence +
    // tone + trailing silence) has been processed, since maxShortTerm is
    // monotonic non-decreasing and each segment is louder than the last.
    const int chunk = static_cast<int> (kSr * 0.1); // 100 ms, the meter's own sub-block grid
    int pos = 0;
    const int numSamples = static_cast<int> (left.size());
    size_t nextSegmentIdx = 0;
    int failures = 0;
    while (pos < numSamples)
    {
        const int n = std::min (chunk, numSamples - pos);
        const float* chans[2] = { left.data() + pos, right.data() + pos };
        meter.process (chans, 2, n);
        pos += n;
        const double tSeconds = static_cast<double> (pos) / kSr;
        while (nextSegmentIdx < segmentEndSeconds.size() && tSeconds >= segmentEndSeconds[nextSegmentIdx] - 1e-9)
        {
            const double expected = -38.0 + static_cast<double> (nextSegmentIdx);
            const double measured = meter.getMaxShortTerm();
            INFO ("segment i=" << nextSegmentIdx << " expected maxS=" << expected << " measured maxS=" << measured);
            if (std::abs (measured - expected) > 0.1)
                ++failures;
            ++nextSegmentIdx;
        }
    }
    REQUIRE (nextSegmentIdx == segmentEndSeconds.size());
    INFO (failures << " of " << segmentEndSeconds.size() << " segments outside +-0.1 LUFS - see QA report for the per-segment table.");
    REQUIRE (failures == 0);
}

// Case 14: live meters, 20 tones in one file:
// (i*20 ms silence; 400 ms @ (-38+i) dBFS; (400 - i*20) ms silence), i = 0..19.
// Expected: Max M sequence = -38, -37, ..., -19 LUFS.
TEST_CASE ("Tech 3341 case 14: live-meter stepped -38..-19 dBFS sequence -> Max M sequence", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    std::vector<double> segmentEndSeconds;
    double tCursor = 0.0;
    for (int i = 0; i < 20; ++i)
    {
        appendStereoSilenceSegment (left, right, kSr, i * 0.02);
        tCursor += i * 0.02;
        appendStereoSineSegment (left, right, 1000.0, -38.0 + i, kSr, 0.4, phase);
        tCursor += 0.4;
        appendStereoSilenceSegment (left, right, kSr, 0.4 - i * 0.02);
        tCursor += 0.4 - i * 0.02;
        segmentEndSeconds.push_back (tCursor);
    }

    // Sample the running getMaxMomentary() ("Max M sequence") right after
    // each full segment.
    const int chunk = static_cast<int> (kSr * 0.1);
    int pos = 0;
    const int numSamples = static_cast<int> (left.size());
    size_t nextSegmentIdx = 0;
    int failures = 0;
    while (pos < numSamples)
    {
        const int n = std::min (chunk, numSamples - pos);
        const float* chans[2] = { left.data() + pos, right.data() + pos };
        meter.process (chans, 2, n);
        pos += n;
        const double tSeconds = static_cast<double> (pos) / kSr;
        while (nextSegmentIdx < segmentEndSeconds.size() && tSeconds >= segmentEndSeconds[nextSegmentIdx] - 1e-9)
        {
            const double expected = -38.0 + static_cast<double> (nextSegmentIdx);
            const double measured = meter.getMaxMomentary();
            INFO ("segment i=" << nextSegmentIdx << " expected maxM=" << expected << " measured maxM=" << measured);
            if (std::abs (measured - expected) > 0.1)
                ++failures;
            ++nextSegmentIdx;
        }
    }
    REQUIRE (nextSegmentIdx == segmentEndSeconds.size());
    INFO (failures << " of " << segmentEndSeconds.size() << " segments outside +-0.1 LUFS - see QA report for the per-segment table.");
    REQUIRE (failures == 0);
}
