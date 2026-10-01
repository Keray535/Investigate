#include "SettingsPopover.h"

#include "LayoutMath.h"
#include "NoticesText.h"

#include "../params/ParameterIDs.h"

namespace investigate
{

namespace tk = tokens;
namespace L = tokens::layout;

namespace
{
    // The component's bounds include room for the one popover shadow.
    constexpr int shadowMargin = tk::shadow::blur + tk::shadow::offsetY;

    constexpr int focusOrderAbout = 1;
    constexpr int focusOrderFirstField = 2;
}

//==============================================================================
PopoverButton::PopoverButton (Glyph glyphToUse, const juce::String& labelText, const juce::String& accessibleName)
    : glyph (glyphToUse), text (labelText)
{
    setTitle (accessibleName);
    setWantsKeyboardFocus (true);
}

void PopoverButton::paint (juce::Graphics& g)
{
    auto& laf = InvestigateLookAndFeel::from (*this);
    const auto bounds = getLocalBounds().toFloat();

    if (hovering)
    {
        g.setColour (tk::colour::surface3);
        g.fillRoundedRectangle (bounds, tk::radius::control);
    }

    const auto contentColour = hovering ? tk::colour::textPrimary : tk::colour::textSecondary;

    if (glyph == Glyph::text)
    {
        laf.drawText (g, text, getLocalBounds(), tk::Type::body, contentColour, juce::Justification::centred);
    }
    else if (glyph == Glyph::close)
    {
        const auto c = bounds.getCentre();
        const float r = (float) L::closeCross;

        g.setColour (contentColour);
        g.drawLine (c.x - r, c.y - r, c.x + r, c.y + r, tk::stroke::hairline);
        g.drawLine (c.x - r, c.y + r, c.x + r, c.y - r, tk::stroke::hairline);
    }
    else
    {
        const auto c = bounds.getCentre();
        const float halfH = (float) L::chevronH * 0.5f;
        const float halfW = (float) L::chevronW * 0.5f;

        g.setColour (contentColour);
        g.drawLine (c.x + halfW, c.y - halfH, c.x - halfW, c.y, tk::stroke::hairline);
        g.drawLine (c.x - halfW, c.y, c.x + halfW, c.y + halfH, tk::stroke::hairline);
    }

    if (keyboardFocusRing)
    {
        g.setColour (tk::colour::accent);
        g.drawRoundedRectangle (bounds.reduced (tk::stroke::focus * 0.5f), tk::radius::control, tk::stroke::focus);
    }
}

void PopoverButton::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void PopoverButton::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void PopoverButton::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onClick != nullptr)
        onClick();
}

bool PopoverButton::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey)
    {
        if (onClick != nullptr)
            onClick();

        return true;
    }

    return false;
}

void PopoverButton::focusGained (FocusChangeType cause)
{
    keyboardFocusRing = cause == focusChangedByTabKey;
    repaint();
}

void PopoverButton::focusLost (FocusChangeType)
{
    keyboardFocusRing = false;
    repaint();
}

