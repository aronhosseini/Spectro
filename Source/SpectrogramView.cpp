#include "SpectrogramView.h"
#include "Params.h"
#include "Theme.h"

using namespace juce;

namespace
{
    constexpr float kMinHz = 15.0f, kMaxHz = 16000.0f;

    float pv (AudioProcessorValueTreeState& a, const char* id) { return a.getRawParameterValue (id)->load(); }

    float fmtPos (float hz, float lo, float hi, bool logAxis)
    {
        if (logAxis) return std::log (hz / lo) / std::log (hi / lo);
        return (hz - lo) / (hi - lo);
    }
}

SpectrogramView::SpectrogramView (SpectroProcessor& p) : proc (p), apvts (p.apvts)
{
    setOpaque (true);
    startTimerHz (30);
}

Rectangle<int> SpectrogramView::plotArea() const
{
    // Leave a strip at the bottom for the frequency scale and a gutter on the right for tick marks.
    return getLocalBounds().withTrimmedBottom (26).withTrimmedRight (12);
}

float SpectrogramView::freqAtFraction (float f) const
{
    const float lo = jmax (kMinHz, pv (apvts, ids::fLo));
    const float hi = jmax (lo * 1.1f, pv (apvts, ids::fHi));
    if (pv (apvts, ids::xlog) > 0.5f)
        return lo * std::pow (hi / lo, f);
    return lo + (hi - lo) * f;
}

