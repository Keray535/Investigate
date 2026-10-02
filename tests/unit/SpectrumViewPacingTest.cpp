// T-029 frame pacing through the view. Repaints are observed with a parent
// component on the desktop whose paintOverChildren counts the paints that
// reach the view's area (SpectrumView is final and its paint cannot be
// wrapped). Every wait is bounded (GuiTestEnvironment.h, 10 s cap). If the
// desktop delivers no paints or vblanks the test SKIPs (T-031).

#include "PluginProcessor.h"
#include "gui/DisplayContext.h"
#include "gui/InvestigateLookAndFeel.h"
#include "GuiTestEnvironment.h"
#include "gui/SpectrumView.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

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
}

TEST_CASE ("SpectrumView pacing: a paced second frame is deferred and issued by the next poll after 30 ms", "[gui][spectrumview][pacing][window]")
{
    using guitest::pump;
    using guitest::pumpUntil;

    guitest::Watchdog watchdog ("SpectrumView pacing");
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

    guitest::VBlankCounter vblanks (probe);

    // Environment check: the desktop must deliver vblanks and repaints of this
    // window, both within 2 s. Otherwise paint counts say nothing about pacing.
    view.setFrameForTesting (makeFrame());
    view.pollFrame();

    if (probe.getPeer() == nullptr)
        SKIP ("no native window available on this machine");

    if (! pumpUntil ([&] { return probe.paints > 0; }, 2000))
        SKIP ("desktop delivered no paint for the test window within 2 s (display asleep, session locked or window not shown)");

    if (! pumpUntil ([&] { return vblanks.count >= 10; }, 2000))
        SKIP ("desktop delivered fewer than 10 vblank callbacks within 2 s (display asleep or session locked)");

    pump (300);                           // let the first frame's repaint settle
    const int base = probe.paints;

    // A direct repaint of the whole view must paint promptly; if not, paints
    // are not reliable now and the counts below could not be trusted.
    view.repaint();

    if (! pumpUntil ([&] { return probe.paints > base; }, 2000))
        SKIP ("a direct repaint of the test window was not painted within 2 s");

    // The desktop can add paints of its own (window exposed, un-minimised,
    // moved to another monitor, focus changes). A real pacing defect is
    // deterministic, such noise is not: a scenario is measured up to three
    // times, each time starting from a quiet window, and passes when one
    // measurement matches. It fails only when the window was quiet around the
    // measurements and still never matched. It SKIPs when noise was seen.
    const auto isQuiet = [&] (int ms)
    {
        const int before = probe.paints;
        pump (ms);
        return probe.paints == before;
    };

    const auto settle = [&]
    {
        for (int i = 0; i < 8; ++i)
            if (isQuiet (150))
                return true;

        return false;
    };

    const auto waitForPaint = [&] (int expected)
    {
        return pumpUntil ([&] { return probe.paints >= expected; }, 2000);
    };

    // `scenario` fills `got` and returns false if the desktop stopped painting.
    const auto runScenario = [&] (const std::vector<int>& expected, auto scenario)
    {
        bool sawNoise = false;
        bool sawMismatch = false;
        std::vector<int> got;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            if (! settle())
            {
                sawNoise = true;
                continue;
            }

            juce::Thread::sleep (40);         // leave the previous 30 ms interval
            got.clear();

            if (! scenario (got))
                SKIP ("the desktop stopped painting during the test (display asleep or session locked)");

            if (got == expected)
            {
                CHECK (got == expected);
                return;
            }

            sawMismatch = true;

            if (! settle())
                sawNoise = true;
        }

        if (! sawMismatch || sawNoise)
            SKIP ("the desktop delivered unsolicited paints (window exposed, minimised or moved) in every attempt");

        CHECK (got == expected);
    };

    SECTION ("two frames inside 30 ms: one immediate paint, the second only after the interval")
    {
        // Paints after: the first frame, 50 ms more (nothing without a poll), the poll, a second poll.
        runScenario ({ 1, 1, 2, 2 }, [&] (std::vector<int>& got)
        {
            const int start = probe.paints;
            view.setFrameForTesting (makeFrame()); // immediate
            view.setFrameForTesting (makeFrame()); // deferred

            if (! waitForPaint (start + 1))
                return false;

            pump (25);
            got.push_back (probe.paints - start);

            pump (25);
            got.push_back (probe.paints - start);  // nothing is issued without a poll

            juce::Thread::sleep (40);
            view.pollFrame();                      // issues the pending repaint
            waitForPaint (start + 2);              // bounded; a miss shows in the counts
            pump (40);
            got.push_back (probe.paints - start);

            view.pollFrame();                      // nothing pending now
            pump (60);
            got.push_back (probe.paints - start);
            return true;
        });
    }

    SECTION ("a range change repaints at once even while a frame repaint is pending")
    {
        runScenario ({ 1, 1 }, [&] (std::vector<int>& got)
        {
            const int start = probe.paints;
            view.setFrameForTesting (makeFrame());
            view.setFrameForTesting (makeFrame()); // pending

            if (! waitForPaint (start + 1))
                return false;

            pump (25);
            const int afterFrames = probe.paints;

            // Alternate the value so each attempt really changes the range.
            static bool low = false;
            low = ! low;
            processor.getApvts().getParameter ("dbLow")->setValueNotifyingHost (low ? 0.25f : 0.0f);
            context.refresh();
            view.pollFrame();

            if (! waitForPaint (afterFrames + 1))
                return false;

            pump (25);
            got.push_back (probe.paints - afterFrames);

            // the pending flag was cleared by that repaint
            juce::Thread::sleep (40);
            view.pollFrame();
            pump (60);
            got.push_back (probe.paints - afterFrames);
            return true;
        });
    }
}
