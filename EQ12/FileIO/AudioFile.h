
/*
 * AudioFile.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2026 brummer <brummer@web.de>
 */

#pragma once

#ifndef AUDIOFILE_H
#define AUDIOFILE_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <sndfile.hh>

#include "CheckResample.h"

/****************************************************************
        class AudioFile - load a Audio File into buffer
                          and resample when needed
                          save a buffer to audio file
****************************************************************/

class AudioFile : public CheckResample {
public:
    uint32_t channels   = 0;
    uint32_t samplesize = 0;
    uint32_t samplerate = 0;

    std::vector<double> samplesL;
    std::vector<double> samplesR;
    std::vector<double> saveBuffer;

    AudioFile() = default;
    ~AudioFile() = default;

    inline bool deinterleave() {
        samplesL.clear();
        samplesR.clear();

        if (saveBuffer.empty())
            return false;

        const uint32_t bufferSize = (channels == 2) ? samplesize : samplesize;

        samplesL.resize(bufferSize);
        samplesR.resize(bufferSize);

        const uint32_t c = (channels == 2) ? 1 : 0;

        for (uint32_t i = 0; i < bufferSize; ++i) {
            samplesL[i] = saveBuffer[i * channels];
            samplesR[i] = saveBuffer[i * channels + c];
        }

        saveBuffer.clear();
        saveBuffer.shrink_to_fit();

        channels   = 2;
        samplesize = bufferSize;

        return true;
    }

    // load audio file into buffer
    inline bool getAudioFile( const std::string& file, const uint32_t expectedSampleRate = 0) {
        SF_INFO info {};
        info.format = 0;
        channels   = 0;
        samplesize = 0;
        samplerate = 0;

        saveBuffer.clear();

        SNDFILE* sndfile = sf_open(file.c_str(), SFM_READ, &info);

        if (!sndfile) {
            std::cerr << "Error: could not open file\n";
            return false;
        }

        if (info.channels > 2) {
            std::cerr << "Error: only mono/stereo supported\n";
            sf_close(sndfile);
            return false;
        }

        saveBuffer.resize(info.frames * info.channels);

        samplesize = static_cast<uint32_t>(sf_readf_double(sndfile, saveBuffer.data(), info.frames));

        if (!samplesize)
            samplesize = info.frames;

        channels   = info.channels;
        samplerate = info.samplerate;

        sf_close(sndfile);

        if (expectedSampleRate && samplerate != expectedSampleRate) {
            saveBuffer = checkSampleRate(samplerate, expectedSampleRate, channels, saveBuffer);
            samplerate = expectedSampleRate;
            samplesize = saveBuffer.size() / channels;
        }

        return deinterleave();
    }

    // save audio file from buffer
    void saveAudioFile(const std::string& name, const std::vector<double>& bufferL,
                    const std::vector<double>& bufferR, const uint32_t sampleRate) {
        const bool stereo = !bufferR.empty();

        const uint32_t frames = stereo ? std::min<uint32_t>(bufferL.size(), bufferR.size()) : bufferL.size();

        if (frames == 0) {
            std::cerr << "Error: empty buffer\n";
            return;
        }

        SF_INFO sfinfo {};
        sfinfo.channels   = stereo ? 2 : 1;
        sfinfo.samplerate = sampleRate;
        sfinfo.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

        SNDFILE* sf = sf_open(name.c_str(), SFM_WRITE, &sfinfo);

        if (!sf) {
            std::cerr << "Error: failed to open "
                      << name << std::endl;
            return;
        }

        if (stereo) {
            std::vector<double> interleaved(frames * 2);

            for (uint32_t i = 0; i < frames; ++i) {
                interleaved[i * 2]     = bufferL[i];
                interleaved[i * 2 + 1] = bufferR[i];
            }

            sf_writef_double(sf, interleaved.data(), frames);
        } else {
            sf_writef_double(sf, bufferL.data(), frames);
        }

        sf_write_sync(sf);
        sf_close(sf);
    }
};

#endif
