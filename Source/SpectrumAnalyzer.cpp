#include "SpectrumAnalyzer.h"
#include "LookAndFeel.h"

using MT::uiFont;

namespace
{
juce::Colour lutFrom (std::initializer_list<std::pair<float, juce::uint32>> stops, float t)
{
    auto prev = *stops.begin();
    for (const auto& s : stops)
    {
        if (t <= s.first)
        {
            const float span = s.first - prev.first;
            const float k = span > 0.0f ? (t - prev.first) / span : 0.0f;
            return juce::Colour (prev.second).interpolatedWith (juce::Colour (s.second), k);
        }
        prev = s;
    }
    return juce::Colour (prev.second);
}

const juce::Colour kCloudCol  { 0xffc6fff2 };
const juce::Colour kCloudBg   { 0xff1a1c1f };
} // namespace

const char* SpectrumAnalyzer::modeName (int m)
{
    static const char* names[] = { "Classic", "3D Waterfall", "Spectral Cloud", "Waveform", "Spectrogram", "1/3-Octave RTA" };
    return names[juce::jlimit (0, (int) NumModes - 1, m)];
}

SpectrumAnalyzer::SpectrumAnalyzer (MagicTouchProcessor& p) : proc (p)
{
    outAvg.fill (-120.0f);
    outPeak.fill (-120.0f);
    inAvg.fill (-120.0f);
    slowAvg.fill (-120.0f);
    peakHoldT.fill (0.0f);
    for (auto& row : wf) row.fill (-120.0f);
    rta.fill (-120.0f);
    rtaPeak.fill (-120.0f);
    rtaHoldT.fill (0.0f);

    for (int i = 0; i < 256; ++i)
    {
        const float t = (float) i / 255.0f;
        fireLut[(size_t) i] = lutFrom ({ { 0.00f, 0xff1a0533 }, { 0.22f, 0xff4b0f73 }, { 0.42f, 0xffa3166b },
                                         { 0.60f, 0xffe8323a }, { 0.76f, 0xffff6a1a }, { 0.90f, 0xffffb02e },
                                         { 1.00f, 0xfffff2c6 } }, t);
        infernoLut[(size_t) i] = lutFrom ({ { 0.00f, 0xff000004 }, { 0.15f, 0xff1b0c41 }, { 0.30f, 0xff4a0c6b },
                                            { 0.45f, 0xff781c6d }, { 0.60f, 0xffa52c60 }, { 0.70f, 0xffcf4446 },
                                            { 0.80f, 0xffed6925 }, { 0.90f, 0xfffb9b06 }, { 0.97f, 0xfff7d13d },
                                            { 1.00f, 0xfffcffa4 } }, t);
    }

    mode = juce::jlimit (0, (int) NumModes - 1, (int) proc.apvts.state.getProperty ("anaMode", 0));
    setRepaintsOnMouseActivity (false);
}

void SpectrumAnalyzer::setBaseColour (juce::Colour c)
{
    if (c != base)
    {
        base = c;
        staticDirty = true;
    }
}

void SpectrumAnalyzer::resized()
{
    auto b = getLocalBounds().toFloat();
    plot = b.withTrimmedLeft (8.0f).withTrimmedRight (30.0f).withTrimmedTop (28.0f).withTrimmedBottom (20.0f);
    legendIn = { plot.getX() + 4.0f, plot.getY() + 3.0f, 30.0f, 14.0f };
    modeRect = { b.getRight() - 138.0f, b.getY() + 6.0f, 128.0f, 18.0f };
    staticDirty = true;
    glassImg = {};
    cloudImg = {};
    sgImg = {};
    wfImg = {};
    waveImg = {};
}

void SpectrumAnalyzer::setMode (int m, bool remember)
{
    mode = juce::jlimit (0, (int) NumModes - 1, m);
    if (remember)
        proc.apvts.state.setProperty ("anaMode", mode, nullptr);
    staticDirty = true;
    cloudImg = {};
    sgImg = {};
    waveImg = {};
    waveLastPos = -1;
    repaint();
}

