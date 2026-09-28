// EBU Tech 3342 v4 (Nov 2023) Table 1 conformance cases 1-4 - Loudness
// Range (LRA). Expected values and the +-1 LU tolerance transcribed from
// docs/research/T-006-ebu-test-cases.md Part B (NOT the flagged
// docs/research/T-002-dsp-references.md table). Cases 5/6 need the official
// "authentic programme" files - see LoudnessMeterTech3341NotRunTest.cpp.
//
// Tech 3342's relative gate is -20 LU (not Integrated Loudness's -10 LU) -
// these tests exercise that code path (LoudnessMeter::addLraSample /
// recomputeLoudnessRange), distinct from the Integrated Loudness gate path
// exercised by the Tech 3341 tests.

#include "LoudnessMeterTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSr = 48000.0;

    // Tech 3342 §4 recommends >=1.5s trailing silence for file-based
    // measurement, to flush the 3s short-term window latency.
    constexpr double kTrailingSilenceSeconds = 2.0;
}

TEST_CASE ("Tech 3342 case 1: -20/-30 dBFS, 20s each -> LRA = 10 LU", "[dsp][loudness][tech3342][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -20.0, kSr, 20.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -30.0, kSr, 20.0, phase);
    appendStereoSilenceSegment (left, right, kSr, kTrailingSilenceSeconds);
    feedStereo (meter, left, right);

    INFO ("LRA=" << meter.getLoudnessRange());
    REQUIRE_THAT (meter.getLoudnessRange(), WithinAbs (10.0, 1.0));
}

TEST_CASE ("Tech 3342 case 2: -20/-15 dBFS, 20s each -> LRA = 5 LU", "[dsp][loudness][tech3342][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -20.0, kSr, 20.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -15.0, kSr, 20.0, phase);
    appendStereoSilenceSegment (left, right, kSr, kTrailingSilenceSeconds);
    feedStereo (meter, left, right);

    INFO ("LRA=" << meter.getLoudnessRange());
    REQUIRE_THAT (meter.getLoudnessRange(), WithinAbs (5.0, 1.0));
}

TEST_CASE ("Tech 3342 case 3: -40/-20 dBFS, 20s each -> LRA = 20 LU", "[dsp][loudness][tech3342][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    appendStereoSineSegment (left, right, 1000.0, -40.0, kSr, 20.0, phase);
    appendStereoSineSegment (left, right, 1000.0, -20.0, kSr, 20.0, phase);
    appendStereoSilenceSegment (left, right, kSr, kTrailingSilenceSeconds);
    feedStereo (meter, left, right);

    INFO ("LRA=" << meter.getLoudnessRange());
    REQUIRE_THAT (meter.getLoudnessRange(), WithinAbs (20.0, 1.0));
}

TEST_CASE ("Tech 3342 case 4: -50/-35/-20/-35/-50 dBFS, 20s each -> LRA = 15 LU", "[dsp][loudness][tech3342][T-006]")
{
    LoudnessMeter meter;
    meter.prepare (kSr, 512, 2);

    std::vector<float> left, right;
    double phase = 0.0;
    for (double db : { -50.0, -35.0, -20.0, -35.0, -50.0 })
        appendStereoSineSegment (left, right, 1000.0, db, kSr, 20.0, phase);
    appendStereoSilenceSegment (left, right, kSr, kTrailingSilenceSeconds);
    feedStereo (meter, left, right);

    INFO ("LRA=" << meter.getLoudnessRange());
    REQUIRE_THAT (meter.getLoudnessRange(), WithinAbs (15.0, 1.0));
}
