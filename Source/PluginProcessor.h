#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>
#include "Params.h"

namespace MT
{
using Coeffs  = juce::dsp::IIR::Coefficients<float>;
using ACoeffs = juce::dsp::IIR::ArrayCoefficients<float>;

// Tone EQ band definitions, shared by the DSP and the GUI's response curve
constexpr float kLowF  = 90.0f,    kLowQ  = 0.70f;
constexpr float kBodyF = 320.0f,   kBodyQ = 0.80f;
constexpr float kPresF = 3200.0f,  kPresQ = 0.70f;
constexpr float kAirF  = 11000.0f, kAirQ  = 0.60f;

inline Coeffs::Ptr makeLowShelfC (double sr, float db) { return Coeffs::makeLowShelf  (sr, kLowF,  kLowQ,  juce::Decibels::decibelsToGain (db)); }
inline Coeffs::Ptr makeLowMidC   (double sr, float db) { return Coeffs::makePeakFilter (sr, kBodyF, kBodyQ, juce::Decibels::decibelsToGain (db)); }
inline Coeffs::Ptr makePresC     (double sr, float db) { return Coeffs::makePeakFilter (sr, kPresF, kPresQ, juce::Decibels::decibelsToGain (db)); }
inline Coeffs::Ptr makeAirC      (double sr, float db) { return Coeffs::makeHighShelf (sr, kAirF,  kAirQ,  juce::Decibels::decibelsToGain (db)); }

// The whole colour/dynamics chain runs 4x oversampled
constexpr int kOsLog2  = 2;
constexpr int kOsRatio = 1 << kOsLog2;
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
    using juce::AudioProcessor::processBlock;

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

    // Sample rate the tone EQ actually runs at (for drawing an exact response curve)
    double getProcessingRate() const { return osr; }

    // Copies the most recent n samples (mono sum) of the input or output into dest
    void copyAnalyserData (float* dest, int n, bool input) const;

    juce::AudioProcessorValueTreeState apvts;

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

    void processChunk (float* L, float* R, const float* scL, const float* scR, int n, int offset);
    void updateDynamicFilters (bool force);

    float tubeStage   (float x, size_t ch, float amount);
    float tapeStage   (float x, size_t ch, float amount);
    float exciteStage (float x, size_t ch, float amount);

    std::array<std::atomic<float>*, MT::kNumParams> raw {};
    std::atomic<float>* outGainRaw = nullptr;
    std::atomic<float>* scExtRaw   = nullptr;
    double sr = 44100.0, osr = 44100.0 * MT::kOsRatio;
    int maxBlock = 512;

    // Oversampling (polyphase IIR half-band, integer latency)
    juce::dsp::Oversampling<float> oversampler { 2, (size_t) MT::kOsLog2,
                                                 juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    int osLatency = 0, totalLatency = 0;

    // ---- filters (all at the oversampled rate)
    std::array<Filter, 2> eqLow, eqBody, eqPres, eqAir;
    std::array<Filter, 2> tubeDc;
    std::array<Filter, 2> tapeBump, tapePre, tapeDe, tapeLp;
    std::array<Filter, 2> exciteHp, excitePost;
    std::array<Filter, 2> scHp;
    Filter midLp1, midLp2, midHp1, midHp2, sideHp1, sideHp2;   // phase-coherent LR4 bass mono

    std::array<float, 4> appliedEq { 99, 99, 99, 99 };
    float appliedTape = -1.0f, lastHpf = -1.0f, lastBassMono = -1.0f;
    bool bassMonoOn = false;
    int coefCountdown = 0;

    // ---- smoothed controls
    juce::SmoothedValue<float> smEq[4];
    juce::SmoothedValue<float> smTube, smTape, smExcite, smWidth, smPunch, smCompMix, smLoud;   // oversampled rate
    juce::SmoothedValue<float> smMix, smOut;                                                     // base rate

    // tube dynamic bias follower
    std::array<float, 2> tubeEnv {};
    float tubeEnvA = 0.0f;

    // transient shaper
    float fastA = 0, fastR = 0, slowA = 0, slowR = 0;
    float fastEnv = 0.0f, slowEnv = 0.0f;

    // compressor (smooth decoupled peak detector with program-dependent release)
    float grRel = 0.0f, grAtt = 0.0f, grSlowRel = 0.0f, grSlowAtt = 0.0f;

    // look-ahead limiter (oversampled -> catches inter-sample peaks)
    int N = 64, wpos = 0;
    std::vector<float> rBuf, dlL, dlR;
    std::vector<float> dqVal; std::vector<juce::uint32> dqPos;  // monotonic deque for the sliding minimum
    int dqHead = 0, dqCount = 0;
    juce::uint32 sampleClock = 0;
    double rSum = 0.0;
    float  rPrev = 1.0f;

    // dry path, delayed to line up with the processed signal
    std::vector<float> dryL, dryR;
    int dryPos = 0;

    // per-block meter accumulators written by processChunk
    float grMaxBlock = 0.0f, limMinBlock = 1.0f, scPeakBlock = 0.0f;

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
