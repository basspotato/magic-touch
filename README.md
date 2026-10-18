# Magic Touch  (VST3 mix & master bus processor)

Signal chain (all stereo, ~1.5 ms look-ahead latency, reported to the host):

EQ (4 bands) -> Tube -> Tape -> Exciter -> Transient "Punch" -> Glue Compressor
-> M/S Width + Bass Mono -> Loudness push -> Look-ahead brick-wall Limiter -> Dry/Wet

## 10 knobs (Tone & Colour)
Bass, Body, Presence, Air, Tube, Tape, Exciter, Width, Bass Mono, Punch

## 10 sliders (Dynamics & Output)
Thresh, Ratio, Attack, Release, Blend (parallel), SC HPF, Loudness, Ceiling, Lim Rel, Dry/Wet

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