//==============================================================================
SettingsPopover::SettingsPopover (juce::AudioProcessorValueTreeState& apvts, DisplayContext& context,
                                  const juce::String& versionLineToUse)
    : versionLine (versionLineToUse),
      displayContext (context),
      aboutButton (PopoverButton::Glyph::text, "About", "About"),
      closeButton (PopoverButton::Glyph::close, {}, "Close"),
      backButton (PopoverButton::Glyph::back, {}, "Back to settings")
{
    setFocusContainerType (FocusContainerType::keyboardFocusContainer);
    setTitle ("Settings");

    const int col1X = L::popoverPadSide;
    const int col2X = L::popoverPadSide + L::popoverColW + L::popoverColGap;
    const int firstRowY = L::popoverTitleH + L::popoverPadTop + L::popoverSectionH;

    struct FieldSpec
    {
        const char* id;
        const char* name;
    };

    static constexpr FieldSpec spectrumFields[] = {
        { ParamIDs::channelMode, "Channel mode" }, { ParamIDs::fftSize, "Block size" },
        { ParamIDs::overlap, "Overlap" },          { ParamIDs::window, "Window" },
        { ParamIDs::avgTime, "Average time" },     { ParamIDs::smoothing, "Smoothing" },
        { ParamIDs::slope, "Slope" },              { ParamIDs::secondary, "Secondary curve" },
        { ParamIDs::showSidechain, "Sidechain trace" }
    };

    static constexpr FieldSpec rangeFields[] = {
        { ParamIDs::freqLow, "Frequency low" }, { ParamIDs::freqHigh, "Frequency high" },
        { ParamIDs::dbLow, "Range low" },       { ParamIDs::dbHigh, "Range high" }
    };

    static constexpr FieldSpec meterFields[] = {
        { ParamIDs::meterScale, "Meter scale" }, { ParamIDs::rmsWindow, "RMS window" }
    };

    const int sectionY = L::popoverTitleH + L::popoverPadTop;

    sections.push_back ({ "Spectrum", { col1X, sectionY, L::popoverColW, L::popoverSectionH } });

    int i = 0;

    for (const auto& f : spectrumFields)
        addRow (apvts, context, f.id, f.name, { col1X, firstRowY + (i++) * L::popoverRowH });

    sections.push_back ({ "Display range", { col2X, sectionY, L::popoverColW, L::popoverSectionH } });

    i = 0;

    for (const auto& f : rangeFields)
        addRow (apvts, context, f.id, f.name, { col2X, firstRowY + (i++) * L::popoverRowH });

    const int metersSectionY = firstRowY + i * L::popoverRowH + L::popoverPadTop;
    sections.push_back ({ "Meters", { col2X, metersSectionY, L::popoverColW, L::popoverSectionH } });

    i = 0;

    for (const auto& f : meterFields)
        addRow (apvts, context, f.id, f.name, { col2X, metersSectionY + L::popoverSectionH + (i++) * L::popoverRowH });

    aboutButton.onClick = [this] { showAboutView(); };
    closeButton.onClick = [this] { requestClose(); };
    backButton.onClick = [this] { showSettingsView(); };

    addAndMakeVisible (aboutButton);
    addAndMakeVisible (closeButton);
    addChildComponent (backButton);

    setVisible (false);
    applyViewVisibility();
}

SettingsPopover::~SettingsPopover()
{
    if (auto* parent = getParentComponent())
        parent->removeMouseListener (this);
}

void SettingsPopover::addRow (juce::AudioProcessorValueTreeState& apvts, DisplayContext& context,
                              const char* paramID, const juce::String& label, juce::Point<int> origin)
{
    Row row;
    row.label = label;
    row.origin = origin;
    row.field = std::make_unique<ParamControl> (apvts, paramID, ParamControl::Style::field, juce::String(), label, &context);
    row.field->setExplicitFocusOrder (focusOrderFirstField + (int) rows.size());
    addAndMakeVisible (*row.field);
    rows.push_back (std::move (row));
}

//==============================================================================
juce::Rectangle<int> SettingsPopover::getFrame() const
{
    return getLocalBounds().reduced (shadowMargin);
}

void SettingsPopover::updateForEditorWidth (int editorWidth)
{
    setBounds (LayoutMath::computePopoverBounds (editorWidth, aboutShown).expanded (shadowMargin));
}

void SettingsPopover::open (int editorWidth)
{
    aboutShown = false;
    applyViewVisibility();
    updateForEditorWidth (editorWidth);
    setVisible (true);
    toFront (false);

    if (auto* parent = getParentComponent())
        parent->addMouseListener (this, true);

    aboutButton.grabKeyboardFocus();
}

void SettingsPopover::close()
{
    if (auto* parent = getParentComponent())
        parent->removeMouseListener (this);

    setVisible (false);
}

void SettingsPopover::requestClose()
{
    if (onCloseRequested != nullptr)
        onCloseRequested();
    else
        close();
}

void SettingsPopover::showSettingsView()
{
    aboutShown = false;
    applyViewVisibility();

    if (auto* parent = getParentComponent())
        updateForEditorWidth (parent->getWidth());

    aboutButton.grabKeyboardFocus();
}

void SettingsPopover::showAboutView()
{
    aboutShown = true;

    if (! noticesBuilt)
        buildNotices();

    applyViewVisibility();

    if (auto* parent = getParentComponent())
        updateForEditorWidth (parent->getWidth());

    backButton.grabKeyboardFocus();
}

