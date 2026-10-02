// T-012 spectrum view black-box check (plugin-validator).
// Usage: InvestigateSpectrumCheck <outDir> [benchFrames] [sw|d2d|lat [cpuSeconds [all|WxH@scale [caseMask]]]]
// Drives only public surfaces: createEditor, the APVTS, the processor's
// processBlock, and the SpectrumView test hooks named in the T-012 task log
// (setFrameForTesting, setHoverForTesting, getMapping, clipboardTextForX).
// Exit code 0 when every check passes. Benchmarks print numbers only.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "gui/HeaderButton.h"
#include "gui/SpectrumMath.h"
#include "params/ParameterIDs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

using investigate::InvestigateEditor;
using investigate::InvestigateProcessor;
using investigate::SpectrumFrame;
using investigate::SpectrumView;
namespace P = investigate::ParamIDs;

namespace
{
    int total = 0, failed = 0;

    void check (bool ok, const juce::String& text)
    {
        ++total;
        if (! ok) ++failed;
        std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", text.toRawUTF8());
    }

    void note (const juce::String& text) { std::printf ("   %s\n", text.toRawUTF8()); }

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

    void savePng (const juce::Image& img, const juce::File& f)
    {
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
    }

    void setChoice (InvestigateProcessor& p, const char* id, int index)
    {
        auto* prm = p.getApvts().getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
    }

    void setReal (InvestigateProcessor& p, const char* id, float v)
    {
        auto* prm = p.getApvts().getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }

    SpectrumView* findView (juce::Component& c)
    {
        if (auto* v = dynamic_cast<SpectrumView*> (&c)) return v;
        for (auto* ch : c.getChildren())
            if (auto* v = findView (*ch)) return v;
        return nullptr;
    }

    investigate::HeaderButton* findHeaderButton (juce::Component& c, int x, int y)
    {
        if (auto* h = dynamic_cast<investigate::HeaderButton*> (&c))
            if (h->getParentComponent() != nullptr && h->getX() == x && h->getY() == y) return h;
        for (auto* ch : c.getChildren())
            if (auto* h = findHeaderButton (*ch, x, y)) return h;
        return nullptr;
    }

    //==========================================================================
    // Signal source. L = 1 kHz at -6 dBFS (+ low noise), R = 3 kHz at -12 dBFS,
    // sidechain = 5 kHz at -20 dBFS. Phase-continuous across calls.
    struct Source
    {
        double ph[5] { 0, 0, 0, 0, 0 };
        juce::Random rnd { 42 };
        float ampL = 0.5f;      // -6.02 dBFS
        float ampR = 0.25f;     // -12.04 dBFS
        float ampSc = 0.1f;     // -20 dBFS
        bool equalLR = false;   // L = R = the 1 kHz tone (placement test)
        bool silent = false;

        void fill (juce::AudioBuffer<float>& b)
        {
            const int n = b.getNumSamples();
            const double sr = 48000.0;
            for (int i = 0; i < n; ++i)
            {
                const float noise = (rnd.nextFloat() * 2.0f - 1.0f) * 0.0005f;
                // A common 400 Hz tone (Mid only) makes the M/S curves differ.
                const float common = equalLR ? 0.0f : 0.2f * (float) std::sin (ph[3]);
                // A 700 Hz tone in antiphase (Side only) makes the Side curve differ from Mid.
                const float side = equalLR ? 0.0f : 0.15f * (float) std::sin (ph[4]);
                float l = ampL * (float) std::sin (ph[0]) + noise + common + side;
                float r = equalLR ? ampL * (float) std::sin (ph[0]) + noise : ampR * (float) std::sin (ph[1]) + noise + common - side;
                float s = ampSc * (float) std::sin (ph[2]) + noise;
                if (silent) l = r = s = 0.0f;
                b.setSample (0, i, l);
                b.setSample (1, i, r);
                if (b.getNumChannels() > 2) b.setSample (2, i, s);
                if (b.getNumChannels() > 3) b.setSample (3, i, s);
                ph[0] += 2.0 * juce::MathConstants<double>::pi * 1000.0 / sr;
                ph[1] += 2.0 * juce::MathConstants<double>::pi * 3000.0 / sr;
                ph[2] += 2.0 * juce::MathConstants<double>::pi * 5000.0 / sr;
                ph[3] += 2.0 * juce::MathConstants<double>::pi * 400.0 / sr;
                ph[4] += 2.0 * juce::MathConstants<double>::pi * 700.0 / sr;
            }
        }
    };

    struct Rig
    {
        InvestigateProcessor proc;
        juce::AudioBuffer<float> buf;
        juce::MidiBuffer midi;
        Source src;
        bool sidechain;

        explicit Rig (bool withSidechain) : sidechain (withSidechain)
        {
            proc.getBus (true, 0)->setCurrentLayout (juce::AudioChannelSet::stereo());
            proc.getBus (false, 0)->setCurrentLayout (juce::AudioChannelSet::stereo());
            if (withSidechain)
                proc.getBus (true, 1)->setCurrentLayout (juce::AudioChannelSet::stereo());
            proc.setRateAndBufferSizeDetails (48000.0, 512);
            proc.prepareToPlay (48000.0, 512);
            buf.setSize (proc.getTotalNumInputChannels(), 512);
        }

        ~Rig() { proc.releaseResources(); }

        void block()
        {
            src.fill (buf);
            proc.processBlock (buf, midi);
        }

        // Real-time-ish feed: one 512-sample block (10.7 ms of audio) per ~10 ms.
        void feed (int ms)
        {
            const int blocks = std::max (1, ms * 48 / 512);
            for (int i = 0; i < blocks; ++i)
            {
                block();
                pump (10);
            }
        }
    };

    struct Peak { int index = -1; float db = -1000.0f; };

    Peak peakOf (const std::array<float, SpectrumFrame::maxPoints>& a, int n)
    {
        Peak p;
        for (int i = 0; i < n; ++i)
            if (a[(size_t) i] > p.db) { p.db = a[(size_t) i]; p.index = i; }
        return p;
    }

    //==========================================================================
    // Pixel classes for the curve colours (ui-spec 5.1), counted over the data
    // rect of a snapshot. Thresholds are wide because of anti-aliasing.
    struct Counts { int primaryA = 0, primaryB = 0, secA = 0, secB = 0, sidechain = 0; };

