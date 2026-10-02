#include "SpectrumView.h"

#include "DesignTokens.h"
#include "InvestigateLookAndFeel.h"
#include "ValueFormat.h"

#include "../PluginProcessor.h"
#include "../params/ParameterIDs.h"

#include <cmath>

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;
namespace sp = tokens::spectrum;

namespace
{
    // ui-spec 12 fallback: flip to false to replace the primary gradient with
    // a flat 14 % fill if the paint budget is missed.
    constexpr bool useGradientFill = true;

    // Safe upper bound for physical pixel columns (2560 wide at up to 6x).
    constexpr int maxColumns = 16384;

    // Largest vertex count of one curve: 2 per column, at most 1 per point.
    constexpr int maxVertices = 2 * SpectrumFrame::maxPoints + 8;

    constexpr int secondaryReset = 2; // `secondary` choices 2 and 3 are the all-time curves

    float snapToPixel (double v, float scale) noexcept
    {
        return (float) (std::round (v * (double) scale) / (double) scale);
    }
}

//==============================================================================
SpectrumView::SpectrumView (InvestigateProcessor& processorToView, DisplayContext& displayContextToUse)
    : processor (processorToView),
      displayContext (displayContextToUse),
      frame (std::make_unique<SpectrumFrame>())
{
    auto& apvts = processor.getApvts();
    channelModeValue = apvts.getRawParameterValue (ParamIDs::channelMode);
    secondaryValue = apvts.getRawParameterValue (ParamIDs::secondary);
    showSidechainValue = apvts.getRawParameterValue (ParamIDs::showSidechain);

    setOpaque (true);
    setTitle ("Spectrum");
    setDescription ("Spectrum display. Hover for a readout; click to reset the max and average curves; "
                    "right-click to copy the frequency.");

    pointX.assign ((size_t) SpectrumFrame::maxPoints, 0.0f);
    columns.assign ((size_t) maxColumns, {});

    for (auto* p : { &pathA, &pathScratch, &fillPath })
        p->preallocateSpace (3 * maxVertices + 16);

    // The sidechain legend swatch (ui-spec 5.1 mockup SWATCH.sc), 16 x 10.
    const float w = (float) sp::swatchW, h = (float) sp::swatchH, mid = h * 0.5f;
    scSwatchPath.startNewSubPath (0.0f, mid);
    scSwatchPath.cubicTo (4.0f, mid, 5.0f, 2.0f, 8.0f, 2.0f);
    scSwatchPath.cubicTo (11.0f, 2.0f, 12.0f, mid, w, mid);
    scSwatchFill = scSwatchPath;
    scSwatchFill.lineTo (w, h);
    scSwatchFill.lineTo (0.0f, h);
    scSwatchFill.closeSubPath();

    hintText = juce::String ("Click: reset max/avg curves ") + juce::String::fromUTF8 ("\xC2\xB7")
             + " Right-click: copy frequency";

    lastRange = displayContext.effective();
}

SpectrumView::~SpectrumView() = default;

//==============================================================================
InvestigateLookAndFeel& SpectrumView::getLaf()
{
    if (laf == nullptr)
    {
        laf = &InvestigateLookAndFeel::from (*this);
        hintTextW = laf->getTextWidth (hintText, tk::Type::hint);

        // The readout faces are monospaced, so their text widths follow from one
        // measured advance; this keeps hover updates free of glyph layout.
        constexpr int sample = 10;
        monoAdvanceReadout = laf->getTextWidth (juce::String::repeatedString ("0", sample), tk::Type::readout) / (float) sample;
        monoAdvanceHover = laf->getTextWidth (juce::String::repeatedString ("0", sample), tk::Type::readoutHover) / (float) sample;
        frozenText = "Frozen";
        badgeTextW = laf->getTextWidth (frozenText, tk::Type::capsBadge);
    }

    return *laf;
}

void SpectrumView::lookAndFeelChanged()
{
    laf = nullptr;
    cacheKey = {};
    legendKeyCode = -1;
    repaint();
}

void SpectrumView::setFrozen (bool shouldBeFrozen)
{
    if (frozen == shouldBeFrozen)
        return;

    frozen = shouldBeFrozen;
    repaint();
}

juce::Rectangle<int> SpectrumView::getDataRect() const noexcept
{
    return LayoutMath::computeDataRect (getWidth(), getHeight());
}

