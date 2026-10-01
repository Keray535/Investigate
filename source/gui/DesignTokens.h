#pragma once

#include <juce_graphics/juce_graphics.h>

/** Every colour, type style, size, spacing, radius, stroke and duration used
    by the editor (docs/design/tokens.json and docs/design/ui-spec.md section
    3). Components must take their values from here; nothing in source/gui may
    hard-code a colour number, font size or spacing outside this header
    (T-011 acceptance criterion, checked by grep). All sizes are logical
    pixels at 100 % scale. */
namespace investigate::tokens
{

//==============================================================================
namespace colour
{
    inline const juce::Colour bgApp         { 0xff0E1219 };
    inline const juce::Colour bgPlot        { 0xff0A0D13 };
    inline const juce::Colour surface       { 0xff131924 };
    inline const juce::Colour surface2      { 0xff1A2130 };
    inline const juce::Colour surface3      { 0xff232C3E };
    inline const juce::Colour card          { 0xff151B27 };
    inline const juce::Colour border        { 0xff2A3447 };
    inline const juce::Colour borderSubtle  { 0xff1A2130 };
    inline const juce::Colour gridMinor     { 0xff161D2A };
    inline const juce::Colour gridMajor     { 0xff243047 };
    inline const juce::Colour textPrimary   { 0xffE8ECF3 };
    inline const juce::Colour textSecondary { 0xffA3AEC2 };
    inline const juce::Colour textTertiary  { 0xff75829A };
    inline const juce::Colour textDisabled  { 0xff4A5468 };
    inline const juce::Colour textOnAccent  { 0xff0A0D13 };
    inline const juce::Colour accent        { 0xff62D6F0 };
    inline const juce::Colour accentTint    { 0xff1B313B };
    inline const juce::Colour stateFrozen   { 0xff8FB8FF };
    inline const juce::Colour stateFrozenTint { 0xff283347 };
    inline const juce::Colour stateWarn     { 0xffEAC75A };
    inline const juce::Colour stateError    { 0xffFF4F64 };
    inline const juce::Colour black         { 0xff000000 };

    // Curves (ui-spec 5.1). Alphas are applied by the drawing code.
    inline const juce::Colour primaryA      { 0xff62D6F0 };
    inline const juce::Colour primaryB      { 0xffFFB454 };
    inline const juce::Colour secondaryA    { 0xff3E8FA3 };
    inline const juce::Colour secondaryB    { 0xffA87A3C };
    inline const juce::Colour sidechain     { 0xffA58BFF };

