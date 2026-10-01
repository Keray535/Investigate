// T-011 editor black-box check (plugin-validator).
// Usage: InvestigateEditorCheck <outDir> [cycles]
// Exit code 0 when every check passes.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "gui/HeaderButton.h"
#include "gui/ModeSelector.h"
#include "gui/ParamControl.h"
#include "params/ParameterIDs.h"

#include <cstdio>
#include <thread>

#define NOMINMAX
#include <windows.h>

using investigate::InvestigateEditor;
using investigate::InvestigateProcessor;

namespace
{
    int total = 0, failed = 0;

    void check (bool ok, const juce::String& text)
    {
        ++total;
        if (! ok) ++failed;
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", text.toRawUTF8());
    }

    // JUCE's modal-loop pump is not compiled into the plugin library, so drain
    // the Win32 queue directly (JUCE's async messages and timers arrive there).
    void pump (int ms)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) ms;
        do
        {
            MSG msg;
            while (PeekMessage (&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage (&msg);
                DispatchMessage (&msg);
            }
            juce::Thread::sleep (1);
        } while (juce::Time::getMillisecondCounter() < end);
    }

    void collect (juce::Component& c, std::vector<investigate::ParamControl*>& pcs,
                  std::vector<investigate::HeaderButton*>& hbs, investigate::ModeSelector*& ms)
    {
        if (auto* p = dynamic_cast<investigate::ParamControl*> (&c)) pcs.push_back (p);
        if (auto* h = dynamic_cast<investigate::HeaderButton*> (&c)) hbs.push_back (h);
        if (auto* m = dynamic_cast<investigate::ModeSelector*> (&c)) ms = m;
        for (auto* ch : c.getChildren()) collect (*ch, pcs, hbs, ms);
    }

    struct Parts
    {
        std::vector<investigate::ParamControl*> pcs;
        std::vector<investigate::HeaderButton*> hbs;
        investigate::ModeSelector* ms = nullptr;
        explicit Parts (juce::Component& e) { collect (e, pcs, hbs, ms); }
        investigate::HeaderButton* headerAt (int x, int y)
        {
            for (auto* h : hbs)
                if (h->getParentComponent() != nullptr && h->getX() == x && h->getY() == y) return h;
            return nullptr;
        }
    };

    void prime (InvestigateProcessor& p)
    {
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> b (2, 512);
        juce::Random r (1);
        juce::MidiBuffer m;
        for (int i = 0; i < 20; ++i)
        {
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < 512; ++s) b.setSample (c, s, r.nextFloat() * 0.2f - 0.1f);
            p.processBlock (b, m);
        }
    }

    std::unique_ptr<InvestigateEditor> open (InvestigateProcessor& p)
    {
        return std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (p.createEditor()));
    }

    bool same (juce::Colour a, juce::Colour b) { return a.getARGB() == b.getARGB(); }

    std::pair<int, int> extent (const juce::Image& img, bool horizontal, int fixed, juce::Colour c)
    {
        const int n = horizontal ? img.getWidth() : img.getHeight();
        int a = -1, b = -1;
        for (int i = 0; i < n; ++i)
        {
            auto px = horizontal ? img.getPixelAt (i, fixed) : img.getPixelAt (fixed, i);
            if (same (px, c)) { if (a < 0) a = i; b = i; }
        }
        return { a, b };
    }

    void expectNear (const juce::String& name, int actual, int expected)
    {
        check (std::abs (actual - expected) <= 2,
               name + ": measured " + juce::String (actual) + ", spec " + juce::String (expected) + " (+-2)");
    }

    void savePng (const juce::Image& img, const juce::File& f)
    {
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
    }

    void measure (InvestigateEditor& ed, const juce::File& out, const juce::String& tag, bool collapsed)
    {
        const int W = ed.getWidth(), H = ed.getHeight();
        const int MS = collapsed ? 0 : 296;
        auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        savePng (img, out.getChildFile (tag + ".png"));
        std::printf ("-- %s (%dx%d, meters %s)\n", tag.toRawUTF8(), W, H, collapsed ? "collapsed" : "shown");

        const juce::Colour bgPlot (0xff0A0D13), surface (0xff131924), border (0xff2A3447), surface2 (0xff1A2130);

        auto hb = extent (img, false, 440, border);
        expectNear (tag + " header border line y", hb.first, 43);

        auto pv = extent (img, false, 20, bgPlot);
        expectNear (tag + " plot top", pv.first, 44);
        expectNear (tag + " plot bottom (exclusive)", pv.second + 1, H - 56);
        auto ph = extent (img, true, 50, bgPlot);
        expectNear (tag + " plot left", ph.first, 0);
        expectNear (tag + " plot right (exclusive)", ph.second + 1, W - MS);

        int cbTop = -1;
        for (int y = H - 80; y < H; ++y)
            if (same (img.getPixelAt (5, y), border)) { cbTop = y; break; }
        expectNear (tag + " control bar top border y", cbTop, H - 56);

        if (! collapsed)
        {
            int sx = -1;
            for (int x = W - 1; x >= 0; --x)
                if (! same (img.getPixelAt (x, 48), surface)) { sx = x; break; }
            expectNear (tag + " meter strip left border x", sx, W - 296);
            check (sx >= 0 && same (img.getPixelAt (sx, 48), border), tag + " meter strip left line is border colour");
            check (same (img.getPixelAt (W - 40, H - 5), surface), tag + " meter strip fill is surface at bottom (x = W-40, y = H-5), got " + img.getPixelAt (W - 40, H - 5).toDisplayString (false));
        }
        else
        {
            check (same (img.getPixelAt (W - 5, 100), bgPlot), tag + " collapsed: plot reaches right edge");
        }

        int fx = -1;
        for (int x = 30; x < 80; ++x)
            if (same (img.getPixelAt (x, H / 2), surface2)) { fx = x; break; }
        expectNear (tag + " data rect left frame x", fx, 48);
        int fy = -1;
        for (int y = 46; y < 80; ++y)
            if (same (img.getPixelAt (W / 4, y), surface2)) { fy = y; break; }
        expectNear (tag + " data rect top frame y", fy, 56);
        int fr = -1;
        for (int x = W - MS - 1; x > W - MS - 40; --x)
            if (same (img.getPixelAt (x, H / 2), surface2)) { fr = x; break; }
        expectNear (tag + " data rect right frame x", fr, 48 + ((W - MS) - 64) - 1);
        int fb = -1;
        for (int y = H - 57; y > H - 100; --y)
            if (same (img.getPixelAt (W / 4, y), surface2)) { fb = y; break; }
        expectNear (tag + " data rect bottom frame y", fb, 56 + ((H - 100) - 36) - 1);

        const int row = H - 28; // 20 px below the chip top (chips are 40 high at y = H - 48)
        std::vector<int> starts;
        int lastEnd = -100;
        for (int x = 0; x < W - MS; ++x)
        {
            if (same (img.getPixelAt (x, row), surface2))
            {
                if (x - lastEnd >= 7) starts.push_back (x);
                lastEnd = x;
            }
        }
        juce::String s;
        for (int x : starts) s << x << " ";
        std::printf ("   chip run starts at y=%d: %s\n", row, s.toRawUTF8());

        // ui-spec 4.3 resolved positions. Panel width = W - MS decides which groups show.
        std::vector<int> expected;
        const int panel = W - MS;
        if (panel >= 880 + 24) expected = { 12, 80, 156, 240, 320, 408, 508 };
        else if (panel >= 592 + 24) expected = { 12, 80, 156, 240, 320, 408, 508 };
        if (panel >= 904)
        {
            const int cStart = 12 + (panel - 24) - 268;
            expected.push_back (cStart);
            expected.push_back (cStart + 148);
        }
        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (i < starts.size()) expectNear (tag + " chip " + juce::String ((int) i) + " x", starts[i], expected[i]);
            else check (false, tag + " chip " + juce::String ((int) i) + " missing");
        }
        check (starts.size() == expected.size(), tag + " chip count " + juce::String ((int) starts.size()) + ", spec " + juce::String ((int) expected.size()));
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? argv[1] : ".");
    out.createDirectory();
    const int cycles = argc > 2 ? juce::String (argv[2]).getIntValue() : 50;

    {
        InvestigateProcessor proc;
        prime (proc);

        // (1) screenshots and edge measurement
        {
            auto ed = open (proc);
            check (ed != nullptr, "createEditor returns an InvestigateEditor");
            check (ed->getWidth() == 1200 && ed->getHeight() == 700, "default size 1200x700 (got " + juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()) + ")");
            ed->addToDesktop (0);
            ed->setVisible (true);
            pump (150);
            measure (*ed, out, "editor-1200x700", false);

            ed->setSize (920, 540);
            pump (100);
            measure (*ed, out, "editor-920x540", false);

            ed->setSize (1200, 700);
            Parts parts (*ed);
            if (auto* mb = parts.headerAt (1200 - 72, 8))
            {
                mb->onClick();
                pump (100);
                measure (*ed, out, "editor-1200x700-collapsed", true);
                mb->onClick();
                pump (50);
            }
            else check (false, "meters toggle button found at (W-72, 8)");
        }

        // (3) persistence and clamp
        {
            auto ed = open (proc);
            ed->setSize (1500, 900);
            pump (50);
            {
                Parts parts (*ed);
                if (auto* mb = parts.headerAt (1500 - 72, 8)) mb->onClick();
            }
            pump (50);
            ed.reset();

            auto vs = proc.getApvts().state.getChildWithName ("ViewState");
            check ((int) vs["editorWidth"] == 1500 && (int) vs["editorHeight"] == 900 && (bool) vs["metersCollapsed"],
                   "ViewState holds 1500x900, collapsed (" + vs["editorWidth"].toString() + "x" + vs["editorHeight"].toString() + ")");

            ed = open (proc);
            check (ed->getWidth() == 1500 && ed->getHeight() == 900, "size persists across editor reopen");
            {
                Parts p2 (*ed);
                auto* mb = p2.headerAt (1500 - 72, 8);
                check (mb != nullptr && ! mb->isToggled(), "metersCollapsed persists across reopen (Meters toggle off)");
            }
            ed.reset();

            juce::MemoryBlock mbk;
            proc.getStateInformation (mbk);
            InvestigateProcessor fresh;
            fresh.setStateInformation (mbk.getData(), (int) mbk.getSize());
            auto ed2 = open (fresh);
            check (ed2->getWidth() == 1500 && ed2->getHeight() == 900, "size persists across getStateInformation/setStateInformation");
            {
                Parts p2 (*ed2);
                auto* mb = p2.headerAt (1500 - 72, 8);
                check (mb != nullptr && ! mb->isToggled(), "metersCollapsed persists across state round trip");
            }
            ed2.reset();

            const int cases[][4] = { { 100, 100, 920, 540 }, { 9999, 9999, 2560, 1600 }, { 919, 1601, 920, 1600 }, { 2561, 539, 2560, 540 } };
            for (auto& c : cases)
            {
                auto v = fresh.getApvts().state.getChildWithName ("ViewState");
                v.setProperty ("editorWidth", c[0], nullptr);
                v.setProperty ("editorHeight", c[1], nullptr);
                auto e = open (fresh);
                check (e->getWidth() == c[2] && e->getHeight() == c[3],
                       "restore clamp " + juce::String (c[0]) + "x" + juce::String (c[1]) + " -> " + juce::String (e->getWidth()) + "x" + juce::String (e->getHeight())
                           + " (spec " + juce::String (c[2]) + "x" + juce::String (c[3]) + ")");
            }

            auto e = open (fresh);
            e->setBoundsConstrained ({ 0, 0, 100, 100 });
            check (e->getWidth() == 920 && e->getHeight() == 540, "live resize 100x100 clamps to 920x540 (got " + juce::String (e->getWidth()) + "x" + juce::String (e->getHeight()) + ")");
            e->setBoundsConstrained ({ 0, 0, 5000, 5000 });
            check (e->getWidth() == 2560 && e->getHeight() == 1600, "live resize 5000x5000 clamps to 2560x1600 (got " + juce::String (e->getWidth()) + "x" + juce::String (e->getHeight()) + ")");
            e->setSize (1234, 777);
            auto v = fresh.getApvts().state.getChildWithName ("ViewState");
            check ((int) v["editorWidth"] == 1234 && (int) v["editorHeight"] == 777, "resize persisted on every resize (ViewState " + v["editorWidth"].toString() + "x" + v["editorHeight"].toString() + ")");
        }

        // (4) automation to control, and (6) freeze auto-unfreeze
        {
            proc.getApvts().state.getChildWithName ("ViewState").setProperty ("metersCollapsed", false, nullptr);
            auto ed = open (proc);
            ed->setSize (1200, 700);
            ed->addToDesktop (0);
            ed->setVisible (true);
            pump (100);
            Parts parts (*ed);
            std::printf ("-- automation: %d ParamControls found\n", (int) parts.pcs.size());

            juce::Random rnd (7);
            for (auto* idc : { investigate::ParamIDs::fftSize, investigate::ParamIDs::overlap, investigate::ParamIDs::window, investigate::ParamIDs::avgTime,
                               investigate::ParamIDs::smoothing, investigate::ParamIDs::slope, investigate::ParamIDs::secondary,
                               investigate::ParamIDs::freqLow, investigate::ParamIDs::freqHigh, investigate::ParamIDs::dbLow,
                               investigate::ParamIDs::dbHigh, investigate::ParamIDs::meterScale, investigate::ParamIDs::rmsWindow })
            {
                auto* prm = proc.getApvts().getParameter (idc);
                int found = 0;
                bool allOk = true, allOkAsync = true;
                for (int trial = 0; trial < 2; ++trial)
                {
                    const float norm = trial == 0 ? 0.0f : rnd.nextFloat();
                    prm->setValueNotifyingHost (norm);
                    pump (1);
                    const float expectReal = prm->convertFrom0to1 (prm->getValue());
                    found = 0;
                    for (auto* pc : parts.pcs)
                        if (pc->getParamID() == idc)
                        {
                            ++found;
                            allOk = allOk && std::abs (pc->getValue() - expectReal) < 1e-3f * std::max (1.0f, std::abs (expectReal));
                        }
                }
                const float norm2 = rnd.nextFloat();
                std::thread t ([prm, norm2] { prm->setValueNotifyingHost (norm2); });
                t.join();
                pump (17);
                const float expectReal2 = prm->convertFrom0to1 (prm->getValue());
                for (auto* pc : parts.pcs)
                    if (pc->getParamID() == idc)
                        allOkAsync = allOkAsync && std::abs (pc->getValue() - expectReal2) < 1e-3f * std::max (1.0f, std::abs (expectReal2));
                check (found > 0 && allOk, juce::String (idc) + ": control(s) follow message-thread set (" + juce::String (found) + " control(s))");
                check (found > 0 && allOkAsync, juce::String (idc) + ": control(s) follow background-thread set within 17 ms");
            }

            for (int i = 0; i < 7; ++i)
            {
                auto* prm = proc.getApvts().getParameter (investigate::ParamIDs::channelMode);
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
                pump (1);
                check (parts.ms != nullptr && parts.ms->getSelectedIndex() == i, "channelMode " + juce::String (i) + " reflected by the Mode selector");
            }
            auto* sc = proc.getApvts().getParameter (investigate::ParamIDs::showSidechain);
            for (float v : { 0.0f, 1.0f })
            {
                sc->setValueNotifyingHost (v);
                pump (1);
                auto* b = parts.headerAt (1200 - 368, 8);
                check (b != nullptr && b->isToggled() == (v > 0.5f), "showSidechain " + juce::String (v) + " reflected by SC toggle");
            }

            auto* cm = proc.getApvts().getParameter (investigate::ParamIDs::channelMode);
            cm->setValueNotifyingHost (cm->convertTo0to1 (0.0f));
            pump (1);
            auto* fb = parts.headerAt (1200 - 302, 8);
            check (fb != nullptr, "Freeze button found");
            if (fb != nullptr)
            {
                fb->onClick();
                pump (1);
                check (ed->isFrozen() && fb->isToggled(), "Freeze engages via button");
                cm->setValueNotifyingHost (cm->convertTo0to1 (1.0f));
                pump (1);
                check (! ed->isFrozen() && ! fb->isToggled(), "Freeze auto-unfreezes on host channelMode change");
                fb->onClick();
                pump (1);
                check (ed->isFrozen(), "Freeze re-engages");
                cm->setValueNotifyingHost (cm->convertTo0to1 (1.0f));
                pump (1);
                check (ed->isFrozen(), "Freeze stays when channelMode is set to the same value");
                cm->setValueNotifyingHost (cm->convertTo0to1 (3.0f));
                pump (1);
                check (! ed->isFrozen(), "Freeze auto-unfreezes on a second channelMode change");
            }
        }

        // (2) open/close cycles
        {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            juce::Random r (3);
            for (int i = 0; i < cycles; ++i)
            {
                auto ed = open (proc);
                if (i % 2 == 0) { ed->addToDesktop (0); ed->setVisible (true); }
                if (i % 5 == 0) ed->setSize (920 + r.nextInt (400), 540 + r.nextInt (300));
                if (i % 7 == 0)
                {
                    Parts p (*ed);
                    if (auto* sb = p.headerAt (ed->getWidth() - 40, 8)) sb->onClick();
                }
                pump (10 + r.nextInt (30));
                ed.reset();
            }
            std::printf ("-- %d open/close cycles done in %.0f ms\n", cycles, juce::Time::getMillisecondCounterHiRes() - t0);
            check (true, juce::String (cycles) + " open/close cycles completed without crash");
        }
    }

    std::printf ("%d checks, %d failed\n", total, failed);
    return failed == 0 ? 0 : 1;
}
