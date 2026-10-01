#include "InvestigateLookAndFeel.h"

#include <BinaryData.h>

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

namespace
{
    struct SvgSource
    {
        const char* data;
        int size;
    };

    SvgSource svgFor (Icon icon, bool small16) noexcept
    {
        switch (icon)
        {
            case Icon::freeze:        return small16 ? SvgSource { BinaryData::iconfreeze16_svg, BinaryData::iconfreeze16_svgSize }
                                                     : SvgSource { BinaryData::iconfreeze_svg, BinaryData::iconfreeze_svgSize };
            case Icon::reset:         return small16 ? SvgSource { BinaryData::iconreset16_svg, BinaryData::iconreset16_svgSize }
                                                     : SvgSource { BinaryData::iconreset_svg, BinaryData::iconreset_svgSize };
            case Icon::settings:      return small16 ? SvgSource { BinaryData::iconsettings16_svg, BinaryData::iconsettings16_svgSize }
                                                     : SvgSource { BinaryData::iconsettings_svg, BinaryData::iconsettings_svgSize };
            case Icon::sidechain:     return small16 ? SvgSource { BinaryData::iconsidechain16_svg, BinaryData::iconsidechain16_svgSize }
                                                     : SvgSource { BinaryData::iconsidechain_svg, BinaryData::iconsidechain_svgSize };
            case Icon::meters:        return small16 ? SvgSource { BinaryData::iconmeters16_svg, BinaryData::iconmeters16_svgSize }
                                                     : SvgSource { BinaryData::iconmeters_svg, BinaryData::iconmeters_svgSize };
            case Icon::logoMark:      return { BinaryData::logomark_svg, BinaryData::logomark_svgSize };
            case Icon::logoWordmark:  return { BinaryData::logowordmark_svg, BinaryData::logowordmark_svgSize };
        }

        return { nullptr, 0 };
    }

