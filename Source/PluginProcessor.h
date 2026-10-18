#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "Params.h"

namespace MT
{
using Coeffs = juce::dsp::IIR::Coefficients<float>;

// EQ band definitions are shared by the DSP and the GUI's response curve
inline Coeffs::Ptr makeLowShelfC (double sr, float db) { return Coeffs::makeLowShelf  (sr, 90.0f,    0.70f, juce::Decibels::decibelsToGain (db)); }
inline Coeffs::Ptr makeLowMidC   (double sr, float db) { return Coeffs::makePeakFilter (sr, 320.0f,   0.80f, juce::Decibels::decibelsToGain (db)); }
inline Coeffs::Ptr makePresC     (double sr, float db) { return Coeffs::makePeakFilter (sr, 3200.0f,  0.70f, juce::Decibels::decibelsToGain (db)); }
inline Coeffs::Ptr makeAirC      (double sr, float db) { return Coeffs::makeHighShelf (sr, 11000.0f, 0.60f, juce::Decibels::decibelsToGain (db)); }
} // namespace MT

class MagicTouchProcessor : public juce::AudioProcessor
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;

    MagicTouchProcessor();
    ~MagicTouchProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Magic Touch"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);
    int  getCurrentPreset() const;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor (accumulated peaks; the editor resets them with exchange(0))
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };
    std::atomic<float> compGR { 0.0f }, limGR { 0.0f };   // positive dB of gain reduction

    // Spectrum analyser hand-off
    std::array<float, fftSize> fftSnapshot {};
    std::atomic<bool> fftReady { false };

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    float tubeStage   (float x, size_t ch, float amount);
    float tapeStage   (float x, size_t ch, float amount);
    float exciteStage (float x, size_t ch, float amount);

    std::array<std::atomic<float>*, MT::kNumParams> raw {};
    double sr = 44100.0;

    // EQ / colour
    std::array<Filter, 2> eqLow, eqBody, eqPres, eqAir;
    std::array<Filter, 2> dcBlock, tapeLp, exciteHp, scHp;
    Filter sideLp1, sideLp2;
    std::array<float, 4> lastEq {};
    bool eqDirty = true;
    float lastHpf = -1.0f, lastBassMono = -1.0f;
    bool bassMonoOn = false;

    // Smoothed controls
    juce::SmoothedValue<float> smTube, smTape, smExcite, smWidth, smPunch, smCompMix, smLoud, smMix;

    // Transient shaper + compressor state
    float fastA = 0, fastR = 0, slowA = 0, slowR = 0;
    float fastEnv = 0.0f, slowEnv = 0.0f;
    float compEnv = -100.0f;

    // Look-ahead brick-wall limiter
    int   N = 64, wpos = 0;
    std::vector<float> winBuf, rBuf, dlL, dlR, dryL, dryR;
    double rSum = 0.0;
    float  rPrev = 1.0f;

    // Analyser FIFO
    std::array<float, fftSize> fifo {};
    int fifoIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicTouchProcessor)
};
