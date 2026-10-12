
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

    const char* version;
    const char* sdkVersion;

    v3_tuid masterUID;

};

inline constexpr PluginDescriptor descriptor = {

    .vendor        = "brummer10",
    .url           = "https://github.com/brummer10/ToneShiftEQ",
    .email         = "mailto:brummer-@web.de",

    .category      = "Audio Module Class",
    .subCategories = "Fx|EQ",

    .masterName    = "ToneShift-EQ12",

    .version       = "2.0.0",
    .sdkVersion    = "VST 3.7.9",

    .masterUID      = V3_ID(0x9b78b7fd, 0x3ca04b5e, 0x8fda77b6, 0xb0406177),

};
