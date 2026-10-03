// T-013 meter panel black-box check (plugin-validator).
//
// Usage: InvestigateMeterCheck <outDir> <mode> [args]
//   func                          processor-driven readout checks through the real editor (exit 0 when all pass)
//   shots                         screenshots (docs/qa/T-013-screenshots) with animated music-like audio
//   paint <WxH> [seconds]         MeterPanel paint cost between z-order probes, real on-screen Direct2D window
//   cpu <WxH@scale> <runs> <caseMask> [seconds]   whole-editor message-thread CPU matrix (T-029 method), interleaved runs
//   cycle <n>                     open/close the editor n times with meters animating
//
// Drives only public surfaces: createEditor, the APVTS, processBlock on a separate
// real-time-paced thread, the MeterPanel readout accessors named in its header, and the
// SpectrumView test hook setFrameForTesting (CPU mode only, as T-029 did, so the
// spectrum animates on every vblank). The meter panel is never updated by this tool:
// the editor's own vblank path (pollProcessor) does that.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "gui/HeaderButton.h"
#include "gui/MeterPanel.h"
#include "gui/SpectrumMath.h"
#include "params/ParameterIDs.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <optional>

#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

using investigate::InvestigateEditor;
using investigate::InvestigateProcessor;
using investigate::MeterPanel;
using investigate::MeterSnapshot;
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

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    // Pumps until cond() or the deadline. Returns the milliseconds taken, or -1 on timeout.
    double waitFor (const std::function<bool()>& cond, int timeoutMs)
    {
        const double t0 = nowMs();
        for (;;)
        {
            if (cond()) return nowMs() - t0;
            if (nowMs() - t0 > timeoutMs) return -1.0;
            pump (1);
        }
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

    template <typename T>
    T* findChild (juce::Component& c)
    {
        if (auto* v = dynamic_cast<T*> (&c)) return v;
        for (auto* ch : c.getChildren())
            if (auto* v = findChild<T> (*ch)) return v;
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
    // Text helpers. The panel prints U+2212 for minus; the dash is U+2014.
    juce::String norm (const juce::String& s)
    {
        return s.replace (juce::String::fromUTF8 ("\xE2\x88\x92"), "-").trim();
    }

    // The numeric value of a cell, or nullopt for dashes and "-inf".
    std::optional<double> num (const juce::String& raw)
    {
        const auto s = norm (raw);
        if (s.isEmpty() || s.containsChar ('i') || s.containsChar ((juce::juce_wchar) 0x2014)) return std::nullopt;
        if (! (s.startsWithChar ('+') || s.startsWithChar ('-') || juce::CharacterFunctions::isDigit (s[0]))) return std::nullopt;
        return s.getDoubleValue();
    }

    juce::String minusInf() { return juce::String::fromUTF8 ("\xE2\x88\x92" "inf"); }
    juce::String dash() { return juce::String::fromUTF8 ("\xE2\x80\x94"); }

    //==========================================================================
    // Music-like programme: kick / snare / hat pattern at 120 bpm, bass, a pad and a noise bed.
    // Peaks sit between about -9 and -1.5 dBFS after calibration; every fifth bar (about every
    // 10 s) the kick is pushed over full scale when clips are allowed.
    struct Music
    {
        static constexpr double sr = 48000.0;
        static constexpr int stepLen = 6000;   // 16th note at 120 bpm
        long long pos = 0;
        double kickT = 1e9, snareT = 1e9, hatT = 1e9, kickPh = 0, bassPh = 0, padPh[6] {}, kickGain = 1.0;
        float prevNoise = 0.0f;
        juce::Random rnd { 7 };
        float gain = 1.0f;
        bool allowClip = false;

        void fill (float* l, float* r, int n)
        {
            const double twoPi = juce::MathConstants<double>::twoPi;
            for (int i = 0; i < n; ++i)
            {
                const long long inStep = pos % stepLen;
                const int step = (int) ((pos / stepLen) % 16);
                const long long bar = pos / (stepLen * 16);
                if (inStep == 0)
                {
                    if (step == 0 || step == 4 || step == 8 || step == 10)
                    {
                        kickT = 0.0;
                        kickGain = (allowClip && step == 0 && bar % 5 == 4) ? 1.9 : 1.0;
                    }
                    if (step == 4 || step == 12) snareT = 0.0;
                    if (step % 2 == 0) hatT = 0.0;
                }
                const double kick = std::sin (kickPh) * std::exp (-kickT / (0.15 * sr)) * 0.8 * kickGain;
                kickPh += twoPi * (48.0 + 110.0 * std::exp (-kickT / (0.03 * sr))) / sr;
                kickT += 1.0;
                const float nz1 = rnd.nextFloat() * 2.0f - 1.0f, nz2 = rnd.nextFloat() * 2.0f - 1.0f;
                const double snEnv = std::exp (-snareT / (0.07 * sr));
                const double snareL = (nz1 * 0.55 + std::sin (twoPi * 185.0 * snareT / sr) * 0.3) * snEnv;
                const double snareR = (nz2 * 0.55 + std::sin (twoPi * 185.0 * snareT / sr) * 0.3) * snEnv;
                snareT += 1.0;
                const double hat = (nz1 - prevNoise) * 0.5 * std::exp (-hatT / (0.02 * sr)) * 0.25;
                prevNoise = nz1;
                hatT += 1.0;
                const double bass = std::sin (bassPh) * 0.28 * (0.6 + 0.4 * std::exp (-(double) inStep / 2500.0));
                bassPh += twoPi * (step < 8 ? 55.0 : 82.4) / sr;
                double padL = 0.0, padR = 0.0;
                const double f0[3] = { 220.0, 277.2, 329.6 };
                for (int k = 0; k < 3; ++k)
                {
                    padL += 0.04 * std::sin (padPh[k]);
                    padR += 0.04 * std::sin (padPh[k + 3]);
                    padPh[k] += twoPi * f0[k] / sr;
                    padPh[k + 3] += twoPi * f0[k] * 1.004 / sr;
                }
                const double bed = 0.01;
                l[i] = gain * (float) (kick + bass + snareL + hat * 0.7 + padL + bed * nz2);
                r[i] = gain * (float) (kick + bass + snareR + hat * 0.3 + padR + bed * nz1);
                ++pos;
            }
        }

        // Scale so the largest unclipped sample of 8 s is 0.85 (-1.4 dBFS).
        void calibrate()
        {
            Music m;
            std::vector<float> a (48000), b (48000);
            float peak = 0.0f;
            for (int s = 0; s < 8; ++s)
            {
                m.fill (a.data(), b.data(), 48000);
                for (int i = 0; i < 48000; ++i) peak = std::max ({ peak, std::abs (a[(size_t) i]), std::abs (b[(size_t) i]) });
            }
            gain = 0.85f / peak;
        }
    };

    //==========================================================================
    // Feeds the processor in real time (one 512-sample block per 10.67 ms) from its own thread.
    class Feeder : public juce::Thread
    {
    public:
        enum Mode { silence = 0, sine = 1, noise = 2, music = 3 };

        Feeder (InvestigateProcessor& p, int channels) : juce::Thread ("meter-check audio"), proc (p), numCh (channels)
        {
            buf.setSize (channels, blockSize);
            mus.calibrate();
        }

        ~Feeder() override { stopThread (2000); }

        std::atomic<int> mode { silence };
        std::atomic<float> ampLDb { -6.0f }, ampRDb { -6.0f };
        std::atomic<int> rSign { 1 };
        std::atomic<float> noiseAmp { 0.2f };
        std::atomic<bool> paused { false };
        std::atomic<bool> clipsAllowed { false };
        std::atomic<int> burst { 0 };          // number of +1.0 samples to put at the head of the next block on channel 0
        std::atomic<int> blocks { 0 };
        std::atomic<double> lastBlockMs { 0.0 };
        std::atomic<float> sineBoost { 1.0f }; // linear factor on the sine amplitude (over-full-scale test)

        void run() override
        {
            double next = nowMs();
            const double blockMs = 1000.0 * blockSize / 48000.0;
            while (! threadShouldExit())
            {
                if (nowMs() < next - 2.0) { juce::Thread::sleep (1); continue; }
                while (nowMs() < next) {}
                next += blockMs;
                if (nowMs() - next > 100.0) next = nowMs() + blockMs;   // resync after a stall
                if (paused.load()) continue;
                generate();
                proc.processBlock (buf, midi);
                lastBlockMs = nowMs();
                ++blocks;
            }
        }

    private:
        static constexpr int blockSize = 512;

        void generate()
        {
            buf.clear();
            auto* l = buf.getWritePointer (0);
            auto* r = numCh > 1 ? buf.getWritePointer (1) : nullptr;
            const int m = mode.load();
            const double twoPi = juce::MathConstants<double>::twoPi;
            if (m == sine)
            {
                const float aL = std::pow (10.0f, ampLDb.load() / 20.0f) * sineBoost.load(), aR = std::pow (10.0f, ampRDb.load() / 20.0f) * sineBoost.load();
                const float sg = (float) rSign.load();
                for (int i = 0; i < blockSize; ++i)
                {
                    const float s = (float) std::sin (ph);
                    l[i] = aL * s;
                    if (r != nullptr) r[i] = sg * aR * s;
                    ph += twoPi * 1000.0 / 48000.0;
                }
            }
            else if (m == noise)
            {
                const float a = noiseAmp.load();
                for (int i = 0; i < blockSize; ++i)
                {
                    l[i] = (rnd.nextFloat() * 2.0f - 1.0f) * a;
                    if (r != nullptr) r[i] = (rnd.nextFloat() * 2.0f - 1.0f) * a;
                }
            }
            else if (m == music)
            {
                mus.allowClip = clipsAllowed.load();
                std::vector<float>& tl = scratchL;
                std::vector<float>& tr = scratchR;
                tl.resize (blockSize);
                tr.resize (blockSize);
                mus.fill (tl.data(), tr.data(), blockSize);
                std::copy (tl.begin(), tl.end(), l);
                if (r != nullptr) std::copy (tr.begin(), tr.end(), r);
            }
            if (numCh > 3)   // sidechain bus: a quieter copy of the programme
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    buf.setSample (2, i, 0.5f * l[i]);
                    buf.setSample (3, i, 0.5f * (r != nullptr ? r[i] : l[i]));
                }
            }
            if (const int n = burst.exchange (0); n > 0)
                for (int i = 0; i < std::min (n, blockSize); ++i) l[i] = 1.0f;
        }

        InvestigateProcessor& proc;
        int numCh;
        juce::AudioBuffer<float> buf;
        juce::MidiBuffer midi;
        juce::Random rnd { 99 };
        Music mus;
        std::vector<float> scratchL, scratchR;
        double ph = 0.0;
    };

    enum class Layout { stereo, stereoSidechain, mono };

    struct Rig
    {
        InvestigateProcessor proc;
        std::unique_ptr<Feeder> feeder;
        Layout layout;

        explicit Rig (Layout l) : layout (l)
        {
            if (l == Layout::mono)
            {
                proc.getBus (true, 0)->setCurrentLayout (juce::AudioChannelSet::mono());
                proc.getBus (false, 0)->setCurrentLayout (juce::AudioChannelSet::mono());
            }
            else
            {
                proc.getBus (true, 0)->setCurrentLayout (juce::AudioChannelSet::stereo());
                proc.getBus (false, 0)->setCurrentLayout (juce::AudioChannelSet::stereo());
                if (l == Layout::stereoSidechain)
                    proc.getBus (true, 1)->setCurrentLayout (juce::AudioChannelSet::stereo());
            }
            proc.setRateAndBufferSizeDetails (48000.0, 512);
            proc.prepareToPlay (48000.0, 512);
            feeder = std::make_unique<Feeder> (proc, proc.getTotalNumInputChannels());
            feeder->startThread (juce::Thread::Priority::high);
        }

        ~Rig()
        {
            feeder->stopThread (2000);
            proc.releaseResources();
        }
    };

    std::unique_ptr<InvestigateEditor> openEditor (InvestigateProcessor& p, int w, int h)
    {
        auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (p.createEditor()));
        ed->setSize (w, h);
        ed->addToDesktop (0);
        ed->setVisible (true);
        // A window wider than the display is shifted left so that the meter strip (right edge) is on screen.
        if (const auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            if (const auto ub = d->userBounds.toNearestInt(); ed->getScreenBounds().getRight() > ub.getRight())
                ed->setTopLeftPosition (ub.getRight() - ed->getWidth(), ub.getY());
        pump (300);
        return ed;
    }

    struct Stats { double median = 0, p95 = 0, worst = 0, mean = 0; int n = 0; };

    Stats stats (std::vector<double> v)
    {
        Stats s;
        s.n = (int) v.size();
        if (v.empty()) return s;
        std::sort (v.begin(), v.end());
        s.median = v[v.size() / 2];
        s.p95 = v[(size_t) std::min<double> ((double) v.size() - 1.0, std::ceil (0.95 * (double) v.size()) - 1.0)];
        s.worst = v.back();
        for (double x : v) s.mean += x;
        s.mean /= (double) v.size();
        return s;
    }

    void disableEcoQoS()
    {
        PROCESS_POWER_THROTTLING_STATE st {};
        st.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        st.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        st.StateMask = 0;
        SetProcessInformation (GetCurrentProcess(), ProcessPowerThrottling, &st, sizeof (st));
        THREAD_POWER_THROTTLING_STATE ts {};
        ts.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
        ts.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
        ts.StateMask = 0;
        SetThreadInformation (GetCurrentThread(), ThreadPowerThrottling, &ts, sizeof (ts));
        SetPriorityClass (GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    }

    //==========================================================================
    // Functional checks
    //==========================================================================
    struct Fx
    {
        Rig& rig;
        InvestigateEditor& ed;
        MeterPanel& mp;
        Feeder& fd;

        juce::String cell (int row, int ch) const { return mp.getCellText (MeterPanel::tableCellIndex (row, ch)); }
        juce::String loud (int c) const { return mp.getCellText (MeterPanel::loudnessCellIndex (c)); }
        juce::String corr() const { return mp.getCellText (MeterPanel::correlationCellIndex); }
        juce::String bal() const { return mp.getCellText (MeterPanel::balanceCellIndex); }
        MeterSnapshot snap() const { return rig.proc.getMeterSnapshot(); }

        void clickReset()
        {
            if (auto* b = findHeaderButton (mp, 216, 12)) b->onClick();
        }

        void sine (float lDb, float rDb, int rSign = 1)
        {
            fd.ampLDb = lDb;
            fd.ampRDb = rDb;
            fd.rSign = rSign;
            fd.mode = Feeder::sine;
        }

        void dumpAll (const juce::String& tag) const
        {
            juce::String s = tag + ": ";
            const char* rowName[] = { "peak", "pkMax", "TP", "TPmax", "head", "rms", "crest", "clips", "tpClips" };
            for (int r = 0; r < MeterPanel::numTableRows; ++r)
                s << rowName[r] << " " << cell (r, 0) << "/" << cell (r, 1) << "  ";
            s << "| M " << loud (MeterPanel::loudM) << " S " << loud (MeterPanel::loudS) << " I " << loud (MeterPanel::loudI)
              << " Mmax " << loud (MeterPanel::loudMMax) << " Smax " << loud (MeterPanel::loudSMax) << " LRA " << loud (MeterPanel::loudLra)
              << " | corr " << corr() << " bal " << bal();
            note (s);
        }
    };

    bool cellIs (const juce::String& got, const juce::String& want) { return got == want; }

    void applyDisplayRate();

    int functional (const juce::File& outDir)
    {
        (void) outDir;
        applyDisplayRate();
        std::printf ("== Processor-driven readouts through the real editor (Direct2D window on the desktop, audio on its own paced thread) ==\n");
        timeBeginPeriod (1);
        disableEcoQoS();

        {
            Rig rig (Layout::stereo);
            auto ed = openEditor (rig.proc, 1200, 700);
            auto* mpp = findChild<MeterPanel> (*ed);
            check (mpp != nullptr, "MeterPanel found in the editor");
            if (mpp == nullptr) return 1;
            Fx f { rig, *ed, *mpp, *rig.feeder };
            auto& mp = *mpp;
            auto& fd = *rig.feeder;
            note ("editor " + juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()) + ", meter panel " + juce::String (mp.getWidth()) + "x" + juce::String (mp.getHeight())
                  + " at " + juce::String (mp.getX()) + "," + juce::String (mp.getY()));

            //----------------------------------------------------------------------
            std::printf ("-- stereo -23 dBFS 1 kHz sine: (A) from a fresh processor, 3.5 s after the sine starts (information)\n");
            f.clickReset();
            f.sine (-23.0f, -23.0f);
            pump (3500);
            f.dumpAll ("A: fresh start, t=3.5 s");
            note ("A: snapshot M " + juce::String (f.snap().momentary, 3) + " S " + juce::String (f.snap().shortTerm, 3) + " I " + juce::String (f.snap().integrated, 3) + " LRA " + juce::String (f.snap().loudnessRange, 3)
                  + " (I and LRA include the 400 ms blocks that straddle the sine onset, which BS.1770 gating keeps)");
            std::printf ("-- (B) steady sine for 4 s, then Reset, then 3.5 s: M, S and I are measured from steady blocks only\n");
            pump (500);
            f.clickReset();
            pump (3500);
            f.dumpAll ("B: 3.5 s after reset");
            const auto m = num (f.loud (MeterPanel::loudM)), s = num (f.loud (MeterPanel::loudS)), in = num (f.loud (MeterPanel::loudI));
            check (m && std::abs (*m + 23.0) <= 0.1, "M reads -23.0 +-0.1 LUFS: " + f.loud (MeterPanel::loudM));
            check (s && std::abs (*s + 23.0) <= 0.1, "S reads -23.0 +-0.1 LUFS: " + f.loud (MeterPanel::loudS));
            check (in && std::abs (*in + 23.0) <= 0.1, "I reads -23.0 +-0.1 LUFS: " + f.loud (MeterPanel::loudI));
            {
                const auto sn = f.snap();
                note ("B: snapshot M " + juce::String (sn.momentary, 3) + " S " + juce::String (sn.shortTerm, 3) + " I " + juce::String (sn.integrated, 3) + " LRA " + juce::String (sn.loudnessRange, 3));
                check (std::abs (*in - (double) sn.integrated) <= 0.051 && std::abs (*s - (double) sn.shortTerm) <= 0.051, "cells equal the snapshot values to 1 decimal");
                const auto mx = num (f.loud (MeterPanel::loudMMax)), sx = num (f.loud (MeterPanel::loudSMax));
                check (mx && std::abs (*mx + 23.0) <= 0.2, "M max near -23.0: " + f.loud (MeterPanel::loudMMax));
                check (sx.has_value() && std::abs (*sx + 23.0) <= 0.2, "S max near -23.0: " + f.loud (MeterPanel::loudSMax));
                check (num (f.loud (MeterPanel::loudLra)).has_value() && std::abs (*num (f.loud (MeterPanel::loudLra))) <= 0.3, "LRA near 0 LU for a steady tone: " + f.loud (MeterPanel::loudLra));
            }

            //----------------------------------------------------------------------
            std::printf ("-- stereo -6 dBFS 1 kHz sine, dBFS mode, RMS window 300 ms\n");
            setChoice (rig.proc, P::meterScale, 0);
            setReal (rig.proc, P::rmsWindow, 300.0f);
            f.clickReset();
            f.sine (-6.0f, -6.0f);
            pump (1800);
            f.dumpAll ("-6 dBFS dBFS");
            for (int ch = 0; ch < 2; ++ch)
            {
                const juce::String c = ch == 0 ? "L" : "R";
                const auto mn = juce::String::fromUTF8 ("\xE2\x88\x92");
                check (f.cell (MeterPanel::rowPeak, ch) == mn + "6.0", c + " peak readout is -6.0 dB: " + f.cell (MeterPanel::rowPeak, ch));
                check (f.cell (MeterPanel::rowRms, ch) == mn + "9.0", c + " RMS readout is -9.0 dB: " + f.cell (MeterPanel::rowRms, ch));
                check (f.cell (MeterPanel::rowCrest, ch) == "3.0", c + " crest is 3.0 dB: " + f.cell (MeterPanel::rowCrest, ch));
                check (f.cell (MeterPanel::rowPeakMax, ch) == mn + "6.0", c + " peak max is -6.0 dB: " + f.cell (MeterPanel::rowPeakMax, ch));
            }

            std::printf ("-- K scales, same -6 dBFS sine: RMS = reported + 3.01 + N\n");
            const struct { int idx; const char* name; const char* expect; } ks[] = { { 1, "K-20", "+14.0" }, { 2, "K-14", "+8.0" }, { 3, "K-12", "+6.0" } };
            for (auto& k : ks)
            {
                setChoice (rig.proc, P::meterScale, k.idx);
                pump (400);
                check (f.cell (MeterPanel::rowRms, 0) == k.expect && f.cell (MeterPanel::rowRms, 1) == k.expect,
                       juce::String (k.name) + " RMS cell = " + k.expect + ": " + f.cell (MeterPanel::rowRms, 0) + " / " + f.cell (MeterPanel::rowRms, 1));
                check (f.cell (MeterPanel::rowPeak, 0) == juce::String::fromUTF8 ("\xE2\x88\x92") + "6.0", juce::String (k.name) + " peak cell unchanged by scale (still dBFS): " + f.cell (MeterPanel::rowPeak, 0));
            }
            std::printf ("-- -20 dBFS peak sine: 0 on K-20, -6 on K-14, -8 on K-12, -23.0 in dBFS\n");
            setChoice (rig.proc, P::meterScale, 1);
            f.clickReset();
            f.sine (-20.0f, -20.0f);
            pump (1800);
            check (f.cell (MeterPanel::rowRms, 0) == "0.0", "K-20 RMS of a -20 dBFS sine reads 0.0: " + f.cell (MeterPanel::rowRms, 0));
            setChoice (rig.proc, P::meterScale, 2);
            pump (400);
            check (f.cell (MeterPanel::rowRms, 0) == juce::String::fromUTF8 ("\xE2\x88\x92") + "6.0", "K-14 RMS of a -20 dBFS sine reads -6.0: " + f.cell (MeterPanel::rowRms, 0));
            setChoice (rig.proc, P::meterScale, 3);
            pump (400);
            check (f.cell (MeterPanel::rowRms, 0) == juce::String::fromUTF8 ("\xE2\x88\x92") + "8.0", "K-12 RMS of a -20 dBFS sine reads -8.0: " + f.cell (MeterPanel::rowRms, 0));
            setChoice (rig.proc, P::meterScale, 0);
            pump (400);
            check (f.cell (MeterPanel::rowRms, 0) == juce::String::fromUTF8 ("\xE2\x88\x92") + "23.0", "dBFS RMS of a -20 dBFS sine reads -23.0: " + f.cell (MeterPanel::rowRms, 0));

            //----------------------------------------------------------------------
            std::printf ("-- headroom = 0 - TP max\n");
            f.clickReset();
            f.sine (-6.0f, -6.0f);
            pump (1200);
            {
                const auto sn = f.snap();
                for (int ch = 0; ch < 2; ++ch)
                {
                    const double want = 0.0 - (double) sn.truePeakMax[(size_t) ch];
                    const auto got = num (f.cell (MeterPanel::rowHeadroom, ch));
                    check (got && std::abs (*got - want) <= 0.051, "headroom ch" + juce::String (ch) + " = 0 - TP max (" + juce::String (sn.truePeakMax[(size_t) ch], 2) + " dBTP -> " + juce::String (want, 2) + "): " + f.cell (MeterPanel::rowHeadroom, ch));
                    const auto tpm = num (f.cell (MeterPanel::rowTruePeakMax, ch));
                    check (tpm && std::abs (*tpm - (double) sn.truePeakMax[(size_t) ch]) <= 0.051, "TP max cell = snapshot truePeakMax: " + f.cell (MeterPanel::rowTruePeakMax, ch));
                }
            }
            f.sine (1.6f, 1.6f);   // peaks about +1.6 dBFS (amplitude 1.2)
            pump (1200);
            {
                const auto sn = f.snap();
                const auto hd = f.cell (MeterPanel::rowHeadroom, 0);
                const double want = 0.0 - (double) sn.truePeakMax[0];
                note ("over-full-scale sine: TP max " + juce::String (sn.truePeakMax[0], 2) + " dBTP, headroom cell " + hd);
                check (hd.startsWith (juce::String::fromUTF8 ("\xE2\x88\x92")) && std::abs (*num (hd) - want) <= 0.051, "negative headroom is shown with U+2212 and equals 0 - TP max: " + hd);
            }

            //----------------------------------------------------------------------
            std::printf ("-- balance: L -6 dBFS, R -12 dBFS, after 3.5 s\n");
            f.clickReset();
            f.sine (-6.0f, -12.0f);
            pump (3500);
            f.dumpAll ("balance");
            check (f.bal() == "L 6.0 dB", "balance reads \"L 6.0 dB\" (snapshot " + juce::String (f.snap().balance, 3) + "): " + f.bal());
            f.sine (-12.0f, -6.0f);
            pump (3500);
            check (f.bal() == "R 6.0 dB", "swapped channels read \"R 6.0 dB\": " + f.bal());
            f.sine (-6.0f, -6.0f);
            pump (3500);
            check (f.bal() == "C 0.0 dB", "equal levels read \"C 0.0 dB\": " + f.bal());

            //----------------------------------------------------------------------
            std::printf ("-- correlation\n");
            f.sine (-12.0f, -12.0f, 1);
            pump (1500);
            check (mp.getCorrelationShown() >= 0.99f && f.corr() == "+1.00", "L = R: bar value " + juce::String (mp.getCorrelationShown(), 3) + ", text " + f.corr());
            f.sine (-12.0f, -12.0f, -1);
            pump (1500);
            check (mp.getCorrelationShown() <= -0.99f && f.corr() == juce::String::fromUTF8 ("\xE2\x88\x92") + "1.00", "L = -R: bar value " + juce::String (mp.getCorrelationShown(), 3) + ", text " + f.corr());
            fd.noiseAmp = 0.2f;
            fd.mode = Feeder::noise;
            pump (2000);
            check (std::abs (mp.getCorrelationShown()) <= 0.1f, "independent noise: bar value " + juce::String (mp.getCorrelationShown(), 3) + ", text " + f.corr() + " (about 0)");
            note ("independent noise snapshot correlation " + juce::String (f.snap().correlation, 3));
            // display filter: a +1 to -1 step must not jump at once (500 ms DSP window and 50 ms filter)
            f.sine (-12.0f, -12.0f, 1);
            pump (1500);
            const float before = mp.getCorrelationShown();
            f.sine (-12.0f, -12.0f, -1);
            float sMin = 2.0f;
            double tCross = -1.0;
            const double tStep = nowMs();
            waitFor ([&]
            {
                sMin = std::min (sMin, mp.getCorrelationShown());
                if (tCross < 0.0 && mp.getCorrelationShown() < 0.0f) tCross = nowMs() - tStep;
                return mp.getCorrelationShown() <= -0.99f;
            }, 3000);
            note ("step +1 -> -1: before " + juce::String (before, 3) + ", crossed zero after " + juce::String (tCross, 0) + " ms, reached -0.99 after " + juce::String (nowMs() - tStep, 0) + " ms, min " + juce::String (sMin, 3));
            check (tCross > 100.0, "the +1 to -1 step crosses zero no earlier than 100 ms (500 ms DSP window + 50 ms display filter)");

            //----------------------------------------------------------------------
            std::printf ("-- ballistics through the processor (silence after a -6 dBFS sine)\n");
            f.clickReset();
            f.sine (-6.0f, -6.0f);
            pump (800);
            fd.mode = Feeder::silence;
            const double tStop = nowMs();
            struct Pt { double t; float bar, hold; };
            std::vector<Pt> pts;
            while (nowMs() - tStop < 3200.0)
            {
                pump (20);
                pts.push_back ({ nowMs() - tStop, mp.getBarPeakDb (0), mp.getHeldPeakDb (0) });
            }
            const auto at = [&] (double t) { Pt best = pts.front(); for (auto& p : pts) if (std::abs (p.t - t) < std::abs (best.t - t)) best = p; return best; };
            const auto p0 = at (150.0), p1 = at (1150.0);
            const double fall = (p0.bar - p1.bar) / ((p1.t - p0.t) / 1000.0);
            note ("bar at " + juce::String (p0.t, 0) + " ms " + juce::String (p0.bar, 2) + " dB, at " + juce::String (p1.t, 0) + " ms " + juce::String (p1.bar, 2) + " dB -> fall " + juce::String (fall, 1) + " dB/s");
            check (std::abs (fall - 20.0) <= 2.0, "peak bar falls at 20 dB/s: " + juce::String (fall, 1));
            const auto h0 = at (150.0), h1 = at (1300.0);
            check (std::abs (h0.hold - h1.hold) <= 0.1f, "peak-hold tick holds for the first 1.3 s: " + juce::String (h0.hold, 2) + " -> " + juce::String (h1.hold, 2) + " dB");
            const auto g0 = at (2000.0), g1 = at (3000.0);
            const double hfall = (g0.hold - g1.hold) / ((g1.t - g0.t) / 1000.0);
            note ("hold at " + juce::String (g0.t, 0) + " ms " + juce::String (g0.hold, 2) + ", at " + juce::String (g1.t, 0) + " ms " + juce::String (g1.hold, 2) + " -> " + juce::String (hfall, 1) + " dB/s");
            check (std::abs (hfall - 30.0) <= 3.0, "hold tick falls at 30 dB/s after 1.5 s: " + juce::String (hfall, 1));
            check (g1.hold >= g1.bar, "hold tick never below the bar");

            //----------------------------------------------------------------------
            std::printf ("-- clip lamps, counts, reset\n");
            f.clickReset();
            fd.mode = Feeder::silence;
            pump (600);
            check (! mp.isCellLit (MeterPanel::tableCellIndex (MeterPanel::rowClips, 0)) && ! mp.getBarPixels (0).lampLit && ! mp.getBarPixels (1).lampLit, "no lamp lit after reset and silence");
            for (int k = 0; k < 3; ++k) { fd.burst = 5; pump (120); }
            pump (300);
            {
                const auto sn = f.snap();
                f.dumpAll ("clipped");
                note ("snapshot sample clips " + juce::String ((juce::int64) sn.sampleClipCount[0]) + "/" + juce::String ((juce::int64) sn.sampleClipCount[1])
                      + ", TP clips " + juce::String ((juce::int64) sn.truePeakClipCount[0]) + "/" + juce::String ((juce::int64) sn.truePeakClipCount[1]));
                check (mp.getBarPixels (0).lampLit && ! mp.getBarPixels (1).lampLit, "left lamp latched, right lamp off (clips only on L)");
                check (f.cell (MeterPanel::rowClips, 0) == juce::String ((juce::int64) sn.sampleClipCount[0]) && sn.sampleClipCount[0] == 15, "sample clip count cell = snapshot (15 expected from 3 x 5 samples): " + f.cell (MeterPanel::rowClips, 0));
                check (f.cell (MeterPanel::rowTpClips, 0) == juce::String ((juce::int64) sn.truePeakClipCount[0]), "true-peak clip count cell = snapshot: " + f.cell (MeterPanel::rowTpClips, 0) + " (snapshot " + juce::String ((juce::int64) sn.truePeakClipCount[0]) + ")");
                check (f.cell (MeterPanel::rowClips, 1) == "0" && f.cell (MeterPanel::rowTpClips, 1) == "0", "R counts stay 0");
                check (mp.isCellLit (MeterPanel::tableCellIndex (MeterPanel::rowClips, 0)) && ! mp.isCellLit (MeterPanel::tableCellIndex (MeterPanel::rowClips, 1)), "Clips cell of L is a lamp, R is not");
            }
            pump (3500);   // the lamp latches (no timeout); by now the 3 s windows hold only silence
            check (mp.getBarPixels (0).lampLit, "lamp stays latched 3.5 s later");
            f.dumpAll ("clipped, 4 s of silence later");

            // The reset: press the panel's Reset button.
            std::printf ("-- reset (Reset button): time from the processor clearing to the panel clearing\n");
            const auto coreCleared = [&]
            {
                if (mp.getBarPixels (0).lampLit || mp.getBarPixels (1).lampLit || mp.isCellLit (MeterPanel::tableCellIndex (MeterPanel::rowClips, 0))) return false;
                if (f.cell (MeterPanel::rowClips, 0) != "0" || f.cell (MeterPanel::rowTpClips, 0) != "0") return false;
                if (f.cell (MeterPanel::rowPeakMax, 0) != minusInf() || f.cell (MeterPanel::rowTruePeakMax, 0) != minusInf()) return false;
                return f.cell (MeterPanel::rowHeadroom, 0) == dash();
            };
            // M max / S max restart from the live M / S, which can be a tiny finite number (for example -148 LUFS) in silence:
            // a dash or any value below -99.9 counts as cleared here, and the finite case is reported separately.
            const auto heldLoudCleared = [&] (const juce::String& t) { if (t == dash() || t == minusInf()) return true; const auto v = num (t); return v.has_value() && *v < -99.9; };
            const auto allCleared = [&]
            {
                return coreCleared() && heldLoudCleared (f.loud (MeterPanel::loudMMax)) && heldLoudCleared (f.loud (MeterPanel::loudSMax))
                    && f.loud (MeterPanel::loudI) == dash() && f.loud (MeterPanel::loudLra) == dash();
            };
            const auto snapCleared = [&]
            {
                const auto sn = f.snap();
                return sn.sampleClipCount[0] == 0 && sn.truePeakClipCount[0] == 0 && ! std::isfinite (sn.peakMax[0]) && ! std::isfinite (sn.truePeakMax[0])
                    && ! std::isfinite (sn.integrated) && ! (sn.maxMomentary > -99.9f) && ! (sn.maxShortTerm > -99.9f);
            };
            check (! allCleared(), "before the reset the panel shows a lit lamp, counts and non-empty maxima");
            f.clickReset();
            const double tClick = nowMs();
            double tSnap = -1.0, tPanel = -1.0, tLamp = -1.0, tCounts = -1.0, tMax = -1.0, tLoud = -1.0;
            waitFor ([&]
            {
                const double t = nowMs();
                if (tSnap < 0.0 && snapCleared()) tSnap = t;
                if (tLamp < 0.0 && ! mp.getBarPixels (0).lampLit && ! mp.getBarPixels (1).lampLit) tLamp = t;
                if (tCounts < 0.0 && f.cell (MeterPanel::rowClips, 0) == "0" && f.cell (MeterPanel::rowTpClips, 0) == "0" && ! mp.isCellLit (MeterPanel::tableCellIndex (MeterPanel::rowClips, 0))) tCounts = t;
                if (tMax < 0.0 && f.cell (MeterPanel::rowPeakMax, 0) == minusInf() && f.cell (MeterPanel::rowTruePeakMax, 0) == minusInf() && f.cell (MeterPanel::rowHeadroom, 0) == dash()) tMax = t;
                if (tLoud < 0.0 && heldLoudCleared (f.loud (MeterPanel::loudMMax)) && heldLoudCleared (f.loud (MeterPanel::loudSMax)) && f.loud (MeterPanel::loudI) == dash() && f.loud (MeterPanel::loudLra) == dash()) tLoud = t;
                if (tPanel < 0.0 && allCleared()) tPanel = t;
                return tSnap >= 0.0 && tPanel >= 0.0;
            }, 1000);
            note ("breakdown after the snapshot cleared (ms): lamps " + juce::String (tLamp - tSnap, 1) + ", clip/TP-clip counts " + juce::String (tCounts - tSnap, 1)
                  + ", peak max / TP max / headroom " + juce::String (tMax - tSnap, 1) + ", M max / S max / I / LRA " + juce::String (tLoud - tSnap, 1));
            check (tLamp >= 0.0 && tLamp - tSnap <= 17.0, "reset: the lamps clear within 1 UI frame of the snapshot (" + juce::String (tLamp - tSnap, 1) + " ms)");
            note ("click -> snapshot cleared " + juce::String (tSnap - tClick, 1) + " ms, click -> panel cleared " + juce::String (tPanel - tClick, 1) + " ms");
            f.dumpAll ("after reset");
            const auto silentForm = [&] (const juce::String& t) { return t == minusInf() || t == dash(); };   // ui-spec 8.4: "-inf" below -99.9, dash when non-finite
            check (silentForm (f.loud (MeterPanel::loudMMax)) && silentForm (f.loud (MeterPanel::loudSMax)),
                   "O-3: after the reset in silence M max reads \"" + f.loud (MeterPanel::loudMMax) + "\" and S max \"" + f.loud (MeterPanel::loudSMax)
                   + "\" (snapshot " + juce::String (f.snap().maxMomentary, 1) + " / " + juce::String (f.snap().maxShortTerm, 1) + "), expected the silent form");
            check (tSnap >= 0.0 && tPanel >= 0.0, "lamps, counts, peak max, TP max, headroom, M max, S max, I and LRA all clear (found within the 1 s deadline)");
            if (tSnap >= 0.0 && tPanel >= 0.0)
                check (tPanel - tSnap <= 17.0, "panel fully cleared " + juce::String (tPanel - tSnap, 1) + " ms after the snapshot showed the reset (1 UI frame = 16.7 ms)");
            // Repeat for the distribution (lamps, counts, peak max, TP max, headroom).
            std::vector<double> lags, lampLags;
            int timeouts = 0;
            for (int i = 0; i < 12; ++i)
            {
                fd.burst = 3;
                pump (400);
                f.clickReset();
                double ts = -1.0, tp = -1.0;
                const auto snapCore = [&] { const auto sn = f.snap(); return sn.sampleClipCount[0] == 0 && sn.truePeakClipCount[0] == 0 && ! std::isfinite (sn.peakMax[0]) && ! std::isfinite (sn.truePeakMax[0]); };
                double tl = -1.0;
                waitFor ([&]
                {
                    const double t = nowMs();
                    if (ts < 0.0 && snapCore()) ts = t;
                    if (tl < 0.0 && ! mp.getBarPixels (0).lampLit) tl = t;
                    if (tp < 0.0 && coreCleared()) tp = t;
                    return ts >= 0.0 && tp >= 0.0;
                }, 1000);
                if (ts < 0.0 || tp < 0.0 || tl < 0.0) ++timeouts; else { lags.push_back (tp - ts); lampLags.push_back (tl - ts); }
                pump (150);
            }
            const auto lg = stats (lags);
            { juce::String l; int late = 0; for (double v : lags) { l << juce::String (v, 0) << " "; if (v > 17.0) ++late; } note ("all 12 lags (ms): " + l + "| above one frame: " + juce::String (late)); }
            note ("12 repeats: snapshot-cleared to panel-cleared median " + juce::String (lg.median, 1) + " ms, worst " + juce::String (lg.worst, 1) + " ms, timeouts " + juce::String (timeouts));
            const auto ll = stats (lampLags);
            note ("12 repeats, lamp only: median " + juce::String (ll.median, 1) + " ms, worst " + juce::String (ll.worst, 1) + " ms");
            check (timeouts == 0 && ll.worst <= 17.0, "reset: the lamp clears within 1 UI frame in every repeat (worst " + juce::String (ll.worst, 1) + " ms)");
            check (timeouts == 0 && lg.worst <= 17.0, "reset: every repeat clears the lamps, counts and maxima (text included) within 1 UI frame (worst " + juce::String (lg.worst, 1) + " ms)");

            // Reset through the API path: processor request + panel reset.
            fd.burst = 4;
            pump (400);
            check (mp.getBarPixels (0).lampLit, "lamp lit again before the API reset");
            rig.proc.requestStatsReset();
            mp.resetHeldValues();
            check (waitFor (coreCleared, 1000) >= 0.0, "requestStatsReset + resetHeldValues clears lamps, counts and maxima (deadline 1 s)");

            // Reset while the signal keeps playing: the lamps and counts clear, the maxima restart from the live signal.
            f.sine (-6.0f, -6.0f);
            fd.burst = 4;
            pump (1500);
            check (f.cell (MeterPanel::rowPeakMax, 0) == "0.0", "peak max shows the clip peak 0.0 before the reset: " + f.cell (MeterPanel::rowPeakMax, 0));
            f.clickReset();
            pump (500);
            check (! mp.getBarPixels (0).lampLit && f.cell (MeterPanel::rowClips, 0) == "0" && f.cell (MeterPanel::rowPeakMax, 0) == juce::String::fromUTF8 ("\xE2\x88\x92") + "6.0",
                   "reset during playback: lamp off, clips 0, peak max restarts at the live -6.0: " + f.cell (MeterPanel::rowPeakMax, 0));

            //----------------------------------------------------------------------
            std::printf ("-- stale rule (stop calling processBlock)\n");
            f.sine (-6.0f, -6.0f);
            pump (4000);
            f.clickReset();
            fd.burst = 2;
            pump (6000);   // steady state: every 3 s window holds the same sine, so I, LRA, M max and S max no longer move
            const auto liveBefore = f.cell (MeterPanel::rowPeak, 0);
            const struct { const char* n; juce::String v; } heldBefore[] = {
                { "peak max", f.cell (MeterPanel::rowPeakMax, 0) }, { "TP max", f.cell (MeterPanel::rowTruePeakMax, 0) }, { "headroom", f.cell (MeterPanel::rowHeadroom, 0) },
                { "clips", f.cell (MeterPanel::rowClips, 0) }, { "TP clips", f.cell (MeterPanel::rowTpClips, 0) }, { "I", f.loud (MeterPanel::loudI) },
                { "LRA", f.loud (MeterPanel::loudLra) }, { "M max", f.loud (MeterPanel::loudMMax) }, { "S max", f.loud (MeterPanel::loudSMax) },
                { "correlation", f.corr() }, { "balance", f.bal() } };
            note ("live peak before stop: " + liveBefore + ", correlation " + f.corr() + ", I " + f.loud (MeterPanel::loudI));
            check (! mp.isStale(), "not stale while blocks arrive");
            const float barBefore = mp.getBarPeakDb (0);
            fd.paused = true;
            const double lastBlock = fd.lastBlockMs.load();
            pump (150);
            check (! mp.isStale(), "not stale 150 ms after the last block");
            const double tStale = waitFor ([&] { return mp.isStale(); }, 1000);
            const double sinceLast = nowMs() - lastBlock;
            note ("stale flag observed " + juce::String (sinceLast, 0) + " ms after the last processBlock (rule: 250 ms; the detector samples once per vblank, 16.7 ms)");
            check (tStale >= 0.0 && sinceLast >= 249.0 && sinceLast <= 285.0, "stale raised 250 to 285 ms after the last block: " + juce::String (sinceLast, 0) + " ms");
            pump (400);
            f.dumpAll ("stale, 0.7 s after the last block");
            for (int ch = 0; ch < 2; ++ch)
                check (f.cell (MeterPanel::rowRms, ch) == minusInf() && f.loud (MeterPanel::loudM) == minusInf() && f.loud (MeterPanel::loudS) == minusInf(),
                       "stale: RMS (ch" + juce::String (ch) + "), M and S read -inf");
            for (int ch = 0; ch < 2; ++ch)
                note ("stale: peak cell ch" + juce::String (ch) + " " + f.cell (MeterPanel::rowPeak, ch) + ", TP cell " + f.cell (MeterPanel::rowTruePeak, ch)
                      + " (the peak and TP readouts are the hold-tick values of ui-spec 8.3, which hold 1.5 s and then fall at 30 dB/s)");
            const struct { const char* n; juce::String v; } heldAfter[] = {
                { "peak max", f.cell (MeterPanel::rowPeakMax, 0) }, { "TP max", f.cell (MeterPanel::rowTruePeakMax, 0) }, { "headroom", f.cell (MeterPanel::rowHeadroom, 0) },
                { "clips", f.cell (MeterPanel::rowClips, 0) }, { "TP clips", f.cell (MeterPanel::rowTpClips, 0) }, { "I", f.loud (MeterPanel::loudI) },
                { "LRA", f.loud (MeterPanel::loudLra) }, { "M max", f.loud (MeterPanel::loudMMax) }, { "S max", f.loud (MeterPanel::loudSMax) },
                { "correlation", f.corr() }, { "balance", f.bal() } };
            for (size_t i = 0; i < std::size (heldBefore); ++i)
                check (heldBefore[i].v == heldAfter[i].v, juce::String ("stale: ") + heldBefore[i].n + " holds (" + heldBefore[i].v + " -> " + heldAfter[i].v + ")");
            check (mp.getBarPeakDb (0) < barBefore - 5.0f, "stale: the bar falls at its ballistics (" + juce::String (barBefore, 1) + " -> " + juce::String (mp.getBarPeakDb (0), 1) + " dB)");
            const double tPeakInf = waitFor ([&] { return f.cell (MeterPanel::rowPeak, 0) == minusInf() && f.cell (MeterPanel::rowTruePeak, 0) == minusInf(); }, 9000);
            note ("stale: the peak and TP readouts reach -inf " + juce::String (nowMs() - lastBlock, 0) + " ms after the last block (hold 1.5 s, then 30 dB/s down to -120 dBFS = about 5.5 s)");
            check (tPeakInf >= 0.0, "stale: the peak and TP readouts eventually read -inf (deadline 9 s)");
            fd.paused = false;
            const double rec = waitFor ([&] { return ! mp.isStale(); }, 1000);
            check (rec >= 0.0 && rec <= 40.0, "recovery on the first counter change (within 2 vblanks): " + juce::String (rec, 1) + " ms");

            std::printf ("-- silence with processBlock still running: bars fall to the floor, text -inf, maxima hold, not stale\n");
            f.sine (-6.0f, -6.0f);
            pump (800);
            fd.mode = Feeder::silence;
            pump (2500);
            check (! mp.isStale(), "not stale while silence blocks arrive");
            check (mp.getBarPeakDb (0) < -30.0f, "bar is falling: " + juce::String (mp.getBarPeakDb (0), 1) + " dB 2.5 s after the signal stopped");
            f.dumpAll ("silence 2.5 s");
            check (f.cell (MeterPanel::rowRms, 0) == minusInf() && f.loud (MeterPanel::loudM) == minusInf(), "silence: RMS and M read -inf");
            check (waitFor ([&] { return f.cell (MeterPanel::rowPeak, 0) == minusInf() && mp.getBarPeakDb (0) < -59.0f; }, 9000) >= 0.0, "silence: peak readout reaches -inf and the bar the floor (deadline 9 s)");
            check (f.cell (MeterPanel::rowPeakMax, 0) != minusInf() && f.cell (MeterPanel::rowClips, 0) != "0" && ! mp.isStale(), "silence: peak max and clip count hold");
            ed->setVisible (false);
        }

        //--------------------------------------------------------------------------
        std::printf ("-- mono layout\n");
        {
            Rig rig (Layout::mono);
            auto ed = openEditor (rig.proc, 1200, 700);
            auto* mpp = findChild<MeterPanel> (*ed);
            check (mpp != nullptr, "mono: MeterPanel found");
            if (mpp != nullptr)
            {
                Fx f { rig, *ed, *mpp, *rig.feeder };
                auto& mp = *mpp;
                f.sine (-6.0f, -6.0f);
                const double t = waitFor ([&] { return mp.isMonoShown(); }, 2000);
                pump (1500);
                f.dumpAll ("mono");
                check (t >= 0.0 && mp.isMonoShown(), "mono input: the panel switches to the mono layout (" + juce::String (t, 0) + " ms)");
                check (f.snap().isMono, "snapshot isMono is set");
                check (mp.getBarPixels (0).rmsY >= 0 && mp.getBarPixels (1).rmsY < 0 && mp.getBarPixels (1).peakY < 0 && mp.getBarPixels (1).holdY < 0 && ! mp.getBarPixels (1).lampLit, "one bar: channel 1 has nothing drawn");
                check (f.corr() == dash() && f.bal() == dash(), "mono: correlation and balance show dashes: " + f.corr() + " / " + f.bal());
                check (f.cell (MeterPanel::rowPeak, 0).isEmpty() && ! f.cell (MeterPanel::rowPeak, 1).isEmpty(), "mono: left value column blank, right column carries the channel: [" + f.cell (MeterPanel::rowPeak, 0) + "] [" + f.cell (MeterPanel::rowPeak, 1) + "]");
                check (f.cell (MeterPanel::rowPeak, 1) == juce::String::fromUTF8 ("\xE2\x88\x92") + "6.0" && f.cell (MeterPanel::rowRms, 1) == juce::String::fromUTF8 ("\xE2\x88\x92") + "9.0", "mono: values on the right column read -6.0 / -9.0");
                check (mp.getCorrelationShown() == mp.getCorrelationShown(), "mono: no NaN in the correlation state");
            }
            ed->setVisible (false);
        }

        timeEndPeriod (1);
        std::printf ("\n%d checks, %d failed\n", total, failed);
        return failed == 0 ? 0 : 1;
    }

    //==========================================================================
    // Screenshots
    //==========================================================================
    juce::Image cropStrip (const juce::Image& full, int logicalW, int logicalH, float scale, int zoom)
    {
        const int sx = (int) std::lround ((logicalW - 296) * scale), sy = (int) std::lround (44 * scale);
        const int w = full.getWidth() - sx, h = (int) std::lround (logicalH * scale) - sy;
        auto crop = full.getClippedImage ({ sx, sy, w, h }).createCopy();
        if (zoom == 1) return crop;
        return crop.rescaled (w * zoom, h * zoom, juce::Graphics::highResamplingQuality);
    }

    int screenshots (const juce::File& outDir)
    {
        timeBeginPeriod (1);
        disableEcoQoS();
        outDir.createDirectory();
        const auto snap = [&] (InvestigateEditor& ed, int w, int h, float scale, const juce::String& tag, bool strip = true)
        {
            auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, scale);
            savePng (img, outDir.getChildFile (tag + ".png"));
            if (strip) savePng (cropStrip (img, w, h, scale, 2), outDir.getChildFile (tag + "-strip2x.png"));
            std::printf ("wrote %s (%dx%d)\n", tag.toRawUTF8(), img.getWidth(), img.getHeight());
        };

        {
            Rig rig (Layout::stereo);
            auto& fd = *rig.feeder;
            auto ed = openEditor (rig.proc, 1200, 700);
            auto* mp = findChild<MeterPanel> (*ed);
            fd.mode = Feeder::music;
            fd.clipsAllowed = false;
            setChoice (rig.proc, P::channelMode, 5);
            if (auto* b = findHeaderButton (*mp, 216, 12)) b->onClick();
            pump (6000);
            const struct { int idx; const char* tag; } sc[] = { { 0, "01-dbfs" }, { 1, "02-k20" }, { 2, "03-k14" }, { 3, "04-k12" } };
            for (auto& s : sc)
            {
                setChoice (rig.proc, P::meterScale, s.idx);
                pump (1100);
                snap (*ed, 1200, 700, 1.0f, s.tag);
            }
            setChoice (rig.proc, P::meterScale, 0);
            pump (500);
            // Clipped state: a clipping programme (kick pushed over full scale), then a burst for TP clips.
            fd.clipsAllowed = true;
            fd.burst = 6;
            pump (12500);   // at least one clipped kick (every fifth bar = 10 s)
            fd.burst = 3;
            pump (700);
            snap (*ed, 1200, 700, 1.0f, "05-clipped");
            // Stale: stop feeding, snapshot at 450 ms (stale since 250 ms; bars still falling).
            fd.paused = true;
            pump (450);
            snap (*ed, 1200, 700, 1.0f, "06-stale-450ms");
            pump (3500);
            snap (*ed, 1200, 700, 1.0f, "06b-stale-4s");
            fd.paused = false;
            pump (800);
            // Collapsed: the Meters toggle in the header (ui-spec 4.2); the panel must hide and the plot must take the width.
            if (auto* mb = findHeaderButton (*ed, 1200 - 72, 8))
            {
                mb->onClick();
                pump (1200);
                snap (*ed, 1200, 700, 1.0f, "12-collapsed", false);
                std::printf ("collapsed: MeterPanel visible = %d\n", (int) mp->isVisible());
                mb->onClick();
                pump (1200);
                std::printf ("expanded again: MeterPanel visible = %d\n", (int) mp->isVisible());
            }
            else
                std::printf ("collapsed shot skipped: Meters toggle not found\n");
            // Minimum size
            ed->setSize (920, 540);
            pump (1200);
            snap (*ed, 920, 540, 1.0f, "07-min-920x540");
            ed->setSize (1200, 700);
            pump (600);
            // Taller / larger sizes for the growth rule
            ed->setSize (1500, 1000);
            pump (1000);
            snap (*ed, 1500, 1000, 1.0f, "08-1500x1000");
            ed->setSize (1200, 700);
            pump (600);
            // Global scale 200 % and 110 %
            for (float gs : { 2.0f, 1.1f })
            {
                juce::Desktop::getInstance().setGlobalScaleFactor (gs);
                pump (1500);
                snap (*ed, 1200, 700, gs, gs == 2.0f ? "09-scale-200" : "10-scale-110");
            }
            juce::Desktop::getInstance().setGlobalScaleFactor (1.0f);
            pump (800);
            ed->setVisible (false);
        }
        {
            Rig rig (Layout::mono);
            auto& fd = *rig.feeder;
            auto ed = openEditor (rig.proc, 1200, 700);
            fd.mode = Feeder::music;
            pump (4000);
            snap (*ed, 1200, 700, 1.0f, "11-mono");
            ed->setVisible (false);
        }
        timeEndPeriod (1);
        return 0;
    }

    //==========================================================================
    // Probes and benchmark rigs
    //==========================================================================
    struct Probe : juce::Component
    {
        Probe() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override { stamp = juce::Time::getHighResolutionTicks(); ++count; }
        juce::int64 stamp = 0;
        int count = 0;
    };

    // Transparent probe laid over a region; counts paints whose clip touches it.
    struct Overlay : juce::Component
    {
        Overlay() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override { ++count; }
        int count = 0;
    };

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

    // Pass-based panel paint cost from JUCE's own paint diagnostics: ComponentListener::componentPainted reports the
    // panel's totalPaintDuration (paint, children and paintOverChildren) for every paint pass that touches it. This is
    // the time spent recording the panel's draw calls on the message thread (Direct2D, as in T-012/T-029); the GPU
    // present is outside it. Passes less than 3 ms apart belong to one vblank and are summed.
    void applyDisplayRate();

    struct PassProbe : juce::ComponentListener
    {
        PassProbe() { samples.reserve (40000); }

        void componentPainted (juce::Component&, const juce::ComponentPaintDiagnostics& d) override
        {
            const int txt = panel != nullptr ? panel->getTextRepaintCount() : 0;
            samples.push_back ({ nowMs(), d.totalPaintDuration.get<std::chrono::duration<double, std::milli>>(), txt != lastText });
            lastText = txt;
            if (d.readFromCache) ++cacheReads;
        }

        struct S { double endMs, durMs; bool text; };
        MeterPanel* panel = nullptr;
        int lastText = 0;
        int cacheReads = 0;
        std::vector<S> samples;
    };

    int paintCostPasses (const juce::String& sizeArg, double seconds)
    {
        timeBeginPeriod (1);
        disableEcoQoS();
        const int w = sizeArg.upToFirstOccurrenceOf ("x", false, false).getIntValue();
        const int h = sizeArg.fromFirstOccurrenceOf ("x", false, false).getIntValue();
        applyDisplayRate();
        Rig rig (Layout::stereoSidechain);
        rig.feeder->mode = Feeder::music;
        rig.feeder->clipsAllowed = true;
        auto ed = openEditor (rig.proc, w, h);
        auto* mp = findChild<MeterPanel> (*ed);
        PassProbe wrap;
        wrap.panel = mp;
        wrap.lastText = mp->getTextRepaintCount();
        mp->addComponentListener (&wrap);
        std::printf ("PAINT-PASSES %dx%d: meter panel %dx%d at %d,%d\n", w, h, mp->getWidth(), mp->getHeight(), mp->getX(), mp->getY());
        const int bar0 = mp->getBarRepaintCount(), txt0 = mp->getTextRepaintCount();
        pump (2000);                                   // warm-up, paints discarded
        wrap.samples.clear();
        const double t0 = nowMs();
        pump ((int) (seconds * 1000.0));
        const double wall = (nowMs() - t0) / 1000.0;
        std::vector<double> frames, passes;
        double lastEnd = -1e9, acc = 0.0;
        for (auto& s : wrap.samples)
        {
            passes.push_back (s.durMs);
            if (s.endMs - lastEnd > 3.0)
            {
                if (acc > 0.0) frames.push_back (acc);
                acc = 0.0;
            }
            acc += s.durMs;
            lastEnd = s.endMs;
        }
        if (acc > 0.0) frames.push_back (acc);
        if (juce::SystemStats::getEnvironmentVariable ("METER_PASSDUMP", {}).isNotEmpty())
            for (size_t i = 0; i < std::min<size_t> (60, wrap.samples.size()); ++i)
                std::printf ("pass %zu: t=%.2f ms (gap %.2f) dur %.3f\n", i, wrap.samples[i].endMs - wrap.samples[0].endMs, i ? wrap.samples[i].endMs - wrap.samples[i - 1].endMs : 0.0, wrap.samples[i].durMs);
        std::vector<double> withText, noText;
        for (auto& smp : wrap.samples) (smp.text ? withText : noText).push_back (smp.durMs);
        const auto st1 = stats (withText), st0 = stats (noText);
        std::printf ("PAINT-SPLIT %dx%d | passes after a text-cell repaint request: n %d, median %.3f, p95 %.3f | passes without: n %d, median %.3f, p95 %.3f\n", w, h,
                     st1.n, st1.median, st1.p95, st0.n, st0.median, st0.p95);
        const auto sf = stats (frames), sp = stats (passes);
        std::printf ("PAINT-PASSES %dx%d | frames with a panel paint %d (%.1f/s), passes %d (%.1f/s) | per frame ms: median %.3f, mean %.3f, p95 %.3f, p99 %.3f, worst %.3f | per pass ms: median %.3f, p95 %.3f, worst %.3f | bar repaints/s %.1f, text cell repaints/s %.1f\n",
                     w, h, (int) frames.size(), (double) frames.size() / wall, (int) passes.size(), (double) passes.size() / wall,
                     sf.median, sf.mean, sf.p95, [&] { auto v = frames; std::sort (v.begin(), v.end()); return v.empty() ? 0.0 : v[(size_t) (0.99 * (double) (v.size() - 1))]; }(), sf.worst,
                     sp.median, sp.p95, sp.worst,
                     (mp->getBarRepaintCount() - bar0) / (wall + 2.0), (mp->getTextRepaintCount() - txt0) / (wall + 2.0));
        mp->removeComponentListener (&wrap);
        ed->removeFromDesktop();
        timeEndPeriod (1);
        return sf.p95 <= (h >= 1400 ? 1.5 : 1.0) && ! frames.empty() ? 0 : 1;
    }

    int paintCost (const juce::String& sizeArg, double seconds)
    {
        timeBeginPeriod (1);
        disableEcoQoS();
        const int w = sizeArg.upToFirstOccurrenceOf ("x", false, false).getIntValue();
        const int h = sizeArg.fromFirstOccurrenceOf ("x", false, false).getIntValue();
        Rig rig (Layout::stereoSidechain);
        rig.feeder->mode = Feeder::music;
        rig.feeder->clipsAllowed = true;
        auto ed = openEditor (rig.proc, w, h);
        auto* mp = findChild<MeterPanel> (*ed);
        auto* parent = mp->getParentComponent();
        Probe startP, endP;
        Overlay over;
        const auto c = mp->getBounds().getTopLeft().translated (0, -1);
        parent->addChildComponent (startP, parent->getIndexOfChildComponent (mp));
        parent->addChildComponent (endP, parent->getIndexOfChildComponent (mp) + 1);
        parent->addChildComponent (over, parent->getIndexOfChildComponent (mp) + 2);
        startP.setBounds (c.x, c.y, 1, 1);
        endP.setBounds (c.x, c.y, 1, 1);
        over.setBounds (mp->getBounds());
        startP.setVisible (true);
        endP.setVisible (true);
        over.setVisible (true);
        std::printf ("PAINT COST %dx%d: meter panel %dx%d at %d,%d; probes before/after in z-order; overlay above counts frames whose clip touches the panel\n", w, h, mp->getWidth(), mp->getHeight(), mp->getX(), mp->getY());
        const double period = 1000.0 / 60.0;
        std::vector<double> ms;
        int frames = 0, panelFrames = 0;
        const int bar0 = mp->getBarRepaintCount(), txt0 = mp->getTextRepaintCount();
        const double t0 = nowMs();
        const int totalFrames = (int) (seconds * 60.0);
        int lastOver = over.count, lastStart = startP.count;
        for (int t = 0; t < totalFrames + 120; ++t)
        {
            startP.repaint();
            endP.repaint();
            const double target = t0 + (t + 1) * period;
            while (nowMs() < target)
            {
                pump (1);
                if (startP.count != lastStart && endP.count == startP.count)
                {
                    const bool panelPainted = over.count != lastOver;
                    if (t >= 120)
                    {
                        ++frames;
                        if (panelPainted)
                        {
                            ++panelFrames;
                            ms.push_back (juce::Time::highResolutionTicksToSeconds (endP.stamp - startP.stamp) * 1000.0);
                        }
                    }
                    lastStart = startP.count;
                    lastOver = over.count;
                }
            }
        }
        const double wall = (nowMs() - t0) / 1000.0;
        const auto st = stats (ms);
        std::printf ("PAINT %dx%d | frames observed %d, frames where the panel painted %d (%.1f/s) | panel paint ms: median %.3f, mean %.3f, p95 %.3f, worst %.3f | bar repaints/s %.1f, text repaints/s %.1f\n",
                     w, h, frames, panelFrames, panelFrames / std::max (1.0, wall - 2.0), st.median, st.mean, st.p95, st.worst,
                     (mp->getBarRepaintCount() - bar0) / wall, (mp->getTextRepaintCount() - txt0) / wall);
        parent->removeChildComponent (&startP);
        parent->removeChildComponent (&endP);
        parent->removeChildComponent (&over);
        ed->removeFromDesktop();
        timeEndPeriod (1);
        return st.p95 <= 1.0 && ! ms.empty() ? 0 : 1;
    }

    // Display refresh. The editor polls the processor on the display's vblank, so the repaint rate follows the refresh
    // rate. METER_FORCE_HZ=<n> switches the primary display to n Hz for the lifetime of this process only (CDS_FULLSCREEN
    // makes the change temporary; Windows restores the mode when the process exits). The rate in force is printed.
    void applyDisplayRate()
    {
        DEVMODEW dm {};
        dm.dmSize = sizeof (dm);
        if (const auto hzStr = juce::SystemStats::getEnvironmentVariable ("METER_FORCE_HZ", {}); hzStr.isNotEmpty())
        {
            const char* hz = hzStr.toRawUTF8();
            if (EnumDisplaySettingsW (nullptr, ENUM_CURRENT_SETTINGS, &dm))
            {
                dm.dmDisplayFrequency = (DWORD) std::atoi (hz);
                dm.dmFields = DM_DISPLAYFREQUENCY;
                const LONG r = ChangeDisplaySettingsExW (nullptr, &dm, nullptr, CDS_FULLSCREEN, nullptr);
                std::printf ("display rate change to %s Hz (temporary, this process): %s\n", hz, r == DISP_CHANGE_SUCCESSFUL ? "ok" : "FAILED");
                juce::Thread::sleep (1500);
            }
        }
        dm = {};
        dm.dmSize = sizeof (dm);
        if (EnumDisplaySettingsW (nullptr, ENUM_CURRENT_SETTINGS, &dm))
            std::printf ("DISPLAY %lux%lu at %lu Hz\n", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency);
    }


    //==========================================================================
    // Round 2: mono label screenshots at several scales, and the update rate
    //==========================================================================
    int monoShots (const juce::File& outDir)
    {
        timeBeginPeriod (1);
        disableEcoQoS();
        outDir.createDirectory();
        Rig rig (Layout::mono);
        rig.feeder->mode = Feeder::music;
        auto ed = openEditor (rig.proc, 1200, 700);
        pump (3000);
        for (float gs : { 1.0f, 1.1f, 1.25f, 1.5f })
        {
            juce::Desktop::getInstance().setGlobalScaleFactor (gs);
            ed->setSize (1200, 700);
            pump (1500);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, gs);
            const auto tag = "r2-11-mono-" + juce::String ((int) std::round (gs * 100.0f));
            std::printf ("snapshot %dx%d, editor %dx%d, global scale %.2f\n", img.getWidth(), img.getHeight(), ed->getWidth(), ed->getHeight(), gs);
            savePng (cropStrip (img, 1200, 700, gs, 3), outDir.getChildFile (tag + "-strip3x.png"));
            std::printf ("wrote %s\n", tag.toRawUTF8());
        }
        juce::Desktop::getInstance().setGlobalScaleFactor (1.0f);
        ed->removeFromDesktop();
        timeEndPeriod (1);
        return 0;
    }

    struct CountProbe : juce::ComponentListener
    {
        void componentPainted (juce::Component&, const juce::ComponentPaintDiagnostics&) override { ++n; }
        int n = 0;
    };

    int rateCheck (const juce::String& sizeArg, double seconds)
    {
        timeBeginPeriod (1);
        disableEcoQoS();
        const int w = sizeArg.upToFirstOccurrenceOf ("x", false, false).getIntValue();
        const int h = sizeArg.fromFirstOccurrenceOf ("x", false, false).getIntValue();
        applyDisplayRate();
        Rig rig (Layout::stereoSidechain);
        rig.feeder->mode = Feeder::music;
        rig.feeder->clipsAllowed = true;
        auto ed = openEditor (rig.proc, w, h);
        auto* mp = findChild<MeterPanel> (*ed);
        auto* sv = findChild<SpectrumView> (*ed);
        CountProbe pm, ps;
        mp->addComponentListener (&pm);
        sv->addComponentListener (&ps);
        // Independent vblank counter on the same thread.
        int vb = 0;
        juce::VBlankAttachment vba (ed.get(), [&] { ++vb; });
        pump (2000);
        pm.n = ps.n = vb = 0;
        const int bar0 = mp->getBarRepaintCount(), txt0 = mp->getTextRepaintCount();
        const double t0 = nowMs();
        pump ((int) (seconds * 1000.0));
        const double wall = (nowMs() - t0) / 1000.0;
        std::printf ("RATE %dx%d over %.1f s: vblank callbacks %.1f/s | meter panel paint passes %.1f/s | spectrum paint passes %.1f/s | bar repaint requests %.1f/s | text repaint requests %.1f/s\n",
                     w, h, wall, vb / wall, pm.n / wall, ps.n / wall, (mp->getBarRepaintCount() - bar0) / wall, (mp->getTextRepaintCount() - txt0) / wall);
        mp->removeComponentListener (&pm);
        sv->removeComponentListener (&ps);
        ed->removeFromDesktop();
        timeEndPeriod (1);
        return 0;
    }

    //==========================================================================
    // CPU matrix (T-029 method)
    //==========================================================================
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

    struct CaseDef { const char* name; bool dual, sec, sc, hover; int secMode; int channelMode; int bit; };
    const CaseDef cases[] = {
        { "(a) single + RT max", false, true, false, false, 1, 2, 1 },
        { "(b) dual + 2 secondaries", true, true, false, false, 1, 5, 2 },
        { "(c) dual + 2 secondaries + SC + hover", true, true, true, true, 1, 5, 4 },
        { "(d) single + all-time average + SC", false, true, true, false, 3, 2, 8 },
    };

    struct CpuResult { double net = 0, gross = 0, base = 0, barPerSec = 0, textPerSec = 0, spectrumPaintsPerSec = 0; };

    CpuResult runCpuCase (Rig& rig, const CaseDef& sc, int physW, int physH, double scale, double cpuSeconds)
    {
        setChoice (rig.proc, P::secondary, sc.secMode);
        setChoice (rig.proc, P::channelMode, sc.channelMode);
        setReal (rig.proc, P::showSidechain, sc.sc ? 1.0f : 0.0f);
        setChoice (rig.proc, P::meterScale, 0);
        auto& fd = *rig.feeder;
        fd.mode = Feeder::music;
        fd.clipsAllowed = true;
        fd.paused = false;
        auto ed = openEditor (rig.proc, (int) std::lround (physW / scale), (int) std::lround (physH / scale));
        auto* view = findChild<SpectrumView> (*ed);
        auto* mp = findChild<MeterPanel> (*ed);
        auto* parent = view->getParentComponent();
        Probe startP, endP, cornerP;
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

        const auto runLoop = [&] (int frames, bool feedSpectrum)
        {
            const double t0 = nowMs();
            for (int t = 0; t < frames; ++t)
            {
                if (feedSpectrum)
                {
                    makeFrame (*frame, m.fLo, m.fHi, t, sc.dual, sc.sec, sc.sc, rnd);
                    view->setFrameForTesting (*frame);
                    if (sc.hover) view->setHoverForTesting (hoverAt (t));
                }
                startP.repaint();
                endP.repaint();
                const double target = t0 + (t + 1) * period;
                while (nowMs() < target) pump (1);
            }
            return nowMs() - t0;
        };

        runLoop (90, true);   // warm-up with audio running

        // Baseline: no audio, no frames, same paced loop with the probe presents (T-029 method).
        fd.paused = true;
        runLoop (60 * 5, false);   // let bars and holds fall to the floor, the editor goes stale
        const int cpuFrames = (int) (cpuSeconds * 60.0);
        const double b0 = threadCpuMs();
        const double bw = runLoop (cpuFrames, false);
        const double baseCpu = threadCpuMs() - b0;

        // Loaded: audio running on its thread, a new spectrum frame per vblank, meters via the editor's own path.
        fd.paused = false;
        runLoop (60, true);
        const int bar0 = mp->getBarRepaintCount(), txt0 = mp->getTextRepaintCount(), corner0 = cornerP.count;
        const double l0 = threadCpuMs();
        const double lw = runLoop (cpuFrames, true);
        const double loadCpu = threadCpuMs() - l0;
        CpuResult r;
        r.gross = 100.0 * loadCpu / lw;
        r.base = 100.0 * baseCpu / bw;
        r.net = 100.0 * (loadCpu - baseCpu) / lw;
        r.barPerSec = 1000.0 * (mp->getBarRepaintCount() - bar0) / lw;
        r.textPerSec = 1000.0 * (mp->getTextRepaintCount() - txt0) / lw;
        r.spectrumPaintsPerSec = 1000.0 * (cornerP.count - corner0) / lw;
        parent->removeChildComponent (&startP);
        parent->removeChildComponent (&endP);
        parent->removeChildComponent (&cornerP);
        ed->removeFromDesktop();
        ed.reset();
        pump (50);
        return r;
    }

    int cpuMatrix (const juce::String& spec, int runs, int caseMask, double seconds)
    {
        timeBeginPeriod (1);
        disableEcoQoS();
        const int pw = spec.upToFirstOccurrenceOf ("x", false, false).getIntValue();
        const int ph = spec.fromFirstOccurrenceOf ("x", false, false).upToFirstOccurrenceOf ("@", false, false).getIntValue();
        const double scale = spec.contains ("@") ? spec.fromFirstOccurrenceOf ("@", false, false).getDoubleValue() : 1.0;
        applyDisplayRate();
        juce::Desktop::getInstance().setGlobalScaleFactor ((float) scale);
        std::printf ("CPU MATRIX physical %dx%d, global scale %.2f, %d runs interleaved over the cases, %.0f s per measurement, net of the paced baseline\n", pw, ph, scale, runs, seconds);
        Rig rig (Layout::stereoSidechain);
        std::vector<std::vector<CpuResult>> res (4);
        for (int r = 0; r < runs; ++r)
            for (int ci = 0; ci < 4; ++ci)
            {
                if ((caseMask & cases[ci].bit) == 0) continue;
                const auto x = runCpuCase (rig, cases[ci], pw, ph, scale, seconds);
                res[(size_t) ci].push_back (x);
                std::printf ("RUN %d | %dx%d@%.2f | %s | net %.1f %% (gross %.1f, baseline %.1f) | bar repaints/s %.1f, text repaints/s %.1f | spectrum data-rect paints/s %.1f\n",
                             r + 1, pw, ph, scale, cases[ci].name, x.net, x.gross, x.base, x.barPerSec, x.textPerSec, x.spectrumPaintsPerSec);
            }
        double worstMedian = 0.0;
        for (int ci = 0; ci < 4; ++ci)
        {
            if (res[(size_t) ci].empty()) continue;
            std::vector<double> n, bp, tp;
            for (auto& x : res[(size_t) ci]) { n.push_back (x.net); bp.push_back (x.barPerSec); tp.push_back (x.textPerSec); }
            const auto sn = stats (n);
            std::sort (n.begin(), n.end());
            std::printf ("SUMMARY %dx%d@%.2f | %s | net median %.1f, min %.1f, max %.1f | bar/s median %.1f | text/s median %.1f\n",
                         pw, ph, scale, cases[ci].name, sn.median, n.front(), n.back(), stats (bp).median, stats (tp).median);
            worstMedian = std::max (worstMedian, sn.median);
        }
        std::printf ("WORST MEDIAN %dx%d@%.2f: %.1f %% (gate 25)\n", pw, ph, scale, worstMedian);
        timeEndPeriod (1);
        return worstMedian <= 25.0 ? 0 : 1;
    }

    //==========================================================================
    int cycles (int n)
    {
        timeBeginPeriod (1);
        Rig rig (Layout::stereoSidechain);
        rig.feeder->mode = Feeder::music;
        rig.feeder->clipsAllowed = true;
        setChoice (rig.proc, P::channelMode, 5);
        const double t0 = nowMs();
        int onDesktop = 0, withScaleChange = 0;
        for (int i = 0; i < n; ++i)
        {
            auto ed = std::unique_ptr<InvestigateEditor> (dynamic_cast<InvestigateEditor*> (rig.proc.createEditor()));
            ed->setSize (i % 3 == 0 ? 920 : 1200, i % 3 == 0 ? 540 : 700);
            if (i % 4 != 3) { ed->addToDesktop (0); ed->setVisible (true); ++onDesktop; }
            pump (120 + (i % 5) * 20);
            if (i % 7 == 0) { setChoice (rig.proc, P::meterScale, (i / 7) % 4); ++withScaleChange; }
            if (i % 6 == 0 && ed->isOnDesktop()) ed->setSize (1000 + (i % 5) * 40, 600);
            if (i % 5 == 0) rig.feeder->burst = 3;
            pump (80);
            if (auto* mp = findChild<MeterPanel> (*ed))
                if (auto* b = findHeaderButton (*mp, 216, 12)) if (i % 9 == 0) b->onClick();
        }
        setChoice (rig.proc, P::meterScale, 0);
        std::printf ("CYCLES: %d open/close cycles (%d on the desktop, %d with scale changes) with meters animating, %.1f s, no crash; audio blocks processed %d\n",
                     n, onDesktop, withScaleChange, (nowMs() - t0) / 1000.0, rig.feeder->blocks.load());
        timeEndPeriod (1);
        return 0;
    }
}

