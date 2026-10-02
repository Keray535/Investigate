// T-012 allocation check for SpectrumView::pollFrame (roadmap Budgets; task
// "no allocation in the vblank callback after the first frame").
// Only pollFrame is measurable: paint() goes through JUCE's rasteriser and
// text layout, which allocate internally, so paint is covered by the audit in
// docs/qa/T-012-unit.md instead.

#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "gui/DisplayContext.h"
#include "gui/InvestigateLookAndFeel.h"
#include "gui/SpectrumView.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace investigate;
using namespace investigate::test;

namespace
{
SpectrumFrame makeFrame()
{
    SpectrumFrame f;
    f.numPoints = 512;

    for (int i = 0; i < f.numPoints; ++i)
    {
        f.freqHz[(size_t) i] = 20.0f * std::pow (1000.0f, (float) i / (float) (f.numPoints - 1));
        f.primaryDb[(size_t) i] = -40.0f - 0.01f * (float) i;
    }

    return f;
}

void wiggleRange (InvestigateProcessor& p, int i)
{
    p.getApvts().getParameter (ParamIDs::dbLow)->setValueNotifyingHost ((i & 1) ? 0.2f : 0.3f);
}
}

TEST_CASE ("SpectrumView::pollFrame with no change allocates nothing", "[gui][alloc][spectrumview]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context (processor.getApvts());
    InvestigateLookAndFeel lookAndFeel;
    SpectrumView view (processor, context);
    view.setLookAndFeel (&lookAndFeel);
    view.setSize (900, 640);
    view.setFrameForTesting (makeFrame());
    view.pollFrame(); // the first poll settles the cached range and legend state

    ScopedAllocGuard guard;

    for (int i = 0; i < 300; ++i)
        view.pollFrame();

    CHECK (guard.count() == 0);
}

// T-012 amended criterion (2026-10-03): while hovering, at most 20 allocations
// per poll, only from rebuilding the readout strings. pollFrame never draws, so
// every counted allocation comes from Investigate code (updateHover), not from
// JUCE's text layout; the count is taken with the link-wide operator new hook.
TEST_CASE ("SpectrumView::pollFrame while hovering stays within 20 allocations per poll", "[gui][alloc][spectrumview]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context (processor.getApvts());
    InvestigateLookAndFeel lookAndFeel;
    SpectrumView view (processor, context);
    view.setLookAndFeel (&lookAndFeel);
    view.setSize (900, 640);
    view.setFrameForTesting (makeFrame());
    view.setHoverForTesting ({ 400.0f, 200.0f });
    view.pollFrame();

    constexpr int numPolls = 40;
    uint64_t total = 0;
    uint64_t worst = 0;

    for (int i = 0; i < numPolls; ++i)
    {
        wiggleRange (processor, i);
        context.refresh();
        ScopedAllocGuard guard;
        view.pollFrame();
        total += guard.count();
        worst = std::max (worst, guard.count());
    }

    INFO ("total " << total << " over " << numPolls << " polls, worst poll " << worst);
    CHECK ((double) total / numPolls <= 20.0);
    CHECK (worst <= 20);
}
