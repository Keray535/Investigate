#include "MeterPanel.h"

#include "InvestigateLookAndFeel.h"
#include "TooltipText.h"
#include "ValueFormat.h"

#include "../PluginProcessor.h"
#include "../params/ParameterFormatting.h"
#include "../params/ParameterIDs.h"

namespace investigate
{

namespace tk = tokens;
namespace M = tokens::meter;

namespace
{
    // Indices of MeterPanel::lists, in draw order.
    enum ListIndex { safeSolid, warmSolid, hotSolid, safePeak, warmPeak, hotPeak, separator, holdTick };

    constexpr const char* tableLabels[] { "Peak", "Peak max", "TP", "TP max", "Headroom", "RMS", "Crest", "Clips", "TP clips" };
    constexpr const char* tableUnits[] { "dBFS", "dBFS", "dBTP", "dBTP", "dB", "", "dB", "", "" };

    const char* scaleName (MeterScale s) noexcept
    {
        switch (s)
        {
            case MeterScale::k20: return "K-20";
            case MeterScale::k14: return "K-14";
            case MeterScale::k12: return "K-12";
            case MeterScale::dBFS: break;
        }

        return "dBFS";
    }

    int roundPx (double logical, float scale) noexcept { return (int) std::lround (logical * (double) scale); }
}

//==============================================================================
class MeterPanel::TooltipZone final : public juce::Component, public juce::TooltipClient
{
public:
    explicit TooltipZone (std::function<juce::String()> sourceToUse) : source (std::move (sourceToUse)) {}

    juce::String getTooltip() override { return source != nullptr ? source() : juce::String(); }

private:
    std::function<juce::String()> source;
};

//==============================================================================
MeterPanel::MeterPanel (InvestigateProcessor& processorToView, DisplayContext& context)
    : processor (processorToView),
      scale (processorToView.getApvts(), ParamIDs::meterScale, ParamControl::Style::inlineChip, "Scale", "Meter scale", &context),
      rms (processorToView.getApvts(), ParamIDs::rmsWindow, ParamControl::Style::inlineChip, "RMS", "RMS window", &context),
      resetButton (Icon::reset, "Reset", HeaderButton::Kind::action, "Reset statistics"),
      meterScaleValue (processorToView.getApvts().getRawParameterValue (ParamIDs::meterScale)),
      rmsWindowValue (processorToView.getApvts().getRawParameterValue (ParamIDs::rmsWindow))
{
    setOpaque (true);

    for (auto& list : lists)
        list.ensureStorageAllocated (2 * M::minBatch);

    resetButton.setTooltipSource ([] { return TooltipText::resetStatistics(); });
    resetButton.onClick = [this]
    {
        processor.requestStatsReset();
        resetHeldValues();

        if (onStatisticsReset != nullptr)
            onStatisticsReset();
    };

    correlationTip = std::make_unique<TooltipZone> ([this] { return TooltipText::correlation (monoShown); });

    addAndMakeVisible (scale);
    addAndMakeVisible (rms);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (*correlationTip);

    for (int ch = 0; ch < 2; ++ch)
        requested[(size_t) ch] = { -2, -2, -2, false };
}

MeterPanel::~MeterPanel() = default;

InvestigateLookAndFeel& MeterPanel::getLaf() const
{
    return InvestigateLookAndFeel::from (*this);
}

//==============================================================================
// Layout

void MeterPanel::resized()
{
    layout = LayoutMath::computeMeterStripLayout (getHeight());

    scale.setBounds (layout.scaleChip);
    rms.setBounds (layout.rmsChip);
    resetButton.setBounds (layout.resetButton);
    correlationTip->setBounds (layout.rightColumn.getX(), layout.correlationY, layout.rightColumn.getWidth(), tk::layout::correlationBlockH);

    layoutCells();

    for (auto& r : requested)
        r = { -2, -2, -2, false };

    requestedCorrX = -2;
    lastTextMs = -1.0e12;
}

void MeterPanel::visibilityChanged()
{
    lastUpdateMs = -1.0;
    lastTextMs = -1.0e12;

    for (auto& r : requested)
        r = { -2, -2, -2, false };

    requestedCorrX = -2;
}

void MeterPanel::layoutCells()
{
    const auto rc = layout.rightColumn;

    for (int row = 0; row < numTableRows; ++row)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& cell = cells[(size_t) tableCellIndex (row, ch)];
            cell.type = (int) tk::Type::readout;
            cell.rightAligned = true;
            cell.bounds = { rc.getX() + M::tableLabelW + M::tableValueW * ch, layout.tableY + tk::layout::tableHeaderH + row * layout.rowH,
                            M::tableValueW, layout.rowH };
            cell.repaintBounds = (row == rowClips || row == rowTpClips) ? cell.bounds.withTrimmedLeft (-M::lampCellRepaintLeft) : cell.bounds;
        }
    }

    const int colW = rc.getWidth() / M::loudColumns;

    for (int i = 0; i < numLoudnessCells; ++i)
    {
        auto& cell = cells[(size_t) loudnessCellIndex (i)];
        const int col = i % M::loudColumns;
        const bool top = i < M::loudColumns;
        const int x = rc.getX() + col * rc.getWidth() / M::loudColumns;
        const int w = col == M::loudColumns - 1 ? rc.getRight() - x : colW;

        cell.type = (int) (top ? tk::Type::readoutLg : tk::Type::value);
        cell.rightAligned = false;
        cell.hasUnit = i == loudLra;
        cell.bounds = { x, layout.loudnessY + (top ? M::loudRow1ValueY : M::loudRow2ValueY), w,
                        top ? M::loudRow1ValueH : M::loudRow2ValueH };
        cell.repaintBounds = cell.bounds;
    }

    auto& corr = cells[(size_t) correlationCellIndex];
    corr.type = (int) tk::Type::readout;
    corr.bounds = { rc.getX() + M::corrValueX, layout.correlationY, M::corrValueW, M::corrTitleH };
    corr.repaintBounds = corr.bounds;

    auto& bal = cells[(size_t) balanceCellIndex];
    bal.type = (int) tk::Type::readout;
    bal.bounds = { rc.getX() + M::corrValueX, layout.correlationY + M::corrBalanceY, M::corrValueW, M::corrBalanceH };
    bal.repaintBounds = bal.bounds;
}

