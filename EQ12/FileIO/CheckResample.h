/*
 * CheckResample.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2024 brummer <brummer@web.de>
 */

/****************************************************************
        CheckResample.h - resample buffer when needed
                          using cubic hermite interpolation
****************************************************************/

#pragma once

#include <cstdint>
#include <cmath>
#include <vector>

class CheckResample {
public:
    CheckResample() = default;

    std::vector<double> checkSampleRate(uint32_t fs_in, uint32_t fs_out,
                        uint32_t chan, const std::vector<double>& input) {
        if (fs_in == fs_out)
            return input;

        const uint32_t inFrames = input.size() / chan;

        const double ratio = double(fs_in) / double(fs_out);
        const uint32_t outFrames = static_cast<uint32_t>(std::ceil(inFrames / ratio));

        std::vector<double> out(outFrames * chan);

        for (uint32_t ch = 0; ch < chan; ++ch) {
            double srcPos = 0.0;

            for (uint32_t i = 0; i < outFrames; ++i) {
                const uint32_t ip = static_cast<uint32_t>(srcPos);
                const double t = srcPos - ip;

                auto S = [&](int idx) -> double {
                    if (idx < 0)
                        return input[ch];

                    if (static_cast<uint32_t>(idx) >= inFrames)
                        return input[(inFrames - 1) * chan + ch];

                    return input[idx * chan + ch];
                };

                const double x0 = S(ip - 1);
                const double x1 = S(ip);
                const double x2 = S(ip + 1);
                const double x3 = S(ip + 2);

                out[i * chan + ch] = hermite(x0, x1, x2, x3, t);

                srcPos += ratio;
            }
        }
        return out;
    }

private:
    static inline double hermite(double x0, double x1, double x2, double x3, double t) {
        const double c0 = x1;
        const double c1 = 0.5 * (x2 - x0);
        const double c2 = x0 - 2.5 * x1 + 2.0 * x2 - 0.5 * x3;
        const double c3 = 0.5 * (x3 - x0) + 1.5 * (x1 - x2);

        return ((c3 * t + c2) * t + c1) * t + c0;
    }
};
