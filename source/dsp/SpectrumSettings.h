#pragma once

#include "SpectrumAnalyser.h"
#include "WindowFunctions.h"

namespace investigate
{

/** Fractional-octave smoothing resolution. Matches the roadmap's
    `smoothing` parameter choices exactly. */
enum class SmoothingMode
{
    off,
    oct1_48,
    oct1_24,
    oct1_12,
    oct1_6,
    oct1_3
};

/** Every spectrum-related setting from the roadmap parameter table, as
    plain values, plus the sample rate and the two channel counts derived
    from the current bus layout (not APVTS parameters themselves, but
    required by `SpectrumAnalyser::Settings`; T-009 fills them in from the
    processor's bus layout on every `SpectrumEngine::processPending()`
    call). No APVTS, no processor dependency, so `SpectrumMapper` and
    `SpectrumEngine` are testable in isolation.

    `numPoints` is not an APVTS parameter (the roadmap parameter table has
    no such entry); it is the number of display points `SpectrumMapper`
    should produce this frame, up to `SpectrumFrame::maxPoints`. T-009/the
    editor decide how many points to request (for example, matched to the
    plot's pixel width); changing it between frames must not reset any
    analyser or mapper state (T-005 acceptance criteria).

    `meterScale` and `rmsWindow` are deliberately absent: they belong to the
    meter pipeline (T-006/T-007), not the spectrum pipeline. */
struct SpectrumSettings
{
    double sampleRate = 48000.0;

    // Bin-domain analysis (SpectrumAnalyser::Settings fields).
    int fftSize = 4096;
    int overlap = 4;
    WindowFunctions::Type window = WindowFunctions::Type::hann;
    ChannelMode channelMode = ChannelMode::mid;
    float avgTimeMs = 1000.0f;
    SecondaryMode secondary = SecondaryMode::realTimeMax;
    int numMainChannels = 2;
    int numSidechainChannels = 0;

    // Display mapping (SpectrumMapper fields).
    float freqLow = 20.0f;
    float freqHigh = 20000.0f;
    float dbLow = -90.0f;
    float dbHigh = 6.0f;
    SmoothingMode smoothing = SmoothingMode::oct1_12;
    float slopeDbPerOct = 4.5f;
    int numPoints = 2048;

    /// Whether the editor currently wants the sidechain trace drawn. The
    /// mapper still computes the sidechain curve whenever the analyser has
    /// one (SpectrumEngine is pure DSP and does not know about UI
    /// visibility); the editor is the one that reads `showSidechain`
    /// (roadmap: `isSidechainActive() && showSidechain`) to decide whether
    /// to draw it. Kept here only so this struct holds every roadmap
    /// spectrum parameter, as the task requires.
    bool showSidechain = true;

    /** The subset of these settings that `SpectrumAnalyser::setSettings()`
        needs. Keeping the conversion in one place means the bin-domain
        analyser and the display mapper can never disagree about fftSize,
        window, channel mode, sample rate or channel counts. */
    SpectrumAnalyser::Settings toAnalyserSettings() const noexcept
    {
        SpectrumAnalyser::Settings s;
        s.sampleRate = sampleRate;
        s.fftSize = fftSize;
        s.overlap = overlap;
        s.window = window;
        s.channelMode = channelMode;
        s.avgTimeMs = avgTimeMs;
        s.secondary = secondary;
        s.numMainChannels = numMainChannels;
        s.numSidechainChannels = numSidechainChannels;
        return s;
    }
};

} // namespace investigate
