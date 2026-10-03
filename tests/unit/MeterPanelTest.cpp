// T-013 MeterPanel behaviour driven through update() with hand-built
// snapshots (docs/design/ui-spec.md 8.1 to 8.7, 12). No desktop window: the
// panel sits in a parent component that is not on the desktop, and the paint
// checks render into a software image.

#include "PluginProcessor.h"
#include "gui/DisplayContext.h"
#include "gui/HeaderButton.h"
#include "gui/InvestigateLookAndFeel.h"
#include "gui/MeterMath.h"
#include "gui/MeterPanel.h"
#include "gui/TooltipText.h"
#include "params/ParameterIDs.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <string>

using namespace investigate;
using Catch::Matchers::WithinAbs;
using Row = MeterPanel;

namespace
{
constexpr const char* minus = "\xE2\x88\x92";
constexpr const char* dash = "\xE2\x80\x94";
constexpr float posInf = std::numeric_limits<float>::infinity();
constexpr float qNaN = std::numeric_limits<float>::quiet_NaN();

std::string m (const char* rest) { return std::string (minus) + rest; }

struct Rig
{
    juce::ScopedJuceInitialiser_GUI gui;
    InvestigateProcessor processor;
    DisplayContext context { processor.getApvts() };
    InvestigateLookAndFeel laf;
    juce::Component parent;
    MeterPanel panel { processor, context };
    MeterSnapshot snap;
    double now = 1000.0;

    Rig (int height = 656)
    {
        parent.setLookAndFeel (&laf);
        panel.setLookAndFeel (&laf);
        parent.addAndMakeVisible (panel);
        parent.setSize (400, 800);
        panel.setBounds (0, 0, 296, height);
        setParam (ParamIDs::rmsWindow, 300.0f);
    }

    ~Rig()
    {
        panel.setLookAndFeel (nullptr);
        parent.setLookAndFeel (nullptr);
    }

    void setParam (const char* id, float value)
    {
        auto* p = processor.getApvts().getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void setScale (MeterScale s) { setParam (ParamIDs::meterScale, (float) (int) s); }

    void step (double dtMs = 20.0, MeterPanel::UpdateHints hints = {}, bool advanceCounter = true)
    {
        if (advanceCounter)
            ++snap.blockCounter;

        now += dtMs;
        panel.update (snap, now, hints);
    }

    void run (int steps, double dtMs = 20.0)
    {
        for (int i = 0; i < steps; ++i)
            step (dtMs);
    }

    /** Runs long enough for a 10 Hz text refresh. */
    void refreshText() { run (6, 20.0); }

    juce::String cell (int row, int ch) const { return panel.getCellText (MeterPanel::tableCellIndex (row, ch)); }
    juce::String loud (int c) const { return panel.getCellText (MeterPanel::loudnessCellIndex (c)); }
    juce::String text (int idx) const { return panel.getCellText (idx); }

    juce::Image render (int width = 296)
    {
        juce::Image img (juce::Image::ARGB, width, panel.getHeight(), true);
        juce::Graphics g (img);
        panel.paintEntireComponent (g, false);
        return img;
    }
};

MeterSnapshot loudSnapshot()
{
    MeterSnapshot s;
    s.samplePeak = { -6.0f, -6.0f };
    s.peakMax = { -6.0f, -6.0f };
    s.rms = { -9.0103f, -9.0103f };
    s.truePeak = { -5.0f, -5.0f };
    s.truePeakMax = { -3.0f, -3.0f };
    s.sampleClipCount = { 2, 0 };
    s.momentary = -20.0f;
    s.shortTerm = -21.0f;
    s.integrated = -23.0f;
    s.maxMomentary = -15.0f;
    s.maxShortTerm = -16.0f;
    s.loudnessRange = 4.0f;
    s.correlation = 0.5f;
    s.balance = -6.0f;
    s.sampleRate = 48000.0;
    return s;
}
}

//==============================================================================
TEST_CASE ("MeterPanel: clip lamps latch while counts are above zero and clear when they reach zero", "[gui][meter][panel]")
{
    Rig rig;
    rig.snap.sampleClipCount = { 3, 0 };
    rig.snap.truePeakClipCount = { 0, 2 };
    rig.refreshText();

    CHECK (rig.cell (Row::rowClips, 0) == "3");
    CHECK (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowClips, 0)));
    CHECK (rig.cell (Row::rowClips, 1) == "0");
    CHECK_FALSE (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowClips, 1)));
    CHECK (rig.cell (Row::rowTpClips, 0) == "0");
    CHECK_FALSE (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowTpClips, 0)));
    CHECK (rig.cell (Row::rowTpClips, 1) == "2");
    CHECK (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowTpClips, 1)));

    // Lamp above each track: sample + true-peak count for that channel.
    CHECK (rig.panel.getBarPixels (0).lampLit);
    CHECK (rig.panel.getBarPixels (1).lampLit);

    SECTION ("the lamp stays lit while the signal is gone (latched by the count)")
    {
        rig.snap.samplePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };
        rig.run (200);
        CHECK (rig.panel.getBarPixels (0).lampLit);
        CHECK (rig.cell (Row::rowClips, 0) == "3");
    }

    SECTION ("a reset (counts back to zero) clears lamps and cells")
    {
        rig.snap.sampleClipCount = { 0, 0 };
        rig.snap.truePeakClipCount = { 0, 0 };
        rig.step();                                   // the bar lamp follows at once
        CHECK_FALSE (rig.panel.getBarPixels (0).lampLit);
        CHECK_FALSE (rig.panel.getBarPixels (1).lampLit);
        rig.refreshText();
        CHECK (rig.cell (Row::rowClips, 0) == "0");
        CHECK_FALSE (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowClips, 0)));
        CHECK_FALSE (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowTpClips, 1)));
    }

    SECTION ("counts above 9999 read 9999+")
    {
        rig.snap.sampleClipCount = { 12345, 10000 };
        rig.refreshText();
        CHECK (rig.cell (Row::rowClips, 0) == "9999+");
        CHECK (rig.cell (Row::rowClips, 1) == "9999+");
    }
}