    juce::Typeface::Ptr loadTypeface (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    /** First line in body-sm / text-primary, an optional second line in the
        unit size / text-secondary (ui-spec 6, tooltips). */
    juce::AttributedString makeTooltipText (const InvestigateLookAndFeel& laf, const juce::String& text)
    {
        juce::AttributedString s;
        s.setJustification (juce::Justification::topLeft);
        s.setWordWrap (juce::AttributedString::byWord);

        const auto newline = text.indexOfChar ('\n');
        const auto first = newline < 0 ? text : text.substring (0, newline);

        s.append (first, laf.getFont (tk::Type::bodySm), tk::colour::textPrimary);

        if (newline >= 0)
        {
            s.append ("\n", laf.getFont (tk::Type::unit), tk::colour::textSecondary);
            s.append (text.substring (newline + 1), laf.getFont (tk::Type::unit), tk::colour::textSecondary);
        }

        return s;
    }
}

//==============================================================================
InvestigateLookAndFeel::InvestigateLookAndFeel()
{
    typefaces.interMedium   = loadTypeface (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    typefaces.interSemiBold = loadTypeface (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    typefaces.monoRegular   = loadTypeface (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    typefaces.monoMedium    = loadTypeface (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize);

    for (size_t i = 0; i < fonts.size(); ++i)
        fonts[i] = makeFont ((tk::Type) i);

    namespace c = tk::colour;

    setColour (juce::TextEditor::backgroundColourId, c::bgPlot);
    setColour (juce::TextEditor::textColourId, c::textPrimary);
    setColour (juce::TextEditor::highlightColourId, c::accent.withAlpha (tk::alpha::selection));
    setColour (juce::TextEditor::highlightedTextColourId, c::textPrimary);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, c::accent);
    setColour (juce::CaretComponent::caretColourId, c::accent);
    setColour (juce::ScrollBar::thumbColourId, c::border);
    setColour (juce::PopupMenu::backgroundColourId, c::surface);
    setColour (juce::PopupMenu::textColourId, c::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, c::surface3);
    setColour (juce::PopupMenu::highlightedTextColourId, c::textPrimary);
    setColour (juce::TooltipWindow::backgroundColourId, c::surface3);
    setColour (juce::TooltipWindow::textColourId, c::textPrimary);
    setColour (juce::TooltipWindow::outlineColourId, c::border);
    setColour (juce::ResizableWindow::backgroundColourId, c::bgApp);

    setDefaultSansSerifTypeface (typefaces.interMedium);
}

InvestigateLookAndFeel::~InvestigateLookAndFeel() = default;

InvestigateLookAndFeel& InvestigateLookAndFeel::from (const juce::Component& c)
{
    auto* laf = dynamic_cast<InvestigateLookAndFeel*> (&c.getLookAndFeel());
    jassert (laf != nullptr);
    return *laf;
}

juce::Font InvestigateLookAndFeel::makeFont (tk::Type type) const
{
    const auto spec = tk::typeSpec (type);
    juce::Typeface::Ptr face;

    switch (spec.face)
    {
        case tk::Face::interMedium:   face = typefaces.interMedium;   break;
        case tk::Face::interSemiBold: face = typefaces.interSemiBold; break;
        case tk::Face::monoRegular:   face = typefaces.monoRegular;   break;
        case tk::Face::monoMedium:    face = typefaces.monoMedium;    break;
    }

    // The point height makes the requested size the em size, as in CSS, so
    // the glyphs match the mockup's px sizes.
    auto options = face != nullptr ? juce::FontOptions (face) : juce::FontOptions();
    juce::Font font (options.withPointHeight (spec.size));

    if (spec.tracking != 0.0f)
        font = font.withExtraKerningFactor (spec.tracking);

    return font;
}

const juce::Font& InvestigateLookAndFeel::getFont (tk::Type type) const noexcept
{
    return fonts[(size_t) type];
}

void InvestigateLookAndFeel::drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                                        tk::Type type, juce::Colour colour, juce::Justification justification) const
{
    g.setColour (colour);
    g.setFont (getFont (type));
    g.drawText (tk::typeSpec (type).uppercase ? text.toUpperCase() : text, area, justification, false);
}

float InvestigateLookAndFeel::getTextWidth (const juce::String& text, tk::Type type) const
{
    return juce::GlyphArrangement::getStringWidth (getFont (type),
                                                   tk::typeSpec (type).uppercase ? text.toUpperCase() : text);
}

const juce::Drawable* InvestigateLookAndFeel::getIcon (Icon icon, juce::Colour colour, bool small16)
{
    const auto key = std::make_tuple ((int) icon, colour.getARGB(), small16);

    if (const auto it = iconCache.find (key); it != iconCache.end())
        return it->second.get();

    const auto source = svgFor (icon, small16);
    std::unique_ptr<juce::Drawable> drawable;

    if (source.data != nullptr)
    {
        // The SVGs use currentColor only (ui-spec 11): substitute the state
        // colour in the text, then parse once.
        const auto svg = juce::String::fromUTF8 (source.data, source.size)
                             .replace ("currentColor", "#" + colour.withAlpha (1.0f).toDisplayString (false));

        if (const auto xml = juce::XmlDocument::parse (svg))
            drawable = juce::Drawable::createFromSVG (*xml);
    }

    const auto* result = drawable.get();
    iconCache.emplace (key, std::move (drawable));
    return result;
}

//==============================================================================
juce::Rectangle<int> InvestigateLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                               juce::Rectangle<int> parentArea)
{
    juce::TextLayout layout;
    layout.createLayout (makeTooltipText (*this, tipText), (float) (L::tooltipMaxW - 2 * L::tooltipPadX));

    const int w = (int) std::ceil (layout.getWidth()) + 2 * L::tooltipPadX;
    const int h = (int) std::ceil (layout.getHeight()) + 2 * L::tooltipPadY;

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - L::tooltipCursorOffset
                                                                       : screenPos.x + L::tooltipCursorOffset + L::tooltipPadX,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - h - L::tooltipPadY
                                                                       : screenPos.y + L::tooltipCursorOffset,
                                 w, h)
        .constrainedWithin (parentArea);
}

void InvestigateLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (tk::colour::surface3);
    g.fillRoundedRectangle (bounds, tk::radius::small);
    g.setColour (tk::colour::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), tk::radius::small, tk::stroke::hairline);

    juce::TextLayout layout;
    layout.createLayout (makeTooltipText (*this, text), (float) (width - 2 * L::tooltipPadX));
    layout.draw (g, bounds.reduced ((float) L::tooltipPadX, (float) L::tooltipPadY));
}

//==============================================================================
int InvestigateLookAndFeel::getDefaultScrollbarWidth()
{
    return L::scrollbarW;
}

void InvestigateLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                             bool isVertical, int thumbStart, int thumbSize,
                                             bool isMouseOver, bool isMouseDown)
{
    // Transparent track, thumb in `border`, `text-tertiary` on hover or drag
    // (ui-spec 6.2).
    const auto thumb = isVertical ? juce::Rectangle<int> (x, y + thumbStart, width, thumbSize)
                                  : juce::Rectangle<int> (x + thumbStart, y, thumbSize, height);

    g.setColour (isMouseOver || isMouseDown ? tk::colour::textTertiary : tk::colour::border);
    g.fillRoundedRectangle (thumb.toFloat(), (float) L::scrollbarW * 0.5f);
}

void InvestigateLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool, bool)
{
    // Six 1 px-radius dots in text-tertiary, in a triangle at the bottom-right
    // corner of the grip (ui-spec 2).
    g.setColour (tk::colour::textTertiary);

    const float pitch = (float) w / 4.0f;

    for (int row = 0; row < 3; ++row)
        for (int col = 0; col <= row; ++col)
        {
            const float cx = (float) w - pitch * (0.7f + (float) col);
            const float cy = (float) h - pitch * (0.7f + (float) (row - col));
            g.fillEllipse (cx - (float) L::resizeGripDot, cy - (float) L::resizeGripDot,
                           2.0f * (float) L::resizeGripDot, 2.0f * (float) L::resizeGripDot);
        }
}

void InvestigateLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    g.setColour (te.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, tk::radius::small);
}

void InvestigateLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    const auto id = te.hasKeyboardFocus (true) && ! te.isReadOnly() ? juce::TextEditor::focusedOutlineColourId
                                                                     : juce::TextEditor::outlineColourId;
    const auto c = te.findColour (id);

    if (c.isTransparent())
        return;

    g.setColour (c);
    g.drawRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f),
                            tk::radius::small, tk::stroke::hairline);
}

//==============================================================================
juce::Font InvestigateLookAndFeel::getPopupMenuFont()
{
    return getFont (tk::Type::body);
}

void InvestigateLookAndFeel::drawPopupMenuBackgroundWithOptions (juce::Graphics& g, int width, int height,
                                                                 const juce::PopupMenu::Options&)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    g.setColour (tk::colour::surface);
    g.fillRoundedRectangle (bounds, tk::radius::control);
    g.setColour (tk::colour::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), tk::radius::control, tk::stroke::hairline);
}

void InvestigateLookAndFeel::drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                           bool isHighlighted, const juce::PopupMenu::Item& item,
                                                           const juce::PopupMenu::Options&)
{
    if (item.isSeparator)
    {
        g.setColour (tk::colour::border);
        g.fillRect (area.reduced (L::menuPadding, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    const auto textColour = item.isEnabled ? tk::colour::textPrimary : tk::colour::textDisabled;

    if (isHighlighted && item.isEnabled)
    {
        g.setColour (tk::colour::surface3);
        g.fillRoundedRectangle (area.toFloat(), tk::radius::small);
    }

    if (item.isTicked)
    {
        g.setColour (tk::colour::accent);
        g.fillEllipse ((float) (area.getX() + L::menuDotX), (float) area.getCentreY() - (float) L::menuDot * 0.5f,
                       (float) L::menuDot, (float) L::menuDot);
    }

    const int textX = area.getX() + L::menuDotX + L::menuDot + tk::space::s8;
    auto textArea = juce::Rectangle<int> (textX, area.getY(), area.getRight() - textX - tk::space::s8, area.getHeight());

    if (item.shortcutKeyDescription.isNotEmpty())
    {
        drawText (g, item.shortcutKeyDescription, textArea, tk::Type::unit, tk::colour::textTertiary,
                  juce::Justification::centredRight);
    }

    drawText (g, item.text, textArea, tk::Type::body, textColour);
}

void InvestigateLookAndFeel::getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator, int,
                                                                   int& idealWidth, int& idealHeight,
                                                                   const juce::PopupMenu::Options&)
{
    if (isSeparator)
    {
        idealWidth = 0;
        idealHeight = tk::space::s6;
        return;
    }

    // Room for the trailing "default" marker on every item keeps all items
    // aligned whichever one carries it.
    idealWidth = (int) std::ceil (getTextWidth (text, tk::Type::body))
               + L::menuDotX + L::menuDot + tk::space::s8
               + L::menuMinTextPad + (int) std::ceil (getTextWidth ("default", tk::Type::unit))
               + tk::space::s8;
    idealHeight = L::menuItemH;
}

int InvestigateLookAndFeel::getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&)
{
    return L::menuPadding;
}

} // namespace investigate
