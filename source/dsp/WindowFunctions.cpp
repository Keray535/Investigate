#include "WindowFunctions.h"

#include <cmath>

namespace investigate
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    // Classic 3-term Blackman window, a0 = 0.42 (the task's specified
    // variant; not the "exact" Blackman with a0 = 7938/18608).
    double blackman (double n, double invN) noexcept
    {
        constexpr double a0 = 0.42;
        constexpr double a1 = 0.5;
        constexpr double a2 = 0.08;
        const double phase = 2.0 * pi * n * invN;
        return a0 - a1 * std::cos (phase) + a2 * std::cos (2.0 * phase);
    }

    // 4-term Nuttall window. Nuttall, A. (1981), "Some windows with very
    // good sidelobe behavior," IEEE Trans. ASSP, 29(1), pp. 84-91.
    // Coefficients as reproduced by scipy.signal.windows.nuttall (the
    // "minimum 4-term Blackman-Harris" variant from that paper); peak
    // sidelobe level of a single such window is approximately -93 dB.
    double nuttall (double n, double invN) noexcept
    {
        constexpr double a0 = 0.3635819;
        constexpr double a1 = 0.4891775;
        constexpr double a2 = 0.1365995;
        constexpr double a3 = 0.0106411;
        const double phase = 2.0 * pi * n * invN;
        return a0 - a1 * std::cos (phase) + a2 * std::cos (2.0 * phase) - a3 * std::cos (3.0 * phase);
    }

    // 5-term flat-top window ("SFT5F"). Heinzel, G., Ruediger, A. &
    // Schilling, R. (2002), "Spectrum and spectral density estimation by
    // the Discrete Fourier transform (DFT), including a comprehensive list
    // of window functions and some new flat-top windows," Max-Planck-
    // Institut fuer Gravitationsphysik. Coefficients also reproduced by
    // scipy.signal.windows.flattop.
    double flatTop (double n, double invN) noexcept
    {
        constexpr double a0 = 0.21557895;
        constexpr double a1 = 0.41663158;
        constexpr double a2 = 0.277263158;
        constexpr double a3 = 0.083578947;
        constexpr double a4 = 0.006947368;
        const double phase = 2.0 * pi * n * invN;
        return a0 - a1 * std::cos (phase) + a2 * std::cos (2.0 * phase)
                   - a3 * std::cos (3.0 * phase) + a4 * std::cos (4.0 * phase);
    }
}

double WindowFunctions::generate (Type type, float* dest, int size) noexcept
{
    if (size <= 0)
        return 0.0;

    const double invN = 1.0 / static_cast<double> (size);
    double sum = 0.0;

    for (int n = 0; n < size; ++n)
    {
        double w = 0.0;
        const double nd = static_cast<double> (n);

        switch (type)
        {
            case Type::hann:
                w = 0.5 - 0.5 * std::cos (2.0 * pi * nd * invN);
                break;

            case Type::blackman:
                w = blackman (nd, invN);
                break;

            case Type::hiRes:
            {
                const double h = nuttall (nd, invN);
                w = h * h;
                break;
            }

            case Type::flatTop:
                w = flatTop (nd, invN);
                break;
        }

        dest[n] = static_cast<float> (w);
        sum += w;
    }

    return sum * invN;
}

} // namespace investigate
