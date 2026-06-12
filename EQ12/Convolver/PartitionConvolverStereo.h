/*
 * PartitionConvolverStereo.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <array>
#include <atomic>
#include <cstring>
#include <fftw3.h>

class PartitionConvolverStereo {
public:
    PartitionConvolverStereo() {
        init(chL);
        init(chR);
    }

    ~PartitionConvolverStereo() {
        destroy(chL);
        destroy(chR);

        delete activeIR.load();
        delete pendingIR.load();
    }

    void setBypass(int bp) {
        bypass = bp;
    }

    void setIR(const double* irL, const double* irR) {
        IRData* ir = new IRData();

        build(ir->H_L, irL);
        build(ir->H_R, irR);

        IRData* old = pendingIR.exchange(ir);
        delete old;
    }

    void process(size_t n, const float* inL, const float* inR,
                                    float* outL, float* outR) {
        swapIR();

        IRData* ir = activeIR.load();
        if (!ir) {
            if (outL != inL)
                std::memcpy(outL, inL, n * sizeof(float));
            if (outR != inR)
                std::memcpy(outR, inR, n * sizeof(float));
            return;
        }

        processChannel(chL, ir->H_L, n, inL, outL);
        processChannel(chR, ir->H_R, n, inR, outR);
    }

private:
    static constexpr size_t IR_LENGTH        = 4096;
    static constexpr size_t PART_SIZE        = 128;
    static constexpr size_t FFT_SIZE         = 256;
    static constexpr size_t NUM_PARTS        = IR_LENGTH / PART_SIZE;
    static constexpr size_t NUM_BINS         = FFT_SIZE / 2 + 1;

    static constexpr size_t OUTPUT_FIFO_SIZE = PART_SIZE * 8;

    int bypass = 0;

    struct Complex {
        double re = 0.0;
        double im = 0.0;
    };

    using Spectrum = std::array<Complex, NUM_BINS>;
    using Part     = std::array<Spectrum, NUM_PARTS>;

    struct IRData {
        Part H_L;
        Part H_R;
    };

    struct Channel {
        // BYPASS DELAY
        std::array<float, PART_SIZE + FFT_SIZE> dryDelay{};
        size_t dryIdx = 0;
        // INPUT FIFO
        std::array<float, PART_SIZE> inFifo{};
        size_t inFill = 0;

        // OUTPUT FIFO
        std::array<float, OUTPUT_FIFO_SIZE> outFifo{};
        size_t outRead  = 0;
        size_t outWrite = 0;
        size_t available = 0;

        // CONV STATE
        std::array<Spectrum, NUM_PARTS> Xhistory{};
        size_t historyPos = 0;

        std::array<double, PART_SIZE> overlap{};

        // FFTW
        double* fftIn  = nullptr;
        double* fftOut = nullptr;

        fftw_complex* fftFreq  = nullptr;
        fftw_complex* fftAccum = nullptr;

        fftw_plan planFwd = nullptr;
        fftw_plan planInv = nullptr;
    };

    Channel chL;
    Channel chR;

    std::atomic<IRData*> activeIR{nullptr};
    std::atomic<IRData*> pendingIR{nullptr};

    void init(Channel& ch) {
        ch.fftIn  = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);
        ch.fftOut = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);

        ch.fftFreq  = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * NUM_BINS);
        ch.fftAccum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * NUM_BINS);

        ch.planFwd = fftw_plan_dft_r2c_1d(FFT_SIZE, ch.fftIn, ch.fftFreq, FFTW_MEASURE);
        ch.planInv = fftw_plan_dft_c2r_1d(FFT_SIZE, ch.fftAccum, ch.fftOut, FFTW_MEASURE);

        ch.outFifo.fill(0.0f);
        ch.inFifo.fill(0.0f);

        ch.dryDelay.fill(0.0f);
        ch.dryIdx = 0;
        ch.available = 0;
        ch.outRead = 0;
        ch.outWrite = 0;
    }

    void destroy(Channel& ch) {
        if (ch.planFwd) fftw_destroy_plan(ch.planFwd);
        if (ch.planInv) fftw_destroy_plan(ch.planInv);

        fftw_free(ch.fftIn);
        fftw_free(ch.fftOut);
        fftw_free(ch.fftFreq);
        fftw_free(ch.fftAccum);
    }

    void build(Part& H, const double* ir) {
        for (size_t p = 0; p < NUM_PARTS; ++p) {
            double time[FFT_SIZE]{};
            fftw_complex freq[NUM_BINS]{};

            for (size_t i = 0; i < PART_SIZE; ++i) {
                size_t idx = p * PART_SIZE + i;
                if (idx < IR_LENGTH)
                    time[i] = ir[idx];
            }

            fftw_plan tmp = fftw_plan_dft_r2c_1d(FFT_SIZE, time, freq, FFTW_ESTIMATE);

            fftw_execute(tmp);
            fftw_destroy_plan(tmp);

            for (size_t k = 0; k < NUM_BINS; ++k) {
                H[p][k].re = freq[k][0];
                H[p][k].im = freq[k][1];
            }
        }
    }

    void swapIR() {
        IRData* p = pendingIR.exchange(nullptr);
        if (!p) return;

        IRData* old = activeIR.exchange(p);
        delete old;
    }

    inline float processDry(Channel& ch, float in) {
        float out = ch.dryDelay[ch.dryIdx];
        ch.dryDelay[ch.dryIdx] = in;
        ch.dryIdx++;
        if (ch.dryIdx == PART_SIZE + FFT_SIZE) ch.dryIdx = 0;

        return out;
    }

    void processChannel(Channel& ch, const Part& H, size_t n, const float* in, float* out) {
        for (size_t i = 0; i < n; ++i) {
            float wet = popOutput(ch);
            float dry = processDry(ch, in[i]);
            out[i] = bypass ? dry : wet;
            ch.inFifo[ch.inFill++] = in[i];

            if (ch.inFill == PART_SIZE) {
                runBlock(ch, H);
                ch.inFill = 0;
            }
        }
    }

    float popOutput(Channel& ch) {
        if (ch.available == 0) return 0.0f;
        float v = ch.outFifo[ch.outRead];

        ch.outRead++;
        if (ch.outRead >= OUTPUT_FIFO_SIZE) ch.outRead = 0;

        ch.available--;
        return v;
    }

    void pushOutput(Channel& ch, float v) {
        if (ch.available >= OUTPUT_FIFO_SIZE) return;

        ch.outFifo[ch.outWrite] = v;
        ch.outWrite++;
        if (ch.outWrite >= OUTPUT_FIFO_SIZE) ch.outWrite = 0;

        ch.available++;
    }

    void runBlock(Channel& ch, const Part& H) {
        std::memset(ch.fftIn, 0, sizeof(double) * FFT_SIZE);

        for (size_t i = 0; i < PART_SIZE; ++i)
            ch.fftIn[i] = ch.inFifo[i];

        fftw_execute(ch.planFwd);

        ch.historyPos = (ch.historyPos + NUM_PARTS - 1) % NUM_PARTS;

        Spectrum& Xnew = ch.Xhistory[ch.historyPos];

        for (size_t k = 0; k < NUM_BINS; ++k) {
            Xnew[k].re = ch.fftFreq[k][0];
            Xnew[k].im = ch.fftFreq[k][1];
        }

        std::memset(ch.fftAccum, 0, sizeof(fftw_complex) * NUM_BINS);

        for (size_t p = 0; p < NUM_PARTS; ++p) {
            const Spectrum& X  = ch.Xhistory[(ch.historyPos + p) % NUM_PARTS];
            const Spectrum& Hp = H[p];

            for (size_t k = 0; k < NUM_BINS; ++k) {
                double xr = X[k].re;
                double xi = X[k].im;

                double hr = Hp[k].re;
                double hi = Hp[k].im;

                ch.fftAccum[k][0] += xr * hr - xi * hi;
                ch.fftAccum[k][1] += xr * hi + xi * hr;
            }
        }

        fftw_execute(ch.planInv);

        constexpr double scale = 1.0 / FFT_SIZE;

        for (size_t i = 0; i < PART_SIZE; ++i) {
            float v = (float)(ch.fftOut[i] * scale + ch.overlap[i]);
            ch.overlap[i] = ch.fftOut[i + PART_SIZE] * scale;

            pushOutput(ch, v);
        }
    }
};
