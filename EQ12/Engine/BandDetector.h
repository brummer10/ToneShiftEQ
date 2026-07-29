
/*
 * BandDetector.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */
 
/****************************************************************
 * @file BandDetector.h
 * @brief Bandpass-filtered envelope detectors for multi-band level metering.
 *
 * Provides a mono BandDetector (bandpass filter + attack/release envelope
 * follower), a StereoBandDetector wrapping a left/right pair, and a
 * Detector class managing an array of NumFilters stereo band detectors
 * for multi-band RMS/power/dB analysis.
****************************************************************/
 
#pragma once

#include <cmath>
#include "Biquad.h"

class BandDetector {
public:

    void prepare(double sr) {
        sampleRate = sr;
        filter.prepare(sr);
        reset();
    }

    void reset() {
        filter.reset();
        envelope = 0.0f;
    }

    void setParameters(float frequency, float q, float attackMs = 10.0f, float releaseMs = 100.0f) {
        filter.setParameters(Biquad::Type::BandPass, frequency, q, 0.0f);

        attack  = calcCoeff(attackMs, frequency, 10.0f); // 1.0
        release = calcCoeff(releaseMs, frequency, 20.0f); // 3.0
    }

    void process(const float input) {
        // Bandpass
        float y = filter.process(input);
        // Instantaneous power
        float level = y * y;
        // Attack / Release envelope
        if (level > envelope)
            envelope += attack * (level - envelope);
        else
            envelope += release * (level - envelope);

    }

    float getRMS() const {
        return std::sqrt(envelope + 1e-20f);
    }

    float getPower() const {
        return envelope;
    }

    float getDB() const {
        return 20.0f * std::log10(getRMS() + 1e-20f);
    }

private:

    float calcCoeff(float timeMs, float frequency, float coef) const {
        float periodMs = (1000.0 / frequency) * coef;
        if (timeMs < periodMs) timeMs = periodMs;

        return 1.0f - std::exp(-1.0f / (0.001f * timeMs * sampleRate));
    }

    Biquad filter;

    double sampleRate = 44100.0;
    float attack  = 0.01f;
    float release = 0.001f;
    float envelope = 0.0f;
};

class StereoBandDetector {
public:

    void prepare(double sr) {
        left.prepare(sr);
        right.prepare(sr);
    }

    void reset() {
        left.reset();
        right.reset();
    }

    void setParameters(float freq, float q, float attack = 10.0f, float release = 100.0f) {
        left.setParameters(freq, q, attack, release);
        right.setParameters(freq, q, attack, release);
    }

    void process(const float l, const float r) {
        left.process(l);
        right.process(r);
    }

    float getPower() const {
        return 0.5f * (left.getPower() + right.getPower());
    }

    float getRMS() const {
        return std::sqrt(getPower() + 1e-20f);
    }

    float getDB() const {
        return 20.0f * std::log10(getRMS() + 1e-20f);
    }

private:
    BandDetector left;
    BandDetector right;
};

class Detector {
public:

    static constexpr int NumFilters = FilterTypes::NumFilters;

    struct DetectorConfig {
        float frequency;
        float q;
        float attackMs;
        float releaseMs;
    };

    void prepare(double sampleRate) {
        for (auto& d : detectors)
            d.prepare(sampleRate);
    }

    void reset() {
        for (auto& d : detectors)
            d.reset();
    }

    void setDetector(int index, const DetectorConfig& cfg) {
        if (index < 0 || index >= NumFilters) return;

        detectors[index].setParameters(cfg.frequency, cfg.q, cfg.attackMs, cfg.releaseMs);
    }

    void process(const float l, const float r) {
        for (auto& d : detectors)
            d.process(l, r);
    }

    void processBlock(uint32_t nframes, const float* input, const float* input1) {
        for (uint32_t i = 0; i < nframes; i++) {
            float l = input[i], r = input1[i];
            process(l, r);
        }
    }

    float getPower(int index) const {
        if (index < 0 || index >= NumFilters)
            return 0.0f;

        return detectors[index].getPower();
    }

    float getRMS(int index) const {
        if (index < 0 || index >= NumFilters)
            return 0.0f;

        return detectors[index].getRMS();
    }

    float getDB(int index) const {
        if (index < 0 || index >= NumFilters)
            return -120.0f;

        return detectors[index].getDB();
    }

private:

    StereoBandDetector detectors[NumFilters];
};
