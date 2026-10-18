#include "PluginEditor.h"

using namespace MT;

namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;

const juce::Colour kKnobColours[10] =
{
    juce::Colour (0xff3ba7ff), juce::Colour (0xff35d0c4), juce::Colour (0xffa8e05f), juce::Colour (0xffa0e7ff),
    juce::Colour (0xffffa63d), juce::Colour (0xffff7a45), juce::Colour (0xffff5cb8), juce::Colour (0xffb583ff),
    juce::Colour (0xff6c8cff), juce::Colour (0xffff5252)
};

const juce::Colour kFaderColours[10] =
{
    juce::Colour (0xff2ee6a6), juce::Colour (0xff2ee6a6), juce::Colour (0xff2ee6a6), juce::Colour (0xff2ee6a6),
    juce::Colour (0xff2ee6a6), juce::Colour (0xff2ee6a6),
    juce::Colour (0xffff4d7d), juce::Colour (0xffff4d7d), juce::Colour (0xffff4d7d),
    juce::Colour (0xffe8ecff)
};

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    g.setColour (juce::Colour (0xff12151f).withAlpha (0.88f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (accent);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText (title, (int) r.getX() + 14, (int) r.getY() + 8, 300, 16, juce::Justification::left, false);
}
} // namespace

//==============================================================================
KnobCell::KnobCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                    const juce::String& t, juce::Colour c)
    : title (t), colour (c), attachment (apvts, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void KnobCell::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedBottom (34));
}

void KnobCell::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    b = b.removeFromBottom (34);
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.1f));
    g.drawText (title.toUpperCase(), b.removeFromTop (16), juce::Justification::centred, false);
    g.setColour (colour.brighter (0.35f));
    g.setFont (uiFont (12.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), b, juce::Justification::centred, false);
}

//==============================================================================
FaderCell::FaderCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                      const juce::String& t, juce::Colour c)
    : title (t), colour (c), attachment (apvts, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::thumbColourId, c);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void FaderCell::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (18).withTrimmedBottom (20));
}

void FaderCell::paint (juce::Graphics& g)
{
    auto lb = getLocalBounds();
    auto topR = lb.removeFromTop (18);
    auto botR = lb.removeFromBottom (20);

    g.setColour (juce::Colours::white.withAlpha (0.72f));
    g.setFont (uiFont (9.5f, true));
    g.drawText (title.toUpperCase(), topR.expanded (4, 0), juce::Justification::centred, false);

    g.setColour (colour.brighter (0.3f));
    g.setFont (uiFont (10.5f));
    g.drawText (slider.getTextFromValue (slider.getValue()), botR.expanded (4, 0), juce::Justification::centred, false);
}

//==============================================================================
MagicTouchEditor::MagicTouchEditor (MagicTouchProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    for (size_t i = 0; i < 10; ++i)
    {
        knobs[i] = std::make_unique<KnobCell> (proc.apvts, kParams[i].id, kParams[i].name, kKnobColours[i]);
        addAndMakeVisible (*knobs[i]);

        faders[i] = std::make_unique<FaderCell> (proc.apvts, kParams[10 + i].id, kParams[10 + i].name, kFaderColours[i]);
        addAndMakeVisible (*faders[i]);
    }

    spec.fill (-120.0f);

    for (auto& pt : particles)
    {
        pt.x = rng.nextFloat() * 1100.0f;
        pt.y = rng.nextFloat() * 720.0f;
        pt.speed = 0.15f + rng.nextFloat() * 0.6f;
        pt.size = 1.5f + rng.nextFloat() * 3.0f;
        pt.phase = rng.nextFloat() * 6.28f;
    }

    setSize (1100, 720);
    startTimerHz (30);
}

