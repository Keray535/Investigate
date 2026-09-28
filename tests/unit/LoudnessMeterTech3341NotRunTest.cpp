// Documents (per CLAUDE.md/task instructions) the Tech 3341 and Tech 3342
// conformance cases that are not run, and why, as Catch2 SKIP tests so they
// show up explicitly in `ctest --output-on-failure` / the Catch2 console
// reporter rather than being silently absent from the suite.

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("Tech 3341 case 6: 5.0-channel sine -> I = -23.0 LUFS (not run)", "[dsp][loudness][tech3341][T-006]")
{
    SKIP ("not run (unsupported channel count - LoudnessMeter is mono/stereo only, "
          "per the roadmap's bus layout and T-006's explicit channel-weight scope: "
          "\"L and R are 1.0; mono is 1.0\"). Case 6 needs 5.0-channel input "
          "(L/R -28 dBFS, C -24 dBFS, Ls/Rs -30 dBFS, no LFE).");
}

TEST_CASE ("Tech 3341 case 7: authentic programme 1 (NLR) -> I = -23.0 LUFS (not run)", "[dsp][loudness][tech3341][T-006]")
{
    SKIP ("not run (official file required) - user decision 2026-09-28, docs/dependencies.md.");
}

TEST_CASE ("Tech 3341 case 8: authentic programme 2 (WLR) -> I = -23.0 LUFS (not run)", "[dsp][loudness][tech3341][T-006]")
{
    SKIP ("not run (official file required) - user decision 2026-09-28, docs/dependencies.md.");
}

TEST_CASE ("Tech 3342 case 5: authentic programme 1 (NLR) -> LRA = 5 LU (not run)", "[dsp][loudness][tech3342][T-006]")
{
    SKIP ("not run (official file required) - user decision 2026-09-28, docs/dependencies.md.");
}

TEST_CASE ("Tech 3342 case 6: authentic programme 2 (WLR) -> LRA = 15 LU (not run)", "[dsp][loudness][tech3342][T-006]")
{
    SKIP ("not run (official file required) - user decision 2026-09-28, docs/dependencies.md.");
}