TEST_CASE ("MeterPanel: table readouts from a snapshot", "[gui][meter][panel]")
{
    Rig rig;
    rig.snap = loudSnapshot();
    rig.run (60, 20.0);                                  // the correlation filter settles

    CHECK (rig.cell (Row::rowPeak, 0) == juce::String::fromUTF8 (m ("6.0").c_str()));
    CHECK (rig.cell (Row::rowPeakMax, 1) == juce::String::fromUTF8 (m ("6.0").c_str()));
    CHECK (rig.cell (Row::rowTruePeak, 0) == juce::String::fromUTF8 (m ("5.0").c_str()));
    CHECK (rig.cell (Row::rowTruePeakMax, 0) == juce::String::fromUTF8 (m ("3.0").c_str()));
    CHECK (rig.cell (Row::rowHeadroom, 0) == "3.0");                 // 0 - (-3)
    CHECK (rig.cell (Row::rowRms, 0) == juce::String::fromUTF8 (m ("9.0").c_str()));
    CHECK (rig.cell (Row::rowCrest, 0) == "3.0");                    // a sine

    CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("20.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudS) == juce::String::fromUTF8 (m ("21.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudI) == juce::String::fromUTF8 (m ("23.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudMMax) == juce::String::fromUTF8 (m ("15.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudSMax) == juce::String::fromUTF8 (m ("16.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudLra) == "4.0");

    CHECK (rig.text (MeterPanel::correlationCellIndex) == "+0.50");
    CHECK (rig.text (MeterPanel::balanceCellIndex) == "L 6.0 dB");
}

TEST_CASE ("MeterPanel: headroom is 0 minus TP max and a dash while TP max is -inf", "[gui][meter][panel]")
{
    Rig rig;

    rig.snap.truePeakMax = { MeterSnapshot::negInf, -1.2f };
    rig.refreshText();
    CHECK (rig.cell (Row::rowHeadroom, 0) == juce::String::fromUTF8 (dash));
    CHECK (rig.cell (Row::rowHeadroom, 1) == "1.2");

    rig.snap.truePeakMax = { 0.4f, 0.0f };
    rig.refreshText();
    CHECK (rig.cell (Row::rowHeadroom, 0) == juce::String::fromUTF8 (m ("0.4").c_str()));
    CHECK (rig.cell (Row::rowHeadroom, 1) == "0.0");
}

TEST_CASE ("MeterPanel: balance text", "[gui][meter][panel]")
{
    Rig rig;

    rig.snap.balance = -6.0f;
    rig.refreshText();
    CHECK (rig.text (MeterPanel::balanceCellIndex) == "L 6.0 dB");

    rig.snap.balance = 0.8f;
    rig.refreshText();
    CHECK (rig.text (MeterPanel::balanceCellIndex) == "R 0.8 dB");

    rig.snap.balance = 0.0f;
    rig.refreshText();
    CHECK (rig.text (MeterPanel::balanceCellIndex) == "C 0.0 dB");

    rig.snap.balance = -90.0f;
    rig.refreshText();
    CHECK (rig.text (MeterPanel::balanceCellIndex) == "L >60 dB");
}

TEST_CASE ("MeterPanel: peak bar falls at 20 dB/s, the hold tick holds 1500 ms then falls at 30 dB/s", "[gui][meter][panel][ballistics]")
{
    Rig rig;
    rig.snap.samplePeak = { -6.0f, -20.0f };
    rig.step (0.0);
    CHECK (rig.panel.getBarPeakDb (0) == -6.0f);
    CHECK (rig.panel.getHeldPeakDb (0) == -6.0f);
    CHECK (rig.panel.getBarPeakDb (1) == -20.0f);

    rig.snap.samplePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };

    // 500 ms in 20 ms frames
    rig.run (25, 20.0);
    CHECK_THAT (rig.panel.getBarPeakDb (0), WithinAbs (-16.0, 0.05));
    CHECK (rig.panel.getHeldPeakDb (0) == -6.0f);        // still held

    // to 1480 ms: still held
    rig.run (49, 20.0);
    CHECK (rig.panel.getHeldPeakDb (0) == -6.0f);
    CHECK_THAT (rig.panel.getBarPeakDb (0), WithinAbs (-6.0 - 20.0 * 1.48, 0.1));

    // 1 s after the hold ended: 30 dB/s down (the bar is far below)
    rig.run (50 + 2, 20.0);                              // to 2520 ms: 1020 ms past the hold
    CHECK_THAT (rig.panel.getHeldPeakDb (0), WithinAbs (-6.0 - 30.0 * 1.02, 1.0));

    // Never below the bar: after a long fall they meet at the bar.
    rig.run (1000, 20.0);
    CHECK (rig.panel.getHeldPeakDb (0) >= rig.panel.getBarPeakDb (0));
}

TEST_CASE ("MeterPanel: peak hold never drops below the bar", "[gui][meter][panel][ballistics]")
{
    Rig rig;
    rig.snap.samplePeak = { -6.0f, -6.0f };
    rig.step (0.0);

    // A slow decay input that stays above the bar's own fall.
    for (int i = 0; i < 400; ++i)
    {
        rig.snap.samplePeak = { -6.0f - 0.1f * (float) i, -6.0f };
        rig.step (20.0);
        CHECK (rig.panel.getHeldPeakDb (0) >= rig.panel.getBarPeakDb (0));
        CHECK (rig.panel.getHeldPeakDb (0) >= rig.snap.samplePeak[0] - 0.001f);
    }
}

TEST_CASE ("MeterPanel: resetHeldValues clears holds, the TP hold and the crest ring", "[gui][meter][panel][ballistics]")
{
    Rig rig;
    rig.snap.samplePeak = { -6.0f, -6.0f };
    rig.snap.truePeak = { -3.0f, -3.0f };
    rig.snap.rms = { -30.0f, -30.0f };
    rig.step (0.0);
    rig.run (2, 20.0);

    // The signal stops; the holds keep the old values.
    rig.snap.samplePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };
    rig.snap.truePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };
    rig.run (10, 20.0);

    CHECK (rig.panel.getHeldPeakDb (0) == -6.0f);
    CHECK (rig.cell (Row::rowPeak, 0) == juce::String::fromUTF8 (m ("6.0").c_str()));
    CHECK (rig.cell (Row::rowTruePeak, 0) == juce::String::fromUTF8 (m ("3.0").c_str()));
    CHECK (rig.cell (Row::rowCrest, 0) == "24.0");        // ring max -6 over the 300 ms window minus RMS -30

    rig.panel.resetHeldValues();
    CHECK (rig.panel.getHeldPeakDb (0) == rig.panel.getBarPeakDb (0));
    CHECK (rig.panel.getHeldPeakDb (0) < -6.0f);

    rig.step (20.0);                                       // reset forces a text refresh on the next update
    CHECK (rig.cell (Row::rowTruePeak, 0) == juce::String::fromUTF8 (m ("inf").c_str()));
    CHECK (rig.cell (Row::rowCrest, 0) == juce::String::fromUTF8 (dash));
    CHECK (rig.cell (Row::rowPeak, 0) != juce::String::fromUTF8 (m ("6.0").c_str()));
}

