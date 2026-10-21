# Magic Touch

VST3 mix/master bus effect for EDM. JUCE 8.0.4, C++17.
Target: Windows 11 (VST3, via GitHub Actions), macOS (local dev at `~/Downloads/MagicTouch`).
Audio quality target: on par with UAD / FabFilter / Plugin Alliance. CPU target: low.

## Build

Windows VST3 is built by GitHub Actions (`.github/workflows/build.yml`) — download from the run's Artifacts tab.

Local (macOS):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MagicTouch_VST3
```

Set `MT_COPY=FALSE` (or define `CI=1`) to skip copying into the system plugin folder.

## Signal chain

EQ (4 bands) → Tube → Tape → Exciter → Transient Punch → Glue Compressor → M/S Width + Bass Mono → Loudness push → Look-ahead brick-wall Limiter → Dry/Wet

- Entire colour/dynamics chain runs **4x oversampled** (polyphase IIR half-band filters). Don't bypass oversampling without flagging the quality trade-off.
- Limiter catches inter-sample (true) peaks at the oversampled rate.
- EQ and tape coefficients are smoothed in place on the audio thread — no zipper noise, no allocation.
- Dry/wet is sample-aligned with the processed signal.

## Source layout

```
Source/
  Params.h              — all APVTS parameter IDs and ranges
  LookAndFeel.h         — palette, fonts, ramp helpers, lit knob + console fader (custom JUCE LookAndFeel)
  SpectrumAnalyzer.*    — FFT-based spectrum display
  PluginProcessor.*     — audio thread, parameter handling
  PluginEditor.*        — GUI
Resources/Fonts/        — Barlow Condensed, Share Tech Mono (binary data via juce_add_binary_data, shared with Potato-Kicker)
```

## Architecture notes

- Parameters live in `Params.h` and are owned by an `AudioProcessorValueTreeState`. Never reach into the APVTS from the audio thread.
- All DSP is in `PluginProcessor`. `PluginEditor` reads metered values via atomics — no locking on the audio thread.
- Sidechain input: stereo. When EXT SIDECHAIN is on, the glue compressor keys from the sidechain (SC HPF bypassed, auto make-up off). The pill is amber when the switch is on but nothing is routed.
- Look-ahead latency ~1.5 ms; this is reported to the host — don't change it without updating the reported latency.

## GUI conventions

- The GUI is branded POTATO MAGIC (the plugin/product name is still Magic Touch).
- Potato-Kicker "light-up console" style: near-black tiles with ramp borders, top light strip and corner brackets; glow text; Barlow Condensed labels, Share Tech Mono values.
- Light ramp = orange -> yellow in the default "Console" theme (appended to the theme list so saved sessions keep their index); the other themes use a deeper -> lighter shade of their colour; "Reactive" follows the level hue.
- Every colour theme is music-reactive. Themes are saved per instance and as the default for new instances.
- Knobs: Kicker lit knob (black glossy cap, backlit ring + ticks that light along the ramp as the value rises; bipolar knobs light from zero). Ramp colours come from the slider's rotarySliderFillColourId (start) / rotarySliderOutlineColourId (end). Knob cells are buffered to images, so the ring only re-renders when a knob moves or the ramp changes.
- Faders: compressor = yellow, limiter = red, output = white; gradient fill intensifies with value.
- Analyzer: dropdown with 6 display modes including scrolling Waveform.
- RMS meter to the left of the sidechain meter.

## Controls

10 knobs (Tone & Colour): Bass, Body, Presence, Air, Tube, Tape, Exciter, Width, Bass Mono, Punch
10 sliders (Dynamics & Output): Thresh, Ratio, Attack, Release, Blend, SC HPF, Loudness, Ceiling, Lim Rel, Dry/Wet
Top bar: Theme picker, Output volume (−36 to +6 dB, double-click = 0 dB — applied after limiter, not changed by presets).

## Working rules

- One feature per session.
- Show a short plan before any big change.
- Give diffs/edits, not full file rewrites — the developer compiles and pushes.
- When debugging a build error, share only the relevant error lines.
- Keep DSP and GUI changes in separate sessions.
- Don't add CPU cost (extra FFTs, high repaint rates, allocations on the audio thread) without flagging it explicitly.
- Never allocate on the audio thread.
