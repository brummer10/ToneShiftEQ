/*
 * IRMorpherStereo.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <memory>
#include <cmath>
#include <cstring>
#include <atomic>
#include <utility>

#include "Convolver.h"

class IRMorpherStereo {
public:
    using Vec  = std::vector<double>;
    int bypass = 0;

    IRMorpherStereo(size_t blockSize = 256, size_t headSize  = 64) {
        convA = std::make_unique<MasterConvolver>();
        bufferAL.resize(8194);
        bufferAR.resize(8194);
    }

    ~IRMorpherStereo() {}

    // load initial stereo IR
    void init(const std::pair<Vec, Vec> ir) {
        convA->setIR(ir.first.data(), ir.second.data());
        IrReady.store(true, std::memory_order_release);
    }

    // change stereo IR
    void setIR(const std::pair<Vec, Vec> ir) {
        convA->setIR(ir.first.data(), ir.second.data());
        IrReady.store(true, std::memory_order_release);
    }

    void setBypass(int bp) {
        bypass = bp;
    }

    size_t getLatency() { return convA->getLatency(); }

    void process(uint32_t nframes, const float* inputL, const float* inputR,
                                            float* outputL, float* outputR) {

        if (!IrReady.load(std::memory_order_acquire)) {

            if (outputL != inputL)
                std::memcpy(outputL, inputL, nframes * sizeof(float));

            if (outputR != inputR)
                std::memcpy(outputR, inputR, nframes * sizeof(float));

            return;
        }
        convA->setBypass(bypass);

        // process active IR
        convA->process(nframes, inputL, inputR, bufferAL.data(), bufferAR.data());
        std::memcpy(outputL, bufferAL.data(), sizeof(float) * nframes);
        std::memcpy(outputR, bufferAR.data(), sizeof(float) * nframes);
    }

    void setMode(int live, const std::pair<Vec, Vec> ir) {
        if (live == 0)
            convA = std::make_unique<MasterConvolver>();
        else
            convA = std::make_unique<LiveConvolver>();
        setIR(ir);
    }

private:

    std::unique_ptr<ConvolverBase> convA;

    // current IR output
    std::vector<float> bufferAL;
    std::vector<float> bufferAR;

    std::atomic<bool> IrReady{false};
};