SpectrumMath::Mapping SpectrumView::currentMapping() const noexcept
{
    const auto& r = displayContext.effective();
    const auto d = getDataRect();

    SpectrumMath::Mapping m;
    m.fLo = r.freqLow;
    m.fHi = r.freqHigh;
    m.dbLo = r.dbLow;
    m.dbHi = r.dbHigh;
    m.x0 = d.getX();
    m.y0 = d.getY();
    m.w = d.getWidth();
    m.h = d.getHeight();
    return m;
}

juce::String SpectrumView::clipboardTextForX (float x) const
{
    return SpectrumMath::formatFrequencyForClipboard (currentMapping().xToFreq ((double) x));
}

//==============================================================================
void SpectrumView::resized()
{
    rebuildGradient();
    cacheKey = {};
    resetToast.active = false;
    copyToast.active = false;
    clearHover();
}

void SpectrumView::rebuildGradient()
{
    const auto d = getDataRect();
    juce::ColourGradient gradient (tk::colour::primaryA.withAlpha (tk::alpha::primaryFillTop), 0.0f, (float) d.getY(),
                                   tk::colour::primaryA.withAlpha (tk::alpha::primaryFillBottom), 0.0f, (float) d.getBottom(),
                                   false);
    primaryFill = juce::FillType (gradient);
}

//==============================================================================
// Frame pulling and settings

bool SpectrumView::showSidechainNow() const noexcept
{
    return haveFrame && frame->hasSidechain && sidechainActive && showSidechainValue->load() >= 0.5f;
}

void SpectrumView::setFrameForTesting (const SpectrumFrame& newFrame)
{
    *frame = newFrame;
    haveFrame = true;
    frameSettings = { (int) channelModeValue->load(), (int) secondaryValue->load() };
    sidechainActive = processor.isSidechainActive();
    showSidechainLast = showSidechainValue->load() >= 0.5f;
    updateLegend();

    if (hovering)
        updateHover();

    repaint (getDataRect());
}

void SpectrumView::pollFrame()
{
    bool needsPaint = false;

    const auto& range = displayContext.effective();

    if (range.freqLow != lastRange.freqLow || range.freqHigh != lastRange.freqHigh
        || range.dbLow != lastRange.dbLow || range.dbHigh != lastRange.dbHigh)
    {
        lastRange = range;
        needsPaint = true;

        if (hovering)
            updateHover();
    }

    const bool scActive = processor.isSidechainActive();
    const bool scShown = showSidechainValue->load() >= 0.5f;

    if (scActive != sidechainActive || scShown != showSidechainLast)
    {
        sidechainActive = scActive;
        showSidechainLast = scShown;
        updateLegend();

        if (hovering)
            updateHover();

        needsPaint = true;
    }

    if (! frozen && processor.getLatestSpectrumFrame (*frame))
    {
        haveFrame = true;
        frameSettings = { (int) channelModeValue->load(), (int) secondaryValue->load() };
        updateLegend();

        if (hovering)
            updateHover();

        needsPaint = true;
    }

    // Toasts repaint only their own bounds, and only while fading (ui-spec 5.6).
    const auto now = juce::Time::getMillisecondCounter();

    for (auto* toast : { &resetToast, &copyToast })
    {
        if (! toast->active)
            continue;

        const int elapsed = (int) (now - toast->startMs);

        if (elapsed >= toast->durationMs)
        {
            toast->active = false;
            repaint (toast->bounds);
        }
        else if (elapsed >= toast->durationMs - tk::motion::toastFadeMs)
        {
            repaint (toast->bounds);
        }
    }

    if (needsPaint)
    {
        lastOverlay = hoverOverlayBounds();
        repaint (getDataRect());
    }
}

//==============================================================================
// Names and legend

juce::String SpectrumView::curveName (int channelMode, bool dual, bool second)
{
    static constexpr std::array<const char*, 5> singleNames { "Mid", "Side", "Left", "Right", "L+R" };

    if (dual || channelMode >= 5)
    {
        const bool midSide = channelMode == 6;

        if (second)
            return midSide ? "S" : "R";

        return midSide ? "M" : "L";
    }

    return singleNames[(size_t) std::clamp (channelMode, 0, 4)];
}

namespace
{
    juce::String secondaryName (int secondaryChoice)
    {
        static constexpr std::array<const char*, 3> names { "RT max", "All-time max", "All-time avg" };
        return names[(size_t) std::clamp (secondaryChoice - 1, 0, 2)];
    }
}

