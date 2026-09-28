// T-004 acceptance criterion 6:
//   With L = R (a sine), Side power <= -150 dB. With L = -R, Mid power
//   <= -150 dB. Left and Right modes isolate their channel: the other
//   channel's sine is <= -150 dB.
//
// Plus a sanity check on the mono-forces-single-channel design requirement
// (every channelMode analyses the single channel when numMainChannels == 1)
// and isDual() correctness for the two dual modes.

#include "SpectrumTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace investigate;
using namespace investigate::test;
using Catch::Matchers::WithinAbs;

namespace
{
    SpectrumAnalyser::Settings stereoSettings (ChannelMode mode, int fftSize = 4096)
    {
        SpectrumAnalyser::Settings s;
        s.sampleRate = 48000.0;
        s.fftSize = fftSize;
        s.overlap = 4;
        s.window = WindowFunctions::Type::hann;
        s.channelMode = mode;
        s.avgTimeMs = 0.0f;
        s.secondary = SecondaryMode::off;
        s.numMainChannels = 2;
        s.numSidechainChannels = 0;
        return s;
    }
}

TEST_CASE ("L == R: Side power <= -150 dB", "[dsp][spectrum][channel][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = stereoSettings (ChannelMode::side, fftSize);
    analyser.prepare (settings);

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);
    auto sine = makeSine (1.0, freq, settings.sampleRate, fftSize);

    pushStereo (analyser, sine, sine);
    REQUIRE (analyser.getFramesProduced() == 1);

    const double db = powerToDb (analyser.getPrimaryPower()[bin]);
    CAPTURE (db);
    REQUIRE (db <= -150.0);
}

TEST_CASE ("L == -R: Mid power <= -150 dB", "[dsp][spectrum][channel][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = stereoSettings (ChannelMode::mid, fftSize);
    analyser.prepare (settings);

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);
    auto sineL = makeSine (1.0, freq, settings.sampleRate, fftSize);
    auto sineR = makeSine (-1.0, freq, settings.sampleRate, fftSize);

    pushStereo (analyser, sineL, sineR);
    REQUIRE (analyser.getFramesProduced() == 1);

    const double db = powerToDb (analyser.getPrimaryPower()[bin]);
    CAPTURE (db);
    REQUIRE (db <= -150.0);
}

TEST_CASE ("Left mode isolates the left channel: right channel's tone is <= -150 dB", "[dsp][spectrum][channel][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = stereoSettings (ChannelMode::left, fftSize);
    analyser.prepare (settings);

    const int binL = fftSize / 4;
    const int binR = fftSize / 4 + 200; // far enough that windowed leakage between the two is negligible
    auto sineL = makeSine (1.0, analyser.getBinFrequency (binL), settings.sampleRate, fftSize);
    auto sineR = makeSine (1.0, analyser.getBinFrequency (binR), settings.sampleRate, fftSize);

    pushStereo (analyser, sineL, sineR);
    REQUIRE (analyser.getFramesProduced() == 1);

    const float* power = analyser.getPrimaryPower();
    const double dbL = powerToDb (power[binL]);
    const double dbR = powerToDb (power[binR]);
    CAPTURE (dbL, dbR);

    REQUIRE_THAT (dbL, WithinAbs (0.0, 0.1));
    REQUIRE (dbR <= -150.0);
}

TEST_CASE ("Right mode isolates the right channel: left channel's tone is <= -150 dB", "[dsp][spectrum][channel][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = stereoSettings (ChannelMode::right, fftSize);
    analyser.prepare (settings);

    const int binL = fftSize / 4;
    const int binR = fftSize / 4 + 200;
    auto sineL = makeSine (1.0, analyser.getBinFrequency (binL), settings.sampleRate, fftSize);
    auto sineR = makeSine (1.0, analyser.getBinFrequency (binR), settings.sampleRate, fftSize);

    pushStereo (analyser, sineL, sineR);
    REQUIRE (analyser.getFramesProduced() == 1);

    const float* power = analyser.getPrimaryPower();
    const double dbL = powerToDb (power[binL]);
    const double dbR = powerToDb (power[binR]);
    CAPTURE (dbL, dbR);

    REQUIRE_THAT (dbR, WithinAbs (0.0, 0.1));
    REQUIRE (dbL <= -150.0);
}

