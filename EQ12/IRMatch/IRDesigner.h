
/*
 * IRDesigner.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

/****************************************************************
 * Adaptive logarithmic spectrum smoother.
 *
 * Applies a Gaussian smoothing kernel with constant width in octaves.
 *
 * Intended for spectral curve processing rather than real-time
 * audio filtering.
 *
 * Thread safety:
 * The internal kernel cache is not synchronized.
 * One instance should only be accessed from a single thread.
****************************************************************/
 
template<typename Vec>
class AdaptiveLogSmoother {
public:

    AdaptiveLogSmoother() = default;

    void prepare(size_t bins, double sampleRate, double sigmaOct = 0.18) {
        buildSmoothKernel(bins, sampleRate, sigmaOct);
    }

    void process(const Vec& in, Vec& out) {
        if (out.size() != in.size())
            out.resize(in.size());

        out[0] = in[0];

        for (size_t i = 1; i < in.size(); ++i) {
            const SmoothKernel& k = smoothKernel_[i];
            const auto* src = in.data() + k.first;
            double sum = 0.0;

            for (size_t j = 0; j < k.weight.size(); ++j)
                sum += src[j] * k.weight[j];

            out[i] = sum;
        }
    }

    Vec process(const Vec& in) {
        if (smoothBuffer_.size() != in.size())
            smoothBuffer_.resize(in.size());

        process(in, smoothBuffer_);
        return smoothBuffer_;
    }

    void setMaxKernel(size_t taps) {
        maxKernel_ = std::max<size_t>(3, taps | 1);
        reset();
    }

    void reset() {
        smoothKernel_.clear();
        kernelBins_ = 0;
        kernelSR_ = 0.0;
        kernelSigma_ = 0.0;
    }

private:

    struct SmoothKernel {
        uint16_t first = 0;
        std::vector<float> weight;
    };

    std::vector<SmoothKernel> smoothKernel_;
    mutable Vec smoothBuffer_;
    size_t maxKernel_ = 257;
    size_t kernelBins_ = 0;
    double kernelSR_ = 0.0;
    double kernelSigma_ = 0.0;

    void buildSmoothKernel(size_t bins, double sr, double sigmaOct) {
        if (kernelBins_ == bins && kernelSR_ == sr && kernelSigma_ == sigmaOct)
            return;

        kernelBins_ = bins;
        kernelSR_ = sr;
        kernelSigma_ = sigmaOct;
        smoothKernel_.clear();
        smoothKernel_.resize(bins);

        const double nyquist = sr * 0.5;
        const double radius = 3.0;

        for (size_t i = 1; i < bins; ++i) {
            double fi = (double)i / (bins - 1) * nyquist;
            double fMin = fi * std::exp2(-radius * sigmaOct);
            double fMax = fi * std::exp2( radius * sigmaOct);
            size_t j1 = std::max<size_t>(1, (size_t)(fMin / nyquist * (bins - 1)));
            size_t j2 = std::min<size_t>(bins - 1, (size_t)(fMax / nyquist * (bins - 1)));
            size_t count = j2 - j1 + 1;

            if (count > maxKernel_) {
                size_t center = (j1 + j2) / 2;
                if (center > maxKernel_ / 2)
                    j1 = center - maxKernel_ / 2;
                else
                    j1 = 1;

                j2 = std::min(j1 + maxKernel_ - 1, bins - 1);
                if (j2 - j1 + 1 < maxKernel_) {
                    if (j2 + 1 > maxKernel_)
                        j1 = j2 - maxKernel_ + 1;
                    else
                        j1 = 1;
                }
            }

            SmoothKernel& k = smoothKernel_[i];

            k.first = static_cast<uint16_t>(j1);
            k.weight.resize(j2 - j1 + 1);

            const double centerFreq = std::sqrt(fMin * fMax);
            double norm = 0.0;
            for (size_t j = j1; j <= j2; ++j) {
                double fj = (double)j / (bins - 1) * nyquist;
                double d = std::log2(fj / centerFreq);
                float w = (float)std::exp(-0.5 * d * d / (sigmaOct * sigmaOct));
                k.weight[j - j1] = w;
                norm += w;
            }

            float inv = 1.0f / (float)norm;

            for (float& w : k.weight)
                w *= inv;
        }
    }
};

class IRDesigner {
public:
    using Vec  = std::vector<double>;

    static Vec lerpv(const Vec& a, const Vec& b, double t) {
        size_t n = a.size();
        Vec out(n);

        for (size_t i = 0; i < n; ++i)
            out[i] = a[i] * (1.0 - t) + b[i] * t;

        return out;
    }

