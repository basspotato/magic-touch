#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace MT;

namespace
{
// Smooth saturating curve (Lambert continued-fraction tanh, clamped where it reaches +/-1 with zero slope).
// Within ~2% of tanh, continuous in value and slope, and many times cheaper; at 4x oversampling
// it sounds the same.
inline float satCurve (float x) noexcept
{
    if (x >  3.0f) return  1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
} // namespace

//==============================================================================
MagicTouchProcessor::MagicTouchProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MAGICTOUCH", createLayout())
{
    for (size_t i = 0; i < kParams.size(); ++i)
        raw[i] = apvts.getRawParameterValue (kParams[i].id);

    outGainRaw = apvts.getRawParameterValue ("outGain");
    scExtRaw   = apvts.getRawParameterValue ("scExt");
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

    // Master output volume: not part of the presets, so it stays put while you browse them
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outGain", 1 }, "Output",
        juce::NormalisableRange<float> (-36.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); })));

    // Compressor detector listens to the external sidechain input instead of the mix
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "scExt", 1 }, "Ext Sidechain", false));

    return layout;
}

//==============================================================================
bool MagicTouchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()
        || layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
    }
    return true;
}

//==============================================================================
void MagicTouchProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr  = sampleRate;
    osr = sr * kOsRatio;
    maxBlock = juce::jmax (1, samplesPerBlock);

    auto P = [this] (int i) { return raw[(size_t) i]->load(); };

    // ---- oversampling
    oversampler.reset();
    oversampler.initProcessing ((size_t) maxBlock);
    osLatency = (int) std::round (oversampler.getLatencyInSamples());

    // ---- filters: every coefficient object is allocated here, later updates are assigned in place
    auto unity = [] { return Coeffs::Ptr (new Coeffs (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f)); };
    for (size_t ch = 0; ch < 2; ++ch)
    {
        for (auto* f : { &eqLow[ch], &eqBody[ch], &eqPres[ch], &eqAir[ch], &tapeBump[ch], &tapePre[ch],
                         &tapeDe[ch], &tapeLp[ch], &scHp[ch] })
        {
            f->coefficients = unity();
            f->reset();
        }
        tubeDc[ch].coefficients     = Coeffs::makeFirstOrderHighPass (osr, 5.0f);
        exciteHp[ch].coefficients   = Coeffs::makeHighPass (osr, 4500.0f, 0.7071f);
        excitePost[ch].coefficients = Coeffs::makeHighPass (osr, 2000.0f, 0.7071f);
        for (auto* f : { &tubeDc[ch], &exciteHp[ch], &excitePost[ch] })
            f->reset();
    }
    for (auto* f : { &midLp1, &midLp2, &midHp1, &midHp2, &sideHp1, &sideHp2 })
    {
        f->coefficients = unity();
        f->reset();
    }

    // ---- smoothed controls
    auto prep = [] (juce::SmoothedValue<float>& s, double rate, double secs, float v)
    {
        s.reset (rate, secs);
        s.setCurrentAndTargetValue (v);
    };
    prep (smEq[0], osr, 0.05, P (pLowShelf));
    prep (smEq[1], osr, 0.05, P (pLowMid));
    prep (smEq[2], osr, 0.05, P (pPresence));
    prep (smEq[3], osr, 0.05, P (pAir));
    prep (smTube,    osr, 0.03, P (pTube)    * 0.01f);
    prep (smTape,    osr, 0.03, P (pTape)    * 0.01f);
    prep (smExcite,  osr, 0.03, P (pExcite)  * 0.01f);
    prep (smWidth,   osr, 0.03, P (pWidth)   * 0.01f);
    prep (smPunch,   osr, 0.03, P (pPunch)   * 0.01f);
    prep (smCompMix, osr, 0.03, P (pCompMix) * 0.01f);
    prep (smLoud,    osr, 0.03, juce::Decibels::decibelsToGain (P (pLoudness)));
    prep (smMix,     sr,  0.03, P (pMix)     * 0.01f);
    prep (smOut,     sr,  0.03, juce::Decibels::decibelsToGain (outGainRaw->load()));

    appliedEq.fill (99.0f);
    appliedTape = -1.0f;
    lastHpf = -1.0f;
    lastBassMono = -1.0f;
    bassMonoOn = false;
    coefCountdown = 0;
    updateDynamicFilters (true);

    // ---- envelopes
    auto coef = [this] (double ms) { return (float) std::exp (-1.0 / (osr * ms * 0.001)); };
    fastA = coef (0.5);   fastR = coef (10.0);
    slowA = coef (20.0);  slowR = coef (80.0);
    fastEnv = slowEnv = 0.0f;
    tubeEnvA = 1.0f - coef (30.0);
    tubeEnv.fill (0.0f);
    grRel = grAtt = grSlowRel = grSlowAtt = 0.0f;
    compDet = 0.0f;
    compGain = 1.0f;
    compGainStep = 0.0f;

    // ---- 1.5 ms look-ahead limiter at the oversampled rate; N - 1 is a multiple of the
    //      oversampling ratio so the total latency is a whole number of host samples
    const int baseLook = juce::jmax (4, (int) std::round (sr * 0.0015));
    N = baseLook * kOsRatio + 1;
    rBuf.assign ((size_t) N, 1.0f);
    dlL.assign  ((size_t) N, 0.0f);
    dlR.assign  ((size_t) N, 0.0f);
    dqVal.assign ((size_t) N + 1, 1.0f);
    dqPos.assign ((size_t) N + 1, 0);
    dqHead = dqCount = 0;
    sampleClock = 0;
    rSum = (double) N;
    rPrev = 1.0f;
    wpos = 0;

    totalLatency = osLatency + (N - 1) / kOsRatio;
    setLatencySamples (totalLatency);

    dryL.assign ((size_t) (totalLatency + maxBlock + 8), 0.0f);
    dryR.assign (dryL.size(), 0.0f);
    dryPos = 0;

    // ---- metering
    rmsCoef = 1.0f - (float) std::exp (-1.0 / (sr * 0.3));
    msInL = msInR = msOutL = msOutR = 0.0f;

    // K-weighting (ITU-R BS.1770-4), designed for the current sample rate from the
    // standard's analogue prototype so it matches the reference coefficients at 48 kHz
    Coeffs::Ptr ks, kh;
    {
        const double K  = std::tan (juce::MathConstants<double>::pi * 1681.974450955533 / sr);
        const double Q  = 0.7071752369554196;
        const double Vh = std::pow (10.0, 3.999843853973347 / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        ks = new Coeffs ((float) (Vh + Vb * K / Q + K * K), (float) (2.0 * (K * K - Vh)), (float) (Vh - Vb * K / Q + K * K),
                         (float) (1.0 + K / Q + K * K),      (float) (2.0 * (K * K - 1.0)), (float) (1.0 - K / Q + K * K));
    }
    {
        const double K = std::tan (juce::MathConstants<double>::pi * 38.13547087602444 / sr);
        const double Q = 0.5003270373238773;
        kh = new Coeffs (1.0f, -2.0f, 1.0f,
                         (float) (1.0 + K / Q + K * K), (float) (2.0 * (K * K - 1.0)), (float) (1.0 - K / Q + K * K));
    }
    for (size_t ch = 0; ch < 2; ++ch)
    {
        kShelf[ch].reset(); kShelf[ch].coefficients = ks;
        kHigh[ch].reset();  kHigh[ch].coefficients  = kh;
    }
    loudBuf.assign ((size_t) juce::jmax (1, (int) std::round (sr * 3.0)), 0.0f);
    n400 = juce::jmax (1, (int) std::round (sr * 0.4));
    loudIdx = 0;
    sum400 = sum3s = 0.0;

    ringIn.fill (0.0f);
    ringOut.fill (0.0f);
    ringPos.store (0);
}

