#pragma once

#include "DesignTokens.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <map>
#include <memory>
#include <tuple>

namespace investigate
{

/** SVG assets from the InvestigateAssets binary-data target (ui-spec 11). */
enum class Icon { freeze, reset, settings, sidechain, meters, logoMark, logoWordmark };

/**
    Investigate's LookAndFeel (T-011, ui-spec 3 and 7.1). It owns the four
    bundled typefaces and the cached, recoloured SVG drawables, and styles the
    pop-up menu, tooltip, scroll bar, corner grip and text editor from the
    design tokens. Components fetch fonts and icons through it, so each
    typeface is created once and each SVG is parsed once per state colour.

    Message-thread only (fonts and drawables are not touched from audio code).
*/
class InvestigateLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    InvestigateLookAndFeel();
    ~InvestigateLookAndFeel() override;

    /** The look-and-feel of `c` if it is ours; asserts otherwise. */
    static InvestigateLookAndFeel& from (const juce::Component& c);

    /** The font for a type token (ui-spec 3.2). Typeface weights map one to
        one to bundled files; no synthetic bold is used. */
    const juce::Font& getFont (tokens::Type type) const noexcept;

    /** Draws `text` in the type token's style (uppercasing caps styles). */
    void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   tokens::Type type, juce::Colour colour,
                   juce::Justification justification = juce::Justification::centredLeft) const;

    /** Width of `text` in the type token's style. */
    float getTextWidth (const juce::String& text, tokens::Type type) const;

    /** A drawable in `colour`, cached per (icon, colour, size variant). The
        16 px icons use the "-16" artwork; null for logos' unused variants. */
    const juce::Drawable* getIcon (Icon icon, juce::Colour colour, bool small16);

    /** The tooltip text format: first line, then an optional second line
        after '\n' in the secondary style. */
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

    int getDefaultScrollbarWidth() override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isVertical, int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown) override;

    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    // Pop-up menus (ui-spec 7.1).
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&, int width, int height, const juce::PopupMenu::Options&) override;
    void drawPopupMenuItemWithOptions (juce::Graphics&, const juce::Rectangle<int>& area, bool isHighlighted,
                                       const juce::PopupMenu::Item&, const juce::PopupMenu::Options&) override;
    void getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                               int& idealWidth, int& idealHeight, const juce::PopupMenu::Options&) override;
    int getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&) override;

private:
    struct Typefaces
    {
        juce::Typeface::Ptr interMedium, interSemiBold, monoRegular, monoMedium;
    };

    juce::Font makeFont (tokens::Type type) const;

    Typefaces typefaces;
    std::array<juce::Font, 15> fonts;

    std::map<std::tuple<int, juce::uint32, bool>, std::unique_ptr<juce::Drawable>> iconCache;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InvestigateLookAndFeel)
};

} // namespace investigate
