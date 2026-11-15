#include "PluginEditor.h"

namespace
{
    const juce::Colour kAccent (0xff4da3ff), kText (0xffe8ecf4), kMuted (0xff8791a5),
                       kPanel (0xb8181c26), kLine (0x1fffffff), kDeep (0xff06080b);
    juce::Font ui (float h, bool bold = true) { return juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain); }
}

// ───────────────────────── Look & Feel
SpectroLookAndFeel::SpectroLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf0141822));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.35f));
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, kDeep);
    setColour (juce::Slider::textBoxOutlineColourId, kLine);
}

void SpectroLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    juce::ColourGradient grad (on ? kAccent.withAlpha (0.55f) : juce::Colour (0x40ffffff).withAlpha (over ? 0.14f : 0.07f), 0, r.getY(),
                               on ? kAccent.withAlpha (0.25f) : juce::Colour (0x20ffffff).withAlpha (0.05f), 0, r.getBottom(), false);
    g.setGradientFill (grad);  g.fillRoundedRectangle (r, 8.f);
    g.setColour (on ? kAccent.withAlpha (0.8f) : juce::Colour (0x2effffff)); g.drawRoundedRectangle (r, 8.f, 1.f);
}

// The label is drawn here, once. (It used to be drawn in drawButtonBackground as well, so JUCE's own
// drawButtonText painted a second copy on top and every TextButton looked doubled / blurry.)
static void drawChipLabel (juce::Graphics& g, juce::Button& b, bool over)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : (over ? kText : kMuted));
    g.setFont (ui (12.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, false);
}

void SpectroLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    drawChipLabel (g, b, over);
}

void SpectroLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    if (b.getButtonText().startsWith ("X: "))  // pill-style toggle
    { drawButtonBackground (g, b, {}, over, false); drawChipLabel (g, b, over); return; }
    auto box = r.removeFromLeft (r.getHeight()).reduced (3.f);
    g.setColour (b.getToggleState() ? kAccent : kDeep); g.fillRoundedRectangle (box, 4.f);
    g.setColour (kLine.brighter (0.2f)); g.drawRoundedRectangle (box, 4.f, 1.f);
    if (b.getToggleState())
    { juce::Path p; p.startNewSubPath (box.getX() + 3, box.getCentreY()); p.lineTo (box.getCentreX() - 1, box.getBottom() - 3.5f); p.lineTo (box.getRight() - 3, box.getY() + 3.5f);
      g.setColour (juce::Colour (0xff04121f)); g.strokePath (p, juce::PathStrokeType (2.f)); }
    g.setColour (kMuted); g.setFont (ui (11.f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (6.f), juce::Justification::centredLeft);
}

void SpectroLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (kDeep); g.fillRoundedRectangle (r, 8.f);
    g.setColour (kLine); g.drawRoundedRectangle (r, 8.f, 1.f);
    juce::Path a; a.addTriangle ((float) w - 20, h * 0.42f, (float) w - 10, h * 0.42f, (float) w - 15, h * 0.62f);
    g.setColour (kMuted); g.fillPath (a);
}

void SpectroLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.f);
    const float rad = juce::jmin (b.getWidth(), b.getHeight()) / 2.f, c = rad - 3.f, ang = a0 + pos * (a1 - a0);
    auto ctr = b.getCentre();
    juce::Path track, val;
    track.addCentredArc (ctr.x, ctr.y, c, c, 0, a0, a1, true);
    val.addCentredArc (ctr.x, ctr.y, c, c, 0, a0, ang, true);
    g.setColour (juce::Colour (0x1affffff)); g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kAccent);                   g.strokePath (val,   juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    auto cap = juce::Rectangle<float> (ctr.x - rad * 0.55f, ctr.y - rad * 0.55f, rad * 1.1f, rad * 1.1f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a5266), cap.getX(), cap.getY(), juce::Colour (0xff171b25), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap); g.setColour (juce::Colour (0x2effffff)); g.drawEllipse (cap, 1.f);
    g.setColour (juce::Colours::white);
    g.drawLine (ctr.x, ctr.y, ctr.x + std::sin (ang) * rad * 0.45f, ctr.y - std::cos (ang) * rad * 0.45f, 2.f);
}

// ───────────────────────── Content
juce::Colour SpectroContent::heat (float v)
{
    static const float p[] = { 0.f, .12f, .28f, .42f, .55f, .72f, .88f, 1.f };
    static const juce::uint32 c[] = { 0xff000008, 0xff061696, 0xff0a5ae6, 0xff00c8c8, 0xff14d73c, 0xffebeb14, 0xffff9600, 0xffff3200 };
    v = juce::jlimit (0.f, 1.f, v);
    for (int i = 1; i < 8; ++i)
        if (v <= p[i]) return juce::Colour (c[i - 1]).interpolatedWith (juce::Colour (c[i]), (v - p[i - 1]) / (p[i] - p[i - 1]));
    return juce::Colour (c[7]);
}

