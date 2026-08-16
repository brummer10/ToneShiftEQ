
/*
 * IREqualiser.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>

#include "Band.h"
#include "BiquadResponse.h"
#include "SVFResponse.h"

/****************************************************************
 * @file IREqualiser.h
 * @brief Frequency-domain magnitude synthesis for the FFT-mode (minimum-phase IR) parametric EQ.
****************************************************************/

class IREqualiser {
public:
    using Vec  = std::vector<double>;

    Vec buildBandSoloIR(const Band& b, const Vec mag_, double sr) {
        size_t n = mag_.size();
        Vec mag(n, -220.0);

        const double nyquist = sr * 0.5;
        double edgeWidth = 0.02; 

        for (size_t i = 1; i < n; ++i) {

            double freq = (double)i / (n - 1) * nyquist;
            double x = log_distance(freq, b.freq);

            double db = 0.0;
            double mask = 0.0;

            switch (b.type) {

                case Band::Peak:
                {
                    db = eval_peak_db(freq, b.freq, b.gain, b.Q);
                    double adb = fabs(db);
                    const double threshold = 0.5;
                    const double edge = 0.5;
                    
                    if (adb > threshold + edge) {
                        mask = 1.0;
                    } else if (adb > threshold - edge) {
                        mask = db_edge_fade(adb, threshold, edge);
                    } else {
                        mask = 0.0;
                    }
                    break;
                }
                case Band::LowShelf:
                {
                    db = eval_low_shelf(freq, b.freq, b.gain, b.Q);
                    if (x < -edgeWidth) mask = 1.0;
                    else if (x < edgeWidth) mask = 1.0 - edge_fade(x, edgeWidth);
                    else mask = 0.0;
                    break;
                }
                case Band::HighShelf:
                {
                    db = eval_high_shelf(freq, b.freq, b.gain, b.Q);
                    if (x > edgeWidth) mask = 1.0;
                    else if (x > -edgeWidth) mask = edge_fade(x, edgeWidth);
                    else mask = 0.0;
                    break;
                }
                case Band::Notch:
                {
                    db = eval_notch_db(freq, b.freq, b.Q, sr);
                    double adb = fabs(db);
                    const double threshold = 0.5;
                    const double edge = 0.5;

                    if (adb > threshold + edge) {
                        mask = 1.0;
                    } else if (adb > threshold - edge) {
                        mask = db_edge_fade(adb, threshold, edge);
                    } else {
                        mask = 0.0;
                    }
                    break;
                }
            }
            mag[i] = db * mask + (-220.0) * (1.0 - mask);
        }

        //mag[0] = mag[1];
        return mag;
    }

    Vec buildBandMuteIR(const Band& b, const Vec mag_, double sr) {
        size_t n = mag_.size();
        Vec mag = mag_;
        const double nyquist = sr * 0.5;
        const double threshold = 1.0;
        const double edge = 0.5;

        for (size_t i = 1; i < n; ++i) {
            double freq = (double)i / (n - 1) * nyquist;
            double x = log_distance(freq, b.freq);
            double db = 0.0;
            double remove_mask = 0.0;

            switch (b.type) {
                case Band::Peak:
                {
                    db = eval_peak_db(freq, b.freq, b.gain, b.Q);
                    double adb = fabs(db);

                    if (adb > threshold + edge)
                        remove_mask = 1.0;
                    else if (adb > threshold - edge)
                        remove_mask = db_edge_fade(adb, threshold, edge);
                    else
                        remove_mask = 0.0;
                    break;
                }
                case Band::LowShelf:
                {
                    if (x < -edge) remove_mask = 1.0;
                    else if (x < edge) remove_mask = 1.0 - db_edge_fade(x + edge, edge, edge);
                    else remove_mask = 0.0;
                    break;
                }
                case Band::HighShelf:
                {
                    if (x > edge) remove_mask = 1.0;
                    else if (x > -edge) remove_mask = db_edge_fade(x + edge, edge, edge);
                    else remove_mask = 0.0;
                    break;
                }
                case Band::Notch:
                {
                    db = eval_notch_db(freq, b.freq, b.Q, sr);
                    double adb = fabs(db);

                    if (adb > threshold + edge)
                        remove_mask = 1.0;
                    else if (adb > threshold - edge)
                        remove_mask = db_edge_fade(adb, threshold, edge);
                    else
                        remove_mask = 0.0;
                    break;
                }
            }
            double keep = 1.0 - remove_mask;
            mag[i] = mag[i] * keep + (-220.0) * remove_mask;
        }
        //mag[0] = mag[1];
        return mag;
    }