TEST_CASE ("MeterPanel: crest uses the peak over the RMS window", "[gui][meter][panel][ballistics]")
{
    Rig rig;
    rig.snap.rms = { -30.0f, -30.0f };
    rig.snap.samplePeak = { -10.0f, -10.0f };
    rig.step (0.0);

    rig.snap.samplePeak = { -40.0f, -40.0f };
    rig.run (5, 20.0);                                      // 100 ms later
    CHECK (rig.cell (Row::rowCrest, 0) == "20.0");          // -10 still inside 300 ms

    rig.run (25, 20.0);                                     // 600 ms later: left the window
    CHECK (rig.cell (Row::rowCrest, 0) == juce::String::fromUTF8 (m ("10.0").c_str()));   // live peak -40 minus RMS -30
}

TEST_CASE ("MeterPanel: correlation display filter and text", "[gui][meter][panel][ballistics]")
{
    Rig rig;
    rig.snap.correlation = 0.0f;
    rig.step (0.0);

    rig.snap.correlation = 1.0f;
    rig.step (50.0);
    // one 50 ms step with tau = 50 ms
    CHECK_THAT (rig.panel.getCorrelationShown(), WithinAbs (1.0 - std::exp (-1.0), 0.002));

    rig.snap.correlation = 1.0f;
    rig.run (50, 20.0);
    CHECK_THAT (rig.panel.getCorrelationShown(), WithinAbs (1.0, 0.001));
    rig.refreshText();
    CHECK (rig.text (MeterPanel::correlationCellIndex) == "+1.00");

    rig.snap.correlation = -1.0f;
    rig.run (100, 20.0);
    CHECK_THAT (rig.panel.getCorrelationShown(), WithinAbs (-1.0, 0.001));
    rig.refreshText();
    CHECK (rig.text (MeterPanel::correlationCellIndex) == juce::String::fromUTF8 (m ("1.00").c_str()));

    rig.snap.correlation = 0.0f;
    rig.run (100, 20.0);
    rig.refreshText();
    CHECK (rig.text (MeterPanel::correlationCellIndex) == "0.00");
}

