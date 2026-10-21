#include "PluginEditor.h"

using namespace MT;

namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;

const juce::Colour kCream { 0xfffff1d8 };   // lit white for pointers and key readouts

// Fader light ramps (bottom -> top), all in the console palette: compressor orange -> yellow,
// limiter orange -> red, output yellow -> white. The fill intensifies with the value.
const juce::Colour kFaderLo[10] =
{
    col::orange, col::orange, col::orange, col::orange, col::orange, col::orange,
    col::orange, col::orange, col::orange,
    col::yellow
};
const juce::Colour kFaderHi[10] =
{
    col::yellow, col::yellow, col::yellow, col::yellow, col::yellow, col::yellow,
    col::faceRed, col::faceRed, col::faceRed,
    juce::Colour (0xfffff1d8)
};

// Group accents (labels, dividers, gain-reduction bars): compressor yellow, limiter red, output white
const juce::Colour kFaderColours[10] =
{
    juce::Colour (0xffffd23f), juce::Colour (0xffffd23f), juce::Colour (0xffffd23f), juce::Colour (0xffffd23f),
    juce::Colour (0xffffd23f), juce::Colour (0xffffd23f),
    juce::Colour (0xffff3d1a), juce::Colour (0xffff3d1a), juce::Colour (0xffff3d1a),
    juce::Colour (0xfff5efe6)
};

// Recessed inset window used for the preset and theme selectors
void insetWindow (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::line.withAlpha (0.35f));
    g.fillRoundedRectangle (r.expanded (1.0f), 4.0f);
    g.setColour (juce::Colour (0xff080504));
    g.fillRoundedRectangle (r, 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX(), r.getY() + 7.0f, false));
    g.fillRoundedRectangle (r, 3.0f);
}

// Section header: a thin rule either side of a lit, ramp-filled title with an LED at each end of the text
void drawPanelTitle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour lo, juce::Colour hi)
{
    const auto f = uiFont (12.5f, true).withExtraKerningFactor (0.2f);
    const float x = r.getX() + 14.0f, w = r.getWidth() - 28.0f, cy = r.getY() + 15.0f;
    const float tw = juce::GlyphArrangement::getStringWidth (f, title) + 32.0f;
    const float mid = x + w * 0.5f, l = mid - tw * 0.5f, rt = mid + tw * 0.5f;

    g.setGradientFill (juce::ColourGradient (col::line.withAlpha (0.0f), x, cy, col::line.withAlpha (0.45f), l, cy, false));
    g.fillRect (x, cy, l - x, 1.0f);
    g.setGradientFill (juce::ColourGradient (col::line.withAlpha (0.45f), rt, cy, col::line.withAlpha (0.0f), x + w, cy, false));
    g.fillRect (rt, cy, x + w - rt, 1.0f);
    drawLed (g, juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ l + 5.0f, cy + 0.5f }), true, lo);
    drawLed (g, juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ rt - 5.0f, cy + 0.5f }), true, lo);
    g.setFont (f);
    glowTextRamp (g, title, juce::Rectangle<float> (l + 10.0f, cy - 9.0f, tw - 20.0f, 17.0f), juce::Justification::centred, lo, hi, 0.25f);
}

// Dark console tile with a thin ramp border, a light strip along the top edge and bright corner brackets
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title,
                juce::Colour lo, juce::Colour hi, bool withTitle = true)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 4.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141210), 0.0f, r.getY(), juce::Colour (0xff0b0a09), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 9.0f);
    g.setGradientFill (juce::ColourGradient (lo.withAlpha (0.06f), 0.0f, r.getY(), lo.withAlpha (0.0f), 0.0f, r.getY() + 46.0f, false));
    g.fillRoundedRectangle (r, 9.0f);
    g.setGradientFill (juce::ColourGradient (lo.withAlpha (0.3f), r.getX(), 0.0f, hi.withAlpha (0.3f), r.getRight(), 0.0f, false));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);

    // light strip along the top edge
    const float lx0 = r.getX() + 26.0f, lx1 = r.getRight() - 26.0f;
    auto strip = juce::ColourGradient (lo.withAlpha (0.0f), lx0, 0.0f, hi.withAlpha (0.0f), lx1, 0.0f, false);
    strip.addColour (0.15, lo.withAlpha (0.9f));
    strip.addColour (0.85, hi.withAlpha (0.9f));
    auto glow = strip;
    glow.multiplyOpacity (0.22f);
    g.setGradientFill (glow);
    g.fillRoundedRectangle (lx0, r.getY() - 1.5f, lx1 - lx0, 6.0f, 3.0f);
    g.setGradientFill (strip);
    g.fillRect (lx0, r.getY() + 0.5f, lx1 - lx0, 1.6f);

    // corner brackets
    g.setGradientFill (juce::ColourGradient (lo.withAlpha (0.95f), r.getX(), 0.0f, hi.withAlpha (0.95f), r.getRight(), 0.0f, false));
    const float L = 14.0f, t = 2.0f, x0 = r.getX() + 3.0f, y0 = r.getY() + 3.0f, x1 = r.getRight() - 3.0f, y1 = r.getBottom() - 3.0f;
    g.fillRect (x0, y0, L, t);         g.fillRect (x0, y0, t, L);
    g.fillRect (x1 - L, y0, L, t);     g.fillRect (x1 - t, y0, t, L);
    g.fillRect (x0, y1 - t, L, t);     g.fillRect (x0, y1 - L, t, L);
    g.fillRect (x1 - L, y1 - t, L, t); g.fillRect (x1 - t, y1 - L, t, L);

    if (withTitle)
        drawPanelTitle (g, r, title, lo, hi);
}