SpectroContent::SpectroContent (SpectroProcessor& p)
    : proc (p), bridge (p.apvts), vblank (this, [this] { tick(); })
{
    setLookAndFeel (&laf);
    setSize (W, H);
    auto& s = proc.apvts;

    // L / R / LR / M / S / MS  (channel choice param)
    juce::TextButton* chips[] = { &chL, &chR, &chLR, &chM, &chS, &chMS };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = chips[i]; addAndMakeVisible (b);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { if (auto* prm = proc.apvts.getParameter ("channel")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); };
    }
    // Weighting  A / B / B / C
    juce::TextButton* wts[] = { &wA, &wB, &wB2, &wC };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = wts[i]; addAndMakeVisible (b);
        b->onClick = [this, i] { if (auto* prm = proc.apvts.getParameter ("weighting")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); };
    }

    fftSize.addItemList ({ "512", "1024", "2048", "4096", "8192" }, 1);
    windowBox.addItemList ({ "Hann", "Hamming", "Blackman", "Rectangular" }, 1);
    timeBox.addItemList ({ "2 s", "5 s", "10 s", "20 s", "30 s", "60 s" }, 1);
    for (auto* c : { &fftSize, &windowBox, &timeBox }) addAndMakeVisible (c);
    timeA = std::make_unique<CA> (s, "time", timeBox);
    sizeA = std::make_unique<CA> (s, "fftSize", fftSize);
    winA  = std::make_unique<CA> (s, "window", windowBox);

    for (auto* sl : { &gain, &floorS })
    { sl->setSliderStyle (juce::Slider::LinearBar); sl->setTextValueSuffix (" dB"); sl->setColour (juce::Slider::trackColourId, kAccent.withAlpha (0.18f)); addAndMakeVisible (sl); }
    for (auto* sl : { &memHi, &memLo })
    { sl->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); sl->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
      sl->setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true); sl->setTextValueSuffix (" dB"); addAndMakeVisible (sl); }
    gainA = std::make_unique<SA> (s, "gain", gain);   floorA = std::make_unique<SA> (s, "floor", floorS);
    hiA   = std::make_unique<SA> (s, "memHi", memHi); loA    = std::make_unique<SA> (s, "memLo", memLo);

    for (auto* t : { &xAxis, &yAxis, &grid, &read, &xLog }) addAndMakeVisible (t);
    xAxisA = std::make_unique<BA> (s, "showX", xAxis); yAxisA = std::make_unique<BA> (s, "showY", yAxis);
    gridA  = std::make_unique<BA> (s, "showGrid", grid); readA = std::make_unique<BA> (s, "showRead", read);
    xLogA  = std::make_unique<BA> (s, "xLog", xLog);

    addAndMakeVisible (exportBtn); exportBtn.onClick = [this] { exportImage(); };

    aron.setFont (juce::FontOptions (22.f, juce::Font::bold), false, juce::Justification::centredLeft);
    aron.setColour (juce::HyperlinkButton::textColourId, kText);
    aron.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    aron.setTooltip ("Aron H on Instagram");
    addAndMakeVisible (aron);

    for (auto& ln : lane)
    {
        ln.img = juce::Image (juce::Image::ARGB, kCols, kRows, true, juce::SoftwareImageType());
        ln.img.clear (ln.img.getBounds(), juce::Colour (0xff000008));
        ln.target.assign (kCols, -120.f); ln.state = ln.prevState = ln.tmp = ln.accMax = ln.target;
    }
    for (size_t i = 0; i < lut.size(); ++i) lut[i] = heat ((float) i / (float) (lut.size() - 1)).getPixelARGB();
    lastMs = juce::Time::getMillisecondCounterHiRes();

    // ── page switch: SPECTROGRAM | 3D METER (footer)
    mainControls = { &chL, &chR, &chLR, &chM, &chS, &chMS, &wA, &wB, &wB2, &wC, &fftSize, &windowBox, &timeBox,
                     &gain, &floorS, &memHi, &memLo, &xAxis, &yAxis, &grid, &read, &xLog, &exportBtn };
    addChildComponent (bridge);
    bridge.setOpaque (true);
    for (auto* b : { &pageSpec, &pageBridge }) { b->setClickingTogglesState (false); addAndMakeVisible (b); }
    pageSpec.setToggleState (true, juce::dontSendNotification);
    pageSpec.onClick   = [this] { setPage (0); };
    pageBridge.onClick = [this] { setPage (1); };
}

void SpectroContent::setPage (int p)
{
    page = p;
    for (auto* c : mainControls) c->setVisible (p == 0);
    bridge.setVisible (p == 1);
    pageSpec.setToggleState (p == 0, juce::dontSendNotification);
    pageBridge.setToggleState (p == 1, juce::dontSendNotification);
    if (p == 0) { clearHistory(); modeIdx = -1; lastMs = juce::Time::getMillisecondCounterHiRes(); }
    repaint();
}

