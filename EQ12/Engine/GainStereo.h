/*
 * GainStereo.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

class GainStereo {
public:
    float gain = 0.0f;
    float* meterLout = &meterL;
    float* meterRout = &meterR;

    GainStereo() {}
    ~GainStereo() {}

    inline void clear_state() {
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 2; ++i) {
                fRec3[ch][i] = 0.0;
                fRec0[ch][i] = 0.0;
                fRec2[ch][i] = 0.0;
                iRec1[ch][i] = 0;
            }
        }
    }

    void setGain(float g) {
        gain = g;
    }

    float getMeterL() const {
        return meterL;
    }

    float getMeterR() const {
        return meterR;
    }

    float getMeterMono() const {
        return std::max(meterL, meterR);
    }

    inline void init(uint32_t samplingFreq) {
        fSamplingFreq = samplingFreq;
        fConst0 = (1.0 / std::min<double>(192000.0, std::max<double>( 1.0, double(fSamplingFreq))));
        gain = 0.0f;
        powerL = 20.0 * log10(0.0000003);
        powerR = 20.0 * log10(0.0000003);
        meterL = -130.0f;
        meterR = -130.0f;
        clear_state();
    }

    void process(int count, const float* inputL, const float* inputR,
                                    float* outputL, float* outputR) {

        const double fSlow0 = (0.0010000000000000009 * std::pow(10.0, (0.050000000000000003 * double(gain))));

        for (int i = 0; i < count; ++i) {
            processSample(0, inputL[i], outputL[i], powerL, meterL, fSlow0);
            processSample(1, inputR[i], outputR[i], powerR, meterR, fSlow0);
        }

        meterL = 20.0f * log10(std::max<double>(0.0000003, powerL));
        meterR = 20.0f * log10(std::max<double>( 0.0000003, powerR));
        *meterLout = meterL;
        *meterRout = meterR;
    }

private:

    void processSample(int ch, float input, float& output, double& power,
                                        float& meter, double gainSmoothed) {

        int iTemp0 = (iRec1[ch][1] < 4096);
        fRec3[ch][0] = gainSmoothed + (0.999 * fRec3[ch][1]);
        double fTemp1 = fRec3[ch][0] * double(input);
        double fTemp2 = std::max<double>(fConst0, std::fabs(fTemp1));

        fRec0[ch][0] = (iTemp0) ? std::max<double>(fRec0[ch][1], fTemp2) : fTemp2;
        iRec1[ch][0] = (iTemp0) ? (iRec1[ch][1] + 1) : 1;
        fRec2[ch][0] = (iTemp0) ? fRec2[ch][1] : fRec0[ch][1];

        power = fRec2[ch][0];
        output = (float)fTemp1;

        fRec3[ch][1] = fRec3[ch][0];
        fRec0[ch][1] = fRec0[ch][0];
        fRec2[ch][1] = fRec2[ch][0] * 0.99999999;
        iRec1[ch][1] = iRec1[ch][0];
    }

private:

    uint32_t fSamplingFreq = 48000;

    double fConst0 = 0.0;
    double powerL = 0.0;
    double powerR = 0.0;
    double fRec3[2][2];
    double fRec0[2][2];
    double fRec2[2][2];
    int iRec1[2][2];

    float meterL = -130.0f;
    float meterR = -130.0f;
};
