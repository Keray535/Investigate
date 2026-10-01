#pragma once

#include "ValueFormat.h"

#include "../params/ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace investigate
{

/** Message-thread view of the facts several controls share: the sample rate
    (from the newest meter snapshot or spectrum frame) and the effective
    display range derived from the four range parameters through
    `DisplayRange.h` (ui-spec 5.4). Controls consult it when painting, so a
    clamped value shows in the warning colour and its tooltip explains why.
    Parameters are never rewritten. */
class DisplayContext
{
public:
    explicit DisplayContext (juce::AudioProcessorValueTreeState& apvts)
        : freqLowValue (apvts.getRawParameterValue (ParamIDs::freqLow)),
          freqHighValue (apvts.getRawParameterValue (ParamIDs::freqHigh)),
          dbLowValue (apvts.getRawParameterValue (ParamIDs::dbLow)),
          dbHighValue (apvts.getRawParameterValue (ParamIDs::dbHigh))
    {
        refresh();
    }

    void setSampleRate (double newSampleRate) noexcept { sampleRate = newSampleRate; }
    double getSampleRate() const noexcept { return sampleRate; }

    /** A usable rate for text that must always show one (the Block tooltip). */
    double getSampleRateOrDefault() const noexcept { return sampleRate > 0.0 ? sampleRate : fallbackRate; }

    /** Re-reads the four parameters and the rate. Returns true if the
        effective range (or which values are clamped) changed since the last
        call, so the caller repaints only then. */
    bool refresh()
    {
        const auto next = ValueFormat::computeEffectiveRange (freqLowValue->load(), freqHighValue->load(),
                                                              dbLowValue->load(), dbHighValue->load(), sampleRate);
        const bool changed = next.freqLow != cached.freqLow || next.freqHigh != cached.freqHigh
                          || next.dbLow != cached.dbLow || next.dbHigh != cached.dbHigh
                          || next.freqLowClamped != cached.freqLowClamped
                          || next.freqHighClamped != cached.freqHighClamped
                          || next.dbLowClamped != cached.dbLowClamped
                          || next.nyquist != cached.nyquist;
        cached = next;
        return changed;
    }

    const ValueFormat::EffectiveRange& effective() const noexcept { return cached; }

private:
    static constexpr double fallbackRate = 48000.0;

    std::atomic<float>* freqLowValue;
    std::atomic<float>* freqHighValue;
    std::atomic<float>* dbLowValue;
    std::atomic<float>* dbHighValue;

    double sampleRate = 0.0;
    ValueFormat::EffectiveRange cached;
};

} // namespace investigate
