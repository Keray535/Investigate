#pragma once

#include <memory>

namespace investigate
{

/** Minimal real-input FFT interface so `SpectrumAnalyser` can be built
    against either the product FFT (pffft) or the `juce::dsp::FFT` reference
    implementation used only by unit tests, without depending on either
    concretely (roadmap risk #2 / T-002 section 1).

    `fftSize` is fixed for the lifetime of an implementation instance and
    must be a power of two between 512 and 65536 inclusive; implementations
    assume the caller has already validated this.

    `forward()` computes the one-sided real spectrum of a real input of
    length `fftSize`: bins 0 .. fftSize/2 inclusive (fftSize/2 + 1 bins in
    total). Bin 0 (DC) and bin fftSize/2 (Nyquist) always have zero
    imaginary part by construction. Transforms are unscaled (matches both
    pffft's and JUCE's raw FFT convention); `SpectrumAnalyser` applies its
    own coherent-gain and 1/N scaling once, after the transform.
*/
class RealFft
{
public:
    virtual ~RealFft() = default;

    /** `timeDomain` must have `getFftSize()` valid samples; `outReal` and
        `outImag` must each have `getNumBins()` valid slots. Real-time-safe
        (no allocation), aside from PffftRealFft's internal copy into its
        own SIMD-aligned scratch buffers. */
    virtual void forward (const float* timeDomain, float* outReal, float* outImag) noexcept = 0;

    virtual int getFftSize() const noexcept = 0;
    int getNumBins() const noexcept { return getFftSize() / 2 + 1; }
};

enum class FftBackend
{
    pffft,        ///< The product FFT (T-021, marton78/pffft, BSD-like, SIMD).
    juceReference ///< juce::dsp::FFT, kept as the correctness reference for tests.
};

/** Allocates a new RealFft implementation for the given backend and size.
    Allocates (creates the FFT plan and, for pffft, SIMD-aligned scratch
    buffers); call only off the audio thread. */
std::unique_ptr<RealFft> createRealFft (FftBackend backend, int fftSize);

} // namespace investigate
