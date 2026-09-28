#pragma once

#include "RealFft.h"

// pffft's include directory is added by investigate_pffft's PUBLIC include
// path (CMakeLists.txt), which exposes the header as <pffft.h> - see
// docs/dependencies.md.
#include <pffft.h>

namespace investigate
{

/** Real-FFT implementation backed by pffft (marton78 fork, v1.1.0,
    BSD-like, SIMD). This is the product FFT used by `SpectrumAnalyser`
    (T-021, T-002 section 1).

    pffft requires all of its own float buffers to be 16-byte-aligned
    (pffft.h), so this class allocates its input/output/work scratch with
    `pffft_aligned_malloc` in the constructor - i.e. only on the analysis
    thread, per the task's "allocate them only on the consumer thread"
    requirement - and copies the caller's (unaligned-in-general) time-domain
    frame into its own aligned input buffer on every `forward()` call. That
    copy, plus the transform itself, is the only work `forward()` does; it
    never allocates. */
class PffftRealFft final : public RealFft
{
public:
    explicit PffftRealFft (int fftSizeIn);
    ~PffftRealFft() override;

    PffftRealFft (const PffftRealFft&) = delete;
    PffftRealFft& operator= (const PffftRealFft&) = delete;

    void forward (const float* timeDomain, float* outReal, float* outImag) noexcept override;
    int getFftSize() const noexcept override { return fftSize; }

private:
    int fftSize;
    PFFFT_Setup* setup = nullptr;
    float* alignedInput = nullptr;   // pffft_aligned_malloc, size fftSize
    float* alignedOutput = nullptr;  // pffft_aligned_malloc, size fftSize (pffft's packed real format)
    float* alignedWork = nullptr;    // pffft_aligned_malloc, size fftSize (pffft's scratch requirement)
};

} // namespace investigate
