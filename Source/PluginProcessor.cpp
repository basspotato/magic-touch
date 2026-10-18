#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace MT;

//==============================================================================
MagicTouchProcessor::MagicTouchProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAGICTOUCH", createLayout())
{
    for (size_t i = 0; i < kParams.size(); ++i)
        raw[i] = apvts.getRawParameterValue (kParams[i].id);
}

juce::AudioProcessorValueTreeState::ParameterLayout MagicTouchProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (size_t i = 0; i < kParams.size(); ++i)
    {
        const auto& p = kParams[i];
        const juce::String suffix (p.suffix);
        const int decimals = p.decimals;
        const bool isBassMono = ((int) i == pBassMono);

        auto attrs = juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([=] (float v, int) -> juce::String
            {
                if (isBassMono && v < 20.0f)
                    return "Off";
                return juce::String (v, decimals) + suffix;
            })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { p.id, 1 }, p.name,
            juce::NormalisableRange<float> (p.min, p.max, p.step, p.skew),
            p.def, attrs));
    }

    return layout;
}

//==============================================================================
bool MagicTouchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void MagicTouchProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    auto P = [this] (int i) { return raw[(size_t) i]->load(); };

    const auto dc = Coeffs::makeFirstOrderHighPass (sr, 10.0f);
    const auto tp = Coeffs::makeFirstOrderLowPass  (sr, 16000.0f);
    const auto eh = Coeffs::makeHighPass           (sr, 4500.0f);

    for (size_t ch = 0; ch < 2; ++ch)
    {
        for (auto* f : { &eqLow[ch], &eqBody[ch], &eqPres[ch], &eqAir[ch],
                         &dcBlock[ch], &tapeLp[ch], &exciteHp[ch], &scHp[ch] })
            f->reset();

        dcBlock[ch].coefficients  = dc;
        tapeLp[ch].coefficients   = tp;
        exciteHp[ch].coefficients = eh;
    }
    sideLp1.reset();
    sideLp2.reset();

    eqDirty = true;
    lastHpf = -1.0f;
    lastBassMono = -1.0f;
    bassMonoOn = false;

    auto coef = [this] (double ms) { return (float) std::exp (-1.0 / (sr * ms * 0.001)); };
    fastA = coef (0.5);   fastR = coef (10.0);
    slowA = coef (20.0);  slowR = coef (80.0);
    fastEnv = slowEnv = 0.0f;
    compEnv = -100.0f;

    // 1.5 ms look-ahead brick-wall limiter
    N = juce::jmax (16, (int) std::round (sr * 0.0015));
    winBuf.assign ((size_t) N, 1.0f);
    rBuf.assign   ((size_t) N, 1.0f);
    dlL.assign    ((size_t) N, 0.0f);
    dlR.assign    ((size_t) N, 0.0f);
    dryL.assign   ((size_t) N, 0.0f);
    dryR.assign   ((size_t) N, 0.0f);
    rSum = (double) N;
    rPrev = 1.0f;
    wpos = 0;
    setLatencySamples (N - 1);

    auto prep = [this] (juce::SmoothedValue<float>& s, float v)
    {
        s.reset (sr, 0.03);
        s.setCurrentAndTargetValue (v);
    };
    prep (smTube,    P (pTube)    * 0.01f);
    prep (smTape,    P (pTape)    * 0.01f);
    prep (smExcite,  P (pExcite)  * 0.01f);
    prep (smWidth,   P (pWidth)   * 0.01f);
    prep (smPunch,   P (pPunch)   * 0.01f);
    prep (smCompMix, P (pCompMix) * 0.01f);
    prep (smLoud,    juce::Decibels::decibelsToGain (P (pLoudness)));
    prep (smMix,     P (pMix)     * 0.01f);

    fifoIdx = 0;
}

//==============================================================================
// Saturation stages
float MagicTouchProcessor::tubeStage (float x, size_t ch, float a)
{
    // Asymmetric tanh = even + odd harmonics (the "warm" part), DC removed afterwards
    const float d = 1.0f + a * 1.5f;
    const float bias = 0.12f * a;
    const float y = (std::tanh (x * d + bias) - std::tanh (bias)) / d;
    const float w = dcBlock[ch].processSample (y);
    return x + (w - x) * a;
}

float MagicTouchProcessor::tapeStage (float x, size_t ch, float a)
{
    // Symmetric soft clip (odd harmonics) with a gentle top-end roll-off
    const float d = 1.0f + a * 2.0f;
    float y = std::tanh (x * d) / d;
    const float lp = tapeLp[ch].processSample (y);
    y += (lp - y) * a * 0.5f;
    return x + (y - x) * a;
}

float MagicTouchProcessor::exciteStage (float x, size_t ch, float a)
{
    // Generate harmonics from the high band only (pure distortion product, linear part removed)
    const float hp = exciteHp[ch].processSample (x);
    const float harm = hp - 0.25f * std::tanh (4.0f * hp);
    return x + a * 3.0f * harm;
}

