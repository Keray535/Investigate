// T-013 allocation checks for MeterPanel::update (roadmap Decisions
// 2026-10-03, task acceptance "Allocation"): 0 allocations in the bar,
// correlation and ballistics paths after the first frame, and at most 20 per
// text refresh tick when every cell changes.
//
// What is measured: update() on a panel inside a parent that is not on the
// desktop, so repaint() requests reach no peer and no JUCE rasteriser or text
// layout runs. Not measurable here: paint() (JUCE's renderer and text layout
// allocate internally; covered by the code audit in docs/qa/T-013-unit.md).

#include "AllocGuard.h"
#include "PluginProcessor.h"
#include "gui/DisplayContext.h"
#include "gui/InvestigateLookAndFeel.h"
#include "gui/MeterPanel.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace investigate;
using namespace investigate::test;

namespace
{
struct AllocRig
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context { processor.getApvts() };
    InvestigateLookAndFeel laf;
    juce::Component parent;
    MeterPanel panel { processor, context };
    MeterSnapshot snap;
    double now = 1000.0;
    uint64_t counter = 0;
    double lastRefresh = -1.0e12;   // replicates the panel's 100 ms text clock
    bool lastWasRefresh = false;

    AllocRig()
    {
        parent.setLookAndFeel (&laf);
        panel.setLookAndFeel (&laf);
        parent.addAndMakeVisible (panel);
        parent.setSize (400, 800);
        panel.setBounds (0, 0, 296, 656);
    }

    ~AllocRig()
    {
        panel.setLookAndFeel (nullptr);
        parent.setLookAndFeel (nullptr);
    }

    /** Allocations made by one update() call. */
    uint64_t step (double dtMs = 1000.0 / 60.0, MeterPanel::UpdateHints hints = {})
    {
        snap.blockCounter = ++counter;
        now += dtMs;
        lastWasRefresh = now - lastRefresh >= 100.0;

        if (lastWasRefresh)
            lastRefresh = now;

        ScopedAllocGuard guard;
        panel.update (snap, now, hints);
        return guard.count();
    }
};

MeterSnapshot activeSnapshot()
{
    MeterSnapshot s;
    s.samplePeak = { -6.0f, -9.0f };
    s.peakMax = { -6.0f, -9.0f };
    s.rms = { -12.0f, -14.0f };
    s.truePeak = { -5.0f, -8.0f };
    s.truePeakMax = { -5.0f, -8.0f };
    s.momentary = -20.0f;
    s.shortTerm = -21.0f;
    s.integrated = -22.0f;
    s.maxMomentary = -18.0f;
    s.maxShortTerm = -19.0f;
    s.loudnessRange = 3.0f;
    s.correlation = 0.4f;
    s.balance = -2.0f;
    return s;
}
}

TEST_CASE ("MeterPanel::update with unchanged data allocates nothing", "[gui][meter][alloc]")
{
    AllocRig rig;
    rig.snap = activeSnapshot();

    for (int i = 0; i < 60; ++i)
        rig.step();                                        // first frames, the correlation filter converges

    uint64_t worst = 0;

    for (int i = 0; i < 600; ++i)                          // 10 s with refresh ticks
        worst = std::max (worst, rig.step());

    CHECK (worst == 0);
}

TEST_CASE ("MeterPanel::update with moving bars, correlation and ballistics allocates nothing between text refreshes", "[gui][meter][alloc]")
{
    AllocRig rig;
    rig.snap = activeSnapshot();

    for (int i = 0; i < 30; ++i)
        rig.step();

    // The first update after the warm-up refreshes the text; the rest of the
    // 100 ms interval must be allocation free even though bars move. Counting
    // refresh frames replicates the panel's own clock (100 ms, double maths).
    uint64_t worstOther = 0;
    uint64_t worstRefresh = 0;
    int otherFrames = 0;

    for (int i = 0; i < 900; ++i)
    {
        const float phase = (float) i * 0.05f;
        rig.snap.samplePeak = { -20.0f + 15.0f * std::sin (phase), -25.0f + 12.0f * std::sin (phase * 1.3f) };
        rig.snap.rms = { rig.snap.samplePeak[0] - 8.0f, rig.snap.samplePeak[1] - 8.0f };
        rig.snap.truePeak = { rig.snap.samplePeak[0] + 1.0f, rig.snap.samplePeak[1] + 1.0f };
        rig.snap.correlation = std::sin (phase * 0.7f);
        rig.snap.balance = 3.0f * std::sin (phase * 0.3f);

        const auto n = rig.step();

        if (rig.lastWasRefresh)
            worstRefresh = std::max (worstRefresh, n);
        else
        {
            worstOther = std::max (worstOther, n);
            ++otherFrames;
        }
    }

    INFO ("worst allocations on a non-refresh frame " << worstOther << " (" << otherFrames << " frames); worst on a refresh frame " << worstRefresh);
    CHECK (otherFrames > 500);
    CHECK (worstOther == 0);
    CHECK (worstRefresh <= 20);
}

TEST_CASE ("MeterPanel::update: falling bars after the signal stops allocate nothing", "[gui][meter][alloc]")
{
    AllocRig rig;
    rig.snap = activeSnapshot();

    for (int i = 0; i < 30; ++i)
        rig.step();

    rig.snap.samplePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };
    rig.snap.truePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };
    rig.snap.rms = { MeterSnapshot::negInf, MeterSnapshot::negInf };
    rig.snap.momentary = rig.snap.shortTerm = MeterSnapshot::negInf;

    // Bars fall for ~3 s of hold and fall; then silence is steady.
    uint64_t worstOther = 0;

    for (int i = 0; i < 600; ++i)
    {
        const auto n = rig.step();

        if (! rig.lastWasRefresh)
            worstOther = std::max (worstOther, n);
    }

    CHECK (worstOther == 0);
}

