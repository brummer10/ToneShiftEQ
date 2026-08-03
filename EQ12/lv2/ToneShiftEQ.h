

/*
 * ToneShiftEQ.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>
#include <unistd.h>

#include <lv2/core/lv2.h>
#include <lv2/state/state.h>
#include <lv2/worker/worker.h>
#include <lv2/atom/atom.h>
#include <lv2/options/options.h>

#include <lv2/atom/util.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/patch/patch.h>
#include "inline-display.h"
#include <cairo/cairo.h>

#include "FFTAnalyzer.h"
#include "IRProcessorStereo.h"
#include "IRMorpherStereo.h"
#include "GainStereo.h"
#include "Engine.h"
#include "ParallelThread.h"
#include "Parameter.h"

///////////////////////// URID SUPPORT ////////////////////////////////

#define PLUGIN_URI "urn:brummer:toneshifteq"
#define TONESHIFTEQ_spectrum PLUGIN_URI "#spectrum"
#define TONESHIFTEQ_irdata PLUGIN_URI "#irdata"
#define TONESHIFTEQ_irphase PLUGIN_URI "#irphase"
#define TONESHIFTEQ_ir_request PLUGIN_URI "#ir_request"


typedef struct {
    LV2_URID atom_Object;
    LV2_URID atom_Float;
    LV2_URID atom_Double;
    LV2_URID atom_Vector;
    LV2_URID atom_URID;
    LV2_URID atom_eventTransfer;
    LV2_URID spectrum_data;
    LV2_URID ir_data;
    LV2_URID ir_phase;
    LV2_URID ir_request;
} URIs;

static inline void map_lv2_uris(LV2_URID_Map* map, URIs* uris) {
    uris->atom_Object             = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_Float              = map->map(map->handle, LV2_ATOM__Float);
    uris->atom_Double             = map->map(map->handle, LV2_ATOM__Double);
    uris->atom_Vector             = map->map(map->handle, LV2_ATOM__Vector);
    uris->atom_URID               = map->map(map->handle, LV2_ATOM__URID);
    uris->atom_eventTransfer      = map->map(map->handle, LV2_ATOM__eventTransfer);
    uris->spectrum_data           = map->map(map->handle, TONESHIFTEQ_spectrum);
    uris->ir_data                 = map->map(map->handle, TONESHIFTEQ_irdata);
    uris->ir_phase                = map->map(map->handle, TONESHIFTEQ_irphase);
    uris->ir_request              = map->map(map->handle, TONESHIFTEQ_ir_request);
}

////////////////////////////// PLUG-IN CLASS ///////////////////////////

namespace toneshifteq {

class Xtoneshifteq
{
private:
    Params*                 param;
    FFTAnalyzer             ana;
    IRProcessor             ip;
    IRMorpherStereo         conv;
    GainStereo              vu;
    Engine                  engine;
    
    const LV2_Atom_Sequence* control;
    LV2_Atom_Sequence* notify;
    LV2_URID_Map* map;

    LV2_Atom_Forge forge;
    LV2_Atom_Sequence* notify_port;
    LV2_Atom_Forge_Frame notify_frame;
    URIs uris;
    std::atomic<bool> pullPhase {false};
    float* par[109]; // engine.param.getParamCount() +1
    float* dyn[12]; // engine.param.getDynamics()
    float* input0;
    float* input1;
    float* output0;
    float* output1;
    float* latency;

    // private functions
    inline void run_dsp_(uint32_t n_samples);
    inline void connect_(uint32_t port,void* data);
    inline void init_dsp_(uint32_t rate);
    inline void connect_all__ports(uint32_t port, void* data);
    inline void activate_f();
    inline void clean_up();
    inline void deactivate_f();
    void analyse();
public:
    float sampleRate;
    std::atomic<bool> atomTransfer {true};
    // inline display
    float lowcut;
    float highcut;
    int hpslopes;
    int lpslopes;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t* data;
    cairo_surface_t* surface;
    LV2_Inline_Display_Image_Surface img;
    LV2_Inline_Display*  queue_draw = nullptr;
    void needTransfer(bool need) { atomTransfer.store(need, std::memory_order_release); }
    const bool irReady() { return engine.dataReady.load(std::memory_order_acquire); }
    const std::vector<float>& getIRMag() { return ip.getIRMag(); }
    const std::vector<float>& getPhase() { return ip.getIRPhase(); }
    const bool hasNewData() { return ana.hasNewData(); }
    const float* getMagnitudes() { return ana.getMagnitudes(); }
    const int getBins() { return ana.getBins(); }
    static const LV2_Inline_Display_Image_Surface* render_inline(
                LV2_Handle instance, uint32_t width, uint32_t height);

    // LV2 Descriptor
    static const LV2_Descriptor descriptor;
    // static wrapper to private functions
    static void deactivate(LV2_Handle instance);
    static void cleanup(LV2_Handle instance);
    static const void* extension_data(const char* uri);
    static void run(LV2_Handle instance, uint32_t n_samples);
    static void activate(LV2_Handle instance);
    static void connect_port(LV2_Handle instance, uint32_t port, void* data);
    static LV2_Handle instantiate(const LV2_Descriptor* descriptor,
                                double rate, const char* bundle_path,
                                const LV2_Feature* const* features);
    Xtoneshifteq();
    ~Xtoneshifteq();
};

} // end namespace toneshifteq
