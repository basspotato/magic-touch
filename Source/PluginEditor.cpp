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
    juce::Colour (0xffffc93c), juce::Colour (0xffffc93c), juce::Colour (0xffffc93c), juce::Colour (0xffffc93c),
    juce::Colour (0xffffc93c), juce::Colour (0xffffc93c),
    juce::Colour (0xffff4d7d), juce::Colour (0xffff4d7d), juce::Colour (0xffff4d7d),
    juce::Colour (0xffe8ecff)
};

void drawPanelTitle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    const float ly = r.getY() + 16.0f, lx = r.getX() + 16.0f;
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (lx - 5.0f, ly - 5.0f, 10.0f, 10.0f);
    g.setColour (accent.brighter (0.4f));
    g.fillEllipse (lx - 2.5f, ly - 2.5f, 5.0f, 5.0f);
    g.setColour (accent);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText (title, (int) r.getX() + 26, (int) r.getY() + 8, 300, 16, juce::Justification::left, false);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent,
                juce::Colour base = juce::Colour (0xff12151f), bool withTitle = true)
{
    // recessed into the chassis: outer lip highlight, body, inner top shadow
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.expanded (1.0f).translated (0.0f, 1.0f), 11.0f, 1.0f);
    g.setColour (base.withAlpha (0.92f));
    g.fillRoundedRectangle (r, 10.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), 0.0f, r.getY(),
                                                 juce::Colours::transparentBlack, 0.0f, r.getY() + 14.0f, false));
        g.fillRect (r.withHeight (14.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.025f), 0.0f, r.getY() + 14.0f,
                                                 juce::Colours::transparentWhite, 0.0f, r.getY() + 120.0f, false));
        g.fillRect (r);
    }
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    if (withTitle)
        drawPanelTitle (g, r, title, accent);
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
    // scale end labels just outside the arc ends
    {
        const auto sb = slider.getBounds().toFloat();
        const auto c = sb.getCentre();
        const float R = juce::jmin (sb.getWidth(), sb.getHeight()) * 0.5f - 2.0f;
        auto fmt = [] (double v, bool sign) { return (sign && v > 0 ? "+" : "") + juce::String ((int) std::round (v)); };
        const bool bip = slider.getMinimum() < 0.0;
        g.setFont (uiFont (8.5f));
        g.setColour (juce::Colours::white.withAlpha (0.38f));
        for (int k = 0; k < 2; ++k)
        {
            const float a = kPi * (k == 0 ? 1.25f : 2.75f);
            const juce::Point<float> p (c.x + (R - 2.0f) * std::sin (a), c.y - (R - 2.0f) * std::cos (a));
            const auto txt = fmt (k == 0 ? slider.getMinimum() : slider.getMaximum(), bip);
            g.drawText (txt, juce::Rectangle<float> (40.0f, 12.0f).withCentre (p.translated (k == 0 ? -6.0f : 6.0f, 9.0f)).toNearestInt(),
                        juce::Justification::centred, false);
        }
    }

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

    // controls keep a cached image of themselves and only re-render when they move,
    // so the music-reactive background can refresh around them cheaply
    for (auto& k : knobs)  k->setBufferedToImage (true);
    for (auto& f : faders) f->setBufferedToImage (true);
    outKnob.setBufferedToImage (true);

    setOpaque (true);
    setSize (1100, 720);
    lastTickMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);   // meters/displays refresh; static parts are cached, so each frame is cheap
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

    themeRect   = juce::Rectangle<int> (topBar.getRight() - 308, topBar.getCentreY() - 13 + 2, 112, 26);
    outTextRect = juce::Rectangle<int> (topBar.getRight() - 188, topBar.getY() + 4, 118, 40);
    outKnob.setBounds (topBar.getRight() - 66, topBar.getY() - 6, 60, 60);
    scButton.setBounds (dynPanel.getRight() - 150, dynPanel.getY() + 6, 138, 20);
    starRect     = { 12, 16, 46, 46 };
    outValueRect = outTextRect.withTrimmedTop (18);
    scHintRect   = { scButton.getX() - 130, scButton.getY(), 122, scButton.getHeight() };
    bgDirty = true;
    displayGlass = {};
    eqCached.fill (99.0f);

    // where the reactive glow is visible: everything except panel interiors and the static top-bar controls
    gapRegion = juce::RectangleList<int> (getLocalBounds());
    for (const auto& pr : { displayRect.toNearestInt(), analyzer.getBounds(), meterRect.toNearestInt(), tonePanel, dynPanel })
        gapRegion.subtract (pr.reduced (3));


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

    {
        const float rl = proc.outRmsL.load(), rr = proc.outRmsR.load();
        const float rdb = 10.0f * std::log10 (0.5f * (rl * rl + rr * rr) + 1.0e-12f);
        rmsOut += (juce::jmax (-90.0f, rdb) - rmsOut) * (1.0f - std::exp (-dt / 0.06f));
    }

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
    const auto accent = liveAccent();
    analyzer.setAccent (accent);
    analyzer.setBaseColour (themeIndex == 0 ? juce::Colour (0xff12151f) : tinted (juce::Colour (0xff12151f), 0.05f));
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

    // ---- does the cached static layer need rebuilding?
    const bool scHint = scButton.getToggleState() && ! proc.scConnected.load();
    if (proc.getCurrentPreset() != bgPreset || scHint != bgScHint)
        bgDirty = true;
    if (themeIndex == 0 && std::abs (hue - bgHue) > 0.02f && now - lastBgBuildMs > 500.0)
        bgDirty = true;   // "Reactive" theme: refresh titles as the hue drifts, at most twice a second

    if (bgDirty)
    {
        repaint();
        return;
    }

    // ---- the reactive glow, particles and titles behind/around the panels refresh at 15 fps;
    //      the meters and displays at the full 30 fps
    if ((++frameCount & 1) == 0)
    {
        for (const auto& gr : gapRegion)
            repaint (gr);
        repaint (tonePanel.withHeight (30));
        repaint (dynPanel.withHeight (30));
    }
    repaint (starRect);
    repaint (displayRect.toNearestInt());
    repaint (meterRect.toNearestInt());
    if (std::abs (outKnob.getValue() - lastOutValue) > 1.0e-6)
    {
        lastOutValue = outKnob.getValue();
        repaint (outValueRect);
    }
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

