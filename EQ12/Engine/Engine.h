/*
 * engine.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#pragma once

#include <vector>
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


class Engine
{
public:
    Params                       param;
    ParallelThread               xrworker;
    AudioFile                    af;
    IRProcessor*                 ip = nullptr;
    IRMorpherStereo*             conv = nullptr;
    FFTAnalyzer*                 ana = nullptr;
    GainStereo*                  vu = nullptr;
    uint32_t                     s_rate = 48000;
    size_t                       irLength = 4096;          
    std::string                  revfile;
    std::string                  srcfile;
    std::atomic<bool>            execute {false};
    std::atomic<bool>            workToDo {false};
    std::atomic<bool>            processIR {false};
    std::atomic<bool>            waitForIR {false};
    std::atomic<bool>            dataReady {false};
    std::atomic<bool>            convLoadIR {false};
    std::atomic<bool>            switchMode {false};

    inline Engine(IRProcessor *ip_, IRMorpherStereo* conv_, FFTAnalyzer* ana_, GainStereo* vu_);

    inline ~Engine();

    inline void init(uint32_t rate, int32_t rt_prio_, int32_t rt_policy_);
    inline void do_work_mono();
    inline void process(uint32_t nframes, const float* input, const float* input1, float* output, float* output1);

private:
    ParallelThread               par;
    float*                       abuffer = nullptr;
    uint32_t                     frames = 0;
    int                          mode = 0;

    void registerParameters();

    inline void processBuffer();
    inline void feedAnanlyzer(uint32_t nframes, float* output, float* output1);
};

inline Engine::Engine(IRProcessor *ip_, IRMorpherStereo* conv_, FFTAnalyzer* ana_, GainStereo* vu_) :
    xrworker(),
    par() {
        ip = ip_;
        conv = conv_;
        ana = ana_;
        vu = vu_;
        registerParameters();
        
        abuffer = new float[8192];
        memset(abuffer, 0, 8192 * sizeof(float));

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
    //                  name           group    min, max, def, step     value                               isStepped  type
    param.registerParam("Enable",     "Global",  0,   1,   0,    1,     (void*)&conv->bypass,        true,  IS_INT);

    for (int i = 0; i < 12; ++i) {
        std::string n = "Band " + std::to_string(i + 1);
        param.registerParam(n + " enable", "EQ", 0, 1, 1, 1, (void*)&ip->bands[i].enabled, true, IS_INT);
        param.registerParam(n + " type", "EQ", 0, 2, defs[i].type, 1, (void*)&ip->bands[i].type, true, IS_INT);
        param.registerParam(n + " mute", "EQ", 0, 1, 0, 1, (void*)&ip->bands[i].mute, true, IS_INT);
        param.registerParam(n + " freq", "EQ", defs[i].freqMin, defs[i].freqMax, defs[i].freqDef, 0.01, (void*)&ip->bands[i].freq, false, IS_DOUBLE);
        param.registerParam(n + " gain", "EQ", -48.0, 24.0, 0.0, 0.01, (void*)&ip->bands[i].gain, false, IS_DOUBLE);
        param.registerParam(n + " Q", "EQ", defs[i].qMin, 10.0, defs[i].qDef, 0.01, (void*)&ip->bands[i].Q, false, IS_DOUBLE);
    }
    //                  name           group    min, max, def, step     value                               isStepped  type
    param.registerParam("Solo Band",      "EQ",  0,  11,   0,    1,     (void*)&ip->solo_band,       true,  IS_INT);
    param.registerParam("Solo enabled",   "EQ",  0,   1,   0,    1,     (void*)&ip->solo_enabled,    true,  IS_INT);

    param.registerParam("Lowcut enable",  "EQ",  0,   1,   0,    1,     (void*)&ip->lowcut_enabled,  true,  IS_INT);
    param.registerParam("Lowcut freq",    "EQ", 19, 2200, 19, 0.01,     (void*)&ip->lowcut,         false,  IS_DOUBLE);
    param.registerParam("Highcut enable", "EQ",  0,   1,   0,    1,     (void*)&ip->highcut_enabled, true,  IS_INT);
    param.registerParam("Highcut freq",   "EQ", 110,22000,22000,0.01,   (void*)&ip->highcut,        false,  IS_DOUBLE);

    param.registerParam("Smooth",         "IR",  0,   1,  0.3, 0.01,    (void*)&ip->smooth_amount,  false,  IS_DOUBLE);
    param.registerParam("Dynamics",       "IR", -1,   1,  0.0, 0.01,    (void*)&ip->dynamics_amount,false,  IS_DOUBLE);
    param.registerParam("Tone Bias",      "IR", -1,   1,  0.0, 0.01,    (void*)&ip->tilt_amount,    false,  IS_DOUBLE);
   
    param.registerParam("Volume Out", "Global",-46,  12,  0.0,  0.1,    (void*)&vu->gain,           false,  IS_FLOAT);

    param.registerParam("Mode",           "EQ",  0,   1,   0,    1,     (void*)&mode,                true,  IS_INT);
};


inline void Engine::init(uint32_t rate, int32_t rt_prio_, int32_t rt_policy_) {
    s_rate = rate;

    ana->init(4096, (float)rate);
    vu->init(rate);
    ip->computeIR(rate);
    execute.store(false, std::memory_order_release);

    xrworker.setThreadName("Worker");
    xrworker.set<Engine, &Engine::do_work_mono>(this);
    //xrworker.runProcess();

    par.setThreadName("RT");
    par.setPriority(rt_prio_, rt_policy_);
    par.set<Engine, &Engine::processBuffer>(this);
};

void Engine::do_work_mono() {
    execute.store(true, std::memory_order_release);

    if (processIR.load(std::memory_order_acquire)) {
        ip->computeIR(s_rate);
        processIR.store(false, std::memory_order_release);
        waitForIR.store(true, std::memory_order_release);
    }

    if (convLoadIR.load(std::memory_order_acquire)) {
        conv->setIR(ip->createIRStereo());
        convLoadIR.store(false, std::memory_order_release);
    }

    if (switchMode.load(std::memory_order_acquire)) {
        conv->setMode(mode, ip->createIRStereo());
        switchMode.store(false, std::memory_order_release);
    }

    execute.store(false, std::memory_order_release);
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
    conv->process(nframes, input, input1, output, output1);
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
