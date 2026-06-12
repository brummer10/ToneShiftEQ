
/*
 * FFTAnalyzer.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


/****************************************************************
        FFTAnalyzer.h - frequency analysis 

****************************************************************/



#pragma once

#include <fftw3.h>
#include <cmath>
#include <atomic>
#include <cstring>

class FFTAnalyzer {
public:
    FFTAnalyzer() = default;

    ~FFTAnalyzer() {
        cleanup();
    }

    void init(int fft_size, float sr) {
        cleanup(); 

        N = fft_size;
        bins = fft_size / 2;
        sample_rate = sr;

        fifo_pos = 0;
        hop_size = N / 2;
        samples_since_last_fft = 0;

        write_index = 0;
        read_index  = 1;
        buffer_ready.store(false, std::memory_order_relaxed);

        in     = (float*)fftwf_malloc(sizeof(float) * N);
        out    = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (bins + 1));
        window = new float[N];
        fifo   = new float[N]();
        smooth = new float[bins];

        mags[0] = new float[bins];
        mags[1] = new float[bins];

        std::memset(in, 0,  N * sizeof(float));
        std::memset(out, 0,  N * sizeof(float));
        std::memset(window, 0, N * sizeof(float));
        std::memset(fifo, 0, N * sizeof(float));
        std::memset(smooth, 0, bins * sizeof(float));

        std::memset(mags[0], 0, bins * sizeof(float));
        std::memset(mags[1], 0, bins * sizeof(float));

        build_hann(window, N);

        float window_gain = compute_window_gain(window, N);
        norm_factor = (2.0f / (N * window_gain));
        plan = fftwf_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

        for (int i = 0; i < bins; ++i) smooth[i] = -90.0f;

        attack  = 0.6f;
        release = 0.05f;
        initialized = true;
    }

    bool isInitialized() const {
        return initialized;
    }

    void reset() {
        if (!initialized) return;

        std::memset(fifo, 0, sizeof(float) * N);
        fifo_pos = 0;
        samples_since_last_fft = 0;

        for (int i = 0; i < bins; ++i)
            smooth[i] = -90.0f;

        buffer_ready.store(false, std::memory_order_release);
    }

    void processBlock(const float* input, int n_samples) {
        if (!initialized) return;

        for (int i = 0; i < n_samples; ++i) {

            float v = input[i];
            if (!std::isfinite(v)) v = 0.0f;

            fifo[fifo_pos++] = v;
            if (fifo_pos >= N)
                fifo_pos = 0;

            samples_since_last_fft++;

            while (samples_since_last_fft >= hop_size) {
                samples_since_last_fft -= hop_size;
                processFFT();
            }
        }
    }

    const float* getMagnitudes() const {
        return mags[read_index];
    }

    bool hasNewData() const {
        return buffer_ready.load(std::memory_order_acquire);
    }

    void clearFlag() {
        buffer_ready.store(false, std::memory_order_release);
    }

    int getBins() const { return bins; }

    void cleanup() {
        if (!initialized) return;

        fftwf_destroy_plan(plan);
        fftwf_free(in);
        fftwf_free(out);

        delete[] window;
        delete[] fifo;
        delete[] smooth;
        delete[] mags[0];
        delete[] mags[1];

        initialized = false;
    }

private:
    int N = 0, bins = 0;
    float sample_rate = 0.0f;

    float* in = nullptr;
    float* window = nullptr;
    float* fifo = nullptr;
    float* smooth = nullptr;
    fftwf_complex* out = nullptr;
    fftwf_plan plan = nullptr;

    float* mags[2] = {nullptr, nullptr};

    int write_index = 0;
    int read_index  = 1;

    std::atomic<bool> buffer_ready {false};

    float norm_factor = 1.0f;

    int fifo_pos = 0;
    int hop_size = 0;
    int samples_since_last_fft = 0;

    float attack = 0.6f;
    float release = 0.05f;

    bool initialized = false;

private:

    void build_hann(float* w, int N) {
        for (int i = 0; i < N; ++i) {
            w[i] = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (N - 1)));
        }
    }

    float compute_window_gain(const float* w, int N) {
        float sum = 0.0f;
        for (int i = 0; i < N; ++i)
            sum += w[i];
        return sum / (float)N;
    }

    void processFFT() {
        int idx = fifo_pos - N;
        if (idx < 0) idx += N;
        for (int j = 0; j < N; ++j) {
            float s = fifo[idx];
            in[j] = s * window[j];

            idx++;
            if (idx >= N)
                idx = 0;
        }

        fftwf_execute(plan);

        float* write_buf = mags[write_index];

        for (int k = 0; k < bins; ++k) {
            float re = out[k][0];
            float im = out[k][1];

            float mag = std::sqrt(re * re + im * im);
            mag *= norm_factor;

            if (!std::isfinite(mag) || mag <= 0.0f)
                mag = 1e-20f;

            float db = 20.0f * std::log10(mag);

            float prev = smooth[k];
            float t = (float)k / (float)bins;
            float rel = release * (0.5f + t);

            float coeff = (db > prev) ? attack : rel;
            float smoothed = prev + coeff * (db - prev);

            smooth[k] = smoothed;
            write_buf[k] = smoothed;
        }

        // swap
        int new_read = write_index;
        write_index = read_index;
        read_index  = new_read;

        buffer_ready.store(true, std::memory_order_release);
    }
};