int main (int argc, char** argv)
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3)
    {
        std::printf ("usage: InvestigateMeterCheck <outDir> func|shots|paint WxH [s]|cpu WxH@scale runs mask [s]|cycle n\n");
        return 2;
    }
    const juce::File out (argv[1]);
    const juce::String mode (argv[2]);
    if (mode == "func") return functional (out);
    if (mode == "shots") return screenshots (out);
    if (mode == "monoshots") return monoShots (out);
    if (mode == "rate") return rateCheck (argc > 3 ? argv[3] : "1280x720", argc > 4 ? juce::String (argv[4]).getDoubleValue() : 10.0);
    if (mode == "paint") return paintCostPasses (argc > 3 ? argv[3] : "1280x720", argc > 4 ? juce::String (argv[4]).getDoubleValue() : 12.0);
    if (mode == "paintpair") return paintCost (argc > 3 ? argv[3] : "1280x720", argc > 4 ? juce::String (argv[4]).getDoubleValue() : 12.0);
    if (mode == "cpu")
        return cpuMatrix (argc > 3 ? argv[3] : "1280x720", argc > 4 ? juce::String (argv[4]).getIntValue() : 3,
                          argc > 5 ? juce::String (argv[5]).getIntValue() : 15, argc > 6 ? juce::String (argv[6]).getDoubleValue() : 12.0);
    if (mode == "cycle") return cycles (argc > 3 ? juce::String (argv[3]).getIntValue() : 50);
    return 2;
}
