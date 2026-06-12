
/*
 * FFTProcessor.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

#include <fftw3.h>

class FFTProcessor {
public:
    using Complex = std::complex<double>;
    using CVec = std::vector<Complex>;

    static CVec fft(const CVec& in) {
        int N = (int)in.size();

        fftw_complex *input = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
        fftw_complex *output = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);

        for (int i = 0; i < N; ++i) {
            input[i][0] = in[i].real();
            input[i][1] = in[i].imag();
        }

        fftw_plan p = fftw_plan_dft_1d(N, input, output, FFTW_FORWARD, FFTW_ESTIMATE);
        fftw_execute(p);

        CVec out(N);
        for (int i = 0; i < N; ++i)
            out[i] = Complex(output[i][0], output[i][1]);

        fftw_destroy_plan(p);
        fftw_free(input);
        fftw_free(output);

        return out;
    }

    static CVec ifft(const CVec& in) {
        int N = (int)in.size();

        fftw_complex *input = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);
        fftw_complex *output = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N);

        for (int i = 0; i < N; ++i) {
            input[i][0] = in[i].real();
            input[i][1] = in[i].imag();
        }

        fftw_plan p = fftw_plan_dft_1d(N, input, output, FFTW_BACKWARD, FFTW_ESTIMATE);
        fftw_execute(p);

        CVec out(N);
        for (int i = 0; i < N; ++i)
            out[i] = Complex(output[i][0] / N, output[i][1] / N);

        fftw_destroy_plan(p);
        fftw_free(input);
        fftw_free(output);

        return out;
    }

    static CVec safe_divide(const CVec& a, const CVec& b) {
        size_t n = a.size();
        CVec out(n);

        for (size_t i = 0; i < n; ++i) {
            double denom = std::norm(b[i]) + EPS;
            out[i] = a[i] * std::conj(b[i]) / denom;
        }

        return out;
    }

    static CVec mps(const CVec& s) {
        CVec log_s(s.size());

        for (size_t i = 0; i < s.size(); ++i)
            log_s[i] = std::log(std::max<double>(std::abs(s[i]), EPS));

        CVec cp = ifft(log_s);
        fold(cp);
        CVec out = fft(cp);

        for (auto& v : out)
            v = std::exp(v);

        return out;
    }

private:
    static constexpr double EPS = 1e-12;

    // Cepstrum min-phase
    static void fold(CVec& r) {
        size_t n = r.size();
        size_t nt = n / 2;

        for (size_t i = 1; i < nt; ++i)
            r[i] += std::conj(r[n - i]);

        for (size_t i = nt + 1; i < n; ++i)
            r[i] = 0.0;
    }

};
