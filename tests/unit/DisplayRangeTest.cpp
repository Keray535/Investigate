// T-005: DisplayRange.h free functions. These four formulas are shared
// verbatim between SpectrumMapper (analysis thread) and the editor's grid
// code (T-012); this test locks the exact roadmap formulas so a future edit
// to either side can't silently diverge from the other.

#include "dsp/DisplayRange.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using Catch::Matchers::WithinAbs;

TEST_CASE ("DisplayRange::effectiveFreqHigh clamps to Nyquist", "[T-005][display-range]")
{
    // freqHigh below Nyquist: passed through unchanged.
    CHECK_THAT (DisplayRange::effectiveFreqHigh (20000.0, 48000.0), WithinAbs (20000.0, 1e-9));

    // freqHigh above Nyquist (e.g. the 40000 Hz parameter max at 44.1 kHz):
    // clamped to sampleRate / 2.
    CHECK_THAT (DisplayRange::effectiveFreqHigh (40000.0, 44100.0), WithinAbs (22050.0, 1e-9));

    // Exactly at Nyquist: unchanged (min of two equal values).
    CHECK_THAT (DisplayRange::effectiveFreqHigh (24000.0, 48000.0), WithinAbs (24000.0, 1e-9));
}

TEST_CASE ("DisplayRange::effectiveFreqLow guarantees at least one octave below fHiEff", "[T-005][display-range]")
{
    // freqLow well below fHiEff/2: passed through unchanged.
    CHECK_THAT (DisplayRange::effectiveFreqLow (20.0, 24000.0), WithinAbs (20.0, 1e-9));

    // freqLow above fHiEff/2 (e.g. freqLow=1000, fHiEff=1200): clamped to
    // fHiEff/2 so the display always spans >= 1 octave.
    CHECK_THAT (DisplayRange::effectiveFreqLow (1000.0, 1200.0), WithinAbs (600.0, 1e-9));

    // Must be derived from fHiEff (the already-clamped value), not the raw
    // freqHigh parameter - verified by construction here since the function
    // only accepts freqHighEffective as its second argument.
    CHECK_THAT (DisplayRange::effectiveFreqLow (500.0, 22050.0), WithinAbs (500.0, 1e-9));
}

TEST_CASE ("DisplayRange::effectiveDbHigh is the identity", "[T-005][display-range]")
{
    CHECK_THAT (DisplayRange::effectiveDbHigh (6.0), WithinAbs (6.0, 1e-9));
    CHECK_THAT (DisplayRange::effectiveDbHigh (-30.0), WithinAbs (-30.0, 1e-9));
    CHECK_THAT (DisplayRange::effectiveDbHigh (30.0), WithinAbs (30.0, 1e-9));
}

TEST_CASE ("DisplayRange::effectiveDbLow guarantees at least 12 dB of span", "[T-005][display-range]")
{
    // dbLow well below dbHigh - 12: unchanged.
    CHECK_THAT (DisplayRange::effectiveDbLow (-90.0, 6.0), WithinAbs (-90.0, 1e-9));

    // dbLow above dbHigh - 12 (e.g. dbLow = -5, dbHigh = 6 -> dbHigh-12 = -6):
    // clamped to dbHigh - 12.
    CHECK_THAT (DisplayRange::effectiveDbLow (-5.0, 6.0), WithinAbs (-6.0, 1e-9));

    // Exactly 12 dB apart: unchanged.
    CHECK_THAT (DisplayRange::effectiveDbLow (-6.0, 6.0), WithinAbs (-6.0, 1e-9));
}
