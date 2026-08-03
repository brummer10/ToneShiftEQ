
/*
 * ToneShiftEQ.cpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#include "ToneShiftEQ.h"

namespace toneshifteq {

// constructor
Xtoneshifteq::Xtoneshifteq() :
    ana(),
    ip(),
    conv(),
    vu(),
    engine(&ip, &conv, &ana, &vu),
    input0(NULL),
    input1(NULL),
    output0(NULL),
    output1(NULL),
    latency(NULL) {};

// destructor
Xtoneshifteq::~Xtoneshifteq() {
    cairo_surface_destroy(surface);
};

///////////////////////// PRIVATE CLASS  FUNCTIONS /////////////////////

void Xtoneshifteq::init_dsp_(uint32_t rate) {
    width = 1;
    height = 1;
    stride = 1;
    data = nullptr;
    surface = nullptr;
    sampleRate = (float)rate;
    engine.init(rate, 20, 1);
    float v = 0.0f;
    for (int i = 0; i< 109; i++) {
        par[i] = &v;
    }
    for (int i = 0; i< 12; i++) {
        dyn[i] = &v;
    }
}

// connect the Ports used by the plug-in class
void Xtoneshifteq::connect_(uint32_t port,void* data) {
    for (int i = 0; i< 109; i++) {
        if (i == (int)port) {
            par[i] = static_cast<float*>(data);
            return;
        }
    }
    switch (port)
    {
        case 109:
            vu.meterLout = static_cast<float*>(data);
            break;
        case 110:
            vu.meterRout = static_cast<float*>(data);
            break;
        case 111:
            input0 = static_cast<float*>(data);
            break;
        case 112:
            input1 = static_cast<float*>(data);
            break;
        case 113:
            output0 = static_cast<float*>(data);
            break;
        case 114:
            output1 = static_cast<float*>(data);
            break;
        case 115:
            notify = (LV2_Atom_Sequence*)data;
            break;
        case 116:
            control = (const LV2_Atom_Sequence*)data;
            break;
        case 117:
            latency = static_cast<float*>(data);
            break;
        default:
            break;
    }
    for (int i = 118; i< 130; i++) {
        if (i == (int)port) {
            dyn[i-118] = static_cast<float*>(data);
            return;
        }
    }
}

void Xtoneshifteq::activate_f() {
    // allocate the internal DSP mem
}

void Xtoneshifteq::clean_up() {
    // delete the internal DSP mem
}

void Xtoneshifteq::deactivate_f() {
    // delete the internal DSP mem
}

// FIXME getDynamics(int index)
void Xtoneshifteq::run_dsp_(uint32_t n_samples) {
    if(n_samples<1) return;
    URIs* uris = &this->uris;
    const uint32_t notify_capacity = this->notify->atom.size;
    lv2_atom_forge_set_buffer(&forge, (uint8_t*)notify, notify_capacity);
    lv2_atom_forge_sequence_head(&forge, &notify_frame, 0);
    if (notify_capacity<n_samples) return;

    // do inplace processing on default
    if(output0 != input0)
        memcpy(output0, input0, n_samples*sizeof(float));
    if(output1 != input1)
        memcpy(output1, input1, n_samples*sizeof(float));

    // check for parameter changes
    for (int i = 0; i< engine.param.getParamCount(); i++) {
        if (engine.param.getParam(i) != (*par[i])) {
            if (i > 0 && i < 85) { // filter update
                engine.processIR.store(true, std::memory_order_release);
                engine.workToDo.store(true, std::memory_order_release);
            }
            engine.param.setParam((int)i, (*par[i]));
        }
    }
    // get request from UI to send the IR data
    LV2_ATOM_SEQUENCE_FOREACH(control, ev) {
        if (lv2_atom_forge_is_object_type(&forge, ev->body.type)) {
            const LV2_Atom_Object* obj = (LV2_Atom_Object*)&ev->body;
            if (obj->body.otype == uris->ir_request) {
                engine.processIR.store(true, std::memory_order_release);
                engine.workToDo.store(true, std::memory_order_release);
           }
        }
    }

    engine.process(n_samples, input0, input1, output0, output1);

    for (int i = 0; i< 12; i++) {
        (*dyn[i]) = engine.getDynamics(i);
    }

    if (!atomTransfer.load(std::memory_order_acquire)) return;

    static constexpr size_t atom_overhead = sizeof(LV2_Atom_Object) + sizeof(LV2_Atom_Property_Body)
                                          + sizeof(LV2_Atom_Vector_Body) + 64; 

    // send spectrum
    if (ana.hasNewData()) {
        size_t needed = atom_overhead + ana.getBins() * sizeof(float);
        if (forge.size - forge.offset >= needed) {
            LV2_Atom_Forge_Frame frame;
            lv2_atom_forge_frame_time(&this->forge, 0);
            lv2_atom_forge_object(&this->forge, &frame, 1, uris->spectrum_data);
            lv2_atom_forge_property_head(&this->forge, uris->atom_Vector, 0);
            lv2_atom_forge_vector(&this->forge, sizeof(float), uris->atom_Float,
                                  ana.getBins(), (void*)ana.getMagnitudes());
            lv2_atom_forge_pop(&this->forge, &frame);
            if (queue_draw) queue_draw->queue_draw(queue_draw->handle);
            ana.clearFlag();
        }
    }

    // send phase data
    if (pullPhase.load(std::memory_order_acquire)) {
        const std::vector<float>& phaseData = engine.ip->getIRPhase();
        size_t phaseSize = phaseData.size();
        size_t needed = atom_overhead + phaseSize * sizeof(float);
        if (phaseData.data() != nullptr && phaseSize > 0 && forge.size - forge.offset >= needed) {
            pullPhase.store(false, std::memory_order_release);
            LV2_Atom_Forge_Frame frame;
            lv2_atom_forge_frame_time(&this->forge, 0);
            lv2_atom_forge_object(&this->forge, &frame, 1, uris->ir_phase);
            lv2_atom_forge_property_head(&this->forge, uris->atom_Vector, 0);
            lv2_atom_forge_vector(&this->forge, sizeof(float), uris->atom_Float,
                                                    phaseSize, (void*)phaseData.data());
            lv2_atom_forge_pop(&this->forge, &frame);
        }
    }

    // send new IR data
    if (engine.dataReady.load(std::memory_order_acquire)) {
        size_t needed = atom_overhead + 4096 * sizeof(float);
        if (forge.size - forge.offset >= needed) {
            engine.dataReady.store(false, std::memory_order_release);
            LV2_Atom_Forge_Frame frame;
            lv2_atom_forge_frame_time(&this->forge, 0);
            lv2_atom_forge_object(&this->forge, &frame, 1, uris->ir_data);
            lv2_atom_forge_property_head(&this->forge, uris->atom_Vector, 0);
            lv2_atom_forge_vector(&this->forge, sizeof(float), uris->atom_Float,
                                  4096, (void*)engine.ip->getIRMag().data());
            lv2_atom_forge_pop(&this->forge, &frame);
            engine.processIR.store(false, std::memory_order_release);
            pullPhase.store(true, std::memory_order_release);
        }
    }

    *(latency) = (int)*(par[84]) ? 0 : engine.conv->getLatency();
}

void Xtoneshifteq::connect_all__ports(uint32_t port, void* data) {
    // connect the Ports used by the plug-in class
    connect_(port,data); 
}

////////////////////// STATIC CLASS  FUNCTIONS  ////////////////////////

#include "DrawInline.cc"

const LV2_Inline_Display_Image_Surface* Xtoneshifteq::render_inline(
            LV2_Handle instance, uint32_t width, uint32_t height) {

    Xtoneshifteq* self = (Xtoneshifteq*)instance;

    if (!self->data || self->width != width || self->height != height * 0.5 || !self->surface) {
        free(self->data);
        self->width  = width;
        self->height = height * 0.5;
        self->stride = width * 4;
        self->data = (uint8_t*)calloc(1, self->stride * self->height);
        cairo_surface_destroy(self->surface);
        self->surface = cairo_image_surface_create_for_data(
            self->data, CAIRO_FORMAT_ARGB32, self->width, self->height, self->stride);
    }

    cairo_t* cr = cairo_create(self->surface);
    draw_inline(self, cr);
    cairo_destroy(cr);

    self->img.data   = self->data;
    self->img.width  = self->width;
    self->img.height = self->height;
    self->img.stride = self->stride;

    return &self->img;
}

LV2_Handle 
Xtoneshifteq::instantiate(const LV2_Descriptor* descriptor,
                            double rate, const char* bundle_path,
                            const LV2_Feature* const* features) {
    // init the plug-in class
    Xtoneshifteq *self = new Xtoneshifteq();
    if (!self) {
        return NULL;
    }

    LV2_URID_Map* map = NULL;
    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            map = (LV2_URID_Map*)features[i]->data;
        }
        if (!strcmp(features[i]->URI, LV2_INLINEDISPLAY__queue_draw)) {
            self->queue_draw = (LV2_Inline_Display*) features[i]->data;
        }
    }
    if (!map) {
        delete self;
        return NULL;
    }

    map_lv2_uris(map, &self->uris);
    lv2_atom_forge_init(&self->forge, map);

    self->map = map;

    self->init_dsp_((uint32_t)rate);
    return (LV2_Handle)self;
}

void Xtoneshifteq::connect_port(LV2_Handle instance, 
                                   uint32_t port, void* data) {
    // connect all ports
    static_cast<Xtoneshifteq*>(instance)->connect_all__ports(port, data);
}

void Xtoneshifteq::activate(LV2_Handle instance) {
    // allocate needed mem
    static_cast<Xtoneshifteq*>(instance)->activate_f();
}

void Xtoneshifteq::run(LV2_Handle instance, uint32_t n_samples) {
    // run dsp
    static_cast<Xtoneshifteq*>(instance)->run_dsp_(n_samples);
}

void Xtoneshifteq::deactivate(LV2_Handle instance) {
    // free allocated mem
    static_cast<Xtoneshifteq*>(instance)->deactivate_f();
}

void Xtoneshifteq::cleanup(LV2_Handle instance) {
    // well, clean up after us
    Xtoneshifteq* self = static_cast<Xtoneshifteq*>(instance);
    self->clean_up();
    delete self;
}

const void* Xtoneshifteq::extension_data(const char* uri) {
    if (!strcmp(uri, LV2_INLINEDISPLAY__interface)) {
        static const LV2_Inline_Display_Interface iface = {
            render_inline
        };
        return &iface;
    }
    return NULL;
}

const LV2_Descriptor Xtoneshifteq::descriptor =
{
    PLUGIN_URI ,
    Xtoneshifteq::instantiate,
    Xtoneshifteq::connect_port,
    Xtoneshifteq::activate,
    Xtoneshifteq::run,
    Xtoneshifteq::deactivate,
    Xtoneshifteq::cleanup,
    Xtoneshifteq::extension_data
};

} // end namespace toneshifteq

////////////////////////// LV2 SYMBOL EXPORT ///////////////////////////

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index) {
    switch (index)
    {
        case 0:
            return &toneshifteq::Xtoneshifteq::descriptor;
        default:
            return NULL;
    }
}

///////////////////////////// FIN //////////////////////////////////////