void SpectroContent::updateLanes (int mode)
{
    if (tapBins < 2 || taps[0].size() < (size_t) tapBins || taps[3].size() < (size_t) tapBins) return;
    auto copy = [&] (Lane& ln, const std::vector<float>& s) { ln.frameDb.assign (s.begin(), s.begin() + tapBins); };
    switch (mode)
    {
        case 0:  copy (lane[0], taps[0]); break;                                   // L
        case 1:  copy (lane[0], taps[1]); break;                                   // R
        case 3:  copy (lane[0], taps[2]); break;                                   // M
        case 4:  copy (lane[0], taps[3]); break;                                   // S
        case 5:  copy (lane[0], taps[2]); copy (lane[1], taps[3]); break;          // MS: mid on top, side below
        default:                                                                   // LR: power average of L and R
            lane[0].frameDb.resize ((size_t) tapBins);
            for (int i = 0; i < tapBins; ++i)
            {
                const float a = std::exp (taps[0][(size_t) i] * 0.230259f), b = std::exp (taps[1][(size_t) i] * 0.230259f);
                lane[0].frameDb[(size_t) i] = 4.3429448f * std::log (0.5f * (a + b) + 1.0e-12f);
            }
            break;
    }
    frameBins = tapBins; frameSr = tapSr;
}

void SpectroContent::resized()
{
    juce::TextButton* chips[] = { &chL, &chR, &chLR, &chM, &chS, &chMS };
    for (int i = 0; i < 6; ++i) chips[i]->setBounds (14, 584 + 34 * i, 34, 28);

    wA.setBounds (178, 618, 34, 28); wB.setBounds (216, 618, 34, 28); wB2.setBounds (254, 618, 34, 28); wC.setBounds (292, 618, 34, 28);
    fftSize.setBounds (140, 720, 100, 30); windowBox.setBounds (330, 720, 140, 30);

    gain.setBounds (640, 594, 120, 28); floorS.setBounds (640, 632, 120, 28);
    memHi.setBounds (560, 700, 80, 70); memLo.setBounds (652, 700, 80, 70);

    xAxis.setBounds (800, 640, 90, 22); yAxis.setBounds (900, 640, 90, 22);
    grid.setBounds (800, 668, 90, 22);  read.setBounds (900, 668, 90, 22);
    xLog.setBounds (812, 700, 86, 26);
    exportBtn.setBounds (906, 700, 94, 26);
    timeBox.setBounds (862, 738, 138, 30);
    aron.setBounds (20, 798, 150, 34);
    bridge.setBounds (0, 0, W, 786);
    pageSpec.setBounds (330, 800, 90, 30); pageBridge.setBounds (426, 800, 90, 30);
}

