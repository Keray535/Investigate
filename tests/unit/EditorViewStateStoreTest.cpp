// T-011 unit tests for source/gui/ViewStateStore.h: tolerant reads of the
// `ViewState` child (missing child, missing or wrong-typed properties),
// clamping, and writes that recreate the child (roadmap State format;
// ui-spec 2 Persistence).

#include "PluginProcessor.h"
#include "gui/ViewStateStore.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace investigate;
using namespace investigate::ViewStateStore;
using investigate::LayoutMath::EditorSize;

namespace
{
juce::ValueTree makeRoot() { return juce::ValueTree ("InvestigateState"); }

juce::ValueTree makeRootWithView()
{
    auto root = makeRoot();
    root.appendChild (juce::ValueTree (viewStateType), nullptr);
    return root;
}

struct CountingListener : juce::ValueTree::Listener
{
    int propertyChanges = 0;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { ++propertyChanges; }
};
}

TEST_CASE ("ViewStateStore defaults when the child is missing", "[gui][viewstate]")
{
    const auto root = makeRoot();
    CHECK (readEditorSize (root) == EditorSize { 1200, 700 });
    CHECK_FALSE (readMetersCollapsed (root));
}

TEST_CASE ("ViewStateStore tolerates an invalid root tree", "[gui][viewstate]")
{
    juce::ValueTree invalid;
    CHECK (readEditorSize (invalid) == EditorSize { 1200, 700 });
    CHECK_FALSE (readMetersCollapsed (invalid));
    CHECK_NOTHROW (writeEditorSize (invalid, { 1000, 600 }));
    CHECK_NOTHROW (writeMetersCollapsed (invalid, true));
    CHECK_FALSE (invalid.isValid());
}

TEST_CASE ("ViewStateStore: child without properties gives defaults", "[gui][viewstate]")
{
    const auto root = makeRootWithView();
    CHECK (readEditorSize (root) == EditorSize { 1200, 700 });
    CHECK_FALSE (readMetersCollapsed (root));
}

TEST_CASE ("ViewStateStore: one missing dimension falls back individually", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);

    view.setProperty (editorWidthProp, 1500, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 1500, 700 });

    view.removeProperty (editorWidthProp, nullptr);
    view.setProperty (editorHeightProp, 800, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 1200, 800 });
}

TEST_CASE ("ViewStateStore: valid sizes round trip", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);
    view.setProperty (editorWidthProp, 1500, nullptr);
    view.setProperty (editorHeightProp, 900, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 1500, 900 });

    view.setProperty (editorWidthProp, 920, nullptr);
    view.setProperty (editorHeightProp, 540, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 920, 540 });

    view.setProperty (editorWidthProp, 2560, nullptr);
    view.setProperty (editorHeightProp, 1600, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 2560, 1600 });
}

TEST_CASE ("ViewStateStore: out-of-range sizes are clamped on read", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);

    view.setProperty (editorWidthProp, 100, nullptr);
    view.setProperty (editorHeightProp, -5, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 920, 540 });

    view.setProperty (editorWidthProp, 99999, nullptr);
    view.setProperty (editorHeightProp, 99999, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 2560, 1600 });

    view.setProperty (editorWidthProp, 0, nullptr);
    view.setProperty (editorHeightProp, 0, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 920, 540 });
}

TEST_CASE ("ViewStateStore: wrong-typed size properties fall back to defaults", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);

    SECTION ("string")
    {
        view.setProperty (editorWidthProp, "wide", nullptr);
        view.setProperty (editorHeightProp, "tall", nullptr);
    }

    SECTION ("void var")
    {
        view.setProperty (editorWidthProp, juce::var(), nullptr);
        view.setProperty (editorHeightProp, juce::var(), nullptr);
    }

    SECTION ("bool")
    {
        view.setProperty (editorWidthProp, true, nullptr);
        view.setProperty (editorHeightProp, false, nullptr);
    }

    SECTION ("memory block")
    {
        juce::MemoryBlock mb ("abcd", 4);
        view.setProperty (editorWidthProp, mb, nullptr);
        view.setProperty (editorHeightProp, mb, nullptr);
    }

    SECTION ("array")
    {
        view.setProperty (editorWidthProp, juce::Array<juce::var> { 1, 2 }, nullptr);
        view.setProperty (editorHeightProp, juce::Array<juce::var> { 1, 2 }, nullptr);
    }

    CHECK (readEditorSize (root) == EditorSize { 1200, 700 });
}

