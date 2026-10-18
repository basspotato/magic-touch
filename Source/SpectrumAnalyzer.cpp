#include "SpectrumAnalyzer.h"
#include "LookAndFeel.h"

using MT::uiFont;

SpectrumAnalyzer::SpectrumAnalyzer (MagicTouchProcessor& p) : proc (p)
{
    outAvg.fill (-120.0f);
    outPeak.fill (-120.0f);
    inAvg.fill (-120.0f);
    peakHoldT.fill (0.0f);
    setRepaintsOnMouseActivity (false);
}

void SpectrumAnalyzer::resized()
{
    auto b = getLocalBounds().toFloat();
    plot = b.withTrimmedLeft (8.0f).withTrimmedRight (30.0f).withTrimmedTop (28.0f).withTrimmedBottom (20.0f);
    legendIn = { b.getRight() - 150.0f, b.getY() + 7.0f, 36.0f, 16.0f };
}

//==============================================================================
void SpectrumAnalyzer::rebuildBands (double sampleRate)
{
    bandsRate = sampleRate;
    const float binHz = (float) sampleRate / (float) size;
    const float sixth = std::pow (2.0f, 1.0f / 12.0f);   // +/- 1/12 octave = 1/6-octave band

    for (int i = 0; i < numPoints; ++i)
    {
        const float f = minF * std::pow (maxF / minF, (float) i / (float) (numPoints - 1));
        freqs[(size_t) i] = f;
        bands[(size_t) i] = { (f / sixth) / binHz, (f * sixth) / binHz };
    }
}

void SpectrumAnalyzer::analyse (bool input, std::array<float, numPoints>& target, float dt, bool withPeak)
{
    proc.copyAnalyserData (work.data(), size, input);
    std::fill (work.begin() + size, work.end(), 0.0f);
    window.multiplyWithWindowingTable (work.data(), (size_t) size);
    fft.performFrequencyOnlyForwardTransform (work.data());

    const int maxBin = size / 2 - 1;
    const float norm = 2.0f / (float) size;
    const float atk = 1.0f - std::exp (-dt / 0.025f);
    const float rel = 1.0f - std::exp (-dt / 0.30f);

    for (int i = 0; i < numPoints; ++i)
    {
        const auto band = bands[(size_t) i];
        float power;

        if (band.b1 - band.b0 < 1.5f)
        {
            // narrower than a bin (low end): interpolate the magnitude at the band centre
            const float c = juce::jlimit (1.0f, (float) maxBin - 1.0f, 0.5f * (band.b0 + band.b1));
            const int k = (int) c;
            const float fr = c - (float) k;
            const float m = (work[(size_t) k] * (1.0f - fr) + work[(size_t) k + 1] * fr) * norm;
            power = m * m;
        }
        else
        {
            const int k0 = juce::jlimit (1, maxBin, (int) std::ceil (band.b0));
            const int k1 = juce::jlimit (k0, maxBin, (int) std::floor (band.b1));
            double sum = 0.0;
            for (int k = k0; k <= k1; ++k)
            {
                const float m = work[(size_t) k] * norm;
                sum += (double) (m * m);
            }
            power = (float) (sum / (double) (k1 - k0 + 1));
        }

        float db = 10.0f * std::log10 (power + 1.0e-14f);
        db += 3.0f * std::log2 (freqs[(size_t) i] / 1000.0f);   // 3 dB/oct tilt, pivot at 1 kHz

        auto& v = target[(size_t) i];
        v += (db - v) * (db > v ? atk : rel);

        if (withPeak)
        {
            auto& pk = outPeak[(size_t) i];
            auto& t = peakHoldT[(size_t) i];
            if (v >= pk) { pk = v; t = 1.0f; }
            else if (t > 0.0f) t -= dt;
            else pk = std::max (v, pk - 10.0f * dt);
        }
    }
}

void SpectrumAnalyzer::update (float dt)
{
    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    if (srate != bandsRate)
        rebuildBands (srate);

    analyse (false, outAvg, dt, true);
    if (showInput)
        analyse (true, inAvg, dt, false);

    repaint();
}

//==============================================================================
float SpectrumAnalyzer::xForFreq (float f) const
{
    return plot.getX() + plot.getWidth() * std::log (f / minF) / std::log (maxF / minF);
}

float SpectrumAnalyzer::freqForX (float x) const
{
    return minF * std::pow (maxF / minF, juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth()));
}

float SpectrumAnalyzer::yForDb (float db) const
{
    return juce::jmap (juce::jlimit (botDb - 6.0f, topDb + 6.0f, db), botDb, topDb, plot.getBottom(), plot.getY());
}

float SpectrumAnalyzer::valueAtX (const std::array<float, numPoints>& a, float x) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth()) * (float) (numPoints - 1);
    const int i = juce::jmin ((int) t, numPoints - 2);
    const float fr = t - (float) i;
    return a[(size_t) i] * (1.0f - fr) + a[(size_t) i + 1] * fr;
}

juce::Path SpectrumAnalyzer::makePath (const std::array<float, numPoints>& a, bool closed) const
{
    juce::Path p;
    auto pt = [&] (int i)
    {
        return juce::Point<float> (xForFreq (freqs[(size_t) i]),
                                   juce::jmin (plot.getBottom(), yForDb (a[(size_t) i])));
    };

    auto prev = pt (0);
    if (closed)
    {
        p.startNewSubPath (prev.x, plot.getBottom());
        p.lineTo (prev);
    }
    else
        p.startNewSubPath (prev);

    // quadratic smoothing through midpoints gives the soft, continuous Ozone-style curve
    for (int i = 1; i < numPoints; ++i)
    {
        const auto cur = pt (i);
        const auto mid = (prev + cur) * 0.5f;
        p.quadraticTo (prev, mid);
        prev = cur;
    }
    p.lineTo (prev);

    if (closed)
    {
        p.lineTo (prev.x, plot.getBottom());
        p.closeSubPath();
    }
    return p;
}

