/*
 * Biquad.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

/****************************************************************
    RBJ-derived second-order (biquad) filters in four variants, covering
    two independent choices: topology and coefficient smoothing.

    Topology:
          - Direct-form   : classic, minimal state, cheapest to run.
          - Coupled-form  : mystran's modified coupled-form realization;
                             amplitude-bounded internal state, tolerates
                             abrupt coefficient changes without blowing up.

    Coefficient smoothing:
          - None    : setParameters() applies new coefficients immediately.
          - Smoothed: setParameters() only sets a target; process()
                      advances the coefficients actually in use one small
                      step per sample toward that target.

    Resulting classes:
          Biquad               - direct-form,  no smoothing
          SmoothBiquad         - direct-form,  smoothed
          DynamicBiquad        - coupled-form, no smoothing
          SmoothDynamicBiquad  - coupled-form, smoothed

    Each has a Stereo* wrapper (processes L/R with identical parameters)
    and a Cascade wrapper (a fixed bank of 12 filters driven by
    a shared FilterConfig, for building parametric EQ chains).

    All variants support Peak, Low Shelf, High Shelf, Low Pass, High Pass,
    Band Pass and Notch.

    Which to use: for filters whose parameters change rarely, plain
    Biquad is simplest and cheapest. For filters modulated continuously
    or at high rate (automation, envelope followers, etc.), prefer one of
    the smoothed and/or coupled-form variants - direct-form biquads can
    produce audible artifacts from hard coefficient jumps under those
    conditions, especially at low frequencies.
****************************************************************/

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

#include "FilterConfig.h"

/****************************************************************
    Standard RBJ direct-form biquad filter.

    setParameters() applies new coefficients (b0, b1, b2, a1, a2)
    immediately - suitable for filters whose parameters change
    infrequently (e.g. in response to a user or preset change). Frequent
    parameter updates on a filter with non-zero internal state can
    produce audible artifacts from the resulting hard coefficient jumps.

    Supports Peak, Low Shelf, High Shelf, Low Pass, High Pass, Band Pass
    and Notch
****************************************************************/


class Biquad : public FilterTypes {
public:

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

        if (q < 0.01f) q = 0.01f;
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

            case Type::BandPass:
            {
                b0 =  alpha;
                b1 =  0.0f;
                b2 = -alpha;

                a0 =  1.0f + alpha;
                a1 = -2.0f * cosw;
                a2 =  1.0f - alpha;
                break;
            }

