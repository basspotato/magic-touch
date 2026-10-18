#pragma once
#include "PluginProcessor.h"

// Compact analyser in the style of iZotope Ozone / Insight:
// 1/6-octave smoothed average spectrum (filled), slow-falling peak-hold line,
// faint input spectrum for before/after comparison, 3 dB/oct tilt, and a
// hover crosshair that reads out frequency, note and level.
class SpectrumAnalyzer : public juce::Component
{
public:
    explicit SpectrumAnalyzer (MagicTouchProcessor&);

    void update (float dtSeconds);          // called from the editor's timer
    void setAccent (juce::Colour c)         { accent = c; }
    void setBaseColour (juce::Colour c)     { base = c; }

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

    void analyse (bool input, std::array<float, numPoints>& target, float dt, bool withPeak);
    void rebuildBands (double sampleRate);
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForDb (float db) const;
    float valueAtX (const std::array<float, numPoints>&, float x) const;
    juce::Path makePath (const std::array<float, numPoints>&, bool closed) const;

    MagicTouchProcessor& proc;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris };
    std::array<float, size * 2> work {};

    struct Band { float b0, b1; };          // fractional bin range for each display point
    std::array<Band, numPoints> bands {};
    std::array<float, numPoints> freqs {};
    std::array<float, numPoints> outAvg {}, outPeak {}, inAvg {}, peakHoldT {};
    double bandsRate = 0.0;

    juce::Rectangle<float> plot, legendIn;
    juce::Colour accent { 0xff3ba7ff }, base { 0xff12151f };
    bool showInput = true;
    float hoverX = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
