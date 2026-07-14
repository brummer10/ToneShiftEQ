# VST3 Wrapper (Travesty)

This project use a lightweight VST3 wrapper built 
on top of DPF's **travesty** headers — a pure-C, 
ISC-licensed reimplementation of the VST3 ABI
(`distrho/src/travesty` in [DISTRHO/DPF](https://github.com/DISTRHO/DPF)).

## Features

* No dependency on the Steinberg VST3 SDK
* Cross-platform support (Linux and Windows)
* Small and straightforward implementation

The wrapper implements the required VST3 component and controller interfaces,
parameter handling, state serialization, audio processing,
and editor integration.

## Why Travesty?

Travesty provides C-compatible definitions of the VST3 interfaces
without requiring the complete Steinberg SDK.
This results in a significantly smaller code base, simpler build integration,
and avoids the complexity of the official SDK while remaining compatible with VST3 hosts.

## License

The wrapper itself is: 

- BSD-3-Clause-licensed

Travesty is a separate project and is licensed under:

- ISC-licensed

