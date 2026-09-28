#include "PffftRealFft.h"

#include <cstring>

namespace investigate
{

PffftRealFft::PffftRealFft (int fftSizeIn)
    : fftSize (fftSizeIn)
{
    setup = pffft_new_setup (fftSize, PFFFT_REAL);
    alignedInput = static_cast<float*> (pffft_aligned_malloc (static_cast<size_t> (fftSize) * sizeof (float)));
    alignedOutput = static_cast<float*> (pffft_aligned_malloc (static_cast<size_t> (fftSize) * sizeof (float)));
    alignedWork = static_cast<float*> (pffft_aligned_malloc (static_cast<size_t> (fftSize) * sizeof (float)));
}

PffftRealFft::~PffftRealFft()
{
    pffft_aligned_free (alignedWork);
    pffft_aligned_free (alignedOutput);
    pffft_aligned_free (alignedInput);
    if (setup != nullptr)
        pffft_destroy_setup (setup);
}

void PffftRealFft::forward (const float* timeDomain, float* outReal, float* outImag) noexcept
{
    std::memcpy (alignedInput, timeDomain, static_cast<size_t> (fftSize) * sizeof (float));

    pffft_transform_ordered (setup, alignedInput, alignedOutput, alignedWork, PFFFT_FORWARD);

    // pffft's ordered real-forward packing (pffft.h): index 0 holds DC in
    // the real slot and Nyquist in what would otherwise be DC's imaginary
    // slot; indices 2k/2k+1 hold bin k's real/imaginary parts for
    // k = 1 .. N/2 - 1.
    const int half = fftSize / 2;

    outReal[0] = alignedOutput[0];
    outImag[0] = 0.0f;
    outReal[half] = alignedOutput[1];
    outImag[half] = 0.0f;

    for (int k = 1; k < half; ++k)
    {
        outReal[k] = alignedOutput[2 * k];
        outImag[k] = alignedOutput[2 * k + 1];
    }
}

} // namespace investigate