void SpectrumView::updateLegend()
{
    const bool dual = frame->isDual;
    const bool secondaryShown = frame->hasSecondary;
    const bool sidechainShown = showSidechainNow();

    const int code = (frameSettings.channelMode << 8) | (frameSettings.secondary << 4)
                   | (dual ? 4 : 0) | (secondaryShown ? 2 : 0) | (sidechainShown ? 1 : 0);

    if (code == legendKeyCode)
        return;

    legendKeyCode = code;
    auto& look = getLaf();

    legendCount = 0;
    int legendX = 0;

    auto add = [&] (Swatch swatch, const juce::String& name)
    {
        auto& e = legend[(size_t) legendCount++];
        e.swatch = swatch;
        e.name = name;
        e.x = legendX;
        e.textX = legendX + sp::swatchW + sp::swatchTextGap;
        const float nameW = look.getTextWidth (name, tk::Type::bodySm);
        e.textW = (int) std::ceil (nameW);
        nameWidths[(size_t) swatch] = nameW;
        legendX = e.textX + e.textW + sp::legendGap;
    };

    add (Swatch::primaryA, curveName (frameSettings.channelMode, dual, false));

    if (dual)
        add (Swatch::primaryB, curveName (frameSettings.channelMode, dual, true));

    if (secondaryShown)
        add (dual ? Swatch::secondaryAB : Swatch::secondaryA, secondaryName (frameSettings.secondary));

    if (sidechainShown)
        add (Swatch::sidechain, "Sidechain");

    legendW = 2 * sp::legendPadX + (legendX - sp::legendGap);
}

juce::Rectangle<int> SpectrumView::legendBounds() const
{
    const auto d = getDataRect();
    return { d.getX() + sp::legendInset, d.getY() + sp::legendInset, legendW, sp::legendH };
}

juce::Rectangle<int> SpectrumView::hintBounds() const
{
    const auto d = getDataRect();
    const int w = (int) std::ceil (hintTextW) + 2 * sp::hintPadX;
    const int h = sp::hintLineH + 2 * sp::hintPadY;
    return { d.getRight() - sp::hintRight - w, d.getBottom() - sp::hintBottom - h, w, h };
}

bool SpectrumView::pointerNearLegend() const
{
    return hovering && haveFrame && legendBounds().expanded (sp::legendDimDistance).contains (hoverPos.toInt());
}

//==============================================================================
// Hover model

void SpectrumView::updateHover()
{
    const auto m = currentMapping();
    const auto d = getDataRect();

    hoverFreq = m.xToFreq ((double) hoverPos.x);
    hoverFreqText = SpectrumMath::formatFrequency (hoverFreq);
    hoverNoteText = SpectrumMath::noteName (hoverFreq);
    hoverRowCount = 0;

    if (! haveFrame)
    {
        cardBounds = {};
        return;
    }

    const int n = std::clamp (frame->numPoints, 0, SpectrumFrame::maxPoints);
    const float* freq = frame->freqHz.data();
    const bool dual = frame->isDual;

    auto addRow = [&] (Swatch swatch, const juce::String& name, const std::array<float, SpectrumFrame::maxPoints>& curve,
                       juce::Colour marker)
    {
        auto& row = hoverRows[(size_t) hoverRowCount++];
        row.swatch = swatch;
        row.name = name;
        row.db = SpectrumMath::interpolateLevel (freq, curve.data(), n, hoverFreq);
        row.value = SpectrumMath::formatLevel (row.db);
        row.marker = marker;
        row.nameW = nameWidths[(size_t) swatch];
        row.valueW = monoAdvanceReadout * (float) row.value.length();
    };

    addRow (Swatch::primaryA, curveName (frameSettings.channelMode, dual, false), frame->primaryDb, tk::colour::primaryA);

    if (dual)
        addRow (Swatch::primaryB, curveName (frameSettings.channelMode, dual, true), frame->primaryBDb, tk::colour::primaryB);
    else if (frame->hasSecondary)
        addRow (Swatch::secondaryA, secondaryName (frameSettings.secondary), frame->secondaryDb, tk::colour::secondaryA);

    if (showSidechainNow())
        addRow (Swatch::sidechain, "Sidechain", frame->sidechainDb, tk::colour::sidechain);

    float contentW = std::max (monoAdvanceHover * (float) hoverFreqText.length(),
                               monoAdvanceReadout * (float) hoverNoteText.length());

    for (int i = 0; i < hoverRowCount; ++i)
    {
        const auto& row = hoverRows[(size_t) i];
        contentW = std::max (contentW, (float) (sp::swatchW + sp::cardSwatchGap + sp::cardValueGap) + row.nameW + row.valueW);
    }

    const int cardW = std::max (sp::cardMinW, (int) std::ceil (contentW) + 2 * (sp::cardPadX + sp::cardBorder));
    const int cardH = 2 * (sp::cardPadY + sp::cardBorder) + sp::cardFreqLineH + sp::cardNoteLineH
                    + 2 * sp::cardSepMargin + 1 + hoverRowCount * sp::cardRowH;

    const auto topLeft = SpectrumMath::placeHoverCard (hoverPos, cardW, cardH, d);
    cardBounds = { topLeft.x, topLeft.y, cardW, cardH };
}