    static Vec harmonic_refine(const Vec& mag, double sr) {
        size_t n = mag.size();
        Vec out = mag;
        const double nyquist = sr * 0.5;

        for (size_t i = 2; i < n - 2; ++i) {

            double freq = (double)i / (n - 1) * nyquist;
            if (freq < 80.0) continue;
            // local neighborhood
            double m2 = mag[i - 2];
            double m1 = mag[i - 1];
            double m0 = mag[i];
            double p1 = mag[i + 1];
            double p2 = mag[i + 2];
            // detect harmonic ridge
            double local_max = std::max<double>({m2, m1, m0, p1, p2});
            double local_avg = (m2 + m1 + p1 + p2) * 0.25;
            double contrast = local_max - local_avg;
            // only act when there's structure
            if (contrast < 1.0) continue;
            // pull neighbors slightly toward structure
            double pull = 0.15;
            //if (freq > 4000.0) pull *= 0.5;
            if (m0 < local_max) {
                out[i] = m0 + (local_max - m0) * pull;
            }
        }

        return out;
    }

    Vec adaptive_log_smooth(const Vec& mag, double sr, double sigmaOct = 0.18) {
        smoother_.prepare(mag.size(), sr);
        return smoother_.process(mag);
    }

    static Vec soften_peaks(const Vec& mag, double amount) {
        Vec out = mag;

        for (size_t i = 1; i < mag.size() - 1; ++i) {
            if (!is_peak(mag, i)) continue;
            double local_avg = (mag[i - 1] + mag[i + 1]) * 0.5;
            double excess = mag[i] - local_avg;

            if (excess > 0.0)
                out[i] = mag[i] - excess * amount;
        }
        return out;
    }

    static Vec spectral_duck(const Vec& mag, const Vec& sidechainDb,
                double thresholdDb, double amount, double tilt, double sr) {
        Vec out = mag;
        size_t n = mag.size();
        const double nyquist = sr * 0.5;

        for (size_t i = 0; i < n; ++i) {
            double freq = (double)i / (n - 1) * nyquist;
            if (freq < 1.0) continue;
            double tiltOffsetDB = tilt * std::log2(freq / 1000.0f);
            double excess = sidechainDb[i]  - thresholdDb - tiltOffsetDB;
            if (excess > 0.0) {
                out[i] -= excess * amount;
                
            }
        }
        return out;
    }

    static Vec spectral_duck(const Vec& mag, const Vec& sidechainDb,
                              double thresholdDb, double amount,
                              double centerFreq, double widthOct,
                              double sr) {
        Vec out = mag;
        size_t n = mag.size();
        const double nyquist = sr * 0.5;
        const double edgeOct = 0.25;

        for (size_t i = 0; i < n; ++i) {
            double freq = (double)i / (n - 1) * nyquist;
            double excess = sidechainDb[i] - thresholdDb;
            if (excess <= 0.0) continue;

            double distOct = std::log2(std::max(freq, 1.0) / centerFreq);
            double halfW = widthOct * 0.5;
            double edgeDist = halfW - std::abs(distOct);
            double mask = std::clamp(edgeDist / edgeOct, 0.0, 1.0);
            mask = 0.5 - 0.5 * std::cos(mask * M_PI);

            out[i] -= excess * amount * mask;
        }
        return out;
    }

    static Vec spectral_dynamics(const Vec& mag, Vec& smooth, double amount, double tilt, double sr) {
        //Vec smooth = adaptive_log_smooth(mag, sr);
        Vec out = mag;
        size_t n = mag.size();
        const double nyquist = sr * 0.5;
        //const double max_boost = 12.0;
        double lo = *std::min_element(mag.begin(), mag.end());
        double hi = *std::max_element(mag.begin(), mag.end());
        const double max_boost = std::max(12.0, (hi - lo) * 0.6);

        for (size_t i = 0; i < mag.size(); ++i) {
            double d = mag[i] - smooth[i];
            double factor = std::pow(2.0, amount);
            double freq = (double)i / (n - 1) * nyquist;
            double norm = std::log(freq / 20.0) / std::log(nyquist / 20.0);
            norm = std::clamp(norm, 0.0, 1.0);
            double centered = (norm - 0.5) * 2.0;
            double weight = 1.0 + tilt * std::tanh(centered);
            if (freq < 40.0) weight *= 0.5;
            weight = std::clamp(weight, 0.0, 2.0);
            double delta = d * factor * weight;
            delta = std::tanh(delta / max_boost) * max_boost;
            out[i] = smooth[i] + delta;
        }
        // dc block
        //if (n > 1) out[0] = out[1];
        return out;
    }