    // Meters (ui-spec 3.1, 8.2)
    inline const juce::Colour meterTrack     { 0xff0A0D13 };
    inline const juce::Colour meterTrackLine { 0xff1A2231 };
    inline const juce::Colour meterSafe      { 0xff34C6A8 };
    inline const juce::Colour meterWarm      { 0xffEAC75A };
    inline const juce::Colour meterHot       { 0xffFF8A3D };
    inline const juce::Colour meterClip      { 0xffFF4F64 };
    inline const juce::Colour meterSafePeak  { 0xff217365 };
    inline const juce::Colour meterWarmPeak  { 0xff857339 };
    inline const juce::Colour meterHotPeak   { 0xff91522A };
    inline const juce::Colour meterHoldTick  { 0xffE8ECF3 };
    inline const juce::Colour meterLampOff   { 0xff1A2130 };
    inline const juce::Colour corrPositive   { 0xff34C6A8 };
    inline const juce::Colour corrNegative   { 0xffFF8A3D };
}

//==============================================================================
/** Alpha values taken from the spec. */
namespace alpha
{
    constexpr float crosshair         = 0.35f;
    constexpr float sidechainStroke   = 0.70f;
    constexpr float sidechainFill     = 0.26f;
    constexpr float primaryFillTop    = 0.30f;
    constexpr float primaryFillBottom = 0.04f;
    constexpr float legendFill        = 0.85f;
    constexpr float frozenBorder      = 0.60f;
    constexpr float toggleOnBorder    = 0.50f;
    constexpr float popoverShadow     = 0.50f;
    constexpr float monoCorrelation   = 0.40f;
    constexpr float selection         = 0.30f;
}

//==============================================================================
/** The four bundled typefaces; weights per ui-spec 3.2. */
enum class Face { interMedium, interSemiBold, monoRegular, monoMedium };

struct TypeSpec
{
    Face face;
    float size;      // px (em size), converted to a JUCE point height by the LookAndFeel
    float tracking;  // extra letter spacing in em
    bool uppercase;
};

enum class Type { caps, capsBadge, bodySm, bodySmSemi, body, bodySemi, choice, value, unit,
                  readout, readoutLg, readoutHover, axis, axisMeter, hint };

constexpr TypeSpec typeSpec (Type t) noexcept
{
    switch (t)
    {
        case Type::caps:         return { Face::interSemiBold, 10.0f, 0.06f, true };
        case Type::capsBadge:    return { Face::interSemiBold, 10.0f, 0.08f, true };
        case Type::bodySm:       return { Face::interMedium,   11.0f, 0.0f,  false };
        case Type::bodySmSemi:   return { Face::interSemiBold, 11.0f, 0.0f,  false };
        case Type::body:         return { Face::interMedium,   12.0f, 0.0f,  false };
        case Type::bodySemi:     return { Face::interSemiBold, 12.0f, 0.0f,  false };
        case Type::choice:       return { Face::interMedium,   13.0f, 0.0f,  false };
        case Type::value:        return { Face::monoMedium,    13.0f, 0.0f,  false };
        case Type::unit:         return { Face::interMedium,   10.0f, 0.0f,  false };
        case Type::readout:      return { Face::monoRegular,   12.0f, 0.0f,  false };
        case Type::readoutLg:    return { Face::monoMedium,    18.0f, 0.0f,  false };
        case Type::readoutHover: return { Face::monoMedium,    15.0f, 0.0f,  false };
        case Type::axis:         return { Face::monoRegular,   11.0f, 0.0f,  false };
        case Type::axisMeter:    return { Face::monoRegular,   10.0f, 0.0f,  false };
        case Type::hint:         return { Face::interMedium,   10.0f, 0.0f,  false };
    }

    return { Face::interMedium, 12.0f, 0.0f, false };
}

//==============================================================================
namespace space
{
    constexpr int s2 = 2, s4 = 4, s6 = 6, s8 = 8, s12 = 12, s16 = 16, s20 = 20, s24 = 24, s32 = 32;
}

namespace radius
{
    constexpr float bar     = 2.0f;
    constexpr float small   = 4.0f;
    constexpr float control = 6.0f;
    constexpr float popover = 8.0f;
}

namespace stroke
{
    constexpr float hairline = 1.0f;
    constexpr float primary  = 1.5f;
    constexpr float focus    = 2.0f;
    constexpr float holdTick = 2.0f;
}

namespace shadow
{
    constexpr int offsetY = 8;
    constexpr int blur    = 24;
}

namespace motion
{
    constexpr int clickDeferMs        = 250;
    constexpr int tooltipDelayMs      = 700;
    constexpr int toastCopyMs         = 1200;
    constexpr int toastResetMs        = 900;
    constexpr int toastFadeMs         = 200;
    constexpr int externalChangeDotMs = 600;
    constexpr int errorFlashMs        = 300;
}

//==============================================================================
namespace layout
{
    constexpr int defaultWidth = 1200, defaultHeight = 700;
    constexpr int minWidth     = 920,  minHeight     = 540;
    constexpr int maxWidth     = 2560, maxHeight     = 1600;

    constexpr int headerH       = 44;
    constexpr int controlBarH   = 56;
    constexpr int meterStripW   = 296;
    constexpr int regionPadding = 12;

    constexpr int plotGutterLeft = 48, plotGutterRight = 16, plotGutterTop = 12, plotGutterBottom = 24;

    constexpr int chipH        = 40;
    constexpr int inlineChipH  = 28;
    constexpr int buttonH      = 28;
    constexpr int resizeGrip   = 12;
    constexpr int resizeGripDot = 1;
    constexpr int chipPadX     = 8;
    constexpr int chipPadTop   = 5;
    constexpr int chipGap      = 8;
    constexpr int groupGap     = 20;
    constexpr int dividerH     = 24;
    constexpr int unitGap      = 3;

    // Control-bar chip widths (ui-spec 4.3)
    constexpr int chipWBlock = 60, chipWOverlap = 68, chipWWindow = 76, chipWAvg = 72, chipWSmooth = 68;
    constexpr int chipWSlope = 92, chipWSecondary = 96;
    constexpr int chipWFreqRange = 140, chipWDbRange = 120;
    constexpr int rangeSepW = 14;
    constexpr int rangeHalfFreqLow = 54, rangeHalfDbLow = 40;
    constexpr int groupAW = 376, groupBW = 196, groupCW = 268;

