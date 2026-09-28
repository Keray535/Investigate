// Shared signal-generation and block-feeding helpers for T-007 meter unit
// tests (LevelMeter, TruePeakDetector, CorrelationMeter, MeterBank).
// Deliberately independent of LoudnessMeterTestUtils.h (T-006) so these
// tests do not have to include dsp/LoudnessMeter.h; the numeric helpers are
// intentionally similar (same deterministic, seeded, wall-clock-free
// approach), duplicated rather than shared across task boundaries per
// CLAUDE.md ownership (unit-tester owns tests/unit/ but keeps each task's
// test files self-contained where practical).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace investigate::test
{
    constexpr double kMeterTestPi = 3.14159265358979323846;

    inline double dbfsToAmplitude (double dbfs) noexcept
    {
        return std::pow (10.0, dbfs / 20.0);
    }

    inline double amplitudeToDbfs (double amplitude) noexcept
    {
        return amplitude > 0.0 ? 20.0 * std::log10 (amplitude) : -std::numeric_limits<double>::infinity();
    }

    /** x[n] = amplitude * sin(2*pi*freqHz*n/sampleRate + phaseRadians), n = 0..numSamples-1,
        starting at absolute sample index `startSample` (so a signal can be generated in
        chunks while staying phase-exact, without needing a mutable phase accumulator that
        could itself drift over very long runs). */
    inline std::vector<float> makeSineAt (double freqHz, double amplitude, double sampleRate,
                                           int numSamples, double phaseRadians, int64_t startSample = 0)
    {
        std::vector<float> out (static_cast<size_t> (numSamples));
        const double w = 2.0 * kMeterTestPi * freqHz / sampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            const double n = static_cast<double> (startSample + i);
            out[static_cast<size_t> (i)] = static_cast<float> (amplitude * std::sin (w * n + phaseRadians));
        }
        return out;
    }

    inline std::vector<float> makeSilence (int numSamples)
    {
        return std::vector<float> (static_cast<size_t> (numSamples), 0.0f);
    }

    inline std::vector<float> makeDc (int numSamples, float value)
    {
        return std::vector<float> (static_cast<size_t> (numSamples), value);
    }

    /** Deterministic LCG-based uniform white noise in [-amplitude, amplitude]. RMS =
        amplitude / sqrt(3) exactly, in the large-N limit - used where a *calibrated* RMS
        level matters (e.g. the balance-meter test), as opposed to makePinkNoise's "roughly
        normalised" amplitude parameter. */
    inline std::vector<float> makeWhiteNoise (double amplitude, int numSamples, uint32_t seed)
    {
        uint32_t state = seed == 0 ? 1u : seed;
        std::vector<float> out (static_cast<size_t> (numSamples));
        for (int i = 0; i < numSamples; ++i)
        {
            state = state * 1664525u + 1013904223u;
            const double u = static_cast<double> (state >> 8) / static_cast<double> (1u << 24); // [0,1)
            out[static_cast<size_t> (i)] = static_cast<float> (amplitude * (u * 2.0 - 1.0));
        }
        return out;
    }

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
            return pink * 0.11f;
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

    /** A linear 20 Hz -> 20 kHz swept sine (in frequency, not log) over the whole buffer,
        peak amplitude `amplitude`. Deterministic, phase-continuous by construction (phase is
        the integral of a continuous instantaneous frequency). */
    inline std::vector<float> makeSweptSine (double amplitude, double sampleRate, int numSamples)
    {
        std::vector<float> out (static_cast<size_t> (numSamples));
        const double f0 = 20.0, f1 = 20000.0;
        const double durationS = static_cast<double> (numSamples) / sampleRate;
        double phase = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = static_cast<double> (i) / sampleRate;
            const double freq = f0 + (f1 - f0) * (t / durationS);
            out[static_cast<size_t> (i)] = static_cast<float> (amplitude * std::sin (phase));
            phase += 2.0 * kMeterTestPi * freq / sampleRate;
        }
        return out;
    }

    inline void appendTo (std::vector<float>& dest, const std::vector<float>& src)
    {
        dest.insert (dest.end(), src.begin(), src.end());
    }

    /** Feeds a stereo pair through any meter with a
        `process(const float* const*, int, int)` method, in fixed-size chunks. */
    template <typename Meter>
    inline void feedStereo (Meter& meter, const std::vector<float>& left, const std::vector<float>& right,
                             int chunkSize = 512)
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

    template <typename Meter>
    inline void feedMono (Meter& meter, const std::vector<float>& mono, int chunkSize = 512)
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

    /** Simple windowed-sinc low-pass FIR (Hann-windowed), used only by the true-peak test's
        4x-oversampled-domain anti-alias/downsample synthesis (T-007 EBU cases 20-23), not by
        any production code. `normalisedCutoff` is the cutoff frequency divided by the
        (4x-domain) sample rate, e.g. 0.125 for a cutoff at fs/2 when the working rate is 4*fs. */
    inline std::vector<float> lowPassFilter (const std::vector<float>& in, double normalisedCutoff, int taps)
    {
        std::vector<double> h (static_cast<size_t> (taps));
        const int m = taps - 1;
        double sum = 0.0;
        for (int i = 0; i < taps; ++i)
        {
            const double x = static_cast<double> (i) - m / 2.0;
            const double sinc = (x == 0.0) ? 2.0 * normalisedCutoff
                                            : std::sin (2.0 * kMeterTestPi * normalisedCutoff * x) / (kMeterTestPi * x);
            const double hann = 0.5 - 0.5 * std::cos (2.0 * kMeterTestPi * i / m);
            h[static_cast<size_t> (i)] = sinc * hann;
            sum += h[static_cast<size_t> (i)];
        }
        for (auto& v : h)
            v /= sum; // unity DC gain

        std::vector<float> out (in.size(), 0.0f);
        const int half = taps / 2;
        for (size_t n = 0; n < in.size(); ++n)
        {
            double acc = 0.0;
            for (int k = 0; k < taps; ++k)
            {
                const int64_t idx = static_cast<int64_t> (n) + k - half;
                if (idx >= 0 && idx < static_cast<int64_t> (in.size()))
                    acc += h[static_cast<size_t> (k)] * static_cast<double> (in[static_cast<size_t> (idx)]);
            }
            out[n] = static_cast<float> (acc);
        }
        return out;
    }

    /** Modified Bessel function of the first kind, order 0, via its standard power
        series (Abramowitz & Stegun 9.6.12). Converges quickly for the argument
        ranges a Kaiser window's beta produces (beta <= ~12 here); 40 terms is far
        more than needed but the cost is trivial and this is test-only code, never
        called from anything real-time. Used only by designKaiserLowPass() below
        (T-024, EBU cases 20-23's anti-alias filter). */
    inline double besselI0 (double x) noexcept
    {
        double sum = 1.0;
        double term = 1.0;
        const double halfX = x * 0.5;
        for (int k = 1; k <= 40; ++k)
        {
            term *= (halfX * halfX) / (static_cast<double> (k) * static_cast<double> (k));
            sum += term;
            if (term < 1.0e-18 * sum)
                break;
        }
        return sum;
    }

    /** Kaiser window shape parameter beta from a target stopband attenuation in dB
        (Kaiser's own empirical formula, Oppenheim & Schafer "Discrete-Time Signal
        Processing" section on FIR window design; also Wikipedia "Kaiser window",
        both citing the same piecewise fit). */
    inline double kaiserBeta (double attenuationDb) noexcept
    {
        if (attenuationDb > 50.0)
            return 0.1102 * (attenuationDb - 8.7);
        if (attenuationDb >= 21.0)
            return 0.5842 * std::pow (attenuationDb - 21.0, 0.4) + 0.07886 * (attenuationDb - 21.0);
        return 0.0;
    }

    /** Designs a linear-phase, unity-DC-gain, Kaiser-windowed-sinc low-pass FIR:
        `normalisedCutoff` is the -6dB-ish cutoff (cutoff frequency / sample rate,
        e.g. 0.1125 for 21.6kHz at a 192kHz working rate - T-024's EBU cases 20-23
        anti-alias filter, cutoff at or below 0.45*fs of the *target* rate);
        `normalisedTransitionWidth` is the passband-to-stopband transition width in
        the same normalised units; `attenuationDb` is the desired stopband floor
        (positive dB). Tap count and beta both come from Kaiser's own standard
        design formulas (Oppenheim & Schafer; Crochiere & Rabiner 1983 gives the
        same formula under "windowed FIR design"): unlike a fixed Hann window
        (whose first-sidelobe floor is a fixed ~-44dB regardless of length), a
        Kaiser window's beta is chosen for the *target* attenuation, so a long
        enough filter genuinely reaches -90dB or more, verified independently by
        filterMagnitudeDb() below rather than assumed. Returns the tap coefficients,
        already tap-count long, in a std::vector<double> (test-only; no dynamic
        allocation constraint applies here, only inside processBlock). */
    inline std::vector<double> designKaiserLowPass (double normalisedCutoff, double normalisedTransitionWidth,
                                                      double attenuationDb)
    {
        const double beta = kaiserBeta (attenuationDb);
        const double deltaOmega = 2.0 * kMeterTestPi * normalisedTransitionWidth;
        int order = static_cast<int> (std::ceil ((attenuationDb - 7.95) / (2.285 * deltaOmega)));
        order = std::max (order, 4);
        int taps = order + 1;
        if ((taps % 2) == 0)
            ++taps; // force odd: a symmetric odd-length kernel has an exact integer group delay.

        std::vector<double> h (static_cast<size_t> (taps));
        const double m = static_cast<double> (taps - 1);
        const double i0Beta = besselI0 (beta);
        double dcSum = 0.0;
        for (int n = 0; n < taps; ++n)
        {
            const double x = static_cast<double> (n) - m / 2.0;
            const double sinc = (x == 0.0) ? 2.0 * normalisedCutoff
                                            : std::sin (2.0 * kMeterTestPi * normalisedCutoff * x) / (kMeterTestPi * x);
            const double ratio = (m > 0.0) ? (2.0 * static_cast<double> (n) / m - 1.0) : 0.0;
            const double window = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - ratio * ratio))) / i0Beta;
            h[static_cast<size_t> (n)] = sinc * window;
            dcSum += h[static_cast<size_t> (n)];
        }
        for (auto& v : h)
            v /= dcSum; // unity DC gain, exactly as lowPassFilter() above does.
        return h;
    }

    /** Filters `in` with a centred, zero-padded-at-the-edges convolution against
        `coeffs` (the same technique lowPassFilter() above uses): out[n] uses
        coeffs applied symmetrically around index n, dropping taps that fall
        outside [0, in.size()). For a linear-phase (symmetric) kernel this makes
        the filtered signal's features land at (approximately) the same sample
        index as the unfiltered signal's, rather than after the kernel's full
        group delay - convenient for comparing a filtered peak's timing against
        the original splice point, at the cost of edge inaccuracy within
        coeffs.size()/2 samples of either end (acceptable here since the signal
        under test fades toward silence well before its ends - see the T-024
        cases 20-23 test). */
    inline std::vector<float> applyFirFilterCentered (const std::vector<float>& in, const std::vector<double>& coeffs)
    {
        const int taps = static_cast<int> (coeffs.size());
        const int half = taps / 2;
        std::vector<float> out (in.size(), 0.0f);
        for (size_t n = 0; n < in.size(); ++n)
        {
            double acc = 0.0;
            for (int k = 0; k < taps; ++k)
            {
                const int64_t idx = static_cast<int64_t> (n) + k - half;
                if (idx >= 0 && idx < static_cast<int64_t> (in.size()))
                    acc += coeffs[static_cast<size_t> (k)] * static_cast<double> (in[static_cast<size_t> (idx)]);
            }
            out[n] = static_cast<float> (acc);
        }
        return out;
    }

    /** |H(f)| in dB, relative to the filter's own DC gain (which designKaiserLowPass()
        normalises to exactly 1.0, i.e. 0dB), evaluated directly from the tap
        coefficients via the DTFT sum - used to independently verify a designed
        filter's stopband floor rather than assuming the Kaiser design formula hit
        its target (T-024 cases 20-23). `normalisedFreq` is frequency / sample rate. */
    inline double filterMagnitudeDb (const std::vector<double>& coeffs, double normalisedFreq)
    {
        double re = 0.0, im = 0.0;
        for (size_t n = 0; n < coeffs.size(); ++n)
        {
            const double phase = 2.0 * kMeterTestPi * normalisedFreq * static_cast<double> (n);
            re += coeffs[n] * std::cos (phase);
            im -= coeffs[n] * std::sin (phase);
        }
        const double mag = std::sqrt (re * re + im * im);
        return 20.0 * std::log10 (std::max (mag, 1.0e-20));
    }
}