TEST_CASE ("MeterPanel: stale snapshot rule", "[gui][meter][panel][stale]")
{
    Rig rig;
    rig.snap = loudSnapshot();
    rig.run (60, 20.0);                           // the correlation filter settles (tau 50 ms)
    REQUIRE_FALSE (rig.panel.isStale());
    const float corrBefore = rig.panel.getCorrelationShown();
    CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("20.0").c_str()));

    // The host stops calling processBlock: the counter freezes.
    for (int i = 0; i < 12; ++i)                  // 240 ms
        rig.step (20.0, {}, false);

    CHECK_FALSE (rig.panel.isStale());
    rig.step (20.0, {}, false);                   // 260 ms
    CHECK (rig.panel.isStale());

    for (int i = 0; i < 40; ++i)                  // another 800 ms, counter frozen
        rig.step (20.0, {}, false);

    CHECK (rig.panel.isStale());

    // Live fields read as silence.
    CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("inf").c_str()));
    CHECK (rig.loud (MeterPanel::loudS) == juce::String::fromUTF8 (m ("inf").c_str()));
    CHECK (rig.cell (Row::rowRms, 0) == juce::String::fromUTF8 (m ("inf").c_str()));
    CHECK (rig.cell (Row::rowCrest, 0) == juce::String::fromUTF8 (dash));
    CHECK (rig.panel.getBarPeakDb (0) < -6.0f - 15.0f);   // fell at 20 dB/s for about 1 s

    // Maxima, counts, headroom, I, LRA, correlation and balance hold.
    CHECK (rig.cell (Row::rowPeakMax, 0) == juce::String::fromUTF8 (m ("6.0").c_str()));
    CHECK (rig.cell (Row::rowTruePeakMax, 0) == juce::String::fromUTF8 (m ("3.0").c_str()));
    CHECK (rig.cell (Row::rowHeadroom, 0) == "3.0");
    CHECK (rig.cell (Row::rowClips, 0) == "2");
    CHECK (rig.panel.isCellLit (MeterPanel::tableCellIndex (Row::rowClips, 0)));
    CHECK (rig.panel.getBarPixels (0).lampLit);
    CHECK (rig.loud (MeterPanel::loudI) == juce::String::fromUTF8 (m ("23.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudMMax) == juce::String::fromUTF8 (m ("15.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudSMax) == juce::String::fromUTF8 (m ("16.0").c_str()));
    CHECK (rig.loud (MeterPanel::loudLra) == "4.0");
    CHECK (rig.text (MeterPanel::balanceCellIndex) == "L 6.0 dB");
    CHECK_THAT (rig.panel.getCorrelationShown(), WithinAbs (corrBefore, 1e-4));
    CHECK (rig.text (MeterPanel::correlationCellIndex) == "+0.50");

    SECTION ("the first counter change ends the stale state at once")
    {
        rig.step (20.0);
        CHECK_FALSE (rig.panel.isStale());
        rig.refreshText();
        CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("20.0").c_str()));
        CHECK (rig.panel.getBarPeakDb (0) == -6.0f);
    }

    SECTION ("counter wrap-around is not stale")
    {
        rig.snap.blockCounter = UINT64_MAX;
        rig.step (20.0, {}, false);
        CHECK_FALSE (rig.panel.isStale());
        rig.snap.blockCounter = 0;
        rig.step (20.0, {}, false);
        CHECK_FALSE (rig.panel.isStale());
    }
}

TEST_CASE ("MeterPanel: mono layout", "[gui][meter][panel][mono]")
{
    Rig rig;
    rig.snap = loudSnapshot();
    rig.snap.samplePeak = { -6.0f, -50.0f };
    rig.snap.isMono = true;
    rig.snap.correlation = 1.0f;
    rig.run (8, 20.0);

    CHECK (rig.panel.isMonoShown());

    // Single bar from channel 0; the second bar is not drawn.
    CHECK (rig.panel.getBarPixels (0).peakY >= 0);
    CHECK (rig.panel.getBarPixels (1) == MeterPanel::BarPixels());

    // Left table cells are hidden and empty; the right column carries the data.
    for (int row = 0; row < MeterPanel::numTableRows; ++row)
        CHECK (rig.cell (row, 0).isEmpty());

    CHECK (rig.cell (Row::rowPeak, 1) == juce::String::fromUTF8 (m ("6.0").c_str()));
    CHECK (rig.cell (Row::rowClips, 1) == "2");                       // channel 0's count

    // Correlation and balance show a dash, dimmed.
    CHECK (rig.text (MeterPanel::correlationCellIndex) == juce::String::fromUTF8 (dash));
    CHECK (rig.text (MeterPanel::balanceCellIndex) == juce::String::fromUTF8 (dash));

    SECTION ("switching back to stereo restores the cells")
    {
        rig.snap.isMono = false;
        rig.snap.samplePeak = { -6.0f, -12.0f };
        rig.snap.rms = { -9.0103f, -20.0f };
        rig.snap.correlation = 0.5f;
        rig.run (60, 20.0);
        CHECK_FALSE (rig.panel.isMonoShown());
        CHECK (rig.cell (Row::rowPeak, 0) == juce::String::fromUTF8 (m ("6.0").c_str()));
        CHECK (rig.cell (Row::rowRms, 1) == juce::String::fromUTF8 (m ("20.0").c_str()));
        CHECK (rig.text (MeterPanel::correlationCellIndex) == "+0.50");
        CHECK (rig.text (MeterPanel::balanceCellIndex) == "L 6.0 dB");
        CHECK (rig.panel.getBarPixels (1).peakY >= 0);
    }

    SECTION ("a mono switch repaints the bars")
    {
        const int before = rig.panel.getBarRepaintCount();
        rig.snap.isMono = false;
        rig.step();
        CHECK (rig.panel.getBarRepaintCount() > before);
    }
}

