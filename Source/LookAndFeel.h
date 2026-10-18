#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace MT
{
inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
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
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 1.0f);
}

// Colour themes: one per knob colour, plus the original level-reactive hue shift
struct Theme { const char* name; juce::uint32 argb; };   // argb == 0 -> reactive
inline constexpr Theme kThemes[] =
{
    { "Reactive", 0 },
    { "Azure",   0xff3ba7ff }, { "Lagoon",  0xff35d0c4 }, { "Lime",    0xffa8e05f }, { "Ice",     0xffa0e7ff },
    { "Amber",   0xffffa63d }, { "Ember",   0xffff7a45 }, { "Magenta", 0xffff5cb8 }, { "Violet",  0xffb583ff },
    { "Indigo",  0xff6c8cff }, { "Crimson", 0xffff5252 }
};
inline constexpr int kNumThemes = (int) (sizeof (kThemes) / sizeof (kThemes[0]));

class MagicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MagicLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff10131c));
        setColour (juce::PopupMenu::textColourId,                  juce::Colours::white.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2a3350));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    }

    // Illuminated-ring hardware knob: machined housing, a backlit light-pipe ring in the knob's
    // colour (the value arc burns bright, the rest glows dimly), a knurled black cap with a
    // glossy face, and an outer scale whose ticks light up with the value. Bipolar-aware.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto c = bounds.getCentre();
        const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
        const bool hot = s.isMouseOverOrDragging();
        const float ang = a0 + pos * (a1 - a0);
        const bool withScale = R > 34.0f;

        float from = a0;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            from = a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0);
        const float lo = juce::jmin (from, ang), hi = juce::jmax (from, ang);

        auto polar = [c] (float radius, float a) { return juce::Point<float> (c.x + radius * std::sin (a), c.y - radius * std::cos (a)); };

        // ---- outer scale
        if (withScale)
        {
            const int ticks = 40;
            for (int i = 0; i <= ticks; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / (float) ticks;
                const bool major = i % 10 == 0;
                const bool lit = a >= lo - 0.001f && a <= hi + 0.001f && hi - lo > 0.002f;
                const float len = R * (major ? 0.11f : 0.06f);
                g.setColour (lit ? col.withAlpha (0.95f) : juce::Colours::white.withAlpha (major ? 0.42f : 0.20f));
                g.drawLine ({ polar (R, a), polar (R - len, a) }, major ? 1.4f : 1.0f);
            }
        }

        const float rH = withScale ? R * 0.84f : R * 0.98f;   // housing
        const float rRo = rH * 0.93f, rRi = rH * 0.79f;        // light-pipe ring
        const float rR = 0.5f * (rRo + rRi), rw = rRo - rRi;
        const float rC = rH * 0.70f;                            // cap

        // ---- housing: dark machined metal with a bevel
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - rH - 1.0f, c.y - rH + 2.0f, rH * 2.0f + 2.0f, rH * 2.0f + 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b404c), c.x - rH * 0.6f, c.y - rH,
                                                 juce::Colour (0xff0a0b0f), c.x + rH * 0.6f, c.y + rH, false));
        g.fillEllipse (c.x - rH, c.y - rH, rH * 2.0f, rH * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), c.x, c.y - rH,
                                                 juce::Colours::white.withAlpha (0.0f), c.x, c.y + rH * 0.2f, false));
        g.drawEllipse (c.x - rH + 0.5f, c.y - rH + 0.5f, rH * 2.0f - 1.0f, rH * 2.0f - 1.0f, 1.0f);

        // ---- light pipe: recessed glass, dim full ring, bright value arc with bloom
        g.setColour (juce::Colour (0xff050608));
        g.drawEllipse (c.x - rR, c.y - rR, rR * 2.0f, rR * 2.0f, rw + 2.0f);
        g.setColour (col.withAlpha (hot ? 0.10f : 0.07f));      // soft halo of the whole backlit ring
        g.drawEllipse (c.x - rR, c.y - rR, rR * 2.0f, rR * 2.0f, rw * 2.4f);
        g.setColour (col.withAlpha (hot ? 0.46f : 0.34f));
        g.drawEllipse (c.x - rR, c.y - rR, rR * 2.0f, rR * 2.0f, rw * 0.75f);

        if (hi - lo > 0.002f)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, rR, rR, 0.0f, lo, hi, true);
            const auto stroke = [&] (float wdt, juce::Colour cc)
            {
                g.setColour (cc);
                g.strokePath (arc, juce::PathStrokeType (wdt, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };
            stroke (rw * 3.4f, col.withAlpha (hot ? 0.13f : 0.09f));
            stroke (rw * 2.2f, col.withAlpha (hot ? 0.24f : 0.17f));
            stroke (rw * 1.1f, col.withAlpha (0.80f));
            stroke (rw * 0.75f, col.brighter (0.45f));
            stroke (juce::jmax (1.0f, rw * 0.28f), juce::Colours::white.withAlpha (0.75f));
        }

        // ---- cap: drop shadow, knurled skirt that turns with the value, glossy black face
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (c.x - rC - 1.0f, c.y - rC + 1.5f, rC * 2.0f + 2.0f, rC * 2.0f + 2.0f);
        g.setColour (juce::Colour (0xff15171c));
        g.fillEllipse (c.x - rC, c.y - rC, rC * 2.0f, rC * 2.0f);
        {
            const int teeth = rC > 22.0f ? 48 : 32;
            for (int i = 0; i < teeth; ++i)
            {
                const float a = ang + juce::MathConstants<float>::twoPi * (float) i / (float) teeth;
                const float lit = 0.5f + 0.5f * std::cos (a + 0.8f);   // light from the top-left
                g.setColour (juce::Colour (0xff2a2d35).interpolatedWith (juce::Colour (0xff5a5f6b), lit * 0.6f));
                g.drawLine ({ polar (rC - 0.5f, a), polar (rC * 0.86f, a) }, 1.1f);
            }
        }
        const float rF = rC * 0.84f;   // face
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff30333c), c.x - rF * 0.35f, c.y - rF * 0.45f,
                                                 juce::Colour (0xff060709), c.x + rF * 0.5f, c.y + rF * 0.8f, true));
        g.fillEllipse (c.x - rF, c.y - rF, rF * 2.0f, rF * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - rF, c.y - rF, rF * 2.0f, rF * 2.0f, 1.0f);

        // coloured reflection of the ring on the cap edge, and a soft specular highlight
        g.setGradientFill (juce::ColourGradient (col.withAlpha (hot ? 0.20f : 0.12f), c.x, c.y + rF,
                                                 col.withAlpha (0.0f), c.x, c.y + rF * 0.2f, false));
        g.fillEllipse (c.x - rF, c.y - rF, rF * 2.0f, rF * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), c.x - rF * 0.3f, c.y - rF * 0.75f,
                                                 juce::Colours::white.withAlpha (0.0f), c.x - rF * 0.1f, c.y - rF * 0.05f, false));
        g.fillEllipse (c.x - rF * 0.78f, c.y - rF * 0.86f, rF * 1.3f, rF * 0.9f);

        // ---- indicator: engraved line with a lit tip
        const auto p0 = polar (rF * 0.30f, ang), p1 = polar (rF * 0.90f, ang);
        g.setColour (col.withAlpha (0.35f));
        g.drawLine ({ p0, p1 }, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.drawLine ({ p0, p1 }, 2.2f);
        g.setColour (col.brighter (0.6f));
        g.fillEllipse (p1.x - 2.0f, p1.y - 2.0f, 4.0f, 4.0f);
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
            g.setColour (juce::Colours::white.withAlpha (major ? 0.26f : 0.12f));
            g.fillRect (cx - 11.0f - len, ty, len, 1.0f);
            g.fillRect (cx + 11.0f, ty, len, 1.0f);
        }

        // recessed slot with an inner shadow and a lip highlight
        const juce::Rectangle<float> slot (cx - 3.5f, top, 7.0f, bottom - top);
        g.setColour (juce::Colour (0xff040507));
        g.fillRoundedRectangle (slot, 3.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), cx - 3.5f, 0.0f,
                                                 juce::Colours::transparentBlack, cx + 1.5f, 0.0f, false));
        g.fillRoundedRectangle (slot, 3.5f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
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

        // machined cap: shadow, brushed metal body, grip grooves, lit index line
        const juce::Rectangle<float> thumb (cx - 14.0f, sliderPos - 8.0f, 28.0f, 16.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f).expanded (1.0f), 4.0f);
        if (hot)
        {
            g.setColour (col.withAlpha (0.22f));
            g.fillRoundedRectangle (thumb.expanded (4.0f), 7.0f);
        }
        juce::ColourGradient body (juce::Colour (0xff6c7282), 0.0f, thumb.getY(),
                                   juce::Colour (0xff1b1d24), 0.0f, thumb.getBottom(), false);
        body.addColour (0.48, juce::Colour (0xff3b3f4b));
        body.addColour (0.52, juce::Colour (0xff2a2d36));
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
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.5f : 0.28f));
        g.drawRoundedRectangle (thumb.reduced (0.5f), 3.5f, 1.0f);
        g.setColour (col.withAlpha (0.45f));
        g.fillRect (thumb.getX() + 3.0f, thumb.getCentreY() - 1.5f, thumb.getWidth() - 6.0f, 3.0f);
        g.setColour (col.brighter (0.6f));
        g.fillRect (thumb.getX() + 4.0f, thumb.getCentreY() - 0.5f, thumb.getWidth() - 8.0f, 1.0f);
    }
};
} // namespace MT