MeterPanel::Geometry MeterPanel::computeGeometry (float physScale) const
{
    Geometry g;
    g.scale = physScale;

    const auto origin = layout.barsColumn.getPosition();
    const int height = layout.barsColumn.getHeight();

    g.bar.topPx = roundPx (origin.y + M::trackTop, physScale);
    g.bar.bottomPx = std::max (g.bar.topPx + 1, roundPx (origin.y + height - M::trackBottomPad, physScale));

    for (int ch = 0; ch < 2; ++ch)
    {
        const int x = origin.x + (monoShown ? M::barMonoX : (ch == 0 ? M::barLeftX : M::barRightX));
        g.xPx[ch] = roundPx (x, physScale);
    }

    g.widthPx = std::max (1, roundPx (origin.x + (monoShown ? M::barMonoX : M::barLeftX) + M::barW, physScale)
                                - roundPx (origin.x + (monoShown ? M::barMonoX : M::barLeftX), physScale));

    const auto rc = layout.rightColumn;
    const int barY = layout.correlationY + M::corrBarY;
    g.corrBarX0 = roundPx (rc.getX(), physScale);
    g.corrBarX1 = roundPx (rc.getX() + M::corrBarW, physScale);
    g.corrCentre = roundPx (rc.getX() + M::corrBarW / 2, physScale);
    g.corrY0 = roundPx (barY, physScale);
    g.corrY1 = roundPx (barY + M::corrBarH, physScale);
    return g;
}

juce::Rectangle<int> MeterPanel::barRepaintRect (int channel) const
{
    const auto origin = layout.barsColumn.getPosition();
    const int x = origin.x + (monoShown ? M::barMonoX : (channel == 0 ? M::barLeftX : M::barRightX));
    const int bottom = origin.y + layout.barsColumn.getHeight() - M::trackBottomPad;
    return { x - 1, origin.y - 1, M::barW + 2, bottom - origin.y + 3 };
}

juce::Rectangle<int> MeterPanel::correlationBarRect() const
{
    return { layout.rightColumn.getX(), layout.correlationY + M::corrBarY, M::corrBarW, M::corrBarH };
}

//==============================================================================
// Data

void MeterPanel::resetHeldValues()
{
    for (auto& c : channels)
    {
        c.peakHold.value = c.bar.value;
        c.truePeakHold.reset();
        c.crest.reset();
    }

    // The reset reaches the audio thread a block later, so the snapshot may
    // still carry the old maxima. Refresh text on every update for a short
    // window (accepted exception to the 10 Hz text rule, ui-spec 8.1/12).
    lastTextMs = -1.0e12;
    forceTextUntilMs = juce::Time::getMillisecondCounterHiRes() + M::resetForceTextMs;
}

