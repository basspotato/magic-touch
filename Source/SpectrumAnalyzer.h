#pragma once
#include "PluginProcessor.h"

// Compact analyser with six display modes (chosen from the header dropdown):
//   Classic        - filled 1/6-octave spectrum + peak hold + input overlay, hover readout
//   3D Waterfall   - perspective landscape of recent spectra in a fire palette
//   Spectral Cloud - smoky persistence trails with a thin average line, mint on graphite
//   Prism          - mirrored rainbow bars with reflections and light streaks
//   Spectrogram    - scrolling time/frequency heat map (inferno palette)
//   1/3-Octave RTA - 31 ISO bands as segmented LED columns with peak caps
//
// Static layers (panel, grids, labels, unlit LEDs, glass) are rendered once into images, so a
// frame only draws what moves.
class SpectrumAnalyzer : public juce::Component
{
public:
    enum Mode { Classic = 0, Waterfall, Cloud, Prism, Spectrogram, Rta, NumModes };
    static const char* modeName (int m);

    explicit SpectrumAnalyzer (MagicTouchProcessor&);

    void update (float dtSeconds);          // called from the editor's timer
    void setAccent (juce::Colour c)         { accent = c; }
    void setBaseColour (juce::Colour c);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    static constexpr int order = 12;
    static constexpr int size  = 1 << order;
    static constexpr int numPoints = 160;
    static constexpr float minF = 20.0f, maxF = 20000.0f;
    static constexpr float topDb = 0.0f, botDb = -84.0f;

    static constexpr int wfRows = 32, wfCols = 80;
    static constexpr int prismBars = 48;
    static constexpr int rtaBands = 31;

    void setMode (int m, bool remember);
    void showModeMenu();
    void analyse (bool input, std::array<float, numPoints>& target, float dt, bool withPeak);
    void rebuildBands (double sampleRate);
    void buildStatic();

    void drawClassic     (juce::Graphics&);
    void drawWaterfall   (juce::Graphics&);
    void renderWaterfall();
    void drawCloud       (juce::Graphics&);
    void drawPrism       (juce::Graphics&);
    void drawSpectrogram (juce::Graphics&);
    void drawRta         (juce::Graphics&, bool staticPart);
    void drawHover       (juce::Graphics&, const std::array<float, numPoints>&);

    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float valueAt (const std::array<float, numPoints>&, float t) const;   // t = 0..1 across 20 Hz..20 kHz
    float valueAtX (const std::array<float, numPoints>& a, float x) const;
    juce::Path makePath (const std::array<float, numPoints>&, bool closed) const;
    juce::Rectangle<float> rtaColumn (int band) const;
    float rtaY (float db) const;

    MagicTouchProcessor& proc;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris };
    std::array<float, size * 2> work {};

    struct Band { float b0, b1; };          // fractional bin range for each display point
    std::array<Band, numPoints> bands {};
    std::array<float, numPoints> freqs {};
    std::array<float, numPoints> outAvg {}, outPeak {}, inAvg {}, peakHoldT {}, slowAvg {};
    double bandsRate = 0.0;

    int mode = Classic;

    // per-mode state
    std::array<std::array<float, wfCols>, wfRows> wf {};
    int wfHead = 0, wfTick = 0;
    juce::Image cloudImg, sgImg, wfImg;
    std::array<juce::Colour, 256> fireLut, infernoLut;
    std::array<float, prismBars> prism {};
    std::array<float, rtaBands> rta {}, rtaPeak {}, rtaHoldT {};
    std::array<std::pair<int, int>, rtaBands> rtaRange {};

    // cached layers
    juce::Image staticImg, glassImg;
    bool staticDirty = true;

    juce::Rectangle<float> plot, legendIn, modeRect;
    juce::Colour accent { 0xff3ba7ff }, base { 0xff12151f };
    bool showInput = true;
    float hoverX = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
