// Shared signal-generation helpers for T-006 LoudnessMeter unit tests.
#pragma once

#include "dsp/LoudnessMeter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace investigate::test
{
    constexpr double kLoudnessTestPi = 3.14159265358979323846;

    /** dBFS (relative to a peak amplitude of 1.0) to linear peak amplitude. */
    inline double dbfsToAmplitude (double dbfs) noexcept
    {
        return std::pow (10.0, dbfs / 20.0);
    }

    /** Generates `numSamples` of a sine at `freqHz`, peak amplitude `amplitude`,
        continuing the phase from `phaseInOut` (radians) across calls so
        multi-block/multi-segment signals stay phase-continuous. */
    inline std::vector<float> makeSine (double freqHz, double amplitude, double sampleRate,
                                         int numSamples, double& phaseInOut)
    {
        std::vector<float> out (static_cast<size_t> (numSamples));
        const double step = 2.0 * kLoudnessTestPi * freqHz / sampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            out[static_cast<size_t> (i)] = static_cast<float> (amplitude * std::sin (phaseInOut));
            phaseInOut += step;
            if (phaseInOut > 2.0 * kLoudnessTestPi)
                phaseInOut -= 2.0 * kLoudnessTestPi;
        }
        return out;
    }

    inline std::vector<float> makeSine (double freqHz, double amplitude, double sampleRate, int numSamples)
    {
        double phase = 0.0;
        return makeSine (freqHz, amplitude, sampleRate, numSamples, phase);
    }

    inline std::vector<float> makeSilence (int numSamples)
    {
        return std::vector<float> (static_cast<size_t> (numSamples), 0.0f);
    }

    /** Deterministic pseudo-random pink-ish noise generator (Paul Kellet's
        refined method), fixed seed via the linear congruential white source -
        deterministic across runs/platforms, no wall-clock/hardware RNG. */
    class PinkNoiseGenerator
    {
    public:
        explicit PinkNoiseGenerator (uint32_t seed) noexcept : state (seed == 0 ? 1u : seed) {}

        float next() noexcept
        {
            const float white = nextWhite();
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            return pink * 0.11f; // roughly normalise to +-1-ish range
        }

    private:
        float nextWhite() noexcept
        {
            state = state * 1664525u + 1013904223u;
            const float u = static_cast<float> (state >> 8) / static_cast<float> (1u << 24);
            return u * 2.0f - 1.0f;
        }

        uint32_t state;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    };

    inline std::vector<float> makePinkNoise (double amplitude, int numSamples, uint32_t seed)
    {
        PinkNoiseGenerator gen (seed);
        std::vector<float> out (static_cast<size_t> (numSamples));
        for (int i = 0; i < numSamples; ++i)
            out[static_cast<size_t> (i)] = static_cast<float> (amplitude) * gen.next();
        return out;
    }

    inline void appendTo (std::vector<float>& dest, const std::vector<float>& src)
    {
        dest.insert (dest.end(), src.begin(), src.end());
    }

    /** Feeds a stereo pair through `meter.process()` in fixed-size chunks
        (default 512, a plausible host block size) rather than one giant
        call, exercising the same block-splitting invariance real hosts
        rely on. */
    inline void feedStereo (LoudnessMeter& meter, const std::vector<float>& left,
                             const std::vector<float>& right, int chunkSize = 512)
    {
        const int numSamples = static_cast<int> (std::min (left.size(), right.size()));
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = std::min (chunkSize, numSamples - pos);
            const float* chans[2] = { left.data() + pos, right.data() + pos };
            meter.process (chans, 2, n);
            pos += n;
        }
    }

    inline void feedMono (LoudnessMeter& meter, const std::vector<float>& mono, int chunkSize = 512)
    {
        const int numSamples = static_cast<int> (mono.size());
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = std::min (chunkSize, numSamples - pos);
            const float* chans[1] = { mono.data() + pos };
            meter.process (chans, 1, n);
            pos += n;
        }
    }

    /** Builds a stereo (in-phase, identical L/R) sine segment of `seconds`
        duration at `dbfsPeak`, continuing `phaseInOut` across calls. */
    inline void appendStereoSineSegment (std::vector<float>& left, std::vector<float>& right,
                                          double freqHz, double dbfsPeak, double sampleRate,
                                          double seconds, double& phaseInOut)
    {
        const int n = static_cast<int> (std::llround (seconds * sampleRate));
        double phaseCopy = phaseInOut;
        auto segL = makeSine (freqHz, dbfsToAmplitude (dbfsPeak), sampleRate, n, phaseInOut);
        auto segR = makeSine (freqHz, dbfsToAmplitude (dbfsPeak), sampleRate, n, phaseCopy);
        appendTo (left, segL);
        appendTo (right, segR);
    }

    inline void appendStereoSilenceSegment (std::vector<float>& left, std::vector<float>& right,
                                             double sampleRate, double seconds)
    {
        const int n = static_cast<int> (std::llround (seconds * sampleRate));
        appendTo (left, makeSilence (n));
        appendTo (right, makeSilence (n));
    }
}
