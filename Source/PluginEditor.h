#pragma once
#include "PluginProcessor.h"
#include "LookAndFeel.h"

//==============================================================================
class KnobCell : public juce::Component
{
public:
    KnobCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
              const juce::String& title, juce::Colour colour);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String title;
    juce::Colour colour;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
class FaderCell : public juce::Component
{
public:
    FaderCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
               const juce::String& title, juce::Colour colour);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String title;
    juce::Colour colour;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

//==============================================================================
class MagicTouchEditor : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit MagicTouchEditor (MagicTouchProcessor&);
    ~MagicTouchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Particle { float x, y, speed, size, phase; };

    void timerCallback() override;
    void updateSpectrum();
    void drawTopBar  (juce::Graphics&, juce::Colour accent);
    void drawDisplay (juce::Graphics&, juce::Colour accent);
    void drawMeters  (juce::Graphics&, juce::Colour accent);
    void stepPreset (int dir);
    void showPresetMenu();

    MagicTouchProcessor& proc;
    MT::MagicLookAndFeel laf;   // declared before the children so it outlives them

    std::array<std::unique_ptr<KnobCell>, 10>  knobs;
    std::array<std::unique_ptr<FaderCell>, 10> faders;

    // layout
    juce::Rectangle<int>   topBar, presetRect, prevRect, nextRect, nameRect, tonePanel, dynPanel;
    juce::Rectangle<float> displayRect, meterRect;

    // analyser
    juce::dsp::FFT fft { MagicTouchProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) MagicTouchProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, MagicTouchProcessor::fftSize * 2> fftData {};
    std::array<float, MagicTouchProcessor::fftSize / 2> spec {};

    // meters (dB)
    float mIn[2] { -60.0f, -60.0f }, mOut[2] { -60.0f, -60.0f };
    float outHold = -60.0f, grComp = 0.0f, grLim = 0.0f;

    // reactive visuals
    float level = 0.0f, bass = 0.0f, hue = 0.52f;
    std::array<Particle, 48> particles {};
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicTouchEditor)
};
