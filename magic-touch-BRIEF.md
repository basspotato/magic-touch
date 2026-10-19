# Magic Touch — Project Brief

Read this first. Make small, targeted changes. Don't rewrite whole files unless asked.

## What it is
VST3 effect for the mix bus and master bus. A final "magic touch" for dynamics and tone in EDM/dance mixes.
- Audio quality target: on par with UAD / FabFilter / Plugin Alliance.
- CPU target: low. Two instances previously overloaded the CPU, so efficiency matters in every change.

## Repo & builds
- Repo: `basspotato/magic-touch` (private)
- Local source: `~/Downloads/MagicTouch` (developer works on a Mac)
- Windows 11 VST3 is built by GitHub Actions and downloaded from the run's Artifacts
- Framework: _[fill in, e.g. JUCE version]_
- Build files: _[fill in, e.g. CMakeLists.txt, .github/workflows/build.yml]_

## Controls
- 10 knobs + 10 sliders, each a different tone shaper (compression, EQ, saturation, tube warmth, etc.)
- 20 presets modeled on the sound of top EDM/dance masters

## GUI
- FabFilter-inspired, with a surprise element
- Knobs: illuminated-ring style (like Acustica/Hardwell SpaceControl gain knobs)
- Every colour theme is music-reactive
- Meters: RMS meter to the left of the sidechain meter
- Faders: compressor = yellow, limiter = red, out = white; gradient fill intensifies with value
- Analyzer: dropdown with 6 display modes, including a scrolling Waveform mode (replaced Prism)

## Working rules
- One feature per session.
- Show a short plan before big changes.
- Give edits/diffs, not full files.
- Developer compiles and pushes; share only relevant error lines when debugging.
- Keep DSP and GUI changes in separate sessions.
- Don't add CPU cost (extra FFTs, high repaint rates) without saying so.

## Current status / next up
_[update at the end of each session: what's done, what's broken, what's next]_
