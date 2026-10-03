#pragma once

#include "DisplayContext.h"
#include "LayoutMath.h"
#include "SpectrumMath.h"

#include "../dsp/SpectrumFrame.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace investigate
{

class InvestigateProcessor;
class InvestigateLookAndFeel;

/**
    The spectrum plot (T-011 region and frame, T-012 content; ui-spec 4.1, 5).
    It draws the cached grid, every curve of the newest `SpectrumFrame`, the
    legend, the hover readout, the FROZEN badge and the toasts, and handles
    click (reset) and right-click (copy frequency).

    Frames are pulled from `pollFrame()`, which the editor calls from its one
    `VBlankAttachment` (the same callback that refreshes `DisplayContext`, so
    the range and the sample rate are always current when a frame is drawn).
    The component covers exactly the plot region; `getDataRect()` is the
    plotted area in its own coordinates.

    Rendering (T-029). Under Direct2D the cost of a frame is mostly work done
    after `paint()` returns, so `paint()` is shaped around what Direct2D does
    cheaply: fills are lists of axis-aligned rectangles (a GPU sprite batch),
    not polygons; strokes drop vertices that lie within 0.04 physical pixels of
    a straight line; only the pixel columns under the clip are traced. Frames
    are paced to 30 fps above a plot size or layer count (`SpectrumMath::pacing`).
    The software renderer keeps the original polygon fills and exact vertices.

    Message-thread only. No allocation in `paint()` or `pollFrame()` after the
    first frame beyond what JUCE's own rasteriser and text layout do
    internally: paths, column and point buffers are reused, and the hover
    strings are rebuilt only when the pointer or a frame changes.
*/
class SpectrumView final : public juce::Component
{
public:
    SpectrumView (InvestigateProcessor& processorToView, DisplayContext& displayContextToUse);
    ~SpectrumView() override;

    /** The Freeze view flag (ui-spec 5.7). While frozen no frames are pulled;
        the last frame stays and the hover readout keeps working on it. */
    void setFrozen (bool shouldBeFrozen);
    bool isFrozen() const noexcept { return frozen; }

    /** Called once per vblank by the editor, after `DisplayContext::refresh()`.
        Pulls the newest frame (unless frozen), notices range and setting
        changes, advances the toasts, and repaints only what changed. */
    void pollFrame();

    /** True when new frames repaint at most every 30 ms (T-029 pacing), and
        whether the last `pollFrame()` asked for a curve repaint. The meter
        panel aligns its own repaints with these (T-029 rule 5), so both share
        one window present. */
    bool isFramePaced() const noexcept { return isPaced(); }
    bool requestedRepaintInLastPoll() const noexcept { return pollRequestedRepaint; }

    /** The plotted area inside the gutters, in this component's coordinates. */
    juce::Rectangle<int> getDataRect() const noexcept;

    // Test and tooling access.
    bool hasFrame() const noexcept { return haveFrame; }
    const SpectrumFrame& getFrame() const noexcept { return *frame; }
    SpectrumMath::Mapping getMapping() const noexcept { return currentMapping(); }

    /** Replaces the displayed frame (used by tools that drive the view with a
        synthetic frame; the processor path never calls this). */
    void setFrameForTesting (const SpectrumFrame& newFrame);

    /** Places the hover pointer at a component-local position, as a mouse move
        would. Tools and tests use it; the UI path uses the mouse callbacks. */
    void setHoverForTesting (juce::Point<float> position) { setHoverFromPosition (position); }

    /** True while a new frame's repaint is being held back by the pacing rule
        (30 ms between curve repaints), until the next `pollFrame()` issues it. */
    bool isFrameRepaintPendingForTesting() const noexcept { return frameRepaintPending; }

    /** The hover frequency's clipboard text for a component-local x, as
        right-click would copy it. */
    juce::String clipboardTextForX (float x) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::MouseCursor getMouseCursor() override;

private:
    enum class Swatch { primaryA, primaryB, secondaryA, secondaryAB, sidechain };

    struct LegendEntry
    {
        Swatch swatch = Swatch::primaryA;
        juce::String name;
        int x = 0;          // swatch x inside the legend box
        int textX = 0;
        int textW = 0;
    };

    struct HoverRow
    {
        Swatch swatch = Swatch::primaryA;
        juce::String name, value;
        double db = 0.0;
        juce::Colour marker;
        float nameW = 0.0f, valueW = 0.0f;
    };

    struct Toast
    {
        juce::String text;
        juce::Rectangle<int> bounds;
        juce::uint32 startMs = 0;
        int durationMs = 0;
        bool active = false;
    };

    // Settings captured with the frame the legend and card describe.
    struct FrameSettings
    {
        int channelMode = 0;
        int secondary = 0;
    };

    InvestigateLookAndFeel& getLaf();
    SpectrumMath::Mapping currentMapping() const noexcept;

    void rebuildCacheIfNeeded (float scale);
    void renderCache (float scale, const SpectrumMath::Mapping& mapping);

    bool showSidechainNow() const noexcept;
    void updateLegend();
    void paintCurves (juce::Graphics&, float pixelScale);
    void paintLegend (juce::Graphics&);
    void paintHover (juce::Graphics&);
    void paintBadge (juce::Graphics&);
    void paintToasts (juce::Graphics&);
    void drawSwatch (juce::Graphics&, Swatch, int x, int y, float alpha) const;
    void rebuildGradient();
    bool isPaced() const noexcept;
    void requestFrameRepaint();
    struct TraceRange
    {
        float left = 0.0f;       // x of column 0
        int firstCol = 0, lastCol = 0;
    };

    void tracePolyline (juce::Path& line, juce::Path* fill, float bottomY, int numCols, float pixelScale,
                        float tolerancePx, const TraceRange&) const;
    void fillUnderCurve (juce::Graphics&, const std::array<float, SpectrumFrame::maxPoints>& curve,
                         const SpectrumMath::Mapping&, int numPoints, float pixelScale, int numCols,
                         const TraceRange&, juce::Colour colour, float flatAlpha, bool gradient);
    void flushFillRects (juce::Graphics&, juce::Colour);

    void updateHover();
    void setHoverFromPosition (juce::Point<float> position);
    void clearHover();
    juce::Rectangle<int> hoverOverlayBounds() const;
    juce::Rectangle<int> legendBounds() const;
    juce::Rectangle<int> hintBounds() const;
    bool pointerNearLegend() const;

    void showToast (Toast& toast, const juce::String& text, juce::Point<int> anchor, bool centredOnX);

    static juce::String curveName (int channelMode, bool dual, bool second);

    InvestigateProcessor& processor;
    DisplayContext& displayContext;
    InvestigateLookAndFeel* laf = nullptr;

    std::atomic<float>* channelModeValue = nullptr;
    std::atomic<float>* secondaryValue = nullptr;
    std::atomic<float>* showSidechainValue = nullptr;

    bool frozen = false;

    // The frame being shown (about 49 KB, so on the heap).
    std::unique_ptr<SpectrumFrame> frame;
    bool haveFrame = false;
    FrameSettings frameSettings;
    bool sidechainActive = false;
    bool showSidechainLast = true;

    // Reused buffers (sized once).
    std::vector<float> pointX;
    std::vector<SpectrumMath::Column> columns;
    std::vector<float> pointY, columnTops;
    std::vector<int> columnTopPx;
    juce::RectangleList<float> fillRects;
    juce::FillType primaryFill;
    juce::Path pathA, pathScratch, fillPath, scSwatchPath, scSwatchFill;

    // Static layer: background, grid and labels.
    struct CacheKey
    {
        int w = 0, h = 0;
        float scale = 0.0f;
        double fLo = 0.0, fHi = 0.0, dbLo = 0.0, dbHi = 0.0;

        bool operator== (const CacheKey&) const = default;
    };

    juce::Image cache;
    CacheKey cacheKey;
    ValueFormat::EffectiveRange lastRange;

    // Legend
    std::array<LegendEntry, 4> legend;
    int legendCount = 0;
    int legendW = 0;
    int legendKeyCode = -1;
    std::array<float, 5> nameWidths {};   // legend name widths by Swatch, reused by the hover card
    float monoAdvanceReadout = 0.0f, monoAdvanceHover = 0.0f, badgeTextW = 0.0f;
    juce::String frozenText;

    // Hover
    bool hovering = false;
    juce::Point<float> hoverPos;
    double hoverFreq = 0.0;
    juce::String hoverFreqText, hoverNoteText;
    std::array<HoverRow, 3> hoverRows;
    int hoverRowCount = 0;
    juce::Rectangle<int> cardBounds;
    juce::Rectangle<int> lastOverlay;
    bool legendDimmed = false;
    juce::String hintText;
    float hintTextW = 0.0f;

    // Frame pacing (ui-spec 12 fallback, T-029; see `SpectrumMath::pacing`):
    // when paced, new frames repaint the plot at most every `minIntervalMs`.
    // The newest frame is always the one painted; only the repaint request is
    // deferred.
    float lastPixelScale = 1.0f;
    double lastFrameRepaintMs = 0.0;
    bool frameRepaintPending = false;
    bool pollRequestedRepaint = false;

    // Interaction
    bool leftDown = false;
    juce::Point<float> leftDownPos;

    Toast resetToast, copyToast;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};

} // namespace investigate