// Potato emblem (ported from Potato-Kicker): orange/yellow line art with a calm face, drawn small for the
// top bar. The whole figure is drawn once into the cached background; only the pupils are redrawn per frame
// (pupilsOnly), glowing with the output level.
juce::Path potatoPath (juce::Point<float> c, float R)
{
    const float radii[16] { 1.02f, 0.97f, 0.90f, 0.93f, 0.99f, 0.95f, 0.91f, 0.98f, 1.06f, 1.03f, 0.94f, 0.90f, 0.96f, 1.04f, 1.00f, 1.05f };
    const float tilt = juce::degreesToRadians (-10.0f), ct = std::cos (tilt), st = std::sin (tilt);
    juce::Point<float> pts[16];
    for (int i = 0; i < 16; ++i)
    {
        const float a = juce::degreesToRadians ((float) i * 22.5f);
        const float x = std::cos (a) * R * 1.22f * radii[i], y = std::sin (a) * R * 1.0f * radii[i];
        pts[i] = c + juce::Point<float> (x * ct - y * st, x * st + y * ct);
    }
    juce::Path p;
    auto mid = [&] (int i, int j) { return (pts[i] + pts[j]) * 0.5f; };
    p.startNewSubPath (mid (15, 0));
    for (int i = 0; i < 16; ++i)
        p.quadraticTo (pts[i], mid (i, (i + 1) % 16));
    p.closeSubPath();
    return p;
}

void drawPotatoBadge (juce::Graphics& g, juce::Point<float> c, float R, float act, bool pupilsOnly)
{
    auto eyeCentre = [&] (int i) { return c + juce::Point<float> (i == 0 ? -0.34f : 0.30f, i == 0 ? -0.16f : -0.19f) * R; };
    auto pupils = [&] (float a)
    {
        for (int i = 0; i < 2; ++i)
        {
            const float pr = R * (0.085f + 0.025f * a);
            const auto pc = eyeCentre (i) + juce::Point<float> (R * 0.025f, R * 0.03f);
            g.setColour (col::amber.withAlpha (0.25f + 0.4f * a));
            g.fillEllipse (juce::Rectangle<float> (pr * 4.0f, pr * 4.0f).withCentre (pc));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff3c8), pc.x - pr * 0.3f, pc.y - pr * 0.3f,
                                                     col::yellow, pc.x + pr, pc.y + pr, true));
            g.fillEllipse (juce::Rectangle<float> (pr * 2.0f, pr * 2.0f).withCentre (pc));
        }
    };

    if (pupilsOnly) { pupils (act); return; }

    const juce::Path spud = potatoPath (c, R);

    // backlight, body and glowing outline
    g.setGradientFill (juce::ColourGradient (col::amber.withAlpha (0.16f), c.x, c.y, col::amber.withAlpha (0.0f), c.x + R * 1.7f, c.y, true));
    g.fillEllipse (juce::Rectangle<float> (R * 3.4f, R * 2.7f).withCentre (c));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a200c), c.x - R * 0.35f, c.y - R * 0.45f,
                                             juce::Colour (0xff0d0704), c.x + R * 0.95f, c.y + R * 0.8f, true));
    g.fillPath (spud);
    const auto sb = spud.getBounds();
    auto rampFill = [&] (float a) { return juce::ColourGradient (col::orange.withAlpha (a), sb.getX(), sb.getBottom(),
                                                                  col::yellow.withAlpha (a), sb.getRight(), sb.getY(), false); };
    const juce::PathStrokeType::JointStyle js = juce::PathStrokeType::curved;
    const auto ec = juce::PathStrokeType::rounded;
    g.setGradientFill (rampFill (0.16f)); g.strokePath (spud, juce::PathStrokeType (5.0f, js, ec));
    g.setGradientFill (rampFill (0.40f)); g.strokePath (spud, juce::PathStrokeType (2.6f, js, ec));
    g.setGradientFill (rampFill (1.0f));  g.strokePath (spud, juce::PathStrokeType (1.3f, js, ec));

    // skin highlight and the little sprout spots
    juce::Path hl;
    hl.addCentredArc (c.x - R * 0.1f, c.y + R * 0.02f, R * 1.0f, R * 0.66f, juce::degreesToRadians (-14.0f),
                      juce::degreesToRadians (-78.0f), juce::degreesToRadians (-22.0f), true);
    g.setColour (col::amberHi.withAlpha (0.35f));
    g.strokePath (hl, juce::PathStrokeType (1.0f, js, ec));
    g.setColour (col::amber.withAlpha (0.5f));
    for (auto p : { juce::Point<float> (-0.86f, 0.34f), juce::Point<float> (0.88f, -0.40f), juce::Point<float> (0.66f, 0.52f),
                    juce::Point<float> (-0.48f, 0.62f), juce::Point<float> (-0.10f, -0.70f) })
        g.drawEllipse (juce::Rectangle<float> (2.6f, 1.8f).withCentre (c + p * R), 0.8f);

    // face: round eyes with glowing pupils, small smile
    for (int i = 0; i < 2; ++i)
    {
        const auto socket = juce::Rectangle<float> (R * 0.30f, R * 0.36f).withCentre (eyeCentre (i));
        g.setColour (juce::Colour (0xff080402)); g.fillEllipse (socket);
        g.setColour (col::amber.withAlpha (0.85f)); g.drawEllipse (socket, 1.0f);
    }
    pupils (0.0f);
    juce::Path mouth;
    mouth.startNewSubPath (c.x - R * 0.22f, c.y + R * 0.30f);
    mouth.quadraticTo (c.x - R * 0.02f, c.y + R * 0.46f, c.x + R * 0.22f, c.y + R * 0.27f);
    g.setColour (col::amber.withAlpha (0.3f)); g.strokePath (mouth, juce::PathStrokeType (3.0f, js, ec));
    g.setColour (col::yellow);                 g.strokePath (mouth, juce::PathStrokeType (1.4f, js, ec));
}

