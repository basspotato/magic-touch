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

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent,
                juce::Colour base = juce::Colour (0xff12151f))
{
    g.setColour (base.withAlpha (0.88f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (accent);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText (title, (int) r.getX() + 14, (int) r.getY() + 8, 300, 16, juce::Justification::left, false);
}
// level meter colour zones
juce::Colour meterZone (float db)
{
    if (db >= -1.0f)  return juce::Colour (0xffff3b4f);
    if (db >= -6.0f)  return juce::Colour (0xffffa63d);
    if (db >= -12.0f) return juce::Colour (0xffffd54a);
    return juce::Colour (0xff2fe08c);
}
} // namespace

//==============================================================================
void PillToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const float rad = b.getHeight() * 0.5f;

    g.setColour (juce::Colour (0xff151927));
    g.fillRoundedRectangle (b, rad);
    g.setColour (on ? accent.withAlpha (0.75f) : juce::Colours::white.withAlpha (over ? 0.28f : 0.13f));
    g.drawRoundedRectangle (b, rad, 1.0f);

    const float cx = b.getX() + 11.0f, cy = b.getCentreY();
    if (on)
    {
        g.setColour (led.withAlpha (0.30f));
        g.fillEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    }
    g.setColour (led);
    g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);

    g.setColour (juce::Colours::white.withAlpha (on ? 0.92f : 0.5f));
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (getButtonText(), b.withTrimmedLeft (21.0f).withTrimmedRight (8.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
}

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
    : AudioProcessorEditor (&p), proc (p), analyzer (p)
{
    setLookAndFeel (&laf);

    for (size_t i = 0; i < 10; ++i)
    {
        knobs[i] = std::make_unique<KnobCell> (proc.apvts, kParams[i].id, kParams[i].name, kKnobColours[i]);
        addAndMakeVisible (*knobs[i]);

        faders[i] = std::make_unique<FaderCell> (proc.apvts, kParams[10 + i].id, kParams[10 + i].name, kFaderColours[i]);
        addAndMakeVisible (*faders[i]);
    }

    addAndMakeVisible (analyzer);

    // master output volume (top right)
    outKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outKnob.setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
    outKnob.setTooltip ("Master output volume (double-click for 0 dB)");
    outAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "outGain", outKnob);
    outKnob.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (outKnob);

    // external sidechain switch
    scButton.setTooltip ("Glue compressor listens to the plugin's sidechain input instead of the mix. "
                         "Route a kick or any track to Magic Touch's sidechain in your DAW.");
    scAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "scExt", scButton);
    addAndMakeVisible (scButton);

    // theme: from the session if saved there, otherwise the last one you picked
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "MagicTouch";
        o.filenameSuffix      = ".settings";
        o.folderName          = "MagicTouch";
        o.osxLibrarySubFolder = "Application Support";
        settings = std::make_unique<juce::PropertiesFile> (o);

        const int t = proc.apvts.state.hasProperty ("theme") ? (int) proc.apvts.state.getProperty ("theme")
                                                              : settings->getIntValue ("theme", 0);
        setTheme (t, false);
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
    lastTickMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
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
    displayRect = row.removeFromLeft (540).toFloat();
    row.removeFromLeft (12);
    meterRect = row.removeFromRight (196).toFloat();
    row.removeFromRight (10);
    analyzer.setBounds (row);
    area.removeFromTop (12);

    tonePanel = area.removeFromLeft (540);
    area.removeFromLeft (12);
    dynPanel = area;

    presetRect = juce::Rectangle<int> (getWidth() / 2 - 190, topBar.getY() + 4, 380, 40);
    auto pr = presetRect;
    prevRect = pr.removeFromLeft (44);
    nextRect = pr.removeFromRight (44);
    nameRect = pr;

    themeRect   = juce::Rectangle<int> (topBar.getRight() - 300, topBar.getCentreY() - 13 + 2, 112, 26);
    outTextRect = juce::Rectangle<int> (topBar.getRight() - 180, topBar.getY() + 4, 118, 40);
    outKnob.setBounds (topBar.getRight() - 58, topBar.getY() - 6, 60, 60);
    scButton.setBounds (dynPanel.getRight() - 150, dynPanel.getY() + 6, 138, 20);

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
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = juce::jlimit (0.004f, 0.1f, (float) ((now - lastTickMs) * 0.001));
    lastTickMs = now;

    // ---- level meters: instant attack, 26 dB/s fall, 1.5 s peak hold
    auto meter = [dt] (MeterCh& m, float peakLin, float rmsLin)
    {
        const float pdb = juce::Decibels::gainToDecibels (peakLin, -90.0f);
        m.peak = pdb > m.peak ? pdb : std::max (pdb, m.peak - 26.0f * dt);
        const float rdb = juce::Decibels::gainToDecibels (rmsLin, -90.0f);
        m.rms += (rdb - m.rms) * (1.0f - std::exp (-dt / 0.06f));
        if (pdb >= m.hold)        { m.hold = pdb; m.holdT = 1.5f; }
        else if (m.holdT > 0.0f)  m.holdT -= dt;
        else                      m.hold = std::max (m.peak, m.hold - 15.0f * dt);
        if (peakLin > 0.9995f)    m.clip = true;
    };
    meter (mtr[0], proc.inPeakL.exchange (0.0f),  proc.inRmsL.load());
    meter (mtr[1], proc.inPeakR.exchange (0.0f),  proc.inRmsR.load());
    meter (mtr[2], proc.outPeakL.exchange (0.0f), proc.outRmsL.load());
    meter (mtr[3], proc.outPeakR.exchange (0.0f), proc.outRmsR.load());

    auto grFollow = [dt] (float& v, float db) { v = db > v ? db : std::max (db, v - 20.0f * dt); };
    grFollow (grComp, proc.compGR.exchange (0.0f));
    grFollow (grLim,  proc.limGR.exchange (0.0f));

    const float scDb = juce::Decibels::gainToDecibels (proc.scPeak.exchange (0.0f), -90.0f);
    scLevel = scDb > scLevel ? scDb : std::max (scDb, scLevel - 26.0f * dt);

    const float outNow = std::max (mtr[2].peak, mtr[3].peak);
    outMax = std::max (outMax, outNow);
    const float lk = 1.0f - std::exp (-dt / 0.08f);
    lufsMDisp += (proc.lufsM.load() - lufsMDisp) * lk;
    lufsSDisp += (proc.lufsS.load() - lufsSDisp) * lk;

    updateSpectrum (dt);

    // reactive visuals: level drives hue + aura, low-end energy drives particles
    const float lvlTarget = juce::jmap (juce::jlimit (-40.0f, 0.0f, outNow), -40.0f, 0.0f, 0.0f, 1.0f);
    level += (lvlTarget - level) * (1.0f - std::exp (-dt / 0.26f));

    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const int bassBins = juce::jlimit (2, (int) spec.size() - 2, (int) (150.0 * MagicTouchProcessor::fftSize / srate));
    float bassDb = -120.0f;
    for (int i = 1; i < bassBins; ++i)
        bassDb = std::max (bassDb, spec[(size_t) i]);
    const float bassTarget = juce::jmap (juce::jlimit (-50.0f, -8.0f, bassDb), -50.0f, -8.0f, 0.0f, 1.0f);
    bass += (bassTarget - bass) * (1.0f - std::exp (-dt / 0.15f));

    const float hueTarget = 0.52f + 0.36f * level;
    hue += (hueTarget - hue) * (1.0f - std::exp (-dt / 0.65f));

    const float frames = dt * 30.0f;   // particle motion was tuned at 30 fps
    for (auto& pt : particles)
    {
        pt.y -= pt.speed * (0.4f + bass * 3.0f) * frames;
        pt.phase += 0.05f * frames;
        if (pt.y < -10.0f)
        {
            pt.y = (float) getHeight() + 10.0f;
            pt.x = rng.nextFloat() * (float) getWidth();
        }
    }

    // push theme colours to the child components
    const auto accent = themeIndex == 0 ? themeBase() : themeBase().brighter (0.18f * level);
    analyzer.setAccent (accent);
    analyzer.setBaseColour (tinted (juce::Colour (0xff12151f), 0.05f));
    analyzer.update (dt);

    if (outKnob.findColour (juce::Slider::rotarySliderFillColourId) != themeBase())
        outKnob.setColour (juce::Slider::rotarySliderFillColourId, themeBase());

    scButton.setAccent (kFaderColours[0]);
    if (! scButton.getToggleState())
        scButton.setLed (juce::Colour (0xff3a3f52));
    else if (! proc.scConnected.load())
        scButton.setLed (juce::Colour (0xffffa63d));
    else
        scButton.setLed (kFaderColours[0].withAlpha (juce::jmap (juce::jlimit (-60.0f, 0.0f, scLevel), -60.0f, 0.0f, 0.35f, 1.0f)));

    repaint();
}