TEST_CASE ("MeterPanel::update: a refresh tick where every cell changes stays within 20 allocations", "[gui][meter][alloc]")
{
    AllocRig rig;
    rig.snap = activeSnapshot();

    for (int i = 0; i < 30; ++i)
        rig.step();

    int checked = 0;
    uint64_t worst = 0;
    uint64_t total = 0;

    for (int tick = 0; tick < 20; ++tick)
    {
        // Change every displayed value to something with different text.
        const float a = -10.0f - 1.7f * (float) (tick + 1);
        rig.snap.samplePeak = { a + 4.0f, a + 3.0f };
        rig.snap.peakMax = { a + 5.0f, a + 4.0f };
        rig.snap.rms = { a - 5.0f, a - 6.0f };
        rig.snap.truePeak = { a + 6.0f, a + 5.0f };
        rig.snap.truePeakMax = { a + 7.0f, a + 8.0f };
        rig.snap.sampleClipCount = { (uint64_t) (tick + 1) * 7, (uint64_t) (tick + 1) * 3 };
        rig.snap.truePeakClipCount = { (uint64_t) (tick + 1) * 5, (uint64_t) (tick + 1) * 11 };
        rig.snap.momentary = a;
        rig.snap.shortTerm = a - 1.1f;
        rig.snap.integrated = a - 2.2f;
        rig.snap.maxMomentary = a + 3.3f;
        rig.snap.maxShortTerm = a + 2.2f;
        rig.snap.loudnessRange = 1.0f + 0.7f * (float) tick;
        rig.snap.correlation = (tick % 2 == 0) ? 0.9f : -0.9f;
        rig.snap.balance = (tick % 2 == 0) ? -1.3f - (float) tick * 0.1f : 2.1f + (float) tick * 0.1f;

        // Run until the refresh, then take the refresh frame's count. The
        // correlation text follows the display filter, so the value settles
        // over a few frames; stepping to the next 100 ms mark is enough.
        uint64_t frameWorst = 0;

        for (int f = 0; f < 6; ++f)
            frameWorst = std::max (frameWorst, rig.step (20.0));

        worst = std::max (worst, frameWorst);
        total += frameWorst;
        ++checked;
    }

    INFO ("worst refresh frame " << worst << ", mean " << (double) total / (double) std::max (1, checked));
    CHECK (worst <= 20);
}

TEST_CASE ("MeterPanel::update: switching mono and scale allocates within the text budget", "[gui][meter][alloc]")
{
    AllocRig rig;
    rig.snap = activeSnapshot();

    for (int i = 0; i < 30; ++i)
        rig.step();

    uint64_t worst = 0;

    for (int i = 0; i < 10; ++i)
    {
        rig.snap.isMono = (i % 2) == 0;
        worst = std::max (worst, rig.step (20.0));
    }

    INFO ("worst frame " << worst);
    CHECK (worst <= 20);
}

TEST_CASE ("MeterPanel::update: forced refreshes after resetHeldValues stay within 20 allocations each", "[gui][meter][alloc]")
{
    AllocRig rig;
    rig.snap = activeSnapshot();

    for (int i = 0; i < 30; ++i)
        rig.step();

    // The force window runs on the real clock, so continue from it.
    rig.now = juce::Time::getMillisecondCounterHiRes();
    rig.panel.resetHeldValues();
    const double t0 = rig.now;
    uint64_t worst = 0;

    for (int i = 1; i <= 10; ++i)
    {
        const float a = -10.0f - 1.9f * (float) i;
        rig.snap.samplePeak = { a + 4.0f, a + 3.0f };
        rig.snap.peakMax = { a + 5.0f, a + 4.0f };
        rig.snap.rms = { a - 5.0f, a - 6.0f };
        rig.snap.truePeak = { a + 6.0f, a + 5.0f };
        rig.snap.truePeakMax = { a + 7.0f, a + 8.0f };
        rig.snap.sampleClipCount = { (uint64_t) i * 7, (uint64_t) i * 3 };
        rig.snap.truePeakClipCount = { (uint64_t) i * 5, (uint64_t) i * 11 };
        rig.snap.momentary = a;
        rig.snap.shortTerm = a - 1.1f;
        rig.snap.integrated = a - 2.2f;
        rig.snap.maxMomentary = a + 3.3f;
        rig.snap.maxShortTerm = a + 2.2f;
        rig.snap.loudnessRange = 1.0f + 0.7f * (float) i;
        rig.snap.correlation = (i % 2 == 0) ? 0.9f : -0.9f;
        rig.snap.balance = (i % 2 == 0) ? -1.3f - (float) i * 0.1f : 2.1f + (float) i * 0.1f;

        rig.snap.blockCounter = ++rig.counter;
        rig.now = t0 + (double) i;   // 1 ms apart, inside the 250 ms window
        ScopedAllocGuard guard;
        rig.panel.update (rig.snap, rig.now, {});
        worst = std::max (worst, guard.count());
    }

    INFO ("worst forced refresh " << worst);
    CHECK (worst <= 20);
}