MeterPanel::BarPixels MeterPanel::computeBarPixels (int channel, const Geometry& geom) const noexcept
{
    const auto& c = channels[(size_t) channel];
    const int src = monoShown ? 0 : channel;

    BarPixels p;
    p.rmsY = geom.bar.yFor (MeterMath::rmsBarDb (effective.rms[(size_t) src], scaleShown));
    p.peakY = geom.bar.yFor (c.bar.value);
    p.holdY = c.peakHold.value > MeterMath::barFloorDb ? geom.bar.yFor (c.peakHold.value) : -1;
    p.lampLit = effective.sampleClipCount[(size_t) src] + effective.truePeakClipCount[(size_t) src] > 0;
    return p;
}

int MeterPanel::computeCorrelationX (const Geometry& geom) const noexcept
{
    if (monoShown)
        return -1;

    const double v = std::clamp ((double) correlationFilter.value, -1.0, 1.0);
    return roundPx ((double) layout.rightColumn.getX() + (double) (M::corrBarW / 2) + v * (double) (M::corrBarW / 2), geom.scale);
}

void MeterPanel::update (const MeterSnapshot& snapshot, double nowMs, UpdateHints hints)
{
    if (! isVisible())
        return;

    applySnapshot (snapshot, nowMs);

    if (nowMs - lastTextMs >= M::textIntervalMs || nowMs < forceTextUntilMs)
    {
        lastTextMs = nowMs;
        refreshText (snapshot, nowMs);
    }

    // Which parts show something new, in whole physical pixels.
    const auto geom = computeGeometry (pixelScale);
    const int numBars = monoShown ? 1 : 2;
    bool any = false;

    for (int ch = 0; ch < 2; ++ch)
    {
        wanted[(size_t) ch] = ch < numBars ? computeBarPixels (ch, geom) : BarPixels();
        barDirty[ch] = ch < numBars && wanted[(size_t) ch] != requested[(size_t) ch];
        any = any || barDirty[ch];
    }

    wantedCorrX = computeCorrelationX (geom);
    corrDirty = ! monoShown && wantedCorrX != requestedCorrX;
    any = any || corrDirty;

    for (const auto& cell : cells)
        any = any || cell.dirty;

    if (! any)
        return;

    // T-029 rule 5: while the spectrum is frame-paced, wait for the vblank on
    // which it repaints, so the window is presented once for both.
    const bool open = ! hints.spectrumPaced || hints.spectrumRepainted || nowMs - lastFlushMs >= M::maxPacedHoldMs;

    if (open)
    {
        lastFlushMs = nowMs;
        flushRepaints();
    }
}

void MeterPanel::applySnapshot (const MeterSnapshot& snapshot, double nowMs)
{
    const double dt = lastUpdateMs < 0.0 ? 0.0 : std::clamp ((nowMs - lastUpdateMs) / 1000.0, 0.0, 0.1);
    lastUpdateMs = nowMs;

    const bool isStale = stale.update (snapshot.blockCounter, nowMs);

    effective = snapshot;

    auto clean = [] (auto& values)
    {
        for (auto& v : values)
            v = MeterMath::sanitizeLevel (v);
    };

    clean (effective.samplePeak);
    clean (effective.peakMax);
    clean (effective.rms);
    clean (effective.truePeak);
    clean (effective.truePeakMax);
    effective.momentary = MeterMath::sanitizeLevel (effective.momentary);
    effective.shortTerm = MeterMath::sanitizeLevel (effective.shortTerm);
    effective.integrated = MeterMath::sanitizeLevel (effective.integrated);
    effective.maxMomentary = MeterMath::sanitizeLevel (effective.maxMomentary);
    effective.maxShortTerm = MeterMath::sanitizeLevel (effective.maxShortTerm);
    effective.loudnessRange = std::isfinite (effective.loudnessRange) ? std::max (0.0f, effective.loudnessRange) : 0.0f;
    effective.correlation = std::isfinite (effective.correlation) ? std::clamp (effective.correlation, -1.0f, 1.0f) : 0.0f;
    effective.balance = std::isfinite (effective.balance) ? effective.balance : 0.0f;

    // ui-spec 8.7: while the host is not calling processBlock the live values
    // read as silence and fall at their normal ballistics; maxima, counts, I,
    // LRA, correlation and balance hold.
    if (isStale)
    {
        constexpr float silence = Decibels::negativeInfinityDb();
        effective.samplePeak.fill (silence);
        effective.truePeak.fill (silence);
        effective.rms.fill (silence);
        effective.momentary = silence;
        effective.shortTerm = silence;
    }

    const auto scaleNow = MeterMath::scaleFromIndex ((int) meterScaleValue->load());
    const bool monoNow = snapshot.isMono;
    rmsWindowMs = (double) rmsWindowValue->load();

    if (scaleNow != scaleShown || monoNow != monoShown || ! haveSnapshot)
    {
        scaleShown = scaleNow;
        monoShown = monoNow;
        haveSnapshot = true;
        lastTextMs = -1.0e12;

        for (auto& r : requested)
            r = { -2, -2, -2, false };

        requestedCorrX = -2;
        repaint();
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& c = channels[(size_t) ch];
        const auto src = (size_t) (monoShown ? 0 : ch);

        c.bar.update (effective.samplePeak[src], dt);
        c.peakHold.update (effective.samplePeak[src], nowMs, dt, c.bar.value);
        c.truePeakHold.update (effective.truePeak[src], nowMs, dt, effective.truePeak[src]);
        c.crest.push (nowMs, effective.samplePeak[src], rmsWindowMs);
    }

    correlationFilter.update (effective.correlation, dt);
}