    static void apply_all_biquad(Vec& mag, double sr, const Band* bands, size_t count,
            bool lowCutEnabled, double lowCutFreq, bool highCutEnabled, double highCutFreq) {
        const size_t n = mag.size();
        if (!n) return;
        const double nyquist = sr * 0.5;

        std::vector<BiquadResponse::Coeffs> active;
        active.reserve(count + 2);

        if (lowCutEnabled)
            active.push_back(BiquadResponse::compute(FilterTypes::Type::HighPass, lowCutFreq, 1.0, 1.0, sr));
        if (highCutEnabled)
            active.push_back(BiquadResponse::compute(FilterTypes::Type::LowPass, highCutFreq, 1.0, 1.0, sr));

        for (size_t bi = 0; bi < count; ++bi) {
            const Band& b = bands[bi];
            if (!b.enabled) continue;
            FilterTypes::Type ftype = b.type == Band::LowShelf  ? FilterTypes::Type::LowShelf
                                     : b.type == Band::HighShelf ? FilterTypes::Type::HighShelf
                                     : b.type == Band::Notch ? FilterTypes::Type::Notch
                                                                   : FilterTypes::Type::Peak;
            active.push_back(BiquadResponse::compute(ftype, b.freq, mapQ(b.Q), b.gain, sr));
        }
        if (active.empty()) return;

        for (size_t i = 0; i < n; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            double w = 2.0 * M_PI * f / sr;
            std::complex<double> zInv  = std::polar(1.0, -w);
            std::complex<double> zInv2 = zInv * zInv;

            double sumDb = 0.0;
            for (auto& c : active)
                sumDb += BiquadResponse::responseDbAtZ(c, zInv, zInv2);
            mag[i] += sumDb;
        }
    }

    static void apply_all_svf(Vec& mag, double sr, const Band* bands, size_t count,
            bool lowCutEnabled, double lowCutFreq,  bool highCutEnabled, double highCutFreq) {
        const size_t n = mag.size();
        if (!n) return;
        const double nyquist = sr * 0.5;

        std::vector<SVFResponse::Coeffs> active;
        active.reserve(count + 2);

        if (lowCutEnabled)  active.push_back(SVFResponse::highPass(lowCutFreq, 1.0, sr));
        if (highCutEnabled) active.push_back(SVFResponse::lowPass(highCutFreq, 1.0, sr));

        for (size_t bi = 0; bi < count; ++bi) {
            const Band& b = bands[bi];
            if (!b.enabled) continue;
            double Q = mapQ(b.Q);
            active.push_back(
                b.type == Band::LowShelf  ? SVFResponse::lowShelf(b.freq, Q, b.gain, sr)
              : b.type == Band::HighShelf ? SVFResponse::highShelf(b.freq, Q, b.gain, sr)
              : b.type == Band::Notch     ? SVFResponse::notch(b.freq, Q, sr)
                                            : SVFResponse::peak(b.freq, Q, b.gain, sr));
        }
        if (active.empty()) return;

        for (size_t i = 0; i < n; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            double w = 2.0 * M_PI * f / sr;
            std::complex<double> z     = std::polar(1.0, w);
            std::complex<double> alpha = 2.0 / (z + 1.0);

            double sumDb = 0.0;
            for (auto& c : active)
                sumDb += SVFResponse::responseDbAtAlpha(c, alpha);
            mag[i] += sumDb;
        }
    }

