#pragma once

#include <juce_core/juce_core.h>

/** The canonical tooltip copy (the mockup's TIPS table, ui-spec 6 Tooltips),
    with U+2212 for minus signs. A tooltip is the first line below plus, after
    a newline, the "how to" line, which the LookAndFeel draws in the secondary
    style at 10 px. */
namespace investigate::TooltipText
{

/** Second line for float controls. */
juce::String howToFloat();

/** Second line for choice controls. */
juce::String howToChoice();

/** First line for the parameter, without any dynamic part. Empty for an
    unknown ID. fftSize returns the generic text; use blockSize() for the
    dynamic one. */
juce::String forParameter (const juce::String& paramID);

/** "FFT block size: 4096 samples, 85.3 ms, 11.7 Hz bin spacing at 48 kHz. ..." */
juce::String blockSize (int samples, double sampleRate);

/** The mode segment tooltips (index = channelMode choice). */
juce::String forChannelMode (int modeIndex);
juce::String channelModeHowTo();

juce::String sidechain (bool busActive);
juce::String freeze();
juce::String resetCurves();
juce::String resetStatistics();
juce::String metersToggle (bool collapsed);
juce::String settings (bool controlsHidden);

} // namespace investigate::TooltipText