MagicTouchEditor::~MagicTouchEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MagicTouchEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    topBar = area.removeFromTop (48);
    area.removeFromTop (10);

    auto row = area.removeFromTop (268);
    meterRect = row.removeFromRight (150).toFloat();
    row.removeFromRight (10);
    displayRect = row.toFloat();
    area.removeFromTop (12);

    tonePanel = area.removeFromLeft (540);
    area.removeFromLeft (12);
    dynPanel = area;

    presetRect = juce::Rectangle<int> (getWidth() / 2 - 190, topBar.getY() + 4, 380, 40);
    auto pr = presetRect;
    prevRect = pr.removeFromLeft (44);
    nextRect = pr.removeFromRight (44);
    nameRect = pr;

    {
        auto inner = tonePanel.reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (6);
        const int cw = inner.getWidth() / 5, ch = inner.getHeight() / 2;
        for (int i = 0; i < 10; ++i)
            knobs[(size_t) i]->setBounds (inner.getX() + (i % 5) * cw, inner.getY() + (i / 5) * ch, cw, ch);
    }
    {
        auto inner = dynPanel.reduced (10, 0).withTrimmedTop (56).withTrimmedBottom (8);
        const int fw = inner.getWidth() / 10;
        for (int i = 0; i < 10; ++i)
            faders[(size_t) i]->setBounds (inner.getX() + i * fw, inner.getY(), fw, inner.getHeight());
    }
}

//==============================================================================
void MagicTouchEditor::timerCallback()
{
    auto follow = [] (float& v, float lin)
    {
        const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
        v = db > v ? db : std::max (db, v - 1.8f);
    };
    follow (mIn[0],  proc.inPeakL.exchange (0.0f));
    follow (mIn[1],  proc.inPeakR.exchange (0.0f));
    follow (mOut[0], proc.outPeakL.exchange (0.0f));
    follow (mOut[1], proc.outPeakR.exchange (0.0f));

    auto grFollow = [] (float& v, float db) { v = db > v ? db : std::max (db, v - 0.5f); };
    grFollow (grComp, proc.compGR.exchange (0.0f));
    grFollow (grLim,  proc.limGR.exchange (0.0f));

    const float outNow = std::max (mOut[0], mOut[1]);
    outHold = outNow > outHold ? outNow : outHold - 0.25f;

    if (proc.fftReady.exchange (false))
        updateSpectrum();

    // reactive visuals: level drives hue + aura, low-end energy drives particles
    const float lvlTarget = juce::jmap (juce::jlimit (-40.0f, 0.0f, outNow), -40.0f, 0.0f, 0.0f, 1.0f);
    level += (lvlTarget - level) * 0.12f;

    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const int bassBins = juce::jlimit (2, (int) spec.size() - 2, (int) (150.0 * MagicTouchProcessor::fftSize / srate));
    float bassDb = -120.0f;
    for (int i = 1; i < bassBins; ++i)
        bassDb = std::max (bassDb, spec[(size_t) i]);
    const float bassTarget = juce::jmap (juce::jlimit (-50.0f, -8.0f, bassDb), -50.0f, -8.0f, 0.0f, 1.0f);
    bass += (bassTarget - bass) * 0.2f;

    const float hueTarget = 0.52f + 0.36f * level;
    hue += (hueTarget - hue) * 0.05f;

    for (auto& pt : particles)
    {
        pt.y -= pt.speed * (0.4f + bass * 3.0f);
        pt.phase += 0.05f;
        if (pt.y < -10.0f)
        {
            pt.y = (float) getHeight() + 10.0f;
            pt.x = rng.nextFloat() * (float) getWidth();
        }
    }

    repaint();
}

void MagicTouchEditor::updateSpectrum()
{
    std::copy (proc.fftSnapshot.begin(), proc.fftSnapshot.end(), fftData.begin());
    std::fill (fftData.begin() + MagicTouchProcessor::fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) MagicTouchProcessor::fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    for (size_t i = 0; i < spec.size(); ++i)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[i] / ((float) MagicTouchProcessor::fftSize * 0.25f), -120.0f);
        spec[i] = db > spec[i] ? 0.5f * (db + spec[i]) : 0.82f * spec[i] + 0.18f * db;
    }
}

