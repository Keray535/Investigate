#pragma once

namespace investigate
{

/** Analysis window generators for `SpectrumAnalyser`.

    All windows are generated in the "periodic" (DFT) form, i.e. with an
    angular divisor of N rather than N-1. This is the standard convention
    for spectral *analysis* windows (as opposed to *filter design* windows,
    which use the symmetric N-1 form) - see F. J. Harris, "On the Use of
    Windows for Harmonic Analysis with the Discrete Fourier Transform,"
    Proc. IEEE, 66(1), 1978. The periodic form matters here for two
    concrete reasons, not just convention:
      - each window's own DTFT is then a finite sum of impulses at a small
        number of integer bin offsets, so a bin-centred sine's spectral
        leakage lands in only 1-3 neighbouring bins instead of spreading
        across the whole spectrum (this is what the Hi-Res leakage
        acceptance test in T-004 relies on);
      - it makes the coherent-gain normalisation in SpectrumAnalyser exact
        for a sine placed exactly on a bin.
*/
class WindowFunctions
{
public:
    enum class Type
    {
        hann,
        blackman,
        hiRes,
        flatTop
    };

    WindowFunctions() = delete;

    /** Fills `dest[0 .. size-1]` with the raw (unnormalised) window samples
        for the given type and size, and returns the window's coherent gain
        (the mean of the generated samples). `SpectrumAnalyser` uses the
        returned coherent gain to scale FFT bins so that a full-scale,
        bin-centred sine reads 0 dBFS, per the standard technique described
        in Harris 1978 and cross-checked in the T-002 research report
        (section 2).

        Real-time safety: writes only into `dest`, does not allocate. Not
        intended to be called from the audio thread (SpectrumAnalyser calls
        it only from the analysis thread, when settings change).
    */
    static double generate (Type type, float* dest, int size) noexcept;
};

} // namespace investigate
