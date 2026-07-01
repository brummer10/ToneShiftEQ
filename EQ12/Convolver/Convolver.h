/*
 * Convolver.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Stereo partitioned convolver with two modes:
 *
 *   MasterConvolver  — 128 samples latency, perfect phase, host-compensated.
 *                      Use for recording, mixing, mastering.
 *
 *   LiveConvolver    — Zero latency, phase twisted. Acceptable for live usage
 *
 * Both derive from ConvolverBase for transparent mode switching.
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <array>
#include <atomic>
#include <cstring>
#include <fftw3.h>

// ============================================================================
// Abstract base class
// ============================================================================

class ConvolverBase {
public:
    virtual ~ConvolverBase() = default;

    virtual void setBypass(int bp) = 0;
    virtual void setIR(const double* irL, const double* irR) = 0;
    virtual void process(size_t n, const float* inL, const float* inR, 
                                            float* outL, float* outR) = 0;
    virtual size_t getLatency() const = 0;
};

// ============================================================================
// MasterConvolver — 128 samples latency, perfect phase
// ============================================================================

class MasterConvolver : public ConvolverBase {
public:
    MasterConvolver() {
        init(chL);
        init(chR);
    }

    ~MasterConvolver() override {
        destroy(chL);
        destroy(chR);
        delete activeIR.load();
        delete pendingIR.load();
        delete trash;
    }

    void setBypass(int bp) override { bypass = bp; }

    void setIR(const double* irL, const double* irR) override {
        delete trash;
        trash = nullptr;

        IRData* ir = new IRData();
        build(ir->H_L, irL);
        build(ir->H_R, irR);

        IRData* old = pendingIR.exchange(ir);
        delete old;
    }

    void process(size_t n, const float* inL, const float* inR,
                           float* outL, float* outR) override {
        swapIR();

        IRData* ir = activeIR.load();
        if (!ir) {
            if (outL != inL) std::memcpy(outL, inL, n * sizeof(float));
            if (outR != inR) std::memcpy(outR, inR, n * sizeof(float));
            return;
        }

        processChannel(chL, ir->H_L, n, inL, outL);
        processChannel(chR, ir->H_R, n, inR, outR);
    }

    size_t getLatency() const override { return PART_SIZE; }

private:

    static constexpr size_t IR_LENGTH = 4096;
    static constexpr size_t PART_SIZE = 128;
    static constexpr size_t FFT_SIZE  = PART_SIZE * 2;
    static constexpr size_t NUM_BINS  = FFT_SIZE / 2 + 1;

    static constexpr size_t NUM_PARTS        = IR_LENGTH / PART_SIZE;  // 32
    static constexpr size_t OUTPUT_FIFO_SIZE = PART_SIZE * 8;

    int bypass = 0;

    struct Complex { double re = 0.0, im = 0.0; };
    using Spectrum = std::array<Complex, NUM_BINS>;

    using Part = std::array<Spectrum, NUM_PARTS>;

    struct IRData { Part H_L, H_R; };

    IRData* trash = nullptr;

    struct Channel {
        std::array<float,  PART_SIZE> dryDelay{};
        size_t dryIdx = 0;
        std::array<float,  PART_SIZE> inFifo{};
        size_t inFill = 0;
        std::array<float,  OUTPUT_FIFO_SIZE> outFifo{};
        size_t outRead = 0, outWrite = 0, available = 0;
        std::array<Spectrum, NUM_PARTS> Xhistory{};
        size_t historyPos = 0;
        std::array<double, PART_SIZE> overlap{};

        double* fftIn           = nullptr;
        double* fftOut          = nullptr;
        fftw_complex* fftFreq   = nullptr;
        fftw_complex* fftAccum  = nullptr;
        fftw_plan planFwd       = nullptr;
        fftw_plan planInv       = nullptr;
    };

    Channel chL, chR;
    std::atomic<IRData*> activeIR {nullptr};
    std::atomic<IRData*> pendingIR{nullptr};

    void init(Channel& ch) {
        ch.fftIn    = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);
        ch.fftOut   = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);
        ch.fftFreq  = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * NUM_BINS);
        ch.fftAccum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * NUM_BINS);
        ch.planFwd  = fftw_plan_dft_r2c_1d(FFT_SIZE, ch.fftIn, ch.fftFreq, FFTW_MEASURE);
        ch.planInv  = fftw_plan_dft_c2r_1d(FFT_SIZE, ch.fftAccum, ch.fftOut, FFTW_MEASURE);
        ch.dryDelay.fill(0.0f);
        ch.dryIdx = 0;
        ch.inFifo.fill(0.0f);
        ch.inFill = 0;
        ch.outFifo.fill(0.0f);
        ch.outRead = ch.outWrite = ch.available = 0;
        ch.overlap.fill(0.0);
        ch.historyPos = 0;
        for (auto& s : ch.Xhistory) 
            for (auto& b : s)
                b = {0.0, 0.0};
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
        double time[FFT_SIZE]{};
        fftw_complex freq[NUM_BINS]{};
        fftw_plan tmp = fftw_plan_dft_r2c_1d(FFT_SIZE, time, freq, FFTW_ESTIMATE);
        for (size_t p = 0; p < NUM_PARTS; ++p) {
            std::memset(time, 0, sizeof(time));
            for (size_t i = 0; i < PART_SIZE; ++i) {
                size_t idx = p * PART_SIZE + i;
                if (idx < IR_LENGTH) time[i] = ir[idx];
            }
            fftw_execute(tmp);
            for (size_t k = 0; k < NUM_BINS; ++k) {
                H[p][k].re = freq[k][0];
                H[p][k].im = freq[k][1];
            }
        }
        fftw_destroy_plan(tmp);
    }

    void swapIR() {
        IRData* p = pendingIR.exchange(nullptr);
        if (!p) return;
        trash = activeIR.exchange(p);
    }

    inline float processDry(Channel& ch, float in) {
        float out = ch.dryDelay[ch.dryIdx];
        ch.dryDelay[ch.dryIdx] = in;
        if (++ch.dryIdx >= PART_SIZE) ch.dryIdx = 0;
        return out;
    }

    void processChannel(Channel& ch, const Part& H, size_t n, 
                                const float* in, float* out) {
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

    inline float popOutput(Channel& ch) {
        if (ch.available == 0) return 0.0f;
        float v = ch.outFifo[ch.outRead];
        if (++ch.outRead >= OUTPUT_FIFO_SIZE) ch.outRead = 0;
        ch.available--;
        return v;
    }

    inline void pushOutput(Channel& ch, float v) {
        if (ch.available >= OUTPUT_FIFO_SIZE) return;
        ch.outFifo[ch.outWrite] = v;
        if (++ch.outWrite >= OUTPUT_FIFO_SIZE) ch.outWrite = 0;
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
                ch.fftAccum[k][0] += X[k].re * Hp[k].re - X[k].im * Hp[k].im;
                ch.fftAccum[k][1] += X[k].re * Hp[k].im + X[k].im * Hp[k].re;
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

// ============================================================================
// LiveConvolver — zero latency, phase twisted
// ============================================================================

class LiveConvolver : public ConvolverBase {
public:
    LiveConvolver() {
        init(chL);
        init(chR);
    }

    ~LiveConvolver() override {
        destroy(chL);
        destroy(chR);
        delete activeIR.load();
        delete pendingIR.load();
        delete trash;
    }

    void setBypass(int bp) override { bypass = bp; }

    void setIR(const double* irL, const double* irR) override {
        delete trash;
        trash = nullptr;

        IRData* ir = new IRData();
        buildHead(ir->headL, irL);
        buildHead(ir->headR, irR);
        buildTail(ir->H_L, irL);
        buildTail(ir->H_R, irR);

        IRData* old = pendingIR.exchange(ir);
        delete old;
    }

    void process(size_t n, const float* inL, const float* inR,
                            float* outL, float* outR) override {
        swapIR();

        IRData* ir = activeIR.load();
        if (!ir) {
            if (outL != inL) std::memcpy(outL, inL, n * sizeof(float));
            if (outR != inR) std::memcpy(outR, inR, n * sizeof(float));
            return;
        }

        processChannel(chL, ir->headL, ir->H_L, n, inL, outL);
        processChannel(chR, ir->headR, ir->H_R, n, inR, outR);
    }

    size_t getLatency() const override { return 0; }

private:

    static constexpr size_t IR_LENGTH = 4096;
    static constexpr size_t PART_SIZE = 128;
    static constexpr size_t FFT_SIZE  = PART_SIZE * 2;
    static constexpr size_t NUM_BINS  = FFT_SIZE / 2 + 1;

    static constexpr size_t NUM_PARTS_TOTAL  = IR_LENGTH / PART_SIZE; // 32
    static constexpr size_t NUM_TAIL         = NUM_PARTS_TOTAL - 1;   // 31
    static constexpr size_t OUTPUT_FIFO_SIZE = PART_SIZE * 8;

    int bypass = 0;

    struct Complex { double re = 0.0, im = 0.0; };
    using Spectrum = std::array<Complex, NUM_BINS>;

    using Head = std::array<double,   PART_SIZE>;
    using Tail = std::array<Spectrum, NUM_TAIL>;

    struct IRData {
        Head headL{}, headR{};
        Tail H_L{},   H_R{};
    };

    IRData* trash = nullptr;

    struct Channel {
        std::array<double, PART_SIZE> prevBlock{};
        std::array<double, PART_SIZE> prevPrevBlock{};
        std::array<float,  PART_SIZE> inFifo{};
        size_t inFill = 0;
        std::array<float,  OUTPUT_FIFO_SIZE> outFifo{};
        size_t outRead = 0, outWrite = 0, available = 0;
        std::array<Spectrum, NUM_TAIL> Xhistory{};
        size_t historyPos = 0;
        std::array<double, PART_SIZE> overlap{};
        double* fftIn           = nullptr;
        double* fftOut          = nullptr;
        fftw_complex* fftFreq   = nullptr;
        fftw_complex* fftAccum  = nullptr;
        fftw_plan planFwd       = nullptr;
        fftw_plan planInv       = nullptr;
    };

    Channel chL, chR;
    std::atomic<IRData*> activeIR {nullptr};
    std::atomic<IRData*> pendingIR{nullptr};

    void init(Channel& ch) {
        ch.fftIn    = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);
        ch.fftOut   = (double*)fftw_malloc(sizeof(double) * FFT_SIZE);
        ch.fftFreq  = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * NUM_BINS);
        ch.fftAccum = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * NUM_BINS);
        ch.planFwd  = fftw_plan_dft_r2c_1d(FFT_SIZE, ch.fftIn, ch.fftFreq, FFTW_MEASURE);
        ch.planInv  = fftw_plan_dft_c2r_1d(FFT_SIZE, ch.fftAccum, ch.fftOut, FFTW_MEASURE);
        ch.prevBlock.fill(0.0);
        ch.prevPrevBlock.fill(0.0);
        ch.inFifo.fill(0.0f);
        ch.inFill = 0;
        ch.outFifo.fill(0.0f);
        ch.outRead = ch.outWrite = ch.available = 0;
        ch.overlap.fill(0.0);
        ch.historyPos = 0;
        for (auto& s : ch.Xhistory)
            for (auto& b : s)
                b = {0.0, 0.0};
    }

    void destroy(Channel& ch) {
        if (ch.planFwd) fftw_destroy_plan(ch.planFwd);
        if (ch.planInv) fftw_destroy_plan(ch.planInv);
        fftw_free(ch.fftIn);
        fftw_free(ch.fftOut);
        fftw_free(ch.fftFreq);
        fftw_free(ch.fftAccum);
    }

    void buildHead(Head& head, const double* ir) {
        for (size_t i = 0; i < PART_SIZE; ++i)
            head[i] = ir[i];
    }

    void buildTail(Tail& H, const double* ir) {
        double time[FFT_SIZE]{};
        fftw_complex freq[NUM_BINS]{};
        fftw_plan tmp = fftw_plan_dft_r2c_1d(FFT_SIZE, time, freq, FFTW_ESTIMATE);
        for (size_t p = 0; p < NUM_TAIL; ++p) {
            std::memset(time, 0, sizeof(time));
            for (size_t i = 0; i < PART_SIZE; ++i) {
                size_t idx = (p + 1) * PART_SIZE + i;
                if (idx < IR_LENGTH) time[i] = ir[idx];
            }
            fftw_execute(tmp);
            for (size_t k = 0; k < NUM_BINS; ++k) {
                H[p][k].re = freq[k][0];
                H[p][k].im = freq[k][1];
            }
        }
        fftw_destroy_plan(tmp);
    }

    void swapIR() {
        IRData* p = pendingIR.exchange(nullptr);
        if (!p) return;
        trash = activeIR.exchange(p);
    }

    void processChannel(Channel& ch, const Head& head, const Tail& tail,
                                size_t n, const float* in, float* out) {
        for (size_t i = 0; i < n; ++i) {
            ch.inFifo[ch.inFill++] = in[i];
            if (ch.inFill == PART_SIZE) {
                runBlock(ch, head, tail);
                ch.prevPrevBlock = ch.prevBlock;
                for (size_t j = 0; j < PART_SIZE; ++j)
                    ch.prevBlock[j] = ch.inFifo[j];
                ch.inFill = 0;
            }
            out[i] = bypass ? in[i] : popOutput(ch);
        }
    }

    inline float popOutput(Channel& ch) {
        if (ch.available == 0) return 0.0f;
        float v = ch.outFifo[ch.outRead];
        if (++ch.outRead >= OUTPUT_FIFO_SIZE) ch.outRead = 0;
        ch.available--;
        return v;
    }

    inline void pushOutput(Channel& ch, float v) {
        if (ch.available >= OUTPUT_FIFO_SIZE) return;
        ch.outFifo[ch.outWrite] = v;
        if (++ch.outWrite >= OUTPUT_FIFO_SIZE) ch.outWrite = 0;
        ch.available++;
    }

    void runBlock(Channel& ch, const Head& head, const Tail& tail) {
        // Head: direct FIR over IR[0..PART_SIZE-1]
        // k <= i → prevBlock[i-k]                      (block n-1)
        // k >  i → prevPrevBlock[PART_SIZE - (k - i)]  (block n-2)
        double blockOut[PART_SIZE]{};
        for (size_t i = 0; i < PART_SIZE; ++i) {
            double acc = 0.0;
            for (size_t k = 0; k < PART_SIZE; ++k) {
                double x = (k <= i) ? ch.prevBlock[i - k]
                    : ch.prevPrevBlock[PART_SIZE - (k - i)];
                acc += x * head[k];
            }
            blockOut[i] = acc;
        }

        // Tail: OLA partitioned FFT over IR[PART_SIZE..IR_LENGTH-1]
        std::memset(ch.fftIn, 0, sizeof(double) * FFT_SIZE);
        for (size_t i = 0; i < PART_SIZE; ++i)
            ch.fftIn[i] = ch.prevBlock[i];
        fftw_execute(ch.planFwd);

        ch.historyPos = (ch.historyPos == 0) ? NUM_TAIL - 1 : ch.historyPos - 1;
        Spectrum& Xnew = ch.Xhistory[ch.historyPos];
        for (size_t k = 0; k < NUM_BINS; ++k) {
            Xnew[k].re = ch.fftFreq[k][0];
            Xnew[k].im = ch.fftFreq[k][1];
        }

        std::memset(ch.fftAccum, 0, sizeof(fftw_complex) * NUM_BINS);
        for (size_t p = 0; p < NUM_TAIL; ++p) {
            const Spectrum& X  = ch.Xhistory[(ch.historyPos + p + 1) % NUM_TAIL];
            const Spectrum& Hp = tail[p];
            for (size_t k = 0; k < NUM_BINS; ++k) {
                ch.fftAccum[k][0] += X[k].re * Hp[k].re - X[k].im * Hp[k].im;
                ch.fftAccum[k][1] += X[k].re * Hp[k].im + X[k].im * Hp[k].re;
            }
        }
        fftw_execute(ch.planInv);

        constexpr double scale = 1.0 / FFT_SIZE;
        for (size_t i = 0; i < PART_SIZE; ++i) {
            double v = blockOut[i] + ch.fftOut[i] * scale + ch.overlap[i];
            ch.overlap[i] = ch.fftOut[i + PART_SIZE] * scale;
            pushOutput(ch, (float)v);
        }
    }
};
