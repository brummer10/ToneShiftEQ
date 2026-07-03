# ToneShiftEQ User Manual

## Introduction

ToneShiftEQ is a 12-band spectral equalizer designed for precision tone shaping,
corrective processing, mixing, mastering, and live audio applications.
It combines interactive spectrum visualization with flexible filter controls
and multiple operating modes to provide both transparent and responsive equalization workflows.

The interface is designed around direct visual interaction,
allowing adjustments either through dedicated controls or directly inside the spectrum display.

---

# Main Interface Overview

The ToneShiftEQ interface consists of four main sections:

* Spectrum Display
* Band Control Panel
* Global Controls
* Monitoring Section

---

# Spectrum Display

The spectrum display is the main editing area.

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

### Draw gain changes

Hold:

Ctrl + Left Mouse Button

Drag vertically across the spectrum area.

ToneShiftEQ automatically selects the nearest band and adjusts its gain.

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

Positive values increase energy.

Negative values reduce energy.

---

### Q

Controls bandwidth.

Low Q values:

* Wide and gentle adjustment

High Q values:

* Narrow and precise adjustment

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

# Processing Mode

ToneShiftEQ provides two operating modes.

## Master Mode

Linear-phase processing with fixed latency.

Recommended for:

* Mastering
* Critical listening
* Transparent processing

Characteristics:

* Maximum precision
* Minimal phase coloration
* Introduces processing latency

---

## Live Mode

Zero-latency processing optimized for real-time use.

Recommended for:

* Live performance
* Monitoring
* Low-latency workflows

Characteristics:

* No processing delay
* Fast response
* Controlled phase deviation

---

# Additional Controls

## Smooth

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
meaning the smoothing amount changes across the frequency range to better match human hearing perception.

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

## Dynamics

The Dynamics control adjusts spectral contrast by
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
* Increase attack and detail
* Enhance transient perception

Negative settings:

* Tame harsh material
* Smooth resonances
* Reduce excessive spectral complexity

---

## Tone Bias

Tone Bias changes how spectral processing is distributed across the frequency range.

Negative values:

* Weight processing toward lower frequencies
* Produce a warmer or darker balance

Positive values:

* Weight processing toward higher frequencies
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

# Saving Impulse Responses

ToneShiftEQ can export generated impulse responses.

Use:

Save IR

The exported impulse response can be used in compatible convolution engines and external processing environments.

---

# Recommended Workflow

1. Begin with corrective cuts.

2. Use Low Cut and High Cut where necessary.

3. Adjust broad tonal balance.

4. Apply narrow corrective filters only when required.

5. Compare using Bypass.

6. Fine-tune output gain.

7. Switch between Master and Live mode according to application requirements.