void MeterPanel::setCell (int index, const MeterMath::FixedText& text, bool lit, bool error, bool hidden, bool dim)
{
    auto& cell = cells[(size_t) index];

    if (text == cell.text && lit == cell.lit && error == cell.error && hidden == cell.hidden && dim == cell.dim)
        return;

    cell.text = text;

    cell.lit = lit;
    cell.error = error;
    cell.hidden = hidden;
    cell.dim = dim;
    cell.dirty = true;
}

void MeterPanel::refreshText (const MeterSnapshot&, double nowMs)
{
    const MeterMath::FixedText none;

    for (int ch = 0; ch < 2; ++ch)
    {
        if (monoShown && ch == 0)
        {
            for (int row = 0; row < numTableRows; ++row)
                setCell (tableCellIndex (row, 0), none, false, false, true);

            continue;
        }

        const auto src = (size_t) (monoShown ? 0 : ch);
        const auto& c = channels[src];

        setCell (tableCellIndex (rowPeak, ch), MeterMath::formatLevel (c.peakHold.value));
        setCell (tableCellIndex (rowPeakMax, ch), MeterMath::formatLevel (effective.peakMax[src]));
        setCell (tableCellIndex (rowTruePeak, ch), MeterMath::formatLevel (c.truePeakHold.value));
        setCell (tableCellIndex (rowTruePeakMax, ch), MeterMath::formatLevel (effective.truePeakMax[src]));

        bool negative = false;
        const auto headroom = MeterMath::formatHeadroom (effective.truePeakMax[src], negative);
        setCell (tableCellIndex (rowHeadroom, ch), headroom, false, negative);

        setCell (tableCellIndex (rowRms, ch), MeterMath::formatRms (effective.rms[src], scaleShown));

        const float windowPeak = std::max (c.crest.maxWithin (nowMs, rmsWindowMs), effective.samplePeak[src]);
        setCell (tableCellIndex (rowCrest, ch), MeterMath::formatCrest (windowPeak, effective.rms[src]));

        setCell (tableCellIndex (rowClips, ch), MeterMath::formatCount (effective.sampleClipCount[src]),
                 effective.sampleClipCount[src] > 0);
        setCell (tableCellIndex (rowTpClips, ch), MeterMath::formatCount (effective.truePeakClipCount[src]),
                 effective.truePeakClipCount[src] > 0);
    }

    setCell (loudnessCellIndex (loudM), MeterMath::formatLoudnessLive (effective.momentary));
    setCell (loudnessCellIndex (loudS), MeterMath::formatLoudnessLive (effective.shortTerm));
    setCell (loudnessCellIndex (loudI), MeterMath::formatLoudnessHeld (effective.integrated));
    setCell (loudnessCellIndex (loudMMax), MeterMath::formatLoudnessMax (effective.maxMomentary));
    setCell (loudnessCellIndex (loudSMax), MeterMath::formatLoudnessMax (effective.maxShortTerm));
    setCell (loudnessCellIndex (loudLra), MeterMath::formatLra (effective.loudnessRange, effective.integrated));

    if (monoShown)
    {
        const auto dash = MeterMath::FixedText().append (MeterMath::dashText);
        setCell (correlationCellIndex, dash, false, false, false, true);
        setCell (balanceCellIndex, dash, false, false, false, true);
    }
    else
    {
        setCell (correlationCellIndex, MeterMath::formatCorrelation (correlationFilter.value));
        setCell (balanceCellIndex, MeterMath::formatBalance (effective.balance));
    }
}

