/*
 * SVF.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <cmath>

#include "FilterConfig.h"

/****************************************************************
 * @file SVF.h
 * @brief State variable filter (SVF), designed by Andrew Simper of Cytomic.
 * @see http://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf
****************************************************************/

class SVF : public FilterTypes {
public:


    void prepare(double sr) noexcept {
        sampleRate = sr;
        m0 = 0.0f;
        m1 = 0.0f;
        m2 = 0.0f;
        reset();
    }

    void reset() noexcept {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    void setParameters(Type type, float freq, float Q, float dbGain) {
        switch(type) {
            case Type::Peak: // Bell
            {
                const double A = std::pow(10.0, dbGain / 40.0);
                g = std::tan(M_PI * freq / sampleRate);
                k = 1.0 / (Q * A);
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0f;
                m1 = k * (A*A - 1.0);
                m2 = 0.0f;
                break;
            }
 
            case Type::LowShelf:
            {
                const double A = std::pow(10.0, dbGain / 40.0);
                g = std::tan(M_PI * freq / sampleRate) / std::sqrt(A);
                k = 1.0 / Q;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0f;
                m1 = k * (A - 1.0);
                m2 = (A*A - 1.0);
                break;
            }

            case Type::HighShelf:
            {
                const double A = std::pow(10.0, dbGain / 40.0);
                g = std::tan(M_PI * freq / sampleRate) * std::sqrt(A);
                k = 1.0 / Q;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = A * A;
                m1 = k * (1.0 - A) * A;
                m2 = (1.0 - A*A);
                break;
            }

            case Type::LowPass:
            {
                g = std::tan(M_PI * freq / sampleRate);
                k = 1.0 / Q;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 0.0f;
                m1 = 0.0f;
                m2 = 1.0f;
                break;
            }

            case Type::HighPass:
            {
                g = std::tan(M_PI * freq / sampleRate);
                k = 1.0 / Q;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0f;
                m1 = -k;
                m2 = -1.0f;
                break;
            }

            case Type::BandPass:
            {
                g = std::tan(M_PI * freq / sampleRate);
                k = 1.0 / Q;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 0.0f;
                m1 = k;     // paper says 1, but that is not same as RBJ bandpass
                m2 = 0.0f;
                break;
            }

            case Type::Notch:
            {
                g = std::tan(M_PI * freq / sampleRate);
                k = 1.0 / Q;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
                m0 = 1.0f;
                m1 = -k;
                m2 = 0.0f;
                break;
            }
        }
    }

    inline float process(float v0) noexcept {
        float v3 = v0 - ic2eq;
        float v1 = a1 * ic1eq + a2 * v3;
        float v2 = ic2eq + a2 * ic1eq + a3 * v3;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

private:
    double sampleRate = 44100.0;
    float g=0, k=0, a1=0, a2=0, a3=0;  // filter coefficients
    float m0=0, m1=0, m2=0;        // mix coefficients
    float ic1eq=0, ic2eq=0;      // internal state

};


class StereoSVF : public FilterTypes {
public:

    void prepare(double sampleRate) {
        left.prepare(sampleRate);
        right.prepare(sampleRate);
    }

    void reset() {
        left.reset();
        right.reset();
    }

    void setParameters(Type type, float frequency, float q, float gainDB) {
        left.setParameters(type, frequency, q, gainDB);

        right.setParameters(type, frequency, q, gainDB);
    }

    inline void process(float& l, float& r) {
        l = left.process(l);
        r = right.process(r);
    }

private:
    SVF left;
    SVF right;
};


class SVFCascade : public FilterTypes {
public:

    void prepare(double sampleRate) {
        for(auto& f : filters)
            f.prepare(sampleRate);
        lowCut.prepare(sampleRate);
        highCut.prepare(sampleRate);
    }

    void reset() {
        for(auto& f : filters)
            f.reset();
        lowCut.reset();
        highCut.reset();
        res = 0;
    }

    void setFilter(int index, const FilterConfig& cfg) {
        if(index < 0 || index >= NumFilters)
            return;

        filters[index].setParameters(cfg.type, cfg.frequency, cfg.q, cfg.gainDB);
    }

    void setLowCut(float freq, bool enabled) {
        if (enabled)
            lowCut.setParameters(Type::HighPass, freq, 0.707f, 0.0f);
        lowCutEnabled = enabled;
    }

    void setHighCut(float freq, bool enabled) {
        if (enabled)
            highCut.setParameters(Type::LowPass, freq, 0.707f, 0.0f);
        highCutEnabled = enabled;
    }

    void setBypass(int b) {
        bypass = b;
        res = 1;
    }

    void process(float& l,float& r) {
        for(auto& f : filters)
            f.process(l,r);
    }

    void processBlock(uint32_t nframes, float* output, float* output1) {
        if (bypass) {
            if (res) reset();
            return;
        }
        for (uint32_t i = 0; i < nframes; i++) {
            float l = output[i], r = output1[i];
            if (lowCutEnabled)  lowCut.process(l, r);
            if (highCutEnabled) highCut.process(l, r);
            for (auto& f : filters) f.process(l, r);
            output[i] = l;
            output1[i] = r;
         }
        
    }

private:
    StereoSVF lowCut;
    StereoSVF highCut;
    StereoSVF filters[NumFilters];

    bool lowCutEnabled  = false;
    bool highCutEnabled = false;
    int bypass = 0;
    int res = 0;
};