juce::Rectangle<int> SpectrumView::hoverOverlayBounds() const
{
    if (! hovering || ! haveFrame)
        return {};

    const auto d = getDataRect();
    const int x = (int) std::floor (hoverPos.x);
    const juce::Rectangle<int> crosshair (x - sp::crosshairSpan, d.getY(), 2 * sp::crosshairSpan + 1, d.getHeight());
    return crosshair.getUnion (cardBounds.expanded (1));
}

void SpectrumView::setHoverFromPosition (juce::Point<float> position)
{
    if (! getDataRect().toFloat().contains (position))
    {
        clearHover();
        return;
    }

    const bool wasHovering = hovering;
    hovering = true;
    hoverPos = position;
    updateHover();

    const auto next = hoverOverlayBounds();
    auto dirty = lastOverlay.getUnion (next);
    lastOverlay = next;

    if (! wasHovering)
    {
        dirty = dirty.getUnion (hintBounds());
        updateMouseCursor();
    }

    const bool dim = pointerNearLegend();

    if (dim != legendDimmed)
    {
        legendDimmed = dim;
        dirty = dirty.getUnion (legendBounds());
    }

    repaint (dirty);
}

void SpectrumView::clearHover()
{
    if (! hovering)
        return;

    hovering = false;
    auto dirty = lastOverlay.getUnion (hintBounds());

    if (legendDimmed)
        dirty = dirty.getUnion (legendBounds());

    legendDimmed = false;
    lastOverlay = {};
    hoverRowCount = 0;
    updateMouseCursor();
    repaint (dirty);
}

juce::MouseCursor SpectrumView::getMouseCursor()
{
    return hovering ? juce::MouseCursor (juce::MouseCursor::CrosshairCursor) : juce::MouseCursor();
}

void SpectrumView::mouseEnter (const juce::MouseEvent& e) { setHoverFromPosition (e.position); }
void SpectrumView::mouseMove (const juce::MouseEvent& e)  { setHoverFromPosition (e.position); }
void SpectrumView::mouseDrag (const juce::MouseEvent& e)  { setHoverFromPosition (e.position); }
void SpectrumView::mouseExit (const juce::MouseEvent&)    { clearHover(); }

//==============================================================================
// Click and right-click (ui-spec 5.6)

void SpectrumView::showToast (Toast& toast, const juce::String& text, juce::Point<int> anchor, bool centredOnX)
{
    const auto d = getDataRect();
    const int w = (int) std::ceil (getLaf().getTextWidth (text, tk::Type::bodySm)) + 2 * (sp::toastPadX + 1);
    const int h = sp::toastLineH + 2 * (sp::toastPadY + 1);

    int x = anchor.x;
    int y = anchor.y;

    if (centredOnX)
    {
        x -= w / 2;
    }
    else
    {
        x = std::clamp (anchor.x + sp::toastCopyDx, d.getX() + sp::toastInset, std::max (d.getX() + sp::toastInset, d.getRight() - sp::toastInset - w));
        y = std::max (d.getY() + sp::toastInset, anchor.y - sp::toastCopyDy);
    }

    repaint (toast.bounds);
    toast.text = text;
    toast.bounds = { x, y, w, h };
    toast.startMs = juce::Time::getMillisecondCounter();
    toast.durationMs = centredOnX ? tk::motion::toastResetMs : tk::motion::toastCopyMs;
    toast.active = true;
    repaint (toast.bounds);
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    if (! getDataRect().toFloat().contains (e.position))
        return;

    if (e.mods.isRightButtonDown())
    {
        const auto text = clipboardTextForX (e.position.x);
        juce::SystemClipboard::copyTextToClipboard (text);
        showToast (copyToast, "Copied " + text + " Hz", e.getPosition(), false);
        return;
    }

    if (e.mods.isLeftButtonDown())
    {
        leftDown = true;
        leftDownPos = e.position;
    }
}