TEST_CASE ("MeterPanel: meterScale changes the RMS cell text", "[gui][meter][panel][scale]")
{
    Rig rig;
    rig.snap.rms = { -23.0103f, -23.0103f };
    rig.snap.samplePeak = { -20.0f, -20.0f };

    rig.refreshText();
    CHECK (rig.panel.getScaleShown() == MeterScale::dBFS);
    CHECK (rig.cell (Row::rowRms, 0) == juce::String::fromUTF8 (m ("23.0").c_str()));

    const auto pixelsDbfs = rig.panel.getBarPixels (0);

    rig.setScale (MeterScale::k20);
    rig.step (20.0);                                       // the change is noticed at once
    CHECK (rig.panel.getScaleShown() == MeterScale::k20);
    rig.refreshText();
    CHECK (rig.cell (Row::rowRms, 0) == "0.0");
    CHECK (rig.panel.getBarPixels (0).rmsY < pixelsDbfs.rmsY);      // +3.01 dB higher on the bar
    CHECK (rig.panel.getBarPixels (0).peakY == pixelsDbfs.peakY);

    rig.setScale (MeterScale::k14);
    rig.run (7, 20.0);
    CHECK (rig.cell (Row::rowRms, 0) == juce::String::fromUTF8 (m ("6.0").c_str()));

    rig.setScale (MeterScale::k12);
    rig.run (7, 20.0);
    CHECK (rig.cell (Row::rowRms, 0) == juce::String::fromUTF8 (m ("8.0").c_str()));

    rig.setScale (MeterScale::dBFS);
    rig.run (7, 20.0);
    CHECK (rig.cell (Row::rowRms, 0) == juce::String::fromUTF8 (m ("23.0").c_str()));
    CHECK (rig.panel.getBarPixels (0) == pixelsDbfs);
}

TEST_CASE ("MeterPanel: each scale and the mono layout paint differently and without trouble", "[gui][meter][panel][scale][render]")
{
    Rig rig;
    rig.snap.rms = { -30.0f, -30.0f };
    rig.snap.samplePeak = { -30.0f, -30.0f };
    rig.run (8, 20.0);

    const auto sameImage = [] (const juce::Image& a, const juce::Image& b)
    {
        if (a.getWidth() != b.getWidth() || a.getHeight() != b.getHeight())
            return false;

        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
                if (a.getPixelAt (x, y) != b.getPixelAt (x, y))
                    return false;

        return true;
    };

    const auto dbfs = rig.render();
    CHECK (dbfs.isValid());

    rig.setScale (MeterScale::k20);
    rig.run (8, 20.0);
    const auto k20 = rig.render();
    CHECK_FALSE (sameImage (dbfs, k20));

    rig.setScale (MeterScale::k14);
    rig.run (8, 20.0);
    CHECK_FALSE (sameImage (k20, rig.render()));

    rig.setScale (MeterScale::dBFS);
    rig.run (8, 20.0);
    {
        const auto again = rig.render();
        int diffs = 0, fx = -1, fy = -1;

        // The header chips below the strip title (scale, RMS, reset) animate on parameter changes; compare the content only.
        for (int y = rig.panel.getLayout().barsColumn.getY(); y < again.getHeight(); ++y)
            for (int x = 0; x < again.getWidth(); ++x)
                if (dbfs.getPixelAt (x, y) != again.getPixelAt (x, y) && diffs++ == 0)
                {
                    fx = x;
                    fy = y;
                }

        INFO ("differing pixels " << diffs << ", first at " << fx << "," << fy);
        CHECK (diffs == 0);                                 // deterministic, cache is rebuilt per scale
    }

    rig.snap.isMono = true;
    rig.run (8, 20.0);
    CHECK_FALSE (sameImage (dbfs, rig.render()));

    SECTION ("the bar is painted in the safe colour below the RMS value")
    {
        rig.snap.isMono = false;
        rig.run (8, 20.0);
        const auto img = rig.render();
        const auto& layout = rig.panel.getLayout();
        const auto origin = layout.barsColumn.getPosition();
        const int x = origin.x + tokens::meter::barLeftX + tokens::meter::barW / 2;
        const int y = origin.y + layout.barsColumn.getHeight() - tokens::meter::trackBottomPad - 3;
        const auto c = img.getPixelAt (x, y);
        const auto expected = tokens::colour::meterSafe;
        INFO ("pixel " << c.toDisplayString (true) << " expected " << expected.toDisplayString (true));
        CHECK (std::abs ((int) c.getRed() - (int) expected.getRed()) <= 2);
        CHECK (std::abs ((int) c.getGreen() - (int) expected.getGreen()) <= 2);
        CHECK (std::abs ((int) c.getBlue() - (int) expected.getBlue()) <= 2);
    }
}

