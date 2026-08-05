
/*
 * ToneShiftEQVST3.cpp
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#include "../clap/ToneShiftEQ.cc"
#include "PluginDescriptor.h"

#include "travesty/factory.h"
#include "travesty/view.h"
#include "travesty/component.h"
#include "travesty/audio_processor.h"
#include "travesty/edit_controller.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#if defined(_WIN32)
# define PLUGIN_EXPORT __declspec(dllexport)
#else
# define PLUGIN_EXPORT __attribute__ ((visibility ("default")))
#endif


enum PluginMode { MODE_MASTER = 0, MODE_LIVE = 1 };

// ---------------------------------------------------------------------------
// helper
// ---------------------------------------------------------------------------

static void asciiToUtf16(int16_t* dst, const char* src, int32_t dstSizeInChars) {
    int32_t i = 0;
    for (; src[i] != '\0' && i < dstSizeInChars - 1; ++i)
        dst[i] = static_cast<int16_t>(static_cast<unsigned char>(src[i]));
    dst[i] = 0;
}

static void utf16ToAscii(char* dst, const int16_t* src, int32_t dstSize) {
    int32_t i = 0;
    for (; src[i] != 0 && i < dstSize - 1; ++i)
        dst[i] = static_cast<char>(src[i]);
    dst[i] = '\0';
}

static double normalisedToPlain(const Parameter& p, double normalised) {
    return p.min + normalised * (p.max - p.min);
}

static double plainToNormalised(const Parameter& p, double plain) {
    const double range = p.max - p.min;
    return range != 0.0 ? (plain - p.min) / range : 0.0;
}

// ---------------------------------------------------------------------------
// v3_audio_processor
// ---------------------------------------------------------------------------

struct toneshifteq_processor : v3_audio_processor_cpp {
    std::atomic_int refcounter{1};
    ToneShiftEQ* const plugin;
    const PluginMode mode;

    toneshifteq_processor(ToneShiftEQ* p, PluginMode m) : plugin(p), mode(m) {
        query_interface = query_interface_processor;
        ref             = ref_processor;
        unref           = unref_processor;

        proc.set_bus_arrangements    = set_bus_arrangements;
        proc.get_bus_arrangement     = get_bus_arrangement;
        proc.can_process_sample_size = can_process_sample_size;
        proc.get_latency_samples     = get_latency_samples;
        proc.setup_processing        = setup_processing;
        proc.set_processing          = set_processing;
        proc.process                 = process;
        proc.get_tail_samples        = get_tail_samples;
    }

    static v3_result V3_API query_interface_processor(void* self, const v3_tuid iid, void** iface) {
        toneshifteq_processor* const p = *static_cast<toneshifteq_processor**>(self);
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_audio_processor_iid)) {
            ++p->refcounter;
            *iface = self;
            return V3_OK;
        }
        *iface = nullptr;

        return V3_NO_INTERFACE;
    }

    static uint32_t V3_API ref_processor(void* self) {
        return ++(*static_cast<toneshifteq_processor**>(self))->refcounter;
    }

    static uint32_t V3_API unref_processor(void* self) {
        return --(*static_cast<toneshifteq_processor**>(self))->refcounter;
    }

    static v3_result V3_API set_bus_arrangements(void*, v3_speaker_arrangement* inputs, int32_t numIn,
                                                        v3_speaker_arrangement* outputs, int32_t numOut) {
        if (numIn != 1 || numOut != 1) return V3_NOT_IMPLEMENTED;
        if (inputs[0] != (V3_SPEAKER_L | V3_SPEAKER_R) || outputs[0] != (V3_SPEAKER_L | V3_SPEAKER_R))
            return V3_NOT_IMPLEMENTED;

        return V3_OK;
    }

    static v3_result V3_API get_bus_arrangement(void*, int32_t, int32_t idx, v3_speaker_arrangement* arr) {
        if (idx != 0) return V3_INVALID_ARG;
        *arr = V3_SPEAKER_L | V3_SPEAKER_R;

        return V3_OK;
    }

    static v3_result V3_API can_process_sample_size(void*, int32_t symbolicSampleSize) {

        return symbolicSampleSize == V3_SAMPLE_32 ? V3_OK : V3_NOT_IMPLEMENTED;
    }

    static uint32_t V3_API get_latency_samples(void* self) {
        toneshifteq_processor* const p = *static_cast<toneshifteq_processor**>(self);

        return p->mode == MODE_LIVE ? 0 : p->plugin->getEngine()->conv->getLatency();
    }

    static v3_result V3_API setup_processing(void* self, v3_process_setup* setup) {
        toneshifteq_processor* const p = *static_cast<toneshifteq_processor**>(self);
        p->plugin->initEngine(static_cast<uint32_t>(setup->sample_rate), 25, 1);
        p->plugin->param->setParam( descriptor.hiddenParameter, static_cast<double>(p->mode));

        return V3_OK;
    }

    static v3_result V3_API set_processing(void*, v3_bool) { return V3_OK; }

    static v3_result V3_API process(void* self, v3_process_data* data) {
        toneshifteq_processor* const p = *static_cast<toneshifteq_processor**>(self);
        Params* const params = p->plugin->param;
        if (data->input_params != nullptr) {
            v3_param_changes* const changes = v3_cpp_obj(data->input_params);
            const int32_t count = changes->get_param_count(data->input_params);
            for (int32_t i = 0; i < count; ++i) {
                v3_param_value_queue** const queue = changes->get_param_data(data->input_params, i);
                if (queue == nullptr) continue;
                v3_param_value_queue* const q = v3_cpp_obj(queue);
                const v3_param_id id = q->get_param_id(queue);
                if ((int)id >= params->getParamCount()) continue;

                const int32_t points = q->get_point_count(queue);
                if (points <= 0) continue;
                int32_t sampleOffset = 0;
                double normalised = 0.0;
                q->get_point(queue, points - 1, &sampleOffset, &normalised);
                params->setParam(id, normalisedToPlain(params->getParameter(id), normalised));

                if (id > 0 && id < 83) {
                    p->plugin->getEngine()->processIR.store(true, std::memory_order_release);
                    p->plugin->getEngine()->workToDo.store(true, std::memory_order_release);
                }
            }
        }

        if (data->output_params != nullptr && params->controllerChanged.load(std::memory_order_acquire)) {
            v3_param_changes* const outChanges = v3_cpp_obj(data->output_params);
            for (int i = 0; i < params->getParamCount(); ++i) {
                if (!params->isParamDirty(i)) continue;
                v3_param_id id = static_cast<v3_param_id>(i);
                int32_t queueIdx = 0;
                v3_param_value_queue** const queue = outChanges->add_param_data(data->output_params, &id, &queueIdx);
                if (queue != nullptr) {
                    int32_t pointIdx = 0;
                    v3_cpp_obj(queue)->add_point(queue, 0, plainToNormalised(params->getParameter(i), params->getParam(i)), &pointIdx);
                }
                params->setParamDirty(i, false);
            }
            params->controllerChanged.store(false, std::memory_order_release);
        }

        if (data->nframes <= 0 || data->num_input_buses < 1 || data->num_output_buses < 1)
            return V3_OK;

        if (data->inputs[0].num_channels < 2 || data->outputs[0].num_channels < 2) {
            const int32_t inCh = data->inputs[0].num_channels;
            const int32_t outCh = data->outputs[0].num_channels;
            const int32_t n = inCh < outCh ? inCh : outCh;
            for (int32_t ch = 0; ch < n; ++ch) {
                float* const src = data->inputs[0].channel_buffers_32[ch];
                float* const dst = data->outputs[0].channel_buffers_32[ch];
                if (src != dst) std::memcpy(dst, src, data->nframes * sizeof(float));
            }

            return V3_OK;
        }

        float* const* in = data->inputs[0].channel_buffers_32;
        float* const* out = data->outputs[0].channel_buffers_32;
        float* const* sidechain = data->inputs[1].channel_buffers_32;
        if (out[0] != in[0]) std::memcpy(out[0], in[0], data->nframes * sizeof(float));
        if (out[1] != in[1]) std::memcpy(out[1], in[1], data->nframes * sizeof(float));
        p->plugin->process(static_cast<uint32_t>(data->nframes), sidechain[0], sidechain[1], out[0], out[1]);

        return V3_OK;
    }

    static uint32_t V3_API get_tail_samples(void*) { return 0; }
};

// ---------------------------------------------------------------------------
// v3_plugin_view
// ---------------------------------------------------------------------------

struct toneshifteq_view : v3_plugin_view_cpp {
    static constexpr int kDefaultWidth = 880;
    static constexpr int kDefaultHeight = 430;

    std::atomic_int refcounter{1};
    ToneShiftEQ* const plugin;
    bool guiCreated = false;

    explicit toneshifteq_view(ToneShiftEQ* p) : plugin(p) {
        query_interface = query_interface_view;
        ref             = ref_view;
        unref           = unref_view;

        view.is_platform_type_supported = is_platform_type_supported;
        view.attached                   = attached;
        view.removed                    = removed;
        view.on_wheel                   = on_wheel;
        view.on_key_down                = on_key_down;
        view.on_key_up                  = on_key_up;
        view.get_size                   = get_size;
        view.on_size                    = on_size;
        view.on_focus                   = on_focus;
        view.set_frame                  = set_frame;
        view.can_resize                 = can_resize;
        view.check_size_constraint      = check_size_constraint;
    }

    static v3_result V3_API query_interface_view(void* self, const v3_tuid iid, void** iface) {
        toneshifteq_view* const v = *static_cast<toneshifteq_view**>(self);
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_plugin_view_iid)) {
            ++v->refcounter;
            *iface = self;
            return V3_OK;
        }
        *iface = nullptr;

        return V3_NO_INTERFACE;
    }

    static uint32_t V3_API ref_view(void* self) {

        return ++(*static_cast<toneshifteq_view**>(self))->refcounter;
    }

    static uint32_t V3_API unref_view(void* self) {

        return --(*static_cast<toneshifteq_view**>(self))->refcounter;
    }

    static v3_result V3_API is_platform_type_supported(void*, const char* platformType) {

        return std::strcmp(platformType, V3_VIEW_PLATFORM_TYPE_NATIVE) == 0 ? V3_OK : V3_NOT_IMPLEMENTED;
    }

    static v3_result V3_API attached(void* self, void* parent, const char* platformType) {
        toneshifteq_view* const v = *static_cast<toneshifteq_view**>(self);
        if (std::strcmp(platformType, V3_VIEW_PLATFORM_TYPE_NATIVE) != 0)
            return V3_NOT_IMPLEMENTED;

        const Window parentWindow = reinterpret_cast<Window>(parent);
        if (!v->guiCreated) {
            v->plugin->startGui(parentWindow);
            v->guiCreated = true;
        }
        v->plugin->setParent(parentWindow);
        v->plugin->showGui();

        return V3_OK;
    }

    static v3_result V3_API removed(void* self) {
        toneshifteq_view* const v = *static_cast<toneshifteq_view**>(self);
        if (v->guiCreated) {
            v->plugin->quitGui();
            v->guiCreated = false;
        }

        return V3_OK;
    }

    static v3_result V3_API on_wheel(void*, float) { return V3_NOT_IMPLEMENTED; }
    static v3_result V3_API on_key_down(void*, int16_t, int16_t, int16_t) { return V3_NOT_IMPLEMENTED; }
    static v3_result V3_API on_key_up(void*, int16_t, int16_t, int16_t) { return V3_NOT_IMPLEMENTED; }

    static v3_result V3_API get_size(void* self, v3_view_rect* rect) {
        toneshifteq_view* const v = *static_cast<toneshifteq_view**>(self);
        int w = kDefaultWidth, h = kDefaultHeight;
        if (v->guiCreated && v->plugin->sw.top != nullptr) {
            w = static_cast<int>(v->plugin->sw.top->width);
            h = static_cast<int>(v->plugin->sw.top->height);
        }
        rect->left = 0; rect->top = 0; rect->right = w; rect->bottom = h;
        return V3_OK;
    }

    static v3_result V3_API on_size(void* self, v3_view_rect* rect) {
        toneshifteq_view* const v = *static_cast<toneshifteq_view**>(self);
        if (v->guiCreated && v->plugin->sw.top != nullptr)
            os_resize_window(v->plugin->getMain()->dpy, v->plugin->sw.top,
                              rect->right - rect->left, rect->bottom - rect->top);
        return V3_OK;
    }

    static v3_result V3_API on_focus(void*, v3_bool) { return V3_OK; }
    static v3_result V3_API set_frame(void*, v3_plugin_frame**) { return V3_OK; }
    static v3_result V3_API can_resize(void*) { return V3_TRUE; }
    static v3_result V3_API check_size_constraint(void*, v3_view_rect*) { return V3_OK; }

};

// ---------------------------------------------------------------------------
// v3_edit_controller
// ---------------------------------------------------------------------------

struct toneshifteq_controller : v3_edit_controller_cpp {
    std::atomic_int refcounter{1};
    ToneShiftEQ* const plugin;
    const PluginMode mode;
    toneshifteq_view* viewPtr = nullptr;

    explicit toneshifteq_controller(ToneShiftEQ* p, PluginMode m) : plugin(p), mode(m) {
        query_interface = query_interface_controller;
        ref             = ref_controller;
        unref           = unref_controller;

        base.initialize = initialize;
        base.terminate  = terminate;

        ctrl.set_component_state           = set_component_state;
        ctrl.set_state                      = set_state;
        ctrl.get_state                      = get_state;
        ctrl.get_parameter_count            = get_parameter_count;
        ctrl.get_parameter_info             = get_parameter_info;
        ctrl.get_parameter_string_for_value = get_parameter_string_for_value;
        ctrl.get_parameter_value_for_string = get_parameter_value_for_string;
        ctrl.normalised_parameter_to_plain  = normalised_parameter_to_plain;
        ctrl.plain_parameter_to_normalised  = plain_parameter_to_normalised;
        ctrl.get_parameter_normalised       = get_parameter_normalised;
        ctrl.set_parameter_normalised       = set_parameter_normalised;
        ctrl.set_component_handler          = set_component_handler;
        ctrl.create_view                    = create_view;
    }

    ~toneshifteq_controller() { delete viewPtr; }

    static v3_result V3_API query_interface_controller(void* self, const v3_tuid iid, void** iface) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_plugin_base_iid) ||
                                                v3_tuid_match(iid, v3_edit_controller_iid)) {
            ++c->refcounter;
            *iface = self;

            return V3_OK;
        }
        *iface = nullptr;

        return V3_NO_INTERFACE;
    }

    static uint32_t V3_API ref_controller(void* self) {
        return ++(*static_cast<toneshifteq_controller**>(self))->refcounter;
    }

    static uint32_t V3_API unref_controller(void* self) {
        return --(*static_cast<toneshifteq_controller**>(self))->refcounter;
    }

    static v3_result V3_API initialize(void*, v3_funknown**) { return V3_OK; }
    static v3_result V3_API terminate(void*) { return V3_OK; }

    static v3_result V3_API set_component_state(void* self, v3_bstream** stream) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if (stream == nullptr) return V3_INVALID_ARG;
        v3_bstream* const s = v3_cpp_obj(stream);
        char buf[4096];
        int32_t total = 0, n = 0;
        do {
            s->read(stream, buf + total, static_cast<int32_t>(sizeof(buf)) - 1 - total, &n);
            total += n;
        } while (n > 0 && total < static_cast<int32_t>(sizeof(buf)) - 1);

        buf[total] = '\0';
        if (total > 0) {
            c->plugin->readState(std::string(buf, total));
            c->plugin->getEngine()->processIR.store(true, std::memory_order_release);
            c->plugin->getEngine()->workToDo.store(true, std::memory_order_release);
        }

        return V3_OK;
    }

    static v3_result V3_API set_state(void* self, v3_bstream** stream) { return set_component_state(self, stream); }

    static v3_result V3_API get_state(void* self, v3_bstream** stream) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if (stream == nullptr) return V3_INVALID_ARG;
        v3_bstream* const s = v3_cpp_obj(stream);
        std::string state;
        c->plugin->saveState(&state);
        int32_t written = 0;
        s->write(stream, state.data(), static_cast<int32_t>(state.size()), &written);

        return V3_OK;
    }

    static int32_t V3_API get_parameter_count(void* self) {

        return (*static_cast<toneshifteq_controller**>(self))->plugin->param->getParamCount();
    }

    static v3_result V3_API get_parameter_info(void* self, int32_t idx, v3_param_info* info) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if (idx < 0 || idx >= c->plugin->param->getParamCount()) return V3_INVALID_ARG;
        const Parameter& def = c->plugin->param->getParameter(idx);

        std::memset(info, 0, sizeof(*info));
        info->param_id = static_cast<v3_param_id>(idx);
        asciiToUtf16(info->title, def.name.c_str(), 128);
        asciiToUtf16(info->short_title, def.name.c_str(), 128);
        info->step_count = def.isStepped ? static_cast<int32_t>(def.max - def.min) : 0;
        info->default_normalised_value = plainToNormalised(def, def.def);
        info->unit_id = 0;

        uint32_t flags = V3_PARAM_CAN_AUTOMATE;

        if (idx ==  descriptor.hiddenParameter) flags |= V3_PARAM_IS_HIDDEN;
        if (c->mode == MODE_LIVE) {
            if (idx == 79 || idx == 80 || idx == 81 || idx == 83) {
                flags |= V3_PARAM_IS_HIDDEN;
            }
        }
        info->flags = flags;

        return V3_OK;
    }

    static v3_result V3_API get_parameter_string_for_value(void* self, v3_param_id id, double normalised, v3_str_128 output) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if ((int)id >= c->plugin->param->getParamCount()) return V3_INVALID_ARG;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", normalisedToPlain(c->plugin->param->getParameter(id), normalised));
        asciiToUtf16(output, buf, 128);

        return V3_OK;
    }

    static v3_result V3_API get_parameter_value_for_string(void* self, v3_param_id id, int16_t* input, double* output) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if ((int)id >= c->plugin->param->getParamCount()) return V3_INVALID_ARG;
        char buf[32];
        utf16ToAscii(buf, input, sizeof(buf));
        *output = plainToNormalised(c->plugin->param->getParameter(id), std::atof(buf));

        return V3_OK;
    }

    static double V3_API normalised_parameter_to_plain(void* self, v3_param_id id, double normalised) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if ((int)id >= c->plugin->param->getParamCount()) return 0.0;

        return normalisedToPlain(c->plugin->param->getParameter(id), normalised);
    }

    static double V3_API plain_parameter_to_normalised(void* self, v3_param_id id, double plain) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if ((int)id >= c->plugin->param->getParamCount()) return 0.0;

        return plainToNormalised(c->plugin->param->getParameter(id), plain);
    }

    static double V3_API get_parameter_normalised(void* self, v3_param_id id) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if ((int)id >= c->plugin->param->getParamCount()) return 0.0;

        return plainToNormalised(c->plugin->param->getParameter(id), c->plugin->param->getParam(id));
    }

    static v3_result V3_API set_parameter_normalised(void* self, v3_param_id id, double normalised) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if ((int)id >= c->plugin->param->getParamCount()) return V3_INVALID_ARG;
        c->plugin->param->setParam(id, normalisedToPlain(c->plugin->param->getParameter(id), normalised));

        return V3_OK;
    }

    static v3_result V3_API set_component_handler(void*, v3_component_handler**) { return V3_OK; }

    static v3_plugin_view** V3_API create_view(void* self, const char*) {
        toneshifteq_controller* const c = *static_cast<toneshifteq_controller**>(self);
        if (c->viewPtr == nullptr)
            c->viewPtr = new toneshifteq_view(c->plugin);
        else
            ++c->viewPtr->refcounter;

        return reinterpret_cast<v3_plugin_view**>(&c->viewPtr);
    }
};

// ---------------------------------------------------------------------------
// v3_component
// ---------------------------------------------------------------------------

struct toneshifteq_component : v3_component_cpp {
    std::atomic_int refcounter{1};
    const PluginMode mode;
    ToneShiftEQ plugin;
    toneshifteq_processor* processorPtr = nullptr;
    toneshifteq_controller* controllerPtr = nullptr;

    explicit toneshifteq_component(PluginMode m) : mode(m) {
        query_interface = query_interface_component;
        ref             = ref_component;
        unref           = unref_component;

        base.initialize = initialize;
        base.terminate  = terminate;

        comp.get_controller_class_id = get_controller_class_id;
        comp.set_io_mode             = set_io_mode;
        comp.get_bus_count           = get_bus_count;
        comp.get_bus_info            = get_bus_info;
        comp.get_routing_info        = get_routing_info;
        comp.activate_bus            = activate_bus;
        comp.set_active              = set_active;
        comp.set_state                = component_set_state;
        comp.get_state                = component_get_state;
    }

    ~toneshifteq_component() {
        delete processorPtr;
        delete controllerPtr;
    }

    static v3_result V3_API query_interface_component(void* self, const v3_tuid iid, void** iface) {
        toneshifteq_component* const c = *static_cast<toneshifteq_component**>(self);

        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_plugin_base_iid) ||
                                                    v3_tuid_match(iid, v3_component_iid)) {
            ++c->refcounter;
            *iface = self;

            return V3_OK;
        }

        if (v3_tuid_match(iid, v3_audio_processor_iid)) {
            if (c->processorPtr == nullptr)
                c->processorPtr = new toneshifteq_processor(&c->plugin, c->mode);
            else
                ++c->processorPtr->refcounter;
            *iface = &c->processorPtr;

            return V3_OK;
        }

        if (v3_tuid_match(iid, v3_edit_controller_iid)) {
            if (c->controllerPtr == nullptr)
                c->controllerPtr = new toneshifteq_controller(&c->plugin, c->mode);
            else
                ++c->controllerPtr->refcounter;
            *iface = &c->controllerPtr;

            return V3_OK;
        }
        *iface = nullptr;

        return V3_NO_INTERFACE;
    }

    static uint32_t V3_API ref_component(void* self) {
        return ++(*static_cast<toneshifteq_component**>(self))->refcounter;
    }

    static uint32_t V3_API unref_component(void* self) {
        toneshifteq_component** const box = static_cast<toneshifteq_component**>(self);
        toneshifteq_component* const c = *box;
        const int rc = --c->refcounter;
        if (rc == 0) { delete c; delete box; return 0; }

        return rc;
    }

    static v3_result V3_API initialize(void*, v3_funknown**) { return V3_OK; }
    static v3_result V3_API terminate(void*) { return V3_OK; }
    static v3_result V3_API get_controller_class_id(void*, v3_tuid) { return V3_NOT_IMPLEMENTED; }
    static v3_result V3_API set_io_mode(void*, int32_t) { return V3_OK; }

    static int32_t V3_API get_bus_count(void*, int32_t mediaType, int32_t direction) { 
        if (mediaType == V3_AUDIO) {
            if (direction == V3_INPUT) return 2;
            return 1;
        } else return 0;
    }

    static v3_result V3_API get_bus_info(void*, int32_t mediaType, int32_t direction, int32_t idx, v3_bus_info* info) {
        if (mediaType != V3_AUDIO) return V3_INVALID_ARG;
        if (direction == V3_INPUT) {
            if (idx == 0) {
                info->media_type = V3_AUDIO;
                info->direction = V3_INPUT;
                info->channel_count = 2;
                asciiToUtf16(info->bus_name, "Audio Input", 128);
                info->bus_type = V3_MAIN;
                info->flags = V3_DEFAULT_ACTIVE;

                return V3_OK;
            }
            if (idx == 1) {
                info->media_type = V3_AUDIO;
                info->direction = V3_INPUT;
                info->channel_count = 2;
                asciiToUtf16(info->bus_name, "Sidechain Input", 128);
                info->bus_type = V3_AUX;
                info->flags = 0;

                return V3_OK;
            }
        } else if (direction == V3_OUTPUT) {
            std::memset(info, 0, sizeof(*info));
            info->media_type = V3_AUDIO;
            info->direction = V3_OUTPUT;
            info->channel_count = 2;
            asciiToUtf16(info->bus_name, "Audio Output", 128);
            info->bus_type = V3_MAIN;
            info->flags = V3_DEFAULT_ACTIVE;

            return V3_OK;
        }
        return V3_INVALID_ARG;
    }

    static v3_result V3_API get_routing_info(void*, v3_routing_info*, v3_routing_info*) { return V3_NOT_IMPLEMENTED; }
    static v3_result V3_API activate_bus(void*, int32_t, int32_t, int32_t, v3_bool) { return V3_OK; }
    static v3_result V3_API set_active(void*, v3_bool) { return V3_OK; }

    static v3_result V3_API component_set_state(void* self, v3_bstream** stream) {
        toneshifteq_component* const c = *static_cast<toneshifteq_component**>(self);
        if (stream == nullptr) return V3_INVALID_ARG;
        v3_bstream* const s = v3_cpp_obj(stream);
        char buf[4096];
        int32_t total = 0, n = 0;
        do {
            s->read(stream, buf + total, static_cast<int32_t>(sizeof(buf)) - 1 - total, &n);
            total += n;
        } while (n > 0 && total < static_cast<int32_t>(sizeof(buf)) - 1);

        buf[total] = '\0';
        if (total > 0) {
            c->plugin.readState(std::string(buf, total));
            c->plugin.getEngine()->processIR.store(true, std::memory_order_release);
            c->plugin.getEngine()->workToDo.store(true, std::memory_order_release);
        }

        return V3_OK;
    }

    static v3_result V3_API component_get_state(void* self, v3_bstream** stream) {
        toneshifteq_component* const c = *static_cast<toneshifteq_component**>(self);
        if (stream == nullptr) return V3_INVALID_ARG;
        v3_bstream* const s = v3_cpp_obj(stream);
        std::string state;
        c->plugin.saveState(&state);
        int32_t written = 0;
        s->write(stream, state.data(), static_cast<int32_t>(state.size()), &written);

        return V3_OK;
    }
};

// ---------------------------------------------------------------------------
// v3_plugin_factory
// ---------------------------------------------------------------------------

struct toneshifteq_factory : v3_plugin_factory_cpp {
    toneshifteq_factory() {
        query_interface = query_interface_factory;
        ref             = ref_factory;
        unref           = unref_factory;

        v1.get_factory_info = get_factory_info;
        v1.num_classes      = num_classes;
        v1.get_class_info   = get_class_info;
        v1.create_instance  = create_instance;

        v2.get_class_info_2 = get_class_info_2;

        v3.get_class_info_utf16 = get_class_info_utf16;
        v3.set_host_context     = set_host_context;
    }

    static v3_result V3_API query_interface_factory(void* self, const v3_tuid iid, void** iface) {
        if (v3_tuid_match(iid, v3_funknown_iid) || v3_tuid_match(iid, v3_plugin_factory_iid) ||
            v3_tuid_match(iid, v3_plugin_factory_2_iid) || v3_tuid_match(iid, v3_plugin_factory_3_iid)) {
            *iface = self;

            return V3_OK;
        }
        *iface = nullptr;

        return V3_NO_INTERFACE;
    }

    static uint32_t V3_API ref_factory(void*) { return 1; }
    static uint32_t V3_API unref_factory(void*) { return 0; }

    static v3_result V3_API get_factory_info(void*, v3_factory_info* info) {
        std::memset(info, 0, sizeof(*info));
        std::snprintf(info->vendor, sizeof(info->vendor), "%s", descriptor.vendor);
        std::snprintf(info->url, sizeof(info->url), "%s", descriptor.url);
        std::snprintf(info->email, sizeof(info->email), "%s", descriptor.email);
        info->flags = 0x10;

        return V3_OK;
    }

    static int32_t V3_API num_classes(void*) { return 2; }

    static v3_result V3_API get_class_info(void*, int32_t idx, v3_class_info* info) {
        if (idx < 0 || idx > 1) return V3_INVALID_ARG;
        std::memset(info, 0, sizeof(*info));
        std::memcpy(info->class_id, idx == 0 ? descriptor.masterUID :  descriptor.liveUID, sizeof(v3_tuid));
        info->cardinality = 0x7FFFFFFF;
        std::snprintf(info->category, sizeof(info->category), "%s", descriptor.category);
        std::snprintf(info->name, sizeof(info->name), "%s", idx == 0 ? descriptor.masterName : descriptor.liveName);

        return V3_OK;
    }

    static v3_result V3_API get_class_info_2(void*, int32_t idx, v3_class_info_2* info) {
        if (idx < 0 || idx > 1) return V3_INVALID_ARG;
        std::memset(info, 0, sizeof(*info));
        std::memcpy(info->class_id, idx == 0 ? descriptor.masterUID :  descriptor.liveUID, sizeof(v3_tuid));
        info->cardinality = 0x7FFFFFFF;
        std::snprintf(info->category, sizeof(info->category), "%s", descriptor.category);
        std::snprintf(info->name, sizeof(info->name), "%s", idx == 0 ? descriptor.masterName : descriptor.liveName);
        info->class_flags = 0;
        std::snprintf(info->sub_categories, sizeof(info->sub_categories), "%s", descriptor.subCategories);
        std::snprintf(info->vendor, sizeof(info->vendor), "%s", descriptor.vendor);
        std::snprintf(info->version, sizeof(info->version), "%s", descriptor.version);
        std::snprintf(info->sdk_version, sizeof(info->sdk_version), "%s", descriptor.sdkVersion);

        return V3_OK;
    }

    static v3_result V3_API get_class_info_utf16(void*, int32_t idx, v3_class_info_3* info) {
        if (idx < 0 || idx > 1) return V3_INVALID_ARG;
        std::memset(info, 0, sizeof(*info));
        std::memcpy(info->class_id, idx == 0 ? descriptor.masterUID :  descriptor.liveUID, sizeof(v3_tuid));
        info->cardinality = 0x7FFFFFFF;
        std::snprintf(info->category, sizeof(info->category), "%s", descriptor.category);
        asciiToUtf16(info->name, idx == 0 ? descriptor.masterName : descriptor.liveName, 64);
        info->class_flags = 0;
        std::snprintf(info->sub_categories, sizeof(info->sub_categories), "%s", descriptor.subCategories);
        asciiToUtf16(info->vendor, descriptor.vendor, 64);
        asciiToUtf16(info->version, descriptor.version, 64);
        asciiToUtf16(info->sdk_version, descriptor.sdkVersion, 64);

        return V3_OK;
    }

    static v3_result V3_API set_host_context(void*, v3_funknown**) { return V3_OK; }

    static v3_result V3_API create_instance(void*, const v3_tuid class_id, const v3_tuid iid, void** instance) {
        PluginMode mode;
        if (v3_tuid_match(class_id, descriptor.masterUID)) mode = MODE_MASTER;
        else if (v3_tuid_match(class_id,  descriptor.liveUID)) mode = MODE_LIVE;
        else return V3_NO_INTERFACE;

        if (!v3_tuid_match(iid, v3_component_iid) && !v3_tuid_match(iid, v3_funknown_iid))
            return V3_NO_INTERFACE;

        toneshifteq_component** const box = new (std::nothrow) toneshifteq_component*;
        if (box == nullptr) return V3_NOMEM;
        *box = new toneshifteq_component(mode);
        *instance = static_cast<void*>(box);

        return V3_OK;
    }
};

static toneshifteq_factory* gFactory = nullptr;

extern "C" {

#if defined(__linux__)
PLUGIN_EXPORT bool ModuleEntry(void*) { if (gFactory == nullptr) gFactory = new toneshifteq_factory(); return true; }
PLUGIN_EXPORT bool ModuleExit() { delete gFactory; gFactory = nullptr; return true; }
#elif defined(__APPLE__)
PLUGIN_EXPORT bool bundleEntry(void*) { if (gFactory == nullptr) gFactory = new toneshifteq_factory(); return true; }
PLUGIN_EXPORT bool bundleExit() { delete gFactory; gFactory = nullptr; return true; }
#elif defined(_WIN32)
PLUGIN_EXPORT bool InitDll() { if (gFactory == nullptr) gFactory = new toneshifteq_factory(); return true; }
PLUGIN_EXPORT bool ExitDll() { delete gFactory; gFactory = nullptr; return true; }
#endif

PLUGIN_EXPORT void* GetPluginFactory() {
    if (gFactory == nullptr) gFactory = new toneshifteq_factory();
    static v3_funknown* factoryPtr;
    factoryPtr = gFactory;

    return &factoryPtr;
}

} // extern "C"
