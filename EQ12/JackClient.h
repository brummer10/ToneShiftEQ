
/*
 * JackClient.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#pragma once

#include <jack/jack.h>
#include <jack/thread.h>

#include <cstdio>
#include <cstring>

#include "Engine.h"

class JackClient {
public:
    JackClient(Engine* engine_, SpectrumViewer* sw_) {
        engine = engine_;
        sw = sw_;
    }

    ~JackClient() {}

    bool start(const char* name = "toneshifteq") {
        client = jack_client_open(name, JackNoStartServer, nullptr);

        if (!client) {
            fprintf(stderr, "jack server not running?\n");
            sw->quitGui();
            return false;
        }

        in_port = jack_port_register(
            client, "in_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

        in_port1 = jack_port_register(
            client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

        side_port = jack_port_register(
            client, "side_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

        side_port1 = jack_port_register(
            client, "side_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

        out_port = jack_port_register(
            client, "out_0", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        out_port1 = jack_port_register(
            client, "out_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        jack_set_xrun_callback(client, xrunCallback, this);
        jack_set_sample_rate_callback(client, srateCallback, this);
        jack_set_buffer_size_callback(client, bufferSizeCallback, this);
        jack_set_process_callback(client, processCallback, this);
        jack_on_shutdown(client, shutdownCallback, this);

        if (jack_activate(client)) {
            fprintf(stderr, "cannot activate client\n");
            sw->quitGui();
            return false;
        }

        if (!jack_is_realtime(client)) {
            fprintf(stderr, "jack isn't running with realtime priority\n");
        } else {
            fprintf(stderr, "jack running with realtime priority\n");
        }

        runProcess = true;
        return true;
    }

    void stop() {
        runProcess = false;
        if (!client) return;

        if (in_port) {
            if (jack_port_connected(in_port)) {
                jack_port_disconnect(client, in_port);
            }
            jack_port_unregister(client, in_port);
            in_port = nullptr;
        }

        if (in_port1) {
            if (jack_port_connected(in_port1)) {
                jack_port_disconnect(client, in_port1);
            }
            jack_port_unregister(client, in_port1);
            in_port1 = nullptr;
        }

        if (side_port) {
            if (jack_port_connected(side_port)) {
                jack_port_disconnect(client, side_port);
            }
            jack_port_unregister(client, side_port);
            side_port = nullptr;
        }

        if (side_port1) {
            if (jack_port_connected(side_port1)) {
                jack_port_disconnect(client, side_port1);
            }
            jack_port_unregister(client, side_port1);
            side_port1 = nullptr;
        }

        if (out_port) {
            if (jack_port_connected(out_port)) {
                jack_port_disconnect(client, out_port);
            }
            jack_port_unregister(client, out_port);
            out_port = nullptr;
        }

        if (out_port1) {
            if (jack_port_connected(out_port1)) {
                jack_port_disconnect(client, out_port1);
            }
            jack_port_unregister(client, out_port1);
            out_port1 = nullptr;
        }

        jack_client_close(client);
        client = nullptr;
    }

private:
    Engine* engine          = nullptr;
    SpectrumViewer* sw      = nullptr;
    jack_client_t* client   = nullptr;
    jack_port_t* in_port    = nullptr;
    jack_port_t* in_port1   = nullptr;
    jack_port_t* side_port  = nullptr;
    jack_port_t* side_port1 = nullptr;
    jack_port_t* out_port   = nullptr;
    jack_port_t* out_port1  = nullptr;
    bool runProcess         = false;
    uint32_t frames         = 0;

private:
    // -------- Static Callbacks --------

    static void shutdownCallback(void* arg) {
        auto* self = static_cast<JackClient*>(arg);
        if (!self) return;
        self->runProcess = false;
        fprintf(stderr, "jack shutdown, exit now\n");
        self->sw->quitGui();
    }

    static int xrunCallback(void* arg) {
        fprintf(stderr, "Xrun\r");
        return 0;
    }

    static int srateCallback(jack_nframes_t samplerate, void* arg) {
        auto* self = static_cast<JackClient*>(arg);
        if (!self) return 0;
        int prio = jack_client_real_time_priority(self->client);
        if (prio < 0) prio = 25;
        fprintf(stderr, "Samplerate %u Hz\n", samplerate);
        self->sw->setSampleRate((int)samplerate);
        self->engine->init(samplerate, prio, 1);
        return 0;
    }

    static int bufferSizeCallback(jack_nframes_t nframes, void* arg) {
        fprintf(stderr, "Buffersize is %u samples\n", nframes);
        return 0;
    }

    static int processCallback(jack_nframes_t nframes, void* arg) {
        auto* self = static_cast<JackClient*>(arg);
        if (!self || !self->runProcess) return 0;

        float* input = static_cast<float*>(
            jack_port_get_buffer(self->in_port, nframes));

        float* input1 = static_cast<float*>(
            jack_port_get_buffer(self->in_port1, nframes));

        float* sideput = static_cast<float*>(
            jack_port_get_buffer(self->side_port, nframes));

        float* sideput1 = static_cast<float*>(
            jack_port_get_buffer(self->side_port1, nframes));

        float* output = static_cast<float*>(
            jack_port_get_buffer(self->out_port, nframes));

        float* output1 = static_cast<float*>(
            jack_port_get_buffer(self->out_port1, nframes));

        if (output != input)
            memcpy(output, input, nframes * sizeof(float));

        if (output1 != input1)
            memcpy(output1, input1, nframes * sizeof(float));

        self->engine->process(nframes, sideput, sideput1, output, output1);

        return 0;
    }
};
