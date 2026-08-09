/*
 * ToneShiftEQ.cc
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */


#include <atomic>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <cmath>

#include <locale.h>

#include "FFTAnalyzer.h"
#include "IRProcessorStereo.h"
#include "IRMorpherStereo.h"
#include "GainStereo.h"
#include "Engine.h"
#include "ParallelThread.h"
#include "Parameter.h"
#include "EQConnector.h"
#define CLAPPLUG
#include "SpectrumViewer.h"

class ToneShiftEQ 
{
public:
    //Widget_t*               TopWin;
    Params*                 param;
    FFTAnalyzer             ana;
    FFTAnalyzer             anain;
    IRProcessor             ip;
    IRMorpherStereo         conv;
    GainStereo              vu;
    GainStereo              vuin;
    Engine                  engine;
    StandaloneConnector     conn;
    SpectrumViewer          sw;

    ToneShiftEQ() : ana(), anain(), ip(), conv(), vu(), vuin(),
                    engine(&ip, &conv, &ana, &anain, &vu, &vuin),
                                        conn(&engine), sw(&conn) {
        title = "ToneShiftEQ";
        firstLoop = true;
        p = 0;
        param = &engine.param;
    }

    ~ToneShiftEQ() {
        fetch.stop();
    }

    void startGui(Window window) {
        main_init(sw.getMain());
        #if defined(_WIN32)
        sw.top  = create_window(sw.getMain(), (HWND) window, 0, 0, 930, 430);
        sw.top->func.expose_callback = sw.draw_window;
        #else
        sw.top  = create_window(sw.getMain(), (Window) window, 0, 0, 930, 430);
        sw.top->func.expose_callback = sw.draw_window;
        #endif
        sw.top->flags |= HIDE_ON_DELETE;
        widget_set_title(sw.top, title.c_str());
        sw.create();
        fetch.startTimeout(16);
        fetch.set<ToneShiftEQ, &ToneShiftEQ::runGui>(this);
    }

    void startGui() {
        main_init(sw.getMain());
        sw.top  = create_window(sw.getMain(), os_get_root_window(sw.getMain(), IS_WINDOW), 0, 0, 930, 430);
        sw.top->func.expose_callback = sw.draw_window;
        sw.top->flags |= HIDE_ON_DELETE;
        sw.create();
        fetch.startTimeout(16);
        fetch.set<ToneShiftEQ, &ToneShiftEQ::runGui>(this);
    }

    void showGui() {
        //engine._notify_ui.store(true, std::memory_order_release);
        getEngineValues();
        widget_show_all(sw.top);
        sw.set_controller_mode();
        sw.zoom_step = (int)param->getParam(113);
        if (sw.zoom_step) sw.updateDbRange();
        sw.threshold_tilt = param->getParam(114);
        firstLoop = true;
    }
    
    void setParent(Window window) {
        #if defined(_WIN32)
        SetParent(sw.top->widget, (HWND) window);
        #else
        XReparentWindow(sw.getMain()->dpy, sw.top->widget, (Window) window, 0, 0);
        #endif
        p = window;
    }

    void checkParentWindowSize(int width, int height) {
        #if defined (IS_VST2)
        if (!p) return;
        int host_width = 1;
        int host_height = 1;
        #if defined(_WIN32)
        RECT rect;
        if (GetClientRect((HWND) p, &rect)) {
            host_width  = rect.right - rect.left;
            host_height = rect.bottom - rect.top;
        }
        #else
        XWindowAttributes attrs;
        if (XGetWindowAttributes(sw.getMain()->dpy, p, &attrs)) {
            host_width  = attrs.width;
            host_height = attrs.height;
        }
        #endif
        if ((host_width != width && host_width != 1) ||
            (host_height != height && host_height != 1)) {
            os_resize_window(sw.getMain()->dpy, sw.top, host_width, host_height);
        }
        #endif
    }

    void hideGui() {
        widget_hide(sw.top);
        firstLoop = false;
    }

    void quitGui() {
        cleanup();
        fetch.stop();
        sw.quitGui();
        main_quit(sw.getMain());
    }

    void runGui() {
        if (firstLoop) {
            checkParentWindowSize(sw.top->width, sw.top->height);
            firstLoop = false;
        }        
        sw.check_spec();
        os_run_embedded(sw.getMain());
        sw.check_irmatch();
    }

    Xputty *getMain() {
        return sw.getMain();
    }

    Engine *getEngine() {
        return &engine;
    }