float SpectrogramView::weightingDb (float f, int type) const
{
    // IEC 61672 curves. 0 = A, 1 = B, 2 = Z (flat), 3 = C.
    const double f2 = (double) f * f;
    if (type == 0)
    {
        const double ra = (12194.0 * 12194.0 * f2 * f2)
                          / ((f2 + 20.6 * 20.6) * std::sqrt ((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0));
        return (float) (20.0 * std::log10 (ra) + 2.0);
    }
    if (type == 1)
    {
        const double rb = (12194.0 * 12194.0 * f2 * (double) f)
                          / ((f2 + 20.6 * 20.6) * std::sqrt (f2 + 158.5 * 158.5) * (f2 + 12194.0 * 12194.0));
        return (float) (20.0 * std::log10 (rb) + 0.17);
    }
    if (type == 3)
    {
        const double rc = (12194.0 * 12194.0 * f2) / ((f2 + 20.6 * 20.6) * (f2 + 12194.0 * 12194.0));
        return (float) (20.0 * std::log10 (rc) + 0.06);
    }
    return 0.0f;
}

void SpectrogramView::resetHistory()
{
    history.clear (history.getBounds(), Colours::black);
    writeRow = 0;
    rowsWritten = 0;
}

void SpectrogramView::rebuildIfNeeded()
{
    const int size = (int) pv (apvts, ids::fftSize);
    const int win  = (int) pv (apvts, ids::window);
    const int wt   = (int) pv (apvts, ids::weighting);
    const int rng  = (int) pv (apvts, ids::range);
    const bool lg  = pv (apvts, ids::xlog) > 0.5f;
    const float lo = pv (apvts, ids::fLo), hi = pv (apvts, ids::fHi);

    const bool axisChanged = lg != lastLog || lo != lastLo || hi != lastHi || rng != lastRange;

    if (size != lastSize || win != lastWin)
    {
        fftOrder = 9 + size;                                   // choice 0 = 512 = 2^9
        const int n = 1 << fftOrder;
        fft = std::make_unique<dsp::FFT> (fftOrder);
        using WF = dsp::WindowingFunction<float>;
        const WF::WindowingMethod methods[] = { WF::hann, WF::hamming, WF::blackman, WF::rectangular };
        windowFn = std::make_unique<WF> ((size_t) n, methods[jlimit (0, 3, win)], true);

        window.assign ((size_t) n, 1.0f);
        windowFn->multiplyWithWindowingTable (window.data(), (size_t) n);   // window applied to ones
        coherentGain = std::accumulate (window.begin(), window.end(), 0.0f) / (float) n;

        ring.assign ((size_t) n, 0.0f);
        work.assign ((size_t) n * 2, 0.0f);
        ringPos = 0;
        lastSize = size; lastWin = win;
    }

    if (axisChanged || size != lastSize || wt != lastWeight || colBin.empty())
    {
        const int n = 1 << fftOrder;
        const double sr = proc.getSampleRateSafe();
        weightCache.assign ((size_t) n / 2 + 1, 0.0f);
        for (int k = 1; k <= n / 2; ++k)
            weightCache[(size_t) k] = weightingDb ((float) (k * sr / n), wt);

        if (axisChanged)
            resetHistory();

        lastLog = lg; lastLo = lo; lastHi = hi; lastRange = rng; lastWeight = wt;
    }
    else if (wt != lastWeight)
        lastWeight = wt;
}

void SpectrogramView::computeRow()
{
    const int n = 1 << fftOrder;
    const double sr = proc.getSampleRateSafe();

    // Copy the last n samples out of the ring buffer, window them, run the FFT.
    for (int i = 0; i < n; ++i)
        work[(size_t) i] = ring[(size_t) ((ringPos + i) % n)] * window[(size_t) i];
    std::fill (work.begin() + n, work.end(), 0.0f);
    fft->performFrequencyOnlyForwardTransform (work.data());

    const float norm = 2.0f / ((float) n * coherentGain);   // full-scale sine -> 1.0 (0 dB)
    const float gain = pv (apvts, ids::gain);
    const float fl   = pv (apvts, ids::floorDb);
    const float top  = pv (apvts, ids::memTop), bot = pv (apvts, ids::memBottom);
    const float contrast = 0.5f + 0.25f * pv (apvts, ids::contrast);   // 0.5 .. 1.5

    const bool descending = pv (apvts, ids::ydir) > 0.5f;
    const int  y = descending ? writeRow : 0;
    if (! descending)
        history.moveImageSection (0, 1, 0, 0, cols, rows - 1);   // push history down, newest on top

    Image::BitmapData bd (history, Image::BitmapData::writeOnly);
    for (int x = 0; x < cols; ++x)
    {
        const float f0 = freqAtFraction ((float) x / cols);
        const float f1 = freqAtFraction ((float) (x + 1) / cols);
        const float b0 = f0 * n / (float) sr, b1 = f1 * n / (float) sr;

        // Peak across the bins this pixel column covers; interpolate when it covers less than one bin.
        float mag;
        if (b1 - b0 < 1.0f)
        {
            const float b = 0.5f * (b0 + b1);
            const int i = jlimit (0, n / 2 - 1, (int) b);
            const float fr = b - (float) i;
            mag = work[(size_t) i] * (1.0f - fr) + work[(size_t) i + 1] * fr;
        }
        else
        {
            mag = 0.0f;
            const int i0 = jlimit (0, n / 2, (int) b0), i1 = jlimit (0, n / 2, (int) b1);
            for (int i = i0; i <= i1; ++i) mag = jmax (mag, work[(size_t) i]);
        }

        const int kb = jlimit (1, n / 2, (int) (0.5f * (b0 + b1)));
        const float db = Decibels::gainToDecibels (mag * norm, -200.0f) + weightCache[(size_t) kb] + gain;

        Colour c = Colours::black;
        if (db > fl)
        {
            float t = (db - bot) / jmax (1.0f, top - bot);
            t = 0.5f + (t - 0.5f) * contrast;
            c = theme::heat (t);
        }
        bd.setPixelColour (x, y, c);
    }

    if (descending) writeRow = (writeRow + 1) % rows;
    rowsWritten = jmin (rows, rowsWritten + 1);
}

void SpectrogramView::timerCallback()
{
    rebuildIfNeeded();

    const bool frozen = pv (apvts, ids::hold) > 0.5f;
    const int got = proc.pullSamples (chunk.data(), (int) chunk.size());
    if (got <= 0 || frozen)
        return;

    const int n = 1 << fftOrder;
    const double rangeSec[] = { 10.0, 30.0, 60.0, 300.0 };
    const double perRow = proc.getSampleRateSafe() * rangeSec[jlimit (0, 3, (int) pv (apvts, ids::range))] / rows;

    for (int i = 0; i < got; ++i)
    {
        ring[(size_t) ringPos] = chunk[(size_t) i];
        ringPos = (ringPos + 1) % n;
        sinceRow += 1.0;
        if (sinceRow >= perRow)
        {
            sinceRow -= perRow;
            computeRow();
        }
    }
    repaint();
}

void SpectrogramView::mouseMove (const MouseEvent& e)
{
    hoverX = (float) e.x;
    hoverY = (float) e.y;
    repaint();
}

void SpectrogramView::paint (Graphics& g)
{
    g.fillAll (Colours::black);
    const auto area = plotArea();

    // Data
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        if (pv (apvts, ids::ydir) > 0.5f && rowsWritten >= rows)
        {
            // Ring buffer: oldest row sits at writeRow.
            const float rh = (float) area.getHeight() / rows;
            const int upper = rows - writeRow;
            g.drawImage (history, area.getX(), area.getY(), area.getWidth(), (int) (upper * rh),
                         0, writeRow, cols, upper);
            g.drawImage (history, area.getX(), area.getY() + (int) (upper * rh), area.getWidth(),
                         area.getHeight() - (int) (upper * rh), 0, 0, cols, writeRow);
        }
        else
            g.drawImage (history, area.toFloat());
    }

    const bool logAxis = pv (apvts, ids::xlog) > 0.5f;
    const float lo = jmax (kMinHz, pv (apvts, ids::fLo));
    const float hi = jmax (lo * 1.1f, pv (apvts, ids::fHi));
    static const float ticks[] = { 15, 31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    static const char* names[] = { "15", "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };

    // Grid
    if (pv (apvts, ids::showGrid) > 0.5f)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        for (float t : ticks)
        {
            if (t < lo || t > hi) continue;
            const float x = area.getX() + area.getWidth() * fmtPos (t, lo, hi, logAxis);
            g.drawVerticalLine ((int) x, (float) area.getY(), (float) area.getBottom());
        }
        for (int i = 1; i < 5; ++i)
            g.drawHorizontalLine (area.getY() + area.getHeight() * i / 5, (float) area.getX(), (float) area.getRight());
    }

    // X axis labels
    if (pv (apvts, ids::showX) > 0.5f)
    {
        auto strip = getLocalBounds().removeFromBottom (26).withTrimmedRight (12);
        g.setColour (theme::bg1);
        g.fillRect (strip);
        g.setFont (theme::font (12.0f));
        for (int i = 0; i < 11; ++i)
        {
            if (ticks[i] < lo || ticks[i] > hi) continue;
            const float x = area.getX() + area.getWidth() * fmtPos (ticks[i], lo, hi, logAxis);
            g.setColour (theme::textDim);
            g.drawVerticalLine ((int) x, (float) strip.getY(), (float) strip.getY() + 6.0f);
            g.setColour (theme::text);
            g.drawText (names[i], Rectangle<int> ((int) x + 3, strip.getY() + 4, 40, 18), Justification::left, false);
        }
    }

    // Y ticks on the right gutter
    if (pv (apvts, ids::showY) > 0.5f)
    {
        g.setColour (theme::textDim);
        for (int i = 1; i < 10; ++i)
            g.drawHorizontalLine (area.getY() + area.getHeight() * i / 10, (float) area.getRight() + 2.0f, (float) area.getRight() + 8.0f);
    }

    // Read-out
    if (pv (apvts, ids::showRead) > 0.5f && area.toFloat().contains (hoverX, hoverY))
    {
        const float f = freqAtFraction ((hoverX - area.getX()) / (float) area.getWidth());
        const String txt = f >= 1000.0f ? String (f / 1000.0f, 2) + " kHz" : String (f, 1) + " Hz";
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawVerticalLine ((int) hoverX, (float) area.getY(), (float) area.getBottom());
        const Rectangle<int> r ((int) hoverX + 8, (int) hoverY - 22, 82, 20);
        g.setColour (theme::bg0.withAlpha (0.82f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (12.0f));
        g.drawText (txt, r, Justification::centred);
    }
}
