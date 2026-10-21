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
    juce::Colour led { 0xff3a2210 }, accent { 0xffffd23f };
};

//==============================================================================
class KnobCell : public juce::Component
{
public:
    KnobCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
              const juce::String& title);

    // the light ramp of the knob ring: colour at the start of travel -> colour at the end
    void setRamp (juce::Colour lo, juce::Colour hi);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String title;
    juce::Colour rampLo, rampHi;
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
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Particle { float x, y, speed, size, phase; };
    struct MeterCh  { float peak = -90.0f, rms = -90.0f, hold = -90.0f, holdT = 0.0f; bool clip = false; };

    void timerCallback() override;
    void updateSpectrum (float dt);
    // drawing is split into a cached static layer and small dynamic layers that repaint per frame
    void drawTopBar  (juce::Graphics&, juce::Colour accent, bool staticPart);
    void drawDisplay (juce::Graphics&, juce::Colour accent, bool staticPart);
    void drawMeters  (juce::Graphics&, juce::Colour accent, bool staticPart);
    void buildChassis();
    void buildBackground();
    void updateEqCurve();
    void stepPreset (int dir);
    void showPresetMenu();
    juce::Colour liveAccent() const;  // Kicker orange reacting to the music
    // Tone EQ display: hover readout, click/drag, wheel and double-click on the four fixed-frequency bands
    juce::Rectangle<float> eqPlotRect() const;
    int   eqBandAt (juce::Point<float> p, bool nodesOnly) const;
    float eqBandDb (int band) const;
    void  setEqBandDb (int band, float db);
    float eqResponseDb (float freq);

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

    juce::Image chassis;      // brushed metal, bezel, screws
    juce::Image background;   // chassis + panels + labels + grids: everything that doesn't move
    juce::RectangleList<int> gapRegion;   // where the music-reactive glow shows (between panels)
    int frameCount = 0;
    juce::Image displayGlass; // glare/inner-shadow overlay for the EQ display
    bool bgDirty = true;
    int bgPreset = -2;
    bool bgScHint = false;
    double lastBgBuildMs = 0.0;
    double lastOutValue = 1.0e9;

    // EQ curve cache (rebuilt only when an EQ knob moves)
    juce::Path eqCurve, eqArea;
    std::array<juce::Point<float>, 4> eqNodes {};
    std::array<float, 4> eqCached { 99.0f, 99.0f, 99.0f, 99.0f };

    // Potato-Kicker light ramp (start of travel -> end of travel)
    const juce::Colour rampLo { MT::col::orange }, rampHi { MT::col::yellow };

    // Tone EQ display interaction
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, 4> eqCs;
    int eqHover = -1, eqDrag = -1;
    bool eqMouseIn = false;
    juce::Point<float> eqMouse;
    float eqLastY = 0.0f;

    // layout
    juce::Rectangle<int>   topBar, presetRect, prevRect, nextRect, nameRect, tonePanel, dynPanel, outTextRect,
                           starRect, outValueRect, scHintRect;
    juce::Rectangle<float> displayRect, meterRect, meterResetRect;

    // analyser
    juce::dsp::FFT fft { MagicTouchProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) MagicTouchProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, MagicTouchProcessor::fftSize * 2> fftData {};
    std::array<float, MagicTouchProcessor::fftSize / 2> spec {};

    // meters (dB)
    std::array<MeterCh, 4> mtr {};   // in L, in R, out L, out R
    float grComp = 0.0f, grLim = 0.0f, scLevel = -90.0f, rmsOut = -90.0f;
    float outMax = -90.0f, lufsMDisp = -90.0f, lufsSDisp = -90.0f;
    double lastTickMs = 0.0;

    // reactive visuals
    float level = 0.0f, bass = 0.0f;
    std::array<Particle, 48> particles {};
    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicTouchEditor)
};