void SettingsPopover::applyViewVisibility()
{
    for (auto& row : rows)
        row.field->setVisible (! aboutShown);

    aboutButton.setVisible (! aboutShown);
    backButton.setVisible (aboutShown);

    if (notices != nullptr)
        notices->setVisible (aboutShown);

    setTitle (aboutShown ? "About Investigate" : "Settings");

    aboutButton.setExplicitFocusOrder (focusOrderAbout);
    closeButton.setExplicitFocusOrder (aboutShown ? 3 : focusOrderFirstField + (int) rows.size());
    backButton.setExplicitFocusOrder (1);

    if (notices != nullptr)
        notices->setExplicitFocusOrder (2);

    repaint();
}

void SettingsPopover::refreshClampState()
{
    for (auto& row : rows)
        row.field->repaint();
}

//==============================================================================
void SettingsPopover::buildNotices()
{
    auto& laf = InvestigateLookAndFeel::from (*this);

    notices = std::make_unique<juce::TextEditor>();
    notices->setMultiLine (true, true);
    notices->setReturnKeyStartsNewLine (false);
    notices->setScrollbarsShown (true);
    notices->setPopupMenuEnabled (false);
    notices->setCaretVisible (false);
    notices->setBorder (juce::BorderSize<int> (0));
    notices->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    notices->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    notices->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    notices->setTitle ("Credits and licenses");

    const auto blocks = NoticesText::build (juce::SystemStats::getJUCEVersion());
    bool first = true;

    for (const auto& block : blocks)
    {
        if (block.kind == NoticesText::Block::Kind::heading)
        {
            // A blank line in a small font stands in for the 12 px of space
            // above a heading (a TextEditor has no paragraph spacing).
            if (! first)
            {
                notices->setFont (laf.getFont (tk::Type::unit));
                notices->insertTextAtCaret ("\n");
            }

            notices->setFont (laf.getFont (tk::Type::bodySemi));
            notices->setColour (juce::TextEditor::textColourId, tk::colour::textPrimary);
        }
        else
        {
            notices->setFont (laf.getFont (tk::Type::bodySm));
            notices->setColour (juce::TextEditor::textColourId, tk::colour::textSecondary);
        }

        notices->insertTextAtCaret (block.text + "\n");
        first = false;
    }

    notices->moveCaretToTop (false);
    notices->setReadOnly (true);
    addChildComponent (*notices);
    noticesBuilt = true;
    resized();
}

//==============================================================================
void SettingsPopover::resized()
{
    const auto f = getFrame();

    aboutButton.setBounds (f.getX() + L::popoverAboutBtnX, f.getY() + L::popoverBtnY, L::popoverAboutBtnW, L::popoverBtn);
    closeButton.setBounds (f.getX() + L::popoverCloseX, f.getY() + L::popoverBtnY, L::popoverBtn, L::popoverBtn);
    backButton.setBounds (f.getX() + L::aboutBackX, f.getY() + L::popoverBtnY, L::popoverBtn, L::popoverBtn);

    for (auto& row : rows)
        row.field->setBounds (f.getX() + row.origin.x + L::popoverColW - L::fieldW,
                              f.getY() + row.origin.y + (L::popoverRowH - L::fieldH) / 2,
                              L::fieldW, L::fieldH);

    if (notices != nullptr)
        notices->setBounds (f.getX() + L::popoverPadSide, f.getY() + L::aboutViewportY, L::aboutViewportW, L::aboutViewportH);

    rebuildShadow();
}

void SettingsPopover::rebuildShadow()
{
    const auto size = getLocalBounds();

    if (size.isEmpty())
        return;

    if (shadowImage.isValid() && shadowFrameSize == size)
        return;

    // Rendered once per size (ui-spec 3.3): switching views re-renders it.
    shadowFrameSize = size;
    shadowImage = juce::Image (juce::Image::ARGB, size.getWidth(), size.getHeight(), true);

    juce::Graphics g (shadowImage);
    juce::DropShadow shadow (tk::colour::black.withAlpha (tk::alpha::popoverShadow), tk::shadow::blur,
                             { 0, tk::shadow::offsetY });
    shadow.drawForRectangle (g, getFrame());
}

