
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
        if (index > 0 && index < 85) { // filter update
            engine->processIR.store(true, std::memory_order_release);
            engine->workToDo.store(true, std::memory_order_release);
        }
    }

    // get values from engine/host parameter when needed
    float getParameterValue(int index) override {
        return (float)engine->param.getParam(index);
    }

    void setParValue(int index, float value) {
        
    }

    float getDynamics(int index) {
        return engine->getDynamics(index);
    }

    // those needs to be done by atom ports in LV2
    float getMeterL() override {
        return engine->vu->getMeterL();
    }

    float getMeterR() override {
        return engine->vu->getMeterR();
    }

    // those needs to be done by atom ports in LV2
    float getInMeterL() override {
        return engine->vuin->getMeterL();
    }

    float getInMeterR() override {
        return engine->vuin->getMeterR();
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

    bool checkNewInData() override {
        return engine->anain->hasNewData();
    }

    int getInBins() override {
        return engine->anain->getBins();
    }

    const float* getInMagnitudes() override {
        return engine->anain->getMagnitudes();
    }

    void clearInAna() override {
        engine->anain->clearFlag();
    }

    bool haveData() override {
        if (engine->dataReady.load(std::memory_order_acquire)) {
            engine->dataReady.store(false, std::memory_order_release);
            return true;
        }
        return false;
    }

    const std::vector<double>& getRef(std::vector<double>& l, std::vector<double>& r, double s) override {
        return engine->ip->getIR(l, r, s);
    }

    const std::vector<float>& getIR() override {
        return engine->ip->getIRMag();
    }

    const std::vector<float>& getPhase() override {
        return engine->ip->getIRPhase();
    }

    const std::pair<std::vector<double>, std::vector<double> > get_ir() override {
        return engine->ip->createIRStereo();
    }

private:
    Engine *engine = nullptr;

};
