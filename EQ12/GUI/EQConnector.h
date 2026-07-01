
/*
 * EQConnector.h
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
#include <cstring>
#include <unistd.h>

#include "Engine.h"

#include "IConnector.h"

class StandaloneConnector : public IConnector {
public:

    StandaloneConnector(Engine *engine_) {
        engine = engine_;
    }
    
    ~StandaloneConnector() {}

    // send value changes from GUI to the engine/host
    void sendValueChanged(int index, float value) override {
        engine->param.setParam(index, value);
        engine->param.setParamDirty(index, true);
        engine->param.controllerChanged.store(true, std::memory_order_release);
        if (index > 0 && index < 82) { // filter update
            engine->processIR.store(true, std::memory_order_release);
            engine->workToDo.store(true, std::memory_order_release);
        } else if (index == 83) { // mode switch
            engine->switchMode.store(true, std::memory_order_release);
            engine->workToDo.store(true, std::memory_order_release);
        }
    }

    // get values from engine/host parameter when needed
    float getParameterValue(int index) override {
        return (float)engine->param.getParam(index);
    }

    void setParValue(int index, float value) {
        
    }

    // those needs to be done by atom ports in LV2
    float getMeterL() override {
        return engine->vu->getMeterL();
    }

    float getMeterR() override {
        return engine->vu->getMeterR();
    }

    bool checkNewData() override {
        return engine->ana->hasNewData();
    }

    int getBins() override {
        return engine->ana->getBins();
    }

    const float* getMagnitudes() override {
        return engine->ana->getMagnitudes();
    }

    void clearAna() override {
        engine->ana->clearFlag();
    }

    bool haveData() override {
        if (engine->dataReady.load(std::memory_order_acquire)) {
            engine->dataReady.store(false, std::memory_order_release);
            return true;
        }
        return false;
    }

    const std::vector<float>& getIR() override {
        return engine->ip->getIRMag();
    }

    const std::pair<std::vector<double>, std::vector<double> > get_ir() override {
        return engine->ip->createIRStereo();
    }

private:
    Engine *engine = nullptr;

};