void SpectroContent::paint (juce::Graphics& g)
{
    // window body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141822), 0, 0, juce::Colour (0xff0d1017), 0, (float) H, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.14f), 150, -40, juce::Colours::transparentBlack, 150, 420, true));
    g.fillRect (0, 0, W, 460);

    // dB scale (top)
    auto drawScale = [&] (juce::Rectangle<int> r)
    {
        g.setColour (kDeep); g.fillRoundedRectangle (r.toFloat(), 8.f);
        auto bar = r.reduced (12, 0).withTop (r.getY() + 6).withHeight (6).toFloat();
        for (int x = 0; x < (int) bar.getWidth(); ++x) { g.setColour (heat (0.04f + 0.96f * x / bar.getWidth())); g.fillRect (bar.getX() + x, bar.getY(), 1.f, bar.getHeight()); }
        g.setColour (kMuted); g.setFont (ui (10.f, false));
        for (int i = 0; i <= 10; ++i) g.drawText (juce::String (100 - i * 10), (int) (bar.getX() + bar.getWidth() * i / 10.f) - 14, r.getY() + 14, 28, 14, juce::Justification::centred);
    };
    drawScale ({ 14, 12, 996, 36 });

    // spectrogram (one lane, or two stacked lanes in MS mode: mid on top, side below)
    g.setColour (juce::Colours::black); g.fillRoundedRectangle (specArea.toFloat(), 8.f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (specArea.toFloat(), 8.f);
        g.reduceClipRegion (clip);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const int sx = juce::jlimit (0, kCols - 8, juce::roundToInt (viewLo * kCols));
        const int sw = juce::jlimit (8, kCols - sx, juce::roundToInt ((viewHi - viewLo) * kCols));
        const int olderRows = kRows - head;
        for (int l = 0; l < (split ? 2 : 1); ++l)
        {
            const auto r = laneRect (l);
            const int olderH = (int) std::round ((double) r.getHeight() * olderRows / kRows);
            if (olderRows > 0 && olderH > 0)
                g.drawImage (lane[l].img, r.getX(), r.getY(), r.getWidth(), olderH, sx, head, sw, olderRows);
            if (head > 0 && r.getHeight() - olderH > 0)
                g.drawImage (lane[l].img, r.getX(), r.getY() + olderH, r.getWidth(), r.getHeight() - olderH, sx, 0, sw, head);
        }
    }
    if (grid.getToggleState())
    {
        g.setColour (juce::Colour (0x12ffffff));
        for (int i = 1; i < 8; ++i)  g.drawHorizontalLine (specArea.getY() + i * specArea.getHeight() / 8, (float) specArea.getX(), (float) specArea.getRight());
    }
    // time axis (Y-AXIS toggle): how far back each row of the spectrogram is
    if (yAxis.getToggleState())
    {
        const float secs = kSeconds[juce::jlimit (0, 5, timeIdx < 0 ? 2 : timeIdx)];
        g.setColour (juce::Colour (0xd0ffffff)); g.setFont (ui (10.f));
        for (int l = 0; l < (split ? 2 : 1); ++l)
        {
            const auto r = laneRect (l);
            for (int k = 0; k < 4; ++k)
            {
                const float v = secs * (1.f - (float) k / 4.f);
                g.drawText ("-" + juce::String (v, v == std::floor (v) ? 0 : 1) + " s", r.getX() + 6, r.getY() + k * r.getHeight() / 4 + (k == 0 ? 3 : -7), 60, 14, juce::Justification::centredLeft);
            }
            g.drawText ("now", r.getX() + 6, r.getBottom() - 16, 60, 14, juce::Justification::centredLeft);
        }
    }
    if (split)   // lane tags
    {
        g.setFont (ui (11.f));
        const char* tags[] = { "MID", "SIDE" };
        for (int l = 0; l < 2; ++l)
        {
            const auto r = laneRect (l);
            g.setColour (juce::Colour (0x99000000)); g.fillRoundedRectangle ((float) r.getRight() - 58.f, (float) r.getY() + 6.f, 52.f, 18.f, 5.f);
            g.setColour (juce::Colours::white);      g.drawText (tags[l], r.getRight() - 58, r.getY() + 6, 52, 18, juce::Justification::centred);
        }
    }
    // frequency axis: positions follow the real axis mapping and the zoom; extra labels appear when zoomed in
    {
        struct Lbl { double f; const char* s; bool major; };
        static const Lbl labels[] = {
            { 15, "15", true }, { 31.25, "31", true }, { 62.5, "63", true }, { 125, "125", true }, { 250, "250", true }, { 500, "500", true },
            { 1000, "1k", true }, { 2000, "2k", true }, { 4000, "4k", true }, { 8000, "8k", true }, { 16000, "16k", true }, { 20000, "20k", true },
            { 20, "20", false }, { 25, "25", false }, { 40, "40", false }, { 50, "50", false }, { 80, "80", false }, { 100, "100", false },
            { 150, "150", false }, { 200, "200", false }, { 300, "300", false }, { 400, "400", false }, { 600, "600", false }, { 800, "800", false },
            { 1500, "1.5k", false }, { 3000, "3k", false }, { 5000, "5k", false }, { 6000, "6k", false }, { 10000, "10k", false }, { 12000, "12k", false } };
        static const Lbl linLabels[] = { { 0, "0", true }, { 2000, "2k", true }, { 4000, "4k", true }, { 6000, "6k", true }, { 8000, "8k", true },
            { 10000, "10k", true }, { 12000, "12k", true }, { 14000, "14k", true }, { 16000, "16k", true }, { 18000, "18k", true }, { 20000, "20k", true },
            { 500, "500", false }, { 1000, "1k", false }, { 1000, "1k", false }, { 3000, "3k", false }, { 5000, "5k", false } };
        const Lbl* set = mapLog ? labels : linLabels;
        const size_t count = mapLog ? sizeof (labels) / sizeof (Lbl) : sizeof (linLabels) / sizeof (Lbl);
        std::vector<float> placed;
        g.setFont (ui (11.f, false));
        for (int pass = 0; pass < 2; ++pass)
            for (size_t li = 0; li < count; ++li)
            {
                const auto& l = set[li];
                if (l.major != (pass == 0)) continue;
                const float x = tToX (freqToT (l.f));
                if (! std::isfinite (x) || x < specArea.getX() + 4 || x > specArea.getRight() - 4) continue;
                bool free = true;
                for (float px : placed) if (std::abs (px - x) < (l.major ? 25.f : 48.f)) { free = false; break; }
                if (! free) continue;
                placed.push_back (x);
                if (grid.getToggleState()) { g.setColour (juce::Colour (0x12ffffff)); g.drawVerticalLine ((int) x, (float) specArea.getY(), (float) specArea.getBottom()); }
                g.setColour (juce::Colour (0xff3a4152)); g.drawVerticalLine ((int) x, (float) specArea.getBottom() - 5, (float) specArea.getBottom());
                g.setColour (kMuted); g.drawText (l.s, (int) x - 20, specArea.getBottom() + 2, 40, 16, juce::Justification::centred);
            }
    }

    // meters (right) – left/right peak, labelled cells
    const int ys[] = { 58, 293 };
    const char* names[] = { "LEFT", "RIGHT" };
    const float pk[] = { meter[0], meter[1] };
    const int dbs[] = { 0, -3, -6, -10, -20, -30, -50, -70, -90, -100 };
    for (int m = 0; m < 2; ++m)
    {
        juce::Rectangle<int> r (934, ys[m], 76, 227);
        g.setColour (kPanel); g.fillRoundedRectangle (r.toFloat(), 10.f);
        g.setColour (kLine);  g.drawRoundedRectangle (r.toFloat(), 10.f, 1.f);
        auto cells = r.reduced (6).withTrimmedBottom (18);
        const float ch = cells.getHeight() / 10.f;
        for (int i = 0; i < 10; ++i)
        {
            auto c = juce::Rectangle<float> ((float) cells.getX(), cells.getY() + i * ch, (float) cells.getWidth(), ch - 2.f);
            const bool lit = pk[m] >= dbs[i];
            g.setColour (heat (1.f - std::abs ((float) dbs[i]) / 105.f).withAlpha (lit ? 1.f : 0.28f));
            g.fillRoundedRectangle (c, 3.f);
            g.setColour (juce::Colour (0xff06080c)); g.setFont (ui (9.f)); g.drawText (juce::String (dbs[i]), c.toNearestInt(), juce::Justification::centred);
        }
        g.setColour (kMuted); g.setFont (ui (10.f)); g.drawText (names[m], r.removeFromBottom (18), juce::Justification::centred);
    }

    // zoom / pan bar: drag the bar or its ends, wheel on the spectrogram zooms, double-click or magnifier resets
    {
        const auto tr = trackRect().toFloat(); const auto th = thumbRect().toFloat();
        const bool zoomed = (viewHi - viewLo) < 0.999f;
        g.setColour (juce::Colour (0x0dffffff)); g.fillRoundedRectangle (tr, 7.f);
        g.setColour (kAccent.withAlpha (zoomed ? 0.55f : 0.30f)); g.fillRoundedRectangle (th, 7.f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        for (float gx : { th.getX() + 5.f, th.getRight() - 5.f }) g.fillRoundedRectangle (gx - 1.f, th.getY() + 3.f, 2.f, th.getHeight() - 6.f, 1.f);
        const juce::Rectangle<float> mg (14.f, 548.f, 30.f, 30.f);
        g.setColour (zoomed ? kAccent.withAlpha (0.35f) : juce::Colour (0x1affffff)); g.fillEllipse (mg);
        g.setColour (kText);
        g.drawEllipse (mg.getX() + 8.f, mg.getY() + 7.f, 11.f, 11.f, 1.8f);
        g.drawLine (mg.getX() + 18.f, mg.getY() + 17.f, mg.getX() + 23.f, mg.getY() + 22.f, 2.f);
    }

    // control cards
    auto card = [&] (juce::Rectangle<int> r, const char* title = nullptr)
    { g.setColour (kPanel); g.fillRoundedRectangle (r.toFloat(), 12.f); g.setColour (kLine); g.drawRoundedRectangle (r.toFloat(), 12.f, 1.f);
      if (title) { g.setColour (kMuted); g.setFont (ui (10.5f)); g.drawText (title, r.getX() + 14, r.getY() + 8, 120, 16, juce::Justification::centredLeft); } };
    card ({ 60, 584, 480, 95 });
    card ({ 60, 687, 480, 95 });
    g.setColour (kMuted); g.setFont (ui (10.5f));
    g.drawText ("WEIGHTING", 74, 617, 100, 28, juce::Justification::centredLeft);
    g.drawText ("FFT",    74, 723, 30, 24, juce::Justification::centredLeft);
    g.drawText ("SIZE",  104, 723, 36, 24, juce::Justification::centredLeft);
    g.drawText ("WINDOW", 250, 723, 70, 24, juce::Justification::centredLeft);
    card ({ 548, 584, 244, 90 });
    g.setColour (kMuted); g.setFont (ui (10.5f));
    g.drawText ("GAIN", 562, 596, 70, 24, juce::Justification::centredLeft);
    g.drawText ("FLOOR", 562, 634, 70, 24, juce::Justification::centredLeft);
    card ({ 548, 682, 244, 100 }, "MEMORY");
    card ({ 800, 584, 210, 100 }, "DISPLAY");
    card ({ 800, 692, 210, 90 });
    g.setColour (kMuted); g.setFont (ui (10.5f)); g.drawText ("TIME", 812, 742, 44, 22, juce::Justification::centredLeft);

    // footer: ARON H (link, small) on the left, logo + SPECTRO (big) + "| spectral analyzer" on the right
    g.setColour (kLine); g.drawHorizontalLine (786, 0, (float) W);
    g.setColour (juce::Colour (0x40000000)); g.fillRect (0, 787, W, H - 787);
    {
        const juce::Font big (juce::FontOptions (34.f, juce::Font::bold).withKerningFactor (0.08f));
        const juce::Font sub (juce::FontOptions (12.f, juce::Font::plain).withKerningFactor (0.14f));
        const juce::String subText ("|  SPECTRAL ANALYZER");
        const int subW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (sub, subText)) + 4;
        const int bigW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (big, "SPECTRO")) + 6;
        const int right = 1010, subX = right - subW, bigX = subX - 10 - bigW;

        g.setColour (kMuted); g.setFont (sub);
        g.drawText (subText, subX, 801, subW, 24, juce::Justification::centredLeft);
        g.setColour (kText); g.setFont (big);
        g.drawText ("SPECTRO", bigX, 792, bigW, 44, juce::Justification::centredLeft);

    }
}

