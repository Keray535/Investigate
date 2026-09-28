#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MeterScale.h"
#include "ParameterIDs.h"
#include "dsp/SpectrumAnalyser.h"
#include "dsp/SpectrumSettings.h"
#include "dsp/WindowFunctions.h"

namespace investigate
{

/** Caches `std::atomic<float>*` pointers from the APVTS in the constructor,
    so the audio thread and the analysis thread (roadmap Threading model)
    read every parameter with a single atomic load each, never a string
    lookup, a `ValueTree` access or an allocation (CLAUDE.md real-time safety
    rules).

    This is also the one place that maps a choice parameter's raw index onto
    the matching `source/dsp/` enum or numeric value (`ChannelMode`,
    `WindowFunctions::Type`, `SecondaryMode`, `SmoothingMode`, the fftSize and
    overlap value tables). `ParameterLayout.cpp` defines each choice list in
    exactly the same order as the corresponding dsp/ enum, so the `static_cast`
    below is correct as long as the two stay in sync; a mismatch would only
    show up as a wrong-but-valid enum value, which is why both files carry
    matching order comments. */
class ParameterRefs
{
public:
    explicit ParameterRefs (juce::AudioProcessorValueTreeState& apvts)
        : channelModeParam   (apvts.getRawParameterValue (ParamIDs::channelMode)),
          fftSizeParam       (apvts.getRawParameterValue (ParamIDs::fftSize)),
          overlapParam       (apvts.getRawParameterValue (ParamIDs::overlap)),
          windowParam        (apvts.getRawParameterValue (ParamIDs::window)),
          avgTimeParam       (apvts.getRawParameterValue (ParamIDs::avgTime)),
          smoothingParam     (apvts.getRawParameterValue (ParamIDs::smoothing)),
          slopeParam         (apvts.getRawParameterValue (ParamIDs::slope)),
          secondaryParam     (apvts.getRawParameterValue (ParamIDs::secondary)),
          freqLowParam       (apvts.getRawParameterValue (ParamIDs::freqLow)),
          freqHighParam      (apvts.getRawParameterValue (ParamIDs::freqHigh)),
          dbLowParam         (apvts.getRawParameterValue (ParamIDs::dbLow)),
          dbHighParam        (apvts.getRawParameterValue (ParamIDs::dbHigh)),
          showSidechainParam (apvts.getRawParameterValue (ParamIDs::showSidechain)),
          meterScaleParam    (apvts.getRawParameterValue (ParamIDs::meterScale)),
          rmsWindowParam     (apvts.getRawParameterValue (ParamIDs::rmsWindow))
    {
        jassert (channelModeParam != nullptr && fftSizeParam != nullptr && overlapParam != nullptr
                 && windowParam != nullptr && avgTimeParam != nullptr && smoothingParam != nullptr
                 && slopeParam != nullptr && secondaryParam != nullptr && freqLowParam != nullptr
                 && freqHighParam != nullptr && dbLowParam != nullptr && dbHighParam != nullptr
                 && showSidechainParam != nullptr && meterScaleParam != nullptr && rmsWindowParam != nullptr);
    }

    ChannelMode getChannelMode() const noexcept
    {
        return static_cast<ChannelMode> (juce::jlimit (0, 6, (int) channelModeParam->load (std::memory_order_relaxed)));
    }

    int getFftSize() const noexcept
    {
        static constexpr int sizes[] { 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536 };
        return sizes[(size_t) juce::jlimit (0, 7, (int) fftSizeParam->load (std::memory_order_relaxed))];
    }

    int getOverlap() const noexcept
    {
        static constexpr int values[] { 1, 2, 4, 8 };
        return values[(size_t) juce::jlimit (0, 3, (int) overlapParam->load (std::memory_order_relaxed))];
    }

    WindowFunctions::Type getWindow() const noexcept
    {
        return static_cast<WindowFunctions::Type> (juce::jlimit (0, 3, (int) windowParam->load (std::memory_order_relaxed)));
    }

    float getAvgTimeMs() const noexcept { return avgTimeParam->load (std::memory_order_relaxed); }

    SmoothingMode getSmoothing() const noexcept
    {
        return static_cast<SmoothingMode> (juce::jlimit (0, 5, (int) smoothingParam->load (std::memory_order_relaxed)));
    }

    float getSlope() const noexcept { return slopeParam->load (std::memory_order_relaxed); }

    SecondaryMode getSecondary() const noexcept
    {
        return static_cast<SecondaryMode> (juce::jlimit (0, 3, (int) secondaryParam->load (std::memory_order_relaxed)));
    }

    float getFreqLow() const noexcept  { return freqLowParam->load (std::memory_order_relaxed); }
    float getFreqHigh() const noexcept { return freqHighParam->load (std::memory_order_relaxed); }
    float getDbLow() const noexcept    { return dbLowParam->load (std::memory_order_relaxed); }
    float getDbHigh() const noexcept   { return dbHighParam->load (std::memory_order_relaxed); }

    bool getShowSidechain() const noexcept
    {
        return showSidechainParam->load (std::memory_order_relaxed) >= 0.5f;
    }

    MeterScale getMeterScale() const noexcept
    {
        return static_cast<MeterScale> (juce::jlimit (0, 3, (int) meterScaleParam->load (std::memory_order_relaxed)));
    }

    float getRmsWindowMs() const noexcept { return rmsWindowParam->load (std::memory_order_relaxed); }

private:
    std::atomic<float>* channelModeParam;
    std::atomic<float>* fftSizeParam;
    std::atomic<float>* overlapParam;
    std::atomic<float>* windowParam;
    std::atomic<float>* avgTimeParam;
    std::atomic<float>* smoothingParam;
    std::atomic<float>* slopeParam;
    std::atomic<float>* secondaryParam;
    std::atomic<float>* freqLowParam;
    std::atomic<float>* freqHighParam;
    std::atomic<float>* dbLowParam;
    std::atomic<float>* dbHighParam;
    std::atomic<float>* showSidechainParam;
    std::atomic<float>* meterScaleParam;
    std::atomic<float>* rmsWindowParam;
};

} // namespace investigate