void SpectrumView::mouseUp (const juce::MouseEvent& e)
{
    if (! leftDown)
        return;

    leftDown = false;

    if (e.position.getDistanceFrom (leftDownPos) > (float) L::clickSlopPx)
        return;

    processor.requestSpectrumReset();

    // Only the all-time curves visibly change, so only they get a toast.
    if ((int) secondaryValue->load() >= secondaryReset)
    {
        const auto d = getDataRect();
        showToast (resetToast, "Max/avg curves reset",
                   { d.getCentreX(), d.getY() + (frozen ? sp::toastResetYFrozen : sp::toastResetY) }, true);
    }
}

//==============================================================================
// Painting

void SpectrumView::rebuildCacheIfNeeded (float scale)
{
    const auto m = currentMapping();
    const CacheKey key { getWidth(), getHeight(), scale, m.fLo, m.fHi, m.dbLo, m.dbHi };

    if (key == cacheKey && cache.isValid())
        return;

    renderCache (scale, m);
    cacheKey = key;
}

void SpectrumView::renderCache (float scale, const SpectrumMath::Mapping& m)
{
    const int pw = std::max (1, (int) std::ceil ((double) getWidth() * scale));
    const int ph = std::max (1, (int) std::ceil ((double) getHeight() * scale));

    if (! cache.isValid() || cache.getWidth() != pw || cache.getHeight() != ph)
        cache = juce::Image (juce::Image::RGB, pw, ph, false);

    juce::Graphics g (cache);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (tk::colour::bgPlot);

    if (! (m.fHi > m.fLo) || ! (m.dbHi > m.dbLo))
        return;

    auto& look = getLaf();
    const auto data = getDataRect();
    const float lineW = 1.0f / scale;

    // Frequency lines and labels (ui-spec 5.3).
    const int labelSet = SpectrumMath::chooseFrequencyLabelSet (m.fLo, m.fHi, m.w);
    const bool minorVisible = SpectrumMath::minorFrequencyLinesVisible (m.fLo, m.fHi, m.w);

    for (int k = (int) std::floor (std::log10 (m.fLo)); k <= (int) std::ceil (std::log10 (m.fHi)); ++k)
    {
        for (int mult = 1; mult <= 9; ++mult)
        {
            const double f = (double) mult * std::pow (10.0, k);

            if (f < m.fLo * (1.0 - 1.0e-9) || f > m.fHi * (1.0 + 1.0e-9))
                continue;

            const bool labelled = SpectrumMath::isLabelledMultiple (labelSet, mult);

            if (mult != 1 && ! labelled && ! minorVisible)
                continue;

            const double x = m.freqToX (f);
            g.setColour (mult == 1 ? tk::colour::gridMajor : tk::colour::gridMinor);
            g.fillRect (snapToPixel (x, scale), (float) data.getY(), lineW, (float) data.getHeight());

            if (! labelled)
                continue;

            const auto text = SpectrumMath::formatFrequencyLabel (f);
            const float textW = look.getTextWidth (text, tk::Type::axis);

            if (x - textW * 0.5 < (double) sp::labelEdgeMargin || x + textW * 0.5 > (double) (getWidth() - sp::labelEdgeMargin))
                continue;

            const int boxW = (int) std::ceil (textW) + sp::axisLabelPadW;
            look.drawText (g, text, { (int) std::lround (x) - boxW / 2, data.getBottom() + sp::freqLabelTop, boxW, sp::axisLabelH },
                           tk::Type::axis, tk::colour::textTertiary, juce::Justification::centred);
        }
    }

    // dB lines and labels.
    const double step = SpectrumMath::chooseDbStep (m.dbLo, m.dbHi, m.h);
    const int first = (int) std::ceil (m.dbLo / step - 1.0e-9);
    const int last = (int) std::floor (m.dbHi / step + 1.0e-9);

    for (int i = first; i <= last; ++i)
    {
        const double db = (double) i * step;
        const double y = m.dbToY (db);

        g.setColour (i == 0 ? tk::colour::gridMajor : tk::colour::gridMinor);
        g.fillRect ((float) data.getX(), snapToPixel (y, scale), (float) data.getWidth(), lineW);

        look.drawText (g, ValueFormat::formatSignedDb ((float) db),
                       { 0, (int) std::lround (y) - sp::axisLabelH / 2, data.getX() - sp::dbLabelGap, sp::axisLabelH },
                       tk::Type::axis, tk::colour::textTertiary, juce::Justification::centredRight);
    }
}