void MeterPanel::flushRepaints()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        if (barDirty[ch])
        {
            repaint (barRepaintRect (ch));
            requested[(size_t) ch] = wanted[(size_t) ch];
            barDirty[ch] = false;
            ++barRepaints;
        }
    }

    if (corrDirty)
    {
        repaint (correlationBarRect());
        requestedCorrX = wantedCorrX;
        corrDirty = false;
        ++barRepaints;
    }

    for (auto& cell : cells)
    {
        if (cell.dirty)
        {
            repaint (cell.repaintBounds);
            cell.dirty = false;
            ++textRepaints;
        }
    }
}

juce::String MeterPanel::getCellText (int cellIndex) const
{
    return cells[(size_t) cellIndex].text.toString();
}

bool MeterPanel::isCellLit (int cellIndex) const noexcept
{
    return cells[(size_t) cellIndex].lit;
}

//==============================================================================
// Static layer

void MeterPanel::rebuildCacheIfNeeded (float physScale)
{
    const CacheKey key { getWidth(), getHeight(), physScale, scaleShown, monoShown };

    if (key == cacheKey && cache.isValid())
        return;

    renderCache (physScale);
    cacheKey = key;
}

void MeterPanel::renderCache (float physScale)
{
    const int pw = std::max (1, (int) std::ceil ((double) getWidth() * physScale));
    const int ph = std::max (1, (int) std::ceil ((double) getHeight() * physScale));

    if (! cache.isValid() || cache.getWidth() != pw || cache.getHeight() != ph)
        cache = juce::Image (juce::Image::RGB, pw, ph, false);

    juce::Graphics g (cache);
    g.addTransform (juce::AffineTransform::scale (physScale));
    g.fillAll (tk::colour::surface);

    // 1 px border on the left edge.
    g.setColour (tk::colour::border);
    g.fillRect (0, 0, 1, getHeight());

    auto& look = getLaf();
    const float inv = 1.0f / physScale;
    const auto geom = computeGeometry (physScale);
    const auto fillPx = [&] (int x0, int y0, int x1, int y1, juce::Colour colour)
    {
        g.setColour (colour);
        g.fillRect (juce::Rectangle<float> ((float) x0 * inv, (float) y0 * inv, (float) (x1 - x0) * inv, (float) (y1 - y0) * inv));
    };

    //--------------------------------------------------------------------------
    // Bars column
    const auto origin = layout.barsColumn.getPosition();
    const int colHeight = layout.barsColumn.getHeight();
    const int numBars = monoShown ? 1 : 2;
    const int lineT = MeterMath::thicknessPx (tk::stroke::hairline, physScale);

    g.setColour (tk::colour::bgPlot);

    for (int ch = 0; ch < numBars; ++ch)
    {
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) geom.xPx[ch] * inv, (float) geom.bar.topPx * inv,
                                                        (float) geom.widthPx * inv, (float) geom.bar.heightPx() * inv),
                                tk::radius::bar);
    }

    const double trackLogical = (double) geom.bar.heightPx() / (double) physScale;
    const auto ticks = MeterMath::computeTicks (scaleShown, trackLogical);

    for (int i = 0; i < ticks.count; ++i)
    {
        const auto tick = ticks.ticks[(size_t) i];
        const int y = std::clamp (geom.bar.yFor (tick.dbfs), geom.bar.topPx, geom.bar.bottomPx - lineT);

        for (int ch = 0; ch < numBars; ++ch)
            fillPx (geom.xPx[ch], y, geom.xPx[ch] + geom.widthPx, y + lineT, tk::colour::meterTrackLine);

        const float yLogical = ((float) y + (float) lineT * 0.5f) * inv;

        if (yLogical < (float) (origin.y + M::trackTop - M::tickEdgeSlack)
            || yLogical > (float) (origin.y + colHeight - M::trackBottomPad + M::tickEdgeSlack))
            continue;

        look.drawText (g, ValueFormat::formatSignedDb ((float) tick.label),
                       { origin.x, (int) std::lround (yLogical) - M::tickLabelH / 2, M::tickLabelRight, M::tickLabelH },
                       tk::Type::axisMeter, tk::colour::textTertiary, juce::Justification::centredRight);
    }

    for (int ch = 0; ch < numBars; ++ch)
    {
        const int x = origin.x + (monoShown ? M::barMonoX : (ch == 0 ? M::barLeftX : M::barRightX));
        g.setColour (tk::colour::meterLampOff);
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) x, (float) origin.y, (float) M::barW, (float) M::lampH), M::lampRadius);

        const int labelW = monoShown ? M::monoLabelW : M::barW + 2 * M::letterPad;
        look.drawText (g, monoShown ? "MONO" : (ch == 0 ? "L" : "R"),
                       { x + M::barW / 2 - labelW / 2, origin.y + colHeight - M::letterBottom, labelW, M::letterH },
                       tk::Type::caps, tk::colour::textSecondary, juce::Justification::centred);
    }

    //--------------------------------------------------------------------------
    // Statistics table
    const auto rc = layout.rightColumn;
    const int headerY = layout.tableY;

    if (! monoShown)
        look.drawText (g, "L", { rc.getX() + M::tableLabelW, headerY, M::tableValueW, tk::layout::tableHeaderH }, tk::Type::caps,
                       tk::colour::textSecondary, juce::Justification::centredRight);

    look.drawText (g, monoShown ? "Mono" : "R", { rc.getX() + M::tableLabelW + M::tableValueW, headerY, M::tableValueW, tk::layout::tableHeaderH },
                   tk::Type::caps, tk::colour::textSecondary, juce::Justification::centredRight);

    for (int row = 0; row < numTableRows; ++row)
    {
        const juce::String label (tableLabels[row]);
        const juce::String unit (row == rowRms ? scaleName (scaleShown) : tableUnits[row]);
        const int y = layout.tableY + tk::layout::tableHeaderH + row * layout.rowH;

        look.drawText (g, label, { rc.getX(), y, M::tableLabelW, layout.rowH }, tk::Type::bodySm, tk::colour::textSecondary);

        if (unit.isNotEmpty())
        {
            const int ux = rc.getX() + (int) std::ceil (look.getTextWidth (label, tk::Type::bodySm)) + M::unitGap;
            look.drawText (g, unit, { ux, y, M::tableLabelW, layout.rowH }, tk::Type::unit, tk::colour::textTertiary);
        }
    }

    // Dividers, centred in the two gaps.
    g.setColour (tk::colour::border);
    g.fillRect (rc.getX(), layout.tableY + layout.tableH + layout.gap / 2, rc.getWidth(), 1);
    g.fillRect (rc.getX(), layout.loudnessY + tk::layout::loudnessBlockH + layout.gap / 2, rc.getWidth(), 1);

    //--------------------------------------------------------------------------
    // Loudness block
    look.drawText (g, "LOUDNESS", { rc.getX(), layout.loudnessY, rc.getWidth() / 2, M::loudTitleH }, tk::Type::caps, tk::colour::textTertiary);
    look.drawText (g, "LUFS", { rc.getX() + rc.getWidth() / 2, layout.loudnessY, rc.getWidth() / 2, M::loudTitleH }, tk::Type::unit,
                   tk::colour::textTertiary, juce::Justification::centredRight);

    static constexpr const char* loudLabels[] { "M", "S", "I", "M max", "S max", "LRA" };

    for (int i = 0; i < numLoudnessCells; ++i)
    {
        const auto& cell = cells[(size_t) loudnessCellIndex (i)];
        const int labelY = layout.loudnessY + (i < M::loudColumns ? M::loudRow1LabelY : M::loudRow2LabelY);
        look.drawText (g, loudLabels[i], { cell.bounds.getX(), labelY, cell.bounds.getWidth(), M::loudLabelH }, tk::Type::caps,
                       tk::colour::textSecondary);
    }

    //--------------------------------------------------------------------------
    // Correlation and balance block (40 % opacity for a mono input, 8.6)
    const float dim = monoShown ? tk::alpha::monoCorrelation : 1.0f;
    const auto faded = [dim] (juce::Colour c) { return c.withMultipliedAlpha (dim); };

    look.drawText (g, "CORRELATION", { rc.getX(), layout.correlationY, rc.getWidth() / 2, M::corrTitleH }, tk::Type::caps,
                   faded (tk::colour::textTertiary));

    g.setColour (faded (tk::colour::bgPlot));
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) geom.corrBarX0 * inv, (float) geom.corrY0 * inv,
                                                    (float) (geom.corrBarX1 - geom.corrBarX0) * inv, (float) (geom.corrY1 - geom.corrY0) * inv),
                            tk::radius::bar);

    for (const int dx : { M::corrTickOffset, M::corrBarW - M::corrTickOffset })
    {
        const int x = roundPx (rc.getX() + dx, physScale);
        fillPx (x, geom.corrY0, x + lineT, geom.corrY1, faded (tk::colour::meterTrackLine));
    }

    fillPx (geom.corrCentre, geom.corrY0, geom.corrCentre + lineT, geom.corrY1, faded (tk::colour::border));

    const int scaleY = layout.correlationY + M::corrScaleY;
    const int third = rc.getWidth() / 3;
    look.drawText (g, ParameterFormatting::minusSign + "1", { rc.getX(), scaleY, third, M::corrScaleH }, tk::Type::axisMeter,
                   faded (tk::colour::textTertiary));
    look.drawText (g, "0", { rc.getX() + third, scaleY, rc.getWidth() - 2 * third, M::corrScaleH }, tk::Type::axisMeter,
                   faded (tk::colour::textTertiary), juce::Justification::centred);
    look.drawText (g, "+1", { rc.getRight() - third, scaleY, third, M::corrScaleH }, tk::Type::axisMeter,
                   faded (tk::colour::textTertiary), juce::Justification::centredRight);

    look.drawText (g, "BALANCE", { rc.getX(), layout.correlationY + M::corrBalanceY, rc.getWidth() / 2, M::corrBalanceH }, tk::Type::caps,
                   faded (tk::colour::textSecondary));
}

