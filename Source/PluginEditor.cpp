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
    g.setColour (on ? juce::Colours::white : (over ? kText : kMuted));
    g.setFont (ui (12.f)); g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void SpectroLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    if (b.getButtonText().startsWith ("X: "))  // pill-style toggle
    { drawButtonBackground (g, b, {}, over, false); return; }
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

SpectroContent::SpectroContent (SpectroProcessor& p) : proc (p)
{
    setLookAndFeel (&laf);
    setSize (W, H);
    auto& s = proc.apvts;

    // L / R / LR / MIS  (channel choice param)
    juce::TextButton* chips[] = { &chL, &chR, &chLR, &chMIS };
    for (int i = 0; i < 4; ++i)
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
    for (auto* c : { &fftSize, &windowBox }) addAndMakeVisible (c);
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

    aron.setFont (juce::FontOptions (28.f, juce::Font::bold), false, juce::Justification::centredLeft);
    aron.setColour (juce::HyperlinkButton::textColourId, kText);
    aron.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    aron.setTooltip ("Aron H on Instagram");
    addAndMakeVisible (aron);

    spec.clear (spec.getBounds(), juce::Colour (0xff000008));
    startTimerHz (30);
}

void SpectroContent::resized()
{
    chL.setBounds (14, 640, 34, 28);  chR.setBounds (14, 674, 34, 28);
    chLR.setBounds (14, 708, 34, 28); chMIS.setBounds (14, 742, 34, 28);

    wA.setBounds (178, 668, 34, 28); wB.setBounds (216, 668, 34, 28); wB2.setBounds (254, 668, 34, 28); wC.setBounds (292, 668, 34, 28);
    fftSize.setBounds (130, 716, 110, 30); windowBox.setBounds (330, 716, 140, 30);

    gain.setBounds (640, 594, 120, 28); floorS.setBounds (640, 632, 120, 28);
    memHi.setBounds (560, 700, 80, 70); memLo.setBounds (652, 700, 80, 70);

    xAxis.setBounds (800, 640, 90, 22); yAxis.setBounds (900, 640, 90, 22);
    grid.setBounds (800, 668, 90, 22);  read.setBounds (900, 668, 90, 22);
    xLog.setBounds (800, 716, 80, 28);
    exportBtn.setBounds (890, 750, 100, 28);
    aron.setBounds (20, 788, 170, 40);
}