    Counts countCurves (const juce::Image& img, juce::Rectangle<int> r)
    {
        Counts c;
        for (int y = r.getY(); y < r.getBottom(); ++y)
            for (int x = r.getX(); x < r.getRight(); ++x)
            {
                const auto px = img.getPixelAt (x, y);
                const int R = px.getRed(), G = px.getGreen(), B = px.getBlue();
                const auto close = [&] (int r, int g, int b, int tol) { return std::abs (R - r) <= tol && std::abs (G - g) <= tol && std::abs (B - b) <= tol; };
                if (close (0x62, 0xD6, 0xF0, 3)) ++c.primaryA;
                else if (close (0xFF, 0xB4, 0x54, 3)) ++c.primaryB;
                else if (close (0x3E, 0x8F, 0xA3, 2)) ++c.secA;
                else if (close (0xA8, 0x7A, 0x3C, 2)) ++c.secB;
                else if (R >= G - 6 && B - G >= 25 && B > 60 && R < 120) ++c.sidechain; // #A58BFF at 26 %
            }
        return c;
    }

    //==========================================================================
    // Independent re-implementation of the spec's clipboard text (ui-spec 5.5,
    // 5.6): 3 significant figures, 1 decimal below 100, none above.
    juce::String specClipboard (double hz)
    {
        const double mag = std::pow (10.0, std::floor (std::log10 (hz)) - 2.0);
        const double r = std::round (hz / mag) * mag;
        return r < 100.0 ? juce::String (r, 1) : juce::String ((int) std::lround (r));
    }

    // Windows clipboard access can fail transiently when another process holds
    // it; retry the read a few times and count the tries.
    int clipboardTries = 0;
    juce::String readClipboardExpecting (const juce::String& expected)
    {
        juce::String t;
        for (int i = 0; i < 10; ++i)
        {
            ++clipboardTries;
            t = juce::SystemClipboard::getTextFromClipboard();
            if (t == expected) break;
            pump (30);
        }
        return t;
    }

    void clickAt (SpectrumView& v, juce::Point<float> pos, bool right)
    {
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        const auto mods = juce::ModifierKeys (right ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier);
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent down (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &v, &v, now, pos, now, 1, false);
        v.mouseDown (down);
        juce::MouseEvent up (src, pos, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &v, &v, now, pos, now, 1, false);
        v.mouseUp (up);
    }

    //==========================================================================
    // Synthetic animated frame for the benchmark: 2048 points, jittery noise
    // floor, moving peaks, so every column holds several vertices.
    void makeFrame (SpectrumFrame& f, double fLo, double fHi, int t, bool dual, bool sec, bool sc, juce::Random& rnd)
    {
        f.numPoints = SpectrumFrame::maxPoints;
        f.isDual = dual;
        f.hasSecondary = sec;
        f.hasSidechain = sc;
        f.sampleRate = 48000.0;
        f.sequence = (uint64_t) t + 1;
        const double ratio = std::log (fHi / fLo);
        const double centre = 0.3 + 0.4 * (0.5 + 0.5 * std::sin (t * 0.05));
        for (int i = 0; i < f.numPoints; ++i)
        {
            const double u = (double) i / (f.numPoints - 1);
            f.freqHz[(size_t) i] = (float) (fLo * std::exp (u * ratio));
            const double peak = 50.0 * std::exp (-std::pow ((u - centre) / 0.02, 2.0));
            const double base = -70.0 + 12.0 * std::sin (u * 40.0) + peak;
            const float jitter = (rnd.nextFloat() - 0.5f) * 6.0f;
            f.primaryDb[(size_t) i] = (float) base + jitter;
            f.primaryBDb[(size_t) i] = (float) (base - 6.0) + (rnd.nextFloat() - 0.5f) * 6.0f;
            f.secondaryDb[(size_t) i] = (float) base + 4.0f;
            f.secondaryBDb[(size_t) i] = (float) base - 2.0f;
            f.sidechainDb[(size_t) i] = (float) (base - 15.0) + (rnd.nextFloat() - 0.5f) * 4.0f;
        }
    }

    struct Stats { double median = 0, p95 = 0, worst = 0, mean = 0; };

    Stats stats (std::vector<double> v)
    {
        Stats s;
        if (v.empty()) return s;
        std::sort (v.begin(), v.end());
        s.median = v[v.size() / 2];
        s.p95 = v[(size_t) std::min<double> ((double) v.size() - 1.0, std::ceil (0.95 * (double) v.size()) - 1.0)];
        s.worst = v.back();
        for (double x : v) s.mean += x;
        s.mean /= (double) v.size();
        return s;
    }

    juce::String fmt (const Stats& s)
    {
        return "median " + juce::String (s.median, 2) + " ms, mean " + juce::String (s.mean, 2) + " ms, p95 "
             + juce::String (s.p95, 2) + " ms, worst " + juce::String (s.worst, 2) + " ms";
    }


    //==========================================================================

    //==========================================================================
    struct Scenario { const char* name; bool dual; bool sec; bool sc; bool hover; };