    static void apply_spectral_dynamics(Vec& mag, double sr, const Band* bands, size_t count,
                    const Vec& sidechainDb, double thresholdDb, double tilt, double amount) {
        const size_t n = mag.size();
        if (!n || sidechainDb.size() != n) return;

        const double nyquist = sr * 0.5;

        for (size_t i = 0; i < n; ++i) {
            double freq = (double)i / (n - 1) * nyquist;
            if (freq < 1.0) continue;

            double tiltOffsetDB = tilt * std::log2(freq / 1000.0);
            double baseThreshold = thresholdDb + tiltOffsetDB;
            double response = 0.0;

            for (size_t bi = 0; bi < count; ++bi) {
                const Band& b = bands[bi];
                if (!b.enabled) continue;

                double m = 0.0;
                switch (b.type) {
                    case Band::Peak:
                        m = eval_peak_db(freq, b.freq, 1.0, b.Q);
                        break;
                    case Band::LowShelf:
                        m = eval_low_shelf(freq, b.freq, 1.0, b.Q);
                        break;
                    case Band::HighShelf:
                        m = eval_high_shelf(freq, b.freq, 1.0, b.Q);
                        break;
                    default:
                        continue;
                }
                if (m <= 0.0) continue;

                double ratio = 3.0;
                switch (b.ratio) {
                    case 0: ratio = 2.0;  break;
                    case 1: ratio = 3.0;  break;
                    case 2: ratio = 4.0;  break;
                    case 3: ratio = 5.0;  break;
                    case 4: ratio = 10.0; break;
                    default: ratio = 3.0; break;
                }

                double band_threshold = b.threshold * (1.0 - (1.0 / ratio));
                double excess = sidechainDb[i] - (baseThreshold + band_threshold);
                if (excess <= 0.0) continue;

                double r = excess * m * amount;
                if (b.expander) {
                    const double max_boost = 12.0;
                    r = std::tanh(r / max_boost) * max_boost;
                }
                response += b.expander ? r : -r;
            }

            if (response != 0.0) mag[i] += response;
        }
    }

    static double mapQ(double q_ui) {
        return std::clamp(q_ui, 0.1, 10.0);
    }

    static double mapQp(double q_ui) {
        // clamp UI range
        q_ui = std::clamp(q_ui, 0.1, 10.0);
        // log-space mapping
        double x = std::log(q_ui);
        // soften curve
        double shaped = std::tanh(x * 0.8);
        // back to linear
        double q = std::exp(shaped * 1.5);
        return q;
    }

    static double q_to_sigma(double q) {
        return 1.0 / (1.5 * q + 0.5);
    }

    static inline double log_distance(double f, double f0) {
        return std::log2((f + 1e-9) / (f0 + 1e-9));
    }

    static double eval_peak_db(double freq, double f0, double gain, double Q_ui) {
        double Q = mapQp(Q_ui);
        double sigma = q_to_sigma(Q);
        double x = std::log2((f0 + 1e-9) / (freq + 1e-9));
        double g = std::exp(-0.5 * (x * x) / (sigma * sigma));
        return gain * g;
    }

    static void apply_peak(Vec& mag, double sr,
                double freq, double gain_db, double Q_ui) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;
        double Q = mapQp(Q_ui);
        double sigma = q_to_sigma(Q);

        for (size_t i = 1; i < n; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            if (f < 10.0) continue;
            double x = std::log2((f + 1e-9) / (freq + 1e-9));
            double g = std::exp(-0.5 * (x * x) / (sigma * sigma));
            mag[i] += gain_db * g;
        }
    }

    static double eval_low_shelf(double freq, double f0, double gain, double Q) {
        double slope = mapQp(Q) * 2.0; // mapQ(Q) * 1.5;
        double x = log_distance(freq, f0);

        double g = 0.5 * (1.0 - std::tanh(slope * x));
        return gain * g;
    }

    static void apply_low_shelf(Vec& mag, double sr,
                    double freq, double gain_db, double Q) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;
        double slope = mapQp(Q) * 2.0; //mapQ(Q) * 1.5;
        double f_min = 2.0 * nyquist / (n - 1);
        double g_dc = 0.5 * (1.0 - std::tanh(slope * log_distance(f_min, freq)));

        for (size_t i = 0; i < n; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            //if (f < 10.0) continue;
            double x = log_distance(f, freq);
            double g;
            if (f <= f_min) g = g_dc; 
            else g = 0.5 * (1.0 - std::tanh(slope * x));
            mag[i] += gain_db * g;
        }
    }

    static double eval_high_shelf(double freq, double f0, double gain, double Q) {
        double slope = mapQp(Q) * 2.0;
        double x = log_distance(freq, f0);

        double g = 0.5 * (1.0 + std::tanh(slope * x));
        return gain * g;
    }

    static void apply_high_shelf(Vec& mag, double sr,
                    double freq, double gain_db, double Q) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;
        double slope =  mapQp(Q) * 2.0; //mapQ(Q) * 1.5;
        double f_max = (n - 2) * nyquist / (n - 1);
        double g_nyq = 0.5 * (1.0 + std::tanh(slope * log_distance(f_max, freq)));

        for (size_t i = 1; i < n; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            if (f < 10.0) continue;
            double g;
            if (f >= f_max) g = g_nyq;
            else g = 0.5 * (1.0 + std::tanh(slope * log_distance(f, freq)));
            mag[i] += gain_db * g;
        }
    }

    static double eval_notch_db(double freq, double f0, double Q_ui, double sr) {
        double Q = mapQ(Q_ui);
        auto c = BiquadResponse::compute(FilterTypes::Type::Notch, f0, Q, 0.0, sr);
        return BiquadResponse::responseDB(c, freq, sr);
    }

    static void apply_notch(Vec& mag, double sr, double freq, double Q_ui) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;
        double Q = mapQ(Q_ui);
        auto c = BiquadResponse::compute(FilterTypes::Type::Notch, freq, Q, 0.0, sr);

        for (size_t i = 1; i < n; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            if (f < 10.0) continue;
            mag[i] += BiquadResponse::responseDB(c, f, sr);
        }
    }

    static void apply_high_rolloff(Vec& mag, double sr, double cutoff, double Q = 0.707) {
        applyRolloff(mag, sr, cutoff, lowPassCoeffs(cutoff, sr, Q), true);
    }

    static void apply_low_rolloff(Vec& mag, double sr, double cutoff, double Q = 0.707) {
        applyRolloff(mag, sr, cutoff, highPassCoeffs(cutoff, sr, Q), false);
    }

