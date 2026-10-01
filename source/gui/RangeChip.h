#pragma once

#include "ParamControl.h"

namespace investigate
{

/**
    The Frequency and Level chips (ui-spec 4.3, 6): one chip with a shared
    label and two independently editable halves, low and high, separated by
    "…". Each half is a `ParamControl` whose hit area is its side of the chip
    at the full chip height (ui-spec 10.3).
*/
class RangeChip final : public juce::Component,
                        private juce::Timer
{
public:
    /** `lowHalfWidth` is the width reserved for the low value before the
        separator (54 for Frequency, 40 for Level). */
    RangeChip (juce::AudioProcessorValueTreeState& apvts,
               const juce::String& lowID, const juce::String& highID,
               const juce::String& label,
               const juce::String& lowName, const juce::String& highName,
               int lowHalfWidth, DisplayContext& context);
    ~RangeChip() override;

    ParamControl& getLow() noexcept { return low; }
    ParamControl& getHigh() noexcept { return high; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void armDot();

    juce::String label;
    int lowHalfWidth;
    juce::uint32 dotUntil = 0;

    ParamControl low;
    ParamControl high;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RangeChip)
};

} // namespace investigate
