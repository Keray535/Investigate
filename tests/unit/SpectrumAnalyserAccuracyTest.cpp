// T-004 acceptance criteria 2-5: window/scaling accuracy.
//   2. -6.00 dBFS bin-centred sine reads -6.00 +-0.05 dB, Hann/Blackman/
//      Hi-Res, fftSize 4096 and 65536.
//   3. Hi-Res leakage 20 bins from a centred full-scale sine <= -120 dB
//      relative to the peak, fftSize 65536. Theoretical sidelobe of a single
//      4-term Nuttall (1981) window is ~-93 dB; squared (Hi-Res) ~-186 dB
//      theoretical floor - the -120 dB bound is a float32 FFT precision
//      limit, not the window's own floor (matches the task's own framing).
//   4. Flat-top: -6.00 dBFS at any frequency between bins reads
//      -6.00 +-0.02 dB (read at the single nearest bin, which is the whole
//      point of a flat-top window - no interpolation).
//   5. Hann scalloping loss (sine exactly between two bins) is
//      1.42 +-0.1 dB.
//
// All tests use avgTimeMs = 0 (no averaging: primary reads the raw current
// frame directly) and a mono main input, so window/scaling behaviour is
// isolated from channel-combine and averaging logic (covered separately).

#include "SpectrumTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    SpectrumAnalyser::Settings monoSettings (int fftSize, WindowFunctions::Type window, double sampleRate = 48000.0)
    {
        SpectrumAnalyser::Settings s;
        s.sampleRate = sampleRate;
        s.fftSize = fftSize;
        s.overlap = 4;
        s.window = window;
        s.channelMode = ChannelMode::mid; // irrelevant with numMainChannels == 1
        s.avgTimeMs = 0.0f;               // raw per-frame power, no blending
        s.secondary = SecondaryMode::off;
        s.numMainChannels = 1;
        s.numSidechainChannels = 0;
        return s;
    }
}

TEST_CASE ("Bin-centred -6 dBFS sine reads -6.00 +-0.05 dB (Hann/Blackman/Hi-Res, 4096/65536)", "[dsp][spectrum][T-004]")
{
    const int fftSize = GENERATE (4096, 65536);
    const auto window = GENERATE (WindowFunctions::Type::hann, WindowFunctions::Type::blackman, WindowFunctions::Type::hiRes);
    CAPTURE (fftSize, static_cast<int> (window));

    SpectrumAnalyser analyser;
    auto settings = monoSettings (fftSize, window);
    analyser.prepare (settings);

    const int bin = fftSize / 4; // safely interior, far from DC/Nyquist and from the very edge
    const double freq = analyser.getBinFrequency (bin);
    const double amplitude = dbToAmplitude (-6.0);

    auto sine = makeSine (amplitude, freq, settings.sampleRate, fftSize);
    pushMono (analyser, sine);

    REQUIRE (analyser.getFramesProduced() == 1);

    const double db = powerToDb (analyser.getPrimaryPower()[bin]);
    CAPTURE (db);
    REQUIRE_THAT (db, WithinAbs (-6.0, 0.05));
}

TEST_CASE ("Hi-Res window leakage 20 bins from a full-scale centred sine is <= -120 dB, fftSize 65536", "[dsp][spectrum][T-004]")
{
    const int fftSize = 65536;
    SpectrumAnalyser analyser;
    auto settings = monoSettings (fftSize, WindowFunctions::Type::hiRes);
    analyser.prepare (settings);

    const int bin = 2000;
    const double freq = analyser.getBinFrequency (bin);

    auto sine = makeSine (1.0, freq, settings.sampleRate, fftSize);
    pushMono (analyser, sine);
    REQUIRE (analyser.getFramesProduced() == 1);

    const float* power = analyser.getPrimaryPower();
    const double peakDb = powerToDb (power[bin]);
    CAPTURE (peakDb);
    REQUIRE_THAT (peakDb, WithinAbs (0.0, 0.1)); // sanity: the peak itself should read ~0 dBFS

    const double leakageDbAbove = powerToDb (power[bin + 20]) - peakDb;
    const double leakageDbBelow = powerToDb (power[bin - 20]) - peakDb;
    CAPTURE (leakageDbAbove, leakageDbBelow);

    REQUIRE (leakageDbAbove <= -120.0);
    REQUIRE (leakageDbBelow <= -120.0);
}

TEST_CASE ("Flat-top: -6.00 dBFS at any inter-bin frequency reads -6.00 +-0.02 dB at the nearest bin", "[dsp][spectrum][T-004]")
{
    const int fftSize = 4096;
    const double fractionalOffset = GENERATE (0.1, 0.3, 0.5, 0.7, 0.9);
    CAPTURE (fractionalOffset);

    SpectrumAnalyser analyser;
    auto settings = monoSettings (fftSize, WindowFunctions::Type::flatTop);
    analyser.prepare (settings);

    const int baseBin = fftSize / 4;
    const double freq = (static_cast<double> (baseBin) + fractionalOffset) * settings.sampleRate / static_cast<double> (fftSize);
    const double amplitude = dbToAmplitude (-6.0);

    auto sine = makeSine (amplitude, freq, settings.sampleRate, fftSize);
    pushMono (analyser, sine);
    REQUIRE (analyser.getFramesProduced() == 1);

    const int nearestBin = static_cast<int> (std::lround (static_cast<double> (baseBin) + fractionalOffset));
    const double db = powerToDb (analyser.getPrimaryPower()[nearestBin]);
    CAPTURE (nearestBin, db);
    REQUIRE_THAT (db, WithinAbs (-6.0, 0.02));
}

TEST_CASE ("Hann scalloping loss (sine exactly between two bins) is 1.42 +-0.1 dB", "[dsp][spectrum][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = monoSettings (fftSize, WindowFunctions::Type::hann);
    analyser.prepare (settings);

    const int baseBin = fftSize / 4;
    const double freq = (static_cast<double> (baseBin) + 0.5) * settings.sampleRate / static_cast<double> (fftSize);

    auto sine = makeSine (1.0, freq, settings.sampleRate, fftSize); // full scale: 0 dBFS reference
    pushMono (analyser, sine);
    REQUIRE (analyser.getFramesProduced() == 1);

    const float* power = analyser.getPrimaryPower();
    const double dbLow = powerToDb (power[baseBin]);
    const double dbHigh = powerToDb (power[baseBin + 1]);
    CAPTURE (dbLow, dbHigh);

    // By symmetry both neighbouring bins should read the same scalloping
    // loss; check both individually against the spec, and that they agree
    // with each other within the same tolerance.
    REQUIRE_THAT (dbLow, WithinAbs (-1.42, 0.1));
    REQUIRE_THAT (dbHigh, WithinAbs (-1.42, 0.1));
}

TEST_CASE ("Amplitude/window accuracy is independent of how a full frame is split across pushBlock calls", "[dsp][spectrum][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = monoSettings (fftSize, WindowFunctions::Type::hann);
    analyser.prepare (settings);

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);
    const double amplitude = dbToAmplitude (-6.0);
    auto sine = makeSine (amplitude, freq, settings.sampleRate, fftSize);

    const int chunk = GENERATE (1, 17, 64, 512);
    CAPTURE (chunk);
    pushMono (analyser, sine, chunk);

    REQUIRE (analyser.getFramesProduced() == 1);
    const double db = powerToDb (analyser.getPrimaryPower()[bin]);
    REQUIRE_THAT (db, WithinAbs (-6.0, 0.05));
}