    // Exact thread cycles (QueryThreadCycleTime), converted to ms with a cycles-per-ms
    // figure calibrated by spinning this thread for 300 ms; GetThreadTimes is a
    // sampled count with too much error for per-frame figures near 1 ms.
    double threadCpuMs()
    {
        static double cyclesPerMs = 0.0;
        const auto now = [] { ULONG64 c = 0; QueryThreadCycleTime (GetCurrentThread(), &c); return (double) c; };
        if (cyclesPerMs == 0.0)
        {
            const double c0 = now();
            const auto t0 = juce::Time::getHighResolutionTicks();
            while (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) < 0.3) {}
            cyclesPerMs = (now() - c0) / 300.0;
            std::printf ("calibrated %.0f cycles per ms (%.2f GHz)\n", cyclesPerMs, cyclesPerMs / 1.0e6);
        }
        return now() / cyclesPerMs;
    }

    // Paint wall-time probes: two 1x1 invisible siblings placed just before and
    // just after the SpectrumView in z-order. Each records a high-resolution
    // tick when painted, so (end - start) is the wall time of the view's
    // paint() including its children, as recorded on the Direct2D context.
    struct Probe : juce::Component
    {
        Probe() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override { stamp = juce::Time::getHighResolutionTicks(); ++count; }
        juce::int64 stamp = 0; int count = 0;
    };

    // Optional extras (T-029): sizesArg is "all" (1280x720 and 2560x1440 at scale 1)
    // or "<physW>x<physH>@<scale>" (global scale factor; the editor is sized in
    // logical units so that the physical size is physW x physH).
    // caseMask selects cases by bit (a=1, b=2, c=4, d=8).
    void benchD2d (int paintFrames, double cpuSeconds, const juce::String& sizesArg = "all", int caseMask = 15)
    {
        timeBeginPeriod (1);
        std::printf ("DIRECT2D: editor on the desktop (default JUCE renderer), frames paced at 60 Hz, synthetic 2048-point animated frame.\n");
        std::printf ("paint ms = wall time between two probe siblings around SpectrumView (z-order), per painted frame.\n");
        std::printf ("CPU %% = message-thread cycle time over the paced loop (gross), minus the same loop without handing frames to the view (net), over >= %.0f s.\n", cpuSeconds);
        struct Sc { const char* name; bool dual, sec, sc, hover; int secMode; int bit; };
        const Sc scs[] = {
            { "(a) single + RT max (factory default view)", false, true, false, false, 1, 1 },
            { "(b) dual + 2 secondaries", true, true, false, false, 1, 2 },
            { "(c) dual + 2 secondaries + SC + hover (worst case)", true, true, true, true, 1, 4 },
            { "(d) single + all-time average + SC", false, true, true, false, 3, 8 },
        };
        int sizes[2][2] = { { 1280, 720 }, { 2560, 1440 } };
        int numSizes = 2;
        double scale = 1.0;
        if (sizesArg != "all")
        {
            sizes[0][0] = sizesArg.upToFirstOccurrenceOf ("x", false, false).getIntValue();
            sizes[0][1] = sizesArg.fromFirstOccurrenceOf ("x", false, false).upToFirstOccurrenceOf ("@", false, false).getIntValue();
            scale = sizesArg.fromFirstOccurrenceOf ("@", false, false).getDoubleValue();
            numSizes = 1;
        }
        juce::Desktop::getInstance().setGlobalScaleFactor ((float) scale);
        std::printf ("global scale factor %.2f, physical window sizes requested: %d\n", scale, numSizes);
        Rig rig (true);
        for (int si = 0; si < numSizes; ++si)
        {
            const int* sz = sizes[si];
            for (auto& sc : scs)
            {
                if ((caseMask & sc.bit) == 0) continue;
                setChoice (rig.proc, P::secondary, sc.secMode);
                auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rig.proc.createEditor()));
                ed->setSize ((int) std::lround (sz[0] / scale), (int) std::lround (sz[1] / scale));
                ed->addToDesktop (0);
                ed->setVisible (true);
                pump (400);
                auto* view = findView (*ed);
                auto* parent = view->getParentComponent();
                Probe startP, endP, cornerP;   // cornerP: never repainted by the harness; counts the view's own data-rect paints
                const auto c = view->getBounds().getTopLeft().translated (0, -1);
                parent->addChildComponent (startP, parent->getIndexOfChildComponent (view));
                parent->addChildComponent (endP, parent->getIndexOfChildComponent (view) + 1);
                startP.setBounds (c.x, c.y, 1, 1);
                endP.setBounds (c.x, c.y, 1, 1);
                startP.setVisible (true);
                endP.setVisible (true);
                {
                    const auto dr = view->getDataRect() + view->getPosition();
                    parent->addChildComponent (cornerP, parent->getIndexOfChildComponent (view) + 1);
                    cornerP.setBounds (dr.getRight() - 1, dr.getBottom() - 1, 1, 1);
                    cornerP.setVisible (true);
                }
                const auto m = view->getMapping();
                juce::Random rnd (5);
                auto frame = std::make_unique<SpectrumFrame>();
                const auto hoverAt = [&] (int t) { return juce::Point<float> ((float) (m.x0 + m.w * (0.3 + 0.4 * (0.5 + 0.5 * std::sin (t * 0.07)))), (float) (m.y0 + m.h * 0.4)); };
                const double period = 1000.0 / 60.0;

                const auto runLoop = [&] (int frames, bool feed, std::vector<double>* paintMs)
                {
                    const double t0 = juce::Time::getMillisecondCounterHiRes();
                    const int p0 = startP.count;
                    for (int t = 0; t < frames; ++t)
                    {
                        makeFrame (*frame, m.fLo, m.fHi, t, sc.dual, sc.sec, sc.sc, rnd);
                        if (feed)
                        {
                            view->setFrameForTesting (*frame);
                            if (sc.hover) view->setHoverForTesting (hoverAt (t));
                            startP.repaint();
                            endP.repaint();
                        }
                        const double target = t0 + (t + 1) * period;
                        int lastCount = startP.count;
                        while (juce::Time::getMillisecondCounterHiRes() < target)
                        {
                            pump (1);
                            if (paintMs != nullptr && startP.count != lastCount && endP.count == startP.count)
                            {
                                paintMs->push_back (juce::Time::highResolutionTicksToSeconds (endP.stamp - startP.stamp) * 1000.0);
                                lastCount = startP.count;
                            }
                        }
                    }
                    return std::pair<double, int> { juce::Time::getMillisecondCounterHiRes() - t0, startP.count - p0 };
                };

                runLoop (60, true, nullptr);
                std::vector<double> times;
                runLoop (paintFrames, true, &times);
                std::printf ("probe counts start %d end %d, view %d,%d %dx%d\n", startP.count, endP.count, view->getX(), view->getY(), view->getWidth(), view->getHeight());

                // CPU: baseline (loop only), then loaded, each cpuSeconds long.
                const int cpuFrames = (int) (cpuSeconds * 60.0);
                const double b0 = threadCpuMs();
                const auto bw = runLoop (cpuFrames, false, nullptr);
                const double baseCpu = threadCpuMs() - b0;
                const double l0 = threadCpuMs();
                const int corner0 = cornerP.count;
                const auto lw = runLoop (cpuFrames, true, nullptr);
                const double loadCpu = threadCpuMs() - l0;
                const double viewPaintsPerSec = 1000.0 * (cornerP.count - corner0) / lw.first;
                const double grossPct = 100.0 * loadCpu / lw.first, netPct = 100.0 * (loadCpu - baseCpu) / lw.first;
                const auto drc = view->getDataRect();
                const bool expectedPaced = investigate::SpectrumMath::pacing::isPaced ((double) drc.getWidth() * drc.getHeight() * scale * scale,
                                                                                       investigate::SpectrumMath::pacing::countLayers (sc.dual, sc.sec, sc.sc));
                std::printf ("DIRECT2D %dx%d | %s | paints %d | paint %s | CPU gross %.1f %%, net %.1f %% (%.1f s, %.1f paints/s, baseline %.1f %%) | view data-rect paints/s %.1f | expected paced %d\n",
                             sz[0], sz[1], sc.name, (int) times.size(), fmt (stats (times)).toRawUTF8(), grossPct, netPct, lw.first / 1000.0,
                             1000.0 * lw.second / lw.first, 100.0 * baseCpu / bw.first, viewPaintsPerSec, (int) expectedPaced);
                std::fflush (stdout);
                parent->removeChildComponent (&startP);
                parent->removeChildComponent (&endP);
                parent->removeChildComponent (&cornerP);
                ed->removeFromDesktop();
            }
        }
        timeEndPeriod (1);
        setChoice (rig.proc, P::secondary, 1);
    }

    // Transparent probe laid over a region of the editor (above the view in z-order).
    // Its paint() is called whenever a repaint clip touches the region; it records
    // the tick, the clip (in the parent's coordinates) and a count.
    struct Overlay : juce::Component
    {
        Overlay() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            stamp = juce::Time::getHighResolutionTicks();
            clip = g.getClipBounds() + getPosition();
            ++count;
        }
        juce::int64 stamp = 0; int count = 0; juce::Rectangle<int> clip;
    };

    // Latency from an input call to the first paint that covers the change (T-029, frame pacing).
    // Frames are fed at 60 Hz (setFrameForTesting, no extra repaints) while the action is issued
    // at a random phase right after a fed frame, i.e. when a paced repaint is most likely pending.
    void benchLatency (int trials)
    {
        timeBeginPeriod (1);
        std::printf ("LATENCY: ms from the call (hover / freeze / left click / right click) to the first paint of the view region covering the change; frames fed at 60 Hz.\n");
        struct Cfg { int w, h; bool dual, sec, sc; int secMode; const char* name; };
        const Cfg cfgs[] = {
            { 1280, 720, false, true, false, 1, "1280x720 (a) single + RT max (unpaced reference)" },
            { 1280, 720, true, true, true, 1, "1280x720 (c) dual + 2 sec + SC (paced by layers)" },
            { 2560, 1440, false, true, false, 1, "2560x1440 (a) single + RT max (paced by area)" },
            { 2560, 1440, true, true, true, 1, "2560x1440 (c) dual + 2 sec + SC (paced)" },
        };
        Rig rig (true);
        juce::Random rnd (11);
        for (auto& c : cfgs)
        {
            setChoice (rig.proc, P::secondary, c.secMode);
            auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rig.proc.createEditor()));
            ed->setSize (c.w, c.h);
            ed->addToDesktop (0);
            ed->setVisible (true);
            pump (400);
            auto* view = findView (*ed);
            auto* parent = view->getParentComponent();
            Overlay over, btnOver;
            parent->addChildComponent (over, parent->getIndexOfChildComponent (view) + 1);
            over.setBounds (view->getBounds());
            over.setVisible (true);
            auto* fb = findHeaderButton (*ed, ed->getWidth() - 302, 8);
            auto* btnParent = fb != nullptr ? fb->getParentComponent() : nullptr;
            if (btnParent != nullptr)
            {
                btnParent->addChildComponent (btnOver, btnParent->getIndexOfChildComponent (fb) + 1);
                btnOver.setBounds (fb->getBounds());
                btnOver.setVisible (true);
            }
            const auto m = view->getMapping();
            auto frame = std::make_unique<SpectrumFrame>();
            int t = 0;
            double nextFeed = juce::Time::getMillisecondCounterHiRes();
            const auto feedUntil = [&] (double untilMs, const std::function<bool()>& stop)
            {
                while (juce::Time::getMillisecondCounterHiRes() < untilMs)
                {
                    const double now = juce::Time::getMillisecondCounterHiRes();
                    if (now >= nextFeed)
                    {
                        makeFrame (*frame, m.fLo, m.fHi, t++, c.dual, c.sec, c.sc, rnd);
                        view->setFrameForTesting (*frame);
                        nextFeed += 1000.0 / 60.0;
                        if (nextFeed < now) nextFeed = now + 1000.0 / 60.0;
                    }
                    pump (1);
                    if (stop && stop()) break;
                }
            };
            feedUntil (juce::Time::getMillisecondCounterHiRes() + 1000.0, {});

            const auto resolveMs = [] (juce::int64 t0) { return juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0; };
            std::printf ("%s (view %dx%d)\n", c.name, view->getWidth(), view->getHeight());
            for (int kind = 0; kind < 4; ++kind)
            {
                const char* kindName[] = { "hover move", "freeze toggle", "left click (reset)", "right click (copy)" };
                std::vector<double> viewMs, btnMs;
                // The reset toast only exists with an all-time secondary curve (all-time max here).
                if (kind == 2) { setChoice (rig.proc, P::secondary, 2); feedUntil (juce::Time::getMillisecondCounterHiRes() + 300.0, {}); }
                int timeouts = 0;
                for (int trial = 0; trial < trials; ++trial)
                {
                    // Align to just after a fed frame, then wait a random 0..16 ms phase.
                    const double target = nextFeed;
                    feedUntil (target + 1.0 + rnd.nextFloat() * 15.0, {});
                    const int c0 = over.count, b0 = btnOver.count;
                    juce::Point<float> p ((float) (m.x0 + m.w * (trial % 2 == 0 ? 0.35 : 0.62)), (float) (m.y0 + m.h * 0.4));
                    const auto t0 = juce::Time::getHighResolutionTicks();
                    if (kind == 0) view->setHoverForTesting (p);
                    else if (kind == 1 && fb != nullptr) fb->onClick();
                    else if (kind == 2) clickAt (*view, p, false);
                    else clickAt (*view, p, true);
                    double vMs = -1.0, bMs = -1.0;
                    const double deadline = juce::Time::getMillisecondCounterHiRes() + 150.0;
                    while (juce::Time::getMillisecondCounterHiRes() < deadline && (vMs < 0.0 || (kind == 1 && bMs < 0.0)))
                    {
                        pump (1);
                        // For hover the paint must cover the new pointer position.
                        if (vMs < 0.0 && over.count != c0 && (kind != 0 || over.clip.contains (p.toInt())))
                            vMs = juce::Time::highResolutionTicksToSeconds (over.stamp - t0) * 1000.0;
                        if (kind == 1 && bMs < 0.0 && btnOver.count != b0)
                            bMs = juce::Time::highResolutionTicksToSeconds (btnOver.stamp - t0) * 1000.0;
                    }
                    (void) resolveMs;
                    if (vMs < 0.0) ++timeouts; else viewMs.push_back (vMs);
                    if (bMs >= 0.0) btnMs.push_back (bMs);
                    if (kind == 1 && fb != nullptr)
                    {
                        feedUntil (juce::Time::getMillisecondCounterHiRes() + 120.0, {});
                        fb->onClick();   // unfreeze
                    }
                    feedUntil (juce::Time::getMillisecondCounterHiRes() + 120.0, {});
                }
                if (kind == 2) setChoice (rig.proc, P::secondary, c.secMode);
                const auto sv = stats (viewMs);
                std::printf ("  %-20s view paint: n %d, median %.1f, p95 %.1f, max %.1f ms, timeouts %d", kindName[kind], (int) viewMs.size(), sv.median, sv.p95, sv.worst, timeouts);
                if (kind == 1 && ! btnMs.empty()) { const auto sb = stats (btnMs); std::printf (" | button paint: median %.1f, p95 %.1f, max %.1f ms", sb.median, sb.p95, sb.worst); }
                std::printf ("\n");
                std::fflush (stdout);
            }
            parent->removeChildComponent (&over);
            if (btnParent != nullptr) btnParent->removeChildComponent (&btnOver);
            ed->removeFromDesktop();
        }
        setChoice (rig.proc, P::secondary, 1);
        timeEndPeriod (1);
    }

    void benchmark (const juce::File& out, int frames, bool doSoftware, bool doD2d, double cpuSecondsArg = 12.0, const juce::String& sizesArg = "all", int caseMaskArg = 15)
    {
        // Windows 11 moves a long-running, unfocused console process onto
        // efficiency cores and lowers its clock (EcoQoS "execution speed
        // throttling"): paint times jump about 5x a few seconds into a run.
        // Opt this process and thread out so the numbers reflect the code.
        {
            PROCESS_POWER_THROTTLING_STATE st {};
            st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
            st.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
            st.StateMask = 0;
            const BOOL pOk = SetProcessInformation (GetCurrentProcess(), ProcessPowerThrottling, &st, sizeof (st));
            THREAD_POWER_THROTTLING_STATE ts {};
            ts.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
            ts.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
            ts.StateMask = 0;
            const BOOL tOk = SetThreadInformation (GetCurrentThread(), ThreadPowerThrottling, &ts, sizeof (ts));
            SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);
            std::printf ("EcoQoS throttling disabled (process %d, thread %d), process priority high\n", (int) pOk, (int) tOk);
        }
        std::printf ("\n== Paint benchmark (%d frames per case, 2048-point animated frame) ==\n", frames);
        std::printf ("SOFTWARE = juce::SoftwareImageType image the size of the editor, clip = data rect, ed->paintEntireComponent.\n");
        std::printf ("DIRECT2D = editor on the desktop; message-thread CPU time per frame (GetThreadTimes over the whole loop) minus an idle baseline.\n");
        if (doD2d)
        {
            benchD2d (frames, cpuSecondsArg, sizesArg, caseMaskArg);
            return;
        }
        const Scenario scenarios[] = {
            { "single, secondary off, no SC", false, false, false, false },
            { "single + RT max (factory default view)", false, true, false, false },
            { "dual + 2 secondaries", true, true, false, false },
            { "single + RT max + SC", false, true, true, false },
            { "dual + 2 secondaries + SC", true, true, true, false },
            { "single + RT max + hover card", false, true, false, true },
            { "dual + 2 secondaries + SC + hover (worst case)", true, true, true, true },
        };
        const int sizes[][2] = { { 1280, 720 }, { 2560, 1440 } };

        Rig rig (true);
        for (auto& sz : sizes)
        {
            for (auto& sc : scenarios)
            {
                if (doSoftware)
                {
                    auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rig.proc.createEditor()));
                    ed->setSize (sz[0], sz[1]);
                    auto* view = findView (*ed);
                    const auto data = view->getDataRect() + view->getPosition();
                    juce::Image img (juce::Image::ARGB, sz[0], sz[1], true, juce::SoftwareImageType());
                    const auto m = view->getMapping();
                    juce::Random rnd (5);
                    auto frame = std::make_unique<SpectrumFrame>();
                    std::vector<double> times;
                    for (int t = 0; t < frames + 20; ++t)
                    {
                        makeFrame (*frame, m.fLo, m.fHi, t, sc.dual, sc.sec, sc.sc, rnd);
                        view->setFrameForTesting (*frame);
                        if (sc.hover)
                            view->setHoverForTesting ({ (float) (m.x0 + m.w * (0.3 + 0.4 * (0.5 + 0.5 * std::sin (t * 0.07)))), (float) (m.y0 + m.h * 0.4) });
                        juce::Graphics g (img);
                        g.reduceClipRegion (data);
                        const auto t0 = juce::Time::getHighResolutionTicks();
                        ed->paintEntireComponent (g, true);
                        const auto t1 = juce::Time::getHighResolutionTicks();
                        if (t >= 20) times.push_back (juce::Time::highResolutionTicksToSeconds (t1 - t0) * 1000.0);
                    }
                    std::printf ("SOFTWARE %dx%d | %s | %s\n", sz[0], sz[1], sc.name, fmt (stats (times)).toRawUTF8());
                    if (sc.dual && sc.hover && sz[0] == 1280)
                        savePng (img, out.getChildFile ("bench-software-dual-1280.png"));
                }

            }
        }
    }
}