void SpectrumView::tracePolyline (juce::Path& line, juce::Path* fill, float bottomY, int numCols) const
{
    line.clear();

    if (fill != nullptr)
        fill->clear();

    bool started = false;
    float prevY = 0.0f, lastX = 0.0f;

    auto add = [&] (float x, float y)
    {
        if (! started)
        {
            line.startNewSubPath (x, y);

            if (fill != nullptr)
            {
                fill->startNewSubPath (x, bottomY);
                fill->lineTo (x, y);
            }

            started = true;
        }
        else
        {
            x = std::max (x, lastX);
            line.lineTo (x, y);

            if (fill != nullptr)
                fill->lineTo (x, y);
        }

        lastX = x;
        prevY = y;
    };

    for (int c = 0; c < numCols; ++c)
    {
        const auto& col = columns[(size_t) c];

        if (! col.used)
            continue;

        if (col.yTop == col.yBottom)
        {
            add (col.xTop, col.yTop);
            continue;
        }

        // Keep both extremes, ordered so the line moves toward the next
        // column without doubling back (ui-spec 5.1 curve geometry).
        const bool topFirst = ! started ? col.xTop <= col.xBottom
                                        : std::abs (prevY - col.yTop) <= std::abs (prevY - col.yBottom);

        if (topFirst)
        {
            add (col.xTop, col.yTop);
            add (col.xBottom, col.yBottom);
        }
        else
        {
            add (col.xBottom, col.yBottom);
            add (col.xTop, col.yTop);
        }
    }

    if (fill != nullptr && started)
    {
        fill->lineTo (lastX, bottomY);
        fill->closeSubPath();
    }
}

void SpectrumView::paintCurves (juce::Graphics& g, float pixelScale)
{
    const auto m = currentMapping();
    const int n = std::clamp (frame->numPoints, 0, SpectrumFrame::maxPoints);

    if (n <= 0 || ! (m.fHi > m.fLo))
        return;

    // x of every display point, shared by all curves of the frame.
    const double xScale = m.w / std::log (m.fHi / m.fLo);
    const float* freq = frame->freqHz.data();

    for (int i = 0; i < n; ++i)
        pointX[(size_t) i] = (float) (m.x0 + std::log ((double) freq[i] / m.fLo) * xScale);

    const int numCols = std::min ((int) std::ceil (m.w * (double) pixelScale), maxColumns);
    const float bottom = (float) (m.y0 + m.h);
    SpectrumMath::Column* cols = columns.data();

    auto reduce = [&] (const std::array<float, SpectrumFrame::maxPoints>& curve)
    {
        SpectrumMath::reduceToColumns (pointX.data(), curve.data(), n, m, pixelScale, cols, numCols);
    };

    const juce::PathStrokeType thin (sp::thinStroke, juce::PathStrokeType::beveled, juce::PathStrokeType::butt);
    const juce::PathStrokeType thick (tk::stroke::primary, juce::PathStrokeType::beveled, juce::PathStrokeType::butt);

    // 1: sidechain silhouette, behind everything.
    if (showSidechainNow())
    {
        reduce (frame->sidechainDb);
        tracePolyline (pathScratch, &fillPath, bottom, numCols);
        g.setColour (tk::colour::sidechain.withAlpha (tk::alpha::sidechainFill));
        g.fillPath (fillPath);
        g.setColour (tk::colour::sidechain.withAlpha (tk::alpha::sidechainStroke));
        g.strokePath (pathScratch, thin);
    }

    // 2: primary A fill.
    reduce (frame->primaryDb);
    tracePolyline (pathA, &fillPath, bottom, numCols);

    if constexpr (useGradientFill)
        g.setFillType (primaryFill);
    else
        g.setColour (tk::colour::primaryA.withAlpha (sp::primaryFillFlat));

    g.fillPath (fillPath);

    // 3 and 4: secondaries, opaque and thin.
    if (frame->hasSecondary)
    {
        reduce (frame->secondaryDb);
        tracePolyline (pathScratch, nullptr, bottom, numCols);
        g.setColour (tk::colour::secondaryA);
        g.strokePath (pathScratch, thin);

        if (frame->isDual)
        {
            reduce (frame->secondaryBDb);
            tracePolyline (pathScratch, nullptr, bottom, numCols);
            g.setColour (tk::colour::secondaryB);
            g.strokePath (pathScratch, thin);
        }
    }

    // 5: primary B, no fill.
    if (frame->isDual)
    {
        reduce (frame->primaryBDb);
        tracePolyline (pathScratch, nullptr, bottom, numCols);
        g.setColour (tk::colour::primaryB);
        g.strokePath (pathScratch, thick);
    }

    // 6: primary A stroke, on top.
    g.setColour (tk::colour::primaryA);
    g.strokePath (pathA, thick);
}