//==============================================================================
void MagicTouchEditor::paint (juce::Graphics& g)
{
    const auto accent = juce::Colour::fromHSV (hue, 0.72f, 1.0f, 1.0f);

    // background
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e1018), 0.0f, 0.0f,
                                             juce::Colour (0xff06070b), 0.0f, (float) getHeight(), false));
    g.fillAll();

    // aura that breathes with the music
    {
        const auto c = displayRect.getCentre();
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.05f + 0.20f * level + 0.12f * bass), c.x, c.y,
                                                 accent.withAlpha (0.0f), c.x + 700.0f, c.y + 300.0f, true));
        g.fillAll();
    }

    // floating motes, faster and brighter with the low end
    for (const auto& pt : particles)
    {
        const float a = (0.05f + 0.22f * bass + 0.08f * level) * (0.5f + 0.5f * std::sin (pt.phase));
        g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        g.fillEllipse (pt.x, pt.y, pt.size, pt.size);
    }

    drawTopBar (g, accent);
    drawDisplay (g, accent);
    drawMeters (g, accent);

    drawPanel (g, tonePanel.toFloat(), "TONE  &  COLOUR", accent);
    drawPanel (g, dynPanel.toFloat(),  "DYNAMICS  &  OUTPUT", accent);

    // group labels above the faders
    auto group = [&] (int a, int b, const juce::String& text, juce::Colour c)
    {
        const int x0 = faders[(size_t) a]->getX() + 4;
        const int x1 = faders[(size_t) b]->getRight() - 4;
        const int y = dynPanel.getY() + 34;
        g.setColour (c.withAlpha (0.55f));
        g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.15f));
        g.drawText (text, x0, y, x1 - x0, 12, juce::Justification::centred, false);
        g.setColour (c.withAlpha (0.25f));
        g.fillRect (x0, y + 15, x1 - x0, 1);
    };
    group (0, 5, "GLUE COMPRESSOR", kFaderColours[0]);
    group (6, 8, "LIMITER", kFaderColours[6]);
    group (9, 9, "OUT", kFaderColours[9]);
}

void MagicTouchEditor::drawTopBar (juce::Graphics& g, juce::Colour accent)
{
    // logo: pulsing four-point sparkle + glowing wordmark
    const float pulse = 0.5f + 0.5f * level;
    juce::Path star;
    star.addStar ({ 34.0f, 38.0f }, 4, 2.5f + 2.0f * pulse, 9.0f + 5.0f * pulse, 0.0f);
    g.setColour (accent.withAlpha (0.25f));
    g.strokePath (star, juce::PathStrokeType (5.0f));
    g.setColour (accent.brighter (0.5f));
    g.fillPath (star);

    g.setFont (uiFont (22.0f, true).withExtraKerningFactor (0.25f));
    g.setColour (accent.withAlpha (0.28f));
    g.drawText ("MAGIC TOUCH", 58, 16, 260, 30, juce::Justification::left, false);
    g.drawText ("MAGIC TOUCH", 60, 18, 260, 30, juce::Justification::left, false);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.drawText ("MAGIC TOUCH", 59, 17, 260, 30, juce::Justification::left, false);

    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.2f));
    g.drawText ("MIX & MASTER BUS PROCESSOR", 60, 46, 260, 14, juce::Justification::left, false);

    // preset selector
    g.setColour (juce::Colour (0xff151927));
    g.fillRoundedRectangle (presetRect.toFloat(), 9.0f);
    g.setColour (accent.withAlpha (0.35f));
    g.drawRoundedRectangle (presetRect.toFloat().reduced (0.5f), 9.0f, 1.0f);

    auto arrow = [&] (juce::Rectangle<int> r, int dir)
    {
        const float cx = (float) r.getCentreX(), cy = (float) r.getCentreY();
        juce::Path p;
        p.startNewSubPath (cx + 4.0f * (float) -dir, cy - 6.0f);
        p.lineTo (cx + 4.0f * (float) dir, cy);
        p.lineTo (cx + 4.0f * (float) -dir, cy + 6.0f);
        g.setColour (accent);
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    arrow (prevRect, -1);
    arrow (nextRect, 1);

    const int cur = proc.getCurrentPreset();
    const juce::String idx = cur >= 0 ? juce::String (cur + 1).paddedLeft ('0', 2) : juce::String ("--");
    const juce::String nm = cur >= 0 ? juce::String (kPresets[(size_t) cur].name) : juce::String ("Init / Custom");

    g.setColour (accent);
    g.setFont (uiFont (13.0f, true));
    g.drawText (idx, nameRect.withWidth (36).translated (8, 0), juce::Justification::centredLeft, false);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (uiFont (14.0f));
    g.drawText (nm, nameRect.withTrimmedLeft (46), juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.15f));
    g.drawText ("20 PRESETS  |  20 CONTROLS", topBar.getRight() - 260, topBar.getY() + 18, 260, 14,
                juce::Justification::right, false);
}