void MagicTouchEditor::updateSpectrum (float dt)
{
    proc.copyAnalyserData (fftData.data(), MagicTouchProcessor::fftSize, false);
    std::fill (fftData.begin() + MagicTouchProcessor::fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) MagicTouchProcessor::fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float atk = 1.0f - std::exp (-dt / 0.05f);
    const float rel = 1.0f - std::exp (-dt / 0.22f);
    for (size_t i = 0; i < spec.size(); ++i)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[i] / ((float) MagicTouchProcessor::fftSize * 0.25f), -120.0f);
        spec[i] += (db - spec[i]) * (db > spec[i] ? atk : rel);
    }
}

//==============================================================================
juce::Colour MagicTouchEditor::themeBase() const
{
    const auto argb = kThemes[themeIndex].argb;
    return argb == 0 ? juce::Colour::fromHSV (hue, 0.72f, 1.0f, 1.0f) : juce::Colour (argb);
}

juce::Colour MagicTouchEditor::tinted (juce::Colour c, float amount) const
{
    return c.interpolatedWith (themeBase(), themeIndex == 0 ? amount * 0.5f : amount);
}

void MagicTouchEditor::setTheme (int index, bool saveAsDefault)
{
    themeIndex = juce::jlimit (0, kNumThemes - 1, index);
    proc.apvts.state.setProperty ("theme", themeIndex, nullptr);

    if (saveAsDefault && settings != nullptr)
    {
        settings->setValue ("theme", themeIndex);
        settings->saveIfNeeded();
    }

    const auto c = kThemes[themeIndex].argb == 0 ? juce::Colour (0xff2a3350)
                                                 : juce::Colour (0xff10131c).interpolatedWith (juce::Colour (kThemes[themeIndex].argb), 0.35f);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, c);
    repaint();
}

