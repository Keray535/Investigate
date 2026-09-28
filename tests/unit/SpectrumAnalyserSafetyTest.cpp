// T-004 acceptance criterion 11: no NaN or Inf in any output for silence,
// full-scale white noise, a DC input, or denormal-range input, across all
// exposed curves (primary, primaryB when dual, sidechain when present,
// real-time max, all-time max, all-time average).

#include "SpectrumTestUtils.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace investigate;
using namespace investigate::test;

namespace
{
    void checkAllCurvesFinite (const SpectrumAnalyser& analyser)
    {
        const int n = analyser.getNumBins();
        REQUIRE (allFinite (analyser.getPrimaryPower(), n));
        REQUIRE (allFinite (analyser.getRealTimeMaxPower(), n));
        REQUIRE (allFinite (analyser.getAllTimeMaxPower(), n));
        REQUIRE (allFinite (analyser.getAllTimeAveragePower(), n));

        if (analyser.isDual())
        {
            REQUIRE (allFinite (analyser.getPrimaryBPower(), n));
            REQUIRE (allFinite (analyser.getRealTimeMaxPower (true), n));
            REQUIRE (allFinite (analyser.getAllTimeMaxPower (true), n));
            REQUIRE (allFinite (analyser.getAllTimeAveragePower (true), n));
        }

        if (analyser.hasSidechain())
            REQUIRE (allFinite (analyser.getSidechainPower(), n));
    }
}

TEST_CASE ("No NaN/Inf for silence, full-scale white noise, DC and denormal-range input", "[dsp][spectrum][safety][T-004]")
{
    enum class SignalType { silence, whiteNoiseFullScale, dc, denormal };
    const auto signalType = GENERATE (SignalType::silence, SignalType::whiteNoiseFullScale, SignalType::dc, SignalType::denormal);
    const auto mode = GENERATE (ChannelMode::mid, ChannelMode::lrDual, ChannelMode::lrPowerMean);
    CAPTURE (static_cast<int> (signalType), static_cast<int> (mode));

    const int fftSize = 2048;
    SpectrumAnalyser::Settings s;
    s.sampleRate = 48000.0;
    s.fftSize = fftSize;
    s.overlap = 4;
    s.window = WindowFunctions::Type::hann;
    s.channelMode = mode;
    s.avgTimeMs = 200.0f;
    s.secondary = SecondaryMode::realTimeMax;
    s.numMainChannels = 2;
    s.numSidechainChannels = 2;

    SpectrumAnalyser analyser;
    analyser.prepare (s);

    std::vector<float> left, right, scL, scR;
    const int numSamples = fftSize * 6;

    switch (signalType)
    {
        case SignalType::silence:
            left = makeSilence (numSamples);
            right = makeSilence (numSamples);
            break;
        case SignalType::whiteNoiseFullScale:
            left = makeWhiteNoise (1.0, numSamples, 1);
            right = makeWhiteNoise (1.0, numSamples, 2);
            break;
        case SignalType::dc:
            left = makeDc (1.0, numSamples);
            right = makeDc (-1.0, numSamples);
            break;
        case SignalType::denormal:
            left = makeWhiteNoise (1e-40, numSamples, 3);
            right = makeWhiteNoise (1e-40, numSamples, 4);
            break;
    }
    scL = left;  // reuse for sidechain: same safety property under test
    scR = right;

    const float* mainCh[2] = { left.data(), right.data() };
    const float* scCh[2] = { scL.data(), scR.data() };
    analyser.pushBlock (mainCh, scCh, numSamples);

    REQUIRE (analyser.getFramesProduced() > 0);
    checkAllCurvesFinite (analyser);
}

TEST_CASE ("Digital silence settles exactly at the -200 dB floor, never below and never non-finite", "[dsp][spectrum][safety][T-004]")
{
    const int fftSize = 2048;
    SpectrumAnalyser::Settings s;
    s.sampleRate = 48000.0;
    s.fftSize = fftSize;
    s.overlap = 4;
    s.window = WindowFunctions::Type::hann;
    s.channelMode = ChannelMode::mid;
    s.avgTimeMs = 0.0f;
    s.numMainChannels = 1;
    s.numSidechainChannels = 0;

    SpectrumAnalyser analyser;
    analyser.prepare (s);

    auto silence = makeSilence (fftSize * 2);
    pushMono (analyser, silence);

    const float* power = analyser.getPrimaryPower();
    for (int k = 0; k < analyser.getNumBins(); ++k)
    {
        REQUIRE (std::isfinite (power[k]));
        REQUIRE (power[k] == 1e-20f); // exact floor, per SpectrumAnalyser.cpp's kFloorPower
    }
}
