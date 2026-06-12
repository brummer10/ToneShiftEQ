/*
 * main.c
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


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


int main(int argc, char *argv[]){

    AudioFile af;
    FFTAnalyzer ana;
    IRProcessor ip;
    IRMorpherStereo conv;
    GainStereo vu;
    Engine engine(&ip, &conv, &ana, &vu);
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
        }

        main_quit(sw.getMain());
        jack.stop();
    }

    printf("bye bye\n");
    return 0;
}

