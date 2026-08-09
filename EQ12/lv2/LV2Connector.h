
/*
 * EQConnector.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>
#include <lv2/state/state.h>
#include <lv2/worker/worker.h>
#include <lv2/atom/atom.h>
#include <lv2/options/options.h>

#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/patch/patch.h>
#include <lv2/parameters/parameters.h>

#include <vector>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <unistd.h>


#include "IConnector.h"

class LV2Connector : public IConnector {
public:
    LV2UI_Write_Function* write_function;
    LV2UI_Controller* controller;
    float dyn[12]; // engine.param.getDynamics()

    LV2Connector(LV2UI_Write_Function* write_function_, LV2UI_Controller* controller_) {
        write_function = write_function_;
        controller = controller_;
        for (int i = 0; i< 115; i++) {
            par[i] = 0.0f;
        }
        for (int i = 0; i< 12; i++) {
            dyn[i] = 0.0f;
        }
    }
    
    ~LV2Connector() {}

    // send value changes from GUI to the engine/host
    void sendValueChanged(int index, float value) override {
        if (index < 115) {
            par[index] = value;
        }
        (*write_function)(*controller, index, sizeof(float), 0, &value);
    }

    // get values from engine/host parameter when needed
    float getParameterValue(int index) override {
        //std::cout << "get " << index << " " << par[index] << std::endl;
        return par[index];
    }

    void setParValue(int index, float value) {
        par[index] = value;
    }

    float getDynamics(int index) {
        return dyn[index];
    }

    // those needs to be done by atom ports in LV2
    float getMeterL() override {
        return 0;
    }

    float getMeterR() override {
        return 0;
    }

    // those needs to be done by atom ports in LV2
    float getInMeterL() override {
        return 0;
    }

    float getInMeterR() override {
        return 0;
    }

    bool checkNewData() override {
        return 0;
    }

    int getBins() override {
        return 0;
    }

    const float* getMagnitudes() override {
        return nullptr;
    }

    void clearAna() override {

    }

    bool checkNewInData() override {
        return 0;
    }

    int getInBins() override {
        return 0;
    }

    const float* getInMagnitudes() override {
        return nullptr;
    }

    void clearInAna() override {

    }

    bool haveData() override {
        return false;
    }

    const std::vector<float>& getIR() override {
        return dummy; //suppress warning 
    }

    const std::vector<double>& getRef(std::vector<double>& l, std::vector<double>& r, double s) override {
        return dummy3; //suppress warning 
    }

    const std::vector<float>& getPhase() override {
        return dummy; //suppress warning 
    }

    const std::pair<std::vector<double>, std::vector<double> > get_ir() override {
        return dummy2; //suppress warning 
    }

private:
    float par[115]; // engine.param.getParamCount()
    std::vector<float> dummy;
    std::vector<double> dummy3;
    std::pair<std::vector<double>, std::vector<double> > dummy2;
};