    void initEngine(uint32_t rate, int32_t prio, int32_t policy) {
        engine.init(rate, prio, policy);
    }

    inline void process(uint32_t n_samples, float* input, float* input1, float* output, float* output1) {
        engine.process(n_samples, input, input1, output, output1);
    }

    void getLatency(uint32_t* latency) {
        (*latency) = 0;
    }

    void copyValuesToGui(Widget_t* wid, float value) {
        xevfunc store = wid->func.value_changed_callback;
        adj_set_value(wid->adj, value);
        wid->func.value_changed_callback = store;
    }

    void getEngineValues() {
        copyValuesToGui(sw.bp,         (float)param->getParam(0));
        
        for (int i = 0; i< 12; i++) {
            copyValuesToGui(sw.fenable[i], (float)param->getParam(i*6 +1));
            copyValuesToGui(sw.ftype[i],   (float)param->getParam(i*6 +2));
            copyValuesToGui(sw.mute[i],    (float)param->getParam(i*6 +3));
            copyValuesToGui(sw.freq[i],    (float)param->getParam(i*6 +4));
            copyValuesToGui(sw.fgain[i],   (float)param->getParam(i*6 +5));
            copyValuesToGui(sw.fq[i],      (float)param->getParam(i*6 +6));
        }

        if (param->getParam(74)) copyValuesToGui(sw.solo[(int)param->getParam(73)], 1.0);

        copyValuesToGui(sw.lowcut,     (float)param->getParam(76));
        copyValuesToGui(sw.highcut,    (float)param->getParam(78));

        copyValuesToGui(sw.smooth,     (float)param->getParam(79));
        copyValuesToGui(sw.dynamics,   (float)param->getParam(80));
        copyValuesToGui(sw.tilt,       (float)param->getParam(81));

        copyValuesToGui(sw.vug,        (float)param->getParam(82));
        copyValuesToGui(sw.hf_fade,    (float)param->getParam(83));

        for (int i = 0; i< 12; i++) {
            copyValuesToGui(sw.threshold[i], (float)param->getParam(85 + i));
        }
        for (int i = 0; i< 12; i++) {
            copyValuesToGui(sw.ratio[i], (float)param->getParam(97 + i));
        }
        copyValuesToGui(sw.vuing,       (float)param->getParam(109));
        copyValuesToGui(sw.side,        (float)param->getParam(110));
        copyValuesToGui(sw.gthr,        (float)param->getParam(111));
        copyValuesToGui(sw.gthrv,       (float)param->getParam(112));
    }


    float check_stod (const std::string& str) {
        char* point = localeconv()->decimal_point;
        if (std::string(".") != point) {
            std::string::size_type point_it = str.find(".");
            std::string temp_str = str;
            if (point_it != std::string::npos)
                temp_str.replace(point_it, 1, point);
            return std::stod(temp_str);
        } else return std::stod(str);
    }

    std::string remove_sub(std::string a, std::string b) {
        std::string::size_type fpos = a.find(b);
        if (fpos != std::string::npos )
            a.erase(a.begin() + fpos, a.begin() + fpos + b.length());
        return (a);
    }

    void readState(std::string _stream) {
        std::string stream = _stream;
        std::string line;
        std::string key;
        std::string value;
        std::size_t pos = _stream.find("|");
        while (pos != std::string::npos) {
            line = stream.substr(0, pos);
            std::istringstream buf(line);
            buf >> key;
            buf >> value;
            if (key.compare("[CONTROLS]") == 0) {
                for (int i = 0; i < param->getParamCount(); i++) {
                    param->setParam(i, check_stod(value));
                    param->setParamDirty(i, true);
                    buf >> value;
                    if (!buf) break;
                }
            }
            key.clear();
            value.clear();
            stream = stream.substr(pos+1);
            pos = stream.find("|");
            if (pos == std::string::npos) break;
        }
        param->controllerChanged.store(true, std::memory_order_release);
        getEngine()->processIR.store(true, std::memory_order_release);
        getEngine()->workToDo.store(true, std::memory_order_release);
    }

    void saveState(std::string *state) {
        std::ostringstream buffer; 
        buffer << "[CONTROLS] ";
        for (int i = 0; i < param->getParamCount(); i++) {
            buffer << param->getParam(i) << " ";
        }
        buffer << "|";
        (*state) = buffer.str();
    }

    void cleanup() {
        // Xputty free all memory used
        // main_quit(sw.getMain());
    }

private:
    ParallelThread          fetch;
    Window                  p;
    std::string             title;
    bool                    firstLoop;

};