juce::Colour MagicTouchEditor::liveAccent() const
{
    if (themeIndex == 0)
        return themeBase();   // "Reactive": the hue sweeps across the spectrum with level

    // every other theme reacts around its own colour: the hue leans warmer/cooler and the
    // colour brightens and saturates as the music gets louder and the low end hits
    const juce::Colour b (kThemes[themeIndex].argb);
    float h = b.getHue() + 0.035f * level + 0.015f * bass;   // a gentle lean, the theme stays itself
    h -= std::floor (h);
    const float sat = juce::jlimit (0.0f, 1.0f, b.getSaturation() * (0.85f + 0.25f * level));
    const float bri = juce::jlimit (0.0f, 1.0f, b.getBrightness() * (0.75f + 0.35f * level));
    return juce::Colour::fromHSV (h, sat, bri, 1.0f).brighter (0.12f * level + 0.08f * bass);
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
    bgDirty = true;
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
void MagicTouchEditor::buildChassis()
{
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    chassis = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (chassis);
    const auto b = chassis.getBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15171e), 0.0f, 0.0f,
                                             juce::Colour (0xff07080b), 0.0f, (float) h, false));
    g.fillAll();

    // brushed-metal grain: fine horizontal streaks
    juce::Random rnd (7);
    for (int y = 0; y < h; ++y)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.010f + rnd.nextFloat() * 0.022f));
        g.fillRect (0, y, w, 1);
        if (rnd.nextFloat() < 0.35f)
        {
            const int x0 = rnd.nextInt (w), len = 40 + rnd.nextInt (300);
            g.setColour (juce::Colours::white.withAlpha (0.02f + rnd.nextFloat() * 0.03f));
            g.fillRect (x0, y, len, 1);
        }
    }

    // vignette
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                             juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true));
    g.fillAll();

    // bezel
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (b.reduced (1.0f), 12.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (b.reduced (3.0f), 10.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (4.0f), 10.0f, 1.0f);

    // engraved separator under the top bar
    const float sy = (float) topBar.getBottom() + 5.0f;
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRect (24.0f, sy, b.getWidth() - 48.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (24.0f, sy + 1.0f, b.getWidth() - 48.0f, 1.0f);

    // corner screws
    auto screw = [&g] (float cx, float cy, float ang)
    {
        const float r = 4.5f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (cx - r - 1.0f, cy - r, r * 2.0f + 2.0f, r * 2.0f + 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8a8f9c), cx - r, cy - r,
                                                 juce::Colour (0xff23252c), cx + r, cy + r, false));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff111216));
        for (int k = 0; k < 2; ++k)
        {
            const float a = ang + (float) k * juce::MathConstants<float>::halfPi;
            g.drawLine (cx - std::cos (a) * r * 0.7f, cy - std::sin (a) * r * 0.7f,
                        cx + std::cos (a) * r * 0.7f, cy + std::sin (a) * r * 0.7f, 1.4f);
        }
    };
    screw (11.0f, 11.0f, 0.3f);
    screw (b.getRight() - 11.0f, 11.0f, 1.1f);
    screw (11.0f, b.getBottom() - 11.0f, 0.7f);
    screw (b.getRight() - 11.0f, b.getBottom() - 11.0f, 0.1f);
}