void MagicTouchProcessor::copyAnalyserDataEndingAt (float* dest, int n, int endPos, bool input) const
{
    const auto& ring = input ? ringIn : ringOut;
    n = juce::jmin (n, ringSize);
    for (int i = 0; i < n; ++i)
        dest[i] = ring[(size_t) ((endPos - n + i) & (ringSize - 1))];
}

void MagicTouchProcessor::copyAnalyserData (float* dest, int n, bool input) const
{
    const auto& ring = input ? ringIn : ringOut;
    n = juce::jmin (n, ringSize);
    const int end = ringPos.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        dest[i] = ring[(size_t) ((end - n + i) & (ringSize - 1))];
}

//==============================================================================
// Coefficients that follow smoothed controls (EQ gains, tape drive), recomputed every
// 32 oversampled samples while moving. Assigned in place: no allocation on the audio thread.
void MagicTouchProcessor::updateDynamicFilters (bool force)
{
    const std::array<float, 4> eq { smEq[0].getCurrentValue(), smEq[1].getCurrentValue(),
                                    smEq[2].getCurrentValue(), smEq[3].getCurrentValue() };
    if (force || eq != appliedEq)
    {
        const auto c0 = ACoeffs::makeLowShelf   (osr, kLowF,  kLowQ,  juce::Decibels::decibelsToGain (eq[0]));
        const auto c1 = ACoeffs::makePeakFilter (osr, kBodyF, kBodyQ, juce::Decibels::decibelsToGain (eq[1]));
        const auto c2 = ACoeffs::makePeakFilter (osr, kPresF, kPresQ, juce::Decibels::decibelsToGain (eq[2]));
        const auto c3 = ACoeffs::makeHighShelf  (osr, kAirF,  kAirQ,  juce::Decibels::decibelsToGain (eq[3]));
        for (size_t ch = 0; ch < 2; ++ch)
        {
            *eqLow[ch].coefficients  = c0;
            *eqBody[ch].coefficients = c1;
            *eqPres[ch].coefficients = c2;
            *eqAir[ch].coefficients  = c3;
        }
        appliedEq = eq;
    }

    const float ta = smTape.getCurrentValue();
    if (force || std::abs (ta - appliedTape) > 1.0e-4f)
    {
        // Ampex ATR-102 style voicing (30 ips mastering deck, subtle): a broad playback-head bump
        // around 100 Hz (applied after the saturator so the low end stays tight), HF pre-emphasis into
        // the saturator with exact de-emphasis after it (highs saturate and squash first, which is the
        // smooth top), and a gentle gap-loss low-pass that stays high and closes slowly with drive.
        // Voicing is from the machine's general character, not measured from a unit.
        const auto bump = ACoeffs::makePeakFilter (osr, kTapeBumpF, kTapeBumpQ, juce::Decibels::decibelsToGain (kTapeBumpDb * ta));
        const auto pre  = ACoeffs::makeHighShelf  (osr, kTapeEmphF, 0.6f, juce::Decibels::decibelsToGain ( kTapeEmphDb * ta));
        const auto de   = ACoeffs::makeHighShelf  (osr, kTapeEmphF, 0.6f, juce::Decibels::decibelsToGain (-kTapeEmphDb * ta));
        const auto lp   = ACoeffs::makeLowPass    (osr, 22000.0f - 5000.0f * ta, 0.6f);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            *tapeBump[ch].coefficients = bump;
            *tapePre[ch].coefficients  = pre;
            *tapeDe[ch].coefficients   = de;
            *tapeLp[ch].coefficients   = lp;
        }
        appliedTape = ta;
    }
}

