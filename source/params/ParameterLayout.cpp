#include "ParameterLayout.h"

#include "ParameterFormatting.h"
#include "ParameterIDs.h"

namespace investigate
{

namespace
{
    /// A NormalisableRange with a log-ish skew set so `centre` lands at the
    /// midpoint of the 0-1 normalised range, matching the roadmap's
    /// "skew (centre ...)" column for freqLow, freqHigh, avgTime and
    /// rmsWindow.
    juce::NormalisableRange<float> makeSkewedRange (float start, float end, float centre)
    {
        juce::NormalisableRange<float> range (start, end);
        range.setSkewForCentre (centre);
        return range;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // channelMode: choice order matches source/dsp/SpectrumAnalyser.h's
    // ChannelMode enum exactly (mid, side, left, right, lrPowerMean, lrDual,
    // msDual), and the display text matches the Settings-panel labels in
    // docs/design/ui-spec.md section 6 ("Mid", "Side", "Left", "Right",
    // "L+R", "L/R", "M/S").
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::channelMode, 1 },
        "Mode",
        juce::StringArray { "Mid", "Side", "Left", "Right", "L+R", "L/R", "M/S" },
        0));

    // fftSize: plain sample counts, no unit (the unit is in the tooltip only,
    // ui-spec section 6).
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::fftSize, 1 },
        "Block size",
        juce::StringArray { "512", "1024", "2048", "4096", "8192", "16384", "32768", "65536" },
        3));

    // overlap: uses the U+00D7 multiplication sign, matching the chip text
    // ("4×") rather than an ASCII "x".
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::overlap, 1 },
        "Overlap",
        juce::StringArray { "1" + ParameterFormatting::multiplySign,
                             "2" + ParameterFormatting::multiplySign,
                             "4" + ParameterFormatting::multiplySign,
                             "8" + ParameterFormatting::multiplySign },
        2));

    // window: choice order matches source/dsp/WindowFunctions.h's Type enum
    // exactly (hann, blackman, hiRes, flatTop).
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::window, 1 },
        "Window",
        juce::StringArray { "Hann", "Blackman", "Hi-Res", "Flat-top" },
        0));

    // avgTime: 0 to 10000 ms, skew centred at 500 ms (roadmap parameter table).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::avgTime, 1 },
        "Avg time",
        makeSkewedRange (0.0f, 10000.0f, 500.0f),
        1000.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::avgTimeToText)
            .withValueFromStringFunction (ParameterFormatting::textToAvgTimeMs)));

    // smoothing: choice order matches source/dsp/SpectrumSettings.h's
    // SmoothingMode enum exactly (off, oct1_48, oct1_24, oct1_12, oct1_6,
    // oct1_3).
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::smoothing, 1 },
        "Smoothing",
        juce::StringArray { "Off", "1/48 oct", "1/24 oct", "1/12 oct", "1/6 oct", "1/3 oct" },
        3));

    // slope: -12 to +12 dB/oct, step 0.5, linear, default 4.5 (T-001, confirmed
    // in the roadmap Decisions dated 2026-09-28).
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::slope, 1 },
        "Slope",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.5f),
        4.5f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::slopeToText)
            .withValueFromStringFunction (ParameterFormatting::textToSlope)));

    // secondary: choice order matches source/dsp/SpectrumAnalyser.h's
    // SecondaryMode enum exactly (off, realTimeMax, allTimeMax,
    // allTimeAverage), with the abbreviated chip/legend text from
    // docs/design/ui-spec.md sections 5.2 and 6.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::secondary, 1 },
        "Secondary curve",
        juce::StringArray { "Off", "RT max", "All-time max", "All-time avg" },
        1));

    // freqLow / freqHigh: log skew centred at 100 Hz / 6325 Hz.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::freqLow, 1 },
        "Frequency low",
        makeSkewedRange (10.0f, 1000.0f, 100.0f),
        20.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::frequencyToText)
            .withValueFromStringFunction (ParameterFormatting::textToFrequencyHz)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::freqHigh, 1 },
        "Frequency high",
        makeSkewedRange (1000.0f, 40000.0f, 6325.0f),
        20000.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::frequencyToText)
            .withValueFromStringFunction (ParameterFormatting::textToFrequencyHz)));

    // dbLow / dbHigh: linear, step 1 dB.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::dbLow, 1 },
        "Range low",
        juce::NormalisableRange<float> (-150.0f, -30.0f, 1.0f),
        -90.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::dbLowToText)
            .withValueFromStringFunction (ParameterFormatting::textToDbLow)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::dbHigh, 1 },
        "Range high",
        juce::NormalisableRange<float> (-30.0f, 30.0f, 1.0f),
        6.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::dbHighToText)
            .withValueFromStringFunction (ParameterFormatting::textToDbHigh)));

    // showSidechain: bool, default on.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::showSidechain, 1 },
        "Sidechain trace",
        true));

    // meterScale: choice order matches source/params/MeterScale.h exactly.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::meterScale, 1 },
        "Meter scale",
        juce::StringArray { "dBFS", "K-20", "K-14", "K-12" },
        0));

    // rmsWindow: 50 to 3000 ms, skew centred at 300 ms.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::rmsWindow, 1 },
        "RMS window",
        makeSkewedRange (50.0f, 3000.0f, 300.0f),
        300.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction (ParameterFormatting::rmsWindowToText)
            .withValueFromStringFunction (ParameterFormatting::textToRmsWindowMs)));

    return layout;
}

} // namespace investigate