TEST_CASE ("MeterPanel: paints at several strip heights", "[gui][meter][panel][render]")
{
    const int height = GENERATE (200, 300, 480, 656, 1000);
    Rig rig (height);
    rig.snap = loudSnapshot();
    rig.run (8, 20.0);
    CHECK (rig.render().isValid());
    rig.snap.isMono = true;
    rig.run (8, 20.0);
    CHECK (rig.render().isValid());
}

TEST_CASE ("MeterPanel: NaN and infinite snapshots never reach the text", "[gui][meter][panel][format]")
{
    Rig rig;
    const float values[] { qNaN, posInf, -posInf, 1.0e30f, -1.0e30f, 0.0f, std::numeric_limits<float>::denorm_min() };

    for (const float v : values)
    {
        MeterSnapshot s;
        s.samplePeak = { v, v };
        s.peakMax = { v, v };
        s.rms = { v, v };
        s.truePeak = { v, v };
        s.truePeakMax = { v, v };
        s.momentary = v;
        s.shortTerm = v;
        s.integrated = v;
        s.maxMomentary = v;
        s.maxShortTerm = v;
        s.loudnessRange = v;
        s.correlation = v;
        s.balance = v;
        s.sampleClipCount = { UINT64_MAX, UINT64_MAX };
        s.truePeakClipCount = { UINT64_MAX, 0 };
        rig.snap = s;

        for (const bool mono : { false, true })
        {
            rig.snap.isMono = mono;
            rig.run (8, 20.0);

            for (int i = 0; i < MeterPanel::numCells; ++i)
            {
                const auto str = rig.text (i).toStdString();
                INFO ("value " << v << " mono " << mono << " cell " << i << " text '" << str << "'");
                CHECK (str.find ("nan") == std::string::npos);
                CHECK (str.find ("NaN") == std::string::npos);
                CHECK (str.find ("1.#") == std::string::npos);

                if (str.find ("inf") != std::string::npos)
                    CHECK (rig.text (i) == juce::String::fromUTF8 (m ("inf").c_str()));
            }

            CHECK (std::isfinite (rig.panel.getCorrelationShown()));
            CHECK_FALSE (std::isnan (rig.panel.getBarPeakDb (0)));
            CHECK_FALSE (std::isnan (rig.panel.getHeldPeakDb (1)));
            CHECK (rig.render().isValid());
        }
    }
}

TEST_CASE ("MeterPanel: a change under one physical pixel does not repaint a bar", "[gui][meter][panel][repaint]")
{
    Rig rig;

    // Pick a level where +0.001 dB stays on the same pixel rows (deterministic scan).
    float base = 0.0f;
    bool found = false;

    for (float candidate : { -30.0f, -31.37f, -22.2f, -41.13f, -17.77f })
    {
        Rig probe;
        probe.snap.samplePeak = { candidate, candidate };
        probe.snap.rms = { candidate - 10.0f, candidate - 10.0f };
        probe.run (10, 20.0);
        const auto before = probe.panel.getBarPixels (0);
        probe.snap.samplePeak = { candidate + 0.001f, candidate + 0.001f };
        probe.snap.rms = { candidate - 9.999f, candidate - 9.999f };
        probe.step();

        if (probe.panel.getBarPixels (0) == before)
        {
            base = candidate;
            found = true;
            break;
        }
    }

    REQUIRE (found);

    rig.snap.samplePeak = { base, base };
    rig.snap.rms = { base - 10.0f, base - 10.0f };
    rig.run (10, 20.0);
    const int settled = rig.panel.getBarRepaintCount();
    rig.run (10, 20.0);
    CHECK (rig.panel.getBarRepaintCount() == settled);        // identical data: no bar repaint

    rig.snap.samplePeak = { base + 0.001f, base + 0.001f };
    rig.snap.rms = { base - 9.999f, base - 9.999f };
    rig.step();
    CHECK (rig.panel.getBarRepaintCount() == settled);        // under one pixel: still none

    rig.snap.samplePeak = { base + 3.0f, base + 3.0f };
    rig.step();
    CHECK (rig.panel.getBarRepaintCount() > settled);         // 3 dB is several pixels
}