private:
    struct BiquadCoeffs { double b0, b1, b2, a1, a2; };

    static BiquadCoeffs lowPassCoeffs(double cutoff, double sr, double Q) {
        double w0 = 2.0 * M_PI * cutoff / sr;
        double c0 = std::cos(w0), s0 = std::sin(w0);
        double alpha = s0 / (2.0 * Q);
        double a0 = 1.0 + alpha;
        return {
            (1.0 - c0) * 0.5 / a0,
            (1.0 - c0)       / a0,
            (1.0 - c0) * 0.5 / a0,
            -2.0 * c0        / a0,
            (1.0 - alpha)    / a0
        };
    }

    static BiquadCoeffs highPassCoeffs(double cutoff, double sr, double Q) {
        double w0 = 2.0 * M_PI * cutoff / sr;
        double c0 = std::cos(w0), s0 = std::sin(w0);
        double alpha = s0 / (2.0 * Q);
        double a0 = 1.0 + alpha;
        return {
             (1.0 + c0) * 0.5 / a0,
            -(1.0 + c0)       / a0,
             (1.0 + c0) * 0.5 / a0,
            -2.0 * c0         / a0,
             (1.0 - alpha)    / a0
        };
    }

    static double biquadMagnitude(const BiquadCoeffs& c, double f, double sr) {
        double w = 2.0 * M_PI * f / sr;
        std::complex<double> z = std::polar(1.0, -w);
        std::complex<double> num = c.b0 + c.b1 * z + c.b2 * z * z;
        std::complex<double> den = 1.0 + c.a1 * z + c.a2 * z * z;
        return std::abs(num) / std::max(std::abs(den), EPS);
    }

    static void applyRolloff(Vec& mag, double sr, double cutoff,
                              const BiquadCoeffs& coeffs, bool aboveCutoff) {
        const size_t n = mag.size();
        const double nyquist = sr * 0.5;
        const size_t cut = std::min<size_t>((size_t)(cutoff / nyquist * (n - 1)), n - 1);

        const double anchorLin = db2lin(mag[cut]);
        const double h0 = biquadMagnitude(coeffs, cutoff, sr);

        const size_t begin = aboveCutoff ? cut : 0;
        const size_t end   = aboveCutoff ? n   : cut + 1;

        for (size_t i = begin; i < end; ++i) {
            double f = (double)i / (n - 1) * nyquist;
            if (!aboveCutoff && f < 1.0) f = 1.0; // avoid the DC bin
            double h = biquadMagnitude(coeffs, f, sr) / h0;
            mag[i] = db(anchorLin * h);
        }
    }

    static constexpr double EPS = 1e-12;

    static double db(double x) {
        return 20.0 * std::log10(std::max<double>(x, EPS));
    }

    static double db2lin(double x) {
        return std::pow(10.0, x / 20.0);
    }

    static inline double db_edge_fade(double db, double threshold, double width) {
        double t = (db - (threshold - width)) / (2.0 * width);
        t = std::clamp(t, 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

    static inline double edge_fade(double x, double width) {
        double t = (x + width) / (2.0 * width);
        t = std::clamp(t, 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    }

};
