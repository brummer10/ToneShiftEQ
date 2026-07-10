/*
 * IRProcessor.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <complex>
#include <algorithm>
#include <utility>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "Band.h"
#include "FFTProcessor.h"
#include "IREqualiser.h"
#include "IRDesigner.h"

class IRProcessor {
public:
    using Complex = std::complex<double>;
    using CVec = std::vector<Complex>;
    using Vec  = std::vector<double>;

    std::atomic<bool> workerReady {false};
    std::atomic<bool> workerBusy {false};

    int solo_band = 0;
    int solo_enabled = 0;
    int lowcut_enabled = 0;
    int highcut_enabled = 0;
    int hf_fade = 0;

    double lowcut = 19.0;
    double highcut = 22000.0;
    double smooth_amount = 0.3;
    double dynamics_amount = 0.0;
    double tilt_amount = 0.0;

    ~IRProcessor() {
        stopWorker();
    }

    IRProcessor() {
        front.store(&bufferA);
        back = &bufferB;
        startWorker();
    }

    struct alignas(64) IRChannelData {
        Vec ref;
        Vec diff;
        Vec src;
        double peak = 0.0;
    };

    struct alignas(64) IRData {
        IRChannelData left;
        IRChannelData right;
    };

    Band bands[12] = {
        // Low Shelf
        {1, Band::LowShelf,    40.0,   0.0, 0.7, 0},

        // Bass
        {1, Band::Peak,        70.0,   0.0, 1.0, 0},
        {1, Band::Peak,       120.0,   0.0, 1.0, 0},

        // Low Mid
        {1, Band::Peak,       210.0,   0.0, 1.0, 0},
        {1, Band::Peak,       370.0,   0.0, 1.0, 0},

        // Mid
        {1, Band::Peak,       650.0,   0.0, 1.0, 0},
        {1, Band::Peak,      1150.0,   0.0, 1.0, 0},

        // Upper Mid
        {1, Band::Peak,      2000.0,   0.0, 1.0, 0},
        {1, Band::Peak,      3500.0,   0.0, 1.0, 0},

        // Presence / Air
        {1, Band::Peak,      6100.0,   0.0, 1.0, 0},
        {1, Band::Peak,     10700.0,   0.0, 1.0, 0},

        // High Shelf
        {1, Band::HighShelf, 18000.0,  0.0, 0.7, 0}
    };

    void computeIR(const Vec& refL, const Vec& refR,
                   const Vec& srcL, const Vec& srcR,
                   double sampleRate_, size_t irLength_ = 4096,
                   bool rebuild = false, size_t fftSize = 0) {

        sampleRate = sampleRate_;
        irLength = irLength_;
        haveFreshSource = rebuild;

        haveReference = !refL.empty() || !refR.empty();
        haveSource = !srcL.empty() || !srcR.empty();

        size_t maxAnalysisSize = sampleRate * 4;

        Vec refL_trunc = center_crop(refL, maxAnalysisSize);
        Vec refR_trunc = center_crop(refR, maxAnalysisSize);

        Vec srcL_trunc = center_crop(srcL, maxAnalysisSize);
        Vec srcR_trunc = center_crop(srcR, maxAnalysisSize);

        size_t maxSize = std::max({ refL_trunc.size(), refR_trunc.size(),
                                    srcL_trunc.size(), srcR_trunc.size() });

        analysisN = (fftSize > 0) ? fftSize : next_pow2(maxSize);
        analysisN = std::max<size_t>(analysisN, irLength * 2);
        synthesisN = next_pow2(irLength * 2);

        updateIR(refL_trunc, refR_trunc, srcL_trunc, srcR_trunc, rebuild);
    }

    void computeIR(double sampleRate_, size_t irLength_ = 4096,
                   bool rebuild = false, size_t fftSize = 0) {

        sampleRate = sampleRate_;
        irLength = 4096;
        haveFreshSource = rebuild;

        haveReference = false;
        haveSource = false;

        analysisN = next_pow2(irLength * 2);
        synthesisN = next_pow2(irLength * 2);

        updateIR();
    }

    std::pair<Vec, Vec> createIRStereo() {
        if (!mag_ir_L_.size()) make_flat(mag_ir_L_, analysisN / 2 + 1);
        if (!mag_ir_R_.size()) make_flat(mag_ir_R_, analysisN / 2 + 1);

        auto buildChannel = [this](const Vec& mag, Vec& phaseOut) {
            Vec synthMag = remap_mag_bins(mag, analysisN, synthesisN);
            if (hf_fade) applyHighFrequencyFade(synthMag, sampleRate);

            CVec Hs = spectrum2fft(synthMag);
            CVec Hmin = fp.mps(Hs);

            size_t phaseResolution = 1024;
            phaseOut.resize(phaseResolution);
            for (size_t k = 0; k < phaseResolution; ++k) {
                size_t srcBin = k * (synthesisN / 2 + 1) / phaseResolution;
                phaseOut[k] = std::arg(Hmin[srcBin]) * (180.0 / M_PI);
            }

            CVec ir_full = fp.ifft(Hmin);
            size_t tail = std::min<size_t>(64, irLength / 4);
            apply_window(ir_full, tail);
            Vec ir(irLength);
            for (size_t i = 0; i < irLength; ++i)
                ir[i] = ir_full[i].real();
            return ir;
        };

        Vec phaseL, phaseR;
        auto irL = buildChannel(mag_ir_L_, phaseL);
        auto irR = buildChannel(mag_ir_R_, phaseR);

        size_t n = std::min(phaseL.size(), phaseR.size());
        gui_phase_.resize(n);
        constexpr float phaseRange = 180.0f;
        constexpr float dbRange = 12.0f;
        for (size_t i = 0; i < n; ++i) {
            float phase = (float)(0.5 * (phaseL[i] + phaseR[i]));
            gui_phase_[i] = (phase / phaseRange) * dbRange;
        }

        return { irL, irR };
    }

    Vec createIR() {
        std::pair<Vec, Vec> ir = createIRStereo();
        return mergeAverage(ir.first, ir.second);
    }

    const std::vector<float>& getIRMag() const { return gui_ir_; }
    const std::vector<float>& getIRPhase() const { return gui_phase_; }
    const Vec& getDiffMag() const { return gui_diff_; }
    const Vec& getRefMag() const { return gui_ref_; }
    const Vec& getSrcMag() const { return gui_src_; }

    void setLowCut(double lc) { lowcut = lc; }
    void setLowCutEnabled(int lc) { lowcut_enabled = lc; }
    void setHighCut(double hc) { highcut = hc; }
    void setHighCutEnabled(int hc) { highcut_enabled = hc; }
    void setSmooth(double sc) { smooth_amount = sc; }
    void setDynamics(double cc) { dynamics_amount = cc; }
    void setTilt(double tc) { tilt_amount = tc; }
    void setIrLength(size_t length) { irLength = length; }
    void setFtype(int i, int ft) { bands[i].type = (Band::Type)ft; }
    void setFreq(int i, double f) { bands[i].freq = f; }
    void setFq(int i, double q) { bands[i].Q = q; }
    void setFgain(int i, double g) { bands[i].gain = g; }
    void setFenable(int i, int e) { bands[i].enabled = e; }
    void setMuteBand(int i, int e) { bands[i].mute = e; }

    void setSoloBand(int i, int e) {
        solo_band = i;
        solo_enabled = e;
    }

    void setHFfade(int f) { hf_fade = f; }

private:
    FFTProcessor fp;
    IREqualiser eq;
    IRDesigner designer;
    Vec mag_ir_L_;
    Vec mag_ir_R_;

    std::vector<float> gui_ir_;
    std::vector<float> gui_phase_;
    Vec gui_ref_;
    Vec gui_diff_;
    Vec gui_src_;

    size_t analysisN = 4096;
    size_t synthesisN = 4096;
    static constexpr double EPS = 1e-12;
    size_t irLength = 4096;
    bool haveSource = false;
    bool haveFreshSource = false;
    bool haveReference = false;
    double peak = 0.0;
    double sampleRate = 48000.0;

    IRData bufferA;
    IRData bufferB;
    std::atomic<IRData*> front { nullptr };
    IRData* back = nullptr;

    std::thread workerThread;
    std::atomic<bool> running { true };
    std::atomic<bool> hasWork { false };
    std::mutex workMutex;

    Vec pendingReferenceL;
    Vec pendingReferenceR;

    Vec pendingSourceL;
    Vec pendingSourceR;

    bool pendingRebuild = false;
    std::condition_variable cv;
    std::mutex cvMutex;

    void applyHighFrequencyFade(Vec& mag, double sr, double startFreq = 20000.0) {
        size_t n = mag.size();
        double nyquist = sr * 0.5;

        for (size_t i = 0; i < n; ++i) {
            double freq = (double)i / (n - 1) * nyquist;
            if (freq <= startFreq) continue;
            double t = (freq - startFreq) / (nyquist - startFreq);
            t = std::clamp(t, 0.0, 1.0);
            double fade = 0.5 * (1.0 + cos(M_PI * t));
            mag[i] += 20.0 * log10(std::max(fade,1e-9));
        }
    }

    static Vec mergeAverage(const Vec& L, const Vec& R) {
        size_t n = std::min(L.size(), R.size());
        Vec out(n);

        for (size_t i = 0; i < n; ++i)
            out[i] = 0.5 * (L[i] + R[i]);

        return out;
    }

    static std::vector<float> mergeAverageFloat(const Vec& L, const Vec& R) {
        size_t n = std::min(L.size(), R.size());
        std::vector<float> out(n);

        for (size_t i = 0; i < n; ++i)
            out[i] = (float) (0.5f * (L[i] + R[i]));

        return out;
    }

    void updateGuiCurves(const IRData& data) {
        gui_ir_ = mergeAverageFloat(mag_ir_L_, mag_ir_R_);
        gui_ref_ = mergeAverage(data.left.ref, data.right.ref);
        gui_diff_ = mergeAverage(data.left.diff, data.right.diff);
        gui_src_ = mergeAverage(data.left.src, data.right.src);
    }

    static Vec remap_mag_bins(const Vec& in, size_t analysisN, size_t synthesisN) {
        size_t outBins = synthesisN / 2 + 1;
        size_t inBins  = in.size();
        Vec out(outBins);
        if (!in.size()) return out;

        for (size_t i = 0; i < outBins; ++i) {
            double freqNorm = (double)i / (double)(outBins - 1);
            double srcPos = freqNorm * (double)(inBins - 1);
            size_t idx0 = (size_t)srcPos;
            size_t idx1 = std::min(idx0 + 1, inBins - 1);
            double frac = srcPos - (double)idx0;
            out[i] = in[idx0] * (1.0 - frac) + in[idx1] * frac;
        }

        return out;
    }

    static Vec center_crop(const Vec& in, size_t size) {
        if (in.size() <= size)
            return in;

        size_t start = (in.size() - size) / 2;
        return Vec(in.begin() + start, in.begin() + start + size);
    }

    void make_flat(Vec& v, size_t bins, double db = 0.0) {
        v.clear();
        v.assign(bins, db);
    }

    void aplayFilter(Vec& mag_ir) {
        if (!mag_ir.size()) return;

        double lowcut_ = lowcut;
        double highcut_ = highcut;
        double smooth_amount_ = smooth_amount;
        double dynamics_amount_ = dynamics_amount;
        double tilt_amount_ = tilt_amount;
        int lowcut_enabled_ = lowcut_enabled;
        int highcut_enabled_ = highcut_enabled;
        int solo_band_ = solo_band;
        int solo_enabled_ = solo_enabled;

        if(lowcut_enabled_) {
            eq.apply_low_rolloff(mag_ir, sampleRate, lowcut_);
        }// else if (haveSource || haveReference) {
         //   eq.apply_low_rolloff(mag_ir, sampleRate, 30.0);
        //}
        if(highcut_enabled_) eq.apply_high_rolloff(mag_ir, sampleRate, highcut_);

        Band localBands[12];
        std::copy(std::begin(bands), std::end(bands), std::begin(localBands));

        for (auto& b : localBands) {
            if (b.enabled) {
                switch (b.type) {
                    case Band::Peak:
                        eq.apply_peak(mag_ir, sampleRate, b.freq, b.gain, b.Q);
                        break;

                    case Band::LowShelf:
                        eq.apply_low_shelf(mag_ir, sampleRate, b.freq, b.gain, b.Q);
                        break;

                    case Band::HighShelf:
                        eq.apply_high_shelf(mag_ir, sampleRate, b.freq, b.gain, b.Q);
                        break;
                }
            }
        }
        
        //apply_peak(mag_ir, sampleRate, 1000.0, -24.0, 1.0); // Q 0.0 - 5  
        Vec smooth = designer.adaptive_log_smooth(mag_ir, sampleRate);
        mag_ir = designer.lerpv(mag_ir, smooth, smooth_amount_);
        mag_ir = designer.spectral_dynamics(mag_ir, smooth, dynamics_amount_, tilt_amount_, sampleRate);
        //mag_ir = designer.adaptive_log_smooth(mag_ir, sampleRate * 0.001);
        //mag_ir = designer.harmonic_refine(mag_ir, sampleRate);
        //mag_ir = designer.soften_peaks(mag_ir, 0.2);

        if (!haveSource && ! haveReference) {
           // peak = std::max(peak, *std::max_element(mag_ir.begin(), mag_ir.end()));
           // for (auto& v : mag_ir) v -= peak;
        }

        if (solo_enabled_) {
            if (localBands[solo_band_].enabled) {
                mag_ir = eq.buildBandSoloIR(localBands[solo_band_], mag_ir, sampleRate, haveSource);
                mag_ir = designer.harmonic_refine(mag_ir, sampleRate);
            }
        } else {
            for (auto& b : localBands) {
                if (b.enabled) {
                    if (b.mute ) {
                        mag_ir = eq.buildBandMuteIR(b, mag_ir, sampleRate);
                        mag_ir = designer.harmonic_refine(mag_ir, sampleRate);
                    }
                }
            }
        }
        designer.smooth_low_end_hermite(mag_ir, sampleRate);
    }

    void processChannel(const Vec& reference, const Vec& source, IRChannelData& out, Vec& mag_ir, bool rebuild) {

        if (rebuild) {
            CVec a(analysisN), b(analysisN);

            for (size_t i = 0; i < reference.size(); ++i)
                a[i] = reference[i];

            for (size_t i = 0; i < source.size(); ++i)
                b[i] = source[i];

            CVec f1 = fp.fft(a);
            CVec f2 = fp.fft(b);

            out.peak = 0.0;

            if (haveSource && haveReference) {
                CVec H = fp.safe_divide(f1, f2);
                out.diff = magnitude_db(H);
            } else if (haveReference) {
                //out.diff = magnitude_db(f1);
                make_flat(out.diff, analysisN / 2 + 1);
            } else if (haveSource) {
                out.diff = magnitude_db(f2);
            } else {
                make_flat(out.ref, analysisN / 2 + 1);
                make_flat(out.diff, analysisN / 2 + 1);
                make_flat(out.src, analysisN / 2 + 1);
            }

            if (haveSource || haveReference) {
                out.diff = designer.adaptive_log_smooth(out.diff, sampleRate);
                out.diff = designer.soften_peaks(out.diff, 0.2);
                out.diff = designer.harmonic_refine(out.diff, sampleRate);

                out.ref = magnitude_db(f1);
                out.ref = designer.adaptive_log_smooth(out.ref, sampleRate);

                out.src = magnitude_db(f2);
                out.src = designer.adaptive_log_smooth(out.src, sampleRate);

                out.peak = std::max(
                    *std::max_element(out.ref.begin(), out.ref.end()),
                    *std::max_element(out.src.begin(), out.src.end())
                );

                for (auto& v : out.ref)  v -= out.peak;
                for (auto& v : out.src)  v -= out.peak;
                if (haveSource && haveReference) {
                    double peak_d = *std::max_element(out.diff.begin(), out.diff.end());
                    double peak_m = *std::min_element(out.diff.begin(), out.diff.end());
                    std::cout << peak_d << "  " << peak_m << std::endl;
                    //peak_d = 0.0 - peak_d;
                    //std::cout << peak_d << std::endl;
                    //for (auto& v : out.diff) v -= peak_d;
                } else if (haveSource)
                    for (auto& v : out.diff) v -= out.peak;
            }
        }

        mag_ir  = remap_mag_bins(out.diff, analysisN, synthesisN);
        peak = out.peak;

        aplayFilter(mag_ir);

    }

    void processIR(const Vec& refL, const Vec& refR, const Vec& srcL,
                            const Vec& srcR, bool rebuild, IRData& out) {
        workerBusy = true;
        workerReady = false;

        if (!rebuild) {
            if (IRData* current = front.load(std::memory_order_acquire)) {
                out = *current;
            }
        }

        processChannel(refL, srcL, out.left, mag_ir_L_, rebuild);
        processChannel(refR, srcR, out.right, mag_ir_R_, rebuild);

        //aplayFilter(mag_ir_L_);
        //aplayFilter(mag_ir_R_);

        updateGuiCurves(out);

        workerReady = true;
        workerBusy = false;
    }

    void stopWorker() {
        running.store(false);
        cv.notify_one();

        if (workerThread.joinable())
            workerThread.join();
    }

    void startWorker() {

        workerThread = std::thread([this]() {
            while (running.load(std::memory_order_acquire)) {
                {
                    std::unique_lock<std::mutex> lock(cvMutex);
                    cv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                            return hasWork.load(std::memory_order_acquire) ||
                                  !running.load(std::memory_order_acquire);
                    });
                }
                if (!running.load()) break;
                if (!hasWork.exchange(false)) continue;

                Vec refL;
                Vec refR;
                Vec srcL;
                Vec srcR;
                bool rebuild;
                {
                    std::lock_guard<std::mutex> lock(workMutex);
                    refL = pendingReferenceL;
                    refR = pendingReferenceR;
                    srcL = pendingSourceL;
                    srcR = pendingSourceR;
                    rebuild = pendingRebuild;
                }
                processIR(refL, refR, srcL, srcR, rebuild, *back);

                IRData* oldFront = front.exchange(back, std::memory_order_acq_rel);
                back = oldFront ? oldFront : (back == &bufferA ? &bufferB : &bufferA);
            }
        });
    }

    void updateIR(const Vec& refL, const Vec& refR, const Vec& srcL, const Vec& srcR, bool rebuild) {
        {
            std::lock_guard<std::mutex> lock(workMutex);

            pendingReferenceL = refL;
            pendingReferenceR = refR;

            pendingSourceL = srcL;
            pendingSourceR = srcR;

            pendingRebuild = rebuild;
        }

        hasWork.store(true, std::memory_order_release);
        cv.notify_one();
    }

    void updateIR() {
        {
            std::lock_guard<std::mutex> lock(workMutex);
            pendingRebuild = false;
        }

        hasWork.store(true, std::memory_order_release);
        cv.notify_one();
    }

    static size_t next_pow2(size_t x) {
        size_t n = 1;
        while (n < x) n <<= 1;
        return n;
    }

    static double db(double x) {
        return 20.0 * std::log10(std::max<double>(x, EPS));
    }

    static double db2lin(double x) {
        return std::pow(10.0, x / 20.0);
    }

    static Vec magnitude_db(const CVec& f) {
        size_t n = f.size() / 2 + 1;
        Vec out(n);

        for (size_t i = 0; i < n; ++i)
            out[i] = db(std::abs(f[i]));

        return out;
    }

    static CVec spectrum2fft(const Vec& mag) {
        size_t n = 2 * (mag.size() - 1);
        CVec out(n);

        for (size_t i = 0; i < mag.size(); ++i)
            out[i] = std::polar(db2lin(mag[i]), 0.0);

        for (size_t i = 1; i < mag.size() - 1; ++i)
            out[n - i] = std::conj(out[i]);

        return out;
    }

    static void apply_window(CVec& ir, size_t tail) {
        size_t n = ir.size();

        for (size_t i = 0; i < tail; ++i) {
            double w = 0.54 - 0.46 * std::cos(M_PI * i / tail);
            ir[n - tail + i] *= w;
        }
    }

    static void normalize(Vec& b) {
        constexpr double loudnessCompensation = 0.75;
        double energy = 0.0;
        double peak = 0.0;
        // get normalization peak
        for (size_t i = 0; i < b.size(); i++) {
            peak = std::max<double>(peak, std::abs( b[i])) ;
        }
        // apply normalize factor and get energy factor
        if (peak != 0.0) {
            peak = 1.0/peak;
            for (size_t i = 0; i < b.size(); i++) {
               b[i] *= peak;
               double v = b[i] ;
               energy += v*v;
            }
        }
        // apply gain factor when needed
        if (energy != 0.0) {
            double gain = 1.0 / std::pow(energy, loudnessCompensation);
            for (size_t i = 0; i < b.size(); i++) {
                b[i] *= gain;
            }
        }
    }

};