void MagicTouchEditor::showThemeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("COLOUR THEME");
    for (int i = 0; i < kNumThemes; ++i)
    {
        const auto argb = kThemes[i].argb;
        menu.addColouredItem (i + 1, kThemes[i].name,
                              argb == 0 ? juce::Colours::white.withAlpha (0.85f) : juce::Colour (argb),
                              true, i == themeIndex);
    }

    juce::Component::SafePointer<MagicTouchEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (themeRect))
                            .withMinimumWidth (themeRect.getWidth()),
                        [safe] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->setTheme (result - 1, true);
                        });
}

//==============================================================================
void MagicTouchEditor::paint (juce::Graphics& g)
{
    const auto accent = themeIndex == 0 ? themeBase() : themeBase().brighter (0.18f * level);
    const auto panelBase = tinted (juce::Colour (0xff12151f), 0.05f);

    // background, lightly tinted by the theme
    g.setGradientFill (juce::ColourGradient (tinted (juce::Colour (0xff0e1018), 0.07f), 0.0f, 0.0f,
                                             tinted (juce::Colour (0xff06070b), 0.03f), 0.0f, (float) getHeight(), false));
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

    drawPanel (g, tonePanel.toFloat(), "TONE  &  COLOUR", accent, panelBase);
    drawPanel (g, dynPanel.toFloat(),  "DYNAMICS  &  OUTPUT", accent, panelBase);

    if (scButton.getToggleState() && ! proc.scConnected.load())
    {
        g.setColour (juce::Colour (0xffffa63d).withAlpha (0.85f));
        g.setFont (uiFont (9.0f, true));
        g.drawText ("NO SIDECHAIN ROUTED", scButton.getX() - 130, scButton.getY(), 122, scButton.getHeight(),
                    juce::Justification::centredRight, false);
    }

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
    g.setColour (tinted (juce::Colour (0xff151927), 0.05f));
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

    // theme selector
    {
        const auto tr = themeRect.toFloat();
        g.setColour (tinted (juce::Colour (0xff151927), 0.05f));
        g.fillRoundedRectangle (tr, 8.0f);
        g.setColour (accent.withAlpha (0.35f));
        g.drawRoundedRectangle (tr.reduced (0.5f), 8.0f, 1.0f);

        const float cx = tr.getX() + 15.0f, cy = tr.getCentreY();
        if (kThemes[themeIndex].argb == 0)
        {
            for (int k = 0; k < 6; ++k)   // little colour wheel for "Reactive"
            {
                juce::Path seg;
                seg.addPieSegment (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f,
                                   (float) k * kPi / 3.0f, (float) (k + 1) * kPi / 3.0f, 0.0f);
                g.setColour (juce::Colour::fromHSV ((float) k / 6.0f, 0.7f, 1.0f, 1.0f));
                g.fillPath (seg);
            }
        }
        else
        {
            g.setColour (accent.withAlpha (0.3f));
            g.fillEllipse (cx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
            g.setColour (themeBase());
            g.fillEllipse (cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.setFont (uiFont (8.0f, true).withExtraKerningFactor (0.15f));
        g.drawText ("THEME", (int) tr.getX() + 28, (int) tr.getY() + 3, 60, 9, juce::Justification::left, false);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (uiFont (11.5f, true));
        g.drawText (kThemes[themeIndex].name, (int) tr.getX() + 28, (int) tr.getY() + 11, 70, 13, juce::Justification::left, false);

        juce::Path caret;   // small down arrow
        const float ax = tr.getRight() - 13.0f;
        caret.startNewSubPath (ax - 3.5f, cy - 1.5f);
        caret.lineTo (ax, cy + 2.0f);
        caret.lineTo (ax + 3.5f, cy - 1.5f);
        g.setColour (accent);
        g.strokePath (caret, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // output volume label + value (the knob itself is a child component)
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.2f));
    g.drawText ("OUTPUT", outTextRect.withHeight (18).translated (0, 3), juce::Justification::centredRight, false);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.setFont (uiFont (15.0f, true));
    g.drawText (outKnob.getTextFromValue (outKnob.getValue()), outTextRect.withTrimmedTop (18),
                juce::Justification::centredRight, false);
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
        g.drawText ("TONE EQ  +  OUTPUT SPECTRUM", (int) r.getX() + 42, (int) r.getY() + 4, 260, 12,
                    juce::Justification::left, false);
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

void MagicTouchEditor::drawMeters (juce::Graphics& g, juce::Colour accent)
{
    const auto r = meterRect;
    drawPanel (g, r, "LEVELS", accent, tinted (juce::Colour (0xff12151f), 0.05f));

    // ---- numeric readouts: output peak (max hold), momentary + short-term loudness
    {
        const float ry = r.getY() + 28.0f, rh = 30.0f, gap = 4.0f;
        const float rw = (r.getWidth() - 24.0f - 2.0f * gap) / 3.0f;
        auto cell = [&] (int k, const char* label, float value, float minShown, juce::Colour vc)
        {
            const juce::Rectangle<float> c (r.getX() + 12.0f + (float) k * (rw + gap), ry, rw, rh);
            g.setColour (juce::Colour (0xff07080d));
            g.fillRoundedRectangle (c, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (c.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.42f));
            g.setFont (uiFont (7.5f, true).withExtraKerningFactor (0.12f));
            g.drawText (label, c.withHeight (12.0f).translated (0.0f, 2.0f).toNearestInt(), juce::Justification::centred, false);
            g.setColour (vc);
            g.setFont (uiFont (12.5f, true));
            g.drawText (value <= minShown ? juce::String ("-inf") : juce::String (value, 1),
                        c.withTrimmedTop (12.0f).toNearestInt(), juce::Justification::centred, false);
        };
        cell (0, "PEAK",   outMax,    -89.0f, outMax > -0.05f ? juce::Colour (0xffff3b4f) : juce::Colours::white.withAlpha (0.92f));
        cell (1, "LUFS M", lufsMDisp, -69.0f, accent.brighter (0.3f));
        cell (2, "LUFS S", lufsSDisp, -69.0f, accent.brighter (0.3f));
        meterResetRect = { r.getX(), ry, r.getWidth(), 50.0f };
    }

    // ---- bar geometry (pixel-aligned for crisp edges)
    const float top    = std::round (r.getY() + 74.0f);
    const float bottom = std::round (r.getBottom() - 24.0f);
    const float H      = bottom - top;
    const float b0     = std::round (r.getX() + (r.getWidth() - 138.0f) * 0.5f);
    const float bw     = 8.0f;

    // more resolution near the top: position = t^2 over a 60 dB range
    auto yOf  = [=] (float db) { const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); return bottom - t * t * H; };
    auto dbOf = [=] (float y)  { return std::sqrt (juce::jlimit (0.0f, 1.0f, (bottom - y) / H)) * 60.0f - 60.0f; };
    auto yGr  = [=] (float db) { return top + juce::jlimit (0.0f, 18.0f, db) / 18.0f * H; };

    auto levelBar = [&] (float x, const MeterCh& m)
    {
        g.setColour (juce::Colour (0xff06070b));
        g.fillRect (x, top, bw, H);
        const float yPk = yOf (m.peak), yRms = yOf (m.rms);
        for (float y = bottom - 2.0f; y >= top; y -= 3.0f)
        {
            const float a = y >= yRms - 1.0f ? 1.0f : (y >= yPk - 1.0f ? 0.40f : 0.07f);
            g.setColour (meterZone (dbOf (y + 1.0f)).withAlpha (a));
            g.fillRect (x, y, bw, 2.0f);
        }
        if (m.hold > -59.0f)
        {
            g.setColour (meterZone (m.hold).brighter (0.4f));
            g.fillRect (x - 1.0f, std::round (yOf (m.hold)) - 1.0f, bw + 2.0f, 2.0f);
        }
    };

    auto grBar = [&] (float x, float db, juce::Colour c)
    {
        g.setColour (juce::Colour (0xff06070b));
        g.fillRect (x, top, bw, H);
        const float yEnd = yGr (db);
        for (float y = top; y + 2.0f <= bottom; y += 3.0f)
        {
            g.setColour (c.withAlpha (y + 2.0f <= yEnd + 0.5f ? 1.0f : 0.07f));
            g.fillRect (x, y, bw, 2.0f);
        }
    };

    auto clipLed = [&] (float x, float w, bool on)
    {
        const juce::Rectangle<float> c (x, top - 9.0f, w, 5.0f);
        if (on)
        {
            g.setColour (juce::Colour (0xffff3b4f).withAlpha (0.35f));
            g.fillRect (c.expanded (1.5f));
        }
        g.setColour (on ? juce::Colour (0xffff3b4f) : juce::Colour (0xff1a1e2a));
        g.fillRect (c);
    };

    // IN | scale | OUT
    clipLed (b0, 18.0f, mtr[0].clip || mtr[1].clip);
    clipLed (b0 + 44.0f, 18.0f, mtr[2].clip || mtr[3].clip);
    levelBar (b0,         mtr[0]);
    levelBar (b0 + 10.0f, mtr[1]);
    levelBar (b0 + 44.0f, mtr[2]);
    levelBar (b0 + 54.0f, mtr[3]);

    // dB scale between IN and OUT
    {
        g.setFont (uiFont (8.0f));
        for (int db = 0; db >= -12; --db)   // 1 dB minor ticks where it matters
        {
            const float y = std::round (yOf ((float) db));
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.fillRect (b0 + 19.0f, y, 2.0f, 1.0f);
            g.fillRect (b0 + 41.0f, y, 2.0f, 1.0f);
        }
        const int marks[] = { 0, -3, -6, -9, -12, -18, -24, -36, -48 };
        for (int db : marks)
        {
            const float y = std::round (yOf ((float) db));
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRect (b0 + 19.0f, y, 4.0f, 1.0f);
            g.fillRect (b0 + 39.0f, y, 4.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (juce::String (db), (int) b0 + 20, (int) y - 5, 22, 10, juce::Justification::centred, false);
        }
    }

    // gain reduction: glue compressor + limiter, 0..18 dB downward
    grBar (b0 + 80.0f, grComp, kFaderColours[0]);
    grBar (b0 + 91.0f, grLim,  kFaderColours[6]);
    {
        g.setFont (uiFont (8.0f));
        for (int db = 0; db <= 18; db += 3)
        {
            const float y = std::round (yGr ((float) db));
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRect (b0 + 100.0f, y, 3.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (juce::String (db), (int) b0 + 104, (int) y - 5, 16, 10, juce::Justification::centredLeft, false);
        }
        g.setFont (uiFont (8.5f, true));
        g.setColour (kFaderColours[0].withAlpha (0.9f));
        g.drawText (juce::String (grComp, 1), (int) b0 + 74, (int) top - 12, 20, 10, juce::Justification::centred, false);
        g.setColour (kFaderColours[6].withAlpha (0.9f));
        g.drawText (juce::String (grLim, 1), (int) b0 + 87, (int) top - 12, 20, 10, juce::Justification::centred, false);
    }

    // external sidechain input level
    {
        const bool active = scButton.getToggleState() && proc.scConnected.load();
        const float x = b0 + 128.0f;
        g.setColour (juce::Colour (0xff06070b));
        g.fillRect (x, top, 10.0f, H);
        const float yLvl = yOf (scLevel);
        for (float y = bottom - 2.0f; y >= top; y -= 3.0f)
        {
            const bool lit = y >= yLvl - 1.0f;
            g.setColour (kFaderColours[0].withAlpha (lit ? (active ? 0.95f : 0.30f) : 0.07f));
            g.fillRect (x, y, 10.0f, 2.0f);
        }
    }

    // labels
    g.setFont (uiFont (8.5f, true).withExtraKerningFactor (0.1f));
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    auto label = [&] (float cx, const char* t)
    {
        g.drawText (t, (int) cx - 20, (int) bottom + 6, 40, 11, juce::Justification::centred, false);
    };
    label (b0 + 9.0f,   "IN");
    label (b0 + 53.0f,  "OUT");
    label (b0 + 94.5f,  "GR");
    label (b0 + 133.0f, "SC");
}

//==============================================================================
void MagicTouchEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (prevRect.contains (pt))       stepPreset (-1);
    else if (nextRect.contains (pt))  stepPreset (1);
    else if (nameRect.contains (pt))  showPresetMenu();
    else if (themeRect.contains (pt)) showThemeMenu();
    else if (meterResetRect.contains (e.position) || meterRect.withTop (meterRect.getY() + 60.0f).withHeight (20.0f).contains (e.position))
    {
        // click the readouts or clip lights to reset peak holds / clip indicators
        outMax = -90.0f;
        for (auto& m : mtr) { m.clip = false; m.hold = m.peak; m.holdT = 0.0f; }
    }
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