// level meter colour: a smooth fade, orange at the bottom -> yellow -> red at the top
juce::Colour meterFade (float db)
{
    if (db <= -24.0f) return col::orange;
    if (db < -6.0f)   return ramp (col::orange, col::yellow, (db + 24.0f) / 18.0f);
    if (db < 0.0f)    return ramp (col::yellow, col::faceRed, (db + 6.0f) / 6.0f);
    return col::faceRed;
}
} // namespace

//==============================================================================
void PillToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const float rad = b.getHeight() * 0.5f;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1713), 0.0f, b.getY(), juce::Colour (0xff0e0b09), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, rad);
    g.setColour (on ? accent.withAlpha (0.75f) : col::line.withAlpha (over ? 0.75f : 0.45f));
    g.drawRoundedRectangle (b, rad, 1.0f);

    const float cx = b.getX() + 11.0f, cy = b.getCentreY();
    if (on)
    {
        g.setColour (led.withAlpha (0.30f));
        g.fillEllipse (cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    }
    g.setColour (led);
    g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);

    g.setColour (on ? col::amberHi : col::ink.withAlpha (0.8f));
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (getButtonText(), b.withTrimmedLeft (21.0f).withTrimmedRight (8.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
}

//==============================================================================
KnobCell::KnobCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                    const juce::String& t)
    : title (t), attachment (apvts, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void KnobCell::setRamp (juce::Colour lo, juce::Colour hi)
{
    rampLo = lo;
    rampHi = hi;
    slider.setColour (juce::Slider::rotarySliderFillColourId, lo);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, hi);
    slider.repaint();
    repaint();
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
        g.setFont (monoFont (9.5f));
        g.setColour (col::inkDim.withAlpha (0.8f));
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
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.1f));
    glowText (g, title.toUpperCase(), b.removeFromTop (16).toFloat(), juce::Justification::centred, col::ink, 0.22f);
    const float norm = (float) slider.valueToProportionOfLength (slider.getValue());
    g.setColour (slider.isMouseButtonDown() ? ramp (rampLo, rampHi, norm).brighter (0.4f) : juce::Colour (0xffb98c62));
    g.setFont (monoFont (12.5f));
    g.drawText (slider.getTextFromValue (slider.getValue()), b, juce::Justification::centred, false);
}

//==============================================================================
FaderCell::FaderCell (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId,
                      const juce::String& t, juce::Colour lo, juce::Colour hi)
    : title (t), rampLo (lo), rampHi (hi), attachment (apvts, paramId, slider)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, lo);
    slider.setColour (juce::Slider::thumbColourId, hi);
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

    g.setFont (uiFont (9.5f, true));
    glowText (g, title.toUpperCase(), topR.expanded (4, 0).toFloat(), juce::Justification::centred, col::ink, 0.22f);

    g.setColour (ramp (rampLo, rampHi, (float) slider.valueToProportionOfLength (slider.getValue())).brighter (0.3f));
    g.setFont (monoFont (10.5f));
    g.drawText (slider.getTextFromValue (slider.getValue()), botR.expanded (4, 0), juce::Justification::centred, false);
}

