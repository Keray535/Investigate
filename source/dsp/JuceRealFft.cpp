#include "JuceRealFft.h"

#include <bit>

namespace investigate
{

namespace
{
    // fftSizeIn is guaranteed by the caller (createRealFft) to be an exact
    // power of two, so the order is just the position of its single set bit.
    int orderForSize (int fftSizeIn) noexcept
    {
        return std::countr_zero (static_cast<unsigned int> (fftSizeIn));
    }
}

JuceRealFft::JuceRealFft (int fftSizeIn)
    : fftSize (fftSizeIn),
      fft (orderForSize (fftSizeIn)),
      scratch (static_cast<size_t> (fftSizeIn) * 2, 0.0f)
{
}

void JuceRealFft::forward (const float* timeDomain, float* outReal, float* outImag) noexcept
{
    std::copy (timeDomain, timeDomain + fftSize, scratch.begin());
    std::fill (scratch.begin() + fftSize, scratch.end(), 0.0f);

    // onlyCalculateNonNegativeFrequencies = true: only bins 0 .. fftSize/2
    // are valid on return, which is exactly the one-sided range RealFft
    // exposes.
    fft.performRealOnlyForwardTransform (scratch.data(), true);

    const int half = fftSize / 2;
    for (int k = 0; k <= half; ++k)
    {
        outReal[k] = scratch[static_cast<size_t> (2 * k)];
        outImag[k] = scratch[static_cast<size_t> (2 * k + 1)];
    }
}

} // namespace investigate