void SpectrumView::drawSwatch (juce::Graphics& g, Swatch swatch, int x, int y, float alpha) const
{
    namespace c = tk::colour;
    const float fx = (float) x, fy = (float) y, w = (float) sp::swatchW;

    auto bar = [&] (juce::Colour colour, float centreY, float thickness)
    {
        g.setColour (colour.withMultipliedAlpha (alpha));
        g.fillRect (fx, fy + centreY - thickness * 0.5f, w, thickness);
    };

    switch (swatch)
    {
        case Swatch::primaryA:
            g.setColour (c::primaryA.withAlpha (sp::primarySwatchFill * alpha));
            g.fillRect (fx, fy + 4.0f, w, 6.0f);
            bar (c::primaryA, 4.75f, tk::stroke::primary);
            break;

        case Swatch::primaryB:
            bar (c::primaryB, 5.0f, tk::stroke::primary);
            break;

        case Swatch::secondaryA:
            bar (c::secondaryA, 5.0f, sp::thinStroke);
            break;

        case Swatch::secondaryAB:
            bar (c::secondaryA, 3.5f, sp::thinStroke);
            bar (c::secondaryB, 7.5f, sp::thinStroke);
            break;

        case Swatch::sidechain:
        {
            const auto move = juce::AffineTransform::translation (fx, fy);
            g.setColour (c::sidechain.withAlpha (tk::alpha::sidechainFill * alpha));
            g.fillPath (scSwatchFill, move);
            g.setColour (c::sidechain.withAlpha (tk::alpha::sidechainStroke * alpha));
            g.strokePath (scSwatchPath, juce::PathStrokeType (sp::thinStroke), move);
            break;
        }
    }
}

void SpectrumView::paintLegend (juce::Graphics& g)
{
    if (legendCount == 0)
        return;

    auto& look = getLaf();
    const float alpha = legendDimmed ? sp::legendDimAlpha : 1.0f;
    const auto box = legendBounds();

    g.setColour (tk::colour::bgPlot.withAlpha (tk::alpha::legendFill * alpha));
    g.fillRoundedRectangle (box.toFloat(), tk::radius::small);

    const int swatchY = box.getY() + (box.getHeight() - sp::swatchH) / 2;

    for (int i = 0; i < legendCount; ++i)
    {
        const auto& e = legend[(size_t) i];
        const int baseX = box.getX() + sp::legendPadX;

        drawSwatch (g, e.swatch, baseX + e.x, swatchY, alpha);
        look.drawText (g, e.name, { baseX + e.textX, box.getY(), e.textW, box.getHeight() }, tk::Type::bodySm,
                       tk::colour::textSecondary.withMultipliedAlpha (alpha), juce::Justification::centredLeft);
    }
}

