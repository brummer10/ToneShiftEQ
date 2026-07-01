
# ToneShift-EQ12

<p align="center">
    <img src="https://github.com/brummer10/ToneShiftEQ/blob/main/ToneShiftEQ.png?raw=true" />
</p>


**ToneShiftEQ** is a modern 12-band equalizer designed for precise spectral shaping, mixing, mastering, and corrective audio processing.

It can operate in two different modes:

* Master → Master → Perfect linear-phase response with 128 samples of latency, ideal for mastering and critical processing.
* Live → Zero-latency processing with slight phase deviation, optimized for real-time performance and live use.

---

## Features

* 12 fully parametric filter bands
* Independent enable, mute and solo controls per band
* Adjustable frequency, gain and Q for every band
* High-pass and low-pass filters
* Tilt control for broad spectral balancing
* Smooth control for natural filter transitions
* Dynamics control for adaptive processing
* Real-time spectrum display
* Stereo processing
* Low CPU consumption
* Real-time safe processing architecture

---

## Usage

Band control points can be moved with the mouse to adjust frequency and gain. Use the mouse wheel over a control point to change the Q factor.

---

## Design Goals

ToneShiftEQ was developed with a focus on:

* Transparent sound quality
* Precise frequency shaping
* Low latency operation
* Efficient CPU usage
* Intuitive visual workflow

The plugin is equally suited for detailed corrective equalization, mastering applications, and creative sound design.

---

## Technical Overview

ToneShiftEQ combines parametric filter design with a partitioned convolution engine.
Filter responses are generated and updated in the background while audio processing remains real-time safe.

---

## Plugin Formats

ToneShiftEQ is available as:

* Stand-alone
* CLAP
* LV2

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

## Build as Clap plugin

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
