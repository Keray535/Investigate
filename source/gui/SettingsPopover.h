#pragma once

#include "DisplayContext.h"
#include "ParamControl.h"

#include <functional>
#include <memory>
#include <vector>

namespace investigate
{

/** A small button in the popover title bars: the "About" text button, the
    close cross and the back chevron (ui-spec 6.1, 6.2). */
class PopoverButton final : public juce::Component,
                            public juce::TooltipClient
{
public:
    enum class Glyph { text, close, back };

    PopoverButton (Glyph glyph, const juce::String& text, const juce::String& accessibleName);

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;
    juce::String getTooltip() override { return {}; }

private:
    Glyph glyph;
    juce::String text;
    bool hovering = false;
    bool keyboardFocusRing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PopoverButton)
};

/**
    The Settings popover and its About view (ui-spec 6.1 and 6.2): one panel
    anchored under the header, 560 x 336 for Settings and 560 x 440 for About,
    with the single cached drop shadow. The component's bounds include the
    shadow margin; only the frame inside it takes mouse events.

    It closes on Esc, on a click outside it and on the close button; focus
    stays inside while it is open (a focus container).
*/
class SettingsPopover final : public juce::Component
{
public:
    SettingsPopover (juce::AudioProcessorValueTreeState& apvts, DisplayContext& context,
                     const juce::String& versionLine);
    ~SettingsPopover() override;

    /** Shows the Settings view, positioned for the given editor width. */
    void open (int editorWidth);
    void close();
    bool isOpen() const noexcept { return isVisible(); }

    /** Re-anchors it after the editor was resized. */
    void updateForEditorWidth (int editorWidth);

    /** Clicks on this component (the gear) do not count as "outside". */
    void setIgnoredComponent (juce::Component* c) noexcept { ignoredComponent = c; }

    /** Called when the popover wants to close (Esc, outside click, close). */
    std::function<void()> onCloseRequested;

    /** Repaint the fields whose warning state depends on the range. */
    void refreshClampState();

    bool isAboutShown() const noexcept { return aboutShown; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Row
    {
        juce::String label;
        std::unique_ptr<ParamControl> field;
        juce::Point<int> origin; // frame-relative top-left of the 256 x 28 row
    };

    struct Section
    {
        juce::String title;
        juce::Rectangle<int> rect;
    };

    juce::Rectangle<int> getFrame() const;
    void addRow (juce::AudioProcessorValueTreeState& apvts, DisplayContext& context,
                 const char* paramID, const juce::String& label, juce::Point<int> origin);
    void showSettingsView();
    void showAboutView();
    void applyViewVisibility();
    void rebuildShadow();
    void buildNotices();
    void requestClose();

    juce::String versionLine;
    DisplayContext& displayContext;
    juce::Component* ignoredComponent = nullptr;

    bool aboutShown = false;
    bool noticesBuilt = false;

    std::vector<Row> rows;
    std::vector<Section> sections;
    PopoverButton aboutButton;
    PopoverButton closeButton;
    PopoverButton backButton;
    std::unique_ptr<juce::TextEditor> notices;

    juce::Image shadowImage;
    juce::Rectangle<int> shadowFrameSize;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPopover)
};

} // namespace investigate
