/*
 * BiquadResponse.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 *
 */


#pragma once

#include <cmath>
#include <complex>
#include <algorithm>

#include "FilterConfig.h"

/****************************************************************
 * @file BiquadResponse.h
 * @brief Stateless magnitude response calculation for RBJ-style biquad filters (see Biquad.h).
****************************************************************/

namespace BiquadResponse {

using Type = FilterTypes::Type;

struct Coeffs {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
};

inline Coeffs compute(Type type, double frequency, double q, double gainDB, double sr) {
    Coeffs c;

    const double A     = std::pow(10.0, gainDB / 40.0);
    const double omega = 2.0 * M_PI * frequency / sr;
    const double sinw  = std::sin(omega);
    const double cosw  = std::cos(omega);

    if (q < 0.01) q = 0.01;
    const double alpha = sinw / (2.0 * q);

    double b0 = 0.0, b1 = 0.0, b2 = 0.0, a0 = 0.0, a1 = 0.0, a2 = 0.0;

    switch (type) {
        case Type::Peak:
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cosw;
            b2 = 1.0 - alpha * A;

            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cosw;
            a2 = 1.0 - alpha / A;
            break;

        case Type::LowShelf:
        {
            const double beta = std::sqrt(A) / q;

            b0 = A*((A+1)-(A-1)*cosw+beta*sinw);
            b1 = 2*A*((A-1)-(A+1)*cosw);
            b2 = A*((A+1)-(A-1)*cosw-beta*sinw);

            a0 = (A+1)+(A-1)*cosw+beta*sinw;
            a1 = -2*((A-1)+(A+1)*cosw);
            a2 = (A+1)+(A-1)*cosw-beta*sinw;
            break;
        }

        case Type::HighShelf:
        {
            const double beta = std::sqrt(A) / q;

            b0 = A*((A+1)+(A-1)*cosw+beta*sinw);
            b1 = -2*A*((A-1)+(A+1)*cosw);
            b2 = A*((A+1)+(A-1)*cosw-beta*sinw);

            a0 = (A+1)-(A-1)*cosw+beta*sinw;
            a1 = 2*((A-1)-(A+1)*cosw);
            a2 = (A+1)-(A-1)*cosw-beta*sinw;
            break;
        }

        case Type::LowPass:
            b0 = (1.0 - cosw) / 2.0;
            b1 =  1.0 - cosw;
            b2 = (1.0 - cosw) / 2.0;
            a0 =  1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 =  1.0 - alpha;
            break;

        case Type::HighPass:
            b0 =  (1.0 + cosw) / 2.0;
            b1 = -(1.0 + cosw);
            b2 =  (1.0 + cosw) / 2.0;
            a0 =   1.0 + alpha;
            a1 =  -2.0 * cosw;
            a2 =   1.0 - alpha;
            break;

        case Type::BandPass:
            b0 =  alpha;
            b1 =  0.0;
            b2 = -alpha;

            a0 =  1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 =  1.0 - alpha;
            break;

        case Type::Notch:
            b0 =  1.0;
            b1 = -2.0 * cosw;
            b2 =  1.0;

            a0 =  1.0 + alpha;
            a1 = -2.0 * cosw;
            a2 =  1.0 - alpha;
            break;
    }

    c.b0 = b0 / a0;
    c.b1 = b1 / a0;
    c.b2 = b2 / a0;
    c.a1 = a1 / a0;
    c.a2 = a2 / a0;
    return c;
}

inline std::complex<double> response(const Coeffs& c, double freq, double sr) {
    const double w = 2.0 * M_PI * freq / sr;
    std::complex<double> z = std::polar(1.0, -w);

    std::complex<double> num = c.b0 + c.b1 * z + c.b2 * z * z;
    std::complex<double> den = 1.0 + c.a1 * z + c.a2 * z * z;

    return num / den;
}

inline double responseDB(const Coeffs& c, double freq, double sr) {
    return 20.0 * std::log10(std::max<double>(std::abs(response(c, freq, sr)), 1e-12));
}

inline std::complex<double> responseAtZ(const Coeffs& c, const std::complex<double>& zInv,
                                                        const std::complex<double>& zInv2) {
    std::complex<double> num = c.b0 + c.b1 * zInv + c.b2 * zInv2;
    std::complex<double> den = 1.0 + c.a1 * zInv + c.a2 * zInv2;
    return num / den;
}

inline double responseDbAtZ(const Coeffs& c, const std::complex<double>& zInv,
                                            const std::complex<double>& zInv2) {
    return 10.0 * std::log10(std::max<double>(std::norm(responseAtZ(c, zInv, zInv2)), 1e-24));
}

} // namespace BiquadResponse