//==============================================================================
// Saturation stages (run at 4x, so their harmonics don't fold back as aliasing)
float MagicTouchProcessor::tubeStage (float x, size_t ch, float a)
{
    // Triode-style asymmetric transfer: the positive half compresses harder than the negative,
    // giving mostly 2nd-order harmonics. The bias grows with level (like grid conduction), so
    // the even-harmonic content rises as the music gets louder.
    auto& env = tubeEnv[ch];
    env += (std::min (1.0f, std::abs (x)) - env) * tubeEnvA;

    const float d = 1.0f + a * 1.8f;
    const float bias = a * (0.08f + 0.30f * env);
    auto f = [] (float u) { return u >= 0.0f ? satCurve (u) : satCurve (0.6f * u) / 0.6f; };

    const float y = (f (x * d + bias) - f (bias)) / d;
    const float w = tubeDc[ch].processSample (y);
    return x + (w - x) * a;
}

float MagicTouchProcessor::tapeStage (float x, size_t ch, float a)
{
    // HF pre-emphasis -> soft odd-harmonic saturation -> de-emphasis -> head bump -> gap loss.
    // The emphasis pair makes high frequencies saturate first, like real tape; the bump comes after
    // the saturator (as on a playback head) so the low end isn't driven into distortion.
    float y = tapePre[ch].processSample (x);
    const float d = 1.0f + a * 1.4f;
    y = satCurve (y * d) / d;
    y = tapeDe[ch].processSample (y);
    y = tapeBump[ch].processSample (y);
    y = tapeLp[ch].processSample (y);
    return x + (y - x) * a;
}