// Log axis: ~13 Hz … ~21 kHz (a log scale cannot contain 0 Hz). Linear axis: 0 Hz … ~21 kHz.
void SpectroContent::axisRange (double sr, bool logX, double& fMin, double& fMax)
{
    fMin = logX ? 15.0 * 0.85 : 0.0;
    fMax = juce::jmin (20000.0 * 1.06, sr * 0.5);
}

void SpectroContent::rebuildColumnMap (int bins, double sr, bool logX)
{
    mapBins = bins; mapSr = sr; mapLog = logX;
    spans.resize ((size_t) kCols);
    const double nyq = sr * 0.5; double fMin, fMax; axisRange (sr, logX, fMin, fMax);
    auto binAt = [&] (double t)
    {
        const double f = logX ? fMin * std::pow (fMax / fMin, t) : fMin + (fMax - fMin) * t;
        return (float) juce::jlimit (0.0, (double) (bins - 1), f / nyq * bins);
    };
    for (int x = 0; x < kCols; ++x)
        spans[(size_t) x] = { binAt ((x - 0.5) / (kCols - 1)), binAt ((x + 0.5) / (kCols - 1)) };
}

// FFT bins → one value per pixel column. Narrow columns (low freqs) interpolate between bins,
// wide columns (high freqs) pool many bins, so nothing looks blocky or aliased.
void SpectroContent::sampleColumns (Lane& ln) const
{
    const float* db = ln.frameDb.data();
    for (int x = 0; x < kCols; ++x)
    {
        const auto s = spans[(size_t) x];
        if (s.hi - s.lo < 1.f)
        {
            const float c = 0.5f * (s.lo + s.hi);
            const int i = (int) c, j = juce::jmin (i + 1, frameBins - 1);
            ln.target[(size_t) x] = db[i] + (db[j] - db[i]) * (c - (float) i);
        }
        else
        {
            const int i0 = (int) std::floor (s.lo), i1 = juce::jmin ((int) std::ceil (s.hi), frameBins - 1);
            double sum = 0.0; float mx = -200.f;
            for (int i = i0; i <= i1; ++i) { sum += std::pow (10.0, db[i] * 0.1); mx = juce::jmax (mx, db[i]); }
            const float mean = (float) (10.0 * std::log10 (sum / (i1 - i0 + 1) + 1.0e-12));
            ln.target[(size_t) x] = 0.5f * (mean + mx);
        }
    }
}

