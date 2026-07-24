# ToneShiftEQ User Manual

## Introduction

ToneShiftEQ is a 12-band equalizer designed
for precision tone shaping, corrective processing, mixing, and mastering.
It combines interactive spectrum visualization with flexible filter controls
to provide transparent and musically natural equalization.

The interface is designed around direct visual interaction,
allowing adjustments either through dedicated controls or directly inside the spectrum display.

ToneShiftEQ provides two processing modes.

The default FFT mode uses minimum-phase parametric convolution
and introduces a fixed latency of 128 samples,
which is automatically reported to the host for delay compensation.

The Biquad mode uses cascaded biquad filters and operates with zero latency.

---

# Main Interface Overview

The interface is divided into four main sections:

* Spectrum Display
* Band Control Panel
* Global Controls
* Monitoring Section

---

# Spectrum Display

The spectrum display is the main editing area.

Each enabled EQ band is represented by an interactive control point
that can be adjusted directly inside the display.

It provides:

* Real-time audio spectrum visualization
* Display of active EQ curves
* Interactive filter control points
* Frequency and gain reference grid

The horizontal axis represents frequency:

* Left: Low frequencies (20 Hz)
* Right: High frequencies (20 kHz)

The vertical axis represents gain:

* Top: Boost
* Bottom: Cut

Moving the mouse across the display shows the current frequency and gain position.

---

# Interactive Editing

ToneShiftEQ supports direct manipulation of EQ bands.

### Move a band

Move the cursor over a band control point until it becomes highlighted.

Click and drag:

* Left / Right → changes frequency
* Up / Down → changes gain

### Adjust Q width

Hover over a band point and use the mouse wheel:

* Scroll up → narrower band
* Scroll down → wider band

### Quick Gain Editing

Hold:

Ctrl + Left Mouse Button

Drag vertically across the spectrum area.

ToneShiftEQ automatically selects the nearest EQ band and adjusts its gain while dragging.
This also functions as a jump-to-position shortcut for individual bands.

---

# EQ Band Controls

ToneShiftEQ provides 12 frequency bands.

Each band contains:

### Frequency

Sets the center or transition frequency of the filter.

Lower bands affect bass content.

Higher bands affect treble content.

---

### Gain

Adjusts boost or attenuation.

Range:

-48 dB to +24 dB

Positive values boost the selected frequency range.

Negative values attenuate the selected frequency range.

---

### Q

Controls bandwidth.

Low Q values:

* Wide and gentle adjustment

High Q values:

* Narrow and precise adjustment

---

### Threshold

Sets the level at which dynamic processing begins for the selected band.

Signals below the threshold are unaffected.

Signals above the threshold are dynamically attenuated according to the selected Ratio.

---

### Ratio

Controls the amount of dynamic gain reduction applied once the threshold is exceeded.

Higher Ratio values produce stronger attenuation.

---

### Filter Type

Available types:

**Low Shelf**

Boosts or cuts everything below the selected frequency.

**Peak**

Boosts or cuts around a center frequency.

**High Shelf**

Boosts or cuts everything above the selected frequency.

---

### Enable

Enables or disables the selected band.

Disabled bands do not affect processing.

---

### Solo

Temporarily isolates an individual band.

Useful for locating resonances or evaluating individual frequency regions.

---

### Mute

Temporarily disables the selected band contribution.

---

### Previous / Next

Switches between band control panels.

---

# Global Filters

## Low Cut

Removes low-frequency content below the selected frequency.

Typical uses:

* Remove rumble
* Remove microphone handling noise
* Clean low-end buildup

Range:

19 Hz – 2200 Hz

---

## High Cut

Removes high-frequency content above the selected frequency.

Typical uses:

* Reduce harshness
* Simulate bandwidth limitations
* Remove excessive high-frequency energy

Range:

110 Hz – 22 kHz

---

# Additional Controls

## Mode switch

* FFT - Minimum-phase convolution engine with 128 samples latency

* Biquad - Cascaded parametric biquad filters with zero latency

---

# Choosing a Processing Mode

## FFT Mode

* 128-sample latency

* Supports Smooth, Contrast, Tone Bias and IR export

