# ToneShiftEQ12 — Technical Description

## Overview

ToneShiftEQ12 is a high-quality stereo parametric convolution equalizer
implemented as an LV2 and CLAP plugin for Linux. It combines a 12-band
minimum-phase EQ engine with a real-time partitioned convolution backend,
a spectrum analyzer, and an interactive graphical interface for direct
curve manipulation.

---

## Signal Processing Architecture

### Minimum-Phase IR Generation

Each EQ band contributes to a combined minimum-phase impulse response (IR) of
4096 samples. The IR is computed from the combined frequency response of all
active bands using a cepstrum-based minimum-phase conversion. This ensures
that all spectral energy is concentrated at the beginning of the IR, which
is the defining property of minimum-phase systems and the basis for the
efficiency of the convolution engine.

Minimum-phase behavior matches that of analog equalizers: the phase response
is frequency-dependent and coupled to the amplitude response, which is
the natural and musically appropriate characteristic for an equalizer.
This is distinct from linear-phase equalization, which introduces
pre-ringing and requires additional latency.

### Partitioned Convolution Engine

The EQ applies the IR to the audio signal using a partitioned overlap-add
(OLA) convolution engine implemented in C++ using FFTW3. The IR is split
into 32 partitions of 128 samples each, processed in the frequency domain
using FFT/IFFT pairs of size 256.

- **Latency**: 128 samples, reported to the host for automatic compensation
- **IR length**: 4096 samples (32 × 128)
- **FFT size**: 256 points (2 × partition size, for linear convolution)
- **Frequency resolution**: 172 Hz per bin at 44.1 kHz

### Real-Time Safe IR Swap

IR updates triggered by parameter changes are handled without interrupting
audio processing. A new IR is built on the UI thread and handed to the
audio thread via a lock-free atomic pointer exchange. The retired IR is
returned to the UI thread for deferred deletion, ensuring no memory
allocation or deallocation occurs on the real-time audio thread.

### Stereo Processing

Left and right channels are processed independently with separate convolution
state (FFT buffers, overlap-add accumulators, input/output FIFOs and history
buffers), sharing only the IR data which is read-only during processing.

---

## Equalizer Bands

ToneShiftEQ12 provides 12 parametric bands:

| Band | Center Freq  | Range            | Default Type |
|------|-------------|------------------|--------------|
| 0    | 40 Hz       | 20 – 60 Hz       | Low Shelf    |
| 1    | 70 Hz       | 40 – 100 Hz      | Peak         |
| 2    | 120 Hz      | 70 – 180 Hz      | Peak         |
| 3    | 210 Hz      | 120 – 300 Hz     | Peak         |
| 4    | 370 Hz      | 200 – 550 Hz     | Peak         |
| 5    | 650 Hz      | 350 – 900 Hz     | Peak         |
| 6    | 1150 Hz     | 650 – 1600 Hz    | Peak         |
| 7    | 2000 Hz     | 1100 – 2800 Hz   | Peak         |
| 8    | 3500 Hz     | 1800 – 5000 Hz   | Peak         |
| 9    | 6100 Hz     | 3500 – 9000 Hz   | Peak         |
| 10   | 10700 Hz    | 6000 – 15000 Hz  | Peak         |
| 11   | 18000 Hz    | 10000 – 20000 Hz | High Shelf   |

Each band provides:
- **Frequency** — logarithmic range as shown above
- **Gain** — ±48 dB
- **Q** — 0.4 to 10.0 (logarithmic)
- **Type** — Low Shelf, Peak, High Shelf
- **Enable / Mute / Solo** — per band

Additional global controls:
- **Low Cut / High Cut** — configurable high-pass and low-pass filters
- **Smooth** — spectrum display smoothing
- **Dynamics** — dynamic EQ response amount
- **Tone Bias** — global tilt filter
- **Volume Out** — output gain

---

## Graphical Interface

The GUI is built on a lightweight custom X11 widget toolkit (xputty) with
Cairo for all 2D rendering, without dependency on Qt, GTK or similar
frameworks.

### Spectrum Analyzer

A real-time FFT spectrum analyzer is displayed behind the EQ curve. Magnitude
data is passed from the audio engine to the GUI via a lock-free data path
and rendered as a filled curve overlay.

### EQ Curve Display

The EQ frequency response is rendered as a layered Cairo surface with:
- Per-band colored fill and glow curves
- Band control points as colored dot markers
- Phase response overlay (toggleable)
- Frequency and dB grid with labeled axes

### Interactive Band Control

- **Hover** — highlights the nearest band control point
- **Drag** — moves the highlighted band in frequency and gain simultaneously
- **Scroll wheel** — adjusts the Q of the highlighted band
- **Ctrl + Left Mouse Drag** — draws a gain curve directly with the mouse;
  the band whose frequency range covers the cursor position is set to the
  gain value at the cursor's vertical position. Also functions as a
  "jump to mouse position" for individual bands.

### HiDPI Support

The interface scales with the system DPI setting. Font sizes are dynamically
adjusted at render time to fit available widget dimensions, preventing label
clipping on high-resolution displays including 4K monitors with KDE
fractional scaling.

---

## Additional Features

- **IR Export** — the current EQ curve can be exported as a stereo WAV file
  containing the computed minimum-phase IR, for use in external convolution
  processors
- **IR Import** — an external IR file can be loaded and converted to a
  minimum-phase EQ curve (4096 samples), approximating the IR's frequency
  response without its phase or reverb characteristics (~98% spectral accuracy)
- **EQ Matching** — two audio reference files can be loaded and analyzed to
  derive the EQ curve required to match one spectrum to the other

---

## Platform and Format

- **OS**: Linux (native X11)
- **Formats**: LV2, CLAP
- **Audio**: Stereo in/out, any sample rate
- **Latency**: 128 samples (host-compensated)
- **Dependencies**: FFTW3, Cairo, X11
- **License**: BSD-3-Clause / Open Source