int main (int argc, char** argv)
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (argc > 1 ? argv[1] : ".");
    out.createDirectory();
    const int benchFrames = argc > 2 ? juce::String (argv[2]).getIntValue() : 300;
    const juce::String mode = argc > 3 ? juce::String (argv[3]) : juce::String();

    if (mode == "lat")
    {
        benchLatency (benchFrames);
        return 0;
    }

    if (mode == "sw" || mode == "d2d")
    {
        benchmark (out, benchFrames, mode == "sw", mode == "d2d",
                   argc > 4 ? juce::String (argv[4]).getDoubleValue() : 12.0,
                   argc > 5 ? juce::String (argv[5]) : juce::String ("all"),
                   argc > 6 ? juce::String (argv[6]).getIntValue() : 15);
        return 0;
    }

    //==========================================================================
    std::printf ("== 1 kHz / -6 dBFS sine placement ==\n");
    {
        Rig rig (false);
        rig.src.equalLR = true;
        setChoice (rig.proc, P::channelMode, 2); // Left
        setChoice (rig.proc, P::window, 0);      // Hann
        setChoice (rig.proc, P::smoothing, 0);   // Off
        setReal (rig.proc, P::slope, 0.0f);
        setChoice (rig.proc, P::secondary, 0);

        auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rig.proc.createEditor()));
        ed->addToDesktop (0);
        ed->setVisible (true);
        pump (100);
        rig.feed (2500);
        pump (100);
        auto* view = findView (*ed);
        check (view != nullptr && view->hasFrame(), "SpectrumView found and has pulled a frame from the processor");
        if (view != nullptr && view->hasFrame())
        {
            const auto& f = view->getFrame();
            const auto m = view->getMapping();
            const auto pk = peakOf (f.primaryDb, f.numPoints);
            const double fPk = f.freqHz[(size_t) pk.index];
            const double xPk = m.freqToX (fPk), xRef = m.freqToX (1000.0);
            const double yPk = m.dbToY (pk.db), yRef = m.dbToY (-6.02);
            note ("frame: numPoints " + juce::String (f.numPoints) + ", peak " + juce::String (fPk, 2) + " Hz, " + juce::String (pk.db, 2) + " dB");
            note ("mapping: x0 " + juce::String (m.x0) + " w " + juce::String (m.w) + " | peak x " + juce::String (xPk, 2) + ", 1 kHz x " + juce::String (xRef, 2)
                  + " | peak y " + juce::String (yPk, 2) + ", -6 dB y " + juce::String (yRef, 2));
            check (std::abs (xPk - xRef) <= 1.0, "peak vertex column within +-1 px of the 1 kHz column (delta " + juce::String (xPk - xRef, 2) + " px)");
            check (pk.db <= -6.0f && pk.db >= -7.5f, "peak level in [-7.5, -6.0] dB (Hann scalloping bound, T-005): " + juce::String (pk.db, 2) + " dB");
            note ("peak level error vs -6.02 dBFS: " + juce::String (pk.db + 6.02, 2) + " dB = " + juce::String (yPk - yRef, 2) + " px");

            // Drawn pixels: topmost primary-A stroke pixel over the data rect.
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            savePng (img, out.getChildFile ("placement-1khz.png"));
            const auto data = view->getDataRect() + view->getPosition();
            int bestX = -1, bestY = 1 << 30;
            for (int x = (int) (view->getPosition().x + xRef) - 40; x < (int) (view->getPosition().x + xRef) + 40; ++x)
                for (int y = data.getY() + 1; y < data.getBottom() - 1; ++y)
                {
                    const auto px = img.getPixelAt (x, y);
                    if (px.getRed() < 130 && px.getGreen() > 170 && px.getBlue() > 190)
                    {
                        if (y < bestY) { bestY = y; bestX = x; }
                        break;
                    }
                }
            const double dx = bestX - (view->getPosition().x + xRef);
            const double dy = bestY - (view->getPosition().y + yPk);
            note ("rendered primary stroke topmost pixel: x " + juce::String (bestX) + ", y " + juce::String (bestY) + " (ref x " + juce::String (view->getPosition().x + xRef, 2) + ")");
            check (std::abs (dx) <= 1.5, "rendered peak column within +-1 px (+0.5 px tolerance for the column centre) of 1 kHz: delta " + juce::String (dx, 2) + " px");
            check (std::abs (dy) <= 2.0, "rendered peak row within 2 px of the frame peak y (stroke half-width): delta " + juce::String (dy, 2) + " px");
        }
    }

    //==========================================================================
    std::printf ("\n== Screenshots, flag combinations and curve colours ==\n");
    {
        struct Case { const char* tag; bool scBus; int mode; int sec; bool showSc; bool expB, expSecA, expSecB, expSc; };
        const Case cases[] = {
            { "01-single-off",              false, 2, 0, true,  false, false, false, false },
            { "02-single-rtmax",            false, 2, 1, true,  false, true,  false, false },
            { "03-single-alltimemax",       false, 2, 2, true,  false, true,  false, false },
            { "04-single-alltimeavg",       false, 2, 3, true,  false, true,  false, false },
            { "05-dual-lr-off",             false, 5, 0, true,  true,  false, false, false },
            { "06-dual-lr-rtmax",           false, 5, 1, true,  true,  true,  true,  false },
            { "07-dual-ms-alltimemax",      false, 6, 2, true,  true,  true,  true,  false },
            { "08-single-sidechain",        true,  2, 0, true,  false, false, false, true  },
            { "09-single-sidechain-hidden", true,  2, 0, false, false, false, false, false },
            { "10-dual-sc-rtmax",           true,  5, 1, true,  true,  true,  true,  true  },
            { "11-single-rtmax-sidechain",  true,  2, 1, true,  false, true,  false, true  },
        };

        std::unique_ptr<Rig> rigs[2];
        std::unique_ptr<InvestigateEditor> eds[2];
        for (int k = 0; k < 2; ++k)
        {
            rigs[k] = std::make_unique<Rig> (k == 1);
            setChoice (rigs[k]->proc, P::window, 0);
            setChoice (rigs[k]->proc, P::smoothing, 3);
            eds[k] = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rigs[k]->proc.createEditor()));
            eds[k]->addToDesktop (0);
            eds[k]->setVisible (true);
        }
        for (auto& c : cases)
        {
            const int k = c.scBus ? 1 : 0;
            auto& rig = *rigs[k];
            setChoice (rig.proc, P::channelMode, c.mode);
            setChoice (rig.proc, P::secondary, c.sec);
            setReal (rig.proc, P::showSidechain, c.showSc ? 1.0f : 0.0f);
            rig.feed (2200);
            pump (60);
            auto img = eds[k]->createComponentSnapshot (eds[k]->getLocalBounds(), true, 1.0f);
            savePng (img, out.getChildFile (juce::String (c.tag) + ".png"));
            auto* view = findView (*eds[k]);
            const auto data = view->getDataRect() + view->getPosition();
            const auto n = countCurves (img, data);
            note (juce::String (c.tag) + ": pixels A " + juce::String (n.primaryA) + ", B " + juce::String (n.primaryB) + ", secA " + juce::String (n.secA)
                  + ", secB " + juce::String (n.secB) + ", SC " + juce::String (n.sidechain)
                  + " | frame isDual " + juce::String ((int) view->getFrame().isDual) + " hasSec " + juce::String ((int) view->getFrame().hasSecondary)
                  + " hasSC " + juce::String ((int) view->getFrame().hasSidechain));
            check (n.primaryA > 100, juce::String (c.tag) + ": primary A drawn");
            check ((n.primaryB > 30) == c.expB, juce::String (c.tag) + ": primary B " + (c.expB ? "drawn" : "absent"));
            check ((n.sidechain > 300) == c.expSc, juce::String (c.tag) + ": sidechain silhouette " + (c.expSc ? "drawn" : "absent"));
        }

        // Hover screenshots on the final (dual + SC + RT max) and a single scenario.
        {
            auto& rig = *rigs[1];
            auto* view = findView (*eds[1]);
            const auto m = view->getMapping();
            view->setHoverForTesting ({ (float) m.freqToX (1000.0), (float) (m.y0 + m.h * 0.45) });
            pump (40);
            savePng (eds[1]->createComponentSnapshot (eds[1]->getLocalBounds(), true, 1.0f), out.getChildFile ("12-hover-dual-sc-rtmax.png"));
            view->setHoverForTesting ({ (float) m.freqToX (12000.0), (float) (m.y0 + m.h * 0.2) });
            pump (40);
            savePng (eds[1]->createComponentSnapshot (eds[1]->getLocalBounds(), true, 1.0f), out.getChildFile ("13-hover-dual-sc-right-edge.png"));
            setChoice (rig.proc, P::channelMode, 2);
            setChoice (rig.proc, P::secondary, 1);
            rig.feed (1500);
            view->setHoverForTesting ({ (float) m.freqToX (1000.0), (float) (m.y0 + m.h * 0.45) });
            pump (40);
            savePng (eds[1]->createComponentSnapshot (eds[1]->getLocalBounds(), true, 1.0f), out.getChildFile ("14-hover-single-sc-rtmax.png"));
            view->setHoverForTesting ({ (float) m.freqToX (25.0), (float) (m.y0 + m.h * 0.05) });
            pump (40);
            savePng (eds[1]->createComponentSnapshot (eds[1]->getLocalBounds(), true, 1.0f), out.getChildFile ("15-hover-single-top-left.png"));
        }

        // Deterministic flag matrix with flat synthetic curves, each on its own pixel
        // row (centre of the row), so a curve's presence is an exact colour test.
        {
            std::printf ("-- synthetic flat-curve flag matrix (exact colours at fixed rows)\n");
            auto& rig = *rigs[1];
            auto* view = findView (*eds[1]);
            view->setHoverForTesting ({ -5.0f, -5.0f });
            setChoice (rig.proc, P::secondary, 1);
            const auto m = view->getMapping();
            const auto vp = view->getPosition();
            const int cx = (int) (m.x0 + m.w * 0.5) + vp.x;
            const int rowA = (int) m.y0 + 100, rowB = (int) m.y0 + 160, rowSA = (int) m.y0 + 220, rowSB = (int) m.y0 + 280, rowSC = (int) m.y0 + 340;
            const auto dbAt = [&] (int row) { return (float) m.yToDb (row + 0.5); };
            const auto isCol = [] (juce::Colour px, int r, int g, int b) { return std::abs (px.getRed() - r) <= 3 && std::abs (px.getGreen() - g) <= 3 && std::abs (px.getBlue() - b) <= 3; };
            auto frame = std::make_unique<SpectrumFrame>();

            juce::Random rndSynth (1);
            struct Flags { bool dual, sec, sc, showSc; const char* name; };
            const Flags matrix[] = {
                { false, false, false, true,  "single, no secondary, no sidechain" },
                { false, true,  false, true,  "single + secondary" },
                { true,  false, false, true,  "dual, no secondary" },
                { true,  true,  false, true,  "dual + 2 secondaries" },
                { false, true,  true,  true,  "single + secondary + sidechain shown" },
                { true,  true,  true,  true,  "dual + 2 secondaries + sidechain shown" },
                { true,  true,  true,  false, "dual + 2 secondaries + sidechain hidden (showSidechain off)" },
            };
            int idx = 0;
            for (auto& f : matrix)
            {
                setReal (rig.proc, P::showSidechain, f.showSc ? 1.0f : 0.0f);
                pump (40);

                const auto render = [&] (bool withSc)
                {
                    makeFrame (*frame, m.fLo, m.fHi, 0, f.dual, f.sec, withSc, rndSynth);
                    for (int i = 0; i < frame->numPoints; ++i)
                    {
                        frame->primaryDb[(size_t) i] = dbAt (rowA);
                        frame->primaryBDb[(size_t) i] = dbAt (rowB);
                        frame->secondaryDb[(size_t) i] = dbAt (rowSA);
                        frame->secondaryBDb[(size_t) i] = dbAt (rowSB);
                        frame->sidechainDb[(size_t) i] = dbAt (rowSC);
                    }
                    view->setFrameForTesting (*frame);
                    return eds[1]->createComponentSnapshot (eds[1]->getLocalBounds(), true, 1.0f);
                };
                const auto img = render (f.sc);
                const auto imgNoSc = render (false);
                if (idx == 3 || idx == 5)
                    savePng (img, out.getChildFile ("20-synth-" + juce::String (idx) + ".png"));

                const auto at = [&] (const juce::Image& im, int row) { return im.getPixelAt (cx, vp.y + row); };
                const bool a = isCol (at (img, rowA), 0x62, 0xD6, 0xF0);
                const bool b = isCol (at (img, rowB), 0xFF, 0xB4, 0x54);
                const bool sa = isCol (at (img, rowSA), 0x3E, 0x8F, 0xA3);
                const bool sb = isCol (at (img, rowSB), 0xA8, 0x7A, 0x3C);
                const auto pxSc = at (img, rowSC + 40), pxNo = at (imgNoSc, rowSC + 40);
                const int dBlue = std::abs (pxSc.getBlue() - pxNo.getBlue()) + std::abs (pxSc.getRed() - pxNo.getRed());
                const bool scDrawn = dBlue >= 15;
                note (juce::String (f.name) + ": A " + juce::String ((int) a) + ", B " + juce::String ((int) b) + ", secA " + juce::String ((int) sa) + ", secB " + juce::String ((int) sb)
                      + ", SC colour delta " + juce::String (dBlue));
                const bool expSc = f.sc && f.showSc;
                check (a, juce::String ("synthetic [") + f.name + "]: primary A line present (#62D6F0)");
                check (b == f.dual, juce::String ("synthetic [") + f.name + "]: primary B line " + (f.dual ? "present (#FFB454)" : "absent"));
                check (sa == f.sec, juce::String ("synthetic [") + f.name + "]: secondary A line " + (f.sec ? "present (#3E8FA3)" : "absent"));
                check (sb == (f.sec && f.dual), juce::String ("synthetic [") + f.name + "]: secondary B line " + ((f.sec && f.dual) ? "present (#A87A3C)" : "absent"));
                check (scDrawn == expSc, juce::String ("synthetic [") + f.name + "]: sidechain silhouette " + (expSc ? "present" : "absent"));
                ++idx;
            }
        }
    }

    //==========================================================================
    std::printf ("\n== Hover, freeze, click, right-click ==\n");
    {
        Rig rig (false);
        setChoice (rig.proc, P::channelMode, 2);
        setChoice (rig.proc, P::secondary, 2); // all-time max
        setChoice (rig.proc, P::smoothing, 0);
        setReal (rig.proc, P::slope, 0.0f);
        setReal (rig.proc, P::avgTime, 0.0f);
        auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rig.proc.createEditor()));
        ed->addToDesktop (0);
        ed->setVisible (true);
        pump (100);
        rig.feed (2000);
        auto* view = findView (*ed);
        const auto m = view->getMapping();

        // Right-click clipboard text vs the independent spec rendition, at several x.
        juce::SystemClipboard::copyTextToClipboard ("sentinel");
        for (double frac : { 0.0, 0.0001, 0.1, 0.25, 0.3333, 0.5, 0.6667, 0.9, 0.999 })
        {
            const float x = (float) (m.x0 + frac * m.w);
            const double f = m.xToFreq (x);
            view->setHoverForTesting ({ x, (float) (m.y0 + 50) });
            clickAt (*view, { x, (float) (m.y0 + 50) }, true);
            const auto expect = specClipboard (f);
            const auto clip = readClipboardExpecting (expect);
            check (clip == expect && clip == view->clipboardTextForX (x),
                   "right-click at " + juce::String (frac, 4) + " of width: f " + juce::String (f, 3) + " Hz -> clipboard \"" + clip + "\", spec \"" + expect + "\"");
        }
        {
            const float x = (float) m.freqToX (1000.0);
            clickAt (*view, { x, (float) (m.y0 + 60) }, true);
            note ("clipboard at the 1 kHz column: \"" + juce::SystemClipboard::getTextFromClipboard() + "\"");
            pump (30);
            savePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), out.getChildFile ("16-toast-copied.png"));
        }
        // Right-click outside the data rect must not copy.
        juce::SystemClipboard::copyTextToClipboard ("sentinel");
        clickAt (*view, { 2.0f, 2.0f }, true);
        check (juce::SystemClipboard::getTextFromClipboard() == "sentinel", "right-click outside the data rect leaves the clipboard untouched");

        // Left-click reset: all-time max curve must drop after the click.
        rig.src.equalLR = true;
        rig.feed (1500);
        const auto before = peakOf (view->getFrame().secondaryDb, view->getFrame().numPoints);
        rig.src.silent = true;
        rig.feed (1800);
        const auto stale = peakOf (view->getFrame().secondaryDb, view->getFrame().numPoints);
        const auto primaryNow = peakOf (view->getFrame().primaryDb, view->getFrame().numPoints);
        check (stale.db > -15.0f && primaryNow.db < stale.db - 20.0f,
               "all-time max keeps the old peak after the signal stops (secondary " + juce::String (stale.db, 1) + " dB, primary " + juce::String (primaryNow.db, 1) + " dB; peak before " + juce::String (before.db, 1) + ")");
        clickAt (*view, { (float) (m.x0 + 100), (float) (m.y0 + 100) }, false);
        const auto clickT = juce::Time::getMillisecondCounterHiRes();
        const auto seq0 = view->getFrame().sequence;
        float after = stale.db;
        double msToReset = -1.0;
        for (int i = 0; i < 40 && msToReset < 0.0; ++i)
        {
            rig.block();
            pump (10);
            const auto& f = view->getFrame();
            if (f.sequence > seq0 && peakOf (f.secondaryDb, f.numPoints).db < stale.db - 20.0f)
            {
                msToReset = juce::Time::getMillisecondCounterHiRes() - clickT;
                after = peakOf (f.secondaryDb, f.numPoints).db;
            }
        }
        check (msToReset >= 0.0, "left-click clears the all-time max curve (peak " + juce::String (stale.db, 1) + " -> " + juce::String (after, 1) + " dB after " + juce::String (msToReset, 0) + " ms)");
        pump (20);
        savePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), out.getChildFile ("17-toast-reset.png"));

        // Drag > 4 px must not reset.
        rig.src.silent = false;
        rig.feed (1500);
        const auto beforeDrag = peakOf (view->getFrame().secondaryDb, view->getFrame().numPoints).db;
        {
            auto src = juce::Desktop::getInstance().getMainMouseSource();
            const auto now = juce::Time::getCurrentTime();
            juce::Point<float> a ((float) (m.x0 + 200), (float) (m.y0 + 100)), b = a + juce::Point<float> (30.0f, 0.0f);
            juce::MouseEvent down (src, a, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0, view, view, now, a, now, 1, false);
            view->mouseDown (down);
            juce::MouseEvent up (src, b, juce::ModifierKeys(), 1, 0, 0, 0, 0, view, view, now, a, now, 1, true);
            view->mouseUp (up);
        }
        rig.src.silent = true;
        rig.feed (800);
        check (peakOf (view->getFrame().secondaryDb, view->getFrame().numPoints).db > -15.0f,
               "left drag (30 px) does not reset the all-time curve (secondary peak " + juce::String (beforeDrag, 1) + " dB before, "
                   + juce::String (peakOf (view->getFrame().secondaryDb, view->getFrame().numPoints).db, 1) + " after)");

        // Freeze
        rig.src.silent = false;
        rig.src.equalLR = false;
        rig.feed (500);
        auto* fb = findHeaderButton (*ed, ed->getWidth() - 302, 8);
        check (fb != nullptr, "Freeze button found");
        if (fb != nullptr)
        {
            const auto s0 = view->getFrame().sequence;
            fb->onClick();
            const auto sClick = view->getFrame().sequence;
            rig.feed (400);
            const auto sFrozen = view->getFrame().sequence;
            check (ed->isFrozen() && sFrozen == sClick, "freeze stops curve updates immediately: sequence " + juce::String ((juce::int64) s0) + " at press, " + juce::String ((juce::int64) sClick) + " after click, " + juce::String ((juce::int64) sFrozen) + " after 400 ms of audio");
            // Hover works on the frozen frame
            const float x = (float) m.freqToX (2000.0);
            view->setHoverForTesting ({ x, (float) (m.y0 + 80) });
            clickAt (*view, { x, (float) (m.y0 + 80) }, true);
            check (juce::SystemClipboard::getTextFromClipboard() == specClipboard (m.xToFreq (x)), "hover and right-click copy work while frozen (clipboard \"" + juce::SystemClipboard::getTextFromClipboard() + "\")");
            pump (30);
            savePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), out.getChildFile ("18-frozen-hover.png"));
            fb->onClick();
            const auto sU = view->getFrame().sequence;
            double ms = -1.0;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int i = 0; i < 30 && ms < 0.0; ++i)
            {
                rig.block();
                pump (10);
                if (view->getFrame().sequence > sU) ms = juce::Time::getMillisecondCounterHiRes() - t0;
            }
            check (! ed->isFrozen() && ms >= 0.0, "unfreeze resumes curve updates (first new frame after " + juce::String (ms, 0) + " ms)");

            fb->onClick();
            pump (5);
            check (ed->isFrozen(), "re-freeze");
            setChoice (rig.proc, P::channelMode, 3);
            pump (40);
            check (! ed->isFrozen(), "changing channelMode while frozen unfreezes");
        }
    }

    //==========================================================================
    benchmark (out, benchFrames, true, false);
    benchmark (out, benchFrames, false, true);

    std::printf ("\n%d checks, %d failed\n", total, failed);
    return failed == 0 ? 0 : 1;
}
