# Magic Touch  (VST3 mix & master bus processor)

Signal chain (all stereo, ~1.5 ms look-ahead latency, reported to the host):

EQ (4 bands) -> Tube -> Tape -> Exciter -> Transient "Punch" -> Glue Compressor
-> M/S Width + Bass Mono -> Loudness push -> Look-ahead brick-wall Limiter -> Dry/Wet

## Engine
- The whole colour and dynamics chain runs **4x oversampled** (polyphase IIR half-band filters),
  so saturation harmonics don't alias back into the audible range and the EQ has no cramping
  near Nyquist (the Air shelf matches its analogue curve).
- The limiter works at the oversampled rate, so it catches **inter-sample (true) peaks**:
  with the ceiling at -1 dB the output measures -1.0 dBTP.
- Tube: asymmetric triode-style curve with level-dependent bias (mostly 2nd harmonic).
  Tape: head bump, HF pre/de-emphasis around the saturator, drive-dependent gap loss.
- Glue compressor: smooth decoupled dB-domain ballistics with a program-dependent slow branch.
- Bass Mono: phase-coherent Linkwitz-Riley 4 split.
- EQ and tape coefficients are smoothed and updated in place (no zipper noise, no allocation
  on the audio thread). Dry/wet stays sample-aligned with the processed signal.

## 10 knobs (Tone & Colour)
Bass, Body, Presence, Air, Tube, Tape, Exciter, Width, Bass Mono, Punch

## 10 sliders (Dynamics & Output)
Thresh, Ratio, Attack, Release, Blend (parallel), SC HPF, Loudness, Ceiling, Lim Rel, Dry/Wet

## Top bar
- **Theme**: click to pick a colour theme (Reactive, or one of the ten knob colours).
  Saved with your session and remembered as the default for new instances.
- **Output**: master output volume (-36 to +6 dB, double-click for 0 dB). Applied last,
  after the limiter, and not changed by presets.

## Sidechain
Magic Touch has a stereo sidechain input. Route a kick (or any track) to it in your DAW,
then switch on **EXT SIDECHAIN** in the Dynamics panel: the glue compressor then ducks
the mix from that signal (SC HPF is bypassed for the external key, and auto make-up is off).
The pill's light is amber when the switch is on but nothing is routed to the sidechain.

## Meters & analyser
- LEVELS: segmented peak + RMS meters for input/output with peak holds and clip lights,
  compressor and limiter gain reduction, sidechain level, and readouts for output peak,
  momentary (LUFS M) and short-term (LUFS S) loudness. Click the readouts to reset.
- LEVELS also has an output RMS meter (300 ms, both channels) next to the SC meter.
- ANALYZER: pick a display from the dropdown in its header (remembered per session):
  Classic (filled spectrum, peak hold, input overlay, hover readout), 3D Waterfall,
  Spectral Cloud, Prism, Spectrogram (scrolling heat map) and 1/3-Octave RTA.

## CPU
The GUI caches everything static and only redraws the meters, displays and moving parts
at 30 fps; knobs and faders redraw only when they move. Closing the plugin window removes
the GUI cost entirely.

## 20 presets
Click the preset name in the top bar (or the arrows). Defined in Source/Params.h, easy to edit.

## Build (local)
Requirements: CMake 3.22+, Git, and
- Windows: Visual Studio 2022 (Desktop C++)
- macOS: Xcode command line tools
- Linux: gcc/clang plus JUCE's Linux dependencies

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --target MagicTouch_VST3

The first configure downloads JUCE automatically. The VST3 lands in
build/MagicTouch_artefacts/Release/VST3/ and is also copied to your system VST3 folder.

## Build without installing anything locally
Push this folder to a GitHub repo. The included workflow (.github/workflows/build.yml)
builds Windows and macOS VST3s; download them from the run's "Artifacts" section.

## macOS note
Unsigned plugins may be blocked. For personal use run:
    codesign --force --deep -s - "Magic Touch.vst3"
    xattr -cr "Magic Touch.vst3"

## Licensing note
JUCE is dual-licensed (AGPLv3 or a commercial/free-tier JUCE license). Check the terms
before you distribute or sell the plugin.
