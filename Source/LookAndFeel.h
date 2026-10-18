#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace MT
{
inline juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

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

    // Glowing arc knob, bipolar-aware (arc grows from the centre for +/- controls)
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float arcR = r - 2.0f;
        const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
        const bool hot = s.isMouseOverOrDragging();
        const float ang = a0 + pos * (a1 - a0);

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (juce::Colour (0xff1f2433));
        g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float from = a0;
        if (s.getMinimum() < 0.0 && s.getMaximum() > 0.0)
            from = a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0);

        const float lo = juce::jmin (from, ang), hi = juce::jmax (from, ang);
        if (hi - lo > 0.002f)
        {
            juce::Path v;
            v.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, lo, hi, true);
            g.setColour (col.withAlpha (hot ? 0.34f : 0.20f));
            g.strokePath (v, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.withAlpha (0.45f));
            g.strokePath (v, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col.brighter (0.25f));
            g.strokePath (v, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // knob body
        const float br = r - 10.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a4260), c.x - br * 0.4f, c.y - br * 0.7f,
                                                 juce::Colour (0xff0f121b), c.x + br * 0.6f, c.y + br * 0.9f, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.28f : 0.12f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

        // pointer
        const float dx = std::sin (ang), dy = -std::cos (ang);
        juce::Path ptr;
        ptr.startNewSubPath (c.x + dx * br * 0.40f, c.y + dy * br * 0.40f);
        ptr.lineTo          (c.x + dx * br * 0.88f, c.y + dy * br * 0.88f);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (ptr, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
        const float top = juce::jmin (minPos, maxPos) - 6.0f;
        const float bottom = juce::jmax (minPos, maxPos) + 6.0f;
        const auto col = s.findColour (juce::Slider::thumbColourId);
        const bool hot = s.isMouseOverOrDragging();

        // scale ticks
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = juce::jmap ((float) i, 0.0f, 10.0f, top + 6.0f, bottom - 6.0f);
            const float len = (i % 5 == 0) ? 7.0f : 4.0f;
            g.drawLine (cx - 12.0f - len, ty, cx - 12.0f, ty, 1.0f);
            g.drawLine (cx + 12.0f, ty, cx + 12.0f + len, ty, 1.0f);
        }

        // track
        g.setColour (juce::Colour (0xff1c2130));
        g.fillRoundedRectangle (cx - 2.5f, top, 5.0f, bottom - top, 2.5f);

        // filled part with glow
        g.setColour (col.withAlpha (hot ? 0.32f : 0.20f));
        g.fillRoundedRectangle (cx - 5.0f, sliderPos, 10.0f, bottom - sliderPos, 5.0f);
        g.setColour (col.withAlpha (0.9f));
        g.fillRoundedRectangle (cx - 2.5f, sliderPos, 5.0f, bottom - sliderPos, 2.5f);

        // thumb
        const juce::Rectangle<float> thumb (cx - 13.0f, sliderPos - 6.5f, 26.0f, 13.0f);
        if (hot)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillRoundedRectangle (thumb.expanded (4.0f), 7.0f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff59628a), 0.0f, thumb.getY(),
                                                 juce::Colour (0xff1d2233), 0.0f, thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.45f : 0.22f));
        g.drawRoundedRectangle (thumb.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (col.brighter (0.4f));
        g.fillRoundedRectangle (thumb.getX() + 5.0f, thumb.getCentreY() - 1.0f, thumb.getWidth() - 10.0f, 2.0f, 1.0f);
    }
};
} // namespace MT
