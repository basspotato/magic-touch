#pragma once
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "SpectrumAnalyzer.h"

//==============================================================================
// Small pill-shaped toggle with a status LED (used for the external sidechain switch)
class PillToggle : public juce::Button
{
public:
    explicit PillToggle (const juce::String& label) : juce::Button (label) { setClickingTogglesState (true); }

    void setLed (juce::Colour c)    { if (c != led)    { led = c;    repaint(); } }
    void setAccent (juce::Colour c) { if (c != accent) { accent = c; repaint(); } }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour led { 0xff3a3f52 }, accent { 0xff2ee6a6 };
};

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
    struct MeterCh  { float peak = -90.0f, rms = -90.0f, hold = -90.0f, holdT = 0.0f; bool clip = false; };

    void timerCallback() override;
    void updateSpectrum (float dt);
    void drawTopBar  (juce::Graphics&, juce::Colour accent);
    void drawDisplay (juce::Graphics&, juce::Colour accent);
    void drawMeters  (juce::Graphics&, juce::Colour accent);
    void buildChassis();
    void stepPreset (int dir);
    void showPresetMenu();
    void showThemeMenu();
    void setTheme (int index, bool saveAsDefault);
    juce::Colour themeBase() const;   // the theme colour (current hue for "Reactive")
    juce::Colour tinted (juce::Colour c, float amount) const;

    MagicTouchProcessor& proc;
    MT::MagicLookAndFeel laf;   // declared before the children so it outlives them

    std::array<std::unique_ptr<KnobCell>, 10>  knobs;
    std::array<std::unique_ptr<FaderCell>, 10> faders;

    SpectrumAnalyzer analyzer;

    juce::Slider outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outAttachment;
    PillToggle scButton { "EXT SIDECHAIN" };
    juce::TooltipWindow tooltips { this, 500 };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scAttachment;

    juce::Image chassis;   // cached static background

    // themes
    int themeIndex = 0;
    std::unique_ptr<juce::PropertiesFile> settings;

    // layout
    juce::Rectangle<int>   topBar, presetRect, prevRect, nextRect, nameRect, tonePanel, dynPanel, themeRect, outTextRect;
    juce::Rectangle<float> displayRect, meterRect, meterResetRect;

    // analyser
    juce::dsp::FFT fft { MagicTouchProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) MagicTouchProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, MagicTouchProcessor::fftSize * 2> fftData {};
    std::array<float, MagicTouchProcessor::fftSize / 2> spec {};

    // meters (dB)
    std::array<MeterCh, 4> mtr {};   // in L, in R, out L, out R
    float grComp = 0.0f, grLim = 0.0f, scLevel = -90.0f;
    float outMax = -90.0f, lufsMDisp = -90.0f, lufsSDisp = -90.0f;
    double lastTickMs = 0.0;

    // reactive visuals
    float level = 0.0f, bass = 0.0f, hue = 0.52f;
    std::array<Particle, 48> particles {};
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicTouchEditor)
};
