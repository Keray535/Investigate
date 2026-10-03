#pragma once

#include "DisplayContext.h"
#include "HeaderButton.h"
#include "LayoutMath.h"
#include "MeterMath.h"
#include "ParamControl.h"

#include "../dsp/MeterSnapshot.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <functional>

namespace investigate
{

class InvestigateProcessor;
class InvestigateLookAndFeel;

/**
    The meter strip (ui-spec 4.4, 8): the scale and RMS chips and the reset
    button in the strip header (T-011), the L/R level bars with peak hold and
    clip lamps, the statistics table, the loudness block, the correlation bar
    and the balance readout (T-013).

    Data arrives from `update()`, which the editor calls once per vblank with a
    copy of the processor's `MeterSnapshot`. The ballistics (peak fall, hold,
    clip latch, the correlation filter, the crest ring) live here; the snapshot
    is never changed.

    Rendering follows the T-029 rules. The static layer (tracks, tick lines
    and labels, table and block labels, dividers) is one cached image. Per
    frame only the lit parts of the bars are drawn, as flat whole-physical-
    pixel rectangles batched one `fillRectList` per colour. A bar is repainted
    only when one of its quantised pixel values changed. The numbers refresh
    at 10 Hz and repaint only the cells whose text changed. When the spectrum
    is frame-paced, repaints wait for the vblank on which the spectrum
    repaints (at most 34 ms), so both share one window present.

    Message-thread only. No allocation in `update()` or the bar and
    correlation paint paths after the first frame; the text cells build a
    `juce::String` only when their text changed.
*/
class MeterPanel final : public juce::Component
{
public:
    MeterPanel (InvestigateProcessor& processorToView, DisplayContext& context);
    ~MeterPanel() override;

    /** The strip-relative layout of ui-spec 4.4 for the current height. */
    const LayoutMath::MeterStripLayout& getLayout() const noexcept { return layout; }

    /** Fired after the processor was asked to reset the statistics, so the
        meter content can clear its held values (peak-hold ticks, TP hold,
        crest window) at once (ui-spec 6, Actions). The panel clears its own
        held values before it calls this. */
    std::function<void()> onStatisticsReset;

    /** What the editor knows about the spectrum's repaints this vblank. */
    struct UpdateHints
    {
        bool spectrumPaced = false;      // the spectrum repaints at most every 30 ms
        bool spectrumRepainted = false;  // it requested a repaint on this vblank
    };

    /** One call per vblank. `nowMs` is a monotonic millisecond clock. Applies
        the ballistics, refreshes the text at 10 Hz and requests repaints for
        what changed. Does nothing while the panel is hidden (collapsed). */
    void update (const MeterSnapshot& snapshot, double nowMs, UpdateHints hints = {});

    /** Clears the held values the editor keeps (peak and TP hold, the bar
        fall, the crest ring). The snapshot's own maxima and counts are
        cleared by the processor. */
    void resetHeldValues();

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    //==========================================================================
    // Readouts, for tests and tools. The index addresses the text cells.
    enum TableRow { rowPeak, rowPeakMax, rowTruePeak, rowTruePeakMax, rowHeadroom, rowRms, rowCrest, rowClips, rowTpClips, numTableRows };
    enum LoudnessCell { loudM, loudS, loudI, loudMMax, loudSMax, loudLra, numLoudnessCells };

    static constexpr int tableCellIndex (int row, int channel) noexcept { return row * 2 + channel; }
    static constexpr int loudnessCellIndex (int cell) noexcept { return numTableRows * 2 + cell; }
    static constexpr int correlationCellIndex = numTableRows * 2 + numLoudnessCells;
    static constexpr int balanceCellIndex = correlationCellIndex + 1;
    static constexpr int numCells = balanceCellIndex + 1;

    juce::String getCellText (int cellIndex) const;
    bool isCellLit (int cellIndex) const noexcept;

    /** The quantised bar state of a channel in physical pixels (the values
        the repaint decision compares). `-1` for a part that is not drawn. */
    struct BarPixels
    {
        int rmsY = -1;
        int peakY = -1;
        int holdY = -1;
        bool lampLit = false;

        bool operator== (const BarPixels&) const = default;
    };

