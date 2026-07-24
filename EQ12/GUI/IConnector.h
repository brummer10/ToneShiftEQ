

/*
 * IConnector.h
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


class IConnector {
public:
    virtual ~IConnector() {}

    virtual void sendValueChanged(int index, float value) = 0;
    virtual float getParameterValue(int index) = 0;
    virtual void setParValue(int index, float value) = 0;

    virtual float getMeterL() = 0;
    virtual float getMeterR() = 0;

    virtual float getDynamics(int index) = 0;

    virtual bool checkNewData() = 0;
    virtual int getBins() = 0;
    virtual const float* getMagnitudes() = 0;
    virtual void clearAna() = 0;

    virtual bool haveData() = 0;
    virtual const std::vector<double>& getRef(std::vector<double>& l, std::vector<double>& r, double s) = 0;
    virtual const std::vector<float>& getIR() = 0;
    virtual const std::vector<float>& getPhase() = 0;
    virtual const std::pair<std::vector<double>, std::vector<double> > get_ir() = 0;
};