    // Header (ui-spec 4.2); x of right-anchored items is W minus these.
    constexpr int headerItemY = 8, headerItemH = 28;
    constexpr int wordmarkX = 16, wordmarkY = 13, wordmarkW = 81, wordmarkH = 18;
    constexpr int modeSingleX = 116, modeSingleW = 200, modeDualX = 324, modeDualW = 92;
    constexpr int modeTrackPad = 2, modeSegH = 24;
    constexpr int modeSegWMid = 44, modeSegWSide = 44, modeSegWL = 32, modeSegWR = 32, modeSegWLR = 44;
    constexpr int modeSegWDual = 44;
    constexpr int sampleRateFromRight = 444, sampleRateW = 60;
    constexpr int scFromRight = 368, scW = 60;
    constexpr int freezeFromRight = 302, freezeW = 84;
    constexpr int resetCurvesFromRight = 212, resetCurvesW = 116;
    constexpr int headerDividerFromRight = 84, headerDividerY = 14, headerDividerH = 16;
    constexpr int metersBtnFromRight = 72, settingsBtnFromRight = 40, iconBtnW = 28;
    constexpr int iconSize = 16;
    constexpr int settingsDotSize = 6, settingsDotInset = 3;
    constexpr int automationDotSize = 4, automationDotInset = 4;
    constexpr int buttonIconGap = 6, buttonPadX = 10;

    // Meter strip (ui-spec 4.4), strip-relative.
    constexpr int stripChipY = 12, stripChipH = 28;
    constexpr int scaleChipX = 12, rmsChipX = 114, stripChipW = 96;
    constexpr int resetStatsX = 216, resetStatsW = 68;
    constexpr int stripBodyY = 52, stripBodyBottomPad = 12, stripBodyColumnPad = 64;
    constexpr int barsColX = 12, barsColW = 80;
    constexpr int rightColX = 108, rightColW = 176;
    constexpr int tableHeaderH = 16, tableRows = 9, rowHMin = 22, rowHMax = 28, tableRowReserve = 214;
    constexpr int loudnessBlockH = 98, correlationBlockH = 68, stripGapMin = 12;

    // Settings popover (ui-spec 6.1, 6.2)
    constexpr int popoverFromRight = 572, popoverY = 48;
    constexpr int popoverW = 560, popoverSettingsH = 336, popoverAboutH = 440;
    constexpr int popoverTitleH = 36, popoverBtn = 28, popoverBtnY = 4;
    constexpr int popoverAboutBtnX = 460, popoverAboutBtnW = 60, popoverCloseX = 528;
    constexpr int popoverColW = 256, popoverColGap = 16;
    constexpr int popoverPadTop = 12, popoverPadSide = 16, popoverPadBottom = 16;
    constexpr int popoverSectionH = 20, popoverRowH = 28;
    constexpr int popoverTitleX = 16;
    constexpr int fieldW = 128, fieldH = 24;
    constexpr int aboutBackX = 4, aboutTitleX = 36, aboutTitleW = 200;
    constexpr int aboutLogoMarkX = 16, aboutLogoY = 52, aboutLogoMarkSize = 32;
    constexpr int aboutWordmarkX = 60;
    constexpr int aboutVersionY = 72, aboutCompanyY = 88, aboutLineH = 14, aboutLineW = 480;
    constexpr int aboutDividerY = 112;
    constexpr int aboutSectionY = 120, aboutViewportY = 140, aboutViewportH = 284;
    constexpr int aboutViewportW = 528;
    constexpr int aboutHeadingGap = 12, aboutParagraphGap = 4;
    constexpr int scrollbarW = 6;
    constexpr int chevronW = 5, chevronH = 10, closeCross = 5;

    // Menu (ui-spec 7.1)
    constexpr int menuItemH = 24, menuPadding = 4, menuDot = 6, menuDotX = 8, menuGap = 4;
    constexpr int menuMinTextPad = 24;

    // Tooltip
    constexpr int tooltipPadX = 8, tooltipPadY = 6, tooltipMaxW = 280, tooltipLineGap = 2;
    constexpr int tooltipCursorOffset = 16;

    // Interaction (tokens.json layout)
    constexpr float dragPixelsFullRange = 240.0f;
    constexpr float fineFactor = 0.1f;
    constexpr float choicePixelsPerStep = 20.0f;
    constexpr float choicePixelsPerStepFine = 60.0f;
    constexpr int   clickSlopPx = 4;
    constexpr float wheelNormalised = 0.02f, wheelNormalisedFine = 0.004f;
    constexpr float dbDragUnitsPerPx = 0.5f, dbDragUnitsPerPxFine = 0.1f;
    constexpr float slopeDragUnitsPerPx = 0.1f, slopeDragUnitsPerPxFine = 0.01f;
    constexpr int   pageSteps = 5;

    // Chip internals
    constexpr int chipLabelH = 12, chipValueY = 20, chipValueH = 16, chipInset = 4;
    constexpr int fieldPadX = 8;
    constexpr int halfHighlightPadY = 3;
    constexpr int sepFontPad = 0;
}

} // namespace investigate::tokens