//==============================================================================
void MagicTouchEditor::paint (juce::Graphics& g)
{
    if (bgDirty || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        buildBackground();

    const auto accent = liveAccent();
    const auto clip = g.getClipBounds();

    g.drawImageAt (background, 0, 0);

    // music-reactive layer: glow, drifting motes and panel rim lights, drawn only in the gaps
    // between panels (where they're visible), so it costs a fraction of a full-window effect
    if (gapRegion.intersects (clip))
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gapRegion);

        const auto c = displayRect.getCentre();
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.05f + 0.20f * level + 0.12f * bass), c.x, c.y,
                                                 accent.withAlpha (0.0f), c.x + 700.0f, c.y + 300.0f, true));
        g.fillRect (clip);

        for (const auto& pt : particles)
        {
            const float a = (0.06f + 0.26f * bass + 0.10f * level) * (0.5f + 0.5f * std::sin (pt.phase));
            g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (pt.x, pt.y, pt.size, pt.size);
        }

        const float rim = juce::jlimit (0.0f, 1.0f, 0.06f + 0.26f * level + 0.18f * bass);
        for (const auto& pr : { displayRect, analyzer.getBounds().toFloat(), meterRect, tonePanel.toFloat(), dynPanel.toFloat() })
        {
            g.setColour (accent.withAlpha (rim * 0.25f));
            g.drawRoundedRectangle (pr.expanded (2.5f), 12.5f, 3.0f);
            g.setColour (accent.withAlpha (rim * 0.7f));
            g.drawRoundedRectangle (pr.expanded (1.0f), 11.0f, 1.0f);
        }
    }

    if (clip.intersects (tonePanel.withHeight (30)))
        drawPanelTitle (g, tonePanel.toFloat(), "TONE  &  COLOUR", accent);
    if (clip.intersects (dynPanel.withHeight (30)))
        drawPanelTitle (g, dynPanel.toFloat(), "DYNAMICS  &  OUTPUT", accent);

    if (clip.intersects (topBar.withY (0).withHeight (topBar.getBottom() + 4)))
    {
        drawTopBar (g, themeBase(), true);   // logo, preset box, theme selector (drawn over the glow)
        drawTopBar (g, accent, false);       // pulsing star + output value
    }
    if (clip.intersects (displayRect.toNearestInt()))
        drawDisplay (g, accent, false);
    if (clip.intersects (meterRect.toNearestInt()))
        drawMeters (g, accent, false);
}

