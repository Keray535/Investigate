#pragma once

#include "RealFft.h"

#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace investigate
{

/** Real-FFT implementation backed by `juce::dsp::FFT`, the correctness
    reference kept alongside the product FFT (pffft) per T-002 section 1 and
    the T-004 task's acceptance criteria (bin-for-bin agreement within 1e-4
    relative power error). Not tuned for speed - see the T-004 task log for
    the pffft-vs-JUCE benchmark that motivated keeping this as a reference
    only. */
class JuceRealFft final : public RealFft
{
public:
    explicit JuceRealFft (int fftSizeIn);

    void forward (const float* timeDomain, float* outReal, float* outImag) noexcept override;
    int getFftSize() const noexcept override { return fftSize; }

private:
    int fftSize;
    juce::dsp::FFT fft;
    std::vector<float> scratch; // size 2 * fftSize, per juce::dsp::FFT's contract
};

} // namespace investigate