void MagicTouchEditor::drawDisplay (juce::Graphics& g, juce::Colour accent)
{
    const auto r = displayRect;
    g.setColour (juce::Colour (0xff080a10));
    g.fillRoundedRectangle (r, 10.0f);

    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;

    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);

        const float x0 = r.getX() + 38.0f, x1 = r.getRight() - 38.0f;
        const float top = r.getY() + 16.0f, bottom = r.getBottom() - 22.0f;
        const float cy = 0.5f * (top + bottom);

        auto fx = [=] (float f) { return x0 + (x1 - x0) * std::log (f / 20.0f) / std::log (1000.0f); };
        auto sy = [=] (float db) { return juce::jmap (juce::jlimit (-84.0f, -6.0f, db), -84.0f, -6.0f, bottom, top); };

        // grid
        g.setFont (uiFont (9.5f));
        const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        const char* fl[]    = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
        for (int i = 0; i < 8; ++i)
        {
            const float x = fx (freqs[i]);
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (x, top, 1.0f, bottom - top);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawText (fl[i], (int) x - 20, (int) bottom + 4, 40, 14, juce::Justification::centred, false);
        }
        for (int db = -20; db >= -80; db -= 20)
        {
            const float y = sy ((float) db);
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (x0, y, x1 - x0, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.drawText (juce::String (db), (int) r.getX() + 4, (int) y - 7, 30, 14, juce::Justification::centredRight, false);
        }

        // spectrum
        {
            juce::Path fill, line;
            const float step = 2.0f;
            const float binHz = (float) srate / (float) MagicTouchProcessor::fftSize;
            const int maxBin = (int) spec.size() - 2;
            bool first = true;

            for (float x = x0; x <= x1; x += step)
            {
                const float t0 = (x - x0) / (x1 - x0);
                const float t1 = juce::jmin (1.0f, (x + step - x0) / (x1 - x0));
                const float f0 = 20.0f * std::pow (1000.0f, t0);
                const float f1 = 20.0f * std::pow (1000.0f, t1);
                const float b0 = f0 / binHz, b1 = f1 / binHz;

                float v;
                if (b1 - b0 < 1.0f)
                {
                    const float b = juce::jlimit (1.0f, (float) maxBin, 0.5f * (b0 + b1));
                    const int i = (int) b;
                    const float fr = b - (float) i;
                    v = spec[(size_t) i] * (1.0f - fr) + spec[(size_t) i + 1] * fr;
                }
                else
                {
                    const int i0 = juce::jlimit (1, maxBin, (int) b0);
                    const int i1 = juce::jlimit (i0, maxBin, (int) std::ceil (b1));
                    v = -200.0f;
                    for (int i = i0; i <= i1; ++i)
                        v = std::max (v, spec[(size_t) i]);
                }

                v += 4.5f * std::log2 (std::sqrt (f0 * f1) / 1000.0f);   // pink-noise style tilt
                const float y = sy (v);

                if (first)
                {
                    line.startNewSubPath (x, y);
                    fill.startNewSubPath (x, bottom);
                    fill.lineTo (x, y);
                    first = false;
                }
                else
                {
                    line.lineTo (x, y);
                    fill.lineTo (x, y);
                }
            }
            fill.lineTo (x1, bottom);
            fill.closeSubPath();

            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.50f), 0.0f, top,
                                                     accent.withAlpha (0.0f), 0.0f, bottom, false));
            g.fillPath (fill);
            g.setColour (accent.withAlpha (0.18f));
            g.strokePath (line, juce::PathStrokeType (6.0f));
            g.setColour (accent.brighter (0.35f));
            g.strokePath (line, juce::PathStrokeType (1.6f));
        }

        // EQ response curve (+/-12 dB) drawn from the exact filters the DSP uses
        {
            auto pv = [this] (int i) { return proc.apvts.getRawParameterValue (kParams[(size_t) i].id)->load(); };
            const std::array<Coeffs::Ptr, 4> cs { makeLowShelfC (srate, pv (0)), makeLowMidC (srate, pv (1)),
                                                  makePresC (srate, pv (2)),     makeAirC (srate, pv (3)) };
            auto magDb = [&] (float f)
            {
                double m = 1.0;
                for (const auto& c : cs)
                    m *= c->getMagnitudeForFrequency ((double) f, srate);
                return (float) juce::Decibels::gainToDecibels (m);
            };
            const float scale = (bottom - top) * 0.5f * 0.9f / 12.0f;
            auto ey = [=] (float db) { return cy - juce::jlimit (-13.0f, 13.0f, db) * scale; };

            juce::Path curve, area;
            area.startNewSubPath (x0, cy);
            bool first = true;
            for (float x = x0; x <= x1; x += 4.0f)
            {
                const float f = 20.0f * std::pow (1000.0f, (x - x0) / (x1 - x0));
                const float y = ey (magDb (f));
                if (first) { curve.startNewSubPath (x, y); first = false; }
                else curve.lineTo (x, y);
                area.lineTo (x, y);
            }
            area.lineTo (x1, cy);
            area.closeSubPath();

            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (x0, cy, x1 - x0, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillPath (area);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.strokePath (curve, juce::PathStrokeType (5.0f));
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.strokePath (curve, juce::PathStrokeType (1.6f));

            // band nodes
            const float nodeF[4] = { 90.0f, 320.0f, 3200.0f, 11000.0f };
            for (int i = 0; i < 4; ++i)
            {
                const float nx = fx (nodeF[i]);
                const float ny = ey (magDb (nodeF[i]));
                g.setColour (kKnobColours[i].withAlpha (0.35f));
                g.fillEllipse (nx - 8.0f, ny - 8.0f, 16.0f, 16.0f);
                g.setColour (kKnobColours[i]);
                g.fillEllipse (nx - 4.5f, ny - 4.5f, 9.0f, 9.0f);
                g.setColour (juce::Colours::white);
                g.drawEllipse (nx - 4.5f, ny - 4.5f, 9.0f, 9.0f, 1.0f);
            }

            // gain axis on the right
            g.setFont (uiFont (9.5f));
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            const int marks[] = { 12, 6, 0, -6, -12 };
            for (int m : marks)
                g.drawText (m > 0 ? "+" + juce::String (m) : juce::String (m), (int) x1 + 4, (int) ey ((float) m) - 7,
                            30, 14, juce::Justification::centredLeft, false);
        }

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.15f));
        g.drawText ("OUTPUT SPECTRUM  +  TONE EQ", (int) r.getX() + 42, (int) r.getY() + 4, 260, 12,
                    juce::Justification::left, false);
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

