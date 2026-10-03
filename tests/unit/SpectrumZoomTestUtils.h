#pragma once

// Shared helpers for the T-035 zoom/freeze tests. Test-only.

#include "SpectrumTestUtils.h"
#include "dsp/SpectrumEngine.h"
#include "dsp/SpectrumFrame.h"
#include "dsp/SpectrumSettings.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace investigate::test
{
    using FramePtr = std::unique_ptr<SpectrumFrame>;

    inline FramePtr newFrame() { return std::make_unique<SpectrumFrame>(); }

    /** Deterministic stereo pink noise (Paul Kellet filter) plus three tones,
        and a sidechain of pink noise plus one tone. `variant` 1 changes the
        tones and the level so it is clearly different audio. */
    struct Samples
    {
        std::vector<float> l, r, sl, sr;
        int size() const { return static_cast<int> (l.size()); }
    };

    class Source
    {
    public:
        explicit Source (uint32_t seed, int variant = 0) : state (seed * 2654435761u + 12345u), variantIndex (variant) {}

        Samples next (int frames)
        {
            Samples s;
            s.l.resize (static_cast<size_t> (frames));
            s.r = s.l;
            s.sl = s.l;
            s.sr = s.l;

            const double f0 = variantIndex == 0 ? 110.0 : 230.0;
            const double f1 = variantIndex == 0 ? 1000.0 : 2500.0;
            const double f2 = variantIndex == 0 ? 6000.0 : 9000.0;
            const double level = variantIndex == 0 ? 0.08 : 0.4;
            const double w = 2.0 * kPi / 48000.0;

            for (int i = 0; i < frames; ++i)
            {
                const double t = static_cast<double> (counter++);
                const double tones = 0.05 * std::sin (w * f0 * t) + 0.05 * std::sin (w * f1 * t + 0.3) + 0.05 * std::sin (w * f2 * t + 1.1);
                s.l[static_cast<size_t> (i)] = static_cast<float> (level * pink (pl) + tones);
                s.r[static_cast<size_t> (i)] = static_cast<float> (level * pink (pr) + 0.6 * tones);
                s.sl[static_cast<size_t> (i)] = static_cast<float> (level * pink (psl) + 0.05 * std::sin (w * 440.0 * t));
                s.sr[static_cast<size_t> (i)] = static_cast<float> (level * pink (psr) + 0.05 * std::sin (w * 440.0 * t + 0.5));
            }
            return s;
        }

    private:
        struct Pink { double b0 = 0, b1 = 0, b2 = 0; };

        double white()
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<double> (state) / 2147483648.0 - 1.0;
        }

        double pink (Pink& p)
        {
            const double x = white();
            p.b0 = 0.99765 * p.b0 + x * 0.0990460;
            p.b1 = 0.96300 * p.b1 + x * 0.2965164;
            p.b2 = 0.57000 * p.b2 + x * 1.0526913;
            return 0.3 * (p.b0 + p.b1 + p.b2 + x * 0.1848);
        }

        uint32_t state;
        int variantIndex;
        uint64_t counter = 0;
        Pink pl, pr, psl, psr;
    };

    inline SpectrumSettings makeSettings (ChannelMode mode = ChannelMode::lrDual,
                                          SecondaryMode secondary = SecondaryMode::allTimeAverage,
                                          int numSidechain = 2)
    {
        SpectrumSettings s;
        s.sampleRate = 48000.0;
        s.fftSize = 4096;
        s.overlap = 4;
        s.channelMode = mode;
        s.avgTimeMs = 200.0f;
        s.secondary = secondary;
        s.numMainChannels = 2;
        s.numSidechainChannels = numSidechain;
        s.smoothing = SmoothingMode::oct1_12;
        s.slopeDbPerOct = 4.5f;
        s.numPoints = 1024;
        return s;
    }

    /** An engine plus the settings it is driven with and a deterministic
        audio source. */
    struct Rig
    {
        explicit Rig (const SpectrumSettings& settings, uint32_t seed = 1, int variant = 0)
            : s (settings), source (seed, variant)
        {
            engine.prepare (s.sampleRate, 512, s.numMainChannels, s.numSidechainChannels);
        }

        void push (const Samples& smp)
        {
            const int total = smp.size();
            for (int off = 0; off < total; off += 512)
            {
                const int n = std::min (512, total - off);
                const float* main[2] = { smp.l.data() + off, smp.r.data() + off };
                engine.pushMain (main, s.numMainChannels, n);
                if (s.numSidechainChannels > 0)
                {
                    const float* sc[2] = { smp.sl.data() + off, smp.sr.data() + off };
                    engine.pushSidechain (sc, s.numSidechainChannels, n);
                }
            }
        }

        /** Pushes `frames` frames from `src` (at most the FIFO capacity). */
        void feed (int frames, Source& src) { push (src.next (frames)); }
        void feed (int frames) { feed (frames, source); }

        bool process() { return engine.processPending (s); }

        /** Feeds `frames` frames in chunks of 16384 with a processPending call after each. */
        void run (int frames, Source& src)
        {
            for (int done = 0; done < frames; done += 16384)
            {
                feed (std::min (16384, frames - done), src);
                process();
            }
        }
        void run (int frames) { run (frames, source); }

        /** Newest frame, or nullptr if none arrived since the last read. */
        FramePtr read()
        {
            auto f = newFrame();
            return engine.readLatest (*f) ? std::move (f) : nullptr;
        }

        SpectrumEngine engine;
        SpectrumSettings s;
        Source source;
    };

    inline bool sameFloats (const float* a, const float* b, int n)
    {
        return std::memcmp (a, b, static_cast<size_t> (n) * sizeof (float)) == 0;
    }

    /** Bitwise equality of everything a frame carries about the curves (not
        the sequence number or the epoch). */
    inline bool sameCurves (const SpectrumFrame& a, const SpectrumFrame& b)
    {
        if (a.numPoints != b.numPoints || a.isDual != b.isDual || a.hasSecondary != b.hasSecondary
            || a.hasSidechain != b.hasSidechain || a.viewFreqLow != b.viewFreqLow || a.viewFreqHigh != b.viewFreqHigh
            || a.overviewNumPoints != b.overviewNumPoints)
            return false;

        const int n = a.numPoints;
        if (! sameFloats (a.freqHz.data(), b.freqHz.data(), n) || ! sameFloats (a.primaryDb.data(), b.primaryDb.data(), n))
            return false;
        if (a.isDual && ! sameFloats (a.primaryBDb.data(), b.primaryBDb.data(), n))
            return false;
        if (a.hasSecondary)
        {
            if (! sameFloats (a.secondaryDb.data(), b.secondaryDb.data(), n))
                return false;
            if (a.isDual && ! sameFloats (a.secondaryBDb.data(), b.secondaryBDb.data(), n))
                return false;
        }
        if (a.hasSidechain && ! sameFloats (a.sidechainDb.data(), b.sidechainDb.data(), n))
            return false;

        return sameFloats (a.overviewFreqHz.data(), b.overviewFreqHz.data(), a.overviewNumPoints)
            && sameFloats (a.overviewDb.data(), b.overviewDb.data(), a.overviewNumPoints);
    }
}
