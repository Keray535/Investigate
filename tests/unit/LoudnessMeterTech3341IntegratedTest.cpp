// EBU Tech 3341 v4.0 (Nov 2023) Table 1 conformance cases 1-5, 9, 12 -
// Integrated/Short-term/Momentary loudness on synthesisable sine signals.
// Expected values and tolerances transcribed from
// docs/research/T-006-ebu-test-cases.md Part A (NOT the flagged
// docs/research/T-002-dsp-references.md table).
//
// Case 6 (5.0-channel input) is out of scope: LoudnessMeter only supports
// 1-2 channels (mono/stereo) per the roadmap's bus layout and this task's
// explicit channel-weight scope - see LoudnessMeterTech3341Case6NotRunTest.cpp
// for the documented "not run" record.
// Cases 7/8 need the official "authentic programme" files (user decision,
// not downloaded) - not run, see docs/qa/T-006-unit.md.

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

// Case 1: stereo 1 kHz sine, -23.0 dBFS peak both channels, in phase, 20 s.
// Expected: M, S, I = -23.0 LUFS (+-0.1 LUFS).
TEST_CASE ("Tech 3341 case 1: stereo -23 dBFS sine -> M,S,I = -23.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -23.0, kSr, 20.0, phase);
    feedStereo (meter, left, right);

    INFO ("M=" << meter.getMomentaryLoudness() << " S=" << meter.getShortTermLoudness()
               << " I=" << meter.getIntegratedLoudness());
    REQUIRE_THAT (meter.getMomentaryLoudness(), WithinAbs (-23.0, 0.1));
    REQUIRE_THAT (meter.getShortTermLoudness(), WithinAbs (-23.0, 0.1));
    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-23.0, 0.1));
}

// Case 2: as #1 at -33.0 dBFS. Expected M,S,I = -33.0 LUFS.
TEST_CASE ("Tech 3341 case 2: stereo -33 dBFS sine -> M,S,I = -33.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -33.0, kSr, 20.0, phase);
    feedStereo (meter, left, right);

    REQUIRE_THAT (meter.getMomentaryLoudness(), WithinAbs (-33.0, 0.1));
    REQUIRE_THAT (meter.getShortTermLoudness(), WithinAbs (-33.0, 0.1));
    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-33.0, 0.1));
}

// Case 3: 10 s @ -36 dBFS, 60 s @ -23 dBFS, 10 s @ -36 dBFS. Expected I = -23.0 LUFS.
TEST_CASE ("Tech 3341 case 3: 10s/-36 + 60s/-23 + 10s/-36 -> I = -23.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -36.0, kSr, 10.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -23.0, kSr, 60.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -36.0, kSr, 10.0, phase);
    feedStereo (meter, left, right);

    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-23.0, 0.1));
}

// Case 4: 10s/-72, 10s/-36, 60s/-23, 10s/-36, 10s/-72. Expected I = -23.0 LUFS.
TEST_CASE ("Tech 3341 case 4: 5-tone stepped signal -> I = -23.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -72.0, kSr, 10.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -36.0, kSr, 10.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -23.0, kSr, 60.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -36.0, kSr, 10.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -72.0, kSr, 10.0, phase);
    feedStereo (meter, left, right);

    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-23.0, 0.1));
}

// Case 5: 20s/-26, 20.1s/-20, 20s/-26. Expected I = -23.0 LUFS.
TEST_CASE ("Tech 3341 case 5: 20s/-26 + 20.1s/-20 + 20s/-26 -> I = -23.0 LUFS", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -26.0, kSr, 20.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -20.0, kSr, 20.1, phase);
    appendStereoSineSegment (left, right, 1000.0, -26.0, kSr, 20.0, phase);
    feedStereo (meter, left, right);

    REQUIRE_THAT (meter.getIntegratedLoudness(), WithinAbs (-23.0, 0.1));
}

// Case 9: 5 repeats of (1.34 s @ -20 dBFS; 1.66 s @ -30 dBFS). Expected
// S = -23.0 LUFS, constant after 3 s.
TEST_CASE ("Tech 3341 case 9: alternating -20/-30 dBFS, 3s period -> S = -23.0 LUFS after 3s", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    for (int rep = 0; rep < 5; ++rep)
    {
        appendStereoSineSegment (left, right, 1000.0, -20.0, kSr, 1.34, phase);
        appendStereoSineSegment (left, right, 1000.0, -30.0, kSr, 1.66, phase);
    }
    feedStereo (meter, left, right);

    // Check S is within tolerance for every reading from the 3rd second
    // onward (steady-state periodic signal, so S should already be settled
    // at the first update at/after 3 s, and stay there).
    LoudnessMeter checker;
    checker.prepare (kSr, 512, 2);
    const int chunk = static_cast<int> (kSr * 0.1); // 100 ms, one sub-block
    int pos = 0;
    const int numSamples = static_cast<int> (left.size());
    bool checkedAny = false;
    while (pos < numSamples)
    {
        const int n = std::min (chunk, numSamples - pos);
        const float* chans[2] = { left.data() + pos, right.data() + pos };
        checker.process (chans, 2, n);
        pos += n;
        const double tSeconds = static_cast<double> (pos) / kSr;
        if (tSeconds >= 3.0 && checker.getShortTermLoudness() != LoudnessMeter::negativeInfinity())
        {
            INFO ("t=" << tSeconds << " S=" << checker.getShortTermLoudness());
            REQUIRE_THAT (checker.getShortTermLoudness(), WithinAbs (-23.0, 0.1));
            checkedAny = true;
        }
    }
    REQUIRE (checkedAny);
    REQUIRE_THAT (meter.getShortTermLoudness(), WithinAbs (-23.0, 0.1));
}

// Case 12: 25 repeats of (0.18 s @ -20 dBFS; 0.22 s @ -30 dBFS). Expected
// M = -23.0 LUFS, constant after 1 s. The 400 ms period exactly equals the
// momentary window, so once the window is full, M should be phase-invariant.
TEST_CASE ("Tech 3341 case 12: alternating -20/-30 dBFS, 400ms period -> M = -23.0 LUFS after 1s", "[dsp][loudness][tech3341][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    for (int rep = 0; rep < 25; ++rep)
    {
        appendStereoSineSegment (left, right, 1000.0, -20.0, kSr, 0.18, phase);
        appendStereoSineSegment (left, right, 1000.0, -30.0, kSr, 0.22, phase);
    }

    const int chunk = static_cast<int> (kSr * 0.1); // 100 ms
    int pos = 0;
    const int numSamples = static_cast<int> (left.size());
    bool checkedAny = false;
    while (pos < numSamples)
    {
        const int n = std::min (chunk, numSamples - pos);
        const float* chans[2] = { left.data() + pos, right.data() + pos };
        meter.process (chans, 2, n);
        pos += n;
        const double tSeconds = static_cast<double> (pos) / kSr;
        if (tSeconds >= 1.0 && meter.getMomentaryLoudness() != LoudnessMeter::negativeInfinity())
        {
            INFO ("t=" << tSeconds << " M=" << meter.getMomentaryLoudness());
            REQUIRE_THAT (meter.getMomentaryLoudness(), WithinAbs (-23.0, 0.1));
            checkedAny = true;
        }
    }
    REQUIRE (checkedAny);
}
