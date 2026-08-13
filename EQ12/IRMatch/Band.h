
/*
 * Band.h
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

struct Band {
    enum Type {
        LowShelf  = 0,
        Peak      = 1,
        HighShelf = 2,
        Notch     = 3
    };

    int enabled;
    Type type;
    double freq;
    double gain;
    double Q;
    double threshold;
    int ratio;
    int expander;
    int mute;
};

struct BandDef {
    Band::Type type;
    double freqDef;
    double freqMin;
    double freqMax;
    double qDef;
    double qMin;
};

static constexpr BandDef defs[12] = {
    {Band::LowShelf,  40.0,    20.0,    60.0,   0.7, 0.4},
    {Band::Peak,      70.0,    40.0,   100.0,   1.0, 0.5},
    {Band::Peak,     120.0,    70.0,   180.0,   1.0, 0.5},
    {Band::Peak,     210.0,   120.0,   300.0,   1.1, 0.5},
    {Band::Peak,     370.0,   200.0,   550.0,   1.2, 0.6},
    {Band::Peak,     650.0,   350.0,   900.0,   1.3, 0.6},
    {Band::Peak,    1150.0,   650.0,  1600.0,   1.4, 0.7},
    {Band::Peak,    2000.0,  1100.0,  2800.0,   1.5, 0.8},
    {Band::Peak,    3500.0,  1800.0,  5000.0,   1.6, 0.8},
    {Band::Peak,    6100.0,  3500.0,  9000.0,   1.5, 0.8},
    {Band::Peak,   10700.0,  6000.0, 15000.0,   1.3, 0.7},
    {Band::HighShelf,18000.0,10000.0,20000.0,   0.7, 0.4}
};