void MagicTouchEditor::buildBackground()
{
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    if (chassis.getWidth() != w || chassis.getHeight() != h)
        buildChassis();

    background = juce::Image (juce::Image::RGB, w, h, false);
    juce::Graphics g (background);
    const auto accent = themeBase();
    const auto panelBase = tinted (juce::Colour (0xff12151f), 0.05f);

    g.drawImageAt (chassis, 0, 0);
    g.setColour (themeBase().withAlpha (themeIndex == 0 ? 0.025f : 0.045f));
    g.fillAll();

    drawDisplay (g, accent, true);
    drawMeters (g, accent, true);

    drawPanel (g, tonePanel.toFloat(), "TONE  &  COLOUR", accent, panelBase, false);
    drawPanel (g, dynPanel.toFloat(),  "DYNAMICS  &  OUTPUT", accent, panelBase, false);

    bgScHint = scButton.getToggleState() && ! proc.scConnected.load();
    if (bgScHint)
    {
        g.setColour (juce::Colour (0xffffa63d).withAlpha (0.85f));
        g.setFont (uiFont (9.0f, true));
        g.drawText ("NO SIDECHAIN ROUTED", scHintRect, juce::Justification::centredRight, false);
    }

    // group labels above the faders
    auto group = [&] (int a, int b, const juce::String& text, juce::Colour c)
    {
        const int x0 = faders[(size_t) a]->getX() + 4;
        const int x1 = faders[(size_t) b]->getRight() - 4;
        const int y = dynPanel.getY() + 34;
        g.setColour (c.withAlpha (0.65f));
        g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.15f));
        g.drawText (text, x0, y, x1 - x0, 12, juce::Justification::centred, false);
        g.setColour (c.withAlpha (0.25f));
        g.fillRect (x0, y + 15, x1 - x0, 1);
    };
    group (0, 5, "GLUE COMPRESSOR", kFaderColours[0]);
    group (6, 8, "LIMITER", kFaderColours[6]);
    group (9, 9, "OUT", kFaderColours[9]);

    bgPreset = proc.getCurrentPreset();
    bgHue = hue;
    lastBgBuildMs = juce::Time::getMillisecondCounterHiRes();
    bgDirty = false;
}

void MagicTouchEditor::drawTopBar (juce::Graphics& g, juce::Colour accent, bool staticPart)
{
    if (! staticPart)
    {
        // logo: pulsing four-point sparkle
        const float pulse = 0.5f + 0.5f * level;
        juce::Path star;
        star.addStar ({ 34.0f, 38.0f }, 4, 2.5f + 2.0f * pulse, 9.0f + 5.0f * pulse, 0.0f);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (star, juce::PathStrokeType (5.0f));
        g.setColour (accent.brighter (0.5f));
        g.fillPath (star);

        // output volume value (the knob itself is a child component)
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.setFont (uiFont (15.0f, true));
        g.drawText (outKnob.getTextFromValue (outKnob.getValue()), outValueRect, juce::Justification::centredRight, false);
        return;
    }

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

    // output volume label
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.2f));
    g.drawText ("OUTPUT", outTextRect.withHeight (18).translated (0, 3), juce::Justification::centredRight, false);
}

// EQ display geometry, shared by the static and dynamic layers
namespace
{
struct DispGeom
{
    float x0, x1, top, bottom, cy;
    float fx (float f) const   { return x0 + (x1 - x0) * std::log (f / 20.0f) / std::log (1000.0f); }
    float sy (float db) const  { return juce::jmap (juce::jlimit (-84.0f, -6.0f, db), -84.0f, -6.0f, bottom, top); }
    float ey (float db) const  { return cy - juce::jlimit (-13.0f, 13.0f, db) * (bottom - top) * 0.5f * 0.9f / 12.0f; }
};
DispGeom dispGeom (juce::Rectangle<float> r)
{
    const float top = r.getY() + 16.0f, bottom = r.getBottom() - 22.0f;
    return { r.getX() + 38.0f, r.getRight() - 38.0f, top, bottom, 0.5f * (top + bottom) };
}
} // namespace

void MagicTouchEditor::updateEqCurve()
{
    auto pv = [this] (int i) { return proc.apvts.getRawParameterValue (kParams[(size_t) i].id)->load(); };
    const std::array<float, 4> eq { pv (0), pv (1), pv (2), pv (3) };
    if (eq == eqCached && ! eqCurve.isEmpty())
        return;
    eqCached = eq;

    const auto d = dispGeom (displayRect);
    const double prate = proc.getProcessingRate();
    const std::array<Coeffs::Ptr, 4> cs { makeLowShelfC (prate, eq[0]), makeLowMidC (prate, eq[1]),
                                          makePresC (prate, eq[2]),     makeAirC (prate, eq[3]) };
    auto magDb = [&] (float f)
    {
        double m = 1.0;
        for (const auto& c : cs)
            m *= c->getMagnitudeForFrequency ((double) f, prate);
        return (float) juce::Decibels::gainToDecibels (m);
    };

    eqCurve.clear();
    eqArea.clear();
    eqArea.startNewSubPath (d.x0, d.cy);
    bool first = true;
    for (float x = d.x0; x <= d.x1; x += 3.0f)
    {
        const float f = 20.0f * std::pow (1000.0f, (x - d.x0) / (d.x1 - d.x0));
        const float y = d.ey (magDb (f));
        if (first) { eqCurve.startNewSubPath (x, y); first = false; }
        else eqCurve.lineTo (x, y);
        eqArea.lineTo (x, y);
    }
    eqArea.lineTo (d.x1, d.cy);
    eqArea.closeSubPath();

    const float nodeF[4] = { kLowF, kBodyF, kPresF, kAirF };
    for (size_t k = 0; k < 4; ++k)
        eqNodes[k] = { d.fx (nodeF[k]), d.ey (magDb (nodeF[k])) };
}

