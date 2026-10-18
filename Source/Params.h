#pragma once
#include <array>

// Single source of truth for all 20 controls and all 20 presets.
namespace MT
{
constexpr int kNumParams = 20;

struct ParamInfo
{
    const char* id;
    const char* name;
    float min, max, step, def, skew;
    const char* suffix;
    int decimals;
};

enum ParamIndex
{
    // 10 knobs: tone & colour
    pLowShelf = 0, pLowMid, pPresence, pAir, pTube, pTape, pExcite, pWidth, pBassMono, pPunch,
    // 10 sliders: dynamics & output
    pCompThresh, pCompRatio, pCompAttack, pCompRelease, pCompMix, pCompHpf,
    pLoudness, pCeiling, pLimRelease, pMix
};

inline constexpr std::array<ParamInfo, kNumParams> kParams {{
    // ---- KNOBS ----
    { "lowShelf",    "Bass",       -6.0f,   6.0f, 0.1f,   0.0f, 1.0f, " dB", 1 },  // low shelf @ 90 Hz
    { "lowMid",      "Body",       -6.0f,   6.0f, 0.1f,   0.0f, 1.0f, " dB", 1 },  // bell @ 320 Hz (mud / body)
    { "presence",    "Presence",   -6.0f,   6.0f, 0.1f,   0.0f, 1.0f, " dB", 1 },  // bell @ 3.2 kHz
    { "air",         "Air",        -6.0f,   6.0f, 0.1f,   0.0f, 1.0f, " dB", 1 },  // high shelf @ 11 kHz
    { "tube",        "Tube",        0.0f, 100.0f, 0.1f,   0.0f, 1.0f, " %",  0 },  // asymmetric, even-harmonic warmth
    { "tape",        "Tape",        0.0f, 100.0f, 0.1f,   0.0f, 1.0f, " %",  0 },  // odd-harmonic soft saturation + HF roll
    { "excite",      "Exciter",     0.0f, 100.0f, 0.1f,   0.0f, 1.0f, " %",  0 },  // harmonic generator above ~4.5 kHz
    { "width",       "Width",       0.0f, 200.0f, 0.1f, 100.0f, 1.0f, " %",  0 },  // mid/side width
    { "bassMono",    "Bass Mono",   0.0f, 300.0f, 1.0f,   0.0f, 1.0f, " Hz", 0 },  // mono below this freq (0 = off)
    { "punch",       "Punch",    -100.0f, 100.0f, 0.1f,   0.0f, 1.0f, " %",  0 },  // transient shaper

    // ---- SLIDERS ----
    { "compThresh",  "Thresh",    -30.0f,   0.0f, 0.1f, -10.0f, 1.0f, " dB", 1 },
    { "compRatio",   "Ratio",       1.0f,  10.0f, 0.1f,   2.0f, 0.6f, ":1",  1 },
    { "compAttack",  "Attack",      0.1f, 100.0f, 0.1f,  10.0f, 0.4f, " ms", 1 },
    { "compRelease", "Release",    20.0f,1000.0f, 1.0f, 150.0f, 0.4f, " ms", 0 },
    { "compMix",     "Blend",       0.0f, 100.0f, 0.1f, 100.0f, 1.0f, " %",  0 },  // parallel compression
    { "compHpf",     "SC HPF",     20.0f, 300.0f, 1.0f,  90.0f, 0.6f, " Hz", 0 },  // sidechain high-pass
    { "loudness",    "Loudness",    0.0f,  12.0f, 0.1f,   0.0f, 1.0f, " dB", 1 },  // push into the limiter
    { "ceiling",     "Ceiling",    -6.0f,   0.0f, 0.1f,  -1.0f, 1.0f, " dB", 1 },
    { "limRelease",  "Lim Rel",    10.0f, 500.0f, 1.0f,  80.0f, 0.5f, " ms", 0 },
    { "mix",         "Dry/Wet",     0.0f, 100.0f, 0.1f, 100.0f, 1.0f, " %",  0 },
}};

struct Preset
{
    const char* name;
    std::array<float, kNumParams> v;
};

// Order: Bass, Body, Presence, Air, Tube, Tape, Exciter, Width, BassMono, Punch,
//        Thresh, Ratio, Attack, Release, Blend, SC HPF, Loudness, Ceiling, LimRel, Dry/Wet
// These are starting points that capture the general character of each style's finished masters.
// They are NOT measured from specific records. Adjust Loudness to taste for your mix level.
inline constexpr std::array<Preset, 20> kPresets {{
    { "Festival Mainstage",     {{ 1.5, -1.0, 0.8, 2.0,  25, 30, 30, 125, 120,  20,  -14, 3.0, 15, 120, 100, 100, 5.0, -0.8,  70, 100 }} },
    { "Big Room Slam",          {{ 2.0, -1.5, 1.0, 1.5,  20, 45, 20, 115, 140,  30,  -16, 4.0, 10,  90, 100, 120, 6.5, -0.5,  50, 100 }} },
    { "Future Bass Shimmer",    {{ 1.0, -1.0, 0.5, 3.0,  15, 20, 45, 140, 100,  10,  -12, 2.5, 20, 160,  80,  90, 4.0, -1.0,  90, 100 }} },
    { "Progressive House Glue", {{ 0.8, -0.5, 0.5, 1.5,  30, 25, 20, 115, 110,   5,  -12, 2.0, 30, 180, 100,  80, 3.5, -1.0, 100, 100 }} },
    { "Deep House Warmth",      {{ 1.2,  0.5,-0.5, 0.5,  55, 30, 10, 105, 100,   0,  -10, 1.8, 30, 200, 100,  70, 2.5, -1.0, 120, 100 }} },
    { "Tech House Punch",       {{ 1.5, -0.8, 0.8, 0.8,  25, 30, 15, 100, 130,  45,  -13, 3.0, 25, 110, 100, 110, 4.5, -0.8,  60, 100 }} },
    { "Melodic Techno Air",     {{ 0.5, -1.0, 0.0, 2.5,  30, 25, 35, 120, 100,  10,  -11, 2.0, 30, 200,  90,  80, 3.0, -1.0, 100, 100 }} },
    { "Peak-Time Techno",       {{ 1.8, -1.2, 1.2, 0.5,  30, 50, 25, 100, 150,  35,  -15, 3.5, 15, 100, 100, 120, 6.0, -0.6,  50, 100 }} },
    { "Trance Euphoria",        {{ 1.0, -1.0, 1.0, 2.5,  20, 25, 45, 135, 110,  15,  -12, 2.5, 20, 150,  90,  90, 4.5, -0.8,  80, 100 }} },
    { "Hard Trance Drive",      {{ 1.5, -1.5, 1.5, 1.5,  25, 45, 35, 110, 130,  25,  -15, 3.5, 12, 100, 100, 120, 6.0, -0.7,  60, 100 }} },
    { "Dubstep Heavyweight",    {{ 3.0, -1.5, 1.0, 1.0,  35, 50, 25, 120,  90,  30,  -16, 4.0, 12, 100, 100, 100, 6.5, -0.5,  50, 100 }} },
    { "Riddim Crush",           {{ 2.5, -2.0, 2.0, 0.5,  25, 65, 30, 100, 100,  40,  -18, 5.0,  8,  80, 100, 140, 6.5, -0.5,  40, 100 }} },
    { "Drum & Bass Roller",     {{ 1.5, -1.0, 1.2, 1.5,  20, 30, 25, 115, 110,  30,  -14, 3.0, 12,  90, 100, 110, 5.5, -0.7,  60, 100 }} },
    { "Liquid DnB Silk",        {{ 1.0,  0.0, 0.0, 2.5,  40, 20, 25, 125, 100,   0,  -11, 2.0, 25, 170,  90,  80, 3.0, -1.0, 110, 100 }} },
    { "Hardstyle Kick Forward", {{ 2.0, -2.0, 2.5, 1.0,  20, 70, 35, 100, 130,  40,  -18, 5.0,  6,  70, 100, 150, 7.0, -0.4,  40, 100 }} },
    { "Electro House Bite",     {{ 1.2, -1.0, 1.8, 1.5,  20, 40, 40, 125, 120,  25,  -14, 3.0, 12, 100, 100, 110, 5.5, -0.7,  60, 100 }} },
    { "Nu-Disco Sparkle",       {{ 0.8,  0.5, 0.0, 3.0,  45, 20, 30, 130, 100,  10,  -10, 1.8, 35, 220,  85,  70, 2.5, -1.0, 120, 100 }} },
    { "Afro House Groove",      {{ 1.0,  0.5, 0.5, 1.5,  45, 25, 20, 110, 100,  20,  -11, 2.0, 30, 180, 100,  80, 3.0, -1.0, 100, 100 }} },
    { "Pop Dance Radio",        {{ 1.0, -0.5, 1.5, 2.5,  25, 30, 40, 115, 110,  15,  -13, 2.5, 18, 130, 100,  90, 5.0, -0.8,  80, 100 }} },
    { "Bass House Growl",       {{ 2.0, -1.5, 1.5, 1.0,  30, 55, 25, 105, 120,  35,  -16, 4.0, 10,  90, 100, 120, 6.5, -0.6,  50, 100 }} },
}};
} // namespace MT