TEST_CASE ("ViewStateStore: double and int64 sizes are accepted", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);

    view.setProperty (editorWidthProp, 1500.7, nullptr);
    view.setProperty (editorHeightProp, (juce::int64) 900, nullptr);
    CHECK (readEditorSize (root) == EditorSize { 1500, 900 });
}

TEST_CASE ("ViewStateStore: non-finite or huge double sizes stay inside the limits", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);

    for (const double v : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity(), 1.0e30, -1.0e30 })
    {
        view.setProperty (editorWidthProp, v, nullptr);
        view.setProperty (editorHeightProp, v, nullptr);
        const auto s = readEditorSize (root);
        CHECK (s.width >= 920);
        CHECK (s.width <= 2560);
        CHECK (s.height >= 540);
        CHECK (s.height <= 1600);
    }
}

TEST_CASE ("ViewStateStore: writeEditorSize creates the child when a state load removed it", "[gui][viewstate]")
{
    auto root = makeRoot();
    REQUIRE_FALSE (root.getChildWithName (viewStateType).isValid());

    writeEditorSize (root, { 1400, 800 });

    const auto view = root.getChildWithName (viewStateType);
    REQUIRE (view.isValid());
    CHECK ((int) view.getProperty (editorWidthProp) == 1400);
    CHECK ((int) view.getProperty (editorHeightProp) == 800);
    CHECK (readEditorSize (root) == EditorSize { 1400, 800 });
    CHECK (root.getNumChildren() == 1);
}

TEST_CASE ("ViewStateStore: writeEditorSize clamps and does not duplicate the child", "[gui][viewstate]")
{
    auto root = makeRoot();
    writeEditorSize (root, { 10, 10 });
    CHECK (readEditorSize (root) == EditorSize { 920, 540 });
    CHECK ((int) root.getChildWithName (viewStateType).getProperty (editorWidthProp) == 920);

    writeEditorSize (root, { 90000, 90000 });
    CHECK ((int) root.getChildWithName (viewStateType).getProperty (editorWidthProp) == 2560);
    CHECK ((int) root.getChildWithName (viewStateType).getProperty (editorHeightProp) == 1600);
    CHECK (root.getNumChildren() == 1);
}

TEST_CASE ("ViewStateStore: writeEditorSize overwrites wrong-typed values and skips no-op writes", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);
    view.setProperty (editorWidthProp, "garbage", nullptr);

    writeEditorSize (root, { 1300, 750 });
    CHECK (view.getProperty (editorWidthProp).isInt());
    CHECK (readEditorSize (root) == EditorSize { 1300, 750 });

    CountingListener listener;
    view.addListener (&listener);
    writeEditorSize (root, { 1300, 750 });
    CHECK (listener.propertyChanges == 0);

    writeEditorSize (root, { 1301, 750 });
    CHECK (listener.propertyChanges == 1);
    view.removeListener (&listener);
}

TEST_CASE ("ViewStateStore: metersCollapsed reads bool, int and double; ignores other types", "[gui][viewstate]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);

    view.setProperty (metersCollapsedProp, true, nullptr);
    CHECK (readMetersCollapsed (root));
    view.setProperty (metersCollapsedProp, false, nullptr);
    CHECK_FALSE (readMetersCollapsed (root));
    view.setProperty (metersCollapsedProp, 1, nullptr);
    CHECK (readMetersCollapsed (root));
    view.setProperty (metersCollapsedProp, 0, nullptr);
    CHECK_FALSE (readMetersCollapsed (root));
    view.setProperty (metersCollapsedProp, 1.0, nullptr);
    CHECK (readMetersCollapsed (root));

    view.setProperty (metersCollapsedProp, "garbage", nullptr);
    CHECK_FALSE (readMetersCollapsed (root));
    view.setProperty (metersCollapsedProp, juce::var(), nullptr);
    CHECK_FALSE (readMetersCollapsed (root));
}