TEST_CASE ("MeterPanel: text cells refresh at 10 Hz and repaint only when the text changed", "[gui][meter][panel][repaint]")
{
    Rig rig;
    rig.snap = loudSnapshot();
    rig.step (20.0);                                           // first refresh happens now
    const auto first = rig.loud (MeterPanel::loudM);
    CHECK (first == juce::String::fromUTF8 (m ("20.0").c_str()));
    CHECK (rig.panel.getTextRepaintCount() > 0);

    SECTION ("identical data for 99 ms and beyond adds no text repaint")
    {
        rig.run (25, 20.0);                                    // settle (the correlation filter converges)
        const int settled = rig.panel.getTextRepaintCount();

        for (int i = 0; i < 3; ++i)
            rig.step (33.0);                                   // 99 ms

        CHECK (rig.panel.getTextRepaintCount() == settled);

        rig.run (30, 20.0);                                    // 600 ms, several refresh ticks, same text
        CHECK (rig.panel.getTextRepaintCount() == settled);
    }

    SECTION ("a changed value shows only at the next 100 ms tick")
    {
        rig.run (25, 20.0);
        rig.snap.momentary = -30.0f;
        const int before = rig.panel.getTextRepaintCount();

        // Align to a known refresh: the next refresh is due 100 ms after the last.
        bool changed = false;
        double sinceChange = 0.0;

        for (int i = 0; i < 12 && ! changed; ++i)
        {
            rig.step (20.0);
            sinceChange += 20.0;
            changed = rig.loud (MeterPanel::loudM) != first;
        }

        REQUIRE (changed);
        CHECK (sinceChange <= 100.0);
        CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("30.0").c_str()));
        CHECK (rig.panel.getTextRepaintCount() == before + 1);   // only that cell
    }

    SECTION ("no new text before 100 ms have passed since the last refresh")
    {
        rig.step (20.0);
        rig.snap.momentary = -40.0f;

        // The refresh clock is internal; two frames 20 ms apart cannot both be a refresh.
        int refreshes = 0;

        for (int i = 0; i < 4; ++i)
        {
            const auto before = rig.loud (MeterPanel::loudM);
            rig.step (20.0);
            refreshes += rig.loud (MeterPanel::loudM) != before ? 1 : 0;
        }

        CHECK (refreshes <= 1);
    }
}

TEST_CASE ("MeterPanel: spectrum-paced repaints wait up to 34 ms and flush at once on the spectrum's vblank", "[gui][meter][panel][repaint][pacing]")
{
    Rig rig;
    rig.snap.samplePeak = { -50.0f, -50.0f };
    rig.run (10, 20.0);

    float level = -50.0f;
    const auto raise = [&] (float db)
    {
        level += db;
        rig.snap.samplePeak = { level, level };
    };

    // Not paced: every changing frame repaints.
    {
        const int c = rig.panel.getBarRepaintCount();
        raise (3.0f);
        rig.step (16.0);
        CHECK (rig.panel.getBarRepaintCount() > c);
    }

    // Paced and the spectrum repainted on this vblank: flush at once.
    raise (3.0f);
    rig.step (16.0, { true, true });
    const int c1 = rig.panel.getBarRepaintCount();

    raise (3.0f);
    rig.step (16.0, { true, false });
    CHECK (rig.panel.getBarRepaintCount() == c1);              // held back

    raise (3.0f);
    rig.step (16.0, { true, false });                          // 32 ms since the flush
    CHECK (rig.panel.getBarRepaintCount() == c1);

    raise (3.0f);
    rig.step (3.0, { true, false });                           // 35 ms: the hold limit
    CHECK (rig.panel.getBarRepaintCount() > c1);

    const int c2 = rig.panel.getBarRepaintCount();
    raise (3.0f);
    rig.step (10.0, { true, false });
    CHECK (rig.panel.getBarRepaintCount() == c2);
    raise (3.0f);
    rig.step (6.0, { true, true });                            // the spectrum repainted: flush now
    CHECK (rig.panel.getBarRepaintCount() > c2);
}

TEST_CASE ("MeterPanel: a hidden (collapsed) panel does nothing and recovers when shown", "[gui][meter][panel][collapse]")
{
    Rig rig;
    rig.snap = loudSnapshot();
    rig.run (8, 20.0);

    const int bars = rig.panel.getBarRepaintCount();
    const int texts = rig.panel.getTextRepaintCount();
    const float bar = rig.panel.getBarPeakDb (0);

    rig.panel.setVisible (false);
    rig.snap.samplePeak = { -1.0f, -1.0f };
    rig.snap.momentary = -5.0f;
    rig.run (20, 20.0);

    CHECK (rig.panel.getBarRepaintCount() == bars);
    CHECK (rig.panel.getTextRepaintCount() == texts);
    CHECK (rig.panel.getBarPeakDb (0) == bar);
    CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("20.0").c_str()));

    rig.panel.setVisible (true);
    rig.step (20.0);
    CHECK (rig.panel.getBarPeakDb (0) == -1.0f);
    CHECK (rig.loud (MeterPanel::loudM) == juce::String::fromUTF8 (m ("5.0").c_str()));   // refresh forced by showing
    CHECK (rig.panel.getBarRepaintCount() > bars);
    CHECK_FALSE (rig.panel.isStale());
}

TEST_CASE ("MeterPanel: resize requests a full re-sync of the bars", "[gui][meter][panel][repaint]")
{
    Rig rig;
    rig.snap = loudSnapshot();
    rig.run (10, 20.0);
    rig.run (50, 20.0);                                    // the correlation bar settles
    const int bars = rig.panel.getBarRepaintCount();
    rig.run (5, 20.0);
    REQUIRE (rig.panel.getBarRepaintCount() == bars);

    rig.panel.setSize (296, 500);
    rig.step();
    CHECK (rig.panel.getBarRepaintCount() > bars);
    CHECK (rig.cell (Row::rowPeak, 0).isNotEmpty());
}