void SpectrumView::paintHover (juce::Graphics& g)
{
    if (! hovering)
        return;

    auto& look = getLaf();
    const auto data = getDataRect();

    // The hint line is shown whenever the pointer is in the data rect.
    {
        const auto box = hintBounds();
        g.setColour (tk::colour::bgPlot.withAlpha (tk::alpha::legendFill));
        g.fillRoundedRectangle (box.toFloat(), tk::radius::small);
        look.drawText (g, hintText, box, tk::Type::hint, tk::colour::textTertiary, juce::Justification::centred);
    }

    if (! haveFrame)
        return;

    // Crosshair: 1 px at round(x) + 0.5 (ui-spec 5.5), no horizontal line.
    g.setColour (tk::colour::textPrimary.withAlpha (tk::alpha::crosshair));
    g.fillRect ((float) std::lround (hoverPos.x), (float) data.getY(), 1.0f, (float) data.getHeight());

    // Markers: the other curves first, primary A on top.
    const auto m = currentMapping();

    auto marker = [&] (const HoverRow& row)
    {
        if (! std::isfinite (row.db) || row.db <= sp::floorDb)
            return;

        const float cy = (float) m.dbToYClamped (row.db);
        const float r = sp::markerRadius;
        g.setColour (tk::colour::bgPlot);
        g.fillEllipse (hoverPos.x - r, cy - r, 2.0f * r, 2.0f * r);
        g.setColour (row.marker);
        g.drawEllipse (hoverPos.x - r, cy - r, 2.0f * r, 2.0f * r, tk::stroke::primary);
    };

    for (int i = 1; i < hoverRowCount; ++i)
        marker (hoverRows[(size_t) i]);

    if (hoverRowCount > 0)
        marker (hoverRows[0]);

    // Card.
    const auto card = cardBounds.toFloat();
    g.setColour (tk::colour::card);
    g.fillRoundedRectangle (card, tk::radius::control);
    g.setColour (tk::colour::border);
    g.drawRoundedRectangle (card.reduced ((float) sp::cardBorder * 0.5f), tk::radius::control, (float) sp::cardBorder);

    const int cx = cardBounds.getX() + sp::cardBorder + sp::cardPadX;
    const int cw = cardBounds.getWidth() - 2 * (sp::cardBorder + sp::cardPadX);
    int y = cardBounds.getY() + sp::cardBorder + sp::cardPadY;

    look.drawText (g, hoverFreqText, { cx, y, cw, sp::cardFreqLineH }, tk::Type::readoutHover, tk::colour::textPrimary);
    y += sp::cardFreqLineH;
    look.drawText (g, hoverNoteText, { cx, y, cw, sp::cardNoteLineH }, tk::Type::readout, tk::colour::textSecondary);
    y += sp::cardNoteLineH + sp::cardSepMargin;

    g.setColour (tk::colour::border);
    g.fillRect (cx, y, cw, 1);
    y += 1 + sp::cardSepMargin;

    for (int i = 0; i < hoverRowCount; ++i)
    {
        const auto& row = hoverRows[(size_t) i];
        drawSwatch (g, row.swatch, cx, y + (sp::cardRowH - sp::swatchH) / 2, 1.0f);

        const int nameX = cx + sp::swatchW + sp::cardSwatchGap;
        look.drawText (g, row.name, { nameX, y, cw - (nameX - cx), sp::cardRowH }, tk::Type::bodySm, tk::colour::textSecondary);
        look.drawText (g, row.value, { cx, y, cw, sp::cardRowH }, tk::Type::readout, tk::colour::textPrimary,
                       juce::Justification::centredRight);
        y += sp::cardRowH;
    }
}

void SpectrumView::paintBadge (juce::Graphics& g)
{
    if (! frozen)
        return;

    auto& look = getLaf();
    const auto d = getDataRect();
    const int w = (int) std::ceil (badgeTextW) + 2 * sp::badgePadX;
    const juce::Rectangle<int> box (d.getCentreX() - w / 2, d.getY() + sp::badgeY, w, sp::badgeH);

    g.setColour (tk::colour::stateFrozen);
    g.fillRoundedRectangle (box.toFloat(), tk::radius::small);
    look.drawText (g, frozenText, box, tk::Type::capsBadge, tk::colour::textOnAccent, juce::Justification::centred);
}

void SpectrumView::paintToasts (juce::Graphics& g)
{
    const auto now = juce::Time::getMillisecondCounter();
    auto& look = getLaf();

    for (const auto* toast : { &resetToast, &copyToast })
    {
        if (! toast->active)
            continue;

        const int elapsed = (int) (now - toast->startMs);
        const int remaining = toast->durationMs - elapsed;

        if (remaining <= 0)
            continue;

        // Linear fade over the last 200 ms.
        const float alpha = remaining >= tk::motion::toastFadeMs ? 1.0f
                                                                  : (float) remaining / (float) tk::motion::toastFadeMs;
        const auto box = toast->bounds.toFloat();

        g.setColour (tk::colour::surface3.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (box, tk::radius::small);
        g.setColour (tk::colour::accent.withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (box.reduced (0.5f), tk::radius::small, tk::stroke::hairline);
        look.drawText (g, toast->text, toast->bounds, tk::Type::bodySm, tk::colour::textPrimary.withMultipliedAlpha (alpha),
                       juce::Justification::centred);
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    const float scale = std::max (0.01f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
    rebuildCacheIfNeeded (scale);

    g.drawImageTransformed (cache, juce::AffineTransform::scale (1.0f / scale));

    const auto data = getDataRect();

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (data);

        if (haveFrame)
            paintCurves (g, scale);
    }

    // The data-rect frame: border-subtle, or state-frozen while frozen
    // (ui-spec 4.1, 5.7).
    g.setColour (frozen ? tk::colour::stateFrozen : tk::colour::borderSubtle);
    g.drawRect (data, 1);

    if (haveFrame)
        paintLegend (g);

    paintHover (g);
    paintBadge (g);
    paintToasts (g);
}

} // namespace investigate