//==============================================================================
MagicTouchEditor::MagicTouchEditor (MagicTouchProcessor& p)
    : AudioProcessorEditor (&p), proc (p), analyzer (p)
{
    setLookAndFeel (&laf);

    for (size_t i = 0; i < 10; ++i)
    {
        knobs[i] = std::make_unique<KnobCell> (proc.apvts, kParams[i].id, kParams[i].name);
        addAndMakeVisible (*knobs[i]);

        faders[i] = std::make_unique<FaderCell> (proc.apvts, kParams[10 + i].id, kParams[10 + i].name, kFaderLo[i], kFaderHi[i]);
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
                         "Route a kick or any track to Potato Magic's sidechain in your DAW.");
    scAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "scExt", scButton);
    addAndMakeVisible (scButton);

    // Potato-Kicker light ramp for the knob rings and the analyzer frame
    for (auto& k : knobs)
        k->setRamp (rampLo, rampHi);
    outKnob.setColour (juce::Slider::rotarySliderFillColourId, rampLo);
    outKnob.setColour (juce::Slider::rotarySliderOutlineColourId, rampHi);
    analyzer.setBaseColour (juce::Colour (0xff0e0b09));
    analyzer.setRamp (rampLo, rampHi);

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

    outTextRect = juce::Rectangle<int> (topBar.getRight() - 188, topBar.getY() + 4, 118, 40);
    outKnob.setBounds (topBar.getRight() - 66, topBar.getY() - 6, 60, 60);
    scButton.setBounds (dynPanel.getRight() - 150, dynPanel.getY() + 6, 138, 20);
    starRect     = { 12, 16, 46, 46 };

    // potato badge: centred in the gap between the title block and the preset selector
    {
        const float titleW = juce::GlyphArrangement::getStringWidth (uiFont (22.0f, true).withExtraKerningFactor (0.3f), "POTATO MAGIC");
        const float subW   = juce::GlyphArrangement::getStringWidth (monoFont (10.5f), "MIX & MASTER BUS PROCESSOR");
        const float textEnd = 60.0f + std::max (titleW, subW);
        badgeRect = juce::Rectangle<int> (60, 44).withCentre ({ (int) ((textEnd + (float) presetRect.getX()) * 0.5f), topBar.getCentreY() });
    }
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

    // reactive visuals: level drives the aura, low-end energy drives particles
    const float lvlTarget = juce::jmap (juce::jlimit (-40.0f, 0.0f, outNow), -40.0f, 0.0f, 0.0f, 1.0f);
    level += (lvlTarget - level) * (1.0f - std::exp (-dt / 0.26f));

    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const int bassBins = juce::jlimit (2, (int) spec.size() - 2, (int) (150.0 * MagicTouchProcessor::fftSize / srate));
    float bassDb = -120.0f;
    for (int i = 1; i < bassBins; ++i)
        bassDb = std::max (bassDb, spec[(size_t) i]);
    const float bassTarget = juce::jmap (juce::jlimit (-50.0f, -8.0f, bassDb), -50.0f, -8.0f, 0.0f, 1.0f);
    bass += (bassTarget - bass) * (1.0f - std::exp (-dt / 0.15f));

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

    // push the music-reactive accent to the analyzer
    const auto accent = liveAccent();
    analyzer.setAccent (accent);
    analyzer.update (dt);

    scButton.setAccent (kFaderColours[0]);
    if (! scButton.getToggleState())
        scButton.setLed (juce::Colour (0xff3a2210));
    else if (! proc.scConnected.load())
        scButton.setLed (juce::Colour (0xffffa63d));
    else
        scButton.setLed (kFaderColours[0].withAlpha (juce::jmap (juce::jlimit (-60.0f, 0.0f, scLevel), -60.0f, 0.0f, 0.35f, 1.0f)));

    // ---- does the cached static layer need rebuilding?
    const bool scHint = scButton.getToggleState() && ! proc.scConnected.load();
    if (proc.getCurrentPreset() != bgPreset || scHint != bgScHint)
        bgDirty = true;

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
    }
    repaint (starRect);
    repaint (badgeRect);
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
juce::Colour MagicTouchEditor::liveAccent() const
{
    // Potato-Kicker orange that reacts to the music: it leans toward yellow and brightens and saturates
    // as the music gets louder and the low end hits
    const juce::Colour b = col::amber;
    float h = b.getHue() + 0.035f * level + 0.015f * bass;
    h -= std::floor (h);
    const float sat = juce::jlimit (0.0f, 1.0f, b.getSaturation() * (0.85f + 0.25f * level));
    const float bri = juce::jlimit (0.0f, 1.0f, b.getBrightness() * (0.75f + 0.35f * level));
    return juce::Colour::fromHSV (h, sat, bri, 1.0f).brighter (0.12f * level + 0.08f * bass);
}

//==============================================================================
void MagicTouchEditor::buildChassis()
{
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    chassis = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (chassis);
    const auto b = chassis.getBounds().toFloat();

    // console page: near-black with a soft vignette
    g.fillAll (col::page);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, b.getCentreX(), b.getCentreY(),
                                             juce::Colours::black.withAlpha (0.45f), 0.0f, 0.0f, true));
    g.fillAll();
}