void MagicTouchEditor::drawMeters (juce::Graphics& g, juce::Colour accent)
{
    const auto r = meterRect;
    drawPanel (g, r, "LEVELS", accent);

    const float top = r.getY() + 44.0f, bottom = r.getBottom() - 30.0f, bw = 12.0f;
    const float x0 = r.getX() + (r.getWidth() - 120.0f) * 0.5f;

    auto yOf = [=] (float db) { return juce::jmap (juce::jlimit (-48.0f, 0.0f, db), -48.0f, 0.0f, bottom, top); };

    juce::ColourGradient grad (juce::Colour (0xff2fe08c), 0.0f, bottom, juce::Colour (0xffff4d5e), 0.0f, top, false);
    grad.addColour (0.72, juce::Colour (0xffffd54a));

    auto levelBar = [&] (float x, float db)
    {
        g.setColour (juce::Colour (0xff0b0d14));
        g.fillRoundedRectangle (x, top, bw, bottom - top, 3.0f);
        const float y = yOf (db);
        if (y < bottom - 0.5f)
        {
            g.setGradientFill (grad);
            g.fillRoundedRectangle (x, y, bw, bottom - y, 3.0f);
        }
    };

    auto grBar = [&] (float x, float db, juce::Colour c)
    {
        g.setColour (juce::Colour (0xff0b0d14));
        g.fillRoundedRectangle (x, top, bw, bottom - top, 3.0f);
        const float y = juce::jmap (juce::jlimit (0.0f, 12.0f, db), 0.0f, 12.0f, top, bottom);
        if (y > top + 0.5f)
        {
            g.setColour (c.withAlpha (0.35f));
            g.fillRoundedRectangle (x - 2.0f, top, bw + 4.0f, y - top, 4.0f);
            g.setColour (c);
            g.fillRoundedRectangle (x, top, bw, y - top, 3.0f);
        }
    };

    levelBar (x0,          mIn[0]);
    levelBar (x0 + 15.0f,  mIn[1]);
    levelBar (x0 + 41.0f,  mOut[0]);
    levelBar (x0 + 56.0f,  mOut[1]);
    grBar    (x0 + 82.0f,  grComp, kFaderColours[0]);
    grBar    (x0 + 108.0f, grLim,  kFaderColours[6]);

    g.setFont (uiFont (9.0f, true));
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    auto label = [&] (float cx, const char* t)
    {
        g.drawText (t, (int) cx - 20, (int) bottom + 6, 40, 12, juce::Justification::centred, false);
    };
    label (x0 + 13.5f,  "IN");
    label (x0 + 54.5f,  "OUT");
    label (x0 + 88.0f,  "COMP");
    label (x0 + 114.0f, "LIMIT");

    // output peak readout
    const juce::String peakText = outHold <= -59.0f ? juce::String ("-inf") : juce::String (outHold, 1);
    g.setColour (outHold > -0.5f ? juce::Colour (0xffff4d5e) : juce::Colours::white.withAlpha (0.75f));
    g.setFont (uiFont (10.5f, true));
    g.drawText (peakText, (int) (x0 + 34.0f), (int) r.getY() + 26, 44, 14, juce::Justification::centred, false);
}

//==============================================================================
void MagicTouchEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (prevRect.contains (pt))      stepPreset (-1);
    else if (nextRect.contains (pt)) stepPreset (1);
    else if (nameRect.contains (pt)) showPresetMenu();
}

void MagicTouchEditor::stepPreset (int dir)
{
    const int n = (int) kPresets.size();
    const int cur = proc.getCurrentPreset();
    const int next = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
    proc.loadPreset (next);
    repaint();
}

void MagicTouchEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const int cur = proc.getCurrentPreset();
    for (int i = 0; i < (int) kPresets.size(); ++i)
        menu.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 2) + "   " + kPresets[(size_t) i].name,
                      true, i == cur);

    juce::Component::SafePointer<MagicTouchEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (nameRect))
                            .withMinimumWidth (nameRect.getWidth()),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->proc.loadPreset (result - 1);
                                safe->repaint();
                            }
                        });
}
