

/*
 * IRtoEQ.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include "Band.h"

#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

/****************************************************************
 * IRtoEQ
 *
 * @brief Approximates a frequency response with a 12-band parametric EQ,
 * including low-cut and high-cut detection.
****************************************************************/

class IRtoEQ {
public:
    struct EQSettings {
        std::array<float, 12> gain{};
        std::array<float, 12> freq{};
        std::array<float, 12> Q{};
        std::array<float, 12> enabled{};
        float lowCut = 19.0f;
        float highCut = 20000.0f;

        EQSettings() {
            for (size_t i = 0; i < 12; ++i) {
                freq[i] = defs[i].freqDef;
                Q[i] = defs[i].qDef;
                enabled[i] = 1.0f;
            }
        }
    };

    EQSettings extractEQSettings(const std::vector<double>& response, double sampleRate) {
        EQSettings eq;
        if (response.empty()) return eq;

        const double hzToBin = 2.0 * (response.size() - 1) / sampleRate;
        const double binToHz = sampleRate / (2.0 * (response.size() - 1));

        auto freqToBin = [&](double freq) {
            return freq * hzToBin;
        };

        auto sampleAtFreq = [&](double freq) {
            double pos = freqToBin(freq);
            if (pos <= 0.0) return response.front();
            if (pos >= response.size() - 1) return response.back();
            size_t i0 = static_cast<size_t>(std::floor(pos));
            size_t i1 = std::min<size_t>(i0 + 1, response.size() - 1);
            float t = static_cast<float>(pos - i0);
            return response[i0] * (1.0f - t) + response[i1] * t;
        };

        // Band Gains and default Freq and Q
        for (size_t i = 0; i < 12; ++i) {
            eq.gain[i] = sampleAtFreq(defs[i].freqDef) * 0.4321f;
            eq.freq[i] = defs[i].freqDef;
            eq.Q[i]    = defs[i].qDef;
            eq.enabled[i] = 1.0f;
        }

        // Smooth edges for low/high-cut detection
        std::vector<double>smooth(response.size());

        for (size_t i = 0; i < response.size(); ++i) {
            float sum = 0.0f;
            int count = 0;
            for (int k = -2; k <= 2; ++k) {
                int idx = static_cast<int>(i) + k;
                if (idx >= 0 && idx < static_cast<int>(response.size())) {
                    sum += response[idx];
                    ++count;
                }
            }
            smooth[i] = sum / count;
        }

        // jump point
        const float peak = *std::max_element(smooth.begin(), smooth.end());
        const float threshold = peak - 6.0f;

        // LowCut
        size_t lowBegin = std::max<size_t>(1, static_cast<size_t>(freqToBin(20.0)));
        size_t lowEnd = std::min<size_t>( smooth.size() - 1, static_cast<size_t>(freqToBin(250.0)));

        for (size_t i = lowBegin + 1; i < lowEnd; ++i) {
            if (smooth[i] > threshold) {
                float t = (threshold - smooth[i-1]) / (smooth[i] - smooth[i-1]);
                double bin = (i - 1) + t;
                float lc = static_cast<float>(bin * binToHz);
                eq.lowCut = lc < 250.0f ? lc : 19.0f;
                break;
            }
        }

        // HighCut
        size_t highBegin = std::min<size_t>(smooth.size() - 1, static_cast<size_t>(freqToBin(2000.0)));
        size_t highEnd = std::min<size_t>(smooth.size() - 1, static_cast<size_t>(freqToBin(20000.0)));

        for (size_t i = highEnd; i > highBegin; --i) {
            if (smooth[i] > threshold) {
                float t = (threshold - smooth[i-1]) / (smooth[i] - smooth[i-1]);
                double bin = (i - 1) + t;
                float hc = static_cast<float>( bin * binToHz);
                eq.highCut = hc > 2000.0f ? hc : 20000.0f;
                break;
            }
        }

        return eq;
    }

};