//==============================================================================
// Dynamic layer

void MeterPanel::addPixelRect (int listIndex, const Geometry& geom, int x0, int y0, int x1, int y1)
{
    if (x1 <= x0 || y1 <= y0)
        return;

    const float inv = 1.0f / geom.scale;
    lists[(size_t) listIndex].addWithoutMerging ({ (float) x0 * inv, (float) y0 * inv,
                                                   ((float) (x1 - x0) + M::spriteSlackPx) * inv,
                                                   ((float) (y1 - y0) + M::spriteSlackPx) * inv });
}

void MeterPanel::flushList (juce::Graphics& g, int listIndex, juce::Colour colour)
{
    auto& list = lists[(size_t) listIndex];
    const int count = list.getNumRectangles();

    if (count == 0)
        return;

    // Pad with rectangles far outside the clip so the sprite-batch size is the
    // same every frame (JUCE caches only a few batch sizes).
    const int target = (count + M::minBatch - 1) / M::minBatch * M::minBatch;

    for (int i = count; i < target; ++i)
        list.addWithoutMerging ({ -1000.0f, -1000.0f, 2.0f, 2.0f });

    g.setColour (colour);
    g.fillRectList (list);
    list.clear();
}

void MeterPanel::paintBars (juce::Graphics& g, const Geometry& geom, juce::Rectangle<int> clip)
{
    const int numBars = monoShown ? 1 : 2;
    const auto zones = MeterMath::zonesFor (scaleShown);
    const int yWarm = geom.bar.yFor (zones.warmFrom);
    const int yHot = geom.bar.yFor (zones.hotFrom);
    const int top = geom.bar.topPx, bottom = geom.bar.bottomPx;
    const int sepT = MeterMath::thicknessPx (tk::stroke::hairline, geom.scale);
    const int holdT = MeterMath::thicknessPx (tk::stroke::holdTick, geom.scale);
    bool anyLit = false;

    for (int ch = 0; ch < numBars; ++ch)
    {
        if (! barRepaintRect (ch).intersects (clip))
            continue;

        const auto st = computeBarPixels (ch, geom);
        const int x0 = geom.xPx[ch], x1 = x0 + geom.widthPx;

        // Stacked by zone, like a segmented hardware meter.
        const auto stack = [&] (int yTop, int yBottom, int firstList)
        {
            addPixelRect (firstList + 0, geom, x0, std::max (yTop, yWarm), x1, yBottom);
            addPixelRect (firstList + 1, geom, x0, std::max (yTop, yHot), x1, std::min (yBottom, yWarm));
            addPixelRect (firstList + 2, geom, x0, yTop, x1, std::min (yBottom, yHot));
        };

        const bool hasRms = st.rmsY < bottom;

        if (hasRms)
            stack (st.rmsY, bottom, safeSolid);

        if (st.peakY < st.rmsY)
        {
            stack (st.peakY, st.rmsY, safePeak);

            if (hasRms)
                addPixelRect (separator, geom, x0, st.rmsY, x1, std::min (bottom, st.rmsY + sepT));
        }

        if (st.holdY >= 0)
        {
            const int y0 = std::clamp (st.holdY - holdT / 2, top, bottom - holdT);
            addPixelRect (holdTick, geom, x0, y0, x1, y0 + holdT);
        }

        if (st.lampLit)
            anyLit = true;
    }

    flushList (g, safeSolid, tk::colour::meterSafe);
    flushList (g, warmSolid, tk::colour::meterWarm);
    flushList (g, hotSolid, tk::colour::meterHot);
    flushList (g, safePeak, tk::colour::meterSafePeak);
    flushList (g, warmPeak, tk::colour::meterWarmPeak);
    flushList (g, hotPeak, tk::colour::meterHotPeak);
    flushList (g, separator, tk::colour::bgPlot);
    flushList (g, holdTick, tk::colour::meterHoldTick);

    if (! anyLit)
        return;

    const auto origin = layout.barsColumn.getPosition();
    g.setColour (tk::colour::meterClip);

    for (int ch = 0; ch < numBars; ++ch)
    {
        if (! barRepaintRect (ch).intersects (clip) || ! computeBarPixels (ch, geom).lampLit)
            continue;

        const int x = origin.x + (monoShown ? M::barMonoX : (ch == 0 ? M::barLeftX : M::barRightX));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) x, (float) origin.y, (float) M::barW, (float) M::lampH), M::lampRadius);
    }
}

