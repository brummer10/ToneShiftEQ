# ToneShift-EQ12

<p align="center">
    <img src="https://github.com/brummer10/ToneShiftEQ/blob/main/ToneShiftEQ.png?raw=true" />
</p>

**ToneShiftEQ** is a modern 12-band parametric equalizer featuring both
a minimum-phase FFT convolution engine and a zero-latency biquad engine.
Each band can also operate as an independent dynamic equalizer with configurable Threshold and Ratio controls.
It is designed for transparent tone shaping, corrective equalization, mixing and mastering.

---

## Features

* 12 fully parametric filter bands
* Independent enable, mute and solo controls per band
* Adjustable frequency, gain and Q for every band
* High-pass and low-pass filters
* APO EQ profile import/export
* FFT/Biquad Mode switch
* HF Fade smooth high-frequency roll-off
* Tone Bias control for broad spectral balancing
* Smooth control for natural filter transitions
* Contrast control for adaptive processing
* Real-time spectrum display with phase overlay
* Interactive curve drawing with Ctrl + drag
* Stereo processing
* Low CPU consumption
* Real-time safe processing architecture
* 128 samples latency, host-compensated

---

## Processing Modes

FFT Mode

* Minimum-phase convolution
* 128 samples latency
* Supports Smooth, Contrast, Tone Bias, HF Fade and IR export
* Best suited for mixing and mastering

Biquad Mode

* Cascaded parametric biquad filters
* Zero latency
* Low CPU usage
* Ideal for recording and live monitoring

---

## Usage

Band control points can be moved with the mouse to adjust frequency and gain.
Use the mouse wheel over a control point to change the Q factor.

Hold Ctrl and drag the left mouse button across the spectrum display to draw
a gain curve directly. ToneShiftEQ automatically selects the nearest band
for each position.

ToneShiftEQ can also import impulse responses by approximating them with its
internal 12-band parametric EQ, allowing further manual refinement.

Equalizer APO configuration files can be imported and exported for easy
exchange with other EQ software.

Each EQ band optionally provides dynamic processing with independent
Threshold and Ratio controls, enabling frequency-selective compression
directly within the equalizer.

---

## Design Goals

ToneShiftEQ was developed with a focus on:

* Transparent, musically natural sound quality
* Minimum-phase response
* Precise frequency shaping
* Efficient CPU usage
* Intuitive visual workflow

The plugin is equally suited for detailed corrective equalization,
mastering applications, and creative sound design.

---

## Technical Overview

ToneShiftEQ provides two interchangeable processing engines.

The FFT engine generates a minimum-phase impulse response from the current EQ settings
and processes audio using partitioned FFT convolution. It also supports importing
existing impulse responses by approximating them with the internal EQ model.

The Biquad engine implements the same filter configuration as a cascade of
second-order IIR filters for true zero-latency operation.

Dynamic processing is performed independently for each band, allowing
frequency-selective compression using configurable Threshold and Ratio controls.

Both engines share the same user interface and parameter model,
allowing instant switching depending on the application.

---

## Plugin Formats

ToneShiftEQ is available as:

* Stand-alone application
* CLAP plugin
* LV2 plugin
* VST3 plugin

---

## Dependencies

ToneShiftEQ relies on a small set of widely available libraries:

* **X11** – windowing (Linux)
* **cairo** – UI rendering
* **libsndfile** – audio I/O
* **FFTW3** – spectral processing
* **jackd** – Stand-alone real-time audio

### Install (Debian/Ubuntu)

    sudo apt install libx11-dev libcairo2-dev libsndfile1-dev libfftw3-dev libjack-jackd2-dev

---

## Build all flavours

    git clone https://github.com/brummer10/ToneShiftEQ.git
    cd ToneShiftEQ
    git submodule init
    git submodule update
    make 
    sudo make install

## Build as Stand-alone application (jackd)

    make standalone
    sudo make install

## Build as CLAP plugin

    make clap
    make install

## Build as VST3 plugin

    make vst3
    make install

## Build as LV2 plugin

    make lv2
    make install

---

## License

BSD-3-Clause

---

## Philosophy

> If it sounds good, it is right.