juce::Rectangle<int> SpectroContent::laneRect (int i) const
{
    if (! split) return specArea;
    const int h = (specArea.getHeight() - 2) / 2;
    return i == 0 ? specArea.withHeight (h) : specArea.withTrimmedTop (h + 2);
}

void SpectroContent::clearHistory()
{
    for (auto& ln : lane) ln.img.clear (ln.img.getBounds(), juce::Colour (0xff000008));
    head = 0; rowAcc = 0.0;
}

void SpectroContent::tick()
{
    if (lane[0].target.empty()) return;   // not constructed yet
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.1, (now - lastMs) * 0.001);
    lastMs = now;

    // channel mode / time scale changed → start with a clean history
    const int mode = juce::jlimit (0, 5, (int) proc.apvts.getRawParameterValue ("channel")->load());
    bool modeChanged = false;
    if (mode != modeIdx) { modeIdx = mode; split = (mode == 5); clearHistory(); modeChanged = true; }
    const int tIdx = juce::jlimit (0, 5, (int) proc.apvts.getRawParameterValue ("time")->load());
    if (tIdx != timeIdx) { timeIdx = tIdx; clearHistory(); }
    const int nLanes = split ? 2 : 1;

    // newest spectra from the audio thread (keeps the previous ones if nothing new arrived)
    const bool gotFrame = proc.fetchFrame (taps, tapBins, tapSr);
    idleSec = gotFrame ? 0.f : idleSec + dt;
    const bool live = idleSec < 0.5f;                // host paused / transport stopped: no frames -> treat as silence, let it decay
    bridge.feed (taps, tapBins, tapSr, dt, live);          // keeps the 3D history running even while that page is hidden
    if (page == 1) { bridge.tickRender (dt); return; }
    if (gotFrame || modeChanged) updateLanes (mode);

    const bool logX = proc.apvts.getRawParameterValue ("xLog")->load() > 0.5f;
    if (frameBins > 1 && (frameBins != mapBins || frameSr != mapSr || logX != mapLog))
    {
        if (mapBins >= 0 && (frameSr != mapSr || logX != mapLog)) clearHistory();   // axis changed: old rows no longer fit
        rebuildColumnMap (frameBins, frameSr, logX);
    }

    // temporal smoothing: fast attack, gentle release (time-based, so it looks identical at any refresh rate)
    const float atk = 1.f - std::exp (-dt / 0.018f), rel = 1.f - std::exp (-dt / 0.110f);
    for (int l = 0; l < nLanes; ++l)
    {
        auto& ln = lane[l];
        if (live && frameBins > 1 && (int) ln.frameDb.size() >= frameBins)
        {
            sampleColumns (ln);
            for (int x = 0; x < kCols; ++x)   // light 5-tap smoothing across frequency
            {
                auto at = [&] (int k) { return ln.target[(size_t) juce::jlimit (0, kCols - 1, k)]; };
                ln.tmp[(size_t) x] = (at (x - 2) + 4.f * at (x - 1) + 6.f * at (x) + 4.f * at (x + 1) + at (x + 2)) * (1.f / 16.f);
            }
            ln.target.swap (ln.tmp);
        }
        else std::fill (ln.target.begin(), ln.target.end(), -120.f);

        for (int x = 0; x < kCols; ++x)
        {
            const float s = ln.state[(size_t) x], t = ln.target[(size_t) x];
            ln.state[(size_t) x] = s + (t - s) * (t > s ? atk : rel);
            ln.accMax[(size_t) x] = juce::jmax (ln.accMax[(size_t) x], ln.state[(size_t) x]);   // peak hold for slow TIME settings
        }
    }

    // how many history rows have elapsed since the last refresh
    rowAcc += (double) dt * ((double) kRows / (double) kSeconds[timeIdx]);
    int n = (int) rowAcc; rowAcc -= n;
    n = juce::jmin (n, kRows / 2);

    if (n > 0)
    {
        const float hi = proc.apvts.getRawParameterValue ("memHi")->load();
        const float lo = juce::jmin (proc.apvts.getRawParameterValue ("floor")->load(), proc.apvts.getRawParameterValue ("memLo")->load());
        const float inv = 1023.f / juce::jmax (1.f, hi - lo);

        for (int l = 0; l < nLanes; ++l)
        {
            auto& ln = lane[l];
            juce::Image::BitmapData bd (ln.img, 0, 0, kCols, kRows, juce::Image::BitmapData::writeOnly);
            int h = head;
            for (int k = 1; k <= n; ++k)
            {
                // interpolate between the previous and current state so rows never repeat → no stair-stepping
                const float a = (float) k / (float) n;
                auto* px = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (h));
                for (int x = 0; x < kCols; ++x)
                {
                    float db = ln.prevState[(size_t) x] + (ln.state[(size_t) x] - ln.prevState[(size_t) x]) * a;
                    if (k == n) db = juce::jmax (db, ln.accMax[(size_t) x]);   // keep short peaks
                    px[x] = lut[(size_t) juce::jlimit (0, 1023, (int) ((db - lo) * inv))];
                }
                h = (h + 1) % kRows;
            }
        }
        head = (head + n) % kRows;
    }
    for (int l = 0; l < nLanes; ++l)
    {
        if (n > 0) lane[l].accMax = lane[l].state;
        lane[l].prevState = lane[l].state;
    }

    // meters: instant attack, smooth fall
    const float pk[2] = { live ? proc.peakL.load() : -100.f, live ? proc.peakR.load() : -100.f };
    for (int i = 0; i < 2; ++i) meter[i] = juce::jmax (pk[i], meter[i] - 45.f * dt);

    // chip states follow the parameters
    const int wt = (int) proc.apvts.getRawParameterValue ("weighting")->load();
    juce::TextButton* chips[] = { &chL, &chR, &chLR, &chM, &chS, &chMS };
    juce::TextButton* wts[]   = { &wA, &wB, &wB2, &wC };
    for (int i = 0; i < 6; ++i) chips[i]->setToggleState (i == mode, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i) wts[i]->setToggleState (i == wt, juce::dontSendNotification);

    repaint (specArea);
    repaint (934, 58, 76, 462);
}

