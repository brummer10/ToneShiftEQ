
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

#include "BiquadResponse.h"
#include "SVFResponse.h"
#include "APOReader.h"
#include "IRtoEQ.h"
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
    int zoom_step = 0;
    float threshold_tilt = 0.0f;

    Widget_t* top = nullptr;
    Widget_t* bp = nullptr;
    Widget_t* frame[FilterTypes::NumFilters];
    Widget_t* ftype[FilterTypes::NumFilters];
    Widget_t* fenable[FilterTypes::NumFilters];
    Widget_t* freq[FilterTypes::NumFilters];
    Widget_t* fq[FilterTypes::NumFilters];
    Widget_t* fgain[FilterTypes::NumFilters];
    Widget_t* solo[FilterTypes::NumFilters];
    Widget_t* mute[FilterTypes::NumFilters];
    Widget_t* prev[FilterTypes::NumFilters];
    Widget_t* next[FilterTypes::NumFilters];
    Widget_t* threshold[FilterTypes::NumFilters];
    Widget_t* ratio[FilterTypes::NumFilters];
    Widget_t* com_ex[FilterTypes::NumFilters];

    Widget_t* lowcut = nullptr;
    Widget_t* highcut = nullptr;

    Widget_t* mode = nullptr;
    Widget_t* save = nullptr;

    Widget_t* hf_fade = nullptr;
    Widget_t* smooth = nullptr;
    Widget_t* dynamics = nullptr;
    Widget_t* tilt = nullptr;
    Widget_t* vug = nullptr;
    Widget_t* vumeterL = nullptr;
    Widget_t* vumeterR = nullptr;
    Widget_t* vuing = nullptr;
    Widget_t* vuinmeterL = nullptr;
    Widget_t* vuinmeterR = nullptr;
    Widget_t* apo_loader = nullptr;
    Widget_t* side = nullptr;
    Widget_t* gthr = nullptr;
    Widget_t* gthrv = nullptr;
    Widget_t* dyn = nullptr;

    std::atomic<bool> havePreset {false};
    std::vector<double> dstL;
    std::vector<double> dstR;

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

    void setInSpec(const float* data, int bin) {
        magin_.clear();
        for (int i = 0; i<bin; i++) {
            magin_.push_back(data[i]);
        }
        os_expose_widget(spec);
    }

    void setSpec(const float* data, int bin) {
        mag_.clear();
        for (int i = 0; i<bin; i++) {
            mag_.push_back(data[i]);
        }
        os_expose_widget(spec);
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

    void init(int width = 930, int height = 430) {
        main_init(&main);
        top = create_window(&main, os_get_root_window(&main, IS_WINDOW), 0, 0, width, height);
        widget_set_title(top, "ToneShift-EQ12");
        widget_set_icon_from_png(top,LDVAR(toneshifteq_png));
        //top->flags = NO_PROPAGATE;
        top->func.expose_callback = draw_window;
    }

    void create(int width = 930, int height = 430) {
        spec_width  = 0;
        spec_height = 0;

        spec = create_widget(&main, top,65, 0, width-130, height);
        os_set_input_mask(spec);

        spec->parent_struct = this;
        spec->flags |= NO_PROPAGATE;
        spec->flags &= ~USE_TRANSPARENCY;
        spec->scale.gravity = NORTHWEST;
        spec->func.expose_callback = draw_callback;
        spec->func.key_press_callback = get_key;
        spec->func.key_release_callback = release_key;
        spec->func.motion_callback = mouse_in_spec;
        spec->func.leave_callback = mouse_leave_spec;
        spec->func.button_release_callback = mouse_move_spec;
        spec->func.button_press_callback = mouse_set_spec;
        spec->func.double_click_callback = reset_spec;
        top->parent_struct = this;
        top->func.key_press_callback = get_key;
        top->func.key_release_callback = release_key;

        Widget_t* ginframe = add_my_frame(top,"", 1, 0, 64, height-82);
        ginframe->scale.gravity = EASTNORTH;
        vuinmeterL = add_my_left_vmeter(ginframe, "Meter", true, 3, 5, 10, height-88);
        vuinmeterL->scale.gravity = WESTSOUTH;
        vuinmeterR = add_my_vmeter(ginframe, "Meter", false, 28, 5, 10, height-88);
        vuinmeterR->scale.gravity = WESTSOUTH;
        vuing = add_my_vslider(ginframe, "Gain", 40, 6, 20, height-90);
        vuing->scale.gravity = WESTSOUTH;
        vuing->parent_struct = this;
        set_adjustment(vuing->adj,0.0, 0.0, -46.0, 12.0, 0.1, CL_CONTINUOS);
        vuing->func.value_changed_callback = set_ingain;

        Widget_t* linframe = add_my_frame(top,"", 1, 350, 64, 80);
        linframe->scale.gravity = EASTWEST;

        side = add_my_input_button(linframe, 16, 0, 40, 40);
        side->parent_struct = this;
        side->func.value_changed_callback = set_side;

        gthr =  add_my_threshold_button(linframe, 16, 40, 40, 40);
        gthr->parent_struct = this;
        gthr->func.value_changed_callback = set_global_threshold;

        gthrv = add_my_mini_slider(linframe, "", 4, 4, 10, 72);
        gthrv->parent_struct = this;
        set_adjustment(gthrv->adj,0.0, 0.0, -46.0, 0.0, 0.1, CL_CONTINUOS);
        gthrv->func.value_changed_callback = set_global_threshold_value;

        Widget_t* gframe = add_my_frame(top,"", width-65, 0, 64, height-82);
        gframe->scale.gravity = WESTSOUTH;
        vumeterL = add_my_vmeter(gframe, "Meter", false, 25, 5, 10, height-88);
        vumeterL->scale.gravity = WESTSOUTH;
        vumeterR = add_my_vmeter(gframe, "Meter", true, 35, 5, 10, height-88);
        vumeterR->scale.gravity = WESTSOUTH;
        vug = add_my_vslider(gframe, "Gain", 3, 6, 20, height-90);
        vug->scale.gravity = WESTSOUTH;
        vug->parent_struct = this;
        set_adjustment(vug->adj,0.0, 0.0, -46.0, 12.0, 0.1, CL_CONTINUOS);
        vug->func.value_changed_callback = set_gain;

        Widget_t* lframe = add_my_frame(top,"", width-65, 350, 64, 80);
        lframe->scale.gravity = SOUTHWEST;
        curFreq = add_my_label(lframe, "",3,0,60,20);
        curGain = add_my_label(lframe, "",3,20,60,20);

        #ifndef CLAPPLUG
        #ifndef LV2PLUG
        Widget_t* quit = add_my_quit_button(lframe, 10, 40, 40, 40, "Quit");
        quit->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        quit->parent_struct = this;
        quit->func.value_changed_callback = quit_response;
        #endif
        #endif

        Widget_t* laframe = add_my_z_frame(spec,"", 0, 331, width-130, 100);
        laframe->scale.gravity = WESTEAST;

        ph = add_my_button(spec, width-165, 0, 20, 20, "φ");
        ph->parent_struct = this;
        ph->scale.gravity = FIXEDSIZE;
        ph->func.value_changed_callback = show_phase;

        int x = 1;
        for (int i = 0; i<FilterTypes::NumFilters; i++) {
            frame[i] = add_my_panel(laframe,"", 275, 0, 270, 99);
            frame[i]->scale.gravity = NORTCENTER;
            double r,g,bcol;
            get_band_color(i, r, g, bcol);
            set_widget_color(frame[i], (Color_state)0, (Color_mod)1, r, g, bcol, 1.0);
           

            prev[i] = add_my_button(frame[i], 5, 78, 22, 18, "<");
            prev[i]->data = i;
            prev[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            prev[i]->parent_struct = this;
            prev[i]->func.value_changed_callback = prev_response;

            fenable[i] = add_my_enable_button(frame[i], 32, 78, 20, 20, "");
            fenable[i]->data = i;
            fenable[i]->parent_struct = this;
            fenable[i]->func.value_changed_callback = set_fenable;
            adj_set_value(fenable[i]->adj, conn->getParameterValue(i * 6 + 1));
            get_band_color(i, r, g, bcol);
            set_widget_color(fenable[i], (Color_state)0, (Color_mod)0, r, g, bcol, 1.0);

            solo[i] = add_my_toggle_button(frame[i], 54, 78, 22, 18, "S");
            solo[i]->data = i;
            solo[i]->flags |= IS_RADIO;
            solo[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            solo[i]->parent_struct = this;
            solo[i]->func.value_changed_callback = solo_response;

            mute[i] = add_my_toggle_button(frame[i], 76, 78, 22, 18, "M");
            mute[i]->data = i;
            mute[i]->flags |= IS_RADIO;
            mute[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            mute[i]->parent_struct = this;
            mute[i]->func.value_changed_callback = mute_response;

            ftype[i] = add_type_combobox(frame[i], "Type", 105, 78, 60, 18);
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

            ratio[i] = add_my_combobox(frame[i], "Ratio", 175, 78, 68, 18);
            ratio[i]->data = i;
            ratio[i]->parent_struct = this;
            combobox_add_entry(ratio[i], "Ratio 2/1");
            combobox_add_entry(ratio[i], "Ratio 3/1");
            combobox_add_entry(ratio[i], "Ratio 4/1");
            combobox_add_entry(ratio[i], "Ratio 5/1");
            combobox_add_entry(ratio[i], "Ratio 10/1");
            combobox_set_active_entry(ratio[i], 1);
            ratio[i]->func.value_changed_callback = set_ratio;

            next[i] = add_my_button(frame[i], 243, 78, 22, 18, ">");
            next[i]->data = i;
            next[i]->flags |= USE_TRANSPARENCY | FAST_REDRAW;
            next[i]->parent_struct = this;
            next[i]->func.value_changed_callback = next_response;

            freq[i] = add_eq_knob(frame[i], "FREQ", "Hz", 37,10,42, 68);
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

            fq[i] = add_eq_knob(frame[i], "Q", "", 141,0, 52, 78);
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

            fgain[i] = add_eq_knob(frame[i], "GAIN", "dB", 80,0,60, 78);
            set_widget_color(fgain[i], (Color_state)0, (Color_mod)1, 0.12, 0.12, 0.14, 1);
            fgain[i]->data = i;
            fgain[i]->parent_struct = this;
            set_adjustment(fgain[i]->adj, 0.0, 0.0, -48.0, 24.0, 0.1, CL_CONTINUOS);
            fgain[i]->func.value_changed_callback = set_fgain;
            fgain[i]->func.button_release_callback = set;

            threshold[i] = add_eq_knob(frame[i], "THRESHOLD", "dB", 184,10,62, 68);
            set_widget_color(threshold[i], (Color_state)0, (Color_mod)1, 0.12, 0.12, 0.14, 1);
            threshold[i]->data = i;
            threshold[i]->parent_struct = this;
            set_adjustment(threshold[i]->adj, 0.0, 0.0, -46.0, 0.0, 0.1, CL_CONTINUOS);
            threshold[i]->func.value_changed_callback = set_threshold;

            com_ex[i] = add_my_comp_exp_button(frame[i], 246,63,15, 15);
            com_ex[i]->data = i;
            com_ex[i]->parent_struct = this;
            com_ex[i]->func.value_changed_callback = set_comp_exp_mode;

            x += 133;
        }

        lowcut = add_my_knob(laframe, "LowCut", "Hz", 0,26,60, 70);
        set_adjustment(lowcut->adj, 19.0, 19.0, 19.0, 2200.0, 0.01, CL_LOGARITHMIC);
        lowcut->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(lowcut, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        lowcut->parent_struct = this;
        lowcut->func.value_changed_callback = set_lowcut;
        lowcut->func.button_release_callback = set;

        highcut = add_my_knob(laframe, "HighCut", "Hz", 60,26,60, 70);
        set_adjustment(highcut->adj, 22000.0, 22000.0, 110.0, 22000.0, 0.01, CL_LOGARITHMIC);
        highcut->flags = USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(highcut, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        highcut->parent_struct = this;
        highcut->func.value_changed_callback = set_highcut;
        highcut->func.button_release_callback = set;

        apo_loader = add_my_file_button(laframe, 120, 20, 40, 40, "APO", " ", ".txt");
        apo_loader->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        apo_loader->parent_struct = this;
        apo_loader->func.user_callback = apo_load_response;

        Widget_t* apo_save = add_ysave_file_button(laframe, 120, 60, 40, 40, "APO", " ", ".txt");
        apo_save->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        apo_save->parent_struct = this;
        apo_save->func.user_callback = apo_save_response;

        #ifndef CLAPPLUG
        mode = add_my_mode_button(laframe, width-165, 0, 20, 20);
        mode->scale.gravity = ASPECT;
        mode->parent_struct = this;
        mode->func.value_changed_callback = set_mode;
        #endif
        #ifndef LV2PLUG
        Widget_t* ir_loader = add_my_lfile_button(laframe, 170, 20, 40, 40, "IR", " ", ".wav|.WAV");
        ir_loader->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        ir_loader->parent_struct = this;
        ir_loader->func.user_callback = ref_load_response;

        save = add_xsave_file_button(laframe, 170, 60, 40, 40, "Save IR", " ", ".wav|.WAV");
        save->parent_struct = this;
        save->func.user_callback = save_response;
        #endif

        hf_fade = add_my_fade_button(laframe, 220, 20, 40, 40);
        hf_fade->parent_struct = this;
        hf_fade->func.value_changed_callback = set_hf_fade;

        bp = add_my_bypass_button(laframe, 220, 60, 40, 40);
        bp->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        bp->parent_struct = this;
        bp->func.value_changed_callback = bp_response;

        dyn = add_my_dyn_button(laframe, 750, 40, 40, 40);
        dyn->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        dyn->parent_struct = this;
        dyn->func.value_changed_callback = dyn_response;

        smooth = add_my_knob(laframe, "Smooth", "", 620,26,60, 70);
        set_adjustment(smooth->adj, 0.3, 0.3, 0.0, 1.0, 0.01, CL_CONTINUOS);
        smooth->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(smooth, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        smooth->parent_struct = this;
        smooth->func.value_changed_callback = set_smooth;
        //smooth->func.button_release_callback = set;

        dynamics = add_my_knob(laframe, "Amount", "", 690,26,60, 70);
        set_adjustment(dynamics->adj, 1.0, 1.0, 0.1, 2.0, 0.01, CL_CONTINUOS);
        dynamics->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(dynamics, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        dynamics->parent_struct = this;
        dynamics->func.value_changed_callback = set_dynamics;
        //dynamics->func.button_release_callback = set;
/*
        tilt = add_my_knob(laframe, "Tone Bias", "", 730,26,60, 70);
        set_adjustment(tilt->adj, 0.0, 0.0, -1.0, 1.0, 0.01, CL_CONTINUOS);
        tilt->flags |= USE_TRANSPARENCY | FAST_REDRAW;
        set_widget_color(tilt, (Color_state)0, (Color_mod)0, 0.15, 0.52, 0.55, 1.0);
        tilt->parent_struct = this;
        tilt->func.value_changed_callback = set_tilt;
        tilt->func.button_release_callback = set;
*/
    }

    void set_controller_mode() {
        int c_mode = conn->getParameterValue(84);
        static int o_mode = 0;
        if (c_mode) {
            if (!o_mode) {
                show_ph = false;
                Metrics_t m;
                os_get_window_metrics(smooth, &m);
                if (m.visible) {
                    smooth_s = adj_get_value(smooth->adj);
                    dynamic_s = adj_get_value(dynamics->adj);
                }
                adj_set_value(smooth->adj, 0.0);
                adj_set_value(dynamics->adj, 1.0);
                xevfunc store = dyn->func.value_changed_callback;
                dyn->func.value_changed_callback = null_callback;
                adj_set_value(dyn->adj, 0.0);
                sendValueChanged(115, 0.0);
                dyn->func.value_changed_callback = store;
                widget_hide(ph);
                widget_hide(smooth);
                widget_hide(dynamics);
                widget_hide(dyn);
                widget_hide(hf_fade);
            }
        } else {
            adj_set_value(dyn->adj, dyn_s);
            widget_show(dyn);
            if ((int) conn->getParameterValue(115)) {
                adj_set_value(dynamics->adj, dynamic_s);
                adj_set_value(smooth->adj, smooth_s);
                widget_show(smooth);
                widget_show(dynamics);
            }
            widget_show(ph);
            widget_show(hf_fade);
        }
        if (!(int)conn->getParameterValue(115)) {
            Metrics_t m;
            os_get_window_metrics(smooth, &m);
            if (m.visible) {
                smooth_s = adj_get_value(smooth->adj);
                dynamic_s = adj_get_value(dynamics->adj);
            }
            adj_set_value(smooth->adj, 0.0);
            adj_set_value(dynamics->adj, 1.0);
            widget_hide(smooth);
            widget_hide(dynamics);
        }
        o_mode = c_mode;
    }

    void updateDbRange() {
        sendValueChanged(113, zoom_step);
        const float t = (float)zoom_step / (float)ZOOM_STEPS;
        db_min = db_min_ * std::pow(-6.0 / db_min_, t);
        db_max = db_max_ * std::pow(6.0 / db_max_, t);
        spec_height = 0;
        rebuild_eq_layer = true;
        expose_widget(spec);
    }

    void show() {
        widget_show_all(top);
        raise_control_panel(active_panel);
        set_controller_mode();
        zoom_step = conn->getParameterValue(113);
        if (zoom_step) updateDbRange();
        if (!conn->getParameterValue(111)) widget_hide(gthrv);
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
        adj_set_value(vuinmeterL->adj, power2db(vuinmeterL, conn->getInMeterL()));
        adj_set_value(vuinmeterR->adj, power2db(vuinmeterR, conn->getInMeterR()));
        bool setRefresh = false;
        if (conn->checkNewInData()) {
            bin = conn->getInBins();
            magin_.clear();
            const float* m = conn->getInMagnitudes();
            for (int i = 0; i<bin; i++) {
                magin_.push_back(m[i]);
            }
            conn->clearInAna();
            setRefresh = true;
        }
        if (conn->checkNewData()) {
            bin = conn->getBins();
            mag_.clear();
            const float* m = conn->getMagnitudes();
            for (int i = 0; i<bin; i++) {
                mag_.push_back(m[i]);
            }
            conn->clearAna();
            setRefresh = true;
        }
        if (setRefresh) expose_widget(spec);
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
    APOReader reader;
    Vec ir_; // filter
    Vec phase_; // phase
    Vec mag_; // spectrum
    Vec magin_; // input spectrum

    char cfreq[64];
    char cgain[64];
    double sampleRate = 48000.0;
    bool threshold_match = false;
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
    bool dynamic_threshold = false;
    std::atomic<bool> set_leak {false};
    std::string apo_file;
    std::string ref_file;

    float smooth_s = 0.3f;
    float dynamic_s = 1.0f;
    float dyn_s = 0.0f;

    const float f_min = 20.0f;
    const float f_max = 20000.0f;
    float db_min = -72.0f;
    const float db_min_ = -72.0f;
    float db_max = 24.0f;
    const float db_max_ = 24.0f;

    static constexpr int   ZOOM_STEPS  = 12;

    static constexpr float THR_TILT_PIVOT_HZ = 1000.0f;
    static constexpr float THR_TILT_STEP = 0.5f;
    static constexpr float THR_TILT_MAX  = 12.0f;

    struct BandCurveLUTs {
        std::vector<double> freq;
        std::vector<std::complex<double>> zInv, zInv2, alpha;
    };

    static void save_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if(user_data !=NULL) {
            std::string ir_file = *(const char**)user_data;
            std::pair<std::vector<double>, std::vector<double> > ir = self->conn->get_ir();        
            self->af.saveAudioFile(ir_file, ir.first, ir.second, self->sampleRate);
        }
    }

    static void ref_load_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if(user_data !=NULL) {
            self->ref_file = *(const char**)user_data;
            if ( self->af.getAudioFile(self->ref_file, self->sampleRate) ) {
                self->dstL.clear();
                self->dstR.clear();
                self->dstL.assign(self->af.samplesL.begin(), self->af.samplesL.end());
                self->dstR.assign(self->af.samplesR.begin(), self->af.samplesR.end());
                const std::vector<double> ref = self->conn->getRef(self->dstL, self->dstR, self->sampleRate);
                IRtoEQ er;
                IRtoEQ::EQSettings cfg = er.extractEQSettings(ref,self->sampleRate); 
                for (int i = 0; i < FilterTypes::NumFilters; ++i) {
                    adj_set_value(self->fenable[i]->adj, (float)cfg.enabled[i]);
                    //adj_set_value(self->ftype[i]->adj,   (float)cfg.type);
                    adj_set_value(self->mute[i]->adj,    0.0f);
                    adj_set_value(self->freq[i]->adj,    (float)cfg.freq[i]);
                    adj_set_value(self->fgain[i]->adj,   (float)cfg.gain[i]);
                    adj_set_value(self->fq[i]->adj,      (float)cfg.Q[i]);
                }

                adj_set_value(self->lowcut->adj,      (float)cfg.lowCut);
                adj_set_value(self->highcut->adj,     (float)cfg.highCut);
                
            }
        }
    }

    // load a APO EQ config file
    static void apo_load_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if(user_data !=NULL) {
            self->apo_file = *(const char**)user_data;
            if (!self->apo_file.empty() ) {
                if (self->reader.loadFile(self->apo_file)) {
                    for (int i = 0; i< FilterTypes::NumFilters; i++) {
                        adj_set_value(self->fenable[i]->adj, (float)self->reader.bands()[i].enabled);
                        adj_set_value(self->ftype[i]->adj,   (float)self->reader.bands()[i].type);
                        adj_set_value(self->mute[i]->adj,    0.0f);
                        adj_set_value(self->freq[i]->adj,    (float)self->reader.bands()[i].freq);
                        adj_set_value(self->fgain[i]->adj,   (float)self->reader.bands()[i].gain);
                        adj_set_value(self->fq[i]->adj,      (float)self->reader.bands()[i].Q);
                    }

                    adj_set_value(self->vug->adj,         (float)self->reader.preampDb());

                    adj_set_value(self->lowcut->adj,      (float)self->reader.lowCut().enabled ?
                                                                 self->reader.lowCut().freqHz : 19.0);
                    adj_set_value(self->highcut->adj,     (float)self->reader.highCut().enabled ?
                                                                 self->reader.highCut().freqHz : 22000.0);
                }
                //std::cout << self->apo_file << std::endl;
            }
        }
    }

    // save EQ settings as APO EQ config file
    static void apo_save_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if (user_data != NULL) {
            self->apo_file = *(const char**)user_data;
            if (!self->apo_file.empty()) {
                std::array<Band, FilterTypes::NumFilters> bandsOut;
                for (int i = 0; i < FilterTypes::NumFilters; i++) {
                    bandsOut[i].enabled = (int)adj_get_value(self->fenable[i]->adj);
                    bandsOut[i].type    = (Band::Type)(int)adj_get_value(self->ftype[i]->adj);
                    bandsOut[i].freq    = adj_get_value(self->freq[i]->adj);
                    bandsOut[i].gain    = adj_get_value(self->fgain[i]->adj);
                    bandsOut[i].Q       = adj_get_value(self->fq[i]->adj);
                    bandsOut[i].mute    = (int)adj_get_value(self->mute[i]->adj);
                }
                self->reader.saveFile(self->apo_file, bandsOut, adj_get_value(self->lowcut->adj),
                                adj_get_value(self->highcut->adj), adj_get_value(self->vug->adj));
            }
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

    static void set_mode(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(84, adj_get_value(w->adj));
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
        if (key->keycode == XKeysymToKeycode(w->app->dpy, XK_plus)) {
            self->zoom_step = std::min<int>(ZOOM_STEPS, self->zoom_step + 1);
            self->updateDbRange();
        } else if (key->keycode == XKeysymToKeycode(w->app->dpy, XK_minus)) {
            self->zoom_step = std::max<int>(0, self->zoom_step - 1);
            self->updateDbRange();
        }
    }

    static void release_key(void *w_, void *key_, void *user_data) {
        Widget_t *w = (Widget_t*)w_;
        if (!w) return;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        XKeyEvent *key = (XKeyEvent*)key_;
        if (key->state & ShiftMask) {
            self->dynamic_threshold = false;
        }
    }

    static void set_gain(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(82, adj_get_value(w->adj));
    }

    static void set_ingain(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(109, adj_get_value(w->adj));
    }

    static void set_side(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(110, adj_get_value(w->adj));
    }

    static void set_global_threshold(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        adj_get_value(w->adj) ? widget_show(self->gthrv) : widget_hide(self->gthrv);
        self->sendValueChanged(111, adj_get_value(w->adj));
    }

    static void set_global_threshold_value(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(112, adj_get_value(w->adj));
    }

    static void set_hf_fade(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(83, adj_get_value(w->adj));
    }

    static void bp_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(0, adj_get_value(w->adj));
    }

    void set_radio(Widget_t* w, bool set) {
         for (int i = 0; i<FilterTypes::NumFilters; i++) {
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
        for(int i = 0; i < FilterTypes::NumFilters; i++) {
            widget_hide(frame[i]);
            if (i == a) {
                active_panel = a;
                widget_show_all(frame[i]);
                //os_raise_widget(frame[i]);
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
        int r = std::min<int>(11, w->data + 1);
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

    static void set_threshold(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(85 + w->data, adj_get_value(w->adj));
    }

    static void set_ratio(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(97 + w->data, (int) adj_get_value(w->adj));
    }

    static void set_comp_exp_mode(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->sendValueChanged(116 + w->data, adj_get_value(w->adj));
    }

    static void set_lowcut(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        float v = adj_get_value(w->adj);
        if (v > 20.0f * 1.02f) self->sendValueChanged(75, 1.0f); // 2% reserve
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
        if (v < 20000.0f * 0.98f) self->sendValueChanged(77, 1.0f); // 2 % reserve
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

    static void dyn_response(void *w_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        self->dyn_s = adj_get_value(w->adj);
        self->sendValueChanged(115, self->dyn_s);
        self->set_controller_mode();
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

    static void reset_spec(void *w_, void* /* xbutton_ */, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        if (w->flags & HAS_POINTER) {
            if (self->band_match) {
                adj_set_value(self->fgain[self->match_band]->adj, 0.0);
            }
        }
    }

    static void mouse_set_spec(void *w_, void *xbutton_, void* user_data) {
        Widget_t *w = (Widget_t*)w_;
        auto* self = static_cast<SpectrumViewer*>(w->parent_struct);
        XButtonEvent *xbutton = (XButtonEvent*)xbutton_;
        if (w->flags & HAS_POINTER) {
            if(xbutton->button == Button1) {
                self->mx = xbutton->x;
                self->my = xbutton->y;
            } else if(xbutton->button == Button3) {
                if (self->band_match) {
                    int v = (int)adj_get_value(self->fenable[self->match_band]->adj);
                    adj_set_value(self->fenable[self->match_band]->adj, v ? 0.0 : 1.0);
                }
            } if(xbutton->state & ShiftMask) {
                self->dynamic_threshold = true;
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
        static const float band_min[FilterTypes::NumFilters] = {
            20.0f, 40.0f, 70.0f, 120.0f, 200.0f, 350.0f,
            650.0f, 1100.0f, 1800.0f, 3500.0f, 6000.0f, 10000.0f
        };
        static const float band_max[FilterTypes::NumFilters] = {
            60.0f, 100.0f, 180.0f, 300.0f, 550.0f, 900.0f,
            1600.0f, 2800.0f, 5000.0f, 9000.0f, 15000.0f, 20000.0f
        };

        for (int i = 0; i < FilterTypes::NumFilters; ++i) {
            if (target_freq >= band_min[i] && target_freq <= band_max[i])
                return i;
        }
        int best = 0;
        float best_dist = 1e9f;
        for (int i = 0; i < FilterTypes::NumFilters; ++i) {
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

        Metrics_t m;
        os_get_window_metrics(w, &m);
        const int width  = m.width;
        const int height = m.height - (80 * w->app->hdpi);
        const float px_to_db = (self->db_max - self->db_min) / (float)height;
        //static constexpr float SNAP_PX = 1.0f;

        if ((xmotion->state & (Button1Mask | ControlMask)) == (Button1Mask | ControlMask)) {
            float target_freq = x_to_freq(x1, self->f_min, self->f_max, width);
            float target_gain = y_to_db(y1, self->db_min, self->db_max, height);

            int band = self->find_band_for_freq(target_freq);

            target_gain = std::clamp(target_gain, -48.0f, 24.0f);
            adj_set_value(self->fgain[band]->adj, target_gain);
            self->sendValueChanged(5 + (6 * band), target_gain);

            self->rebuild_eq_layer = true;

            self->mx = x1;
            self->my = y1;
        } else if(xmotion->state & Button1Mask) {
            self->match_state = 1;
            if (self->band_match) {
                if (self->dynamic_threshold) {
                    float vg = adj_get_value(self->threshold[self->match_band]->adj);
                    float deltay = (float)y1 - self->my;
                    vg += deltay * -px_to_db;
                    self->my = y1;
                    //if (std::abs(vg) < SNAP_PX * px_to_db) vg = 0.0;
                    adj_set_value(self->threshold[self->match_band]->adj, vg);

                } else {
                    float v = adj_get_value(self->freq[self->match_band]->adj);
                    float deltaX = (float)x1 - self->mx;
                    v *= std::pow(2.0, deltaX * 0.005);
                    self->mx = x1;
                    adj_set_value(self->freq[self->match_band]->adj, v);

                    float vg = adj_get_value(self->fgain[self->match_band]->adj);
                    float deltay = (float)y1 - self->my;
                    vg += deltay * -px_to_db;
                    self->my = y1;
                    //if (std::abs(vg) < SNAP_PX * px_to_db) vg = 0.0;
                    adj_set_value(self->fgain[self->match_band]->adj, vg);
                }
            } else if (self->threshold_match) {
                const float pxt_to_db = (self->db_max_ - self->db_min_) / (float)height;
                float vg = adj_get_value(self->gthrv->adj);
                float deltay = (float)y1 - self->my;
                vg += deltay * -pxt_to_db;
                self->my = y1;
                //if (std::abs(vg) < SNAP_PX * px_to_db) vg = 0.0;
                adj_set_value(self->gthrv->adj, vg);
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
                    // expose_widget(self->spec);
                } else if(xbutton->button == Button5) {
                    float vq = adj_get_value(self->fq[self->match_band]->adj);
                    vq *= std::pow(2.0, -0.1);
                    adj_set_value(self->fq[self->match_band]->adj,vq);
                    // expose_widget(self->spec);
                }
            } else if (self->threshold_match) {
                if (xbutton->button == Button4) {
                    self->threshold_tilt = std::clamp(self->threshold_tilt + THR_TILT_STEP,
                                                       -THR_TILT_MAX, THR_TILT_MAX);
                } else if (xbutton->button == Button5) {
                    self->threshold_tilt = std::clamp(self->threshold_tilt - THR_TILT_STEP,
                                                       -THR_TILT_MAX, THR_TILT_MAX);
                }
                self->sendValueChanged(114, self->threshold_tilt);
                expose_widget(self->spec);
            } else {
                if(xbutton->button == Button4) {
                    self->zoom_step = std::max<int>(0, self->zoom_step - 1);
                    self->updateDbRange();
                } else if(xbutton->button == Button5) {
                    self->zoom_step = std::min<int>(ZOOM_STEPS, self->zoom_step + 1);
                    self->updateDbRange();
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

    static float y_to_db(float y, float db_min, float db_max, int height) {
        float norm = 1.0f - (y / height);
        norm = clampf(norm, 0.0f, 1.0f);
        return db_min + norm * (db_max - db_min);
    }

    static float freq_to_x(float freq, float f_min, float f_max, int width) {
        const float x_pad = 3.0f;
        const float inv_log_range = 1.0f / log10f(f_max / f_min);

        freq = std::clamp(freq, f_min, f_max);

        float norm = log10f(freq / f_min) * inv_log_range;
        return x_pad + norm * (width - 2.0f * x_pad);
    }

    static inline double x_to_freq(double x, double f_min, double f_max, int width) {
        double t = x / (double)width;
        // log interpolation
        double log_min = std::log(f_min);
        double log_max = std::log(f_max);
        double log_f = log_min + t * (log_max - log_min);
        return std::exp(log_f);
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

    inline float hermiteLookup(const Vec& v, float index) {
        int size = static_cast<int>(v.size());
        int i1 = std::clamp((int)index, 0, size - 1);
        int i0 = std::max<int>(i1 - 1, 0);
        int i2 = std::min<int>(i1 + 1, size - 1);
        int i3 = std::min<int>(i1 + 2, size - 1);

        float t = index - i1;
        float p0 = v[i0];
        float p1 = v[i1];
        float p2 = v[i2];
        float p3 = v[i3];

        // Catmull-Rom (Hermite)
        float c0 = p1;
        float c1 = 0.5f * (p2 - p0);
        float c2 = p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
        float c3 = 0.5f*(p3 - p0) + 1.5f*(p1 - p2);

        return ((c3*t + c2)*t + c1)*t + c0;
    }

    static double mapQ(double q_ui) {
        return std::clamp(q_ui, 0.1, 10.0);
    }

    static double mapQp(double q_ui) {
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

    static double eval_band_db(IConnector* conn, bool dy, int i, double f, double sr) {
        if (f < 10.0) return 0.0;
        double freq = conn->getParameterValue(i * 6 + 4);
        double gain = conn->getParameterValue(i * 6 + 5);
        double q = conn->getParameterValue(i * 6 + 6);
        int type = conn->getParameterValue(i * 6 + 2);
        double x = std::log2((f + 1e-9) / (freq + 1e-9));
        double Q = mapQ(q);
        double dyn = 0.0;
        if (dy) dyn = conn->getDynamics(i);

        switch (type) {
            case 0: { // Band::LowShelf
                double slope = Q * 2.0;
                double g = 0.5 * (1.0 - std::tanh(slope * x));
                return (gain + dyn) * g;
            }
            case 1: { // Band::Peak
                double sigma = 1.0 / (1.5 * Q + 0.5);
                double g = std::exp(-0.5 * (x * x) / (sigma * sigma));
                return (gain + dyn) * g;
            }
            case 2: { // Band::HighShelf
                double slope = Q * 2.0;
                double g = 0.5 * (1.0 + std::tanh(slope * x));
                return (gain + dyn) * g;
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
        for (int i = 0; i < FilterTypes::NumFilters; ++i) {
            float x = freq_to_x(conn->getParameterValue(i * 6 + 4), f_min, f_max, width);
            float y = db_to_y(conn->getParameterValue(i * 6 + 5), db_min, db_max, height);

            float dx = mx - x;
            float dy = my - y;

            if (dx*dx + dy*dy < 12*12) {
                if (match_band != i) {
                    raise_control_panel(i);
                }
                band_match = true;
                match_band = i;
                rebuild_eq_layer = true;
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
        if (conn->getParameterValue(111)) {
            float y = db_to_y(conn->getParameterValue(112), db_min_, db_max_, height);
            float dy = my - y;
            if (dy * dy < 12*12) {
                threshold_match = true;
                return;
            }
            threshold_match = false;
        }
    }

    // draw band curves
    BandCurveLUTs build_frequency_luts(int width, int steps, int model) {
        BandCurveLUTs lut;
        lut.freq.resize(steps);
        if (model == 1) { lut.zInv.resize(steps); lut.zInv2.resize(steps); }
        if (model == 2) { lut.alpha.resize(steps); }

        for (int x = 0; x < steps; ++x) {
            lut.freq[x] = x_to_freq(x, f_min, f_max, width);
            const double w = 2.0 * M_PI * lut.freq[x] / sampleRate;
            if (model == 1) {
                std::complex<double> zInv = std::polar(1.0, -w);
                lut.zInv[x]  = zInv;
                lut.zInv2[x] = zInv * zInv;
            } else if (model == 2) {
                std::complex<double> z = std::polar(1.0, w);
                lut.alpha[x] = 2.0 / (z + 1.0);
            }
        }
        return lut;
    }

    template <typename DbFunc>
    double findEdgeOctaves(DbFunc dbAt, double epsilon, double startOct, double sign) {
        double lo = 0.0, hi = startOct;
        while (std::fabs(dbAt(sign * hi)) > epsilon && hi < 12.0) hi *= 2.0;
        hi = std::min<double>(hi, 12.0);
        for (int iter = 0; iter < 20; ++iter) {
            double mid = 0.5 * (lo + hi);
            if (std::fabs(dbAt(sign * mid)) > epsilon) lo = mid; else hi = mid;
        }
        return sign * hi;
    }

    struct BandFilterModel {
        int model = 0, type = 0;
        double bandFreq = 0, gd = 0, slope = 0, invSig2 = 0;
        BiquadResponse::Coeffs biquad;
        SVFResponse::Coeffs    svf;

        double dbAtFreq(double f, double sampleRate) const {
            switch (model) {
                case 1:  return BiquadResponse::responseDB(biquad, f, sampleRate);
                case 2:  return SVFResponse::responseDB(svf, f, sampleRate);
                default: {
                    double xl = std::log2((f + 1e-9) / (bandFreq + 1e-9));
                    if (type == 0) return gd * 0.5 * (1.0 - std::tanh(slope * xl));
                    if (type == 2) return gd * 0.5 * (1.0 + std::tanh(slope * xl));
                    return gd * std::exp(-invSig2 * xl * xl);
                }
            }
        }

        double dbAtLUT(int x, double f, const BandCurveLUTs& lut, double sampleRate) const {
            if (f < 10.0) return 0.0;
            switch (model) {
                case 1:  return BiquadResponse::responseDbAtZ(biquad, lut.zInv[x], lut.zInv2[x]);
                case 2:  return SVFResponse::responseDbAtAlpha(svf, lut.alpha[x]);
                default: {
                    double xl = std::log2((f + 1e-9) / (bandFreq + 1e-9));
                    if (type == 0) return gd * 0.5 * (1.0 - std::tanh(slope * xl));
                    if (type == 2) return gd * 0.5 * (1.0 + std::tanh(slope * xl));
                    return gd * std::exp(-invSig2 * xl * xl);
                }
            }
        }
    };

    BandFilterModel build_band_model(const int model, int type, double bandFreq, double Q, double gd, double sampleRate) {
        BandFilterModel m;
        m.model = model;
        m.type = type;
        m.bandFreq = bandFreq;
        m.gd = gd;
        m.slope   = mapQp(Q) * 2.0;
        m.invSig2 = 0.5 / std::pow(1.0 / (1.5 * mapQp(Q) + 0.5), 2);

        if (model == 1) {
            const FilterTypes::Type ftype = type == 0 ? FilterTypes::Type::LowShelf
                                           : type == 2 ? FilterTypes::Type::HighShelf
                                                        : FilterTypes::Type::Peak;
            m.biquad = BiquadResponse::compute(ftype, bandFreq, Q, gd, sampleRate);
        } else if (model == 2) {
            m.svf = type == 0 ? SVFResponse::lowShelf(bandFreq, Q, gd, sampleRate)
                  : type == 2 ? SVFResponse::highShelf(bandFreq, Q, gd, sampleRate)
                               : SVFResponse::peak(bandFreq, Q, gd, sampleRate);
        }

        return m;
    }

    inline void compute_band_x_range(const BandFilterModel& m, double epsilon_db,
                double sampleRate, int width, int steps, int& xLo, int& xHi) {

        auto dbAt = [&](double xl) {
            return m.dbAtFreq(m.bandFreq * std::exp2(xl), sampleRate);
        };

        xLo = 0; xHi = steps - 1;
        if (m.type == 1) {
            double fLo = m.bandFreq * std::exp2(findEdgeOctaves(dbAt, epsilon_db, 1.0, -1.0));
            double fHi = m.bandFreq * std::exp2(findEdgeOctaves(dbAt, epsilon_db, 1.0,  1.0));
            xLo = std::max<double>(0, (int)std::floor(freq_to_x((float)fLo, f_min, f_max, width)) - 1);
            xHi = std::min<double>(steps - 1, (int)std::ceil(freq_to_x((float)fHi, f_min, f_max, width)) + 1);
        } else if (m.type == 0) {
            double fHi = m.bandFreq * std::exp2(findEdgeOctaves(dbAt, epsilon_db, 1.0, 1.0));
            xHi = std::min<double>(steps - 1, (int)std::ceil(freq_to_x((float)fHi, f_min, f_max, width)) + 1);
        } else {
            double fLo = m.bandFreq * std::exp2(findEdgeOctaves(dbAt, epsilon_db, 1.0, -1.0));
            xLo = std::max<double>(0, (int)std::floor(freq_to_x((float)fLo, f_min, f_max, width)) - 1);
        }
    }

    inline void build_band_path(cairo_t* cr, const BandFilterModel& m, const BandCurveLUTs& lut,
                    int xLo, int xHi, float y0, int height, double& startX, double& stopX) {
        bool isStarted = false;
        double lastX = xLo;
        startX = xLo; stopX = xLo;

        cairo_new_path(cr);
        for (int x = xLo; x <= xHi; ++x) {
            double f  = lut.freq[x];
            double db = m.dbAtLUT(x, f, lut, sampleRate);
            double y  = db_to_y(db, db_min, db_max, height);
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
    }

    inline void stroke_band_fill_glow_line(cairo_t* cr, double startX, double stopX,
                                    float y0, double r, double g, double bcol) {
        cairo_line_to(cr, stopX, y0);
        cairo_line_to(cr, startX, y0);
        cairo_close_path(cr);

        cairo_set_source_rgba(cr, r, g, bcol, t.band_fill_alpha);
        cairo_fill_preserve(cr);

        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_pattern_t* glow = cairo_pattern_create_linear(startX, 0, stopX, 0);
        cairo_pattern_add_color_stop_rgba(glow, 0,   r, g, bcol, t.band_glow_alpha * 0.1);
        cairo_pattern_add_color_stop_rgba(glow, 0.5, r, g, bcol, t.band_glow_alpha * 0.8);
        cairo_pattern_add_color_stop_rgba(glow, 1,   r, g, bcol, t.band_glow_alpha * 0.1);
        for (int k = 0; k < 3; ++k) {
            cairo_set_line_width(cr, 6.0 + k * 4.0);
            cairo_set_source(cr, glow);
            cairo_stroke_preserve(cr);
        }
        cairo_pattern_destroy(glow);

        cairo_pattern_t* grad = cairo_pattern_create_linear(startX, 0, stopX, 0);
        cairo_pattern_add_color_stop_rgba(grad, 0,   r, g, bcol, t.band_line_alpha * 0.1);
        cairo_pattern_add_color_stop_rgba(grad, 0.5, r, g, bcol, t.band_line_alpha * 1.0);
        cairo_pattern_add_color_stop_rgba(grad, 1,   r, g, bcol, t.band_line_alpha * 0.1);
        cairo_set_line_width(cr, 1.5);
        cairo_set_source(cr, grad);
        cairo_stroke(cr);
        cairo_pattern_destroy(grad);
    }

    void draw_band_curves(cairo_t* cr, bool dyn, int width, int height) {
        const int MODEL = dyn ? 1 : (int)conn->getParameterValue(84); // dynamic is always Biquad
        const int STEPS = width;
        const float y0 = db_to_y(0.0, db_min, db_max, height);
        const double epsilon_db = 0.5 * (db_max - db_min) / (double)height;

        BandCurveLUTs lut = build_frequency_luts(width, STEPS, MODEL);

        for (int i = 0; i < FilterTypes::NumFilters; ++i) {
            if (!(int)conn->getParameterValue(i * 6 + 1)) continue;

            double bandFreq = conn->getParameterValue(i * 6 + 4);
            double gain     = conn->getParameterValue(i * 6 + 5);
            double q        = conn->getParameterValue(i * 6 + 6);
            int    type     = (int)conn->getParameterValue(i * 6 + 2);

            double d = 0.0;
            if (dyn) {
                d = conn->getDynamics(i);
                if (d == 0.0) continue;
            }

            double Q  = mapQ(q);
            double gd = dyn ? d : gain;
            if (std::fabs(gd) <= epsilon_db) continue;

            BandFilterModel model = build_band_model(MODEL, type, bandFreq, Q, gd, sampleRate);

            int xLo, xHi;
            compute_band_x_range(model, epsilon_db, sampleRate, width, STEPS, xLo, xHi);

            double startX, stopX;
            build_band_path(cr, model, lut, xLo, xHi, y0, height, startX, stopX);

            double r, g, bcol;
            get_band_color(i, r, g, bcol);
            stroke_band_fill_glow_line(cr, startX, stopX, y0, r, g, bcol);
        }
    }

    void draw_band_points(cairo_t* cr, const int width, const int height) {
        for(int i = 0; i<FilterTypes::NumFilters; i++) {
            double r,g,bcol;
            get_band_color(i, r, g, bcol);

            int on = conn->getParameterValue(i * 6 + 1);
            float db = db_to_y(conn->getParameterValue(i * 6 + 5), db_min, db_max, height);
            float freq = freq_to_x(conn->getParameterValue(i * 6 + 4), f_min, f_max, width);
            cairo_set_source_rgba(cr, r, g, bcol, on ? 1.0 : 0.5);

            cairo_set_line_width(cr, 10.0);
            cairo_move_to(cr, freq, db);
            cairo_line_to(cr, freq, db);
            cairo_stroke(cr);

            if (on) {
                if (band_match && (match_band == i)) {
                    draw_band_ring(cr, freq, db, i, match_state);
                }
            }
        }
    }

    void draw_threshold_line(cairo_t* cr, const int width, const int height) {
        double gt = conn->getParameterValue(112);
        double y = db_to_y(gt, db_min_, db_max_, height);
        cairo_set_source_rgba(cr, 0.95, 0.65, 0.15, 0.7);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
        cairo_stroke(cr);

        if (threshold_tilt != 0.0f) {
            bool started = false;
            for (int px = 0; px < width; ++px) {
                float freq = x_to_freq((float)px, f_min, f_max, (float)width);
                float eff = std::clamp((float)gt + threshold_tilt * std::log2(freq / THR_TILT_PIVOT_HZ), db_min_, db_max_);
                float ey = db_to_y(eff, db_min_, db_max_, height);
                started ? cairo_line_to(cr, px, ey) : cairo_move_to(cr, px, ey);
                started = true;
            }
            cairo_set_source_rgba(cr, 0.98, 0.55, 0.1, 0.55);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke_preserve(cr);

            cairo_line_to(cr, width, y);
            cairo_line_to(cr, 0, y);
            cairo_close_path(cr);
            cairo_set_source_rgba(cr, 0.95, 0.6, 0.1, 0.1);
            cairo_fill(cr);
        }

        if (threshold_match) {
            cairo_set_source_rgba(cr, 0.95, 0.65, 0.15, 0.2);
            cairo_set_line_width(cr, 7.0);
            cairo_move_to(cr, 0, y);
            cairo_line_to(cr, width, y);
            cairo_stroke(cr);
        }
    }

    void create_background(Widget_t *w, const int width, const int height) {
        std::vector<double> freqs = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
        std::vector<double> minor_freqs = {30, 40, 60, 70, 80, 90, 300, 400, 600, 700, 800,
                                                    900, 3000, 4000, 6000, 7000, 8000, 9000};
        std::vector<double> dbs = { -48, -24, -18, -12, -6, -3, -2, -1, 0, 1, 2, 3, 6, 12, 18, 24};

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
        surface_set_font_from_ttf(cr, LDVAR_FONT(RobotoCondensedRegular_ttf),
                                      LDLEN_FONT(RobotoCondensedRegular_ttf));
        cairo_set_font_size(cr, 11);
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
                major ? 0.5 : 0.4
            );

            cairo_set_line_width(cr, major ? 1.5 : 1.0);
            cairo_move_to(cr, x, 0);
            cairo_line_to(cr, x, height);
            cairo_stroke(cr);
        }
        for (double f : minor_freqs) {
            double x = freq_to_x(f, f_min, f_max, width);
            cairo_set_source_rgba(
                cr, t.grid_minor_r, t.grid_minor_g, t.grid_minor_b, 0.15 );

            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, x, 0);
            cairo_line_to(cr, x, height);
            cairo_stroke(cr);
        }

        // dB lines
        for (double db : dbs) {
            double y = db_to_y(db, db_min, db_max, height);
            bool major = (db == 0);
            bool minor = (db == 3 || db == -3);
            bool minor_minor = (db < 3 && db > -3 && db != 0);
            cairo_set_source_rgba(
                cr,
                major ? t.grid_major_r : t.grid_minor_r,
                major ? t.grid_major_g : t.grid_minor_g,
                major ? t.grid_major_b : t.grid_minor_b,
                major ? 0.6 : 0.4
            );

            cairo_set_line_width(cr, major ? 1.5 : 1.0);
            if (minor && db_min < -36.0) continue;
            if (minor_minor && db_min < -9.6) continue;
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

        // 0 dB line
        double y = db_to_y(0, db_min_, db_max_, height);
        cairo_set_source_rgba(cr, 0.75, 0.2, 0.9, 0.4);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
        cairo_stroke(cr);

        // dB labels
        for (double db : dbs) {
            double y = db_to_y(db, db_min, db_max, height);
            bool minor = (db == 3 || db == -3);
            bool minor_minor = (db < 3 && db > -3 && db != 0);
            char buf[16];
            sprintf(buf, "%.0f", db);
            cairo_set_source_rgba(cr, t.text_dim_r, t.text_dim_g, t.text_dim_b, 0.7);
            cairo_move_to(cr, 5, y - 2);
            if (y > height - 25) continue;
            if (minor && db_min < -36.0) continue;
            if (minor_minor && db_min < -9.6) continue;
            cairo_text_path (cr, buf);
            cairo_fill (cr);
        }

        // dB labels
        for (double db : dbs) {
            double y = db_to_y(db, db_min_, db_max_, height);
            if ((db == 3 || db == -3) && db != 0) continue;
            if (db < 3 && db > -3 && db != 0) continue;
            char buf[16];
            sprintf(buf, "%.0f", db);
            cairo_text_extents_t extents;
            cairo_text_extents(cr, buf, &extents);
            cairo_set_source_rgba(cr, t.text_dim_r, t.text_dim_g, t.text_dim_b, 0.4);
            cairo_move_to(cr, width-5-extents.width, y - 2);
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
        draw_band_curves(cr, false, width, height);
        if (show_ph) drawSpectrum(cr, phase_, width, height, db_min, db_max, 1, sample_rate, 0.945, 0.114, 0.192, "",        height-80);
        drawSpectrum(cr, ir_,    width, height, db_min, db_max, 2.5, sample_rate, 0.545, 0.914, 0.992, "",      height-80);
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

        draw_band_curves(cr, true, width, height);
        if (conn->getParameterValue(111)) draw_threshold_line(cr, width, height);
        drawSpectrum(cr, magin_, width, height, db_min_, db_max_, 1.5, sample_rate, 0.2, 0.75, 0.45, "", height-100, false, true);
        drawSpectrum(cr, mag_, width, height, db_min_, db_max_, 1.5, sample_rate, 0.45, 0.2, 0.75, "", height-100, false, true);
    }

    void drawSpectrum(cairo_t* cr, const Vec& mags, int width, int height, float dB_min, float dB_max,
                      double line_width, float sample_rate, float r, float g, float b, const char* label,
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

        for (int px = 0; px < width; ++px) {
            float freq = x_to_freq((float)px, f_min, f_max, (float)width);
            float bin = freq * fft_size / sample_rate;
            if (bin < 1.0f) continue;
            if (bin > bins - 3) break;
            float mag = hermiteLookup(mags, bin);
            float y = db_to_y(mag, dB_min, dB_max, height);
            if (!started) {
                cairo_move_to(cr, px, y);
                started = true;
            } else {
                cairo_line_to(cr, px, y);
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
            cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.35, 0.85, 0.55, 0.25);
            cairo_pattern_add_color_stop_rgba(pat, 1.0, r, g, b, 0.05);
            cairo_set_source(cr, pat);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);
        }
        cairo_stroke(cr);
    }
};
