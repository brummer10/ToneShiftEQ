/*
 * main.c
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#include <atomic>
#include <cmath>
#include <vector>
#include <signal.h>
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <string>
#include <condition_variable>
#include <utility>

#include "AudioFile.h"
#include "FFTAnalyzer.h"
#include "IRProcessorStereo.h"
#include "IRMorpherStereo.h"
#include "GainStereo.h"
#include "Engine.h"
#include "EQConnector.h"


#include "SpectrumViewer.h"

#include "JackClient.h"


void copyValuesToGui(Widget_t* wid, float value) {
    xevfunc store = wid->func.value_changed_callback;
    adj_set_value(wid->adj, value);
    wid->func.value_changed_callback = store;
}

void getEngineValues(Params* param, SpectrumViewer* sw) {
    copyValuesToGui(sw->bp,         (float)param->getParam(0));
    
    for (int i = 0; i< 12; i++) {
        copyValuesToGui(sw->fenable[i], (float)param->getParam(i*6 +1));
        copyValuesToGui(sw->ftype[i],   (float)param->getParam(i*6 +2));
        copyValuesToGui(sw->mute[i],    (float)param->getParam(i*6 +3));
        copyValuesToGui(sw->freq[i],    (float)param->getParam(i*6 +4));
        copyValuesToGui(sw->fgain[i],   (float)param->getParam(i*6 +5));
        copyValuesToGui(sw->fq[i],      (float)param->getParam(i*6 +6));
    }

    if (param->getParam(74)) copyValuesToGui(sw->solo[(int)param->getParam(73)], 1.0);

    copyValuesToGui(sw->lowcut,     (float)param->getParam(76));
    copyValuesToGui(sw->highcut,    (float)param->getParam(78));

    copyValuesToGui(sw->smooth,     (float)param->getParam(79));
    copyValuesToGui(sw->dynamics,   (float)param->getParam(80));
    copyValuesToGui(sw->tilt,       (float)param->getParam(81));

    copyValuesToGui(sw->vug,        (float)param->getParam(82));
    copyValuesToGui(sw->hf_fade,    (float)param->getParam(83));

    for (int i = 0; i< 12; i++) {
        copyValuesToGui(sw->threshold[i], (float)param->getParam(85 + i));
    }
    for (int i = 0; i< 12; i++) {
        copyValuesToGui(sw->ratio[i],(float)param->getParam(97 + i));
    }
    copyValuesToGui(sw->vuing,       (float)param->getParam(109));
    copyValuesToGui(sw->side,        (float)param->getParam(110));
    copyValuesToGui(sw->gthr,        (float)param->getParam(111));
    copyValuesToGui(sw->gthrv,       (float)param->getParam(112));
}

int main(int argc, char *argv[]){

    AudioFile af;
    FFTAnalyzer ana;
    FFTAnalyzer anain;
    IRProcessor ip;
    IRMorpherStereo conv;
    GainStereo vu;
    GainStereo vuin;
    Engine engine(&ip, &conv, &ana, &anain, &vu, &vuin);
    StandaloneConnector conn(&engine);
    SpectrumViewer sw(&conn);
    JackClient jack(&engine, &sw);

    bool startUi = jack.start();

    if (startUi) {
        sw.init();
        sw.create();
        sw.show();
        Atom WM_DELETE_WINDOW = os_register_wm_delete_window(sw.top);
        sw.run = true;
        while (sw.run) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            XEvent xev;
            if (XCheckTypedWindowEvent(sw.getMain()->dpy, sw.top->widget, ClientMessage, &xev)){
                if (xev.xclient.data.l[0] == (long int)WM_DELETE_WINDOW) {
                    sw.quitGui();
                }
            }

            sw.check_spec();
            os_run_embedded(sw.getMain());
            sw.check_irmatch();
            if (engine.param.paramChanged.load(std::memory_order_acquire)) {
                getEngineValues(&engine.param, &sw);
                engine.param.paramChanged.store(false, std::memory_order_release);
            }
        }

        main_quit(sw.getMain());
        jack.stop();
    }

    printf("bye bye\n");
    return 0;
}

