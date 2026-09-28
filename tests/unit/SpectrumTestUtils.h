#pragma once

// Shared helpers for the T-004 SpectrumAnalyser/SampleFifo/RealFft unit
// tests (tests/unit/*SpectrumTest*.cpp, RealFftAgreementTest.cpp,
// SampleFifoTest.cpp). Not linked into source/ - test-only.

#include "dsp/SpectrumAnalyser.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace investigate::test
{
    constexpr double kPi = 3.14159265358979323846;

    inline double powerToDb (double power) noexcept
    {
        return 10.0 * std::log10 (std::max (power, 1e-300));
    }

    inline double dbToAmplitude (double db) noexcept
    {
        return std::pow (10.0, db / 20.0);
    }

    /** Continuous-phase sine, amplitude in linear full-scale units
        (1.0 == 0 dBFS), starting at sample 0. */
    inline std::vector<float> makeSine (double amplitude, double freqHz, double sampleRate, int numSamples, double startPhase = 0.0)
    {
        std::vector<float> out (static_cast<size_t> (numSamples));
        const double w = 2.0 * kPi * freqHz / sampleRate;
        for (int n = 0; n < numSamples; ++n)
            out[static_cast<size_t> (n)] = static_cast<float> (amplitude * std::sin (w * static_cast<double> (n) + startPhase));
        return out;
    }

    inline std::vector<float> makeSilence (int numSamples)
    {
        return std::vector<float> (static_cast<size_t> (numSamples), 0.0f);
    }

    inline std::vector<float> makeDc (double amplitude, int numSamples)
    {
        return std::vector<float> (static_cast<size_t> (numSamples), static_cast<float> (amplitude));
    }

    inline std::vector<float> makeWhiteNoise (double amplitude, int numSamples, int seed)
    {
        juce::Random rng (seed);
        std::vector<float> out (static_cast<size_t> (numSamples));
        for (auto& s : out)
            s = static_cast<float> (amplitude * (2.0 * rng.nextDouble() - 1.0));
        return out;
    }

    /** Pushes a mono buffer to a 1-main-channel analyser, in chunks of
        `chunkSize` samples (whole buffer at once if chunkSize <= 0). */
    inline void pushMono (investigate::SpectrumAnalyser& analyser, const std::vector<float>& samples, int chunkSize = -1)
    {
        const int total = static_cast<int> (samples.size());
        if (chunkSize <= 0)
            chunkSize = total;

        int offset = 0;
        while (offset < total)
        {
            const int n = std::min (chunkSize, total - offset);
            const float* p = samples.data() + offset;
            const float* mainCh[1] = { p };
            analyser.pushBlock (mainCh, nullptr, n);
            offset += n;
        }
    }

    /** Pushes two independent main channels (stereo), in chunks. */
    inline void pushStereo (investigate::SpectrumAnalyser& analyser,
                             const std::vector<float>& left,
                             const std::vector<float>& right,
                             int chunkSize = -1)
    {
        jassert (left.size() == right.size());
        const int total = static_cast<int> (left.size());
        if (chunkSize <= 0)
            chunkSize = total;

        int offset = 0;
        while (offset < total)
        {
            const int n = std::min (chunkSize, total - offset);
            const float* mainCh[2] = { left.data() + offset, right.data() + offset };
            analyser.pushBlock (mainCh, nullptr, n);
            offset += n;
        }
    }

    inline bool allFinite (const float* data, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            if (! std::isfinite (data[i]))
                return false;
        return true;
    }
}
