/*
 * engine.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>

#include "Band.h"
#include "Parameter.h"
#include "AudioFile.h"
#include "ParallelThread.h"
#include "Biquad.h"
#include "SVF.h"
#include "BandDetector.h"


class Engine : public FilterTypes {
public:
    Params                          param;
    ParallelThread                  xrworker;
    AudioFile                       af;
    IRProcessor*                    ip = nullptr;
    IRMorpherStereo*                conv = nullptr;
    FFTAnalyzer*                    ana = nullptr;
    GainStereo*                     vu = nullptr;
    SmoothCascade                   tsc;
    SmoothDynamicCascade            com;
    Detector                        tsd;
    SVFCascade                      svf;
    uint32_t                        s_rate = 48000;
    size_t                          irLength = 4096;          
    std::string                     revfile;
    std::string                     srcfile;
    std::atomic<bool>               execute {false};
    std::atomic<bool>               workToDo {false};
    std::atomic<bool>               processIR {false};
    std::atomic<bool>               waitForIR {false};
    std::atomic<bool>               dataReady {false};
    std::atomic<bool>               convLoadIR {false};

    inline Engine(IRProcessor *ip_, IRMorpherStereo* conv_, FFTAnalyzer* ana_, GainStereo* vu_);

    inline ~Engine();

    inline float getDynamics(int index);
    inline void init(uint32_t rate, int32_t rt_prio_, int32_t rt_policy_);
    inline void do_work_mono();
    inline void process(uint32_t nframes, const float* input, const float* input1, float* output, float* output1);

private:
    ParallelThread                  par;
    float*                          abuffer = nullptr;
    uint32_t                        frames = 0;
    int                             mode = 0;
    float                           dynamicThreshold[NumFilters];
    int                             dynamicRatio[NumFilters];

    enum class ModeState { Running, FadingOut, FadingIn };
    ModeState modeState = ModeState::Running;
    int pendingMode = 0;
    int currentMode = 0;
    int warmupBlocks = 1;
    float fadeGain = 1.0f;
    static constexpr float fadeStep = 0.25f;

    std::array<FilterConfig, NumFilters> baseFilterConfig {};
    std::array<bool, NumFilters> dynamicActive {};
    std::array<std::atomic<float>, NumFilters> dynGainOffset {};
    std::array<float, NumFilters> smoothedDynGainDB {};

    void registerParameters();
    void updateCascadeFromParams();
    inline void processBuffer();
    inline void processDynamic();
    inline void applyDynamicGains();
    inline void feedAnanlyzer(uint32_t nframes, float* output, float* output1);
};

inline Engine::Engine(IRProcessor *ip_, IRMorpherStereo* conv_, FFTAnalyzer* ana_, GainStereo* vu_) :
    xrworker(),
    par() {
        ip = ip_;
        conv = conv_;
        ana = ana_;
        vu = vu_;
        
        abuffer = new float[8192];
        memset(abuffer, 0, 8192 * sizeof(float));

        for (int i = 0; i< NumFilters; i++) {
            dynamicThreshold[i] = 0.0;
            dynamicRatio[i] = 1;
        }

        for (auto& g : dynGainOffset)
            g.store(0.0f, std::memory_order_relaxed);

        registerParameters();
        xrworker.start();
        par.start();
};

inline Engine::~Engine(){
    ana->cleanup();
    xrworker.stop();
    par.stop();
    delete[] abuffer;

};

void Engine::registerParameters() {
    //                  name           group    min, max, def, step     value                    isStepped  type
    param.registerParam("Enable",     "Global",  0,   1,   0,    1,     (void*)&conv->bypass,        true,  IS_INT);

    for (int i = 0; i < NumFilters; ++i) {
        std::string n = "Band " + std::to_string(i + 1);
        param.registerParam(n + " enable", "EQ", 0, 1, 1, 1, (void*)&ip->bands[i].enabled, true, IS_INT);
        param.registerParam(n + " type", "EQ", 0, 3, defs[i].type, 1, (void*)&ip->bands[i].type, true, IS_INT);
        param.registerParam(n + " mute", "EQ", 0, 1, 0, 1, (void*)&ip->bands[i].mute, true, IS_INT);
        param.registerParam(n + " freq", "EQ", defs[i].freqMin, defs[i].freqMax, defs[i].freqDef, 0.01, (void*)&ip->bands[i].freq, false, IS_DOUBLE);
        param.registerParam(n + " gain", "EQ", -48.0, 24.0, 0.0, 0.01, (void*)&ip->bands[i].gain, false, IS_DOUBLE);
        param.registerParam(n + " Q", "EQ", defs[i].qMin, 10.0, defs[i].qDef, 0.01, (void*)&ip->bands[i].Q, false, IS_DOUBLE);
    }
    //                  name           group    min, max, def, step     value                    isStepped  type
    param.registerParam("Solo Band",      "EQ",  0,  11,   0,    1,     (void*)&ip->solo_band,       true,  IS_INT);
    param.registerParam("Solo enabled",   "EQ",  0,   1,   0,    1,     (void*)&ip->solo_enabled,    true,  IS_INT);

    param.registerParam("Lowcut enable",  "EQ",  0,   1,   0,    1,     (void*)&ip->lowcut_enabled,  true,  IS_INT);
    param.registerParam("Lowcut freq",    "EQ", 19, 2200, 19, 0.01,     (void*)&ip->lowcut,         false,  IS_DOUBLE);
    param.registerParam("Highcut enable", "EQ",  0,   1,   0,    1,     (void*)&ip->highcut_enabled, true,  IS_INT);
    param.registerParam("Highcut freq",   "EQ", 110,22000,22000,0.01,   (void*)&ip->highcut,        false,  IS_DOUBLE);

    param.registerParam("Smooth",         "IR",  0,   1,  0.3, 0.01,    (void*)&ip->smooth_amount,  false,  IS_DOUBLE);
    param.registerParam("Contrast",       "IR", -1,   1,  0.0, 0.01,    (void*)&ip->dynamics_amount,false,  IS_DOUBLE);
    param.registerParam("Tone Bias",      "IR", -1,   1,  0.0, 0.01,    (void*)&ip->tilt_amount,    false,  IS_DOUBLE);
   
    param.registerParam("Volume Out", "Global",-46,  12,  0.0,  0.1,    (void*)&vu->gain,           false,  IS_FLOAT);

    param.registerParam("HF Fade",        "EQ",  0,   1,   0,    1,     (void*)&ip->hf_fade,         true,  IS_INT);
    param.registerParam("Mode",           "EQ",  0,   2,   0,    1,     (void*)&mode,                true,  IS_INT);

    for (int i = 0; i < NumFilters; ++i) {
        param.registerParam("Threshold" + std::to_string(i + 1), "Compressor",-46.0,0.0, 0.0,0.1, (void*)&dynamicThreshold[i],false,  IS_FLOAT);
    }

    for (int i = 0; i < NumFilters; ++i) {
        param.registerParam("Ratio" + std::to_string(i + 1), "Compressor",0, 4, 1,1, (void*)&dynamicRatio[i],true,  IS_INT);
    }
   

};


inline void Engine::init(uint32_t rate, int32_t rt_prio_, int32_t rt_policy_) {
    par.stop();
    xrworker.stop();
    s_rate = rate;

    ana->init(4096, (float)rate);
    vu->init(rate);
    ip->computeIR((double)rate);
    tsc.prepare((double)rate);
    com.prepare((double)rate);
    tsd.prepare((double)rate);
    svf.prepare((double)rate);
    updateCascadeFromParams();
    execute.store(false, std::memory_order_release);

    xrworker.start();
    par.start();

    xrworker.setThreadName("Worker");
    xrworker.set<Engine, &Engine::do_work_mono>(this);
    //xrworker.runProcess();

    par.setThreadName("RT");
    par.setPriority(rt_prio_, rt_policy_);
    par.set<Engine, &Engine::processBuffer>(this);
};


inline float Engine::getDynamics(int index) {
    return dynGainOffset[index];
}


void Engine::updateCascadeFromParams() {
    static const Type typeMap[] = {
        Type::LowShelf,
        Type::Peak,
        Type::HighShelf,
        Type::Notch
    };

    for (int i = 0; i < NumFilters; ++i) {
        FilterConfig cfg;
        Detector::DetectorConfig cfg_d;

        bool soloed = ip->solo_enabled && (i != ip->solo_band);
        bool inactive = soloed || !ip->bands[i].enabled || ip->bands[i].mute;

        if (inactive) {
            cfg.type      = Type::Peak;
            cfg.frequency = (float)ip->bands[i].freq;
            cfg.q         = 1.0f;
            cfg.gainDB    = 0.0f;

            cfg_d.frequency = (float)ip->bands[i].freq;
            cfg_d.q         = (float)std::clamp(ip->bands[i].Q, defs[i].qMin, 10.0);
            cfg_d.attackMs  = 0.01f;
            cfg_d.releaseMs = 0.001f;
        } else {
            cfg.type      = typeMap[ip->bands[i].type];
            cfg.frequency = (float)ip->bands[i].freq;
            cfg.q         = (float)std::clamp(ip->bands[i].Q, defs[i].qMin, 10.0);
            cfg.gainDB    = (float)ip->bands[i].gain;

            cfg_d.frequency = (float)ip->bands[i].freq;
            cfg_d.q         = (float)std::clamp(ip->bands[i].Q, defs[i].qMin, 10.0);
            cfg_d.attackMs  = 8.0f;
            cfg_d.releaseMs = 80.0f;
        }

        baseFilterConfig[i] = cfg;
        dynamicActive[i]    = !inactive;

        tsc.setFilter(i, cfg);
        svf.setFilter(i, cfg);
        tsd.setDetector(i, cfg_d);
    }
    tsc.setLowCut((float)ip->lowcut, ip->solo_enabled ? false : ip->lowcut_enabled);
    tsc.setHighCut((float)ip->highcut, ip->solo_enabled ? false : ip->highcut_enabled);
    svf.setLowCut((float)ip->lowcut, ip->solo_enabled ? false : ip->lowcut_enabled);
    svf.setHighCut((float)ip->highcut, ip->solo_enabled ? false : ip->highcut_enabled);
}

void Engine::do_work_mono() {
    execute.store(true, std::memory_order_release);

    if (processIR.load(std::memory_order_acquire)) {
        updateCascadeFromParams();
        ip->MODEL = mode;
        ip->computeIR(s_rate);
        processIR.store(false, std::memory_order_release);
        waitForIR.store(true, std::memory_order_release);
    }

    if (convLoadIR.load(std::memory_order_acquire)) {
        conv->setIR(ip->createIRStereo());
        convLoadIR.store(false, std::memory_order_release);
    }

    execute.store(false, std::memory_order_release);
}

inline void Engine::processDynamic() {
    float ratio     = 3.0f;   // 3:1

    for (int i = 0; i < NumFilters; ++i) {
        if (!dynamicActive[i]) {
            dynGainOffset[i].store(0.0f, std::memory_order_relaxed);
            continue;
        }
        if (dynamicThreshold[i] > -0.1f) continue;
        float levelDB = tsd.getDB(i);
        float gainReductionDB = 0.0f;
        switch (dynamicRatio[i]) {
            case 0: ratio = 2.0f;
            break;
            case 1: ratio = 3.0f;
            break;
            case 2: ratio = 4.0f;
            break;
            case 3: ratio = 5.0f;
            break;
            case 4: ratio = 10.0f;
            break;
            default : ratio = 3.0f;
            break;
        }

        if (levelDB > dynamicThreshold[i])
            gainReductionDB = (levelDB - dynamicThreshold[i]) * (1.0f - 1.0f / ratio);

        dynGainOffset[i].store(-gainReductionDB, std::memory_order_release);
    }
}

inline void Engine::applyDynamicGains() {
    for (int i = 0; i < NumFilters; ++i) {
        if (!dynamicActive[i]) continue;
        float dynGainDB = dynGainOffset[i].load(std::memory_order_acquire);
        FilterConfig cfg = baseFilterConfig[i];
        cfg.gainDB = std::clamp(dynGainDB, -48.0f, 24.0f);

        com.setFilter(i, cfg);
    }
}

inline void Engine::processBuffer() {
    if (!frames) return;
        ana->processBlock(abuffer, frames);
}


inline void Engine::feedAnanlyzer(uint32_t nframes, float* output, float* output1) {
    for (uint32_t i = 0; i < nframes; ++i) {
        const float l = std::fabs(output[i]);
        const float r = std::fabs(output1[i]);
        abuffer[i] = (l > r) ? output[i] : output1[i];
    }

    frames = nframes;
    par.runProcess();
}

inline void Engine::process(uint32_t nframes, const float* input,
                const float* input1, float* output, float* output1) {

    if(nframes<1) return;
    if (output != input)
        std::memcpy(output, input, nframes * sizeof(float));

    if (output1 != input1)
        std::memcpy(output1, input1, nframes * sizeof(float));

    if (modeState == ModeState::FadingOut) {
        fadeGain -= fadeStep;
        if (fadeGain <= 0.0f) {
            fadeGain = 0.0f;
            tsc.reset();
            svf.reset();
            com.reset();
            conv->reset();
            for (auto& g : smoothedDynGainDB)
                g = 0.0f;
            warmupBlocks = 1;
            currentMode = pendingMode;
            modeState = ModeState::FadingIn;
        }
    } else if (modeState == ModeState::FadingIn) {
        if (warmupBlocks > 0) {
            warmupBlocks--;
        } else {
            fadeGain += fadeStep;
            if (fadeGain >= 1.0f) {
                fadeGain = 1.0f;
                modeState = ModeState::Running;
            }
        }
    } else if (currentMode != mode) {
        pendingMode = mode;
        modeState = ModeState::FadingOut;
    }
    switch (currentMode) {
        case 0:
            conv->process(nframes, input, input1, output, output1);
            break;
        case 1:
            tsc.setBypass(conv->bypass);
            tsc.processBlock(nframes, output, output1);
            break;
        case 2:
            svf.setBypass(conv->bypass);
            svf.processBlock(nframes, output, output1);
            break;
    }

    tsd.processBlock(nframes, output, output1);
    processDynamic();
    applyDynamicGains();
    com.setBypass(conv->bypass);
    com.processBlock(nframes, output, output1);

    if (fadeGain < 1.0f) {
        for (uint32_t i = 0; i < nframes; ++i) {
            output[i]  *= fadeGain;
            output1[i] *= fadeGain;
        }
    }

    vu->process(nframes, output, output1, output, output1);

    feedAnanlyzer(nframes, output, output1);
    if(workToDo.load(std::memory_order_acquire) && !execute.load(std::memory_order_acquire)) {
        workToDo.store(false, std::memory_order_release);
        xrworker.runProcess();
    }
    if(waitForIR.load(std::memory_order_acquire) && ip->workerReady.load(std::memory_order_acquire)) {
        dataReady.store(true, std::memory_order_release);
        if (!execute.load(std::memory_order_acquire)) {
            waitForIR.store(false, std::memory_order_release);
            convLoadIR.store(true, std::memory_order_release);
            xrworker.runProcess();
        }
    }
}
