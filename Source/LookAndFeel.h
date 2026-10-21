#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>

namespace MT
{
//==============================================================================
// "Light-up console" palette, shared with Potato-Kicker: near-black panels, orange -> yellow light ramp
namespace col
{
    inline const juce::Colour page    { 0xff070707 };
    inline const juce::Colour ink     { 0xffffb35e };   // labels
    inline const juce::Colour inkDim  { 0xff9a7658 };   // values and secondary text
    inline const juce::Colour amber   { 0xffff8a1e };   // primary accent
    inline const juce::Colour amberHi { 0xffffc27a };   // hot highlight
    inline const juce::Colour led     { 0xffff7a12 };
    inline const juce::Colour ledOff  { 0xff2c1406 };
    inline const juce::Colour tick    { 0xffb3a493 };
    inline const juce::Colour line    { 0xffff8a1e };   // tile borders and brackets (used with low alpha)
    inline const juce::Colour orange  { 0xffff7212 };   // start of the light ramp
    inline const juce::Colour yellow  { 0xffffd23f };   // end of the light ramp
    inline const juce::Colour faceRed { 0xffff3d1a };
}

struct Typefaces
{
    juce::Typeface::Ptr semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
};
inline const Typefaces& typefaces() { static Typefaces t; return t; }

// Barlow Condensed reads smaller than the default sans at the same height, so sizes are scaled to keep the layout
inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (bold ? typefaces().bold : typefaces().semi).withHeight (height * 1.2f));
}
inline juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (typefaces().mono).withHeight (height));
}

inline juce::Colour ramp (juce::Colour lo, juce::Colour hi, float t)
{
    return lo.interpolatedWith (hi, juce::jlimit (0.0f, 1.0f, t));
}

// Text with a soft glow, like the lit labels on the console
inline void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
                      juce::Justification j, juce::Colour c, float glow)
{
    g.setColour (c.withAlpha (glow));
    for (auto d : { juce::Point<float> (-1, 0), juce::Point<float> (1, 0), juce::Point<float> (0, -1), juce::Point<float> (0, 1) })
        g.drawText (s, r.translated (d.x, d.y), j, false);
    g.setColour (c);
    g.drawText (s, r, j, false);
}

// Glowing text filled with the ramp from left to right
inline void glowTextRamp (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j,
                          juce::Colour lo, juce::Colour hi, float glow)
{
    g.setColour (lo.withAlpha (glow));
    for (auto d : { juce::Point<float> (-1, 0), juce::Point<float> (1, 0), juce::Point<float> (0, -1), juce::Point<float> (0, 1) })
        g.drawText (s, r.translated (d.x, d.y), j, false);
    g.setGradientFill (juce::ColourGradient (lo, r.getX(), 0.0f, hi, r.getRight(), 0.0f, false));
    g.drawText (s, r, j, false);
}

// Small console LED
inline void drawLed (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour c = col::led)
{
    const auto ctr = r.getCentre();
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.45f), ctr.x, ctr.y, c.withAlpha (0.0f), ctr.x + r.getWidth() * 1.4f, ctr.y, true));
        g.fillEllipse (r.expanded (r.getWidth() * 0.9f));
        juce::ColourGradient gr (juce::Colour (0xfffff3c8), ctr.x - r.getWidth() * 0.12f, ctr.y - r.getHeight() * 0.15f,
                                 c.darker (0.7f), ctr.x + r.getWidth() * 0.5f, ctr.y, true);
        gr.addColour (0.45, c);
        g.setGradientFill (gr);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a2208), ctr.x - r.getWidth() * 0.1f, ctr.y - r.getHeight() * 0.15f,
                                                 col::ledOff, ctr.x + r.getWidth() * 0.5f, ctr.y, true));
    }
    g.fillEllipse (r);
    g.setColour (juce::Colours::black);
    g.drawEllipse (r, 0.8f);
}

// glass screen finish: inner shadow round the edge and a soft diagonal glare
inline void drawGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);

    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, r.getY(),
                                             juce::Colours::transparentBlack, 0.0f, r.getY() + 12.0f, false));
    g.fillRect (r.withHeight (12.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), r.getX(), 0.0f,
                                             juce::Colours::transparentBlack, r.getX() + 10.0f, 0.0f, false));
    g.fillRect (r.withWidth (10.0f));

    juce::Path glare;
    glare.startNewSubPath (r.getX(), r.getY());
    glare.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
    glare.lineTo (r.getX() + r.getWidth() * 0.30f, r.getBottom());
    glare.lineTo (r.getX(), r.getBottom());
    glare.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.045f), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.45f, r.getBottom(), false));
    g.fillPath (glare);

    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (col::line.withAlpha (0.18f));
    g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 1.0f);
}

class MagicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MagicLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff0b0908));
        setColour (juce::PopupMenu::textColourId,                  col::ink);
        setColour (juce::PopupMenu::headerTextColourId,            col::inkDim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a160a));
        setColour (juce::PopupMenu::highlightedTextColourId,       col::amberHi);
    }

    juce::Font getPopupMenuFont() override { return uiFont (13.5f); }

    // Black glossy knob with a backlit light-pipe ring (Potato-Kicker style). The lit part of the ring and the
    // scale go from the ramp's start colour at the start of travel to its end colour at the end, and the whole
    // light gets brighter as the value rises. Bipolar-aware: the lit arc runs from zero to the value.
    // The ramp comes from the slider's rotarySliderFillColourId (start) and rotarySliderOutlineColourId (end).
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        using juce::Point;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 2.0f;
        const auto c = bounds.getCentre();
        const auto cLo = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto cHi = s.findColour (juce::Slider::rotarySliderOutlineColourId);
        const bool hot = s.isMouseOverOrDragging();

        const float n = juce::jlimit (0.0f, 1.0f, pos);
        float tFrom = 0.0f;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            tFrom = (float) s.valueToProportionOfLength (0.0);
        const float tLo = juce::jmin (tFrom, n), tHi = juce::jmax (tFrom, n);
        const bool lit = tHi - tLo > 0.002f;

        const float k = juce::jmin (1.0f, 0.3f + 0.7f * n + (hot ? 0.1f : 0.0f));   // light intensity
        const float rO = size * 0.5f - 1.5f, rB = size * 0.25f;
        const float rw = size * 0.036f;
        const float rR = rB + rw * 0.5f + size * 0.04f;

        auto at   = [&] (float t) { return ramp (cLo, cHi, t); };
        auto ang  = [&] (float t) { return a0 + t * (a1 - a0); };
        auto pol  = [&] (float r, float t) { const float a = ang (t); return Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };
        auto box  = [&] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

        // light spill on the panel behind the knob
        if (lit)
        {
            g.setGradientFill (juce::ColourGradient (at (n).withAlpha (0.10f * k), c.x, c.y, at (n).withAlpha (0.0f), c.x + size * 0.62f, c.y, true));
            g.fillEllipse (box (size * 0.62f));
        }

        // outer scale: ticks light up between zero (or the start) and the value
        const bool scale = size >= 60.0f;
        for (int i = 0; scale && i <= 40; ++i)
        {
            const bool major = i % 4 == 0;
            const float t = (float) i / 40.0f;
            const bool on = lit && t >= tLo - 0.0001f && t <= tHi + 0.0001f;
            const auto p1 = pol (major ? rO - 5.5f : rO - 3.0f, t), p2 = pol (rO, t);
            if (on)
            {
                g.setColour (at (t).withAlpha (0.35f * k));  g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 4.0f : 3.0f);
                g.setColour (at (t).brighter (0.35f));       g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.5f : 1.0f);
            }
            else
            {
                g.setColour (col::tick.withAlpha (major ? 0.75f : 0.4f)); g.drawLine (p1.x, p1.y, p2.x, p2.y, major ? 1.3f : 0.8f);
            }
        }

        // 0..10 scale numbers inside the ticks on the larger knobs
        if (size >= 84.0f)
        {
            const float rn = 0.5f * ((rR + rw * 0.5f + 1.2f) + (rO - 5.5f));
            g.setFont (uiFont (size * 0.085f));
            for (int j = 0; j <= 10; ++j)
            {
                const float t = (float) j / 10.0f;
                const bool on = lit && t >= tLo - 0.0001f && t <= tHi + 0.0001f;
                g.setColour (on ? at (t).brighter (0.55f) : juce::Colour (0xff8f8174));
                g.drawText (juce::String (j), juce::Rectangle<float> (14.0f, 11.0f).withCentre (pol (rn, t)),
                            juce::Justification::centred, false);
            }
        }

        // groove, dim backlit ring, then the lit value arc with bloom (segmented so it fades along its length)
        auto strokeArc = [&] (float t0, float t1, float width, float alpha, float bright)
        {
            const int segs = juce::jmax (2, (int) ((t1 - t0) * 24.0f));
            for (int i = 0; i < segs; ++i)
            {
                const float u0 = t0 + (t1 - t0) * (float) i / (float) segs, u1 = t0 + (t1 - t0) * (float) (i + 1) / (float) segs;
                juce::Path seg;
                seg.addCentredArc (c.x, c.y, rR, rR, 0.0f, ang (u0), ang (u1), true);
                g.setColour (at (0.5f * (u0 + u1)).brighter (bright).withAlpha (juce::jlimit (0.0f, 1.0f, alpha)));
                g.strokePath (seg, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            }
        };
        g.setColour (juce::Colour (0xff040303));
        g.drawEllipse (box (rR), rw + 3.0f);
        strokeArc (0.0f, 1.0f, rw * 0.8f, 0.10f + 0.12f * k, 0.0f);
        if (lit)
        {
            strokeArc (tLo, tHi, rw * 4.0f, 0.12f * k, 0.0f);
            strokeArc (tLo, tHi, rw * 2.3f, 0.28f * k, 0.0f);
            strokeArc (tLo, tHi, rw * 1.1f, 0.65f + 0.35f * k, 0.0f);
            strokeArc (tLo, tHi, rw * 0.6f, 1.0f, 0.45f);
            const auto tip = pol (rR, n);
            g.setColour (at (n).brighter (0.6f).withAlpha (0.35f)); g.fillEllipse (juce::Rectangle<float> (rw * 3.2f, rw * 3.2f).withCentre (tip));
            g.setColour (juce::Colour (0xfffff4d6));               g.fillEllipse (juce::Rectangle<float> (rw * 1.3f, rw * 1.3f).withCentre (tip));
        }

        // light spilling onto the skirt
        g.setColour (at (n).withAlpha (0.12f + 0.3f * k));
        g.drawEllipse (box (rB + 2.2f), 1.6f);

        // skirt, glossy body, shine
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3632), c.x, c.y - rB - 3.0f, juce::Colours::black, c.x, c.y + rB + 3.0f, false));
        g.fillEllipse (box (rB + 1.6f));
        juce::ColourGradient body (juce::Colour (0xff4e4a46), c.x - rB * 0.2f, c.y - rB * 0.4f,
                                   juce::Colour (0xff020202), c.x - rB * 0.2f + rB * 1.56f, c.y - rB * 0.4f, true);
        body.addColour (0.32, juce::Colour (0xff1d1b1a));
        g.setGradientFill (body);
        g.fillEllipse (box (rB - 1.0f));
        g.setColour (juce::Colours::black);
        g.drawEllipse (box (rB - 1.0f), 0.8f);
        const Point<float> sc (c.x - rB * 0.22f, c.y - rB * 0.4f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.24f), sc.x, sc.y,
                                                 juce::Colours::white.withAlpha (0.0f), sc.x + rB * 0.55f, sc.y, true));
        g.fillEllipse (juce::Rectangle<float> (rB * 1.1f, rB * 0.6f).withCentre (sc));

        // pointer: lit line in the ramp colour for the current position
        const auto p1 = pol (rB * 0.15f, n), p2 = pol (rB - 3.0f, n);
        g.setColour (at (n).withAlpha (0.25f + 0.4f * k));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, 5.0f);
        g.setColour (juce::Colour (0xfffff1d8));
        g.drawLine (p1.x, p1.y, p2.x, p2.y, juce::jmax (2.0f, size * 0.026f));
    }

    // Slim glowing vertical fader with a glassy thumb
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, const juce::Slider::SliderStyle style,
                           juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }

        const float cx = (float) x + (float) w * 0.5f;
        // minPos/maxPos are only meaningful for two-value sliders, so take the track ends from the range itself
        juce::ignoreUnused (minPos, maxPos);
        const float pLo = (float) s.getPositionOfValue (s.getMinimum());
        const float pHi = (float) s.getPositionOfValue (s.getMaximum());
        const float top = juce::jmin (pLo, pHi) - 6.0f;
        const float bottom = juce::jmax (pLo, pHi) + 6.0f;
        const auto col = s.findColour (juce::Slider::thumbColourId);
        const bool hot = s.isMouseOverOrDragging();

        // scale ticks (major every 5)
        for (int i = 0; i <= 20; ++i)
        {
            const float ty = std::round (juce::jmap ((float) i, 0.0f, 20.0f, top + 6.0f, bottom - 6.0f));
            const bool major = i % 5 == 0;
            const float len = major ? 6.0f : 3.0f;
            g.setColour (MT::col::tick.withAlpha (major ? 0.55f : 0.28f));
            g.fillRect (cx - 11.0f - len, ty, len, 1.0f);
            g.fillRect (cx + 11.0f, ty, len, 1.0f);
        }

        // recessed slot with an inner shadow and a lit lip
        const juce::Rectangle<float> slot (cx - 3.5f, top, 7.0f, bottom - top);
        g.setColour (juce::Colour (0xff040302));
        g.fillRoundedRectangle (slot, 3.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), cx - 3.5f, 0.0f,
                                                 juce::Colours::transparentBlack, cx + 1.5f, 0.0f, false));
        g.fillRoundedRectangle (slot, 3.5f);
        g.setColour (MT::col::line.withAlpha (0.30f));
        g.drawRoundedRectangle (slot.expanded (0.5f), 4.0f, 1.0f);

        // backlit fill: fades up from a dim ember at the bottom to full colour at the cap,
        // and the whole fill burns hotter as the value rises
        const float prop = (float) s.valueToProportionOfLength (s.getValue());
        const float fillH = bottom - 3.0f - sliderPos;
        if (fillH > 0.5f)
        {
            const auto fill = juce::Rectangle<float> (cx - 1.75f, sliderPos, 3.5f, fillH);
            const auto hotCol  = col.withMultipliedSaturation (0.85f + 0.3f * prop).brighter (0.1f + 0.55f * prop);
            const auto coldCol = col.darker (0.85f).withAlpha (0.30f);

            juce::ColourGradient glow (hotCol.withAlpha ((hot ? 0.10f : 0.05f) + 0.22f * prop), 0.0f, sliderPos,
                                       col.withAlpha (0.0f), 0.0f, bottom, false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (fill.expanded (5.0f, 2.0f), 5.0f);

            juce::ColourGradient body (hotCol, 0.0f, sliderPos, coldCol, 0.0f, bottom, false);
            body.addColour (0.35, col.withAlpha (0.55f + 0.35f * prop));
            g.setGradientFill (body);
            g.fillRoundedRectangle (fill.expanded (1.0f, 0.0f), 2.25f);

            juce::ColourGradient core (juce::Colours::white.withAlpha (0.35f + 0.45f * prop), 0.0f, sliderPos,
                                       juce::Colours::white.withAlpha (0.0f), 0.0f, sliderPos + fillH * 0.85f, false);
            g.setGradientFill (core);
            g.fillRect (cx - 0.5f, sliderPos, 1.0f, fillH);
        }

        // console cap: shadow, glossy warm-black body, grip grooves, lit index line
        const juce::Rectangle<float> thumb (cx - 14.0f, sliderPos - 8.0f, 28.0f, 16.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f).expanded (1.0f), 4.0f);
        if (hot)
        {
            g.setColour (col.withAlpha (0.22f));
            g.fillRoundedRectangle (thumb.expanded (4.0f), 7.0f);
        }
        juce::ColourGradient body (juce::Colour (0xff57514b), 0.0f, thumb.getY(),
                                   juce::Colour (0xff141211), 0.0f, thumb.getBottom(), false);
        body.addColour (0.48, juce::Colour (0xff34302c));
        body.addColour (0.52, juce::Colour (0xff25221f));
        g.setGradientFill (body);
        g.fillRoundedRectangle (thumb, 3.5f);
        for (int k = -1; k <= 1; k += 2)
        {
            const float gy = thumb.getCentreY() + (float) k * 4.0f;
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (thumb.getX() + 4.0f, gy, thumb.getWidth() - 8.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (thumb.getX() + 4.0f, gy + 1.0f, thumb.getWidth() - 8.0f, 1.0f);
        }
        g.setColour (MT::col::tick.withAlpha (hot ? 0.6f : 0.32f));
        g.drawRoundedRectangle (thumb.reduced (0.5f), 3.5f, 1.0f);
        g.setColour (col.withAlpha (0.45f));
        g.fillRect (thumb.getX() + 3.0f, thumb.getCentreY() - 1.5f, thumb.getWidth() - 6.0f, 3.0f);
        g.setColour (col.brighter (0.6f));
        g.fillRect (thumb.getX() + 4.0f, thumb.getCentreY() - 0.5f, thumb.getWidth() - 8.0f, 1.0f);
    }
};
} // namespace MT
