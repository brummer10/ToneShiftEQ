
/*
 * PluginDescriptor.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include "base.h"

struct PluginDescriptor {

    const char* vendor;
    const char* url;
    const char* email;

    const char* category;
    const char* subCategories;

    const char* masterName;
    const char* liveName;

    const char* version;
    const char* sdkVersion;

    v3_tuid masterUID;
    v3_tuid liveUID;

    int hiddenParameter;
};

inline constexpr PluginDescriptor descriptor = {

    .vendor        = "brummer10",
    .url           = "https://github.com/brummer10/ToneShiftEQ",
    .email         = "mailto:brummer-@web.de",

    .category      = "Audio Module Class",
    .subCategories = "Fx|EQ",

    .masterName    = "ToneShift-EQ12M",
    .liveName      = "ToneShift-EQ12L",

    .version       = "0.7.1",
    .sdkVersion    = "VST 3.7.9",

    .masterUID      = V3_ID(0xf56bf8c5, 0xf60047ce, 0xa5a7dd43, 0x1717129a),
    .liveUID        = V3_ID(0xc1de8249, 0x99ae4365, 0x85a775bc, 0x91ea8d7e),

    .hiddenParameter = 84,
};