* Best suited for mixing, mastering and offline processing

## Biquad Mode

* Zero latency

* Traditional parametric EQ behaviour

* Ideal for live monitoring and real-time performance

---

## HF Fade (FFT mode only)

Applies a smooth high-frequency roll-off between 20 kHz and the Nyquist frequency.

This reduces unnecessary ultrasonic energy while preserving the audible frequency range.

---

## Smooth (FFT mode only)

The Smooth control determines how much fine spectral detail is preserved in the generated response.

Lower values:

* Preserve small spectral structures
* Retain narrow resonances and detailed peaks
* More analytical and detailed sound shaping

Higher values:

* Reduce small irregularities
* Create broader and smoother response curves
* Produce more natural and less aggressive correction

Smooth does not simply blur the spectrum uniformly.
ToneShiftEQ uses adaptive logarithmic smoothing,
meaning the smoothing amount changes across the frequency range
to better match human hearing.

Typical use cases:

* Low values:

  * Precise corrective EQ
  * Resonance treatment
  * Detailed spectral shaping

* High values:

  * General tonal balancing
  * Mastering applications
  * Natural sounding corrections

---

## Contrast (FFT mode only)

The Contrast control adjusts spectral contrast by
enhancing or reducing local frequency differences relative to the surrounding spectrum.

Positive values:

* Increase spectral contrast
* Make peaks and details more pronounced
* Increase clarity and presence
* Can create a more vivid or energetic sound

Negative values:

* Reduce spectral contrast
* Suppress excessive peaks
* Produce a smoother and more controlled sound
* Can reduce harshness and spectral density

Internally, ToneShiftEQ compares local spectral structures against a smoothed reference and dynamically adjusts their intensity.

Typical use cases:

Positive settings:

* Add articulation to vocals
* Increase perceived attack and detail
* Enhance transient perception

Negative settings:

* Tame harsh material
* Smooth resonances
* Reduce excessive spectral complexity

---

## Tone Bias (FFT mode only)

Tone Bias changes how spectral processing is distributed across the frequency range.

Negative values:

* Weight processing toward lower frequencies
* Produce a warmer or darker balance

Positive values:

* Places greater emphasis on higher frequencies.
* Produce a brighter or more open balance

Tone Bias does not directly act as a conventional EQ tilt filter.
Instead, it changes how strongly spectral modifications are applied across the frequency spectrum.

Typical use cases:

Negative settings:

* Reduce excessive brightness
* Add warmth

Positive settings:

* Increase clarity
* Add air and presence

---

# Monitoring Section

## Gain Slider

Controls overall output level.

Range:

-46 dB to +12 dB

---

## Level Meters

Displays left and right output levels.

Use the meters to avoid clipping and monitor output balance.

---

## Bypass

Temporarily disables ToneShiftEQ processing for A/B comparison.

---

# Import and Export

## Load IR

Loads an impulse response and approximates it using the internal 12-band parametric EQ.

This provides a convenient starting point for further manual adjustments.

---

## Save IR

Exports the current FFT EQ response as a minimum-phase impulse response.

The exported impulse response can be used in compatible convolution processors.

---

## Load APO

Loads an Equalizer APO configuration file and converts the supported filters into ToneShiftEQ settings.

Unsupported filter types are ignored.

---

## Save APO

Exports the current EQ settings as an Equalizer APO configuration file.

Only filters supported by Equalizer APO are written to the configuration.

---

# Technical Notes

ToneShiftEQ uses a minimum-phase convolution engine. This means the phase
response behaves similarly to analog equalizers: phase and amplitude are
coupled, which produces a natural and musical character without pre-ringing.

Processing introduces a fixed latency of 128 samples, which is automatically
reported to the host for compensation. In practice this latency is inaudible
and transparent in any DAW that supports latency compensation.

---

# Recommended Workflow

1. Load an existing IR or APO configuration (optional).

2. Begin with corrective cuts.

3. Use Low Cut and High Cut where necessary.

4. Apply Dynamic EQ where resonances require level-dependent control.

5. Adjust the overall tonal balance.

6. Compare using Bypass.

7. Export the result as an IR or APO configuration if desired.