//==============================================================================
void MagicTouchEditor::paint (juce::Graphics& g)
{
    if (bgDirty || background.getWidth() != getWidth() || background.getHeight() != getHeight())
        buildBackground();

    const auto accent = liveAccent();
    const auto clip = g.getClipBounds();

    g.drawImageAt (background, 0, 0);

    // music-reactive layer: drifting motes, drawn only in the gaps
    // between panels (where they're visible), so it costs a fraction of a full-window effect
    if (gapRegion.intersects (clip))
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (gapRegion);

        for (const auto& pt : particles)
        {
            const float a = (0.06f + 0.26f * bass + 0.10f * level) * (0.5f + 0.5f * std::sin (pt.phase));
            g.setColour (accent.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (pt.x, pt.y, pt.size, pt.size);
        }
    }

    if (clip.intersects (topBar.withY (0).withHeight (topBar.getBottom() + 4)))
    {
        drawTopBar (g, col::amber, true);    // title, preset box (drawn over the background)
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
    const auto accent = col::amber;

    g.drawImageAt (chassis, 0, 0);

    // soft light from the top of the page, and a hairline under the top bar
    g.setGradientFill (juce::ColourGradient (rampLo.withAlpha (0.07f), (float) w * 0.5f, 0.0f,
                                             rampLo.withAlpha (0.0f), (float) w * 0.5f, (float) h * 0.7f, true));
    g.fillRect (0, 0, w, h);
    {
        const float sy = (float) topBar.getBottom() + 5.0f;
        auto hair = juce::ColourGradient (rampLo.withAlpha (0.0f), 24.0f, sy, rampHi.withAlpha (0.0f), (float) w - 24.0f, sy, false);
        hair.addColour (0.12, rampLo.withAlpha (0.45f));
        hair.addColour (0.88, rampHi.withAlpha (0.45f));
        g.setGradientFill (hair);
        g.fillRect (24.0f, sy, (float) w - 48.0f, 1.0f);
    }

    drawDisplay (g, accent, true);
    drawMeters (g, accent, true);

    drawPanel (g, tonePanel.toFloat(), "TONE  &  COLOUR", rampLo, rampHi);
    drawPanel (g, dynPanel.toFloat(),  "DYNAMICS  &  OUTPUT", rampLo, rampHi);

    bgScHint = scButton.getToggleState() && ! proc.scConnected.load();
    if (bgScHint)
    {
        g.setColour (juce::Colour (0xffffa63d).withAlpha (0.85f));
        g.setFont (uiFont (9.0f, true));
        g.drawText ("NO SIDECHAIN ROUTED", scHintRect, juce::Justification::centredRight, false);
    }

    // group headers above the faders: a label with a thin rule either side, like the section titles
    auto group = [&] (int a, int b, const juce::String& text, juce::Colour c)
    {
        const float x0 = (float) faders[(size_t) a]->getX() + 6.0f;
        const float x1 = (float) faders[(size_t) b]->getRight() - 6.0f;
        const float cy = (float) dynPanel.getY() + 40.0f;
        const auto f = uiFont (9.5f, true).withExtraKerningFactor (0.15f);
        const float tw = juce::GlyphArrangement::getStringWidth (f, text) + 14.0f;
        const float mid = 0.5f * (x0 + x1), l = mid - tw * 0.5f, rt = mid + tw * 0.5f;
        if (l - x0 > 4.0f)
        {
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.0f), x0, cy, c.withAlpha (0.5f), l, cy, false));
            g.fillRect (x0, cy, l - x0, 1.0f);
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.5f), rt, cy, c.withAlpha (0.0f), x1, cy, false));
            g.fillRect (rt, cy, x1 - rt, 1.0f);
        }
        g.setFont (f);
        glowText (g, text, juce::Rectangle<float> (l + 4.0f, cy - 7.0f, tw - 8.0f, 14.0f), juce::Justification::centred, c, 0.22f);
    };
    group (0, 5, "GLUE COMPRESSOR", kFaderColours[0]);
    group (6, 8, "LIMITER", kFaderColours[6]);
    group (9, 9, "OUT", kFaderColours[9]);

    // dividing lines between the groups (compressor | limiter | out): a lit rule fading out at both ends, with an LED on each end
    auto divider = [&] (int lastOfLeftGroup)
    {
        const float x = std::round ((float) faders[(size_t) lastOfLeftGroup]->getRight());
        const float y0 = (float) dynPanel.getY() + 34.0f, y1 = (float) dynPanel.getBottom() - 14.0f;
        auto line = juce::ColourGradient (rampLo.withAlpha (0.0f), x, y0, rampHi.withAlpha (0.0f), x, y1, false);
        line.addColour (0.12, rampLo.withAlpha (0.6f));
        line.addColour (0.88, rampHi.withAlpha (0.6f));
        auto bloom = line;
        bloom.multiplyOpacity (0.22f);
        g.setGradientFill (bloom);
        g.fillRoundedRectangle (x - 2.5f, y0, 6.0f, y1 - y0, 3.0f);
        g.setGradientFill (line);
        g.fillRect (x, y0, 1.0f, y1 - y0);
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ x + 0.5f, y0 }), true, rampLo);
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ x + 0.5f, y1 }), true, rampHi);
    };
    divider (5);
    divider (8);

    bgPreset = proc.getCurrentPreset();
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

        // potato badge: the pupils glow with the output level (the rest is in the cached background)
        drawPotatoBadge (g, badgeRect.toFloat().getCentre() + juce::Point<float> (0.0f, -1.0f), 15.0f, level, true);

        // output volume value (the knob itself is a child component)
        g.setColour (kCream);
        g.setFont (monoFont (15.0f));
        g.drawText (outKnob.getTextFromValue (outKnob.getValue()), outValueRect, juce::Justification::centredRight, false);
        return;
    }

    g.setFont (uiFont (22.0f, true).withExtraKerningFactor (0.3f));
    glowTextRamp (g, "POTATO MAGIC", juce::Rectangle<float> (59.0f, 11.0f, 300.0f, 32.0f), juce::Justification::left, rampLo, rampHi, 0.35f);

    g.setColour (juce::Colour (0xffb98c62));
    g.setFont (monoFont (10.5f));
    g.drawText ("MIX & MASTER BUS PROCESSOR", 60, 46, 300, 14, juce::Justification::left, false);

    drawPotatoBadge (g, badgeRect.toFloat().getCentre() + juce::Point<float> (0.0f, -1.0f), 15.0f, 0.0f, false);

    // preset selector
    insetWindow (g, presetRect.toFloat().reduced (1.0f));

    auto arrow = [&] (juce::Rectangle<int> r, int dir)
    {
        const auto c = r.toFloat().getCentre();
        juce::Path tri;
        if (dir < 0) tri.addTriangle (c.x - 5.0f, c.y, c.x + 4.0f, c.y - 5.5f, c.x + 4.0f, c.y + 5.5f);
        else         tri.addTriangle (c.x + 5.0f, c.y, c.x - 4.0f, c.y - 5.5f, c.x - 4.0f, c.y + 5.5f);
        g.setColour (col::ink);
        g.fillPath (tri);
    };
    arrow (prevRect, -1);
    arrow (nextRect, 1);

    const int cur = proc.getCurrentPreset();
    const juce::String idx = cur >= 0 ? juce::String (cur + 1).paddedLeft ('0', 2) : juce::String ("--");
    const juce::String nm = cur >= 0 ? juce::String (kPresets[(size_t) cur].name) : juce::String ("Init / Custom");

    g.setFont (monoFont (14.0f));
    glowText (g, idx, nameRect.withWidth (36).translated (8, 0).toFloat(), juce::Justification::centredLeft, col::amber, 0.25f);
    g.setFont (monoFont (15.0f));
    glowText (g, nm, nameRect.withTrimmedLeft (46).toFloat(), juce::Justification::centredLeft, rampHi, 0.25f);

    // output volume label
    g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.2f));
    glowText (g, "OUTPUT", outTextRect.withHeight (18).translated (0, 3).toFloat(), juce::Justification::centredRight, col::ink, 0.22f);
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
juce::String fmtHz (float f)  { return f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }
juce::String fmtDb (float db) { return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB"; }

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
    eqCs = cs;
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
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff100d0a), 0.0f, r.getY(), juce::Colour (0xff060504), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::line.withAlpha (0.30f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (rampLo.withAlpha (0.85f), r.getX(), 0.0f, rampHi.withAlpha (0.85f), r.getRight(), 0.0f, false));
        for (auto [bx, by, dx, dy] : { std::tuple<float, float, float, float> { r.getX() + 4.0f, r.getY() + 4.0f, 1.0f, 1.0f },
                                       { r.getRight() - 4.0f, r.getY() + 4.0f, -1.0f, 1.0f },
                                       { r.getX() + 4.0f, r.getBottom() - 4.0f, 1.0f, -1.0f },
                                       { r.getRight() - 4.0f, r.getBottom() - 4.0f, -1.0f, -1.0f } })
        {
            g.fillRect (dx > 0 ? bx : bx - 14.0f, dy > 0 ? by : by - 2.0f, 14.0f, 2.0f);
            g.fillRect (dx > 0 ? bx : bx - 2.0f, dy > 0 ? by : by - 14.0f, 2.0f, 14.0f);
        }

        g.setFont (monoFont (9.5f));
        const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        const char* fl[]    = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
        for (int i = 0; i < 8; ++i)
        {
            const float x = std::round (d.fx (freqs[i]));
            g.setColour (col::tick.withAlpha (0.045f));
            g.fillRect (x, d.top, 1.0f, d.bottom - d.top);
            g.setColour (col::tick.withAlpha (0.30f));
            g.drawText (fl[i], (int) x - 20, (int) d.bottom + 4, 40, 14, juce::Justification::centred, false);
        }
        for (int db = -20; db >= -80; db -= 20)
        {
            const float y = std::round (d.sy ((float) db));
            g.setColour (col::tick.withAlpha (0.045f));
            g.fillRect (d.x0, y, d.x1 - d.x0, 1.0f);
            g.setColour (col::tick.withAlpha (0.28f));
            g.drawText (juce::String (db), (int) r.getX() + 4, (int) y - 7, 30, 14, juce::Justification::centredRight, false);
        }

        g.setColour (col::tick.withAlpha (0.10f));
        g.fillRect (d.x0, std::round (d.cy), d.x1 - d.x0, 1.0f);
        g.setFont (monoFont (9.5f));
        g.setColour (col::tick.withAlpha (0.30f));
        const int marks[] = { 12, 6, 0, -6, -12 };
        for (int m : marks)
            g.drawText (m > 0 ? "+" + juce::String (m) : juce::String (m), (int) d.x1 + 4, (int) d.ey ((float) m) - 7,
                        30, 14, juce::Justification::centredLeft, false);

        g.setColour (col::tick.withAlpha (0.35f));
        g.setFont (uiFont (9.5f, true).withExtraKerningFactor (0.15f));
        g.drawText ("TONE EQ  +  OUTPUT SPECTRUM", (int) r.getX() + 42, (int) r.getY() + 4, 260, 12,
                    juce::Justification::left, false);
        g.setColour (col::inkDim);
        g.setFont (monoFont (9.0f));
        g.drawText ("DRAG A BAND  /  DOUBLE-CLICK RESETS  /  WHEEL FINE-TUNES", (int) r.getRight() - 42 - 290, (int) r.getY() + 4, 290, 12,
                    juce::Justification::right, false);
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
        g.setColour (kCream.withAlpha (0.07f));
        g.fillPath (eqArea);
        g.setColour (kCream.withAlpha (0.14f));
        g.strokePath (eqCurve, juce::PathStrokeType (4.0f));
        g.setColour (kCream.withAlpha (0.92f));
        g.strokePath (eqCurve, juce::PathStrokeType (1.6f));
    }

    for (size_t k = 0; k < 4; ++k)
    {
        const auto n = eqNodes[k];
        const auto nodeCol = ramp (rampLo, rampHi, (float) k / 3.0f);
        if ((int) k == (eqDrag >= 0 ? eqDrag : eqHover))   // band under the mouse / being dragged
        {
            g.setColour (nodeCol.withAlpha (0.25f));
            g.fillEllipse (n.x - 13.0f, n.y - 13.0f, 26.0f, 26.0f);
            g.setColour (kCream.withAlpha (0.8f));
            g.drawEllipse (n.x - 11.0f, n.y - 11.0f, 22.0f, 22.0f, 1.2f);
        }
        g.setColour (nodeCol.withAlpha (0.35f));
        g.fillEllipse (n.x - 8.0f, n.y - 8.0f, 16.0f, 16.0f);
        g.setColour (nodeCol);
        g.fillEllipse (n.x - 4.5f, n.y - 4.5f, 9.0f, 9.0f);
        g.setColour (kCream);
        g.drawEllipse (n.x - 4.5f, n.y - 4.5f, 9.0f, 9.0f, 1.0f);
    }

    // mouse feedback: crosshair with a frequency / EQ-level readout, and a label on the band you're over or dragging
    if (eqMouseIn || eqDrag >= 0)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());

        const float mx = juce::jlimit (d.x0, d.x1, eqMouse.x);
        const float f = 20.0f * std::pow (1000.0f, (mx - d.x0) / (d.x1 - d.x0));
        g.setColour (kCream.withAlpha (0.16f));
        g.fillRect (std::round (mx), d.top, 1.0f, d.bottom - d.top);

        auto tag = [&] (const juce::String& text, juce::Point<float> anchor, juce::Colour edge)
        {
            g.setFont (monoFont (11.0f));
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
            auto box = juce::Rectangle<float> (tw, 17.0f).withCentre (anchor);
            box = box.withX (juce::jlimit (r.getX() + 4.0f, r.getRight() - tw - 4.0f, box.getX()))
                     .withY (juce::jlimit (d.top, d.bottom - 17.0f, box.getY()));
            g.setColour (juce::Colour (0xe6080504));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (edge.withAlpha (0.55f));
            g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (kCream);
            g.drawText (text, box, juce::Justification::centred, false);
        };

        const int active = eqDrag >= 0 ? eqDrag : eqHover;
        if (active < 0)
            tag (fmtHz (f) + "   " + fmtDb (eqResponseDb (f)), { mx + 58.0f, d.top + 12.0f }, col::line);
        else
        {
            const float nodeF[4] = { kLowF, kBodyF, kPresF, kAirF };
            tag (juce::String (kParams[(size_t) active].name).toUpperCase() + "  " + fmtHz (nodeF[active]) + "  " + fmtDb (eqBandDb (active)),
                 { eqNodes[(size_t) active].x, eqNodes[(size_t) active].y - 24.0f }, ramp (rampLo, rampHi, (float) active / 3.0f));
        }
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
        drawPanel (g, r, "LEVELS", rampLo, rampHi);

    // ---- numeric readouts: output peak (max hold), momentary + short-term loudness
    {
        const float ry = r.getY() + 28.0f, rh = 30.0f, gap = 4.0f;
        const float rw = (r.getWidth() - 24.0f - 2.0f * gap) / 3.0f;
        auto cell = [&] (int k, const char* label, float value, float minShown, juce::Colour vc)
        {
            const juce::Rectangle<float> c (r.getX() + 12.0f + (float) k * (rw + gap), ry, rw, rh);
            if (staticPart)
            {
                g.setColour (juce::Colour (0xff0a0705));
                g.fillRoundedRectangle (c, 4.0f);
                g.setColour (col::line.withAlpha (0.25f));
                g.drawRoundedRectangle (c.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (col::tick.withAlpha (0.42f));
                g.setFont (uiFont (7.5f, true).withExtraKerningFactor (0.12f));
                g.drawText (label, c.withHeight (12.0f).translated (0.0f, 2.0f).toNearestInt(), juce::Justification::centred, false);
                return;
            }
            g.setColour (vc);
            g.setFont (monoFont (12.5f));
            g.drawText (value <= minShown ? juce::String ("-inf") : juce::String (value, 1),
                        c.withTrimmedTop (12.0f).toNearestInt(), juce::Justification::centred, false);
        };
        cell (0, "PEAK",   outMax,    -89.0f, outMax > -0.05f ? col::faceRed : col::tick.withAlpha (0.92f));
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
            g.setColour (juce::Colour (0xff050403));
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
    auto zoneAt = [&] (float y) { return meterFade (dbOf (y + 1.0f)); };

    auto levelBar = [&] (float x, const MeterCh& m)
    {
        bar (x, bw, yOf (m.rms), yOf (m.peak), zoneAt);
        if (! staticPart && m.hold > -59.0f)
        {
            g.setColour (meterFade (m.hold).brighter (0.4f));
            g.fillRect (x - 1.0f, std::round (yOf (m.hold)) - 1.0f, bw + 2.0f, 2.0f);
        }
    };

    auto grBar = [&] (float x, float db, juce::Colour c)
    {
        if (staticPart)
        {
            g.setColour (juce::Colour (0xff050403));
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
            g.setColour (juce::Colour (0xff2c1406));
            g.fillRect (c);
        }
        else if (on)
        {
            g.setColour (col::faceRed.withAlpha (0.35f));
            g.fillRect (c.expanded (1.5f));
            g.setColour (col::faceRed);
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
        g.setFont (monoFont (9.0f));
        for (int db = 0; db >= -12; --db)   // 1 dB minor ticks where it matters
        {
            const float y = std::round (yOf ((float) db));
            g.setColour (col::tick.withAlpha (0.14f));
            g.fillRect (b0 + 19.0f, y, 2.0f, 1.0f);
            g.fillRect (b0 + 41.0f, y, 2.0f, 1.0f);
        }
        const int marks[] = { 0, -3, -6, -9, -12, -18, -24, -36, -48 };
        for (int db : marks)
        {
            const float y = std::round (yOf ((float) db));
            g.setColour (col::tick.withAlpha (0.38f));
            g.fillRect (b0 + 19.0f, y, 4.0f, 1.0f);
            g.fillRect (b0 + 39.0f, y, 4.0f, 1.0f);
            g.setColour (col::tick.withAlpha (0.55f));
            g.drawText (juce::String (db), (int) b0 + 20, (int) y - 5, 22, 10, juce::Justification::centred, false);
        }

        // GR scale
        for (int db = 0; db <= 18; db += 3)
        {
            const float y = std::round (yGr ((float) db));
            g.setColour (col::tick.withAlpha (0.38f));
            g.fillRect (b0 + 100.0f, y, 3.0f, 1.0f);
            g.setColour (col::tick.withAlpha (0.55f));
            g.drawText (juce::String (db), (int) b0 + 104, (int) y - 5, 16, 10, juce::Justification::centredLeft, false);
        }

        // labels
        g.setFont (uiFont (8.5f, true).withExtraKerningFactor (0.1f));
        g.setColour (col::tick.withAlpha (0.55f));
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
    g.setColour (col::tick.withAlpha (0.85f));
    g.drawText (rmsOut <= -69.0f ? juce::String ("-inf") : juce::String (rmsOut, 1),
                (int) (xRms + wide * 0.5f) - 14, (int) top - 12, 28, 10, juce::Justification::centred, false);
}

//==============================================================================
void MagicTouchEditor::mouseDown (const juce::MouseEvent& e)
{
    // Tone EQ display: grab the band under the mouse, or jump the nearest band to the clicked level
    if (eqPlotRect().contains (e.position))
    {
        const auto d = dispGeom (displayRect);
        int k = eqBandAt (e.position, true);
        const bool onNode = k >= 0;
        if (! onNode)
            k = eqBandAt (e.position, false);

        eqDrag = k;
        eqLastY = e.position.y;
        eqMouse = e.position;
        eqMouseIn = true;
        proc.apvts.getParameter (kParams[(size_t) k].id)->beginChangeGesture();
        if (! onNode)
            setEqBandDb (k, (d.cy - e.position.y) / ((d.bottom - d.top) * 0.45f / 12.0f));
        return;
    }

    const auto pt = e.getPosition();
    if (prevRect.contains (pt))       stepPreset (-1);
    else if (nextRect.contains (pt))  stepPreset (1);
    else if (nameRect.contains (pt))  showPresetMenu();
    else if (meterResetRect.contains (e.position) || meterRect.withTop (meterRect.getY() + 60.0f).withHeight (20.0f).contains (e.position))
    {
        // click the readouts or clip lights to reset peak holds / clip indicators
        outMax = -90.0f;
        for (auto& m : mtr) { m.clip = false; m.hold = m.peak; m.holdT = 0.0f; }
    }
}

//==============================================================================
// Tone EQ display interaction. The four bands sit at fixed frequencies (Bass 90 Hz, Body 320 Hz,
// Presence 3.2 kHz, Air 11 kHz), so the vertical position is the control: drag a band's dot (Shift = fine),
// click anywhere to jump the nearest band to that level, double-click to reset it, wheel to fine-tune.
juce::Rectangle<float> MagicTouchEditor::eqPlotRect() const
{
    const auto d = dispGeom (displayRect);
    return { d.x0, d.top, d.x1 - d.x0, d.bottom - d.top };
}

int MagicTouchEditor::eqBandAt (juce::Point<float> p, bool nodesOnly) const
{
    const auto d = dispGeom (displayRect);
    const float nodeF[4] = { kLowF, kBodyF, kPresF, kAirF };
    int best = -1;
    float bestDist = nodesOnly ? 16.0f : 1.0e9f;
    for (int k = 0; k < 4; ++k)
    {
        const float dist = nodesOnly ? p.getDistanceFrom (eqNodes[(size_t) k]) : std::abs (p.x - d.fx (nodeF[k]));
        if (dist < bestDist)
        {
            bestDist = dist;
            best = k;
        }
    }
    return best;
}

float MagicTouchEditor::eqBandDb (int band) const
{
    auto* p = proc.apvts.getParameter (kParams[(size_t) band].id);
    return p->convertFrom0to1 (p->getValue());
}

void MagicTouchEditor::setEqBandDb (int band, float db)
{
    auto* p = proc.apvts.getParameter (kParams[(size_t) band].id);
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (kParams[(size_t) band].min, kParams[(size_t) band].max, db)));
}

float MagicTouchEditor::eqResponseDb (float freq)
{
    const double prate = proc.getProcessingRate();
    double m = 1.0;
    for (const auto& c : eqCs)
        if (c != nullptr)
            m *= c->getMagnitudeForFrequency ((double) freq, prate);
    return (float) juce::Decibels::gainToDecibels (m);
}

void MagicTouchEditor::mouseMove (const juce::MouseEvent& e)
{
    eqMouse = e.position;
    eqMouseIn = eqPlotRect().contains (e.position);
    eqHover = eqMouseIn ? eqBandAt (e.position, true) : -1;
    setMouseCursor (eqHover >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                 : eqMouseIn ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
}

void MagicTouchEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (eqDrag < 0)
        return;
    const auto d = dispGeom (displayRect);
    const float pxPerDb = (d.bottom - d.top) * 0.45f / 12.0f;
    const float dy = e.position.y - eqLastY;
    eqLastY = e.position.y;
    eqMouse = e.position;
    setEqBandDb (eqDrag, eqBandDb (eqDrag) - dy / pxPerDb * (e.mods.isShiftDown() ? 0.25f : 1.0f));
}

void MagicTouchEditor::mouseUp (const juce::MouseEvent&)
{
    if (eqDrag < 0)
        return;
    proc.apvts.getParameter (kParams[(size_t) eqDrag].id)->endChangeGesture();
    eqDrag = -1;
}

void MagicTouchEditor::mouseExit (const juce::MouseEvent&)
{
    if (eqDrag >= 0)
        return;
    eqMouseIn = false;
    eqHover = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void MagicTouchEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! eqPlotRect().contains (e.position))
        return;
    int k = eqBandAt (e.position, true);
    if (k < 0)
        k = eqBandAt (e.position, false);
    auto* p = proc.apvts.getParameter (kParams[(size_t) k].id);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
}

void MagicTouchEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! eqPlotRect().contains (e.position) || w.deltaY == 0.0f)
        return;
    int k = eqBandAt (e.position, true);
    if (k < 0)
        k = eqBandAt (e.position, false);
    auto* p = proc.apvts.getParameter (kParams[(size_t) k].id);
    p->beginChangeGesture();
    setEqBandDb (k, eqBandDb (k) + (w.deltaY > 0.0f ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.1f : 0.25f));
    p->endChangeGesture();
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
