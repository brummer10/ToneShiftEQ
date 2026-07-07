# ToneShift-EQ12

<p align="center">
    <img src="https://github.com/brummer10/ToneShiftEQ/blob/main/ToneShiftEQ.png?raw=true" />
</p>

**ToneShiftEQ** is a modern 12-band parametric equalizer featuring both
a minimum-phase FFT convolution engine and a zero-latency biquad engine.
It is designed for transparent tone shaping, corrective equalization, mixing and mastering.

---

## Features

* 12 fully parametric filter bands
* Independent enable, mute and solo controls per band
* Adjustable frequency, gain and Q for every band
* High-pass and low-pass filters
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
and processes audio using partitioned FFT convolution.

The Biquad engine implements the same filter configuration as
a cascade of second-order IIR filters for true zero-latency operation.

Both engines share the same user interface and parameter model,
allowing instant switching depending on the application.

---

## Plugin Formats

ToneShiftEQ is available as:

* Stand-alone application
* CLAP plugin
* LV2 plugin

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

## Build

    git clone https://github.com/brummer10/ToneShiftEQ.git
    cd ToneShiftEQ
    git submodule init
    git submodule update
    make
    sudo make install

## Build as CLAP plugin

    make clap
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