void MeterPanel::paintCorrelation (juce::Graphics& g, const Geometry& geom, juce::Rectangle<int> clip)
{
    if (monoShown || ! correlationBarRect().intersects (clip))
        return;

    const float inv = 1.0f / geom.scale;
    const int x = computeCorrelationX (geom);
    const int lineT = MeterMath::thicknessPx (tk::stroke::hairline, geom.scale);
    const int inset = MeterMath::thicknessPx ((float) M::corrFillInset, geom.scale);
    const int y0 = geom.corrY0 + inset, y1 = geom.corrY1 - inset;

    const auto fillPx = [&] (int ax0, int ay0, int ax1, int ay1)
    {
        if (ax1 > ax0 && ay1 > ay0)
            g.fillRect (juce::Rectangle<float> ((float) ax0 * inv, (float) ay0 * inv,
                                                ((float) (ax1 - ax0) + M::spriteSlackPx) * inv,
                                                ((float) (ay1 - ay0) + M::spriteSlackPx) * inv));
    };

    const bool positive = correlationFilter.value >= 0.0f;
    g.setColour (positive ? tk::colour::corrPositive : tk::colour::corrNegative);

    if (positive)
        fillPx (geom.corrCentre + lineT, y0, x, y1);
    else
        fillPx (x, y0, geom.corrCentre, y1);

    const int indicatorW = MeterMath::thicknessPx ((float) M::corrIndicatorW, geom.scale);
    const int ix = std::clamp (x - indicatorW / 2, geom.corrBarX0, geom.corrBarX1 - indicatorW);
    g.setColour (tk::colour::textPrimary);
    fillPx (ix, geom.corrY0, ix + indicatorW, geom.corrY1);
}