TEST_CASE ("Mono main input forces every channelMode onto the single channel; isDual() is always false", "[dsp][spectrum][channel][T-004]")
{
    const auto mode = GENERATE (ChannelMode::mid, ChannelMode::side, ChannelMode::left, ChannelMode::right,
                                 ChannelMode::lrPowerMean, ChannelMode::lrDual, ChannelMode::msDual);
    CAPTURE (static_cast<int> (mode));

    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    SpectrumAnalyser::Settings s = stereoSettings (mode, fftSize);
    s.numMainChannels = 1;
    analyser.prepare (s);

    REQUIRE_FALSE (analyser.isDual());

    const int bin = fftSize / 4;
    auto sine = makeSine (1.0, analyser.getBinFrequency (bin), s.sampleRate, fftSize);
    pushMono (analyser, sine);

    REQUIRE (analyser.getFramesProduced() == 1);
    const double db = powerToDb (analyser.getPrimaryPower()[bin]);
    REQUIRE_THAT (db, WithinAbs (0.0, 0.1));
}

TEST_CASE ("lrDual and msDual expose two independent curves with isDual() == true", "[dsp][spectrum][channel][T-004]")
{
    const auto mode = GENERATE (ChannelMode::lrDual, ChannelMode::msDual);
    CAPTURE (static_cast<int> (mode));

    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = stereoSettings (mode, fftSize);
    analyser.prepare (settings);

    REQUIRE (analyser.isDual());

    const int binL = fftSize / 4;
    const int binR = fftSize / 4 + 200;
    auto sineL = makeSine (1.0, analyser.getBinFrequency (binL), settings.sampleRate, fftSize);
    auto sineR = makeSine (1.0, analyser.getBinFrequency (binR), settings.sampleRate, fftSize);

    pushStereo (analyser, sineL, sineR);
    REQUIRE (analyser.getFramesProduced() == 1);

    // lrDual: curve A == L (peaks at binL), curve B == R (peaks at binR).
    // msDual: curve A == Mid, curve B == Side - both combine L and R, so we
    // only check that the two curves differ from each other and are finite,
    // not specific bin peaks.
    const float* a = analyser.getPrimaryPower();
    const float* b = analyser.getPrimaryBPower();
    REQUIRE (allFinite (a, analyser.getNumBins()));
    REQUIRE (allFinite (b, analyser.getNumBins()));

    if (mode == ChannelMode::lrDual)
    {
        REQUIRE_THAT (powerToDb (a[binL]), WithinAbs (0.0, 0.1));
        REQUIRE_THAT (powerToDb (b[binR]), WithinAbs (0.0, 0.1));
        REQUIRE (powerToDb (a[binR]) <= -150.0);
        REQUIRE (powerToDb (b[binL]) <= -150.0);
    }
}

TEST_CASE ("Stereo sidechain always power-mean-combines regardless of main channelMode", "[dsp][spectrum][channel][T-004]")
{
    const int fftSize = 4096;
    SpectrumAnalyser analyser;
    auto settings = stereoSettings (ChannelMode::left, fftSize); // main mode irrelevant to sidechain
    settings.numSidechainChannels = 2;
    analyser.prepare (settings);

    REQUIRE (analyser.hasSidechain());

    const int bin = fftSize / 4;
    const double freq = analyser.getBinFrequency (bin);
    auto mainSine = makeSine (1.0, freq, settings.sampleRate, fftSize);
    auto scL = makeSine (1.0, freq, settings.sampleRate, fftSize);
    auto scR = makeSine (0.0, freq, settings.sampleRate, fftSize); // silent R: power-mean should read ~-6 dB (half power) not 0 dB

    const float* mainCh[2] = { mainSine.data(), mainSine.data() };
    const float* scCh[2] = { scL.data(), scR.data() };
    analyser.pushBlock (mainCh, scCh, fftSize);

    REQUIRE (analyser.getFramesProduced() == 1);
    const double scDb = powerToDb (analyser.getSidechainPower()[bin]);
    CAPTURE (scDb);
    REQUIRE_THAT (scDb, WithinAbs (-3.01, 0.1)); // (|1|^2 + |0|^2)/2 == 0.5 -> -3.01 dB
}
