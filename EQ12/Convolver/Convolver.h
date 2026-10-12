/*
 * Convolver.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Stereo (dual-mono) convolver, ZERO latency, crossfading IR switch:
 *
 *   - taps 0..PART_SIZE-1  : direct FIR in the time domain (exact for any host block size)
 *   - taps PART_SIZE..4095 : uniform partitioned FFT convolution (overlap-save), computed
 *                            one internal block ahead, so it never needs the current block
 *   - IR switch            : the input spectra (FDL) are shared. Sample-accurate crossfade,
 *                            up to kMaxSlots IRs at once (retrigger during a fade is fine).
 *
 * Host block sizes may be anything (1 .. n, variable, loop cuts), the internal block size
 * PART_SIZE is independent of it. getLatency() is 0 -> do NOT report latency to the host.
 *
 * FFT layer: AudioFFT (split-complex, all state inside the object, no global
 * planner -> no locking needed between plugin instances).
 *
 * Threading:
 *   control thread (exactly one): setIR(), setFadeTime(), setEqualPower(), setBypass()
 *   audio thread:                 process(), reset() (only while process() is not running)
 * The audio thread never allocates, locks or frees. Retired IRs travel back through a small
 * SPSC queue and are deleted in setIR()/drainRetired()/destructor.
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "AudioFFT.h"

class Convolver {
public:
    Convolver() {
        buildFft.init(FFT_SIZE);
        initChannel(chL);
        initChannel(chR);
        for (auto& q : retireQ) q.store(nullptr, std::memory_order_relaxed);
        setFadeTime(48000.0, 30.0f);
        reset();
    }

    ~Convolver() {
        delete pendingIR.exchange(nullptr);
        for (auto& s : slots) delete s.ir;
        drainRetired();
    }

    void setBypass(int bp) { bypass.store(bp, std::memory_order_relaxed); }

    // Crossfade duration for IR changes. Call once the sample rate is known.
    void setFadeTime(double sampleRate, float ms = 30.0f) {
        const double samples = std::max(1.0, sampleRate * static_cast<double>(ms) * 0.001);
        fadeStep.store(static_cast<float>(1.0 / samples), std::memory_order_relaxed);
    }

    // false (default): linear fade, right for similar IRs (EQ variants)
    // true:            equal-power fade, right for uncorrelated IRs (different cabs)
    void setEqualPower(bool on) { equalPower.store(on, std::memory_order_relaxed); }

    // Must not be called concurrently from several threads (shares buildFft/buildIn).
    // Takes IR_LENGTH (4096) samples per channel. Takes effect at the next internal
    // block boundary (< PART_SIZE samples) and is faded in over the fade time.
    void setIR(const double* irL, const double* irR) {
        drainRetired();

        IRData* ir = new IRData();
        build(ir->L, irL);
        build(ir->R, irR);

        IRData* old = pendingIR.exchange(ir, std::memory_order_acq_rel);
        delete old;
    }

    // Free IRs that the audio thread has retired
    void drainRetired() {
        unsigned tail = rTail.load(std::memory_order_relaxed);
        const unsigned head = rHead.load(std::memory_order_acquire);
        while (tail != head) {
            delete retireQ[tail % kRetireCap].exchange(nullptr, std::memory_order_acquire);
            ++tail;
        }
        rTail.store(tail, std::memory_order_release);
    }

    void reset() {
        resetChannel(chL);
        resetChannel(chR);
        historyPos = 0;
        fill = 0;
        fresh = true;
    }

    // In-place is allowed. Without an IR the signal is passed through.
    void process(size_t n, const float* inL, const float* inR, float* outL, float* outR) {
        if (fresh) {
            prepareNextBlock();
            fresh = false;
        }
        const bool byp = bypass.load(std::memory_order_relaxed) != 0;

        size_t pos = 0;
        while (pos < n) {
            const size_t chunk = std::min(n - pos, PART_SIZE - fill);
            processChunk(chL, 0, inL + pos, outL + pos, chunk, byp);
            processChunk(chR, 1, inR + pos, outR + pos, chunk, byp);
            fill += chunk;
            pos  += chunk;
            if (fill == PART_SIZE) {
                pushInputSpectra();
                prepareNextBlock();
                fill = 0;
            }
        }
    }

    size_t getLatency() const { return 0; }

private:

    static constexpr size_t IR_LENGTH = 4096;
    static constexpr size_t PART_SIZE = 128;                       // internal block, power of two
    static constexpr size_t FFT_SIZE  = PART_SIZE * 2;
    static constexpr size_t NUM_BINS  = FFT_SIZE / 2 + 1;          // == AudioFFT::ComplexSize(FFT_SIZE)
    static constexpr size_t NUM_TAIL  = IR_LENGTH / PART_SIZE - 1; // 31 partitions (taps 128..4095)
    static constexpr int    kMaxSlots = 4;                         // IRs active at the same time
    static constexpr unsigned kRetireCap = 16;

    static_assert(IR_LENGTH > PART_SIZE && IR_LENGTH % PART_SIZE == 0, "IR_LENGTH must be a multiple of PART_SIZE");

    // Split-complex spectrum (bins 0..N/2, as AudioFFT delivers it)
    struct Spectrum {
        std::array<float, NUM_BINS> re{};
        std::array<float, NUM_BINS> im{};
    };

    struct IRChan {
        std::array<float, PART_SIZE>    head{};   // taps 0..PART_SIZE-1, stored reversed
        std::array<Spectrum, NUM_TAIL>  tail{};   // partitions of taps PART_SIZE..
    };
    struct IRData { IRChan L, R; };

    struct Slot {
        IRData* ir = nullptr;
        float   g = 0.0f;          // linear gain, sum over all slots == 1
        bool    dying = false;     // gain reached 0, released at the next block boundary
    };

    struct Channel {
        std::array<float, FFT_SIZE>  buf{};       // [previous block | current block]
        std::array<Spectrum, NUM_TAIL> Xhistory{};
        std::array<float, PART_SIZE> tailMix{};   // tail of the current block, already weighted
        std::array<float, PART_SIZE> tailTmp{};
        std::array<float, FFT_SIZE>  fftOut{};
        std::array<float, NUM_BINS>  accRe{};
        std::array<float, NUM_BINS>  accIm{};

        audiofft::AudioFFT fft;   // one object per channel: not usable concurrently
    };

    // Only used by setIR()/build(), i.e. never from the audio thread.
    audiofft::AudioFFT          buildFft;
    std::array<float, FFT_SIZE> buildIn{};

    Channel chL, chR;
    std::array<Slot, kMaxSlots> slots{};
    std::array<float, kMaxSlots * PART_SIZE> wts{};   // [slot][PART_SIZE] gains for the current block
    int    newest = -1;
    size_t historyPos = 0;
    size_t fill = 0;
    bool   fresh = true;

    std::atomic<int>      bypass{0};
    std::atomic<float>    fadeStep{1.0f / 1440.0f};
    std::atomic<bool>     equalPower{false};
    std::atomic<IRData*>  pendingIR{nullptr};
    std::atomic<IRData*>  retireQ[kRetireCap];
    std::atomic<unsigned> rHead{0}, rTail{0};

    void resetChannel(Channel& ch) {
        ch.buf.fill(0.0f);
        for (auto& s : ch.Xhistory) {
            s.re.fill(0.0f);
            s.im.fill(0.0f);
        }
        ch.tailMix.fill(0.0f);
        ch.tailTmp.fill(0.0f);
        ch.fftOut.fill(0.0f);
        ch.accRe.fill(0.0f);
        ch.accIm.fill(0.0f);
    }

    void initChannel(Channel& ch) {
        ch.fft.init(FFT_SIZE);
        resetChannel(ch);
    }

    void build(IRChan& H, const double* ir) {
        for (size_t i = 0; i < PART_SIZE; ++i)
            H.head[PART_SIZE - 1 - i] = static_cast<float>(ir[i]);

        for (size_t q = 0; q < NUM_TAIL; ++q) {
            buildIn.fill(0.0f);
            for (size_t i = 0; i < PART_SIZE; ++i) {
                const size_t idx = (q + 1) * PART_SIZE + i;
                if (idx < IR_LENGTH) buildIn[i] = static_cast<float>(ir[idx]);
            }
            // transform straight into the partition's spectrum
            buildFft.fft(buildIn.data(), H.tail[q].re.data(), H.tail[q].im.data());
        }
    }

    void retire(IRData* ir) {
        if (!ir) return;
        const unsigned head = rHead.load(std::memory_order_relaxed);
        const unsigned tail = rTail.load(std::memory_order_acquire);
        if (head - tail >= kRetireCap) return;   // cannot happen in normal use (leak instead of blocking)
        retireQ[head % kRetireCap].store(ir, std::memory_order_release);
        rHead.store(head + 1, std::memory_order_release);
    }

    // Direct-form head + tail of the current block, per slot weighted with the fade gains.
    void processChunk(Channel& ch, int c, const float* in, float* out,
                      size_t chunk, bool byp) {
        float* cur = ch.buf.data() + PART_SIZE + fill;
        std::memcpy(cur, in, chunk * sizeof(float));

        if (byp || newest < 0) {
            std::memcpy(out, cur, chunk * sizeof(float));
            return;
        }

        for (size_t t = 0; t < chunk; ++t) out[t] = ch.tailMix[fill + t];

        for (int s = 0; s < kMaxSlots; ++s) {
            if (!slots[s].ir) continue;
            const IRChan& ic = (c == 0) ? slots[s].ir->L : slots[s].ir->R;
            const float* h = ic.head.data();
            const float* w = &wts[static_cast<size_t>(s) * PART_SIZE + fill];
            for (size_t t = 0; t < chunk; ++t) {
                const float* x = cur + t - (PART_SIZE - 1);
                float acc = 0.0f;
                for (size_t i = 0; i < PART_SIZE; ++i) acc += h[i] * x[i];
                out[t] += w[t] * acc;
            }
        }
    }

    // After a complete block k-1: X_{k-1} = FFT([x_{k-2}, x_{k-1}]) into the history, then shift.
    void pushInputSpectra() {
        historyPos = (historyPos + NUM_TAIL - 1) % NUM_TAIL;
        pushChannel(chL);
        pushChannel(chR);
    }

    void pushChannel(Channel& ch) {
        Spectrum& Xnew = ch.Xhistory[historyPos];
        ch.fft.fft(ch.buf.data(), Xnew.re.data(), Xnew.im.data());
        std::memmove(ch.buf.data(), ch.buf.data() + PART_SIZE, PART_SIZE * sizeof(float));
    }

    // Tail of one IR for the NEXT block: sum_i H_i * X_{k-1-i}, last PART_SIZE samples of the IFFT.
    void computeTail(Channel& ch, const IRChan& H, float* out) {
        float* accRe = ch.accRe.data();
        float* accIm = ch.accIm.data();
        for (size_t k = 0; k < NUM_BINS; ++k) { accRe[k] = 0.0f; accIm[k] = 0.0f; }

        size_t idx = historyPos;
        for (size_t p = 0; p < NUM_TAIL; ++p) {
            const Spectrum& X  = ch.Xhistory[idx];
            const Spectrum& Hp = H.tail[p];
            const float* xr = X.re.data();
            const float* xi = X.im.data();
            const float* hr = Hp.re.data();
            const float* hi = Hp.im.data();
            for (size_t k = 0; k < NUM_BINS; ++k) {
                accRe[k] += xr[k] * hr[k] - xi[k] * hi[k];
                accIm[k] += xr[k] * hi[k] + xi[k] * hr[k];
            }
            if (++idx == NUM_TAIL) idx = 0;
        }

        // AudioFFT::ifft() already scales (no 1/N here)
        ch.fft.ifft(ch.fftOut.data(), accRe, accIm);
        std::memcpy(out, ch.fftOut.data() + PART_SIZE, PART_SIZE * sizeof(float));
    }

    // Block boundary: manage slots, compute fade gains for the next block, compute tails.
    void prepareNextBlock() {
        // release IRs that finished fading out
        for (auto& s : slots)
            if (s.dying) { retire(s.ir); s = Slot{}; }

        // new IR waiting?
        if (pendingIR.load(std::memory_order_relaxed) != nullptr) {
            IRData* nir = pendingIR.exchange(nullptr, std::memory_order_acquire);
            if (nir) {
                int idx = -1;
                for (int i = 0; i < kMaxSlots; ++i)
                    if (!slots[i].ir) { idx = i; break; }
                if (idx < 0) {
                    float best = 2.0f;
                    for (int i = 0; i < kMaxSlots; ++i)
                        if (i != newest && slots[i].g < best) { best = slots[i].g; idx = i; }
                    retire(slots[idx].ir);
                    slots[idx] = Slot{};
                }
                slots[idx].ir = nir;
                slots[idx].g = 0.0f;
                newest = idx;
            }
        }

        // per-sample gains: older IRs fall linearly to 0
        wts.fill(0.0f);
        if (newest >= 0) {
            const float step = fadeStep.load(std::memory_order_relaxed);
            const bool  eq   = equalPower.load(std::memory_order_relaxed);
            for (size_t t = 0; t < PART_SIZE; ++t) {
                float sum = 0.0f;
                for (int s = 0; s < kMaxSlots; ++s) {
                    if (!slots[s].ir || s == newest) continue;
                    float g = slots[s].g - step;
                    if (g < 0.0f) g = 0.0f;
                    slots[s].g = g;
                    sum += g;
                }
                float gn = 1.0f - sum;
                if (gn < 0.0f) gn = 0.0f;
                slots[newest].g = gn;
                for (int s = 0; s < kMaxSlots; ++s)
                    if (slots[s].ir)
                        wts[static_cast<size_t>(s) * PART_SIZE + t] = eq ? std::sqrt(slots[s].g) : slots[s].g;
            }
            for (int s = 0; s < kMaxSlots; ++s)
                if (slots[s].ir && s != newest && slots[s].g <= 0.0f) slots[s].dying = true;
        }

        // tail for the next block, mixed with the gains right away
        prepareTail(chL, 0);
        prepareTail(chR, 1);
    }

    void prepareTail(Channel& ch, int c) {
        ch.tailMix.fill(0.0f);
        for (int s = 0; s < kMaxSlots; ++s) {
            if (!slots[s].ir) continue;
            const IRChan& ic = (c == 0) ? slots[s].ir->L : slots[s].ir->R;
            computeTail(ch, ic, ch.tailTmp.data());
            const float* w = &wts[static_cast<size_t>(s) * PART_SIZE];
            for (size_t t = 0; t < PART_SIZE; ++t) ch.tailMix[t] += w[t] * ch.tailTmp[t];
        }
    }
};
