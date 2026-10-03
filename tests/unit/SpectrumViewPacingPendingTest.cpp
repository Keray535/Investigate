// T-013 carry-over 3: the SpectrumView pacing deferral observed through
// isFrameRepaintPendingForTesting(), with no desktop window. Complements the
// window-based test in SpectrumViewPacingTest.cpp. The only clock is JUCE's
// millisecond timer, as in the production code; the checks that depend on
// "inside 30 ms" are retried when the machine stalled between two calls.

#include "PluginProcessor.h"
#include "gui/DisplayContext.h"
#include "gui/InvestigateLookAndFeel.h"
#include "gui/SpectrumView.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace investigate;

namespace
{
SpectrumFrame makeFrame()
{
    SpectrumFrame f;
    f.numPoints = 256;

    for (int i = 0; i < f.numPoints; ++i)
    {
        f.freqHz[(size_t) i] = 20.0f * std::pow (1000.0f, (float) i / (float) (f.numPoints - 1));
        f.primaryDb[(size_t) i] = -40.0f - 0.05f * (float) i;
    }

    return f;
}
}

TEST_CASE ("SpectrumView pacing (no window): two frames inside 30 ms leave a repaint pending, the poll after 30 ms issues it", "[gui][spectrumview][pacing]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context (processor.getApvts());
    InvestigateLookAndFeel lookAndFeel;
    SpectrumView view (processor, context);
    view.setLookAndFeel (&lookAndFeel);
    view.setSize (2600, 1500);                  // data rect above 2.0 Mpx: paced

    bool verified = false;

    for (int attempt = 0; attempt < 5 && ! verified; ++attempt)
    {
        juce::Thread::sleep (40);               // leave any previous 30 ms interval
        const double start = juce::Time::getMillisecondCounterHiRes();

        view.setFrameForTesting (makeFrame());  // issues at once
        const bool firstPending = view.isFrameRepaintPendingForTesting();
        view.setFrameForTesting (makeFrame());  // inside 30 ms: deferred
        const bool secondPending = view.isFrameRepaintPendingForTesting();
        view.pollFrame();                       // still inside 30 ms: stays pending
        const bool stillPending = view.isFrameRepaintPendingForTesting();
        const double elapsed = juce::Time::getMillisecondCounterHiRes() - start;

        if (elapsed > 25.0)
            continue;                           // the machine stalled: not a valid observation

        verified = true;
        CHECK (view.isFramePaced());
        CHECK_FALSE (firstPending);
        CHECK (secondPending);
        CHECK (stillPending);
        CHECK_FALSE (view.requestedRepaintInLastPoll());

        juce::Thread::sleep (35);
        view.pollFrame();                       // the next poll after 30 ms issues it
        CHECK_FALSE (view.isFrameRepaintPendingForTesting());
        CHECK (view.requestedRepaintInLastPoll());

        view.pollFrame();                       // nothing new: nothing requested
        CHECK_FALSE (view.requestedRepaintInLastPoll());
        CHECK_FALSE (view.isFrameRepaintPendingForTesting());
    }

    if (! verified)
        SKIP ("the machine stalled for more than 25 ms in every attempt");
}

TEST_CASE ("SpectrumView pacing (no window): an unpaced size never defers", "[gui][spectrumview][pacing]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context (processor.getApvts());
    InvestigateLookAndFeel lookAndFeel;
    SpectrumView view (processor, context);
    view.setLookAndFeel (&lookAndFeel);
    view.setSize (900, 640);

    view.setFrameForTesting (makeFrame());
    view.setFrameForTesting (makeFrame());
    view.setFrameForTesting (makeFrame());

    CHECK_FALSE (view.isFramePaced());
    CHECK_FALSE (view.isFrameRepaintPendingForTesting());
}