TEST_CASE ("ViewStateStore: writeMetersCollapsed creates the child and keeps the size", "[gui][viewstate]")
{
    auto root = makeRoot();
    writeMetersCollapsed (root, true);
    REQUIRE (root.getChildWithName (viewStateType).isValid());
    CHECK (readMetersCollapsed (root));

    writeEditorSize (root, { 1400, 800 });
    writeMetersCollapsed (root, false);
    CHECK_FALSE (readMetersCollapsed (root));
    CHECK (readEditorSize (root) == EditorSize { 1400, 800 });
    CHECK (root.getNumChildren() == 1);
}

TEST_CASE ("ViewStateStore: state survives an XML round trip and a replaced root", "[gui][viewstate]")
{
    auto root = makeRoot();
    writeEditorSize (root, { 1432, 811 });
    writeMetersCollapsed (root, true);

    const auto xml = root.createXml();
    REQUIRE (xml != nullptr);
    const auto restored = juce::ValueTree::fromXml (*xml);
    CHECK (readEditorSize (restored) == EditorSize { 1432, 811 });
    CHECK (readMetersCollapsed (restored));

    // replaceState swaps the whole tree: reads and writes always go through the new root.
    auto replacement = makeRoot();
    CHECK (readEditorSize (replacement) == EditorSize { 1200, 700 });
    writeEditorSize (replacement, { 1000, 600 });
    CHECK (readEditorSize (replacement) == EditorSize { 1000, 600 });
}

TEST_CASE ("ViewStateStore: an unrelated ViewState-like child does not confuse reads", "[gui][viewstate]")
{
    auto root = makeRoot();
    root.appendChild (juce::ValueTree ("PARAM"), nullptr);
    root.appendChild (juce::ValueTree ("Other"), nullptr);
    writeEditorSize (root, { 1100, 650 });
    CHECK (root.getNumChildren() == 3);
    CHECK (readEditorSize (root) == EditorSize { 1100, 650 });
}

TEST_CASE ("ViewStateStore: size and metersCollapsed persist through the processor's real state save and restore", "[gui][viewstate][processor]")
{
    // The host path is getStateInformation (XML) -> setStateInformation
    // (ValueTree::fromXml), which turns every property into a string. The
    // editor reads the size through ViewStateStore in its constructor
    // (T-011 acceptance: the size persists across a state save and restore).
    InvestigateProcessor source;
    writeEditorSize (source.getApvts().state, { 1432, 811 });
    writeMetersCollapsed (source.getApvts().state, true);

    juce::MemoryBlock block;
    source.getStateInformation (block);
    REQUIRE (block.getSize() > 0);

    InvestigateProcessor restored;
    restored.setStateInformation (block.getData(), (int) block.getSize());

    CHECK (readEditorSize (restored.getApvts().state) == EditorSize { 1432, 811 });
    CHECK (readMetersCollapsed (restored.getApvts().state));
}

TEST_CASE ("ViewStateStore: numeric strings as produced by XML parsing are read as numbers", "[gui][viewstate][processor]")
{
    auto root = makeRootWithView();
    auto view = root.getChildWithName (viewStateType);
    view.setProperty (editorWidthProp, "1432", nullptr);
    view.setProperty (editorHeightProp, "811", nullptr);
    view.setProperty (metersCollapsedProp, "1", nullptr);
    CHECK (readEditorSize (root) == EditorSize { 1432, 811 });
    CHECK (readMetersCollapsed (root));
}