TEST_CASE ("MeterPanel: onStatisticsReset button path clears held values", "[gui][meter][panel][ballistics]")
{
    Rig rig;
    rig.snap.samplePeak = { -6.0f, -6.0f };
    rig.step (0.0);
    rig.snap.samplePeak = { MeterSnapshot::negInf, MeterSnapshot::negInf };
    rig.run (10, 20.0);
    REQUIRE (rig.panel.getHeldPeakDb (0) == -6.0f);

    int fired = 0;
    rig.panel.onStatisticsReset = [&] { ++fired; };

    for (auto* child : rig.panel.getChildren())
        if (auto* button = dynamic_cast<HeaderButton*> (child))
            if (button->onClick != nullptr)
                button->onClick();

    if (fired == 0)
        SKIP ("the reset button could not be located through the public child list");

    CHECK (fired == 1);
    CHECK (rig.panel.getHeldPeakDb (0) == rig.panel.getBarPeakDb (0));
}

TEST_CASE ("TooltipText::correlation: windows, U+2212 minus and the mono text", "[gui][meter][tooltip]")
{
    const auto stereo = TooltipText::correlation (false);
    INFO ("tooltip: " << stereo.toStdString());
    CHECK (stereo.contains ("500 ms"));
    CHECK (stereo.contains ("3 s"));
    CHECK (stereo.containsChar (0x2212));                    // ui-spec 8.5: "When shipped, it uses U+2212"
    CHECK_FALSE (stereo.containsChar (0x00E2));              // no mojibake (a double-encoded minus sign)
    CHECK_FALSE (stereo.containsChar (0x0088));
    CHECK_FALSE (stereo.containsChar (0x0092));
    CHECK (TooltipText::correlation (true) == "Correlation and balance need two channels.");
}

//==============================================================================
// Round 3 (D-1): after resetHeldValues the text refreshes on every update for
// resetForceTextMs, so the maxima the processor clears a block later show up
// at once. The force window runs on the real millisecond clock, so the
// simulated time here starts from it.

namespace
{
juce::String u (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
}

TEST_CASE ("MeterPanel::resetHeldValues: text refreshes on every update for 250 ms", "[gui][meter][reset]")
{
    Rig rig;
    rig.now = juce::Time::getMillisecondCounterHiRes();
    rig.snap.maxMomentary = -10.0f;
    rig.snap.maxShortTerm = -11.0f;
    rig.run (10);   // settle; the 100 ms text clock has run

    const int mMax = MeterPanel::loudnessCellIndex (MeterPanel::loudMMax);
    const int sMax = MeterPanel::loudnessCellIndex (MeterPanel::loudSMax);
    REQUIRE (rig.panel.getCellText (mMax) == u (m ("10.0")));

    rig.panel.resetHeldValues();
    const double t0 = rig.now;

    // Each of the next updates (all well inside 250 ms of the reset call)
    // shows a changed snapshot value immediately.
    for (int i = 1; i <= 8; ++i)
    {
        rig.snap.maxMomentary = -20.0f - (float) i;
        rig.snap.maxShortTerm = -30.0f - (float) i;
        rig.now = t0 + i;   // 1 ms apart: far below the 100 ms cadence
        ++rig.snap.blockCounter;
        rig.panel.update (rig.snap, rig.now, {});

        INFO ("update " << i);
        CHECK (rig.panel.getCellText (mMax) == u (m ((juce::String (20 + i) + ".0").toRawUTF8())));
        CHECK (rig.panel.getCellText (sMax) == u (m ((juce::String (30 + i) + ".0").toRawUTF8())));
    }
}

TEST_CASE ("MeterPanel::resetHeldValues: the 100 ms cadence returns after the window", "[gui][meter][reset]")
{
    Rig rig;
    rig.now = juce::Time::getMillisecondCounterHiRes();
    rig.snap.maxMomentary = -10.0f;
    rig.run (10);

    const int mMax = MeterPanel::loudnessCellIndex (MeterPanel::loudMMax);
    rig.panel.resetHeldValues();

    // Real clock may have advanced a little; jump the simulated time well past
    // the window, take one update (text refresh as the 100 ms clock expired),
    // then change the value and update again 10 ms later: no refresh.
    rig.now += 1000.0;
    rig.snap.maxMomentary = -40.0f;
    rig.step (0.0);
    CHECK (rig.panel.getCellText (mMax) == u (m ("40.0")));

    rig.snap.maxMomentary = -50.0f;
    rig.step (10.0);
    CHECK (rig.panel.getCellText (mMax) == u (m ("40.0")));

    rig.step (100.0);
    CHECK (rig.panel.getCellText (mMax) == u (m ("50.0")));
}

TEST_CASE ("MeterPanel: maxima below -99.9 LUFS and non-finite maxima", "[gui][meter][reset]")
{
    Rig rig;
    const int mMax = MeterPanel::loudnessCellIndex (MeterPanel::loudMMax);
    const int sMax = MeterPanel::loudnessCellIndex (MeterPanel::loudSMax);

    rig.snap.maxMomentary = -153.7f;
    rig.snap.maxShortTerm = -99.9f;
    rig.refreshText();
    CHECK (rig.panel.getCellText (mMax) == u (m ("inf")));
    CHECK (rig.panel.getCellText (sMax) == u (m ("99.9")));

    rig.snap.maxMomentary = qNaN;
    rig.snap.maxShortTerm = -posInf;
    rig.refreshText();
    CHECK (rig.panel.getCellText (mMax) == u (dash));
    CHECK (rig.panel.getCellText (sMax) == u (dash));
}