    static void reconstruct_low_end(Vec& mag, double sr) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;
        size_t end = (size_t)((150.0 / nyquist) * (n - 1));
        end = std::min(end, n - 1);
        Vec xs, ys;
        build_log_points(mag, xs, ys, sr, 16);
        clamp_outliers(ys);
        smooth_points(ys);

        for (size_t i = 1; i < end; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            double x = std::log(f + 1.0);
            mag[i] = interp_monotonic(xs, ys, x);
        }
        mag[0] = mag[1];
    }

    static void smooth_low_end_log(Vec& mag, double sr) {
        size_t n = mag.size();
        size_t end = (size_t)((150.0 / (sr * 0.5)) * (n - 1));
        end = std::min(end, n - 1);
        Vec logMag = mag;
        for (size_t i = 2; i < end; ++i) {
            double a = logMag[i - 1];
            double b = logMag[i];
            double c = logMag[i + 1];
            double smoothed = (a + b + c) / 3.0;
            mag[i] = 0.7 * smoothed + 0.3 * b;
        }
        if (n > 1) mag[0] = mag[1];
    }

    static void smooth_low_end_hermite(Vec& mag, double sr) {
        size_t n = mag.size();
        size_t end = (size_t)((150.0 / (sr * 0.5)) * (n - 1));
        end = std::min(end, n - 3);
        Vec logMag = mag;
        for (size_t i = 1; i < end - 1; ++i) {
            double p0 = logMag[i - 1];
            double p1 = logMag[i];
            double p2 = logMag[i + 1];
            double p3 = logMag[i + 2];
            double m1 = 0.5 * (p2 - p0);
            double m2 = 0.5 * (p3 - p1);
            mag[i] = hermite(p1, p2, m1, m2, 0.5);
        }
        if (n > 1) mag[0] = mag[1];
    }

private:
    AdaptiveLogSmoother<Vec> smoother_;

    static double hermite(double p0, double p1,
                          double m0, double m1,
                          double t) {
        double t2 = t * t;
        double t3 = t2 * t;

        double h00 =  2*t3 - 3*t2 + 1;
        double h10 =      t3 - 2*t2 + t;
        double h01 = -2*t3 + 3*t2;
        double h11 =      t3 - t2;

        return h00*p0 + h10*m0 + h01*p1 + h11*m1;
    }

    static double getSmoothing(double freq) {
        double x = std::log10(freq + 1.0);
        double s = 0.25 + 0.15 * x + 0.08 * x * x;
        return std::min(s, 1.8);
    }

    static Vec build_prefix_sum(const Vec& mag) {
        Vec ps(mag.size() + 1, 0.0);
        for (size_t i = 0; i < mag.size(); ++i)
            ps[i + 1] = ps[i] + mag[i];
        return ps;
    }

    static double range_sum(const Vec& ps, size_t i1, size_t i2) {
        return ps[i2 + 1] - ps[i1];
    }

    static bool is_peak(const Vec& mag, size_t i) {
        return mag[i] > mag[i - 1] && mag[i] > mag[i + 1];
    }

    static void build_log_points(const Vec& mag, Vec& xs, Vec& ys,
                                    double sr, int numPoints = 16) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;
        double fMin = 20.0;
        double fMax = 150.0;
        xs.resize(numPoints);
        ys.resize(numPoints);

        for (int i = 0; i < numPoints; ++i) {
            double t = (double)i / (numPoints - 1);
            double f = fMin * std::pow(fMax / fMin, t);
            size_t idx = (size_t)((f / nyquist) * (n - 1));
            idx = std::clamp(idx, (size_t)1, n - 1);
            xs[i] = std::log(f);
            ys[i] = mag[idx];
        }
    }

    static void clamp_outliers(Vec& ys) {
        for (size_t i = 1; i < ys.size() - 1; ++i) {
            double lo = std::min(ys[i - 1], ys[i + 1]);
            double hi = std::max(ys[i - 1], ys[i + 1]);
            ys[i] = std::clamp(ys[i], lo, hi);
        }
    }

    static void smooth_points(Vec& ys) {
        Vec tmp = ys;
        for (size_t i = 1; i < ys.size() - 1; ++i) {
            ys[i] = 0.25 * tmp[i - 1] + 0.5 * tmp[i] + 0.25 * tmp[i + 1];
        }
    }

    static double interp_monotonic( const Vec& xs, const Vec& ys, double x) {
        size_t n = xs.size();

        for (size_t i = 1; i < n; ++i) {
            if (x <= xs[i]) {
                double x0 = xs[i - 1];
                double x1 = xs[i];
                double y0 = ys[i - 1];
                double y1 = ys[i];
                double t = (x - x0) / (x1 - x0 + 1e-12);
                double m = (y1 - y0);
                return y0 + t * m;
            }
        }
        return ys.back();
    }

};