// ───────── frequency zoom / pan
juce::Rectangle<int> SpectroContent::thumbRect() const
{
    const auto t = trackRect();
    const int x0 = t.getX() + juce::roundToInt (viewLo * (float) t.getWidth());
    const int x1 = t.getX() + juce::roundToInt (viewHi * (float) t.getWidth());
    return { x0, t.getY(), juce::jmax (18, x1 - x0), t.getHeight() };
}

void SpectroContent::setView (float lo, float hi)
{
    const float w = juce::jlimit (kMinView, 1.f, hi - lo);
    lo = juce::jlimit (0.f, 1.f - w, lo);
    viewLo = lo; viewHi = lo + w;
    repaint();
}

float SpectroContent::tToX (float t) const
{
    return (float) specArea.getX() + (t - viewLo) / (viewHi - viewLo) * (float) specArea.getWidth();
}

float SpectroContent::freqToT (double f) const
{
    double fMin, fMax; axisRange (mapSr > 0.0 ? mapSr : 44100.0, mapLog, fMin, fMax);
    return (float) (mapLog ? std::log (f / fMin) / std::log (fMax / fMin) : (f - fMin) / (fMax - fMin));
}

void SpectroContent::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (juce::Rectangle<int> (14, 548, 30, 30).contains (p)) { setView (0.f, 1.f); return; }   // magnifier = reset
    if (! trackRect().expanded (0, 8).contains (p)) return;

    const auto th = thumbRect();
    dragLo = viewLo; dragHi = viewHi; dragX = p.x;
    if (std::abs (p.x - th.getX()) <= 7)          drag = Drag::left;
    else if (std::abs (p.x - th.getRight()) <= 7) drag = Drag::right;
    else
    {
        drag = Drag::pan;
        if (! th.contains (p))   // click on the empty track: centre the view there
        {
            const float c = (float) (p.x - trackRect().getX()) / (float) trackRect().getWidth(), w = viewHi - viewLo;
            setView (c - 0.5f * w, c + 0.5f * w);
            dragLo = viewLo; dragHi = viewHi;
        }
    }
}