bool SettingsPopover::hitTest (int x, int y)
{
    return getFrame().contains (x, y);
}

void SettingsPopover::paint (juce::Graphics& g)
{
    auto& laf = InvestigateLookAndFeel::from (*this);
    const auto f = getFrame();

    if (shadowImage.isValid())
        g.drawImageAt (shadowImage, 0, 0);

    g.setColour (tk::colour::surface);
    g.fillRoundedRectangle (f.toFloat(), tk::radius::popover);
    g.setColour (tk::colour::border);
    g.drawRoundedRectangle (f.toFloat().reduced (0.5f), tk::radius::popover, tk::stroke::hairline);

    // Title bar divider.
    g.fillRect (f.getX(), f.getY() + L::popoverTitleH, f.getWidth(), 1);

    if (! aboutShown)
    {
        laf.drawText (g, "Settings", { f.getX() + L::popoverTitleX, f.getY(), L::aboutTitleW, L::popoverTitleH },
                      tk::Type::bodySemi, tk::colour::textPrimary);

        for (const auto& s : sections)
            laf.drawText (g, s.title, s.rect.translated (f.getX(), f.getY()), tk::Type::caps, tk::colour::textTertiary);

        for (const auto& row : rows)
            laf.drawText (g, row.label,
                          { f.getX() + row.origin.x, f.getY() + row.origin.y, L::popoverColW - L::fieldW, L::popoverRowH },
                          tk::Type::body, tk::colour::textSecondary);

        return;
    }

    laf.drawText (g, "About", { f.getX() + L::aboutTitleX, f.getY(), L::aboutTitleW, L::popoverTitleH },
                  tk::Type::bodySemi, tk::colour::textPrimary);

    if (const auto* mark = laf.getIcon (Icon::logoMark, tk::colour::accent, false))
        mark->drawWithin (g, { (float) (f.getX() + L::aboutLogoMarkX), (float) (f.getY() + L::aboutLogoY),
                               (float) L::aboutLogoMarkSize, (float) L::aboutLogoMarkSize },
                          juce::RectanglePlacement::centred, 1.0f);

    if (const auto* word = laf.getIcon (Icon::logoWordmark, tk::colour::textPrimary, false))
        word->drawWithin (g, { (float) (f.getX() + L::aboutWordmarkX), (float) (f.getY() + L::aboutLogoY),
                               (float) L::wordmarkW, (float) L::wordmarkH },
                          juce::RectanglePlacement::centred, 1.0f);

    laf.drawText (g, versionLine, { f.getX() + L::aboutWordmarkX, f.getY() + L::aboutVersionY, L::aboutLineW, L::aboutLineH },
                  tk::Type::bodySm, tk::colour::textSecondary);
    laf.drawText (g, "Investigate Audio",
                  { f.getX() + L::aboutWordmarkX, f.getY() + L::aboutCompanyY, L::aboutLineW, L::aboutLineH },
                  tk::Type::bodySm, tk::colour::textTertiary);

    g.setColour (tk::colour::border);
    g.fillRect (f.getX() + L::popoverPadSide, f.getY() + L::aboutDividerY, L::aboutViewportW, 1);

    laf.drawText (g, "Credits and licenses",
                  { f.getX() + L::popoverPadSide, f.getY() + L::aboutSectionY, L::aboutViewportW, L::popoverSectionH },
                  tk::Type::caps, tk::colour::textTertiary);
}

//==============================================================================
bool SettingsPopover::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        requestClose();
        return true;
    }

    if (aboutShown && key == juce::KeyPress::backspaceKey && (notices == nullptr || ! notices->hasKeyboardFocus (true)))
    {
        showSettingsView();
        return true;
    }

    return false;
}

void SettingsPopover::mouseDown (const juce::MouseEvent& e)
{
    if (! isVisible())
        return;

    const auto* source = e.eventComponent;

    if (source == nullptr || source == this || isParentOf (source))
        return;

    if (ignoredComponent != nullptr && (source == ignoredComponent || ignoredComponent->isParentOf (source)))
        return;

    requestClose();
}

} // namespace investigate
