#include "RealFft.h"

#include "JuceRealFft.h"
#include "PffftRealFft.h"

namespace investigate
{

std::unique_ptr<RealFft> createRealFft (FftBackend backend, int fftSize)
{
    switch (backend)
    {
        case FftBackend::juceReference:
            return std::make_unique<JuceRealFft> (fftSize);

        case FftBackend::pffft:
        default:
            return std::make_unique<PffftRealFft> (fftSize);
    }
}

} // namespace investigate