void SpectroContent::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::none) return;
    const float d = (float) (e.x - dragX) / (float) trackRect().getWidth();
    switch (drag)
    {
        case Drag::pan:   setView (dragLo + d, dragHi + d); break;
        case Drag::left:  setView (juce::jlimit (0.f, dragHi - kMinView, dragLo + d), dragHi); break;
        case Drag::right: setView (dragLo, juce::jlimit (dragLo + kMinView, 1.f, dragHi + d)); break;
        default: break;
    }
}

void SpectroContent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (specArea.contains (e.getPosition()) || trackRect().expanded (0, 8).contains (e.getPosition())) setView (0.f, 1.f);
}

void SpectroContent::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    auto cursor = juce::MouseCursor::NormalCursor;
    if (juce::Rectangle<int> (14, 548, 30, 30).contains (p)) cursor = juce::MouseCursor::PointingHandCursor;
    else if (trackRect().expanded (0, 8).contains (p))
    {
        const auto th = thumbRect();
        cursor = (std::abs (p.x - th.getX()) <= 7 || std::abs (p.x - th.getRight()) <= 7) ? juce::MouseCursor::LeftRightResizeCursor
               : th.contains (p) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor;
    }
    setMouseCursor (cursor);
}

void SpectroContent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float w = viewHi - viewLo;
    if (specArea.contains (e.getPosition()))
    {
        if (std::abs (wheel.deltaX) > std::abs (wheel.deltaY))       // sideways scroll pans
            setView (viewLo - wheel.deltaX * w, viewHi - wheel.deltaX * w);
        else                                                          // wheel zooms around the cursor
        {
            const float frac = (float) (e.x - specArea.getX()) / (float) specArea.getWidth();
            const float at = viewLo + frac * w;
            const float nw = juce::jlimit (kMinView, 1.f, w * std::exp (-wheel.deltaY * 1.5f));
            setView (at - frac * nw, at - frac * nw + nw);
        }
    }
    else if (trackRect().expanded (0, 8).contains (e.getPosition()))
        setView (viewLo - wheel.deltaY * w, viewHi - wheel.deltaY * w);
    else
        Component::mouseWheelMove (e, wheel);
}

juce::Image SpectroContent::orderedSpec() const
{
    const int nLanes = split ? 2 : 1;
    juce::Image out (juce::Image::ARGB, kCols, kRows * nLanes, true, juce::SoftwareImageType());
    juce::Graphics g (out);
    for (int l = 0; l < nLanes; ++l)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (0, l * kRows, kCols, kRows);
        g.drawImageAt (lane[l].img, 0, l * kRows - head);               // rows head..end → top
        g.drawImageAt (lane[l].img, 0, l * kRows + kRows - head);       // rows 0..head   → bottom (newest)
    }
    return out;
}

void SpectroContent::exportImage()
{
    chooser = std::make_unique<juce::FileChooser> ("Export spectrogram", juce::File::getSpecialLocation (juce::File::userPicturesDirectory).getChildFile ("Spectro.png"), "*.png");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult(); if (f == juce::File()) return;
        juce::PNGImageFormat png; juce::FileOutputStream os (f.withFileExtension ("png"));
        if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (orderedSpec(), os); }
    });
}

// ───────────────────────── Editor (resizable)
SpectroEditor::SpectroEditor (SpectroProcessor& p) : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    limits.setMinimumSize (640, 525);
    limits.setMaximumSize (2048, 1680);
    limits.setFixedAspectRatio ((double) SpectroContent::W / SpectroContent::H);
    setConstrainer (&limits);
    setResizable (true, true);                 // drag corner / host resize
    // start 15 % smaller than the 1024x840 design size (still freely resizable)
    setSize (juce::roundToInt (SpectroContent::W * 0.85f), juce::roundToInt (SpectroContent::H * 0.85f));
}

void SpectroEditor::resized()
{
    const float s = (float) getWidth() / SpectroContent::W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, SpectroContent::W, SpectroContent::H);
}