//==============================================================================
void MagicTouchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    const int numSamples = buffer.getNumSamples();
    float* dataL = buffer.getWritePointer (0);
    float* dataR = buffer.getWritePointer (1);

    auto P = [this] (int i) { return raw[(size_t) i]->load(); };

    // ---- EQ coefficients (rebuilt only when a knob moves)
    const std::array<float, 4> eq { P (pLowShelf), P (pLowMid), P (pPresence), P (pAir) };
    if (eqDirty || eq != lastEq)
    {
        const auto c0 = makeLowShelfC (sr, eq[0]);
        const auto c1 = makeLowMidC   (sr, eq[1]);
        const auto c2 = makePresC     (sr, eq[2]);
        const auto c3 = makeAirC      (sr, eq[3]);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            eqLow[ch].coefficients  = c0;
            eqBody[ch].coefficients = c1;
            eqPres[ch].coefficients = c2;
            eqAir[ch].coefficients  = c3;
        }
        lastEq = eq;
        eqDirty = false;
    }

    // ---- Sidechain high-pass
    const float hpf = P (pCompHpf);
    if (hpf != lastHpf)
    {
        const auto c = Coeffs::makeHighPass (sr, hpf);
        for (size_t ch = 0; ch < 2; ++ch)
            scHp[ch].coefficients = c;
        lastHpf = hpf;
    }

    // ---- Bass mono crossover
    const float bm = P (pBassMono);
    const bool bmOn = bm >= 20.0f;
    if (bmOn && (bm != lastBassMono || ! bassMonoOn))
    {
        if (! bassMonoOn)
        {
            sideLp1.reset();
            sideLp2.reset();
        }
        const auto c = Coeffs::makeLowPass (sr, bm);
        sideLp1.coefficients = c;
        sideLp2.coefficients = c;
        lastBassMono = bm;
    }
    bassMonoOn = bmOn;

    // ---- Smoothed targets
    smTube.setTargetValue    (P (pTube)    * 0.01f);
    smTape.setTargetValue    (P (pTape)    * 0.01f);
    smExcite.setTargetValue  (P (pExcite)  * 0.01f);
    smWidth.setTargetValue   (P (pWidth)   * 0.01f);
    smPunch.setTargetValue   (P (pPunch)   * 0.01f);
    smCompMix.setTargetValue (P (pCompMix) * 0.01f);
    smLoud.setTargetValue    (juce::Decibels::decibelsToGain (P (pLoudness)));
    smMix.setTargetValue     (P (pMix)     * 0.01f);

    // ---- Compressor settings
    const float thresh = P (pCompThresh);
    const float ratio  = P (pCompRatio);
    const float slope  = 1.0f - 1.0f / ratio;
    const float makeup = -thresh * slope * 0.4f;     // partial auto make-up
    const float knee   = 6.0f;
    const float atkC   = (float) std::exp (-1.0 / (sr * 0.001 * (double) P (pCompAttack)));
    const float relC   = (float) std::exp (-1.0 / (sr * 0.001 * (double) P (pCompRelease)));

    // ---- Limiter settings
    const float ceilLin = juce::Decibels::decibelsToGain (P (pCeiling));
    const float rc      = (float) std::exp (-1.0 / (sr * 0.001 * (double) P (pLimRelease)));

    float pInL = 0.0f, pInR = 0.0f, pOutL = 0.0f, pOutR = 0.0f;
    float grMax = 0.0f, limMin = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = dataL[i], inR = dataR[i];
        pInL = std::max (pInL, std::abs (inL));
        pInR = std::max (pInR, std::abs (inR));

        float l = inL, r = inR;

        // 1) Four-band tone EQ
        l = eqAir[0].processSample (eqPres[0].processSample (eqBody[0].processSample (eqLow[0].processSample (l))));
        r = eqAir[1].processSample (eqPres[1].processSample (eqBody[1].processSample (eqLow[1].processSample (r))));

        // 2) Tube -> Tape -> Exciter
        const float tubeA = smTube.getNextValue();
        const float tapeA = smTape.getNextValue();
        const float excA  = smExcite.getNextValue();
        l = tubeStage (l, 0, tubeA);    r = tubeStage (r, 1, tubeA);
        l = tapeStage (l, 0, tapeA);    r = tapeStage (r, 1, tapeA);
        l = exciteStage (l, 0, excA);   r = exciteStage (r, 1, excA);

        // 3) Transient shaper (linked)
        const float punch = smPunch.getNextValue();
        {
            const float det = std::max (std::abs (l), std::abs (r));
            const float fc = det > fastEnv ? fastA : fastR;
            const float sc = det > slowEnv ? slowA : slowR;
            fastEnv = det + fc * (fastEnv - det);
            slowEnv = det + sc * (slowEnv - det);
            const float tr = juce::jlimit (-1.0f, 1.0f, (fastEnv - slowEnv) / (slowEnv + 1.0e-4f));
            const float gT = juce::jmax (0.25f, 1.0f + punch * 0.7f * tr);
            l *= gT;
            r *= gT;
        }

        // 4) Glue compressor: stereo-linked, soft knee, high-passed sidechain, parallel blend
        {
            const float scL = scHp[0].processSample (l);
            const float scR = scHp[1].processSample (r);
            const float det = std::max (std::abs (scL), std::abs (scR));
            const float lvl = juce::Decibels::gainToDecibels (det, -100.0f);
            const float cc = lvl > compEnv ? atkC : relC;
            compEnv = lvl + cc * (compEnv - lvl);

            const float over = compEnv - thresh;
            float gr = 0.0f;
            if (2.0f * over < -knee)
                gr = 0.0f;
            else if (2.0f * std::abs (over) <= knee)
            {
                const float t = over + knee * 0.5f;
                gr = slope * t * t / (2.0f * knee);
            }
            else
                gr = slope * over;

            grMax = std::max (grMax, gr);

            const float g = juce::Decibels::decibelsToGain (makeup - gr);
            const float cm = smCompMix.getNextValue();
            l += (l * g - l) * cm;
            r += (r * g - r) * cm;
        }

        // 5) Stereo: mid/side width + mono bass
        {
            const float wd = smWidth.getNextValue();
            const float m = 0.5f * (l + r);
            float s = 0.5f * (l - r);
            if (bassMonoOn)
                s -= sideLp2.processSample (sideLp1.processSample (s));
            s *= wd;
            l = m + s;
            r = m - s;
        }

        // 6) Loudness push into the limiter
        const float lg = smLoud.getNextValue();
        l *= lg;
        r *= lg;

        // 7) Look-ahead brick-wall limiter
        const float pk = std::max (std::abs (l), std::abs (r));
        const float gt = pk > ceilLin ? ceilLin / pk : 1.0f;
        winBuf[(size_t) wpos] = gt;

        float m = 1.0f;
        for (const float v : winBuf)
            m = std::min (m, v);

        if (m < rPrev)
            rPrev = m;
        else
            rPrev = rc * rPrev + (1.0f - rc) * m;

        rSum += (double) rPrev - (double) rBuf[(size_t) wpos];
        rBuf[(size_t) wpos] = rPrev;
        const float limGain = (float) (rSum / (double) N);
        limMin = std::min (limMin, limGain);

        dlL[(size_t) wpos]  = l;   dlR[(size_t) wpos]  = r;
        dryL[(size_t) wpos] = inL; dryR[(size_t) wpos] = inR;

        const int rd = (wpos + 1) % N;   // sample from N-1 steps ago
        const float oL = juce::jlimit (-ceilLin, ceilLin, dlL[(size_t) rd] * limGain);
        const float oR = juce::jlimit (-ceilLin, ceilLin, dlR[(size_t) rd] * limGain);
        const float dL = dryL[(size_t) rd], dR = dryR[(size_t) rd];

        // 8) Global dry/wet (dry is delayed to stay phase-aligned with the limiter)
        const float mx = smMix.getNextValue();
        const float outL = dL + (oL - dL) * mx;
        const float outR = dR + (oR - dR) * mx;
        dataL[i] = outL;
        dataR[i] = outR;
        wpos = rd;

        pOutL = std::max (pOutL, std::abs (outL));
        pOutR = std::max (pOutR, std::abs (outR));

        // Analyser feed
        fifo[(size_t) fifoIdx++] = 0.5f * (outL + outR);
        if (fifoIdx >= fftSize)
        {
            fftSnapshot = fifo;
            fftReady.store (true);
            fifoIdx = 0;
        }
    }

    // Publish meters (accumulated; the editor resets them)
    inPeakL.store  (std::max (inPeakL.load(),  pInL));
    inPeakR.store  (std::max (inPeakR.load(),  pInR));
    outPeakL.store (std::max (outPeakL.load(), pOutL));
    outPeakR.store (std::max (outPeakR.load(), pOutR));
    compGR.store   (std::max (compGR.load(), grMax * smCompMix.getTargetValue()));
    limGR.store    (std::max (limGR.load(), -juce::Decibels::gainToDecibels (limMin, -60.0f)));
}

//==============================================================================
void MagicTouchProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) kPresets.size())
        return;

    const auto& preset = kPresets[(size_t) index];

    for (size_t i = 0; i < kParams.size(); ++i)
    {
        if (auto* p = apvts.getParameter (kParams[i].id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (preset.v[i]));
            p->endChangeGesture();
        }
    }

    apvts.state.setProperty ("preset", index, nullptr);
}

int MagicTouchProcessor::getCurrentPreset() const
{
    return (int) apvts.state.getProperty ("preset", -1);
}

//==============================================================================
juce::AudioProcessorEditor* MagicTouchProcessor::createEditor()
{
    return new MagicTouchEditor (*this);
}

void MagicTouchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MagicTouchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MagicTouchProcessor();
}