            case Type::Notch:
            {
                b0 =  1.0f;
                b1 = -2.0f * cosw;
                b2 =  1.0f;

                a0 =  1.0f + alpha;
                a1 = -2.0f * cosw;
                a2 =  1.0f - alpha;
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

/****************************************************************
    Stereo version from the RBJ direct-form biquad filter 
****************************************************************/

class StereoBiquad : public FilterTypes {
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
    Biquad left;
    Biquad right;
};

/****************************************************************
    Cascade 12 filter stereo from the RBJ direct-form biquad filter 
****************************************************************/

class Cascade : public FilterTypes {
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
    StereoBiquad lowCut;
    StereoBiquad highCut;
    StereoBiquad filters[NumFilters];

    bool lowCutEnabled  = false;
    bool highCutEnabled = false;
    int bypass = 0;
    int res = 0;
};

/****************************************************************
    RBJ direct-form biquad filter with per-sample coefficient smoothing.

    setParameters() computes only the target coefficients (b0, b1, b2,
    a1, a2); process() advances the coefficients actually in use one
    small step toward that target on every sample, rather than applying
    them immediately. This softens the transition when parameters change
    between calls, reducing the audible artifacts a direct-form biquad
    can produce from a hard coefficient jump while its internal state is
    non-zero.

    Supports Peak, Low Shelf, High Shelf, Low Pass, High Pass, Band Pass
    and Notch
****************************************************************/

class SmoothBiquad : public FilterTypes {
public:

    void prepare(double sampleRate) {
        sr = sampleRate;
        reset();
    }

    void reset() {
        x1 = x2 = 0.0f;
        y1 = y2 = 0.0f;
        b0 = tb0; b1 = tb1; b2 = tb2; a1 = ta1; a2 = ta2;
    }

    void setSmoothCoeff(float c) {
        smoothCoeff = c;
    }

    void setParameters(Type type, float frequency, float q, float gainDB) {
        const float A = std::pow(10.0f, gainDB / 40.0f);
        const float omega = 2.0f * M_PI * frequency / sr;
        const float sinw = std::sin(omega);
        const float cosw = std::cos(omega);

        float qq = q;
        if (qq < 0.01f) qq = 0.01f;
        const float alpha = sinw / (2.0f * qq);

        float nb0 = 1, nb1 = 0, nb2 = 0, na0 = 1, na1 = 0, na2 = 0;

        switch (type) {
            case Type::Peak:
                nb0 = 1 + alpha * A;
                nb1 = -2 * cosw;
                nb2 = 1 - alpha * A;

                na0 = 1 + alpha / A;
                na1 = -2 * cosw;
                na2 = 1 - alpha / A;
                break;

            case Type::LowShelf:
            {
                float beta = std::sqrt(A) / qq;

                nb0 = A*((A+1)-(A-1)*cosw+beta*sinw);
                nb1 = 2*A*((A-1)-(A+1)*cosw);
                nb2 = A*((A+1)-(A-1)*cosw-beta*sinw);

                na0 = (A+1)+(A-1)*cosw+beta*sinw;
                na1 = -2*((A-1)+(A+1)*cosw);
                na2 = (A+1)+(A-1)*cosw-beta*sinw;
                break;
            }

            case Type::HighShelf:
            {
                float beta = std::sqrt(A) / qq;

                nb0 = A*((A+1)+(A-1)*cosw+beta*sinw);
                nb1 = -2*A*((A-1)+(A+1)*cosw);
                nb2 = A*((A+1)+(A-1)*cosw-beta*sinw);

                na0 = (A+1)-(A-1)*cosw+beta*sinw;
                na1 = 2*((A-1)-(A+1)*cosw);
                na2 = (A+1)-(A-1)*cosw-beta*sinw;
                break;
            }

            case Type::LowPass:
                nb0 = (1.0f - cosw) / 2.0f;
                nb1 =  1.0f - cosw;
                nb2 = (1.0f - cosw) / 2.0f;
                na0 =  1.0f + alpha;
                na1 = -2.0f * cosw;
                na2 =  1.0f - alpha;
                break;

            case Type::HighPass:
                nb0 =  (1.0f + cosw) / 2.0f;
                nb1 = -(1.0f + cosw);
                nb2 =  (1.0f + cosw) / 2.0f;
                na0 =   1.0f + alpha;
                na1 =  -2.0f * cosw;
                na2 =   1.0f - alpha;
                break;

            case Type::BandPass:
                nb0 =  alpha;
                nb1 =  0.0f;
                nb2 = -alpha;

                na0 =  1.0f + alpha;
                na1 = -2.0f * cosw;
                na2 =  1.0f - alpha;
                break;

            case Type::Notch:
                nb0 =  1.0f;
                nb1 = -2.0f * cosw;
                nb2 =  1.0f;

                na0 =  1.0f + alpha;
                na1 = -2.0f * cosw;
                na2 =  1.0f - alpha;
                break;
        }

        tb0 = nb0 / na0;
        tb1 = nb1 / na0;
        tb2 = nb2 / na0;
        ta1 = na1 / na0;
        ta2 = na2 / na0;
    }

    inline void smooth(float& c, float target) {
        if ( c == target) return;
        c += smoothCoeff * (target - c);

        constexpr float eps = 1e-8f;
        if (std::fabs(target - c) < eps)
            c = target;
    }

    inline float process(float x) {
        smooth(b0, tb0);
        smooth(b1, tb1);
        smooth(b2, tb2);
        smooth(a1, ta1);
        smooth(a2, ta2);

        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

        x2 = x1;
        x1 = x;

        y2 = y1;
        y1 = y;

        return y;
    }

private:
    double sr = 44100.0;
    float b0=1,b1=0,b2=0,a1=0,a2=0;
    float tb0=1,tb1=0,tb2=0,ta1=0,ta2=0;
    float smoothCoeff = 0.05f;
    float x1=0,x2=0,y1=0,y2=0;
};

/****************************************************************
    Stereo version from the RBJ direct-form biquad filter 
    with per-sample coefficient smoothing.
****************************************************************/

class StereoSmoothBiquad : public FilterTypes {
public:

    void prepare(double sampleRate) {
        left.prepare(sampleRate);
        right.prepare(sampleRate);
    }

    void setSmoothCoeff(float c) {
        left.setSmoothCoeff(c);
        right.setSmoothCoeff(c);
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
    SmoothBiquad left;
    SmoothBiquad right;
};

/****************************************************************
    Cascade 12 filter stereo from the RBJ direct-form biquad filter 
    with per-sample coefficient smoothing.
****************************************************************/

class SmoothCascade : public FilterTypes {
public:

    void prepare(double sampleRate) {
        for(auto& f : filters)
            f.prepare(sampleRate);
        lowCut.prepare(sampleRate);
        highCut.prepare(sampleRate);
        setSmoothTimeMs(5.0f, sampleRate);
    }

    void setSmoothTimeMs(float ms, double sampleRate) {
        float c = 1.0f - std::exp(-1.0f / (0.001f * ms * (float)sampleRate));
        for (auto& f : filters)
            f.setSmoothCoeff(c);
        lowCut.setSmoothCoeff(c);
        highCut.setSmoothCoeff(c);
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
    StereoSmoothBiquad lowCut;
    StereoSmoothBiquad highCut;
    StereoSmoothBiquad filters[NumFilters];

    bool lowCutEnabled  = false;
    bool highCutEnabled = false;
    int bypass = 0;
    int res = 0;
};

/****************************************************************
    RBJ-derived biquad filter using mystran's modified coupled-form
    topology (KVR Audio).

    Unlike a direct-form biquad, this topology keeps its internal state
    amplitude-bounded and does not blow up when coefficients change
    abruptly between calls to setParameters() - useful for filters whose
    parameters are updated frequently, even without additional smoothing.

    Supports Peak, Low Shelf, High Shelf, Low Pass, High Pass, Band Pass
    and Notch. Denormals are flushed to zero in the state variables.
****************************************************************/

class DynamicBiquad : public FilterTypes {
public:

    void prepare(double sampleRate) {
        sr = sampleRate;
        reset();
    }

    void reset() {
        z1 = z2 = 0.0f;
    }

    void setParameters(Type type, float frequency, float q, float gainDB) {

        frequency = std::clamp(frequency, 10.0f, float(sr * 0.49));
        q = std::max(q, 0.01f);

        const double A     = std::pow(10.0, gainDB / 40.0);
        const double omega = 2.0 * M_PI * frequency / sr;
        const double sinw  = std::sin(omega);
        const double cosw  = std::cos(omega);
        const double alpha = sinw / (2.0 * q);

        double b0 = 0.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a0 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;

        switch(type) {

            case Type::Peak:
            {
                b0 = 1.0 + alpha * A;
                b1 = -2.0 * cosw;
                b2 = 1.0 - alpha * A;

                a0 = 1.0 + alpha / A;
                a1 = -2.0 * cosw;
                a2 = 1.0 - alpha / A;
                break;
            }

            case Type::LowShelf:
            {
                const double beta = std::sqrt(A) / q;

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
                const double beta = std::sqrt(A) / q;

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
                b0 = (1.0-cosw)*0.5;
                b1 = 1.0-cosw;
                b2 = (1.0-cosw)*0.5;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;
            }

            case Type::HighPass:
            {
                b0 = (1.0+cosw)*0.5;
                b1 = -(1.0+cosw);
                b2 = (1.0+cosw)*0.5;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;
            }

            case Type::BandPass:
            {
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;
            }

            case Type::Notch:
            {
                b0 = 1.0;
                b1 = -2.0*cosw;
                b2 = 1.0;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;
            }
        }

        // RBJ -> normalized
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;

        a1 /= a0;
        a2 /= a0;

        const double ee = std::sqrt(std::max(1e-20, 1.0 + a1 + a2));

        e  = (float)ee;
        f  = (float)a2;

        t0 = (float)(b0 / ee);
        t1 = (float)(-b2 / ee);
        t2 = (float)((b0 + b1 + b2) / (ee * ee));
    }

    inline float process(float x) {
        float tmp = z1 * f + (x - z2) * e;
        if (std::fabs(tmp) < 1e-15f) tmp = 0.0f;
        float y = tmp * t0 + z1 * t1 + z2 * t2;
        z2 = tmp * e + z2;
        if (std::fabs(z2) < 1e-15f)  z2 = 0.0f;
        z1 = tmp;

        return y;
    }

private:
    double sr = 44100.0;
    // mystran coefficients
    float t0 = 1.0f;
    float t1 = 0.0f;
    float t2 = 0.0f;
    float e  = 1.0f;
    float f  = 0.0f;
    // state
    float z1 = 0.0f;
    float z2 = 0.0f;
    // dcblocker
    double dc_x1 = 0.0;
    double dc_y1 = 0.0;
    double dc_R  = 0.996;
};

/****************************************************************
    Stereo version from the RBJ-derived biquad filter
****************************************************************/

class StereoDynamicBiquad : public FilterTypes {
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
    DynamicBiquad left;
    DynamicBiquad right;
};


/****************************************************************
    Cascade 12 filter stereo from the RBJ-derived biquad filter
****************************************************************/

class DynamicCascade : public FilterTypes {
public:

    void prepare(double sampleRate) {
        for(auto& f : filters)
            f.prepare(sampleRate);
    }

    void reset() {
        for(auto& f : filters)
            f.reset();
        res = 0;
    }

    void setFilter(int index, const FilterConfig& cfg) {
        if(index < 0 || index >= NumFilters)
            return;

        filters[index].setParameters(cfg.type, cfg.frequency, cfg.q, cfg.gainDB);
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
            for (auto& f : filters) f.process(l, r);
            output[i] = l;
            output1[i] = r;
         }
        
    }

private:
    StereoDynamicBiquad filters[NumFilters];

    int bypass = 0;
    int res = 0;
};

/****************************************************************
    RBJ-derived biquad filter using mystran's modified coupled-form
    topology, with additional per-sample coefficient smoothing.

    setParameters() computes only the target coupled-form coefficients
    (t0, t1, t2, e, f); process() advances the coefficients actually in
    use one small step toward that target on every sample, rather than
    applying them immediately. Combined with the coupled-form topology's
    bounded internal state, this makes the filter safe for continuous,
    high-rate parameter modulation (e.g. automation or a control signal
    updating gain/frequency/Q every block) without the energy spikes or
    instability that direct-form biquads can exhibit under the same
    conditions - particularly at low frequencies, where hard coefficient
    jumps interact badly with the filter's long ring-down time.

    Supports Peak, Low Shelf, High Shelf, Low Pass, High Pass, Band Pass
    and Notch. Denormals are flushed to zero in the state variables.
****************************************************************/

class SmoothDynamicBiquad : public FilterTypes {
public:

    void prepare(double sampleRate) {
        sr = sampleRate;
        reset();
    }

    void reset() {
        z1 = z2 = 0.0f;
        t0 = tt0; t1 = tt1; t2 = tt2; e = te; f = tf;
    }

    void setSmoothCoeff(float c) {
        smoothCoeff = c;
    }

    void setParameters(Type type, float frequency, float q, float gainDB) {
        frequency = std::clamp(frequency, 10.0f, float(sr * 0.49));
        q = std::max(q, 0.01f);

        const double A     = std::pow(10.0, gainDB / 40.0);
        const double omega = 2.0 * M_PI * frequency / sr;
        const double sinw  = std::sin(omega);
        const double cosw  = std::cos(omega);
        const double alpha = sinw / (2.0 * q);

        double b0 = 0.0, b1 = 0.0, b2 = 0.0, a0 = 0.0, a1 = 0.0, a2 = 0.0;

        switch (type) {
            case Type::Peak:
                b0 = 1.0 + alpha * A;
                b1 = -2.0 * cosw;
                b2 = 1.0 - alpha * A;

                a0 = 1.0 + alpha / A;
                a1 = -2.0 * cosw;
                a2 = 1.0 - alpha / A;
                break;

            case Type::LowShelf:
            {
                const double beta = std::sqrt(A) / q;

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
                const double beta = std::sqrt(A) / q;

                b0 = A*((A+1)+(A-1)*cosw+beta*sinw);
                b1 = -2*A*((A-1)+(A+1)*cosw);
                b2 = A*((A+1)+(A-1)*cosw-beta*sinw);

                a0 = (A+1)-(A-1)*cosw+beta*sinw;
                a1 = 2*((A-1)-(A+1)*cosw);
                a2 = (A+1)-(A-1)*cosw-beta*sinw;
                break;
            }

            case Type::LowPass:
                b0 = (1.0-cosw)*0.5;
                b1 = 1.0-cosw;
                b2 = (1.0-cosw)*0.5;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;

            case Type::HighPass:
                b0 = (1.0+cosw)*0.5;
                b1 = -(1.0+cosw);
                b2 = (1.0+cosw)*0.5;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;

            case Type::BandPass:
                b0 = alpha;
                b1 = 0.0;
                b2 = -alpha;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;

            case Type::Notch:
                b0 = 1.0;
                b1 = -2.0*cosw;
                b2 = 1.0;

                a0 = 1.0+alpha;
                a1 = -2.0*cosw;
                a2 = 1.0-alpha;
                break;
        }

        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;

        const double ee = std::sqrt(std::max(1e-20, 1.0 + a1 + a2));

        te  = (float)ee;
        tf  = (float)a2;
        tt0 = (float)(b0 / ee);
        tt1 = (float)(-b2 / ee);
        tt2 = (float)((b0 + b1 + b2) / (ee * ee));
    }

    inline void smooth(float& c, float target) {
        if ( c == target) return;
        c += smoothCoeff * (target - c);

        constexpr float eps = 1e-8f;
        if (std::fabs(target - c) < eps)
            c = target;
    }

    inline float process(float x) {
        smooth(t0, tt0);
        smooth(t1, tt1);
        smooth(t2, tt2);
        smooth(e,  te);
        smooth(f,  tf);

        float tmp = z1 * f + (x - z2) * e;
        if (std::fabs(tmp) < 1e-15f) tmp = 0.0f;
        float y = tmp * t0 + z1 * t1 + z2 * t2;
        z2 = tmp * e + z2;
        if (std::fabs(z2) < 1e-15f) z2 = 0.0f;
        z1 = tmp;

        return y;
    }

private:
    double sr = 44100.0;
    float t0 = 1.0f, t1 = 0.0f, t2 = 0.0f, e = 1.0f, f = 0.0f;
    float tt0 = 1.0f, tt1 = 0.0f, tt2 = 0.0f, te = 1.0f, tf = 0.0f;
    float smoothCoeff = 0.05f;

    float z1 = 0.0f, z2 = 0.0f;
};

/****************************************************************
    Stereo version from the RBJ-derived biquad filter
    with additional per-sample coefficient smoothing.
****************************************************************/

class StereoSmoothDynamicBiquad : public FilterTypes {
public:

    void prepare(double sampleRate) {
        left.prepare(sampleRate);
        right.prepare(sampleRate);
    }

    void reset() {
        left.reset();
        right.reset();
    }

    void setSmoothCoeff(float c) {
        left.setSmoothCoeff(c);
        right.setSmoothCoeff(c);
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
    SmoothDynamicBiquad left;
    SmoothDynamicBiquad right;
};

/****************************************************************
    Cascade 12 filter stereo from the RBJ-derived biquad filter
    with additional per-sample coefficient smoothing.
****************************************************************/

class SmoothDynamicCascade : public FilterTypes {
public:

    void prepare(double sampleRate) {
        for(auto& f : filters)
            f.prepare(sampleRate);
        setSmoothTimeMs(15.0f, sampleRate);
    }

    void setSmoothTimeMs(float ms, double sampleRate) {
        float c = 1.0f - std::exp(-1.0f / (0.001f * ms * (float)sampleRate));
        for (auto& f : filters)
            f.setSmoothCoeff(c);
    }

    void reset() {
        for(auto& f : filters)
            f.reset();
        res = 0;
    }

    void setFilter(int index, const FilterConfig& cfg) {
        if(index < 0 || index >= NumFilters)
            return;

        filters[index].setParameters(cfg.type, cfg.frequency, cfg.q, cfg.gainDB);
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
            for (auto& f : filters) f.process(l, r);
            output[i] = l;
            output1[i] = r;
         }
        
    }

private:
    StereoSmoothDynamicBiquad filters[NumFilters];

    int bypass = 0;
    int res = 0;
};