float MagicTouchProcessor::exciteStage (float x, size_t ch, float a)
{
    // Generate harmonics from the high band only (pure distortion product, linear part removed),
    // then high-pass them so intermodulation doesn't muddy the mids
    const float hp = exciteHp[ch].processSample (x);
    const float harm = excitePost[ch].processSample (hp - 0.25f * satCurve (4.0f * hp));
    return x + a * 3.0f * harm;
}

//==============================================================================
void MagicTouchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBuf = getBusBuffer (buffer, true, 0);
    const int numSamples = buffer.getNumSamples();
    if (mainBuf.getNumChannels() < 2 || numSamples == 0)
        return;

    float* dataL = mainBuf.getWritePointer (0);
    float* dataR = mainBuf.getWritePointer (1);

    // ---- external sidechain
    const float* scInL = nullptr;
    const float* scInR = nullptr;
    if (getBusCount (true) > 1)
    {
        auto scBuf = getBusBuffer (buffer, true, 1);
        if (scBuf.getNumChannels() > 0)
        {
            scInL = scBuf.getReadPointer (0);
            scInR = scBuf.getNumChannels() > 1 ? scBuf.getReadPointer (1) : scInL;
        }
    }
    scConnected.store (scInL != nullptr);

    auto P = [this] (int i) { return raw[(size_t) i]->load(); };

    // ---- smoothed targets
    smEq[0].setTargetValue (P (pLowShelf));
    smEq[1].setTargetValue (P (pLowMid));
    smEq[2].setTargetValue (P (pPresence));
    smEq[3].setTargetValue (P (pAir));
    smTube.setTargetValue    (P (pTube)    * 0.01f);
    smTape.setTargetValue    (P (pTape)    * 0.01f);
    smExcite.setTargetValue  (P (pExcite)  * 0.01f);
    smWidth.setTargetValue   (P (pWidth)   * 0.01f);
    smPunch.setTargetValue   (P (pPunch)   * 0.01f);
    smCompMix.setTargetValue (P (pCompMix) * 0.01f);
    smLoud.setTargetValue    (juce::Decibels::decibelsToGain (P (pLoudness)));
    smMix.setTargetValue     (P (pMix)     * 0.01f);
    smOut.setTargetValue     (juce::Decibels::decibelsToGain (outGainRaw->load()));

    // ---- sidechain high-pass (only rebuilt when the slider moves)
    const float hpf = P (pCompHpf);
    if (std::abs (hpf - lastHpf) > 1.0e-3f)
    {
        const auto c = ACoeffs::makeHighPass (osr, hpf, 0.7071f);
        for (size_t ch = 0; ch < 2; ++ch)
            *scHp[ch].coefficients = c;
        lastHpf = hpf;
    }

    // ---- bass mono: Linkwitz-Riley 4 split. Side keeps only the high band; mid goes through the
    //      matching LR4 all-pass, so mid and side stay phase-aligned around the crossover.
    const float bm = P (pBassMono);
    const bool bmOn = bm >= 20.0f;
    if (bmOn && (std::abs (bm - lastBassMono) > 1.0e-3f || ! bassMonoOn))
    {
        if (! bassMonoOn)
            for (auto* f : { &midLp1, &midLp2, &midHp1, &midHp2, &sideHp1, &sideHp2 })
                f->reset();
        const auto lp = ACoeffs::makeLowPass  (osr, bm, 0.7071f);
        const auto hp = ACoeffs::makeHighPass (osr, bm, 0.7071f);
        *midLp1.coefficients = lp;  *midLp2.coefficients = lp;
        *midHp1.coefficients = hp;  *midHp2.coefficients = hp;
        *sideHp1.coefficients = hp; *sideHp2.coefficients = hp;
        lastBassMono = bm;
    }
    bassMonoOn = bmOn;

    float pInL = 0.0f, pInR = 0.0f, pOutL = 0.0f, pOutR = 0.0f;
    grMaxBlock = 0.0f;
    limMinBlock = 1.0f;
    scPeakBlock = 0.0f;
    int rp = ringPos.load (std::memory_order_relaxed);
    const float ceilLin = juce::Decibels::decibelsToGain (P (pCeiling));
    const int dsz = (int) dryL.size();

    for (int off = 0; off < numSamples; off += maxBlock)
    {
        const int n = juce::jmin (maxBlock, numSamples - off);
        float* L = dataL + off;
        float* R = dataR + off;

        // 1) input metering + store the dry signal
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            pInL = std::max (pInL, std::abs (inL));
            pInR = std::max (pInR, std::abs (inR));
            msInL += (inL * inL - msInL) * rmsCoef;
            msInR += (inR * inR - msInR) * rmsCoef;
            dryL[(size_t) ((dryPos + i) % dsz)] = inL;
            dryR[(size_t) ((dryPos + i) % dsz)] = inR;
            ringIn[(size_t) ((rp + i) & (ringSize - 1))] = 0.5f * (inL + inR);
        }

        // 2) the oversampled colour + dynamics chain
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = oversampler.processSamplesUp (block);
        processChunk (up.getChannelPointer (0), up.getChannelPointer (1),
                      scInL != nullptr ? scInL + off : nullptr,
                      scInR != nullptr ? scInR + off : nullptr,
                      (int) up.getNumSamples(), off);
        oversampler.processSamplesDown (block);

        // 3) back at the host rate: ceiling safety, dry/wet, output volume, metering
        for (int i = 0; i < n; ++i)
        {
            const float wL = juce::jlimit (-ceilLin, ceilLin, L[i]);
            const float wR = juce::jlimit (-ceilLin, ceilLin, R[i]);
            const int rdx = ((dryPos + i - totalLatency) % dsz + dsz) % dsz;
            const float dL = dryL[(size_t) rdx], dR = dryR[(size_t) rdx];

            const float mx = smMix.getNextValue();
            const float og = smOut.getNextValue();
            const float outL = (dL + (wL - dL) * mx) * og;
            const float outR = (dR + (wR - dR) * mx) * og;
            L[i] = outL;
            R[i] = outR;

            pOutL = std::max (pOutL, std::abs (outL));
            pOutR = std::max (pOutR, std::abs (outR));
            msOutL += (outL * outL - msOutL) * rmsCoef;
            msOutR += (outR * outR - msOutR) * rmsCoef;

            // K-weighted loudness: sliding 400 ms and 3 s windows of summed channel power
            const float kl = kHigh[0].processSample (kShelf[0].processSample (outL));
            const float kr = kHigh[1].processSample (kShelf[1].processSample (outR));
            const float pw = kl * kl + kr * kr;
            const int size = (int) loudBuf.size();
            sum400 += (double) pw - (double) loudBuf[(size_t) ((loudIdx - n400 + size) % size)];
            sum3s  += (double) pw - (double) loudBuf[(size_t) loudIdx];
            loudBuf[(size_t) loudIdx] = pw;
            loudIdx = (loudIdx + 1) % size;

            ringOut[(size_t) ((rp + i) & (ringSize - 1))] = 0.5f * (outL + outR);
        }

        dryPos = (dryPos + n) % dsz;
        rp = (rp + n) & (ringSize - 1);
    }
    ringPos.store (rp, std::memory_order_release);

    // Guard against tiny negative sums from floating point drift
    sum400 = std::max (0.0, sum400);
    sum3s  = std::max (0.0, sum3s);
    auto toLufs = [] (double sum, int n) { return (float) (-0.691 + 10.0 * std::log10 (sum / (double) n + 1.0e-12)); };
    lufsM.store (toLufs (sum400, n400));
    lufsS.store (toLufs (sum3s, (int) loudBuf.size()));

    inRmsL.store  (std::sqrt (msInL));
    inRmsR.store  (std::sqrt (msInR));
    outRmsL.store (std::sqrt (msOutL));
    outRmsR.store (std::sqrt (msOutR));

    inPeakL.store  (std::max (inPeakL.load(),  pInL));
    inPeakR.store  (std::max (inPeakR.load(),  pInR));
    outPeakL.store (std::max (outPeakL.load(), pOutL));
    outPeakR.store (std::max (outPeakR.load(), pOutR));
    compGR.store   (std::max (compGR.load(), grMaxBlock * smCompMix.getTargetValue()));
    limGR.store    (std::max (limGR.load(), -juce::Decibels::gainToDecibels (limMinBlock, -60.0f)));
    scPeak.store   (std::max (scPeak.load(), scPeakBlock));
}

