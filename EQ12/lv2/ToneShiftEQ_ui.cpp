
/*
 * ToneShiftEQ_ui.cpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


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

#include <cmath>
#include <iostream>

#include "LV2Connector.h"

#define LV2PLUG  // hide quit button
#include "SpectrumViewer.h"


#define PLUGIN_URI     "urn:brummer:toneshifteq"
#define PLUGIN_UI_URI  "urn:brummer:toneshifteq_ui"

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


class XToneShiftEQ_UI {
private:

    LV2UI_Write_Function write_function;
    LV2UI_Controller controller;

    LV2_URID_Map* map;
    LV2UI_Resize* resize;
    LV2_Atom_Forge forge;
    URIs uris;

    void* parentXwindow;
    LV2Connector conn;

    void copyValuesToGui(Widget_t* wid, float value);

    void getEngineValues(uint32_t port, float value);

    void sendParameter(uint32_t port, float value);

    void handleAtom(const LV2_Atom_Object* obj);

public:
    SpectrumViewer sw;
    bool check;

    static LV2UI_Handle instantiate(const LV2UI_Descriptor* descriptor,
            const char* plugin_uri, const char* bundle_path,
            LV2UI_Write_Function write_function, LV2UI_Controller controller,
            LV2UI_Widget* widget, const LV2_Feature* const* features);

    static void cleanup(LV2UI_Handle ui);

    static void port_event(LV2UI_Handle ui, uint32_t port, uint32_t buffer_size,
                                            uint32_t format, const void* buffer);

    static void notify_dsp(XToneShiftEQ_UI* self);

    static int idle(LV2UI_Handle ui);

    static const void* extension_data(const char* uri);

    XToneShiftEQ_UI();
    ~XToneShiftEQ_UI();
};


XToneShiftEQ_UI::XToneShiftEQ_UI()
    : write_function(nullptr),
      controller(nullptr),
      map(nullptr),
      parentXwindow(nullptr),
      conn(&write_function, &controller),
      sw(&conn) {}

XToneShiftEQ_UI::~XToneShiftEQ_UI() {}

void XToneShiftEQ_UI::sendParameter(uint32_t port, float value) {
    write_function(controller, port, sizeof(float), 0, &value);
}

void XToneShiftEQ_UI::handleAtom(const LV2_Atom_Object* obj) {
    if (obj->body.otype == uris.spectrum_data) {
        const LV2_Atom* vector_data = NULL;
        const int n_props  = lv2_atom_object_get(obj,uris.atom_Vector, &vector_data, NULL);
        if (!n_props) return;
        const LV2_Atom_Vector* vec = (LV2_Atom_Vector*)LV2_ATOM_BODY(vector_data);
        if (vec->atom.type == uris.atom_Float) {
            const float* data =  (float*) LV2_ATOM_BODY(&vec->atom);
            uint32_t bins = (vector_data->size - sizeof(LV2_Atom_Vector_Body)) / vec->atom.size;
            sw.setSpec(data, bins);
        }
    } else if (obj->body.otype == uris.ir_data) {
        const LV2_Atom* vector_data = NULL;
        const int n_props  = lv2_atom_object_get(obj,uris.atom_Vector, &vector_data, NULL);
        if (!n_props) return;
        const LV2_Atom_Vector* vec = (LV2_Atom_Vector*)LV2_ATOM_BODY(vector_data);
        if (vec->atom.type == uris.atom_Float) {
            const float* data =  (float*) LV2_ATOM_BODY(&vec->atom);
            uint32_t bins = (vector_data->size - sizeof(LV2_Atom_Vector_Body)) / vec->atom.size;
            sw.setFilter(data, bins);
        }
    } else if (obj->body.otype == uris.ir_phase) {
        const LV2_Atom* vector_data = NULL;
        const int n_props  = lv2_atom_object_get(obj,uris.atom_Vector, &vector_data, NULL);
        if (!n_props) return;
        const LV2_Atom_Vector* vec = (LV2_Atom_Vector*)LV2_ATOM_BODY(vector_data);
        if (vec->atom.type == uris.atom_Float) {
            const float* data =  (float*) LV2_ATOM_BODY(&vec->atom);
            uint32_t bins = (vector_data->size - sizeof(LV2_Atom_Vector_Body)) / vec->atom.size;
            sw.setPhase(data, bins);
        }
    }
}

static void null_callba(void*,void*) {}

void XToneShiftEQ_UI::copyValuesToGui(Widget_t* wid, float value) {
    xevfunc store = wid->func.value_changed_callback;
    wid->func.value_changed_callback = null_callba;
    adj_set_value(wid->adj, value);
    wid->func.value_changed_callback = store;
}

void XToneShiftEQ_UI::getEngineValues(uint32_t port, float value) {
    switch (port) {

    case 0:
        copyValuesToGui(sw.bp, value);
        break;
    case 74:
        if (value)
            copyValuesToGui(sw.solo[(int)conn.getParameterValue(73)], 1.0f);
        break;
    case 76:
        copyValuesToGui(sw.lowcut, value);
        break;
    case 78:
        copyValuesToGui(sw.highcut, value);
        break;
    case 79:
        copyValuesToGui(sw.smooth, value);
        break;
    case 80:
        copyValuesToGui(sw.dynamics, value);
        break;
    case 81:
        copyValuesToGui(sw.tilt, value);
        break;
    case 82:
        copyValuesToGui(sw.vug, value);
        break;
    case 83:
        copyValuesToGui(sw.hf_fade, value);
        break;
    case 84:
        copyValuesToGui(sw.mode, value);
        break;
    default:
        break;
    }

    // Filter-Parameter 1..72
    if (port >= 1 && port <= 72) {
        const int filter = (port - 1) / 6;
        const int param  = (port - 1) % 6;

        switch (param) {

        case 0:
            copyValuesToGui(sw.fenable[filter], value);
            break;
        case 1:
            copyValuesToGui(sw.ftype[filter], value);
            break;
        case 2:
            copyValuesToGui(sw.mute[filter], value);
            break;
        case 3:
            copyValuesToGui(sw.freq[filter], value);
            break;
        case 4:
            copyValuesToGui(sw.fgain[filter], value);
            break;
        case 5:
            copyValuesToGui(sw.fq[filter], value);
            break;
        }
    }
}

void XToneShiftEQ_UI::port_event(LV2UI_Handle handle, uint32_t port, uint32_t buffer_size,
                                                    uint32_t format, const void* buffer) {
    XToneShiftEQ_UI* self = static_cast<XToneShiftEQ_UI*>(handle);

    if (format == 0) {
        const float value = *(const float*)buffer;
        if (port == 85) {
            adj_set_value(self->sw.vumeterL->adj, power2db(self->sw.vumeterL, value));
        } else if (port == 86) {
            adj_set_value(self->sw.vumeterR->adj, power2db(self->sw.vumeterR, value));
        } else {
            self->conn.setParValue(port, value);
            self->getEngineValues(port, value);
        }
        return;
    } else if (format == self->uris.atom_eventTransfer) {
        const LV2_Atom* atom = (const LV2_Atom*)buffer;

        if (atom->type == self->uris.atom_Object) {
            self->handleAtom((const LV2_Atom_Object*)atom);
        }
    }
}

LV2UI_Handle XToneShiftEQ_UI::instantiate(const LV2UI_Descriptor* descriptor,
        const char* plugin_uri, const char* bundle_path,
        LV2UI_Write_Function write_function, LV2UI_Controller controller,
        LV2UI_Widget* widget, const LV2_Feature* const* features) {

    if (strcmp(plugin_uri, PLUGIN_URI))
        return nullptr;

    XToneShiftEQ_UI* self = new XToneShiftEQ_UI();

    self->write_function = write_function;
    self->controller = controller;
    self->resize = nullptr;
    self->check = true;
    LV2_Options_Option *opts = NULL;

    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_UI__parent)) {
            self->parentXwindow = features[i]->data;
        } else if (!strcmp(features[i]->URI, LV2_UI__resize)) {
            self->resize = (LV2UI_Resize*)features[i]->data;
        } else if(!strcmp(features[i]->URI, LV2_OPTIONS__options)) {
            opts = (LV2_Options_Option*)features[i]->data;
        } else if (!strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = (LV2_URID_Map*)features[i]->data;
        }
    }

    if (!self->map) {
        delete self;
        return nullptr;
    }
    if (self->parentXwindow == NULL)  {
        fprintf(stderr, "ERROR: Failed to open parentXwindow for %s\n", plugin_uri);
        delete self;
        return nullptr;
    }
    if (opts != NULL && self->map != NULL) {
        const LV2_URID atom_Float = self->map->map(self->map->handle, LV2_ATOM__Float);
        const LV2_URID ui_sampleRate = self->map->map(self->map->handle, LV2_PARAMETERS__sampleRate);
        for (const LV2_Options_Option* o = opts; o->key; ++o) {
            if (o->context == LV2_OPTIONS_INSTANCE &&
              o->key == ui_sampleRate && o->type == atom_Float) {
                float sr = *(float*)o->value;
                self->sw.setSampleRate((double)sr);
                //fprintf(stderr, "SampleRate = %iHz\n",(int)*(float*)o->value);
            }
        }
    }

    map_lv2_uris(self->map, &self->uris);
    lv2_atom_forge_init(&self->forge, self->map);

    main_init(self->sw.getMain());

    #if defined(_WIN32)
    self->sw.top  = create_window(self->sw.getMain(), (HWND) self->parentXwindow, 0, 0, 880, 430);
    self->sw.top->func.expose_callback = self->sw.draw_window;
    #else
    self->sw.top  = create_window(self->sw.getMain(), (Window) self->parentXwindow, 0, 0, 880, 430);
    self->sw.top->func.expose_callback = self->sw.draw_window;
    #endif
    self->sw.create();
    widget_show_all(self->sw.top);

    *widget = (LV2UI_Widget)self->sw.top->widget;

    if (self->resize){
        self->resize->ui_resize(self->resize->handle, 880, 430);
    }

    return (LV2UI_Handle)self;
}

// notify the engine that the UI needs the IR data
void XToneShiftEQ_UI::notify_dsp(XToneShiftEQ_UI* self) {
    uint8_t obj_buf[1024];
    lv2_atom_forge_set_buffer(&self->forge, obj_buf, 1024);
    LV2_Atom_Forge_Frame frame;
    LV2_Atom* msg = (LV2_Atom*)lv2_atom_forge_object(&self->forge, &frame, 0, self->uris.ir_request);

    self->write_function(self->controller, 92, lv2_atom_total_size(msg),
                       self->uris.atom_eventTransfer, msg);
}

// LV2 idle interface to host
int XToneShiftEQ_UI::idle(LV2UI_Handle handle) {
    XToneShiftEQ_UI* self = static_cast<XToneShiftEQ_UI*>(handle);
    if (self->check) {
        self->notify_dsp(self);
        self->check = false;
    }
    run_embedded(self->sw.getMain());
    return 0;
}

// LV2 resize interface to host
static int ui_resize(LV2UI_Feature_Handle handle, int w, int h) {
    XToneShiftEQ_UI* self = static_cast<XToneShiftEQ_UI*>(handle);
    if (self) send_configure_event(self->sw.top,0, 0, w, h);
    return 0;
}

// connect idle and resize functions to host
const void* XToneShiftEQ_UI::extension_data(const char* uri) {
    static const LV2UI_Idle_Interface idle = { XToneShiftEQ_UI::idle };
    static const LV2UI_Resize resize = { 0 ,ui_resize };
    if (!strcmp(uri, LV2_UI__idleInterface)) {
        return &idle;
    }
    if (!strcmp(uri, LV2_UI__resize)) {
        return &resize;
    }
    return NULL;
}

// cleanup after usage
void XToneShiftEQ_UI::cleanup(LV2UI_Handle handle) {
    XToneShiftEQ_UI* self = static_cast<XToneShiftEQ_UI*>(handle);
    self->sw.quitGui();
    main_quit(self->sw.getMain());
    free(self);
}

static const LV2UI_Descriptor ui_descriptor =
{
    PLUGIN_UI_URI,
    XToneShiftEQ_UI::instantiate,
    XToneShiftEQ_UI::cleanup,
    XToneShiftEQ_UI::port_event,
    XToneShiftEQ_UI::extension_data
};

LV2_SYMBOL_EXPORT
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    switch(index) {
        case 0:
            return &ui_descriptor;

        default:
            return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////