void MagicTouchEditor::drawDisplay (juce::Graphics& g, juce::Colour accent, bool staticPart)
{
    const auto r = displayRect;
    const auto d = dispGeom (r);

    if (staticPart)
    {
        g.setColour (juce::Colour (0xff080a10));
        g.fillRoundedRectangle (r, 10.0f);

        g.setFont (uiFont (9.5f));
        const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        const char* fl[]    = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
        for (int i = 0; i < 8; ++i)
        {
            const float x = std::round (d.fx (freqs[i]));
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (x, d.top, 1.0f, d.bottom - d.top);
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawText (fl[i], (int) x - 20, (int) d.bottom + 4, 40, 14, juce::Justification::centred, false);
        }
        for (int db = -20; db >= -80; db -= 20)
        {
            const float y = std::round (d.sy ((float) db));
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (d.x0, y, d.x1 - d.x0, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.drawText (juce::String (db), (int) r.getX() + 4, (int) y - 7, 30, 14, juce::Justification::centredRight, false);
        }

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRect (d.x0, std::round (d.cy), d.x1 - d.x0, 1.0f);
        g.setFont (uiFont (9.5f));
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        const int marks[] = { 12, 6, 0, -6, -12 };
        for (int m : marks)
            g.drawText (m > 0 ? "+" + juce::String (m) : juce::String (m), (int) d.x1 + 4, (int) d.ey ((float) m) - 7,
                        30, 14, juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.15f));
        g.drawText ("TONE EQ  +  OUTPUT SPECTRUM", (int) r.getX() + 42, (int) r.getY() + 4, 260, 12,
                    juce::Justification::left, false);
        return;
    }

    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (d.x0, d.top - 4.0f, d.x1 - d.x0, d.bottom - d.top + 4.0f).toNearestInt());

        // spectrum
        {
            juce::Path fill, line;
            const float step = 3.0f;
            const float binHz = (float) srate / (float) MagicTouchProcessor::fftSize;
            const int maxBin = (int) spec.size() - 2;
            bool first = true;

            for (float x = d.x0; x <= d.x1; x += step)
            {
                const float t0 = (x - d.x0) / (d.x1 - d.x0);
                const float t1 = juce::jmin (1.0f, (x + step - d.x0) / (d.x1 - d.x0));
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
                const float y = d.sy (v);

                if (first)
                {
                    line.startNewSubPath (x, y);
                    fill.startNewSubPath (x, d.bottom);
                    fill.lineTo (x, y);
                    first = false;
                }
                else
                {
                    line.lineTo (x, y);
                    fill.lineTo (x, y);
                }
            }
            fill.lineTo (d.x1, d.bottom);
            fill.closeSubPath();

            g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.50f), 0.0f, d.top,
                                                     accent.withAlpha (0.0f), 0.0f, d.bottom, false));
            g.fillPath (fill);
            g.setColour (accent.withAlpha (0.18f));
            g.strokePath (line, juce::PathStrokeType (4.0f));
            g.setColour (accent.brighter (0.35f));
            g.strokePath (line, juce::PathStrokeType (1.5f));
        }

        // EQ response curve (cached; rebuilt only when an EQ knob moves)
        updateEqCurve();
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillPath (eqArea);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.strokePath (eqCurve, juce::PathStrokeType (4.0f));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.strokePath (eqCurve, juce::PathStrokeType (1.6f));
    }

    for (size_t k = 0; k < 4; ++k)
    {
        const auto n = eqNodes[k];
        g.setColour (kKnobColours[k].withAlpha (0.35f));
        g.fillEllipse (n.x - 8.0f, n.y - 8.0f, 16.0f, 16.0f);
        g.setColour (kKnobColours[k]);
        g.fillEllipse (n.x - 4.5f, n.y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (n.x - 4.5f, n.y - 4.5f, 9.0f, 9.0f, 1.0f);
    }

    // glass finish, pre-rendered once
    if (displayGlass.getWidth() != (int) r.getWidth() + 4)
    {
        displayGlass = juce::Image (juce::Image::ARGB, (int) r.getWidth() + 4, (int) r.getHeight() + 4, true);
        juce::Graphics gg (displayGlass);
        drawGlass (gg, juce::Rectangle<float> (2.0f, 2.0f, r.getWidth(), r.getHeight()), 10.0f);
    }
    g.drawImageAt (displayGlass, (int) r.getX() - 2, (int) r.getY() - 2);
}

