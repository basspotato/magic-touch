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
    static constexpr int ringSize = 8192;   // analyser history (power of two)

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

    // Copies the most recent n samples (mono sum) of the input or output into dest
    void copyAnalyserData (float* dest, int n, bool input) const;

    // Read by the editor (accumulated peaks; the editor resets them with exchange(0))
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };
    std::atomic<float> compGR { 0.0f }, limGR { 0.0f };   // positive dB of gain reduction
    std::atomic<float> scPeak { 0.0f };                   // external sidechain input peak

    // Current values (not reset by the editor)
    std::atomic<float> inRmsL { 0.0f }, inRmsR { 0.0f }, outRmsL { 0.0f }, outRmsR { 0.0f };
    std::atomic<float> lufsM { -100.0f }, lufsS { -100.0f };   // momentary (400 ms) / short-term (3 s)
    std::atomic<bool>  scConnected { false };                  // host is feeding the sidechain bus

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    float tubeStage   (float x, size_t ch, float amount);
    float tapeStage   (float x, size_t ch, float amount);
    float exciteStage (float x, size_t ch, float amount);

    std::array<std::atomic<float>*, MT::kNumParams> raw {};
    std::atomic<float>* outGainRaw = nullptr;
    std::atomic<float>* scExtRaw   = nullptr;
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
    juce::SmoothedValue<float> smTube, smTape, smExcite, smWidth, smPunch, smCompMix, smLoud, smMix, smOut;

    // Transient shaper + compressor state
    float fastA = 0, fastR = 0, slowA = 0, slowR = 0;
    float fastEnv = 0.0f, slowEnv = 0.0f;
    float compEnv = -100.0f;

    // Look-ahead brick-wall limiter
    int   N = 64, wpos = 0;
    std::vector<float> winBuf, rBuf, dlL, dlR, dryL, dryR;
    double rSum = 0.0;
    float  rPrev = 1.0f;

    // Analyser history rings
    std::array<float, ringSize> ringIn {}, ringOut {};
    std::atomic<int> ringPos { 0 };

    // Metering: RMS (300 ms) and K-weighted loudness
    float msInL = 0, msInR = 0, msOutL = 0, msOutR = 0, rmsCoef = 0.0f;
    std::array<Filter, 2> kShelf, kHigh;
    std::vector<float> loudBuf;
    int loudIdx = 0, n400 = 1;
    double sum400 = 0.0, sum3s = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicTouchProcessor)
};
