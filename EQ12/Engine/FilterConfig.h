
/*
 * FilterConfig.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <cstdint>

class FilterTypes {
public:
    enum class Type {
        Peak,
        LowShelf,
        HighShelf,
        LowPass,
        HighPass,
        BandPass,
        Notch
    };

    struct FilterConfig {
        Type type;
        float frequency;
        float q;
        float gainDB;
    };

    static constexpr int NumFilters = 12;
};