//==============================================================================
void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // panel
    g.setGradientFill (juce::ColourGradient (base.brighter (0.06f), 0.0f, b.getY(),
                                             juce::Colour (0xff05060a), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (accent);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText ("ANALYZER", (int) b.getX() + 14, (int) b.getY() + 8, 120, 16, juce::Justification::left, false);

    // legend: IN (click to toggle) / OUT / PEAK
    {
        g.setFont (uiFont (9.0f, true));
        auto item = [&] (float x, juce::Colour c, const char* t, bool line, bool on)
        {
            g.setColour (c.withAlpha (on ? 1.0f : 0.25f));
            if (line) g.fillRect (x, b.getY() + 14.5f, 9.0f, 1.5f);
            else      g.fillEllipse (x + 1.0f, b.getY() + 11.0f, 7.0f, 7.0f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.22f));
            g.drawText (t, (int) x + 12, (int) b.getY() + 8, 34, 14, juce::Justification::left, false);
        };
        item (legendIn.getX() + 2.0f,  juce::Colour (0xff9aa3b8), "IN", false, showInput);
        item (legendIn.getX() + 44.0f, accent, "OUT", false, true);
        item (legendIn.getX() + 92.0f, juce::Colours::white, "PEAK", true, true);
    }

    juce::Graphics::ScopedSaveState ss (g);

    // grid
    g.setFont (uiFont (8.5f));
    for (int decade = 10; decade <= 10000; decade *= 10)
        for (int m = 1; m <= 9; ++m)
        {
            const float f = (float) (decade * m);
            if (f < minF || f > maxF) continue;
            const float x = std::round (xForFreq (f));
            g.setColour (juce::Colours::white.withAlpha (m == 1 ? 0.085f : 0.032f));
            g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
        }

    const float labF[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* labT[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    g.setColour (juce::Colours::white.withAlpha (0.32f));
    for (int i = 0; i < 8; ++i)
        g.drawText (labT[i], (int) xForFreq (labF[i]) - 16, (int) plot.getBottom() + 4, 32, 12,
                    juce::Justification::centred, false);

    for (int db = 0; db >= (int) botDb; db -= 12)
    {
        const float y = std::round (yForDb ((float) db));
        g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.09f : 0.045f));
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawText (juce::String (db), (int) plot.getRight() + 2, (int) y - 6, 26, 12, juce::Justification::centredLeft, false);
    }

    g.reduceClipRegion (plot.toNearestInt().withTrimmedTop (-2));

    // input (before) spectrum
    if (showInput)
    {
        g.setColour (juce::Colour (0xff9aa3b8).withAlpha (0.07f));
        g.fillPath (makePath (inAvg, true));
        g.setColour (juce::Colour (0xff9aa3b8).withAlpha (0.38f));
        g.strokePath (makePath (inAvg, false), juce::PathStrokeType (1.0f));
    }

    // output (after) spectrum
    {
        juce::ColourGradient grad (accent.withAlpha (0.55f), 0.0f, plot.getY(),
                                   accent.withAlpha (0.03f), 0.0f, plot.getBottom(), false);
        grad.addColour (0.55, accent.withAlpha (0.22f));
        g.setGradientFill (grad);
        g.fillPath (makePath (outAvg, true));

        const auto line = makePath (outAvg, false);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (line, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.brighter (0.35f));
        g.strokePath (line, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // peak hold
    g.setColour (juce::Colours::white.withAlpha (0.42f));
    g.strokePath (makePath (outPeak, false), juce::PathStrokeType (1.0f));

    // hover readout
    if (hoverX >= plot.getX() && hoverX <= plot.getRight())
    {
        const float f = freqForX (hoverX);
        const float db = valueAtX (outAvg, hoverX);
        const float y = yForDb (db);

        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRect (std::round (hoverX), plot.getY(), 1.0f, plot.getHeight());
        g.setColour (accent.brighter (0.5f));
        g.fillEllipse (hoverX - 3.0f, y - 3.0f, 6.0f, 6.0f);

        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int midi = (int) std::round (69.0f + 12.0f * std::log2 (f / 440.0f));
        const juce::String note = juce::String (names[(midi % 12 + 12) % 12]) + juce::String (midi / 12 - 1);
        const juce::String fs = f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String ((int) std::round (f)) + " Hz";
        const juce::String txt = fs + "   " + note + "   " + juce::String (db, 1) + " dB";

        const float w = 128.0f, h = 18.0f;
        float bx = hoverX + 8.0f;
        if (bx + w > plot.getRight()) bx = hoverX - 8.0f - w;
        const juce::Rectangle<float> box (bx, plot.getY() + 4.0f, w, h);
        g.setColour (juce::Colour (0xe0080a10));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (accent.withAlpha (0.5f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.setFont (uiFont (9.5f, true));
        g.drawText (txt, box.toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
void SpectrumAnalyzer::mouseMove (const juce::MouseEvent& e) { hoverX = e.position.x; }
void SpectrumAnalyzer::mouseExit (const juce::MouseEvent&)   { hoverX = -1.0f; }

void SpectrumAnalyzer::mouseDown (const juce::MouseEvent& e)
{
    if (legendIn.contains (e.position))
    {
        showInput = ! showInput;
        if (showInput)
            inAvg.fill (-120.0f);
    }
}