void SpectrumAnalyzer::showModeMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("ANALYZER DISPLAY");
    for (int i = 0; i < NumModes; ++i)
        menu.addItem (i + 1, modeName (i), true, i == mode);

    juce::Component::SafePointer<SpectrumAnalyzer> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (localAreaToGlobal (modeRect.toNearestInt()))
                            .withMinimumWidth ((int) modeRect.getWidth()),
                        [safe] (int r)
                        {
                            if (safe != nullptr && r > 0)
                                safe->setMode (r - 1, true);
                        });
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

    // 1/3-octave ISO bands, each made of the display points inside it
    const float third = std::pow (2.0f, 1.0f / 6.0f);
    for (int k = 0; k < rtaBands; ++k)
    {
        const float fc = 1000.0f * std::pow (2.0f, (float) (k - 17) / 3.0f);
        int i0 = numPoints, i1 = -1;
        for (int i = 0; i < numPoints; ++i)
            if (freqs[(size_t) i] >= fc / third && freqs[(size_t) i] < fc * third)
            {
                i0 = std::min (i0, i);
                i1 = std::max (i1, i);
            }
        if (i1 < i0)   // nothing inside: use the nearest point
        {
            int best = 0;
            for (int i = 1; i < numPoints; ++i)
                if (std::abs (std::log (freqs[(size_t) i] / fc)) < std::abs (std::log (freqs[(size_t) best] / fc)))
                    best = i;
            i0 = i1 = best;
        }
        rtaRange[(size_t) k] = { i0, i1 };
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

    analyse (false, outAvg, dt, mode == Classic);
    if (mode == Classic && showInput)
        analyse (true, inAvg, dt, false);

    const float pw = plot.getWidth(), ph = plot.getHeight();

    switch (mode)
    {
        case Waterfall:
        {
            if (++wfTick >= 2 || ! wfImg.isValid())   // a new slice every other frame: ~2 s of history
            {
                wfTick = 0;
                wfHead = (wfHead + 1) % wfRows;
                auto& row = wf[(size_t) wfHead];
                for (int c = 0; c < wfCols; ++c)
                    row[(size_t) c] = valueAt (outAvg, (float) c / (float) (wfCols - 1));
                renderWaterfall();   // the landscape is only re-rendered when it actually changes
            }
            break;
        }

        case Cloud:
        {
            const float k = 1.0f - std::exp (-dt / 0.9f);
            for (int i = 0; i < numPoints; ++i)
                slowAvg[(size_t) i] += (outAvg[(size_t) i] - slowAvg[(size_t) i]) * k;

            if (cloudImg.getWidth() != (int) pw || cloudImg.getHeight() != (int) ph)
                cloudImg = juce::Image (juce::Image::ARGB, juce::jmax (1, (int) pw), juce::jmax (1, (int) ph), true);

            cloudImg.multiplyAllAlphas (0.88f);   // older traces fade into smoke
            juce::Graphics g (cloudImg);
            g.addTransform (juce::AffineTransform::translation (-plot.getX(), -plot.getY()));
            const auto line = makePath (outAvg, false);
            g.setColour (kCloudCol.withAlpha (0.05f));
            g.strokePath (line, juce::PathStrokeType (9.0f));
            g.setColour (kCloudCol.withAlpha (0.10f));
            g.strokePath (line, juce::PathStrokeType (3.5f));
            g.setColour (kCloudCol.withAlpha (0.22f));
            g.strokePath (line, juce::PathStrokeType (1.2f));
            break;
        }

        case Waveform:
            feedWaveform();
            break;

        case Spectrogram:
        {
            const int w = juce::jmax (2, (int) pw), h = juce::jmax (2, (int) ph);
            if (sgImg.getWidth() != w || sgImg.getHeight() != h)
            {
                sgImg = juce::Image (juce::Image::RGB, w, h, true);
                sgImg.clear (sgImg.getBounds(), juce::Colours::black);
            }
            sgImg.moveImageSection (0, 0, 1, 0, w - 1, h);   // scroll one pixel to the left
            for (int y = 0; y < h; ++y)
            {
                const float t = (float) (h - 1 - y) / (float) (h - 1);
                const float a = juce::jlimit (0.0f, 1.0f, (valueAt (outAvg, t) + 84.0f) / 78.0f);
                sgImg.setPixelAt (w - 1, y, infernoLut[(size_t) (a * 255.0f)]);
            }
            break;
        }

        case Rta:
        {
            for (int k = 0; k < rtaBands; ++k)
            {
                const auto [i0, i1] = rtaRange[(size_t) k];
                double p = 0.0;
                for (int i = i0; i <= i1; ++i)
                    p += std::pow (10.0, (double) outAvg[(size_t) i] / 10.0);
                const float db = (float) (10.0 * std::log10 (p / (double) (i1 - i0 + 1) + 1.0e-14));

                auto& v = rta[(size_t) k];
                v = db > v ? db : std::max (db, v - 24.0f * dt);
                auto& pk = rtaPeak[(size_t) k];
                auto& ht = rtaHoldT[(size_t) k];
                if (v >= pk)         { pk = v; ht = 1.0f; }
                else if (ht > 0.0f)  ht -= dt;
                else                 pk = std::max (v, pk - 12.0f * dt);
            }
            break;
        }

        default: break;
    }

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

float SpectrumAnalyzer::valueAt (const std::array<float, numPoints>& a, float t) const
{
    const float p = juce::jlimit (0.0f, 1.0f, t) * (float) (numPoints - 1);
    const int i = juce::jmin ((int) p, numPoints - 2);
    const float fr = p - (float) i;
    return a[(size_t) i] * (1.0f - fr) + a[(size_t) i + 1] * fr;
}

float SpectrumAnalyzer::valueAtX (const std::array<float, numPoints>& a, float x) const
{
    return valueAt (a, (x - plot.getX()) / plot.getWidth());
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

    // quadratic smoothing through midpoints gives a soft, continuous curve
    for (int i = 1; i < numPoints; ++i)
    {
        const auto cur = pt (i);
        p.quadraticTo (prev, (prev + cur) * 0.5f);
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

juce::Rectangle<float> SpectrumAnalyzer::rtaColumn (int band) const
{
    const float w = plot.getWidth() / (float) rtaBands;
    return { std::round (plot.getX() + (float) band * w + 1.0f), plot.getY(), std::max (2.0f, std::floor (w - 2.0f)), plot.getHeight() };
}

float SpectrumAnalyzer::rtaY (float db) const
{
    return juce::jmap (juce::jlimit (-72.0f, 0.0f, db), -72.0f, 0.0f, plot.getBottom(), plot.getY());
}

//==============================================================================
void SpectrumAnalyzer::buildStatic()
{
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    staticImg = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (staticImg);
    const auto b = getLocalBounds().toFloat();

    // panel background, per mode
    juce::Colour top = base.brighter (0.06f), bot = juce::Colour (0xff05060a);
    if (mode == Cloud)        { top = kCloudBg.brighter (0.05f); bot = kCloudBg.darker (0.4f); }
    else if (mode == Waveform) { top = juce::Colour (0xff0b0d14); bot = juce::Colour (0xff030407); }
    else if (mode == Waterfall) { top = juce::Colour (0xff0d0b14); bot = juce::Colour (0xff030306); }
    g.setGradientFill (juce::ColourGradient (top, 0.0f, b.getY(), bot, 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 10.0f);

    // mode dropdown pill
    g.setColour (juce::Colour (0xff151927));
    g.fillRoundedRectangle (modeRect, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (modeRect.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (uiFont (9.5f, true));
    g.drawText (modeName (mode), modeRect.withTrimmedLeft (9.0f).withTrimmedRight (18.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
    {
        juce::Path caret;
        const float ax = modeRect.getRight() - 10.0f, cy = modeRect.getCentreY();
        caret.startNewSubPath (ax - 3.0f, cy - 1.5f);
        caret.lineTo (ax, cy + 1.5f);
        caret.lineTo (ax + 3.0f, cy - 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.strokePath (caret, juce::PathStrokeType (1.4f));
    }

    g.setFont (uiFont (8.5f));
    const float labF[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* labT[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };

    auto logGrid = [&] (float minorA, float majorA)
    {
        for (int decade = 10; decade <= 10000; decade *= 10)
            for (int m = 1; m <= 9; ++m)
            {
                const float f = (float) (decade * m);
                if (f < minF || f > maxF) continue;
                const float x = std::round (xForFreq (f));
                g.setColour (juce::Colours::white.withAlpha (m == 1 ? majorA : minorA));
                g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
            }
    };
    auto freqLabels = [&] (float alpha)
    {
        g.setColour (juce::Colours::white.withAlpha (alpha));
        for (int i = 0; i < 8; ++i)
            g.drawText (labT[i], (int) xForFreq (labF[i]) - 16, (int) plot.getBottom() + 4, 32, 12,
                        juce::Justification::centred, false);
    };
    auto dbGrid = [&] (float alpha, float labelAlpha)
    {
        for (int db = 0; db >= (int) botDb; db -= 12)
        {
            const float y = std::round (yForDb ((float) db));
            g.setColour (juce::Colours::white.withAlpha (db == 0 ? alpha * 2.0f : alpha));
            g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
            g.setColour (juce::Colours::white.withAlpha (labelAlpha));
            g.drawText (juce::String (db), (int) plot.getRight() + 2, (int) y - 6, 26, 12, juce::Justification::centredLeft, false);
        }
    };

    switch (mode)
    {
        case Classic:
            logGrid (0.032f, 0.085f);
            dbGrid (0.045f, 0.30f);
            freqLabels (0.32f);
            break;

        case Cloud:   // brighter instrument-style grid
            logGrid (0.07f, 0.16f);
            dbGrid (0.10f, 0.45f);
            freqLabels (0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawRect (plot, 1.0f);
            break;

        case Waterfall:
        {
            // perspective floor lines
            for (int k = 0; k <= 4; ++k)
            {
                const float d = (float) k / 4.0f;
                const float y = plot.getBottom() - 6.0f - d * plot.getHeight() * 0.42f;
                const float x0 = plot.getX() + plot.getWidth() * 0.20f * d;
                const float x1 = x0 + plot.getWidth() * (1.0f - 0.22f * d);
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.drawLine (x0, y, x1, y, 1.0f);
            }
            for (int i = 0; i < 8; ++i)
            {
                const float t = std::log (labF[i] / minF) / std::log (maxF / minF);
                const float xf = plot.getX() + plot.getWidth() * t;
                const float xb = plot.getX() + plot.getWidth() * 0.20f + plot.getWidth() * 0.78f * t;
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.drawLine (xf, plot.getBottom() - 6.0f, xb, plot.getBottom() - 6.0f - plot.getHeight() * 0.42f, 1.0f);
            }
            freqLabels (0.35f);
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawText ("TIME", (int) plot.getRight() + 2, (int) (plot.getBottom() - plot.getHeight() * 0.45f), 28, 12,
                        juce::Justification::centredLeft, false);
            break;
        }

        case Waveform:
        {
            const float cy = std::round (plot.getCentreY()), half = plot.getHeight() * 0.46f;
            for (float a : { 0.5f, -0.5f })   // -6 dB guides
            {
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRect (plot.getX(), std::round (cy - a * half), plot.getWidth(), 1.0f);
            }
            g.setFont (uiFont (8.5f));
            for (int sec = 0; sec <= 5; ++sec)   // time ruler, newest audio on the right
            {
                const float x = std::round (plot.getRight() - (float) sec * wavePxPerSec);
                if (x < plot.getX()) break;
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawText (sec == 0 ? juce::String ("NOW") : "-" + juce::String (sec) + "s", (int) x - 18, (int) plot.getBottom() + 4, 36, 12,
                            juce::Justification::centred, false);
            }
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawText ("0 dB", (int) plot.getRight() + 2, (int) (cy - half) - 6, 28, 12, juce::Justification::centredLeft, false);
            g.drawText ("-6",   (int) plot.getRight() + 2, (int) (cy - 0.5f * half) - 6, 28, 12, juce::Justification::centredLeft, false);
            break;
        }

        case Spectrogram:
        {
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawText ("TIME", (int) plot.getX() + 4, (int) plot.getBottom() + 4, 60, 12, juce::Justification::centredLeft, false);
            g.drawText ("NOW", (int) plot.getRight() - 40, (int) plot.getBottom() + 4, 40, 12, juce::Justification::centredRight, false);
            break;
        }

        case Rta:
            drawRta (g, true);
            break;

        default: break;
    }

    staticDirty = false;
}

//==============================================================================
void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    if (staticDirty || staticImg.getWidth() != getWidth() || staticImg.getHeight() != getHeight())
        buildStatic();
    g.drawImageAt (staticImg, 0, 0);

    const auto b = getLocalBounds().toFloat();

    // title with status LED
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (b.getX() + 11.0f, b.getY() + 11.0f, 10.0f, 10.0f);
    g.setColour (accent.brighter (0.4f));
    g.fillEllipse (b.getX() + 13.5f, b.getY() + 13.5f, 5.0f, 5.0f);
    g.setColour (accent);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText ("ANALYZER", (int) b.getX() + 26, (int) b.getY() + 8, 120, 16, juce::Justification::left, false);

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (plot.toNearestInt().withTrimmedTop (-2));

    switch (mode)
    {
        case Classic:     drawClassic (g); break;
        case Waterfall:   drawWaterfall (g); break;
        case Cloud:       drawCloud (g); break;
        case Waveform:    drawWaveform (g); break;
        case Spectrogram: drawSpectrogram (g); break;
        case Rta:         drawRta (g, false); break;
        default: break;
    }
}

void SpectrumAnalyzer::paintOverChildren (juce::Graphics& g)
{
    if (glassImg.getWidth() != getWidth() + 4)
    {
        glassImg = juce::Image (juce::Image::ARGB, getWidth() + 4, getHeight() + 4, true);
        juce::Graphics gg (glassImg);
        MT::drawGlass (gg, juce::Rectangle<float> (2.0f, 2.0f, (float) getWidth(), (float) getHeight()), 10.0f);
    }
    g.drawImageAt (glassImg, -2, -2);
}

//==============================================================================
void SpectrumAnalyzer::drawClassic (juce::Graphics& g)
{
    const juce::Colour inCol (0xff9aa3b8);
    if (showInput)
    {
        g.setColour (inCol.withAlpha (0.07f));
        g.fillPath (makePath (inAvg, true));
        g.setColour (inCol.withAlpha (0.38f));
        g.strokePath (makePath (inAvg, false), juce::PathStrokeType (1.0f));
    }

    juce::ColourGradient grad (accent.withAlpha (0.55f), 0.0f, plot.getY(),
                               accent.withAlpha (0.03f), 0.0f, plot.getBottom(), false);
    grad.addColour (0.55, accent.withAlpha (0.22f));
    g.setGradientFill (grad);
    g.fillPath (makePath (outAvg, true));

    const auto line = makePath (outAvg, false);
    g.setColour (accent.withAlpha (0.18f));
    g.strokePath (line, juce::PathStrokeType (4.0f));
    g.setColour (accent.brighter (0.35f));
    g.strokePath (line, juce::PathStrokeType (1.4f));

    g.setColour (juce::Colours::white.withAlpha (0.42f));
    g.strokePath (makePath (outPeak, false), juce::PathStrokeType (1.0f));

    // legend (IN toggles the input trace)
    g.setFont (uiFont (8.5f, true));
    auto item = [&] (float x, juce::Colour c, const char* t, bool line_, bool on)
    {
        const float y = legendIn.getY();
        g.setColour (c.withAlpha (on ? 1.0f : 0.25f));
        if (line_) g.fillRect (x, y + 6.0f, 9.0f, 1.5f);
        else       g.fillEllipse (x + 1.0f, y + 3.5f, 6.0f, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.22f));
        g.drawText (t, (int) x + 11, (int) y, 30, 13, juce::Justification::left, false);
    };
    item (legendIn.getX(),         inCol, "IN", false, showInput);
    item (legendIn.getX() + 34.0f, accent, "OUT", false, true);
    item (legendIn.getX() + 74.0f, juce::Colours::white, "PEAK", true, true);

    drawHover (g, outAvg);
}

void SpectrumAnalyzer::drawWaterfall (juce::Graphics& g)
{
    if (wfImg.isValid())
        g.drawImageAt (wfImg, (int) plot.getX(), (int) plot.getY());
}

void SpectrumAnalyzer::renderWaterfall()
{
    const int iw = juce::jmax (1, (int) plot.getWidth()), ih = juce::jmax (1, (int) plot.getHeight());
    if (wfImg.getWidth() != iw || wfImg.getHeight() != ih)
        wfImg = juce::Image (juce::Image::ARGB, iw, ih, true);
    else
        wfImg.clear (wfImg.getBounds());

    juce::Graphics g (wfImg);
    g.addTransform (juce::AffineTransform::translation (-plot.getX(), -plot.getY()));
    const float W = plot.getWidth(), H = plot.getHeight();

    // back (oldest) to front (newest) so nearer slices cover the ones behind
    for (int r = 0; r < wfRows; ++r)
    {
        const int idx = (wfHead + 1 + r) % wfRows;
        const float t = (float) r / (float) (wfRows - 1);   // 0 = back, 1 = front
        const float d = 1.0f - t;
        const float baseY = plot.getBottom() - 6.0f - d * H * 0.42f;
        const float x0 = plot.getX() + W * 0.20f * d;
        const float w = W * (1.0f - 0.22f * d);
        const float ampH = H * 0.52f * (1.0f - 0.35f * d);
        const auto& row = wf[(size_t) idx];

        juce::Path p;
        p.startNewSubPath (x0, baseY);
        for (int c = 0; c < wfCols; ++c)
        {
            const float a = juce::jlimit (0.0f, 1.0f, (row[(size_t) c] + 80.0f) / 74.0f);
            p.lineTo (x0 + w * (float) c / (float) (wfCols - 1), baseY - a * ampH);
        }
        p.lineTo (x0 + w, baseY);
        p.closeSubPath();

        const float fog = 0.45f + 0.55f * t;   // distant slices are darker
        juce::ColourGradient grad (fireLut[30].withMultipliedBrightness (fog), 0.0f, baseY,
                                   fireLut[255].withMultipliedBrightness (fog), 0.0f, baseY - ampH, false);
        grad.addColour (0.30, fireLut[110].withMultipliedBrightness (fog));
        grad.addColour (0.55, fireLut[170].withMultipliedBrightness (fog));
        grad.addColour (0.80, fireLut[225].withMultipliedBrightness (fog));
        g.setGradientFill (grad);
        g.fillPath (p);
        if (r % 2 == 1 || r == wfRows - 1)   // ridge lines on every other slice
        {
            g.setColour (fireLut[240].withAlpha (0.18f + 0.3f * t));
            g.strokePath (p, juce::PathStrokeType (0.8f));
        }
    }
}

void SpectrumAnalyzer::drawCloud (juce::Graphics& g)
{
    if (cloudImg.isValid())
        g.drawImageAt (cloudImg, (int) plot.getX(), (int) plot.getY());

    // thin bright average line on top of the smoke
    g.setColour (kCloudCol.withAlpha (0.9f));
    g.strokePath (makePath (slowAvg, false), juce::PathStrokeType (1.0f));
    drawHover (g, outAvg);
}

void SpectrumAnalyzer::feedWaveform()
{
    const double srate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 44100.0;
    const int w = juce::jmax (2, (int) plot.getWidth()), h = juce::jmax (2, (int) plot.getHeight());
    if (waveImg.getWidth() != w || waveImg.getHeight() != h)
        waveImg = juce::Image (juce::Image::ARGB, w, h, true);

    // pull every sample that arrived since the last frame
    const int pos = proc.getAnalyserWritePos();
    if (waveLastPos < 0)
        waveLastPos = pos;
    int n = (pos - waveLastPos) & (MagicTouchProcessor::ringSize - 1);
    waveLastPos = pos;
    n = juce::jmin (n, MagicTouchProcessor::ringSize / 2);
    if (n <= 0)
        return;
    proc.copyAnalyserDataEndingAt (waveBuf.data(), n, pos, false);

    const int samplesPerCol = juce::jmax (1, (int) std::round (srate / (double) wavePxPerSec));
    const float a1 = std::exp (-juce::MathConstants<float>::twoPi * 200.0f  / (float) srate);
    const float a2 = std::exp (-juce::MathConstants<float>::twoPi * 2500.0f / (float) srate);

    const juce::Colour lowCol (0xffff2d55), midCol (0xff2fe08c), hiCol (0xff3ba7ff);
    const float cy = (float) h * 0.5f, half = (float) h * 0.46f;

    for (int i = 0; i < n; ++i)
    {
        const float x = waveBuf[(size_t) i];
        waveLp1 = x + a1 * (waveLp1 - x);
        waveLp2 = x + a2 * (waveLp2 - x);
        const float lo = waveLp1, mid = waveLp2 - waveLp1, hi = x - waveLp2;
        wPeak = std::max (wPeak, std::abs (x));
        wSq += x * x;
        wLo += lo * lo; wMid += mid * mid; wHi += hi * hi;

        if (++waveColCount < samplesPerCol)
            continue;

        // one finished column: scroll left a pixel and draw it at the right edge
        const float rms = std::sqrt (wSq / (float) samplesPerCol);
        const float l = std::sqrt (wLo), m = std::sqrt (wMid), hgh = std::sqrt (wHi) * 1.6f;
        const float sum = l + m + hgh + 1.0e-9f;
        const float r = (lowCol.getFloatRed()   * l + midCol.getFloatRed()   * m + hiCol.getFloatRed()   * hgh) / sum;
        const float gg = (lowCol.getFloatGreen() * l + midCol.getFloatGreen() * m + hiCol.getFloatGreen() * hgh) / sum;
        const float bb = (lowCol.getFloatBlue()  * l + midCol.getFloatBlue()  * m + hiCol.getFloatBlue()  * hgh) / sum;
        const auto col = juce::Colour::fromFloatRGBA (r, gg, bb, 1.0f).withMultipliedSaturation (1.15f).brighter (0.2f);

        waveImg.moveImageSection (0, 0, 1, 0, w - 1, h);
        waveImg.clear ({ w - 1, 0, 1, h });
        juce::Graphics g (waveImg);
        const float pk = juce::jmin (1.0f, wPeak) * half;
        const float rm = juce::jmin (1.0f, rms * 1.41f) * half;
        g.setColour (col.withAlpha (0.55f));
        g.fillRect ((float) (w - 1), cy - pk, 1.0f, pk * 2.0f + 1.0f);
        g.setColour (col.brighter (0.45f));
        g.fillRect ((float) (w - 1), cy - rm, 1.0f, rm * 2.0f + 1.0f);

        waveColCount = 0;
        wPeak = wSq = wLo = wMid = wHi = 0.0f;
    }
}

void SpectrumAnalyzer::drawWaveform (juce::Graphics& g)
{
    if (waveImg.isValid())
        g.drawImageAt (waveImg, (int) plot.getX(), (int) plot.getY());

    // centre line and a glowing "now" playhead at the right edge
    const float cy = std::round (plot.getCentreY());
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRect (plot.getX(), cy, plot.getWidth(), 1.0f);
    const float x = plot.getRight() - 1.0f;
    g.setColour (accent.withAlpha (0.25f));
    g.fillRect (x - 3.0f, plot.getY(), 6.0f, plot.getHeight());
    g.setColour (accent.brighter (0.6f));
    g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
}

void SpectrumAnalyzer::drawSpectrogram (juce::Graphics& g)
{
    if (sgImg.isValid())
        g.drawImageAt (sgImg, (int) plot.getX(), (int) plot.getY());

    // frequency guides on the left
    g.setFont (uiFont (8.5f, true));
    const float fs[] = { 100.0f, 1000.0f, 10000.0f };
    const char* ft[] = { "100", "1k", "10k" };
    for (int i = 0; i < 3; ++i)
    {
        const float t = std::log (fs[i] / minF) / std::log (maxF / minF);
        const float y = std::round (plot.getBottom() - t * plot.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (ft[i], (int) plot.getX() + 4, (int) y - 12, 30, 11, juce::Justification::left, false);
    }
}

void SpectrumAnalyzer::drawRta (juce::Graphics& g, bool staticPart)
{
    auto segColour = [] (float db)
    {
        if (db > -6.0f)  return juce::Colour (0xffff3b4f);
        if (db > -18.0f) return juce::Colour (0xffffd54a);
        return juce::Colour (0xff2fe08c);
    };
    const float bottom = plot.getBottom();

    if (staticPart)
    {
        g.setFont (uiFont (8.0f));
        for (int db = 0; db >= -72; db -= 12)
        {
            const float y = std::round (rtaY ((float) db));
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.drawText (juce::String (db), (int) plot.getRight() + 2, (int) y - 6, 26, 12, juce::Justification::centredLeft, false);
        }
        const char* lab[] = { "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
        for (int k = 0; k < rtaBands; ++k)
        {
            const auto col = rtaColumn (k);
            for (float y = bottom - 2.0f; y >= plot.getY(); y -= 3.0f)
            {
                const float db = juce::jmap (y, bottom, plot.getY(), -72.0f, 0.0f);
                g.setColour (segColour (db).withAlpha (0.08f));
                g.fillRect (col.getX(), y, col.getWidth(), 2.0f);
            }
            if (k % 3 == 1 && k / 3 < 10)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawText (lab[k / 3], (int) col.getCentreX() - 14, (int) bottom + 4, 28, 12, juce::Justification::centred, false);
            }
        }
        return;
    }

    for (int k = 0; k < rtaBands; ++k)
    {
        const auto col = rtaColumn (k);
        const float yTop = rtaY (rta[(size_t) k]);
        for (float y = bottom - 2.0f; y >= yTop - 1.0f && y >= plot.getY(); y -= 3.0f)
        {
            const float db = juce::jmap (y, bottom, plot.getY(), -72.0f, 0.0f);
            g.setColour (segColour (db));
            g.fillRect (col.getX(), y, col.getWidth(), 2.0f);
        }
        if (rtaPeak[(size_t) k] > -71.0f)
        {
            const float yp = std::round (rtaY (rtaPeak[(size_t) k]));
            g.setColour (segColour (rtaPeak[(size_t) k]).brighter (0.6f));
            g.fillRect (col.getX(), yp - 1.0f, col.getWidth(), 2.0f);
        }
    }
}

void SpectrumAnalyzer::drawHover (juce::Graphics& g, const std::array<float, numPoints>& a)
{
    if (hoverX < plot.getX() || hoverX > plot.getRight())
        return;

    const float f = freqForX (hoverX);
    const float db = valueAtX (a, hoverX);
    const float y = yForDb (db);
    const auto hc = mode == Cloud ? kCloudCol : accent;

    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.fillRect (std::round (hoverX), plot.getY(), 1.0f, plot.getHeight());
    g.setColour (hc.brighter (0.5f));
    g.fillEllipse (hoverX - 3.0f, y - 3.0f, 6.0f, 6.0f);

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midi = (int) std::round (69.0f + 12.0f * std::log2 (f / 440.0f));
    const juce::String note = juce::String (names[(midi % 12 + 12) % 12]) + juce::String (midi / 12 - 1);
    const juce::String fs = f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String ((int) std::round (f)) + " Hz";
    const juce::String txt = fs + "   " + note + "   " + juce::String (db, 1) + " dB";

    const float w = 128.0f, h = 18.0f;
    float bx = hoverX + 8.0f;
    if (bx + w > plot.getRight()) bx = hoverX - 8.0f - w;
    const juce::Rectangle<float> box (bx, plot.getY() + 20.0f, w, h);
    g.setColour (juce::Colour (0xe0080a10));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (hc.withAlpha (0.5f));
    g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (uiFont (9.5f, true));
    g.drawText (txt, box.toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
void SpectrumAnalyzer::mouseMove (const juce::MouseEvent& e) { hoverX = e.position.x; }
void SpectrumAnalyzer::mouseExit (const juce::MouseEvent&)   { hoverX = -1.0f; }

void SpectrumAnalyzer::mouseDown (const juce::MouseEvent& e)
{
    if (modeRect.contains (e.position))
    {
        showModeMenu();
        return;
    }
    if (mode == Classic && legendIn.contains (e.position))
    {
        showInput = ! showInput;
        if (showInput)
            inAvg.fill (-120.0f);
    }
}