void MeterPanel::paintCells (juce::Graphics& g, juce::Rectangle<int> clip)
{
    auto& look = getLaf();

    for (const auto& cell : cells)
    {
        if (cell.hidden || cell.text.empty() || ! cell.repaintBounds.intersects (clip))
            continue;

        const auto type = (tk::Type) cell.type;
        const auto string = cell.text.toString();   // built at paint time only, never in the vblank path
        auto colour = cell.error ? tk::colour::stateError : tk::colour::textPrimary;

        if (cell.dim)
            colour = colour.withMultipliedAlpha (tk::alpha::monoCorrelation);

        if (cell.lit)
        {
            // A clip count above zero becomes a lamp (ui-spec 8.3).
            const int w = (int) std::ceil (look.getTextWidth (string, type)) + 2 * M::lampCellPadX;
            const juce::Rectangle<int> lamp (cell.bounds.getRight() - w, cell.bounds.getCentreY() - M::lampCellH / 2, w, M::lampCellH);
            g.setColour (tk::colour::meterClip);
            g.fillRoundedRectangle (lamp.toFloat(), tk::radius::small);
            look.drawText (g, string, lamp, type, tk::colour::textOnAccent, juce::Justification::centred);
            continue;
        }

        look.drawText (g, string, cell.bounds, type, colour,
                       cell.rightAligned ? juce::Justification::centredRight : juce::Justification::centredLeft);

        if (cell.hasUnit && cell.text.view() != MeterMath::dashText)
        {
            const int ux = cell.bounds.getX() + (int) std::ceil (look.getTextWidth (string, type)) + M::loudUnitGap;
            look.drawText (g, "LU", { ux, cell.bounds.getY(), cell.bounds.getWidth(), cell.bounds.getHeight() }, tk::Type::unit,
                           tk::colour::textSecondary);
        }
    }
}

void MeterPanel::paint (juce::Graphics& g)
{
    const float physScale = std::max (0.01f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());

    if (physScale != pixelScale)
    {
        pixelScale = physScale;

        for (auto& r : requested)
            r = { -2, -2, -2, false };

        requestedCorrX = -2;
    }

    rebuildCacheIfNeeded (physScale);
    g.drawImageTransformed (cache, juce::AffineTransform::scale (1.0f / physScale));

    const auto clip = g.getClipBounds();
    const auto geom = computeGeometry (physScale);

    paintBars (g, geom, clip);
    paintCorrelation (g, geom, clip);
    paintCells (g, clip);
}

} // namespace investigate