//==============================================================================
void MagicTouchProcessor::processChunk (float* L, float* R, const float* scL, const float* scR, int n, int)
{
    auto P = [this] (int i) { return raw[(size_t) i]->load(); };
    const bool useExtSc = scL != nullptr && scExtRaw->load() > 0.5f;

    // ---- compressor settings
    const float thresh = P (pCompThresh);
    const float ratio  = P (pCompRatio);
    const float slope  = 1.0f - 1.0f / ratio;
    // partial auto make-up (off when ducking from an external sidechain, so quiet parts aren't boosted)
    const float makeup = useExtSc ? 0.0f : -thresh * slope * 0.4f;
    const float knee   = 6.0f;
    const double atkMs = (double) P (pCompAttack), relMs = (double) P (pCompRelease);
    auto oc = [this] (double ms) { return (float) std::exp (-1.0 / (osr * 0.001 * ms)); };
    // compressor ballistics run once per host sample (every kOsRatio oversampled samples)
    auto bc = [this] (double ms) { return (float) std::exp (-1.0 / (sr * 0.001 * ms)); };
    const float atkC     = bc (atkMs);
    const float relC     = bc (relMs);
    const float atkSlowC = bc (juce::jlimit (30.0, 300.0, atkMs * 8.0));   // program-dependent "glue" branch
    const float relSlowC = bc (relMs * 3.0);

    // ---- limiter settings
    const float ceilLin = juce::Decibels::decibelsToGain (P (pCeiling));
    const float rc      = oc ((double) P (pLimRelease));

    for (int j = 0; j < n; ++j)
    {
        if (--coefCountdown <= 0)
        {
            for (auto& s : smEq)
                s.skip (32);
            updateDynamicFilters (false);
            coefCountdown = 32;
        }

        float l = L[j], r = R[j];

        // 1) four-band tone EQ
        l = eqAir[0].processSample (eqPres[0].processSample (eqBody[0].processSample (eqLow[0].processSample (l))));
        r = eqAir[1].processSample (eqPres[1].processSample (eqBody[1].processSample (eqLow[1].processSample (r))));

        // 2) tube -> tape -> exciter
        const float tubeA = smTube.getNextValue();
        const float tapeA = smTape.getNextValue();
        const float excA  = smExcite.getNextValue();
        if (tubeA > 1.0e-4f) { l = tubeStage (l, 0, tubeA); r = tubeStage (r, 1, tubeA); }
        if (tapeA > 1.0e-4f) { l = tapeStage (l, 0, tapeA); r = tapeStage (r, 1, tapeA); }
        if (excA  > 1.0e-4f) { l = exciteStage (l, 0, excA); r = exciteStage (r, 1, excA); }

        // 3) transient shaper (linked)
        {
            const float punch = smPunch.getNextValue();
            const float det = std::max (std::abs (l), std::abs (r));
            fastEnv = det + (det > fastEnv ? fastA : fastR) * (fastEnv - det);
            slowEnv = det + (det > slowEnv ? slowA : slowR) * (slowEnv - det);
            const float tr = juce::jlimit (-1.0f, 1.0f, (fastEnv - slowEnv) / (slowEnv + 1.0e-4f));
            const float gT = juce::jmax (0.25f, 1.0f + punch * 0.7f * tr);
            l *= gT;
            r *= gT;
        }

        // 4) glue compressor: stereo-linked, soft knee, smooth decoupled ballistics in the dB domain,
        //    plus a slow branch that holds gain reduction longer on sustained material (auto release)
        {
            float srcL = l, srcR = r;
            if (scL != nullptr)
            {
                const float eL = scL[j >> kOsLog2], eR = scR[j >> kOsLog2];
                scPeakBlock = std::max (scPeakBlock, std::max (std::abs (eL), std::abs (eR)));
                if (useExtSc) { srcL = eL; srcR = eR; }
            }
            // the SC HPF shapes the internal detector; an external key (e.g. a kick) is used full-range
            float dL = scHp[0].processSample (srcL);
            float dR = scHp[1].processSample (srcR);
            if (useExtSc) { dL = srcL; dR = srcR; }

            // peak over the oversampled samples (so inter-sample peaks count), evaluated once per host sample
            compDet = std::max (compDet, std::max (std::abs (dL), std::abs (dR)));
            if ((j & (kOsRatio - 1)) == kOsRatio - 1)
            {
                const float xdB = juce::Decibels::gainToDecibels (compDet, -120.0f);
                compDet = 0.0f;
                const float over = xdB - thresh;
                float gc = 0.0f;
                if (2.0f * over <= -knee)          gc = 0.0f;
                else if (2.0f * over < knee)       { const float t = over + knee * 0.5f; gc = slope * t * t / (2.0f * knee); }
                else                               gc = slope * over;

                grRel = std::max (gc, relC * grRel + (1.0f - relC) * gc);
                grAtt = atkC * grAtt + (1.0f - atkC) * grRel;
                grSlowAtt = gc > grSlowAtt ? atkSlowC * grSlowAtt + (1.0f - atkSlowC) * gc
                                           : relSlowC * grSlowAtt + (1.0f - relSlowC) * gc;
                const float gr = std::max (grAtt, grSlowAtt);
                grMaxBlock = std::max (grMaxBlock, gr);

                // glide to the new gain over the next host sample (no steps at the oversampled rate)
                compGainStep = (juce::Decibels::decibelsToGain (makeup - gr) - compGain) / (float) kOsRatio;
            }
            compGain += compGainStep;

            const float g = compGain;
            const float cm = smCompMix.getNextValue();
            l += (l * g - l) * cm;
            r += (r * g - r) * cm;
        }

        // 5) stereo: mid/side width + phase-coherent mono bass
        {
            const float wd = smWidth.getNextValue();
            float m = 0.5f * (l + r);
            float s = 0.5f * (l - r);
            if (bassMonoOn)
            {
                m = midLp2.processSample (midLp1.processSample (m)) + midHp2.processSample (midHp1.processSample (m));
                s = sideHp2.processSample (sideHp1.processSample (s));
            }
            s *= wd;
            l = m + s;
            r = m - s;
        }

        // 6) loudness push into the limiter
        const float lg = smLoud.getNextValue();
        l *= lg;
        r *= lg;

        // 7) look-ahead brick-wall limiter at 4x (inter-sample peaks are seen and caught)
        {
            const float pk = std::max (std::abs (l), std::abs (r));
            const float gt = pk > ceilLin ? ceilLin / pk : 1.0f;

            // sliding-window minimum over the look-ahead (monotonic deque, O(1) per sample)
            const int cap = (int) dqVal.size();
            while (dqCount > 0 && dqVal[(size_t) ((dqHead + dqCount - 1) % cap)] >= gt)
                --dqCount;
            const int tail = (dqHead + dqCount) % cap;
            dqVal[(size_t) tail] = gt;
            dqPos[(size_t) tail] = sampleClock;
            ++dqCount;
            while ((juce::uint32) (sampleClock - dqPos[(size_t) dqHead]) >= (juce::uint32) N)
            {
                dqHead = (dqHead + 1) % cap;
                --dqCount;
            }
            ++sampleClock;
            const float m = dqVal[(size_t) dqHead];

            rPrev = m < rPrev ? m : rc * rPrev + (1.0f - rc) * m;

            // moving average over the look-ahead: the gain glides down and arrives exactly with the peak
            rSum += (double) rPrev - (double) rBuf[(size_t) wpos];
            rBuf[(size_t) wpos] = rPrev;
            const float limGain = (float) (rSum / (double) N);
            limMinBlock = std::min (limMinBlock, limGain);

            dlL[(size_t) wpos] = l;
            dlR[(size_t) wpos] = r;
            const int rd = (wpos + 1) % N;   // sample from N-1 steps ago
            L[j] = juce::jlimit (-ceilLin, ceilLin, dlL[(size_t) rd] * limGain);
            R[j] = juce::jlimit (-ceilLin, ceilLin, dlR[(size_t) rd] * limGain);
            wpos = rd;
        }
    }

    // refresh the running sum now and then so floating-point drift can't build up
    if (sampleClock % (juce::uint32) (1 << 20) < (juce::uint32) n)
    {
        double s = 0.0;
        for (auto v : rBuf) s += (double) v;
        rSum = s;
    }
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