void MagicTouchEditor::drawMeters (juce::Graphics& g, juce::Colour accent, bool staticPart)
{
    const auto r = meterRect;
    if (staticPart)
        drawPanel (g, r, "LEVELS", accent, tinted (juce::Colour (0xff12151f), 0.05f), false);
    else
        drawPanelTitle (g, r, "LEVELS", accent);

    // ---- numeric readouts: output peak (max hold), momentary + short-term loudness
    {
        const float ry = r.getY() + 28.0f, rh = 30.0f, gap = 4.0f;
        const float rw = (r.getWidth() - 24.0f - 2.0f * gap) / 3.0f;
        auto cell = [&] (int k, const char* label, float value, float minShown, juce::Colour vc)
        {
            const juce::Rectangle<float> c (r.getX() + 12.0f + (float) k * (rw + gap), ry, rw, rh);
            if (staticPart)
            {
                g.setColour (juce::Colour (0xff07080d));
                g.fillRoundedRectangle (c, 4.0f);
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.drawRoundedRectangle (c.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.42f));
                g.setFont (uiFont (7.5f, true).withExtraKerningFactor (0.12f));
                g.drawText (label, c.withHeight (12.0f).translated (0.0f, 2.0f).toNearestInt(), juce::Justification::centred, false);
                return;
            }
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
    //      IN | dB scale | OUT    GR comp, GR lim | GR scale    RMS   SC
    const float top    = std::round (r.getY() + 74.0f);
    const float bottom = std::round (r.getBottom() - 24.0f);
    const float H      = bottom - top;
    const float b0     = std::round (r.getX() + (r.getWidth() - 158.0f) * 0.5f);
    const float bw     = 8.0f;
    const float xRms = b0 + 124.0f, xSc = b0 + 148.0f, wide = 10.0f;

    // more resolution near the top: position = t^2 over a 60 dB range
    auto yOf  = [=] (float db) { const float t = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); return bottom - t * t * H; };
    auto dbOf = [=] (float y)  { return std::sqrt (juce::jlimit (0.0f, 1.0f, (bottom - y) / H)) * 60.0f - 60.0f; };
    auto yGr  = [=] (float db) { return top + juce::jlimit (0.0f, 18.0f, db) / 18.0f * H; };

    // segmented bar: static layer draws the unlit segments, dynamic layer only the lit ones
    auto bar = [&] (float x, float w, float yLit, float yDim, std::function<juce::Colour (float)> colourAt)
    {
        if (staticPart)
        {
            g.setColour (juce::Colour (0xff06070b));
            g.fillRect (x, top, w, H);
        }
        for (float y = bottom - 2.0f; y >= top; y -= 3.0f)
        {
            float a;
            if (staticPart)                a = 0.07f;
            else if (y >= yLit - 1.0f)     a = 1.0f;
            else if (y >= yDim - 1.0f)     a = 0.40f;
            else                           break;
            g.setColour (colourAt (y).withAlpha (a));
            g.fillRect (x, y, w, 2.0f);
        }
    };
    auto zoneAt = [&] (float y) { return meterZone (dbOf (y + 1.0f)); };

    auto levelBar = [&] (float x, const MeterCh& m)
    {
        bar (x, bw, yOf (m.rms), yOf (m.peak), zoneAt);
        if (! staticPart && m.hold > -59.0f)
        {
            g.setColour (meterZone (m.hold).brighter (0.4f));
            g.fillRect (x - 1.0f, std::round (yOf (m.hold)) - 1.0f, bw + 2.0f, 2.0f);
        }
    };

    auto grBar = [&] (float x, float db, juce::Colour c)
    {
        if (staticPart)
        {
            g.setColour (juce::Colour (0xff06070b));
            g.fillRect (x, top, bw, H);
        }
        const float yEnd = yGr (db);
        for (float y = top; y + 2.0f <= bottom; y += 3.0f)
        {
            const bool lit = y + 2.0f <= yEnd + 0.5f;
            if (! staticPart && ! lit) break;
            g.setColour (c.withAlpha (staticPart ? 0.07f : 1.0f));
            g.fillRect (x, y, bw, 2.0f);
        }
    };

    auto clipLed = [&] (float x, float w, bool on)
    {
        const juce::Rectangle<float> c (x, top - 9.0f, w, 5.0f);
        if (staticPart)
        {
            g.setColour (juce::Colour (0xff1a1e2a));
            g.fillRect (c);
        }
        else if (on)
        {
            g.setColour (juce::Colour (0xffff3b4f).withAlpha (0.35f));
            g.fillRect (c.expanded (1.5f));
            g.setColour (juce::Colour (0xffff3b4f));
            g.fillRect (c);
        }
    };

    // IN | scale | OUT
    clipLed (b0, 18.0f, mtr[0].clip || mtr[1].clip);
    clipLed (b0 + 44.0f, 18.0f, mtr[2].clip || mtr[3].clip);
    levelBar (b0,         mtr[0]);
    levelBar (b0 + 10.0f, mtr[1]);
    levelBar (b0 + 44.0f, mtr[2]);
    levelBar (b0 + 54.0f, mtr[3]);

    // gain reduction: glue compressor + limiter, 0..18 dB downward
    grBar (b0 + 80.0f, grComp, kFaderColours[0]);
    grBar (b0 + 91.0f, grLim,  kFaderColours[6]);

    // output RMS (300 ms, both channels)
    bar (xRms, wide, yOf (rmsOut), yOf (rmsOut), zoneAt);

    // external sidechain input level
    {
        const bool active = scButton.getToggleState() && proc.scConnected.load();
        const auto scCol = kFaderColours[0].withAlpha (active ? 1.0f : 0.32f);
        bar (xSc, wide, yOf (scLevel), yOf (scLevel), [scCol] (float) { return scCol; });
    }

    if (staticPart)
    {
        // dB scale between IN and OUT
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

        // GR scale
        for (int db = 0; db <= 18; db += 3)
        {
            const float y = std::round (yGr ((float) db));
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.fillRect (b0 + 100.0f, y, 3.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (juce::String (db), (int) b0 + 104, (int) y - 5, 16, 10, juce::Justification::centredLeft, false);
        }

        // labels
        g.setFont (uiFont (8.5f, true).withExtraKerningFactor (0.1f));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        auto label = [&] (float cx, const char* t)
        {
            g.drawText (t, (int) cx - 20, (int) bottom + 6, 40, 11, juce::Justification::centred, false);
        };
        label (b0 + 9.0f,          "IN");
        label (b0 + 53.0f,         "OUT");
        label (b0 + 94.5f,         "GR");
        label (xRms + wide * 0.5f, "RMS");
        label (xSc + wide * 0.5f,  "SC");
        return;
    }

    // live numbers above the GR and RMS bars
    g.setFont (uiFont (8.5f, true));
    g.setColour (kFaderColours[0].withAlpha (0.9f));
    g.drawText (juce::String (grComp, 1), (int) b0 + 74, (int) top - 12, 20, 10, juce::Justification::centred, false);
    g.setColour (kFaderColours[6].withAlpha (0.9f));
    g.drawText (juce::String (grLim, 1), (int) b0 + 87, (int) top - 12, 20, 10, juce::Justification::centred, false);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawText (rmsOut <= -69.0f ? juce::String ("-inf") : juce::String (rmsOut, 1),
                (int) (xRms + wide * 0.5f) - 14, (int) top - 12, 28, 10, juce::Justification::centred, false);
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
    bgDirty = true;
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
                                safe->bgDirty = true;
                                safe->repaint();
                            }
                        });
}