void SpectroContent::paint (juce::Graphics& g)
{
    // window body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141822), 0, 0, juce::Colour (0xff0d1017), 0, (float) H, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.14f), 150, -40, juce::Colours::transparentBlack, 150, 420, true));
    g.fillRect (0, 0, W, 460);

    // traffic lights + title
    const juce::uint32 tl[] = { 0xffff5f57, 0xfffebc2e, 0xff28c840 };
    for (int i = 0; i < 3; ++i) { g.setColour (juce::Colour (tl[i])); g.fillEllipse (14.f + i * 20.f, 17.f, 12.f, 12.f); }
    g.setColour (kText); g.setFont (ui (12.f)); g.drawText ("SPECTRO", 82, 12, 60, 22, juce::Justification::centred);
    g.setColour (kLine); g.drawHorizontalLine (46, 0, (float) W); g.drawHorizontalLine (84, 0, (float) W);
    g.setColour (kText); g.setFont (ui (14.f)); g.drawText ("SPECTRO", 16, 52, 90, 28, juce::Justification::centredLeft);
    g.setColour (kMuted); g.drawText ("|  SPECTRAL ANALYSIS", 100, 52, 260, 28, juce::Justification::centredLeft);

    // dB scale (top)
    auto drawScale = [&] (juce::Rectangle<int> r)
    {
        g.setColour (kDeep); g.fillRoundedRectangle (r.toFloat(), 8.f);
        auto bar = r.reduced (12, 0).withTop (r.getY() + 6).withHeight (6).toFloat();
        for (int x = 0; x < (int) bar.getWidth(); ++x) { g.setColour (heat (0.04f + 0.96f * x / bar.getWidth())); g.fillRect (bar.getX() + x, bar.getY(), 1.f, bar.getHeight()); }
        g.setColour (kMuted); g.setFont (ui (10.f, false));
        for (int i = 0; i <= 10; ++i) g.drawText (juce::String (100 - i * 10), (int) (bar.getX() + bar.getWidth() * i / 10.f) - 14, r.getY() + 14, 28, 14, juce::Justification::centred);
    };
    drawScale ({ 14, 94, 996, 36 });

    // spectrogram
    g.setColour (juce::Colours::black); g.fillRoundedRectangle (specArea.toFloat(), 8.f);
    g.drawImage (spec, specArea.toFloat().reduced (0.f, 0.f).withTrimmedBottom (0));
    if (grid.getToggleState())
    {
        g.setColour (juce::Colour (0x12ffffff));
        for (int i = 0; i < 11; ++i) g.drawVerticalLine (specArea.getX() + (int) ((0.058f + i * 0.0902f) * specArea.getWidth()), (float) specArea.getY(), (float) specArea.getBottom());
        for (int i = 1; i < 8; ++i)  g.drawHorizontalLine (specArea.getY() + i * specArea.getHeight() / 8, (float) specArea.getX(), (float) specArea.getRight());
    }
    // frequency axis
    g.setColour (kMuted); g.setFont (ui (11.f, false));
    const char* fl[] = { "15", "31", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    for (int i = 0; i < 11; ++i)
    {
        const int x = specArea.getX() + (int) ((0.058f + i * 0.0902f) * specArea.getWidth());
        g.drawText (fl[i], x - 20, specArea.getBottom() + 2, 40, 16, juce::Justification::centred);
        g.setColour (juce::Colour (0xff3a4152)); g.drawVerticalLine (x, (float) specArea.getBottom() - 5, (float) specArea.getBottom()); g.setColour (kMuted);
    }

    // meters (right) – left/right peak, labelled cells
    const int ys[] = { 144, 336 };
    const char* names[] = { "LEFT", "RIGHT" };
    const float pk[] = { proc.peakL.load(), proc.peakR.load() };
    const int dbs[] = { 0, -3, -6, -10, -20, -30, -50, -70, -90, -100 };
    for (int m = 0; m < 2; ++m)
    {
        juce::Rectangle<int> r (934, ys[m], 76, 184);
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

    // scrub bar
    g.setColour (juce::Colour (0x0dffffff)); g.fillRoundedRectangle (48.f, 556.f, 962.f, 14.f, 7.f);
    g.setColour (kAccent.withAlpha (0.3f));  g.fillRoundedRectangle (48.f, 556.f, 290.f, 14.f, 7.f);
    g.setColour (juce::Colour (0x1affffff)); g.fillEllipse (14.f, 548.f, 30.f, 30.f);

    // control cards
    auto card = [&] (juce::Rectangle<int> r, const char* title = nullptr)
    { g.setColour (kPanel); g.fillRoundedRectangle (r.toFloat(), 12.f); g.setColour (kLine); g.drawRoundedRectangle (r.toFloat(), 12.f, 1.f);
      if (title) { g.setColour (kMuted); g.setFont (ui (10.5f)); g.drawText (title, r.getX() + 14, r.getY() + 8, 120, 16, juce::Justification::centredLeft); } };
    card ({ 60, 584, 480, 64 });                 drawScale ({ 70, 590, 460, 52 });
    card ({ 60, 656, 480, 52 }, "WEIGHTING");
    card ({ 60, 712, 480, 52 }, nullptr);
    g.setColour (kMuted); g.setFont (ui (10.5f)); g.drawText ("FFT", 74, 726, 40, 24, juce::Justification::centredLeft);
    g.drawText ("SIZE", 90, 726, 40, 24, juce::Justification::centredLeft);
    g.drawText ("WINDOW", 250, 726, 70, 24, juce::Justification::centredLeft);
    card ({ 548, 584, 244, 90 });
    g.setColour (kMuted); g.setFont (ui (10.5f));
    g.drawText ("GAIN", 562, 596, 70, 24, juce::Justification::centredLeft);
    g.drawText ("FLOOR", 562, 634, 70, 24, juce::Justification::centredLeft);
    card ({ 548, 682, 244, 100 }, "MEMORY");
    card ({ 800, 584, 210, 100 }, "DISPLAY");
    card ({ 800, 700, 210, 40 });
}

void SpectroContent::timerCallback()
{
    std::vector<float> db; int bins = 0; double sr = 44100.0;
    bool any = false;
    while (proc.fetchFrame (db, bins, sr)) { pushRow (db, bins, sr); any = true; }
    juce::ignoreUnused (any);

    // reflect channel / weighting params on the chip buttons
    const int ch = (int) proc.apvts.getRawParameterValue ("channel")->load();
    const int wt = (int) proc.apvts.getRawParameterValue ("weighting")->load();
    juce::TextButton* chips[] = { &chL, &chR, &chLR, &chMIS };
    juce::TextButton* wts[]   = { &wA, &wB, &wB2, &wC };
    for (int i = 0; i < 4; ++i) { chips[i]->setToggleState (i == ch, juce::dontSendNotification);
                                  wts[i]->setToggleState (i == wt, juce::dontSendNotification); }
    repaint();
}

void SpectroContent::pushRow (const std::vector<float>& db, int bins, double sr)
{
    // scroll image up by one row, draw the new row at the bottom
    const int w = spec.getWidth(), h = spec.getHeight();
    juce::Image next (juce::Image::ARGB, w, h, true);
    { juce::Graphics g (next); g.drawImageAt (spec, 0, -1); }
    spec = next;

    const float hi = proc.apvts.getRawParameterValue ("memHi")->load();
    const float floorDb = proc.apvts.getRawParameterValue ("floor")->load();
    const float memLoDb = proc.apvts.getRawParameterValue ("memLo")->load();
    const float lo = juce::jmin (floorDb, memLoDb);
    const bool logX = proc.apvts.getRawParameterValue ("xLog")->load() > 0.5f;
    const double nyq = sr * 0.5, fMin = 15.0 * 0.85, fMax = 16000.0 * 1.12;

    juce::Image::BitmapData bd (spec, juce::Image::BitmapData::writeOnly);
    for (int x = 0; x < w; ++x)
    {
        const double t = (double) x / (w - 1);
        const double f = logX ? fMin * std::pow (fMax / fMin, t) : fMin + (fMax - fMin) * t;
        const int bin = juce::jlimit (0, bins - 1, (int) std::round (f / nyq * bins));
        const float v = (db[(size_t) bin] - lo) / juce::jmax (1.f, hi - lo);
        bd.setPixelColour (x, h - 1, heat (v));
    }
}

void SpectroContent::exportImage()
{
    chooser = std::make_unique<juce::FileChooser> ("Export spectrogram", juce::File::getSpecialLocation (juce::File::userPicturesDirectory).getChildFile ("Spectro.png"), "*.png");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult(); if (f == juce::File()) return;
        juce::PNGImageFormat png; juce::FileOutputStream os (f.withFileExtension ("png"));
        if (os.openedOk()) { os.setPosition (0); os.truncate(); png.writeImageToStream (spec, os); }
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
    setSize (SpectroContent::W, SpectroContent::H);
}

void SpectroEditor::resized()
{
    const float s = (float) getWidth() / SpectroContent::W;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, SpectroContent::W, SpectroContent::H);
}