    BarPixels getBarPixels (int channel) const noexcept { return wanted[(size_t) channel]; }
    float getBarPeakDb (int channel) const noexcept { return channels[(size_t) channel].bar.value; }
    float getHeldPeakDb (int channel) const noexcept { return channels[(size_t) channel].peakHold.value; }
    float getCorrelationShown() const noexcept { return correlationFilter.value; }
    bool isStale() const noexcept { return stale.stale; }
    bool isMonoShown() const noexcept { return monoShown; }
    MeterScale getScaleShown() const noexcept { return scaleShown; }
    int getBarRepaintCount() const noexcept { return barRepaints; }
    int getTextRepaintCount() const noexcept { return textRepaints; }

private:
    class TooltipZone;

    struct ChannelState
    {
        MeterMath::FallingLevel bar;
        MeterMath::HeldLevel peakHold;
        MeterMath::HeldLevel truePeakHold;
        MeterMath::CrestRing crest;
    };

    struct TextCell
    {
        MeterMath::FixedText text;
        juce::Rectangle<int> bounds, repaintBounds;
        int type = 0;                 // tokens::Type
        bool rightAligned = true;
        bool lit = false;             // clip lamp
        bool error = false;           // headroom below zero
        bool dim = false;             // mono correlation block
        bool hidden = false;
        bool hasUnit = false;         // LRA "LU"
        bool dirty = false;
    };

    struct Geometry
    {
        MeterMath::BarGeometry bar;
        int xPx[2] {};                // left edge of each track, physical
        int widthPx = 0;
        int corrBarX0 = 0, corrBarX1 = 0, corrCentre = 0;
        int corrY0 = 0, corrY1 = 0;
        float scale = 1.0f;
    };

    InvestigateLookAndFeel& getLaf() const;
    void layoutCells();
    Geometry computeGeometry (float physScale) const;
    BarPixels computeBarPixels (int channel, const Geometry&) const noexcept;
    int computeCorrelationX (const Geometry&) const noexcept;
    void applySnapshot (const MeterSnapshot&, double nowMs);
    void refreshText (const MeterSnapshot&, double nowMs);
    void setCell (int index, const MeterMath::FixedText&, bool lit = false, bool error = false, bool hidden = false, bool dim = false);
    void flushRepaints();

    juce::Rectangle<int> barRepaintRect (int channel) const;
    juce::Rectangle<int> correlationBarRect() const;

    void rebuildCacheIfNeeded (float physScale);
    void renderCache (float physScale);
    void paintBars (juce::Graphics&, const Geometry&, juce::Rectangle<int> clip);
    void paintCorrelation (juce::Graphics&, const Geometry&, juce::Rectangle<int> clip);
    void paintCells (juce::Graphics&, juce::Rectangle<int> clip);
    void flushList (juce::Graphics&, int listIndex, juce::Colour);
    void addPixelRect (int listIndex, const Geometry&, int x0, int y0, int x1, int y1);

    InvestigateProcessor& processor;
    LayoutMath::MeterStripLayout layout;

    ParamControl scale;
    ParamControl rms;
    HeaderButton resetButton;
    std::unique_ptr<TooltipZone> correlationTip;

    std::atomic<float>* meterScaleValue = nullptr;
    std::atomic<float>* rmsWindowValue = nullptr;

    // Ballistics and derived state.
    std::array<ChannelState, 2> channels;
    MeterMath::CorrelationFilter correlationFilter;
    MeterMath::StaleDetector stale;
    MeterSnapshot effective;           // the snapshot with the stale rule applied
    MeterScale scaleShown = MeterScale::dBFS;
    double rmsWindowMs = 300.0;
    bool monoShown = false;
    double lastUpdateMs = -1.0;
    double lastTextMs = -1.0e12;
    // Same clock as the nowMs passed to update() (juce hi-res millisecond counter).
    double forceTextUntilMs = -1.0e12;
    double lastFlushMs = -1.0e12;
    bool haveSnapshot = false;

    // What the bars and the correlation bar show, and what was last requested.
    float pixelScale = 1.0f;
    std::array<BarPixels, 2> wanted, requested;
    int wantedCorrX = -1, requestedCorrX = -2;
    bool barDirty[2] { false, false };
    bool corrDirty = false;
    bool invalidateRequested = true;
    int barRepaints = 0, textRepaints = 0;

    std::array<TextCell, numCells> cells;

    // Reused rectangle lists, one per colour (sized once).
    std::array<juce::RectangleList<float>, 8> lists;

    // Static layer.
    struct CacheKey
    {
        int w = 0, h = 0;
        float scale = 0.0f;
        MeterScale meterScale = MeterScale::dBFS;
        bool mono = false;

        bool operator== (const CacheKey&) const = default;
    };

    juce::Image cache;
    CacheKey cacheKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterPanel)
};

} // namespace investigate
