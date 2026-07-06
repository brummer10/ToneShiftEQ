
/*
 * SpectrumViewer.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <atomic>
#include <utility>

#include "xwidgets.h"
#include "widgets.cc"
#include "AudioFile.h"
#include "EQController.h"
#include "IConnector.h"

class SpectrumViewer {
public:
    using Vec = std::vector<float>;
    bool run = false;
    int active_panel = 0;
    Widget_t* top = nullptr;
    Widget_t* bp = nullptr;
    Widget_t* frame[12];
    Widget_t* ftype[12];
    Widget_t* fenable[12];
    Widget_t* freq[12];
    Widget_t* fq[12];
    Widget_t* fgain[12];
    Widget_t* solo[12];
    Widget_t* mute[12];
    Widget_t* prev[12];
    Widget_t* next[12];

    Widget_t* lowcut = nullptr;
    Widget_t* highcut = nullptr;

    Widget_t* mode = nullptr;

    Widget_t* smooth = nullptr;
    Widget_t* dynamics = nullptr;
    Widget_t* tilt = nullptr;
    Widget_t* vug = nullptr;
    Widget_t* vumeterL = nullptr;
    Widget_t* vumeterR = nullptr;
    std::atomic<bool> havePreset {false};

    SpectrumViewer(IConnector *conn_) {
        conn = conn_;
    }

    ~SpectrumViewer() {
        if(eq_layer) cairo_surface_destroy(eq_layer);
    }

    void setData(const std::vector<float>& ir) {
        ir_.assign(ir.begin(), ir.end());
    }

    void setPhase(const std::vector<float>& phase) {
        phase_.assign(phase.begin(), phase.end());
    }

    void setSpec(const float* data, int bin) {
        mag_.clear();
        for (int i = 0; i<bin; i++) {
            mag_.push_back(data[i]);
            expose_widget(spec);
        }
    }

    void setFilter(const float* data, int bin) {
        ir_.clear();
        for (int i = 0; i<bin; i++) {
            ir_.push_back(data[i]);
        }
        rebuild_eq_layer = true;
        expose_widget(spec);
    }

    void setPhase(const float* data, int bin) {
        phase_.clear();
        for (int i = 0; i<bin; i++) {
            phase_.push_back(data[i]);
        }
    }

    void init(int width = 880, int height = 430) {
        main_init(&main);
        top = create_window(&main, os_get_root_window(&main, IS_WINDOW), 0, 0, width, height);
        widget_set_title(top, "ToneShift-EQ12");
        widget_set_icon_from_png(top,LDVAR(toneshifteq_png));
        //top->flags = NO_PROPAGATE;
        top->func.expose_callback = draw_window;
    }

    void create(int width = 880, int height = 430) {
        spec_width  = 0;
        spec_height = 0;

        spec = create_widget(&main, top,0, 0, width-80, height);
        #ifndef _WIN32
        XSelectInput(spec->app->dpy, spec->widget,StructureNotifyMask|ExposureMask|KeyPressMask 
                    |EnterWindowMask|LeaveWindowMask|ButtonReleaseMask|KeyReleaseMask
                    |ButtonPressMask|Button1MotionMask|PointerMotionMask);
        #endif

        spec->parent_struct = this;
        spec->flags |= NO_PROPAGATE;
        spec->scale.gravity = NORTHWEST;
        spec->func.expose_callback = draw_callback;
        spec->func.key_press_callback = get_key;
        spec->func.key_release_callback = release_key;
        spec->func.motion_callback = mouse_in_spec;
        spec->func.leave_callback = mouse_leave_spec;
        spec->func.button_release_callback = mouse_move_spec;
        spec->func.button_press_callback = mouse_set_spec;
        top->parent_struct = this;
        top->func.key_press_callback = get_key;
        top->func.key_release_callback = release_key;

        Widget_t* gframe = add_my_frame(top,"", width-79, 0, 78, height-82);
        gframe->scale.gravity = WESTSOUTH;
        vumeterL = add_my_vmeter(gframe, "Meter", false, 35, 5, 10, height-88);
        vumeterL->scale.gravity = WESTSOUTH;
        vumeterR = add_my_vmeter(gframe, "Meter", true, 45, 5, 10, height-88);
        vumeterR->scale.gravity = WESTSOUTH;
        vug = add_my_vslider(gframe, "Gain", 8, 6, 20, height-90);
        vug->scale.gravity = WESTSOUTH;
        vug->parent_struct = this;
        set_adjustment(vug->adj,0.0, 0.0, -46.0, 12.0, 0.1, CL_CONTINUOS);
        vug->func.value_changed_callback = set_gain;

        Widget_t* lframe = add_my_frame(top,"", width-79, 350, 78, 80);
        lframe->scale.gravity = SOUTHWEST;
        curFreq = add_my_label(lframe, "",5,0,60,20);
        curGain = add_my_label(lframe, "",5,20,60,20);

        bp = add_my_toggle_button(lframe, 5, 40, 60, 20, "Bypass");
        bp->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        bp->parent_struct = this;
        bp->func.value_changed_callback = bp_response;

        #ifndef CLAPPLUG
        #ifndef LV2PLUG
        Widget_t* quit = add_my_button(lframe, 5, 60, 60, 20, "Quit");
        quit->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        quit->parent_struct = this;
        quit->func.value_changed_callback = quit_response;
        #endif
        #endif

        Widget_t* laframe = add_my_z_frame(spec,"", 1, 331, width-80, 100);
        laframe->scale.gravity = WESTEAST;

        ph = add_my_button(spec, width-105, 0, 20, 20, "φ");
        ph->parent_struct = this;
        ph->func.value_changed_callback = show_phase;

        int x = 1;
        for (int i = 0; i<12; i++) {
            frame[i] = add_my_panel(laframe,"", 285, 0, 250, 99);
            frame[i]->scale.gravity = NORTCENTER;
            double r,g,bcol;
            get_band_color(i, r, g, bcol);
            set_widget_color(frame[i], (Color_state)0, (Color_mod)1, r, g, bcol, 1.0);
           

            prev[i] = add_my_button(frame[i], 10, 78, 20, 18, "<");
            prev[i]->data = i;
            prev[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            prev[i]->parent_struct = this;
            prev[i]->func.value_changed_callback = prev_response;

            solo[i] = add_my_toggle_button(frame[i], 40, 78, 20, 18, "S");
            solo[i]->data = i;
            solo[i]->flags |= IS_RADIO;
            solo[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            solo[i]->parent_struct = this;
            solo[i]->func.value_changed_callback = solo_response;

            mute[i] = add_my_toggle_button(frame[i], 60, 78, 20, 18, "M");
            mute[i]->data = i;
            mute[i]->flags |= IS_RADIO;
            mute[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            mute[i]->parent_struct = this;
            mute[i]->func.value_changed_callback = mute_response;

            ftype[i] = add_type_combobox(frame[i], "Type", 95, 78, 60, 18);
            ftype[i]->data = i;
            ftype[i]->parent_struct = this;
            combobox_add_entry(ftype[i],"Low Shelf");
            combobox_add_entry(ftype[i],"Peak");
            combobox_add_entry(ftype[i],"High Shelf");
            if (i>0 && i<11) {
                combobox_set_active_entry(ftype[i], 1);
            } else if (i>=11) {
                combobox_set_active_entry(ftype[i], 2);
            } else {
                combobox_set_active_entry(ftype[i], 0);
            }
            ftype[i]->func.value_changed_callback = set_ftype;

            fenable[i] = add_my_enable_button(frame[i], 178, 78, 20, 20, "");
            fenable[i]->data = i;
            fenable[i]->parent_struct = this;
            fenable[i]->func.value_changed_callback = set_fenable;
            adj_set_value(fenable[i]->adj, conn->getParameterValue(i * 6 + 1));
            
            get_band_color(i, r, g, bcol);

            set_widget_color(fenable[i], (Color_state)0, (Color_mod)0, r, g, bcol, 1.0);

            next[i] = add_my_button(frame[i], 220, 78, 20, 18, ">");
            next[i]->data = i;
            next[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            next[i]->parent_struct = this;
            next[i]->func.value_changed_callback = next_response;

            freq[i] = add_eq_knob(frame[i], "FREQ", "Hz", 37,10,52, 68);
            set_widget_color(freq[i], (Color_state)0, (Color_mod)1, 0.12, 0.12, 0.14, 1);
            freq[i]->data = i;
            freq[i]->parent_struct = this;
            freq[i]->func.value_changed_callback = set_freq;
            freq[i]->func.button_release_callback = set;
            if (i == 0)
                set_adjustment(freq[i]->adj, 40.0,    40.0,    20.0,    60.0,    0.01, CL_LOGARITHMIC);
            else if (i == 1)
                set_adjustment(freq[i]->adj, 70.0,    70.0,    40.0,   100.0,    0.01, CL_LOGARITHMIC);
            else if (i == 2)
                set_adjustment(freq[i]->adj, 120.0,  120.0,    70.0,   180.0,    0.01, CL_LOGARITHMIC);
            else if (i == 3)
                set_adjustment(freq[i]->adj, 210.0,  210.0,   120.0,   300.0,    0.01, CL_LOGARITHMIC);
            else if (i == 4)
                set_adjustment(freq[i]->adj, 370.0,  370.0,   200.0,   550.0,    0.01, CL_LOGARITHMIC);
            else if (i == 5)
                set_adjustment(freq[i]->adj, 650.0,  650.0,   350.0,   900.0,    0.01, CL_LOGARITHMIC);
            else if (i == 6)
                set_adjustment(freq[i]->adj, 1150.0, 1150.0,  650.0,  1600.0,    0.01, CL_LOGARITHMIC);
            else if (i == 7)
                set_adjustment(freq[i]->adj, 2000.0, 2000.0, 1100.0,  2800.0,    0.01, CL_LOGARITHMIC);
            else if (i == 8)
                set_adjustment(freq[i]->adj, 3500.0, 3500.0, 1800.0,  5000.0,    0.01, CL_LOGARITHMIC);
            else if (i == 9)
                set_adjustment(freq[i]->adj, 6100.0, 6100.0, 3500.0,  9000.0,    0.01, CL_LOGARITHMIC);
            else if (i == 10)
                set_adjustment(freq[i]->adj, 10700.0,10700.0,6000.0, 15000.0,    0.01, CL_LOGARITHMIC);
            else if (i == 11)
                set_adjustment(freq[i]->adj, 18000.0,18000.0,10000.0,20000.0,    0.01, CL_LOGARITHMIC);            

            fq[i] = add_eq_knob(frame[i], "Q", "", 161,10, 52, 68);
            set_widget_color(fq[i], (Color_state)0, (Color_mod)1, 0.12, 0.12, 0.14, 1);
            fq[i]->data = i;
            fq[i]->parent_struct = this;
            fq[i]->func.value_changed_callback = set_fq;
            fq[i]->func.button_release_callback = set;
            if (i == 0)          // Low Shelf
                set_adjustment(fq[i]->adj, 0.7, 0.7, 0.4, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 1)     // 70 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 2)     // 120 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 3)     // 210 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 4)     // 370 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 5)     // 650 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 6)     // 1150 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 7)     // 2000 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 8)     // 3500 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 9)     // 6100 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 10)    // 10700 Hz
                set_adjustment(fq[i]->adj, 1.4, 1.4, 0.5, 10.0, 0.01, CL_LOGARITHMIC);
            else if (i == 11)    // High Shelf
                set_adjustment(fq[i]->adj, 0.7, 0.7, 0.4, 10.0, 0.01, CL_LOGARITHMIC);

            fgain[i] = add_eq_knob(frame[i], "GAIN", "dB", 90,0,70, 78);
            set_widget_color(fgain[i], (Color_state)0, (Color_mod)1, 0.12, 0.12, 0.14, 1);
            fgain[i]->data = i;
            fgain[i]->parent_struct = this;
            set_adjustment(fgain[i]->adj, 0.0, 0.0, -48.0, 24.0, 0.1, CL_CONTINUOS);
            fgain[i]->func.value_changed_callback = set_fgain;
            fgain[i]->func.button_release_callback = set;
            x += 133;
        }

        lowcut = add_my_knob(laframe, "LowCut", "Hz", 20,26,60, 70);
        set_adjustment(lowcut->adj, 19.0, 19.0, 19.0, 2200.0, 0.01, CL_LOGARITHMIC);
        lowcut->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(lowcut, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        lowcut->parent_struct = this;
        lowcut->func.value_changed_callback = set_lowcut;
        lowcut->func.button_release_callback = set;

        highcut = add_my_knob(laframe, "HighCut", "Hz", 120,26,60, 70);
        set_adjustment(highcut->adj, 22000.0, 22000.0, 110.0, 22000.0, 0.01, CL_LOGARITHMIC);
        highcut->flags = USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(highcut, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        highcut->parent_struct = this;
        highcut->func.value_changed_callback = set_highcut;
        highcut->func.button_release_callback = set;

        #ifndef CLAPPLUG
        mode = add_my_combobox(laframe,"Mode", 190, 25, 90, 20);
        mode->scale.gravity = ASPECT;
        mode->parent_struct = this;
        combobox_add_entry(mode,"FFT");
        combobox_add_entry(mode,"Biquad");
        combobox_set_active_entry(mode, 0);
        mode->func.value_changed_callback = set_mode;
        add_tooltip(mode->childlist->childs[0], "Mode");
        #ifndef LV2PLUG
        Widget_t* save = add_xsave_file_button(laframe, 190, 65, 90, 20, "Save IR", " ", ".wav|.WAV");
        save->parent_struct = this;
        save->func.user_callback = save_response;
        #endif
        #endif

        smooth = add_my_knob(laframe, "Smooth", "", 590,26,60, 70);
        set_adjustment(smooth->adj, 0.3, 0.3, 0.0, 1.0, 0.01, CL_CONTINUOS);
        smooth->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(smooth, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        smooth->parent_struct = this;
        smooth->func.value_changed_callback = set_smooth;
        smooth->func.button_release_callback = set;

        dynamics = add_my_knob(laframe, "Contrast", "", 660,26,60, 70);
        set_adjustment(dynamics->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        dynamics->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(dynamics, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        dynamics->parent_struct = this;
        dynamics->func.value_changed_callback = set_dynamics;
        dynamics->func.button_release_callback = set;

        tilt = add_my_knob(laframe, "Tone Bias", "", 730,26,60, 70);
        set_adjustment(tilt->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        tilt->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(tilt, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        tilt->parent_struct = this;
        tilt->func.value_changed_callback = set_tilt;
        tilt->func.button_release_callback = set;
    }

    void show() {
        widget_show_all(top);
        raise_control_panel(active_panel);
        #ifndef CLAPPLUG
        set_controller_mode();
        #endif
    }

    void setSampleRate(const double sr) {
        sampleRate = sr;
    }

    const double getSampleRate() {
        return sampleRate;
    }

    void quitGui() {
        run = false;
        if (top) destroy_widget(top, top->app);
    }

    Xputty *getMain() {
        return &main;
    }

    void check_spec() {
        adj_set_value(vumeterL->adj, power2db(vumeterL, conn->getMeterL()));
        adj_set_value(vumeterR->adj, power2db(vumeterR, conn->getMeterR()));
        if (conn->checkNewData()) {
            bin = conn->getBins();
            mag_.clear();
            const float* m = conn->getMagnitudes();
            for (int i = 0; i<bin; i++) {
                mag_.push_back(m[i]);
            }
            conn->clearAna();
            expose_widget(spec);
        }
    }

    void check_irmatch() {
        if (conn->haveData()) {
            setData(conn->getIR());
            setPhase(conn->getPhase());
            rebuild_eq_layer = true;
            expose_widget(spec);
        }
    }

    static void draw_window(void* w_, void* user_data) {
        Widget_t* w = (Widget_t*)w_;
        cairo_t* cr = w->crb;
        cairo_set_source_rgb(cr, 0.157, 0.165, 0.212);
        cairo_paint(cr);
    }

private:
    Xputty main;
    Widget_t* ref_label = nullptr;
    Widget_t* src_label = nullptr;
    Widget_t* spec = nullptr;
    Widget_t* curFreq = nullptr;
    Widget_t* curGain = nullptr;
    Widget_t* ph = nullptr;
    IConnector* conn = nullptr;
    AudioFile af;
    Vec ir_; // filter
    Vec phase_; // phase
    Vec mag_; // spectrum

    char cfreq[64];
    char cgain[64];
    double sampleRate = 48000.0;
    bool band_match = false;
    bool show_ph = false;
    int match_state = -1;
    int match_band = -1;
    int mx = 0;
    int my = 0;
    int bin = 0;
    int spec_width  = 0;
    int spec_height = 0;
    cairo_surface_t *eq_layer = nullptr;
    bool rebuild_eq_layer = true;
    bool capture_line = false;
    std::atomic<bool> set_leak {false};

    float smooth_s = 0.3f;
    float dynamic_s = 0.0f;
    float tilt_s = 0.0f;

    const float f_min = 20.0f;
    const float f_max = 20000.0f;
    const float db_min = -72.0f;
    const float db_max = 24.0f;

    static void save_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if(user_data !=NULL) {
            std::string ir_file = *(const char**)user_data;
            std::pair<std::vector<double>, std::vector<double> > ir = self->conn->get_ir();        
            self->af.saveAudioFile(ir_file, ir.first, ir.second, self->sampleRate);
        }
    }

    // send value changes from GUI to the engine/host
    void sendValueChanged(int index, float value) {
        conn->sendValueChanged(index, value);
    }

    // Callbacks
    static void quit_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !adj_get_value(w->adj)){
            self->run = false;
            destroy_widget(self->top, self->top->app);
        }
    }

    void set_controller_mode() {
        #ifndef CLAPPLUG
        int c_mode = conn->getParameterValue(83);
        if (c_mode) {
            show_ph = false;
            smooth_s = adj_get_value(smooth->adj);
            dynamic_s = adj_get_value(dynamics->adj);
            tilt_s =  adj_get_value(tilt->adj);
            adj_set_value(smooth->adj, 0.0);
            adj_set_value(dynamics->adj, 0.0);
            adj_set_value(tilt->adj, 0.0);
            widget_hide(ph);
            widget_hide(smooth);
            widget_hide(dynamics);
            widget_hide(tilt);
        } else {
            adj_set_value(smooth->adj, smooth_s);
            adj_set_value(dynamics->adj, dynamic_s);
            adj_set_value(tilt->adj, tilt_s);
            widget_show(ph);
            widget_show(smooth);
            widget_show(dynamics);
            widget_show(tilt);
        }
        #endif
    }

    static void set_mode(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(83, adj_get_value(w->adj));
        self->set_controller_mode();
    }

    static void show_phase(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if (w->flags & HAS_POINTER && !adj_get_value(w->adj)){
            self->show_ph = self->show_ph ? false : true;
            self->rebuild_eq_layer = true;
            expose_widget(self->spec);

        }
    }

    static void get_key(void *w_, void *key_, void *user_data) {
        Widget_t *w = (Widget_t*)w_;
        if (!w) return;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        XKeyEvent *key = (XKeyEvent*)key_;
        if (key->keycode == XKeysymToKeycode(w->app->dpy,XK_Control_L)) {
            self->capture_line = true;
        }
    }

    static void release_key(void *w_, void *key_, void *user_data) {
        Widget_t *w = (Widget_t*)w_;
        if (!w) return;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        XKeyEvent *key = (XKeyEvent*)key_;
        if (key->keycode == XKeysymToKeycode(w->app->dpy,XK_Control_L)) {
            self->capture_line = false;
        }
    }

    static void set_gain(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(82, adj_get_value(w->adj));
    }

    static void bp_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(0, adj_get_value(w->adj));
    }

    void set_radio(Widget_t* w, bool set) {
         for (int i = 0; i<12; i++) {
            Widget_t *wid = solo[i];
            if (wid != w) {
                xevfunc store = wid->func.value_changed_callback;
                wid->func.value_changed_callback = null_callback;
                adj_set_value(wid->adj, 0.0);
                conn->sendValueChanged(73,wid->data);
                conn->sendValueChanged(74,(int)adj_get_value(wid->adj));
                wid->func.value_changed_callback = store;
            }
        }
        if (set) {
            Widget_t * p = (Widget_t*)w->parent;
            int i = 0;
            for(;i<p->childlist->elem;i++) {
                Widget_t *wid = p->childlist->childs[i];
                if (wid->adj && wid->flags & IS_RADIO) {
                    xevfunc store = wid->func.value_changed_callback;
                    wid->func.value_changed_callback = null_callback;
                    if (wid != w) adj_set_value(wid->adj, 0.0);
                    conn->sendValueChanged(wid->data * 6 + 3, (int)adj_get_value(wid->adj));
                    wid->func.value_changed_callback = store;
                }
            }
        }
    }

    void raise_control_panel(int a) {
        for(int i = 0; i < 12; i++) {
            widget_hide(frame[i]);
            if (i == a) {
                active_panel = a;
                widget_show_all(frame[i]);
                os_raise_widget(frame[i]);
            }
        }
    }

    static void prev_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        int r = std::max<int>(0, w->data - 1);
        self->raise_control_panel(r);
    }

    static void next_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        int r = std::min<int>(12, w->data + 1);
        self->raise_control_panel(r);
    }

    static void solo_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->set_radio(w, true);
        self->sendValueChanged(73, w->data);
        self->sendValueChanged(74, adj_get_value(w->adj));
    }

    static void mute_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->set_radio(w, false);
        self->sendValueChanged(3 + (6 * w->data), adj_get_value(w->adj));
    }

    static void set_ftype(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(2 + (6 * w->data), adj_get_value(w->adj));
    }

    static void set_freq(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(4 + (6 * w->data), adj_get_value(w->adj));
    }

    static void set_fq(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(6 + (6 * w->data), adj_get_value(w->adj));
    }

    static void set_fgain(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(5 + (6 * w->data), adj_get_value(w->adj));
    }

    static void set_fenable(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(1 + (6 * w->data), adj_get_value(w->adj));
    }

    static void set_lowcut(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        float v = adj_get_value(w->adj);
        if (v > 20.0f) self->sendValueChanged(75, 1.0f);
        else  self->sendValueChanged(75, 0.0f);
        self->sendValueChanged(76, v);
    }

    static void set_lowcut_enable(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(75, adj_get_value(w->adj));
    }

    static void set_highcut(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        float v = adj_get_value(w->adj);
        if (v < 20000) self->sendValueChanged(77, 1.0f);
        else  self->sendValueChanged(77, 0.0f);
        self->sendValueChanged(78, v);
    }

    static void set_highcut_enable(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(77, adj_get_value(w->adj));
    }

    static void set_smooth(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(79, adj_get_value(w->adj));
    }

    static void set_dynamics(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(80, adj_get_value(w->adj));
    }

    static void set_tilt(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(81, adj_get_value(w->adj));
    }

    static void set(void *w_, void *event, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->set_leak.store(true, std::memory_order_release);
    }

    static void mouse_set_spec(void *w_, void *xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->button == Button1) {
                self->mx = xbutton->x;
                self->my = xbutton->y;
            }
        }
    }

    void infoString(float x, float y) {
        Metrics_t m;
        os_get_window_metrics(spec, &m);
        const int width  = m.width;
        const int height = m.height- (80 * spec->app->hdpi);
        if ((int)y > height) {
            mouse_leave_spec(spec, nullptr);
            return;
        }

        float freq = x_to_freq(x, f_min, f_max, width);
        float g = y_to_db(y, db_min, db_max, height);
        if (freq >= 10000.0f)
            snprintf(cfreq, 63, "%.1f kHz", freq / 1000.0);
        else if (freq >= 1000.0f)
            snprintf(cfreq, 63, "%.2f kHz", freq / 1000.0);
        else if (freq >= 100.0f)
            snprintf(cfreq, 63, "%.1f Hz", freq );
        else
            snprintf(cfreq, 63, " %.1f Hz", freq);

        if (g > 10.0f) 
            snprintf(cgain, 63, " %.1f dB", g);
        else if (g > -0.001f) 
            snprintf(cgain, 63, "  %.1f dB", g);
        else if (g > -10.0f) 
            snprintf(cgain, 63, " %.1f dB", g);
        else
            snprintf(cgain, 63, "%.1f dB", g);
        curFreq->label = cfreq;
        curGain->label = cgain;
        expose_widget(curFreq);
        expose_widget(curGain);
    }

    static void mouse_leave_spec(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->curFreq->label = "";
        self->curGain->label = "";
        expose_widget(self->curFreq);
        expose_widget(self->curGain);
    }

    int find_band_for_freq(float target_freq) {
        static const float band_min[12] = {
            20.0f, 40.0f, 70.0f, 120.0f, 200.0f, 350.0f,
            650.0f, 1100.0f, 1800.0f, 3500.0f, 6000.0f, 10000.0f
        };
        static const float band_max[12] = {
            60.0f, 100.0f, 180.0f, 300.0f, 550.0f, 900.0f,
            1600.0f, 2800.0f, 5000.0f, 9000.0f, 15000.0f, 20000.0f
        };

        for (int i = 0; i < 12; ++i) {
            if (target_freq >= band_min[i] && target_freq <= band_max[i])
                return i;
        }
        int best = 0;
        float best_dist = 1e9f;
        for (int i = 0; i < 12; ++i) {
            float center = adj_get_value(freq[i]->adj);
            float dist = std::abs(std::log(target_freq / center));
            if (dist < best_dist) { best_dist = dist; best = i; }
        }
        return best;
    }

    static void mouse_in_spec(void *w_, void *xmotion_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        XMotionEvent *xmotion = (XMotionEvent*)xmotion_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        int x1 = xmotion->x;
        int y1 = xmotion->y;
        self->match_state = 0;
        self->infoString(x1, y1);
        //std::cout << "x " << x1 << " y " << y1 << std::endl;
        if(xmotion->state & Button1Mask) {
            if (self->capture_line) {
                Metrics_t m;
                os_get_window_metrics(w, &m);
                const int width  = m.width;
                const int height = m.height - (80 * w->app->hdpi);

                float target_freq = x_to_freq(x1, self->f_min, self->f_max, width);
                float target_gain = y_to_db(y1, self->db_min, self->db_max, height);

                int band = self->find_band_for_freq(target_freq);

                target_gain = std::clamp(target_gain, -48.0f, 24.0f);
                adj_set_value(self->fgain[band]->adj, target_gain);
                self->sendValueChanged(5 + (6 * band), target_gain);

                self->rebuild_eq_layer = true;
                expose_widget(self->spec);

                self->mx = x1;
                self->my = y1;
            } else {
                self->match_state = 1;
                if (self->band_match) {
                    float v = adj_get_value(self->freq[self->match_band]->adj);
                    float deltaX = (float)x1 - self->mx;
                    v *= std::pow(2.0, deltaX * 0.005);
                    self->mx = x1;
                    adj_set_value(self->freq[self->match_band]->adj, v);

                    float vg = adj_get_value(self->fgain[self->match_band]->adj);
                    float deltay = (float)y1 - self->my;
                    vg += deltay * -0.1;
                    self->my = y1;
                    if (std::abs(vg) < 0.2) vg = 0.0;
                    adj_set_value(self->fgain[self->match_band]->adj, vg);
                    expose_widget(self->spec);
                }
            }
        } else {
            self->find_hovered_band(x1, y1);
        }
    }

    static void mouse_move_spec(void *w_, void *xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if (self->band_match) {
                if(xbutton->button == Button4) {
                    float vq = adj_get_value(self->fq[self->match_band]->adj);
                    vq *= std::pow(2.0, 0.1);
                    adj_set_value(self->fq[self->match_band]->adj,vq);
                    expose_widget(self->spec);
                } else if(xbutton->button == Button5) {
                    float vq = adj_get_value(self->fq[self->match_band]->adj);
                    vq *= std::pow(2.0, -0.1);
                    adj_set_value(self->fq[self->match_band]->adj,vq);
                    expose_widget(self->spec);
                }
            }
        }

    }

    // Helpers
    static float clampf(float x, float lo, float hi) {
        return (x < lo) ? lo : (x > hi) ? hi : x;
    }

    static float db_to_y(float db, float db_min, float db_max, int height) {
        float norm = (db - db_min) / (db_max - db_min);
        norm = clampf(norm, 0.0f, 1.0f);
        return (1.0f - norm) * height;
    }

    static float freq_to_x(float freq, float f_min, float f_max, int width) {
        const float x_pad = 3.0f;
        const float inv_log_range = 1.0f / log10f(f_max / f_min);

        freq = std::clamp(freq, f_min, f_max);

        float norm = log10f(freq / f_min) * inv_log_range;
        return x_pad + norm * (width - 2.0f * x_pad);
    }

    static void draw_text(cairo_t* cr, float x, float y, const char* txt) {
        cairo_move_to(cr, std::max<float>(5.0f, x), y);
        cairo_text_path (cr, txt);
        cairo_fill (cr);
    }

    // Drawing

    struct Theme {
        // background
        double bg_r = 0.08;
        double bg_g = 0.09;
        double bg_b = 0.11;

        // grid
        double grid_major_r = 0.35;
        double grid_major_g = 0.38;
        double grid_major_b = 0.42;

        double grid_minor_r = 0.22;
        double grid_minor_g = 0.24;
        double grid_minor_b = 0.28;

        // text
        double text_r = 0.8;
        double text_g = 0.82;
        double text_b = 0.85;

        double text_dim_r = 0.5;
        double text_dim_g = 0.52;
        double text_dim_b = 0.55;

        // spectrum
        double spec_alpha = 0.35;

        // band fill / glow
        double band_fill_alpha = 0.22;
        double band_glow_alpha = 0.08;
        double band_line_alpha = 0.95;
    };
    Theme t;

    static void draw_callback(void* w_, void* user_data) {
        Widget_t* w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->draw(w_);
    }

    static void get_band_color(int i, double &r, double &g, double &b) {
        switch(i) {
        case 0:  r=0.95; g=0.45; b=0.20; break;
        case 1:  r=0.95; g=0.60; b=0.20; break;
        case 2:  r=0.95; g=0.80; b=0.25; break;
        case 3:  r=0.75; g=0.90; b=0.25; break;
        case 4:  r=0.40; g=0.85; b=0.35; break;
        case 5:  r=0.25; g=0.85; b=0.55; break;
        case 6:  r=0.25; g=0.80; b=0.75; break;
        case 7:  r=0.25; g=0.70; b=0.95; break;
        case 8:  r=0.30; g=0.55; b=0.95; break;
        case 9:  r=0.45; g=0.45; b=0.95; break;
        case 10: r=0.60; g=0.40; b=0.95; break;
        default: r=0.80; g=0.35; b=0.95; break;
        }
    }

    static double mapQ(double q_ui) {
        // clamp UI range
        q_ui = std::clamp(q_ui, 0.1, 10.0);
        // log-space mapping
        double x = std::log(q_ui);
        // soften curve
        double shaped = std::tanh(x * 0.8);
        // back to linear
        double q = std::exp(shaped * 1.5);
        return q;
    }

    static float y_to_db(float y, float db_min, float db_max, int height) {
        float norm = 1.0f - (y / height);
        norm = clampf(norm, 0.0f, 1.0f);
        return db_min + norm * (db_max - db_min);
    }

    static inline double x_to_freq(double x, double f_min, double f_max, int width) {
        double t = x / (double)width;
        // log interpolation
        double log_min = std::log(f_min);
        double log_max = std::log(f_max);
        double log_f = log_min + t * (log_max - log_min);
        return std::exp(log_f);
    }

    static double eval_band_db(IConnector* conn, int i, double f, double sr) {
        if (f < 10.0) return 0.0;
        double freq = conn->getParameterValue(i * 6 + 4);
        double gain = conn->getParameterValue(i * 6 + 5);
        double q = conn->getParameterValue(i * 6 + 6);
        int type = conn->getParameterValue(i * 6 + 2);
        double x = std::log2((f + 1e-9) / (freq + 1e-9));
        double Q = mapQ(q);

        switch (type) {
            case 0: { // Band::LowShelf
                double slope = Q * 2.0;
                double g = 0.5 * (1.0 - std::tanh(slope * x));
                return gain * g;
            }
            case 1: { // Band::Peak
                double sigma = 1.0 / (1.5 * Q + 0.5);
                double g = std::exp(-0.5 * (x * x) / (sigma * sigma));
                return gain * g;
            }
            case 2: { // Band::HighShelf
                double slope = Q * 2.0;
                double g = 0.5 * (1.0 + std::tanh(slope * x));
                return gain * g;
            }
        }
        return 0.0;
    }

    void draw_band_ring(cairo_t* cr, float x, float y, int i, int state) {
        double r,g,bcol;
        get_band_color(i, r, g, bcol);
        double radius = 6.0;
        double ring   = 10.0;
        double alpha  = 1.0;

        if (state == 0) {
            radius = 7.5;
            ring   = 14.0;
            alpha  = 0.5;
        }
        else if (state == 1) {
            radius = 8.5;
            ring   = 18.0;
            alpha  = 0.6;
        }

        // glow
        cairo_arc(cr, x, y, ring, 0, 2*M_PI);
        cairo_set_source_rgba(cr, r, g, bcol, 0.15);
        cairo_fill(cr);

        // ring
        cairo_arc(cr, x, y, radius + 3, 0, 2*M_PI);
        cairo_set_line_width(cr, 2.0);
        cairo_set_source_rgba(cr, r, g, bcol, 0.9 * alpha);
        cairo_stroke(cr);

        // center dot
        cairo_arc(cr, x, y, radius, 0, 2*M_PI);
        cairo_set_source_rgba(cr, r, g, bcol, 1.0 * alpha);
        cairo_fill(cr);
    }

    void find_hovered_band(float mx, float my) {
        Metrics_t m;
        os_get_window_metrics(spec, &m);
        const int width  = m.width;
        const int height = m.height- (80 * spec->app->hdpi);
        for (int i = 0; i < 12; ++i) {
            float x = freq_to_x(conn->getParameterValue(i * 6 + 4), f_min, f_max, width);
            float y = db_to_y(conn->getParameterValue(i * 6 + 5), db_min, db_max, height);

            float dx = mx - x;
            float dy = my - y;

            if (dx*dx + dy*dy < 12*12) {
                band_match = true;
                match_band = i;
                rebuild_eq_layer = true;
                raise_control_panel(match_band);
                os_expose_widget(spec);
                return ;
            } else if (band_match) {
                band_match = false;
                rebuild_eq_layer = true;
                mouse_leave_spec(spec, nullptr);
                os_expose_widget(spec);
            }
        }
        band_match = false;
    }

    void draw_band_curves(cairo_t* cr, int width, int height) {
        const int STEPS = width;
        float y0 = db_to_y(0.0, db_min, db_max, height);

        for (int i = 0; i < 12; ++i) {
            if (!(int)conn->getParameterValue(i * 6 + 1)) continue;
            bool isStarted = false;
            double startX = 0.0;
            double stopX = 0.0;
            double lastX = 0.0;
            // band color
            double r,g,bcol;
            get_band_color(i, r, g, bcol);
            cairo_new_path(cr);
            for (int x = 0; x < STEPS; ++x) {
                double freq = x_to_freq(x, f_min, f_max, width);
                double db = eval_band_db(conn, i, freq, sampleRate);
                double y = db_to_y(db, db_min, db_max, height);
                if (fabs(y - y0) < 0.5) continue;

                if (!isStarted) {
                    cairo_move_to(cr, x, y);
                    startX = x;
                    lastX = x;
                    isStarted = true;
                } else if (x > lastX + 2.0) {
                    cairo_line_to(cr, x, y);
                }
                stopX = x;
            }

            // fill
            cairo_line_to(cr, stopX, y0);
            cairo_line_to(cr, startX, y0);
            cairo_close_path(cr);

            cairo_set_source_rgba(cr, r, g, bcol, t.band_fill_alpha);
            cairo_fill_preserve(cr);
            // glow
            cairo_pattern_t* glow = cairo_pattern_create_linear(startX, 0, stopX, 0);
            cairo_pattern_add_color_stop_rgba(glow, 0, r, g, bcol,t.band_glow_alpha * 0.1);
            cairo_pattern_add_color_stop_rgba(glow, 0.5, r, g, bcol,t.band_glow_alpha * 0.8);
            cairo_pattern_add_color_stop_rgba(glow, 1, r, g, bcol,t.band_glow_alpha * 0.1);
            for (int k = 0; k < 3; ++k) {
                double width_glow = 6.0 + k * 4.0;

                cairo_set_line_width(cr, width_glow);
                cairo_set_source(cr, glow);
                //cairo_set_source_rgba(cr, r, g, bcol, t.band_fill_alpha * 0.5);
                cairo_stroke_preserve(cr);
            }
            cairo_pattern_destroy(glow);

            // line
            cairo_pattern_t* grad = cairo_pattern_create_linear(startX, 0, stopX, 0);
            cairo_pattern_add_color_stop_rgba(grad, 0, r, g, bcol,t.band_line_alpha * 0.1);
            cairo_pattern_add_color_stop_rgba(grad, 0.5, r, g, bcol,t.band_line_alpha * 1);
            cairo_pattern_add_color_stop_rgba(grad, 1, r, g, bcol,t.band_line_alpha * 0.1);
            cairo_set_line_width(cr, 1.5);
            //cairo_set_source_rgba(cr, r, g, bcol, t.band_line_alpha);
            cairo_set_source(cr, grad);
            cairo_stroke(cr);
            cairo_pattern_destroy(grad);
        }
    }

    void draw_band_points(cairo_t* cr, const int width, const int height) {
        for(int i = 0; i<12; i++) {
            double r,g,bcol;
            get_band_color(i, r, g, bcol);
            cairo_set_source_rgba(cr, r, g, bcol, 1.0);

            int on = conn->getParameterValue(i * 6 + 1);
            float db = db_to_y(conn->getParameterValue(i * 6 + 5), db_min, db_max, height);
            float freq = freq_to_x(conn->getParameterValue(i * 6 + 4), f_min, f_max, width);
            if (on) {
                cairo_set_line_width(cr, 10.0);
                cairo_move_to(cr, freq, db);
                cairo_line_to(cr, freq, db);
                cairo_stroke(cr);
                if (band_match && (match_band == i)) {
                    draw_band_ring(cr, freq, db, i, match_state);
                }
            }
        }
    }

    void create_background(Widget_t *w, const int width, const int height) {
        std::vector<double> freqs = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        std::vector<double> minor_freqs = {30, 40, 60, 70, 80, 90, 300, 400, 600, 700, 800,
                                                    900, 3000, 4000, 6000, 7000, 8000, 9000};
        std::vector<double> dbs = { -48, -24, -18, -12, -6, 0, 6, 12, 18, 24};

        if (w->image) cairo_surface_destroy(w->image);
        w->image = nullptr;
        w->image = cairo_surface_create_similar (w->surface,
                            CAIRO_CONTENT_COLOR_ALPHA, width, height);
        if (!w->image || cairo_surface_status(w->image) != CAIRO_STATUS_SUCCESS) {
            w->image = nullptr;
            return;
        }
        cairo_t *cr = cairo_create (w->image);
        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_destroy(cr);
            return;
        }
        cairo_set_source_rgb(cr, t.bg_r, t.bg_g, t.bg_b);
        cairo_rectangle(cr, 0, 0, width, height);
        cairo_fill(cr);

        cairo_pattern_t* grad = cairo_pattern_create_linear(0, 0, 0, height);
        cairo_pattern_add_color_stop_rgba(grad, 0, 1,1,1,0.04);
        cairo_pattern_add_color_stop_rgba(grad, 1, 0,0,0,0.1);

        cairo_rectangle(cr, 0, 0, width, height);
        cairo_set_source(cr, grad);
        cairo_fill(cr);

        cairo_pattern_destroy(grad);

        cairo_set_source_rgba(cr, 0.267, 0.267, 0.267, 0.8);
        cairo_set_line_width(cr, 1.0);

        // Frequency lines
        for (double f : freqs) {
            double x = freq_to_x(f, f_min, f_max, width);
            bool major = (f == 100 || f == 1000 || f == 10000);
            cairo_set_source_rgba(
                cr,
                major ? t.grid_major_r : t.grid_minor_r,
                major ? t.grid_major_g : t.grid_minor_g,
                major ? t.grid_major_b : t.grid_minor_b,
                major ? 0.5 : 0.25
            );

            cairo_set_line_width(cr, major ? 1.5 : 1.0);
            cairo_move_to(cr, x, 0);
            cairo_line_to(cr, x, height);
            cairo_stroke(cr);
        }
        for (double f : minor_freqs) {
            double x = freq_to_x(f, f_min, f_max, width);
            cairo_set_source_rgba(
                cr, t.grid_minor_r, t.grid_minor_g, t.grid_minor_b, 0.1 );

            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, x, 0);
            cairo_line_to(cr, x, height);
            cairo_stroke(cr);
        }

        // dB lines
        for (double db : dbs) {
            double y = db_to_y(db, db_min, db_max, height);
            bool major = (db == 0);
            cairo_set_source_rgba(
                cr,
                major ? t.grid_major_r : t.grid_minor_r,
                major ? t.grid_major_g : t.grid_minor_g,
                major ? t.grid_major_b : t.grid_minor_b,
                major ? 0.6 : 0.25
            );

            cairo_set_line_width(cr, major ? 1.5 : 1.0);
            cairo_move_to(cr, 0, y);
            cairo_line_to(cr, width, y);
            cairo_stroke(cr);
        }
        // frequency labels
        for (double f : freqs) {
            double x = freq_to_x(f, f_min, f_max, width);
            double ax = 0;
            char buf[32];
            if (f >= 1000)
                sprintf(buf, "%.0fk", f / 1000.0);
            else
                sprintf(buf, "%.0f", f);
            if (f >= 500 && f <=999) ax = 18 * w->app->hdpi;
            if (f >= 501 && f <=1000) ax = 15 * w->app->hdpi;
            cairo_set_source_rgba(cr, t.text_dim_r, t.text_dim_g, t.text_dim_b, 0.7);
            cairo_move_to(cr, x + 4, (height - ax) - 6);
            cairo_text_path (cr, buf);
            cairo_fill (cr);
        }

        // dB labels
        for (double db : dbs) {
            double y = db_to_y(db, db_min, db_max, height);
            char buf[16];
            sprintf(buf, "%.0f", db);
            cairo_set_source_rgba(cr, t.text_dim_r, t.text_dim_g, t.text_dim_b, 0.7);
            cairo_move_to(cr, 5, y - 2);
            cairo_text_path (cr, buf);
            cairo_fill (cr);
        }
        cairo_destroy(cr);
    }

    void create_eq_layer(Widget_t *w, const int width, const int height, const float sample_rate) {
        if (spec_height != height || spec_width != width) {
            if(eq_layer) cairo_surface_destroy(eq_layer);
            eq_layer = nullptr;
            eq_layer = cairo_surface_create_similar (w->surface,
                                CAIRO_CONTENT_COLOR_ALPHA, width, height);
            if (!eq_layer || cairo_surface_status(eq_layer) != CAIRO_STATUS_SUCCESS) {
                eq_layer = nullptr;
                return;
            }
        }
        cairo_t *cr = cairo_create (eq_layer);
        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_destroy(cr);
            return;
        }
        cairo_set_source_surface (cr, w->image, 0, 0);
        cairo_rectangle(cr,0, 0, width, height);
        cairo_fill(cr);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        draw_band_points(cr, width, height);
        draw_band_curves(cr, width, height);
        if (show_ph) drawSpectrum(cr, phase_, width, height, 1, sample_rate, 0.945, 0.114, 0.192, "",        height-80);
        drawSpectrum(cr, ir_,    width, height, 2.5, sample_rate, 0.545, 0.914, 0.992, "",      height-80);
        cairo_destroy(cr);
        rebuild_eq_layer = false;
    }

    void draw(void* w_) {
        Widget_t* w = (Widget_t*)w_;

        Metrics_t m;
        os_get_window_metrics(w, &m);
        if (!m.visible) return;

        cairo_t* cr = w->crb;

        const float sample_rate = sampleRate;

        const int width  = m.width;
        const int height = m.height- (80 * w->app->hdpi);
        if (spec_height != height || spec_width != width) {
            create_background(w, width, height);
            create_eq_layer(w, width, height, sample_rate);
        }
        if (!eq_layer || rebuild_eq_layer) {
            create_eq_layer(w, width, height, sample_rate);
        }
        if (eq_layer) {
            cairo_set_source_surface (w->crb, eq_layer, 0, 0);
            cairo_rectangle(w->crb,0, 0, width, height);
            cairo_fill(w->crb);
        } else {
            create_background(w, width, height);
            create_eq_layer(w, width, height, sample_rate);
            return;
        }
        spec_height = height;
        spec_width = width;

        drawSpectrum(cr, mag_, width, height, 1.5, sample_rate, 0.45, 0.2, 0.75, "", height-100, false, true);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11);

    }

    void drawSpectrum(cairo_t* cr, const Vec& mags, int width, int height, double line_width,
                      float sample_rate, float r, float g, float b, const char* label,
                      float label_y, bool dash = false, bool fill = false) {

        if (mags.empty()) return;

        cairo_set_source_rgba(cr, r, g, b, t.spec_alpha);
        //draw_text(cr, width - 60, label_y, label);

        cairo_set_line_width(cr, line_width);
        static const double dashes[] = {2.0};
        if (dash) {
            cairo_set_dash(cr, dashes, 1, 0);
            cairo_set_line_width(cr, 1.0);
        } else {
            cairo_set_dash(cr, dashes, 0, 0);
        }

        int bins = mags.size();
        int fft_size = bins * 2;

        bool started = false;

        float last_x = -1.0f;
        for (int i = 1; i < bins; ++i) {
            float freq = (float)i * sample_rate / fft_size;
            if (freq < f_min || freq > f_max) continue;

            float x = freq_to_x(freq, f_min, f_max, width);
            float y = db_to_y(mags[i], db_min, db_max, height);

            if (!started) {
                cairo_move_to(cr, 3, y);
                started = true;
            } else {
                if (x > last_x + 0.8f) {
                    cairo_line_to(cr, x, y);
                    last_x = x;
                }
            }
        }
        cairo_stroke_preserve(cr);
    
        // Spectrum fill
        if (started && fill) {
            //cairo_set_source_rgba(cr,  0.17, 0.82, 0.64, 0.15);
            cairo_line_to(cr, width, height);
            cairo_line_to(cr, 3, height);
            cairo_close_path(cr);
            cairo_pattern_t* pat = cairo_pattern_create_linear(0, 0, 0, height);
            cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.75, 0.2, 0.9, 0.25);
            cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.45, 0.2, 0.75, 0.05);
            cairo_set_source(cr, pat);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);
        }
        cairo_stroke(cr);
    }
};
