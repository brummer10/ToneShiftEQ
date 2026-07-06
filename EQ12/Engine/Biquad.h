/*
 * Biquad.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#pragma once

#include <vector>
#include <cmath>

class Biquad {
public:
    enum class Type {
        Peak,
        LowShelf,
        HighShelf,
        LowPass,
        HighPass
    };

    void prepare(double sampleRate) {
        sr = sampleRate;
        reset();
    }

    void reset() {
        x1 = x2 = 0.0;
        y1 = y2 = 0.0;
    }

    void setParameters(Type type, float frequency, float q, float gainDB) {
        const float A = std::pow(10.0f, gainDB / 40.0f);
        const float omega = 2.0f * M_PI * frequency / sr;
        const float sinw = std::sin(omega);
        const float cosw = std::cos(omega);

        const float alpha = sinw / (2.0f * q);

        float b0 = 0.0,b1 = 0.0,b2 = 0.0,a0 = 0.0,a1 = 0.0,a2 = 0.0;

        switch(type) {
            case Type::Peak:
            {
                b0 = 1 + alpha * A;
                b1 = -2 * cosw;
                b2 = 1 - alpha * A;

                a0 = 1 + alpha / A;
                a1 = -2 * cosw;
                a2 = 1 - alpha / A;
                break;
            }

            case Type::LowShelf:
            {
                float beta = std::sqrt(A) / q;

                b0 = A*((A+1)-(A-1)*cosw+beta*sinw);
                b1 = 2*A*((A-1)-(A+1)*cosw);
                b2 = A*((A+1)-(A-1)*cosw-beta*sinw);

                a0 = (A+1)+(A-1)*cosw+beta*sinw;
                a1 = -2*((A-1)+(A+1)*cosw);
                a2 = (A+1)+(A-1)*cosw-beta*sinw;

                break;
            }

            case Type::HighShelf:
            {
                float beta = std::sqrt(A) / q;

                b0 = A*((A+1)+(A-1)*cosw+beta*sinw);
                b1 = -2*A*((A-1)+(A+1)*cosw);
                b2 = A*((A+1)+(A-1)*cosw-beta*sinw);

                a0 = (A+1)-(A-1)*cosw+beta*sinw;
                a1 = 2*((A-1)-(A+1)*cosw);
                a2 = (A+1)-(A-1)*cosw-beta*sinw;

                break;
            }

            case Type::LowPass:
            {
                b0 = (1.0f - cosw) / 2.0f;
                b1 =  1.0f - cosw;
                b2 = (1.0f - cosw) / 2.0f;
                a0 =  1.0f + alpha;
                a1 = -2.0f * cosw;
                a2 =  1.0f - alpha;
                break;
            }

            case Type::HighPass:
            {
                b0 =  (1.0f + cosw) / 2.0f;
                b1 = -(1.0f + cosw);
                b2 =  (1.0f + cosw) / 2.0f;
                a0 =   1.0f + alpha;
                a1 =  -2.0f * cosw;
                a2 =   1.0f - alpha;
                break;
            }
        }

        // normalize
        this->b0 = b0 / a0;
        this->b1 = b1 / a0;
        this->b2 = b2 / a0;

        this->a1 = a1 / a0;
        this->a2 = a2 / a0;
    }

    inline float process(float x) {
        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

        x2=x1;
        x1=x;

        y2=y1;
        y1=y;

        return y;
    }

private:

    double sr = 44100.0;

    float b0=1,b1=0,b2=0;
    float a1=0,a2=0;

    float x1=0,x2=0;
    float y1=0,y2=0;
};

class StereoBiquad {
public:

    void prepare(double sampleRate) {
        left.prepare(sampleRate);
        right.prepare(sampleRate);
    }

    void reset() {
        left.reset();
        right.reset();
    }

    void setParameters(Biquad::Type type, float frequency, float q, float gainDB) {
        left.setParameters(type, frequency, q, gainDB);

        right.setParameters(type, frequency, q, gainDB);
    }

    inline void process(float& l, float& r) {
        l = left.process(l);
        r = right.process(r);
    }

private:
    Biquad left;
    Biquad right;
};

class ToneShiftCascade {
public:

    static constexpr int NumFilters = 12;

    struct FilterConfig {
        Biquad::Type type;
        float frequency;
        float q;
        float gainDB;
    };

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
            lowCut.setParameters(Biquad::Type::HighPass, freq, 0.707f, 0.0f);
        lowCutEnabled = enabled;
    }

    void setHighCut(float freq, bool enabled) {
        if (enabled)
            highCut.setParameters(Biquad::Type::LowPass, freq, 0.707f, 0.0f);
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
    StereoBiquad lowCut;
    StereoBiquad highCut;
    StereoBiquad filters[NumFilters];

    bool lowCutEnabled  = false;
    bool highCutEnabled = false;
    int bypass = 0;
    int res = 0;
};
