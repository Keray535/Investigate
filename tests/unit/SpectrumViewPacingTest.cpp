// T-029 frame pacing through the view. Repaints are observed with a parent
// component on the desktop whose paintOverChildren counts the paints that
// reach the view's area (SpectrumView is final and its paint cannot be
// wrapped). The only waits are short message-loop pumps.

#include "PluginProcessor.h"
#include "gui/DisplayContext.h"
#include "gui/InvestigateLookAndFeel.h"
#include "gui/SpectrumView.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

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

struct Probe final : juce::Component
{
    void paintOverChildren (juce::Graphics&) override { ++paints; }
    int paints = 0;
};

// JUCE_MODAL_LOOPS_PERMITTED is off, so drain the thread's Win32 message queue
// (the peer's repaint timer and window messages) for a fixed time.
void pump (int ms)
{
    const auto end = juce::Time::getMillisecondCounterHiRes() + (double) ms;

    while (juce::Time::getMillisecondCounterHiRes() < end)
    {
        MSG msg;

        while (PeekMessageW (&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage (&msg);
            DispatchMessageW (&msg);
        }

        juce::Thread::sleep (1);
    }
}
}

TEST_CASE ("SpectrumView pacing: a paced second frame is deferred and issued by the next poll after 30 ms", "[gui][spectrumview][pacing][window]")
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context (processor.getApvts());
    InvestigateLookAndFeel lookAndFeel;
    SpectrumView view (processor, context);
    view.setLookAndFeel (&lookAndFeel);

    Probe probe;
    probe.setOpaque (true);
    probe.addAndMakeVisible (view);
    probe.setBounds (0, 0, 2600, 1500);   // data rect above 2.0 Mpx: paced
    view.setBounds (probe.getLocalBounds());
    probe.addToDesktop (0);
    probe.setVisible (true);

    view.setFrameForTesting (makeFrame());
    view.pollFrame();
    pump (300);

    if (probe.getPeer() == nullptr || probe.paints == 0)
        SKIP ("no native window / paint available on this machine");

    const int base = probe.paints;

    SECTION ("two frames inside 30 ms: one immediate paint, the second only after the interval")
    {
        juce::Thread::sleep (40);              // leave the previous interval
        view.setFrameForTesting (makeFrame()); // immediate
        view.setFrameForTesting (makeFrame()); // deferred
        pump (25);
        CHECK (probe.paints - base == 1);

        pump (25);
        CHECK (probe.paints - base == 1);      // nothing is issued without a poll

        juce::Thread::sleep (40);
        view.pollFrame();                      // issues the pending repaint
        pump (40);
        CHECK (probe.paints - base == 2);

        view.pollFrame();                      // nothing pending now
        pump (40);
        CHECK (probe.paints - base == 2);
    }

    SECTION ("a range change repaints at once even while a frame repaint is pending")
    {
        juce::Thread::sleep (40);
        view.setFrameForTesting (makeFrame());
        view.setFrameForTesting (makeFrame()); // pending
        pump (25);
        const int afterFrames = probe.paints;

        processor.getApvts().getParameter ("dbLow")->setValueNotifyingHost (0.25f);
        context.refresh();
        view.pollFrame();
        pump (25);
        CHECK (probe.paints - afterFrames == 1);

        // the pending flag was cleared by that repaint
        juce::Thread::sleep (40);
        view.pollFrame();
        pump (40);
        CHECK (probe.paints - afterFrames == 1);
    }
}
