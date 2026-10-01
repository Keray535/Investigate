#include "TooltipText.h"

#include "../params/ParameterIDs.h"

namespace investigate::TooltipText
{

namespace
{
    juce::String minus() { return juce::String::fromUTF8 ("\xE2\x88\x92"); }
}

juce::String howToFloat()
{
    return "Drag or scroll to change, Shift for fine. Click to type. Double-click to reset.";
}

juce::String howToChoice()
{
    return "Click to choose. Drag or scroll to step. Double-click to reset.";
}

juce::String forParameter (const juce::String& id)
{
    if (id == ParamIDs::fftSize)
        return "FFT block size in samples. Larger blocks resolve low frequencies better but react more slowly.";

    if (id == ParamIDs::overlap)
        return "How much consecutive blocks overlap. Higher values refresh the curve more often and cost more CPU.";

    if (id == ParamIDs::window)
        return "Analysis window. Hann suits music. Blackman and Hi-Res separate quiet partials from loud ones. Flat-top reads sine levels most accurately.";

    if (id == ParamIDs::avgTime)
        return "Time for the curve to fall by 20 dB. Off shows every block unaveraged. It also sets how fast the RT max curve falls.";

    if (id == ParamIDs::smoothing)
        return "Fractional-octave smoothing of the displayed curve.";

    if (id == ParamIDs::slope)
        return "Tilts the display around 1 kHz. +4.5 dB/oct makes typical music look roughly flat.";

    if (id == ParamIDs::secondary)
        return "Second curve. RT max falls back at the average time. All-time max and All-time avg keep accumulating until you reset them.";

    if (id == ParamIDs::freqLow)
        return "Lowest displayed frequency.";

    if (id == ParamIDs::freqHigh)
        return "Highest displayed frequency.";

    if (id == ParamIDs::dbLow)
        return "Bottom of the level axis.";

    if (id == ParamIDs::dbHigh)
        return "Top of the level axis. Top and bottom stay at least 12 dB apart.";

    if (id == ParamIDs::meterScale)
        return "Meter scale. K-20, K-14 and K-12 add the +3 dB sine reference to RMS and put 0 at "
               + minus() + "20, " + minus() + "14 or " + minus() + "12 dBFS.";

    if (id == ParamIDs::rmsWindow)
        return "Length of the RMS window used by the level meters.";

    if (id == ParamIDs::channelMode)
        return "Channel mode";

    if (id == ParamIDs::showSidechain)
        return "Show the sidechain input as a reference silhouette.";

    return {};
}

juce::String blockSize (int samples, double sampleRate)
{
    const auto sr = sampleRate > 0.0 ? sampleRate : 48000.0; // DisplayContext::fallbackRate
    const auto ms = (double) samples / sr * 1000.0;
    const auto bin = sr / (double) samples;
    const auto khz = juce::String (sr / 1000.0, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");

    return "FFT block size: " + juce::String (samples) + " samples, " + juce::String (ms, 1) + " ms, "
         + juce::String (bin, 1) + " Hz bin spacing at " + khz + " kHz. "
         + "Larger blocks resolve low frequencies better but react more slowly.";
}

juce::String forChannelMode (int modeIndex)
{
    switch (modeIndex)
    {
        case 0:  return "Mid: the sum of left and right, halved";
        case 1:  return "Side: the difference of left and right, halved";
        case 2:  return "Left channel only";
        case 3:  return "Right channel only";
        case 4:  return "Power mean of left and right";
        case 5:  return "Left and right as two curves";
        case 6:  return "Mid and side as two curves";
        default: return "Channel mode";
    }
}

juce::String channelModeHowTo()
{
    return "Scroll to step. Double-click to reset to Mid.";
}

juce::String sidechain (bool busActive)
{
    return busActive ? "Show the sidechain input as a reference silhouette behind the main curve."
                     : "No sidechain input is connected. Route a track to the sidechain bus in your host.";
}

juce::String freeze()
{
    return "Hold the current spectrum. Hover still works. Turns off when the channel mode changes.";
}

juce::String resetCurves()
{
    return "Clear the all-time max and all-time average curves. Clicking the spectrum does the same.";
}

juce::String resetStatistics()
{
    return "Clear peak maxima, clip counts, and the integrated loudness measurement.";
}

juce::String metersToggle (bool collapsed)
{
    return collapsed ? "Show the meter panel" : "Hide the meter panel to give the spectrum more room";
}

juce::String settings (bool controlsHidden)
{
    return controlsHidden ? "All settings. Some controls do not fit in the bar at this width and are only here."
                          : "All settings";
}

} // namespace investigate::TooltipText
