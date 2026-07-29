/*
 * SVFResponse.h
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

/****************************************************************
 * @file SVFResponse.h
 * @brief Stateless magnitude response calculation for Cytomic-SVF-style  filters (see SVF.h).
****************************************************************/

namespace SVFResponse {

struct Coeffs {
    double m0 = 0.0, m1 = 0.0, m2 = 0.0;
    double a1 = 0.0, a2 = 0.0, a3 = 0.0;
};

inline Coeffs peak(double f0, double Q, double gainDB, double sr) {
    Coeffs c;
    const double A = std::pow(10.0, gainDB / 40.0);
    const double g = std::tan(M_PI * f0 / sr);
    const double k = 1.0 / (Q * A);
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    c.m0 = 1.0;
    c.m1 = k * (A * A - 1.0);
    c.m2 = 0.0;
    return c;
}

inline Coeffs lowShelf(double f0, double Q, double gainDB, double sr) {
    Coeffs c;
    const double A = std::pow(10.0, gainDB / 40.0);
    const double g = std::tan(M_PI * f0 / sr) / std::sqrt(A);
    const double k = 1.0 / Q;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    c.m0 = 1.0;
    c.m1 = k * (A - 1.0);
    c.m2 = (A * A - 1.0);
    return c;
}

inline Coeffs highShelf(double f0, double Q, double gainDB, double sr) {
    Coeffs c;
    const double A = std::pow(10.0, gainDB / 40.0);
    const double g = std::tan(M_PI * f0 / sr) * std::sqrt(A);
    const double k = 1.0 / Q;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    c.m0 = A * A;
    c.m1 = k * (1.0 - A) * A;
    c.m2 = (1.0 - A * A);
    return c;
}

inline Coeffs lowPass(double f0, double Q, double sr) {
    Coeffs c;
    const double g = std::tan(M_PI * f0 / sr);
    const double k = 1.0 / Q;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    c.m0 = 0.0;
    c.m1 = 0.0;
    c.m2 = 1.0;
    return c;
}

inline Coeffs highPass(double f0, double Q, double sr) {
    Coeffs c;
    const double g = std::tan(M_PI * f0 / sr);
    const double k = 1.0 / Q;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    c.m0 = 1.0;
    c.m1 = -k;
    c.m2 = -1.0;
    return c;
}

inline Coeffs notch(double f0, double Q, double sr) {
    Coeffs c;
    const double g = std::tan(M_PI * f0 / sr);
    const double k = 1.0 / Q;
    c.a1 = 1.0 / (1.0 + g * (g + k));
    c.a2 = g * c.a1;
    c.a3 = g * c.a2;
    c.m0 = 1.0;
    c.m1 = -k;
    c.m2 = 0.0;
    return c;
}

inline std::complex<double> response(const Coeffs& c, double freq, double sr) {
    const double w = 2.0 * M_PI * freq / sr;
    std::complex<double> z = std::polar(1.0, w);
    std::complex<double> alpha = 2.0 / (z + 1.0);

    std::complex<double> D1 = 1.0 - c.a1 * alpha;
    std::complex<double> D2 = 1.0 - (1.0 - c.a3) * alpha;

    std::complex<double> num = c.m1 * c.a2 * (1.0 - alpha)
                              + c.m2 * (c.a2 * c.a2 * alpha + c.a3 * D1);
    std::complex<double> den = D1 * D2 + c.a2 * c.a2 * alpha * alpha;

    return c.m0 + num / den;
}

inline double responseDB(const Coeffs& c, double freq, double sr) {
    return 20.0 * std::log10(std::max<double>(std::abs(response(c, freq, sr)), 1e-12));
}

inline std::complex<double> responseAtAlpha(const Coeffs& c, const std::complex<double>& alpha) {
    std::complex<double> D1 = 1.0 - c.a1 * alpha;
    std::complex<double> D2 = 1.0 - (1.0 - c.a3) * alpha;
    std::complex<double> num = c.m1 * c.a2 * (1.0 - alpha)
                              + c.m2 * (c.a2 * c.a2 * alpha + c.a3 * D1);
    std::complex<double> den = D1 * D2 + c.a2 * c.a2 * alpha * alpha;
    return c.m0 + num / den;
}

inline double responseDbAtAlpha(const Coeffs& c, const std::complex<double>& alpha) {
    return 10.0 * std::log10(std::max<double>(std::norm(responseAtAlpha(c, alpha)), 1e-24));
}

} // namespace SVFResponse
