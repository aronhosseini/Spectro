#include "Bridge.h"
#include <cmath>

namespace
{
    const juce::Colour kAccent (0xff4da3ff), kText (0xffe8ecf4), kMuted (0xff8791a5), kPanel (0xb8181c26), kLine (0x1fffffff);
    juce::Font ui (float h, bool bold = true) { return juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain); }

    constexpr float kDepth = 1.6f;
    const char* const kSrcNames[5] = { "Left", "Right", "Left + Right", "Mid", "Side" };
    const juce::uint32 kDefaultColours[8] = { 0xff2f9e63, 0xffa8a840, 0xffb87a35, 0xffa84444, 0xff4458b0, 0xff3a7fb0, 0xff2f9e9e, 0xff2fa07a };

    struct ViewPreset { float yaw, pitch; };
    const ViewPreset kViews[5] = { { 0.f, 36.f }, { 0.f, 12.f }, { 38.f, 28.f }, { 75.f, 32.f }, { 75.f, 11.f } };

    // colour picker used by the tap swatches
    struct Picker : juce::ColourSelector, private juce::ChangeListener
    {
        std::function<void (juce::Colour)> onPick;
        explicit Picker (juce::Colour c)
            : juce::ColourSelector (juce::ColourSelector::showColourAtTop | juce::ColourSelector::showSliders | juce::ColourSelector::showColourspace)
        { setCurrentColour (c); setSize (250, 290); addChangeListener (this); }
        ~Picker() override { removeChangeListener (this); }
        void changeListenerCallback (juce::ChangeBroadcaster*) override { if (onPick) onPick (getCurrentColour()); }
    };
}

struct MeterBridge::Swatch : juce::Component
{
    juce::Colour colour;
    std::function<void (juce::Colour)> onChange;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colour); g.fillRoundedRectangle (r, 6.f);
        g.setColour (juce::Colour (0x55ffffff)); g.drawRoundedRectangle (r, 6.f, 1.f);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! contains (e.getPosition())) return;
        auto picker = std::make_unique<Picker> (colour);
        juce::Component::SafePointer<Swatch> self (this);
        picker->onPick = [self] (juce::Colour c) { if (self != nullptr) { self->colour = c; self->repaint(); if (self->onChange) self->onChange (c); } };
        juce::CallOutBox::launchAsynchronously (std::move (picker), getScreenBounds(), nullptr);
    }
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
};

// ═════════════════════════════════════════════════════════════ construction
MeterBridge::MeterBridge (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    setOpaque (true);
    for (int i = 0; i < kSrc; ++i)
    {
        hist[(size_t) i].assign ((size_t) kBR * kBC, -120.f);
        for (auto* v : { &state[(size_t) i], &prevS[(size_t) i], &accMax[(size_t) i], &target[(size_t) i] }) v->assign ((size_t) kBC, -120.f);
    }
    tmpCols.assign ((size_t) kBC, 0.f);

    // ── mode / view / freeze

    const char* v3n[5] = { "Front High", "Front Low", "Diagonal", "Side High", "Side Low" };
    for (int i = 0; i < 5; ++i)
    {
        view3[(size_t) i].setButtonText (v3n[i]); addAndMakeVisible (view3[(size_t) i]);
        view3[(size_t) i].onClick = [this, i] { applyPreset (i); };
    }
    const char* v2n[3] = { "Overlay", "Tile", "Stack" };
    for (int i = 0; i < 3; ++i)
    {
        view2[(size_t) i].setButtonText (v2n[i]); addAndMakeVisible (view2[(size_t) i]);
        view2[(size_t) i].onClick = [this, i] { seti ("b_view2", i); refreshButtons(); };
    }
    freezeBtn.setClickingTogglesState (true);
    freezeBtn.onClick = [this] { frozen = freezeBtn.getToggleState(); };
    addAndMakeVisible (freezeBtn);

    // ── tabs
    for (auto* b : { &tabOptions, &tabTaps, &tabReset }) { b->setClickingTogglesState (false); addAndMakeVisible (b); }
    tabReset.onClick = [this] { resetAll(); };
    tabOptions.onClick = [this] { seti ("b_tab", 0); refreshTabs(); };
    tabTaps.onClick    = [this] { seti ("b_tab", 1); refreshTabs(); };

    // ── options
    cMouse.addItemList ({ "L: Rotate / R: Pan", "L: Pan / R: Rotate" }, 1);
    cPreset.addItemList ({ "Tap Colors", "Height Gradient", "Ice", "Mono" }, 1);
    cFFT.addItemList ({ "512", "1024", "2048", "4096", "8192" }, 1);
    cOverlap.addItemList ({ "50 %", "75 %", "87.5 %" }, 1);
    cLOD.addItemList ({ "Low", "Medium", "High" }, 1);
    cScroll.addItemList ({ "2 s", "5 s", "10 s", "20 s", "30 s", "Max Info Density" }, 1);
    // one-time migration: earlier builds saved a dim preset (Tap Colors / Ice) that would override the new default
    if (geti ("b_presetver", 0) < 2) { seti ("b_preset", 1); seti ("b_presetver", 2); }
    cMouse.setSelectedId (geti ("b_mouse", 0) + 1, juce::dontSendNotification);
    cPreset.setSelectedId (geti ("b_preset", 1) + 1, juce::dontSendNotification);
    cLOD.setSelectedId (geti ("b_lod", 1) + 1, juce::dontSendNotification);
    cScroll.setSelectedId (geti ("b_scroll", 2) + 1, juce::dontSendNotification);
    cMouse.onChange  = [this] { seti ("b_mouse",  cMouse.getSelectedId()  - 1); };
    cPreset.onChange = [this] { seti ("b_preset", cPreset.getSelectedId() - 1); };
    cLOD.onChange    = [this] { seti ("b_lod",    cLOD.getSelectedId()    - 1); };
    cScroll.onChange = [this] { seti ("b_scroll", cScroll.getSelectedId() - 1); };
    for (auto* c : { &cMouse, &cPreset, &cFFT, &cOverlap, &cLOD, &cScroll }) addAndMakeVisible (c);
    fftA = std::make_unique<CA> (apvts, "fftSize", cFFT);
    ovA  = std::make_unique<CA> (apvts, "overlap", cOverlap);

    auto setupSlider = [this] (juce::Slider& sl, double lo, double hi, double step, double def, const char* key, const char* suffix)
    {
        sl.setSliderStyle (juce::Slider::LinearBar);
        sl.setRange (lo, hi, step);
        sl.setNumDecimalPlacesToDisplay (step < 0.1 ? 2 : 1);
        sl.setTextValueSuffix (suffix);
        sl.setColour (juce::Slider::trackColourId, kAccent.withAlpha (0.18f));
        sl.setValue ((double) getf (key, (float) def), juce::dontSendNotification);
        addAndMakeVisible (sl);
    };
    setupSlider (sZoom, 0.4, 3.0, 0.01, 1.3, "b_zoom", " x");
    setupSlider (sHeight, 0.2, 2.5, 0.01, 1.0, "b_height", " x");
    setupSlider (sHist, 2.0, 30.0, 0.5, 10.0, "b_hist", " s");
    setupSlider (sVis, 0.0, 100.0, 1.0, 100.0, "b_vis", " %");
    sVis.setNumDecimalPlacesToDisplay (0);
    sVis.onValueChange = [this] { setf ("b_vis", (float) sVis.getValue()); repaint (canvas); };
    sZoom.onValueChange   = [this] { setf ("b_zoom", (float) sZoom.getValue()); cam.zoom = camTarget.zoom = (float) sZoom.getValue(); };
    sHeight.onValueChange = [this] { setf ("b_height", (float) sHeight.getValue()); };
    sHist.onValueChange   = [this] { setf ("b_hist", (float) sHist.getValue()); };

    // ── meter taps
    loadSlots();
    for (int i = 0; i < kSlots; ++i)
    {
        auto& c = tapSrc[(size_t) i];
        c.addItem (juce::String::fromUTF8 ("\xe2\x80\x94"), 1);
        for (int k = 0; k < kSrc; ++k) c.addItem (kSrcNames[k], k + 2);
        c.setSelectedId (slotSrc[(size_t) i] + 2, juce::dontSendNotification);
        c.onChange = [this, i] { slotSrc[(size_t) i] = tapSrc[(size_t) i].getSelectedId() - 2; storeSlot (i); };
        addAndMakeVisible (c);

        tapShow[(size_t) i].setToggleState (slotShow[(size_t) i], juce::dontSendNotification);
        tapShow[(size_t) i].onClick = [this, i] { slotShow[(size_t) i] = tapShow[(size_t) i].getToggleState(); storeSlot (i); };
        tapFocus[(size_t) i].setToggleState (slotFocus[(size_t) i], juce::dontSendNotification);
        tapFocus[(size_t) i].onClick = [this, i] { slotFocus[(size_t) i] = tapFocus[(size_t) i].getToggleState(); storeSlot (i); };
        addAndMakeVisible (tapShow[(size_t) i]); addAndMakeVisible (tapFocus[(size_t) i]);

        tapCol[(size_t) i] = std::make_unique<Swatch>();
        tapCol[(size_t) i]->colour = slotCol[(size_t) i];
        tapCol[(size_t) i]->onChange = [this, i] (juce::Colour c) { slotCol[(size_t) i] = c; storeSlot (i); };
        addAndMakeVisible (*tapCol[(size_t) i]);
    }

    cam.zoom = camTarget.zoom = getf ("b_zoom", 1.3f);
    applyPreset (juce::jlimit (0, 4, geti ("b_view3", 0)));
    cam = camTarget;
    refreshButtons(); refreshTabs();
}

MeterBridge::~MeterBridge() = default;

void MeterBridge::loadSlots()
{
    for (int i = 0; i < kSlots; ++i)
    {
        const auto k = "tap" + juce::String (i);
        slotSrc[(size_t) i]   = geti (k + "_src", i == 0 ? 2 : -1);      // default: Left + Right on the first tap
        slotShow[(size_t) i]  = geti (k + "_show", i == 0 ? 1 : 0) != 0;
        slotFocus[(size_t) i] = geti (k + "_focus", 0) != 0;
        const auto saved = apvts.state.getProperty (k + "_col", juce::var()).toString();
        slotCol[(size_t) i] = saved.isNotEmpty() ? juce::Colour::fromString (saved) : juce::Colour (kDefaultColours[i]);
    }
}

void MeterBridge::storeSlot (int i)
{
    const auto k = "tap" + juce::String (i);
    seti (k + "_src", slotSrc[(size_t) i]);
    seti (k + "_show", slotShow[(size_t) i] ? 1 : 0);
    seti (k + "_focus", slotFocus[(size_t) i] ? 1 : 0);
    apvts.state.setProperty (k + "_col", slotCol[(size_t) i].toString(), nullptr);
}

void MeterBridge::applyPreset (int idx)
{
    idx = juce::jlimit (0, 4, idx);
    camTarget.yaw = kViews[idx].yaw; camTarget.pitch = kViews[idx].pitch;
    camTarget.panX = camTarget.panY = 0.f;
    seti ("b_view3", idx);
    refreshButtons();
}

void MeterBridge::resetAll()
{
    frozen = false; freezeBtn.setToggleState (false, juce::dontSendNotification);

    // options (FFT size / overlap are plugin-wide settings shared with the 2D page, so they are left alone)
    cMouse.setSelectedId (1, juce::sendNotificationSync);
    cPreset.setSelectedId (2, juce::sendNotificationSync);   // Height Gradient
    cLOD.setSelectedId (2, juce::sendNotificationSync);      // Medium
    cScroll.setSelectedId (3, juce::sendNotificationSync);   // 10 s
    sHeight.setValue (1.0, juce::sendNotificationSync);
    sHist.setValue (10.0, juce::sendNotificationSync);
    sVis.setValue (100.0, juce::sendNotificationSync);
    sZoom.setValue (1.3, juce::sendNotificationSync);

    // meter taps back to the default: first tap = Left + Right shown, everything else off, default colours
    for (int i = 0; i < kSlots; ++i)
    {
        slotSrc[(size_t) i] = i == 0 ? 2 : -1; slotShow[(size_t) i] = i == 0; slotFocus[(size_t) i] = false;
        slotCol[(size_t) i] = juce::Colour (kDefaultColours[i]);
        tapSrc[(size_t) i].setSelectedId (slotSrc[(size_t) i] + 2, juce::dontSendNotification);
        tapShow[(size_t) i].setToggleState (slotShow[(size_t) i], juce::dontSendNotification);
        tapFocus[(size_t) i].setToggleState (false, juce::dontSendNotification);
        tapCol[(size_t) i]->colour = slotCol[(size_t) i]; tapCol[(size_t) i]->repaint();
        storeSlot (i);
    }

    // camera, view, history and display range
    camTarget.zoom = 1.3f;
    applyPreset (0);
    cam = camTarget;
    autoHi = -30.f; autoLo = -90.f;
    hoverId = 0;
    clearHistory();
    refreshButtons(); refreshTabs();
    repaint();
}

void MeterBridge::refreshButtons()
{
    const bool d3 = is3D();
    b2D.setToggleState (! d3, juce::dontSendNotification);
    b3D.setToggleState (d3, juce::dontSendNotification);
    const int v3 = geti ("b_view3", 0), v2 = geti ("b_view2", 0);
    for (int i = 0; i < 5; ++i) { view3[(size_t) i].setVisible (d3); view3[(size_t) i].setToggleState (i == v3, juce::dontSendNotification); }
    for (int i = 0; i < 3; ++i) { view2[(size_t) i].setVisible (! d3); view2[(size_t) i].setToggleState (i == v2, juce::dontSendNotification); }
    for (auto* c : std::initializer_list<juce::Component*> { &cMouse, &cLOD, &sZoom, &sHeight, &sHist }) c->setAlpha (d3 ? 1.f : 0.4f);
    cScroll.setAlpha (d3 ? 0.4f : 1.f);
    repaint();
}

void MeterBridge::refreshTabs()
{
    const bool opt = geti ("b_tab", 0) == 0;
    tabOptions.setToggleState (opt, juce::dontSendNotification);
    tabTaps.setToggleState (! opt, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &cMouse, &cPreset, &cFFT, &cOverlap, &cLOD, &cScroll, &sZoom, &sHeight, &sHist }) c->setVisible (opt);
    for (int i = 0; i < kSlots; ++i)
    { tapSrc[(size_t) i].setVisible (! opt); tapShow[(size_t) i].setVisible (! opt); tapFocus[(size_t) i].setVisible (! opt); tapCol[(size_t) i]->setVisible (! opt); }
    repaint();
}

// ═════════════════════════════════════════════════════════════ layout
void MeterBridge::resized()
{
    const int w3[5] = { 100, 94, 84, 92, 86 };
    int x = 70;
    for (int i = 0; i < 5; ++i) { view3[(size_t) i].setBounds (x, 14, w3[i], 28); x += w3[i] + 6; }
    const int w2[3] = { 84, 64, 70 };
    x = 70;
    for (int i = 0; i < 3; ++i) { view2[(size_t) i].setBounds (x, 14, w2[i], 28); x += w2[i] + 6; }
    freezeBtn.setBounds (918, 14, 92, 28);
    sVis.setBounds (812, 14, 96, 28);

    tabOptions.setBounds (14, 750, 110, 28); tabTaps.setBounds (130, 750, 124, 28); tabReset.setBounds (260, 750, 84, 28);

    const int colX[3] = { 30, 360, 690 }, rowY[3] = { 598, 642, 686 };
    auto place = [&] (juce::Component& c, int col, int row) { c.setBounds (colX[col] + 106, rowY[row], 190, 30); };
    place (cMouse, 0, 0);   place (cPreset, 0, 1);  place (cFFT, 0, 2);
    place (cOverlap, 1, 0); place (cLOD, 1, 1);     place (cScroll, 1, 2);
    place (sZoom, 2, 0);    place (sHeight, 2, 1);  place (sHist, 2, 2);

    for (int i = 0; i < kSlots; ++i)
    {
        const int x0 = i < 4 ? 30 : 520, y = 608 + (i % 4) * 32;
        tapSrc[(size_t) i].setBounds (x0, y, 190, 28);
        tapShow[(size_t) i].setBounds (x0 + 206, y, 28, 28);
        tapFocus[(size_t) i].setBounds (x0 + 262, y, 28, 28);
        tapCol[(size_t) i]->setBounds (x0 + 318, y + 2, 80, 24);
    }
}

// ═════════════════════════════════════════════════════════════ data
void MeterBridge::axisRange (double sr, bool logX, double& fMin, double& fMax)
{
    fMin = logX ? 15.0 * 0.85 : 0.0;
    fMax = juce::jmin (20000.0 * 1.06, sr * 0.5);
}

float MeterBridge::freqToT (double f) const
{
    double fMin, fMax; axisRange (mapSr > 0.0 ? mapSr : 44100.0, mapLog, fMin, fMax);
    return (float) (mapLog ? std::log (f / fMin) / std::log (fMax / fMin) : (f - fMin) / (fMax - fMin));
}

double MeterBridge::tToFreq (float t) const
{
    double fMin, fMax; axisRange (mapSr > 0.0 ? mapSr : 44100.0, mapLog, fMin, fMax);
    return mapLog ? fMin * std::pow (fMax / fMin, (double) t) : fMin + (fMax - fMin) * (double) t;
}

void MeterBridge::clearHistory()
{
    for (auto& h : hist) std::fill (h.begin(), h.end(), -120.f);
    head = 0; rowAcc = 0.0;
}

void MeterBridge::rebuildMap (int bins, double sr, bool logX)
{
    mapBins = bins; mapSr = sr; mapLog = logX;
    spans.resize ((size_t) kBC);
    double fMin, fMax; axisRange (sr, logX, fMin, fMax);
    const double nyq = sr * 0.5;
    auto binAt = [&] (double t)
    {
        const double f = logX ? fMin * std::pow (fMax / fMin, t) : fMin + (fMax - fMin) * t;
        return (float) juce::jlimit (0.0, (double) (bins - 1), f / nyq * bins);
    };
    for (int x = 0; x < kBC; ++x) spans[(size_t) x] = { binAt ((x - 0.5) / (kBC - 1)), binAt ((x + 0.5) / (kBC - 1)) };
}

// narrow columns interpolate between FFT bins, wide ones pool many bins (no blockiness, no aliasing)
void MeterBridge::sampleColumns (const float* db, int bins, std::vector<float>& out) const
{
    for (int x = 0; x < kBC; ++x)
    {
        const auto s = spans[(size_t) x];
        if (s.hi - s.lo < 1.f)
        {
            const float c = 0.5f * (s.lo + s.hi); const int i = (int) c, j = juce::jmin (i + 1, bins - 1);
            out[(size_t) x] = db[i] + (db[j] - db[i]) * (c - (float) i);
        }
        else
        {
            const int i0 = (int) std::floor (s.lo), i1 = juce::jmin ((int) std::ceil (s.hi), bins - 1);
            double sum = 0.0; float mx = -200.f;
            for (int i = i0; i <= i1; ++i) { sum += std::pow (10.0, db[i] * 0.1); mx = juce::jmax (mx, db[i]); }
            out[(size_t) x] = 0.5f * ((float) (10.0 * std::log10 (sum / (i1 - i0 + 1) + 1.0e-12)) + mx);
        }
    }
}

float MeterBridge::desiredSeconds (double sr) const
{
    if (is3D()) return getf ("b_hist", 10.f);
    static const float t[5] = { 2.f, 5.f, 10.f, 20.f, 30.f };
    const int idx = juce::jlimit (0, 5, geti ("b_scroll", 2));
    if (idx < 5) return t[idx];
    const int sizeIdx = juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("fftSize")->load());
    const int ov = juce::jlimit (0, 2, (int) apvts.getRawParameterValue ("overlap")->load());
    const double hop = (double) ((512 << sizeIdx) >> (1 + ov));
    return (float) ((double) kBR * hop / juce::jmax (8000.0, sr));   // one row per FFT frame
}

void MeterBridge::feed (const std::array<std::vector<float>, 4>& taps, int bins, double sr, float dt, bool live)
{
    const bool logX = apvts.getRawParameterValue ("xLog")->load() > 0.5f;
    const bool valid = bins > 1 && (int) taps[0].size() >= bins && (int) taps[3].size() >= bins;
    const bool have = valid && live;     // no fresh frames (host paused) -> the targets fall to silence instead of holding the last spectrum
    if (valid && (bins != mapBins || sr != mapSr || logX != mapLog))
    {
        if (mapBins >= 0 && (sr != mapSr || logX != mapLog)) clearHistory();
        rebuildMap (bins, sr, logX);
    }
    const float secs = desiredSeconds (mapSr > 0.0 ? mapSr : sr);
    if (std::abs (secs - curSecs) > 1.0e-3f) { curSecs = secs; clearHistory(); }

    const float atk = 1.f - std::exp (-dt / 0.018f), rel = 1.f - std::exp (-dt / 0.110f);
    const float* srcPtr[kSrc] = {};
    if (have)
    {
        lrBuf.resize ((size_t) bins);
        for (int i = 0; i < bins; ++i)   // L+R as a power average (keeps out-of-phase content visible)
        {
            const float a = std::exp (taps[0][(size_t) i] * 0.230259f), b = std::exp (taps[1][(size_t) i] * 0.230259f);
            lrBuf[(size_t) i] = 4.3429f * std::log (0.5f * (a + b) + 1.0e-12f);
        }
        srcPtr[0] = taps[0].data(); srcPtr[1] = taps[1].data(); srcPtr[2] = lrBuf.data(); srcPtr[3] = taps[2].data(); srcPtr[4] = taps[3].data();
    }
    for (size_t s = 0; s < (size_t) kSrc; ++s)
    {
        if (have)
        {
            sampleColumns (srcPtr[s], bins, target[s]);
            for (int x = 0; x < kBC; ++x)   // light smoothing across frequency
            {
                auto at = [&] (int k) { return target[s][(size_t) juce::jlimit (0, kBC - 1, k)]; };
                tmpCols[(size_t) x] = 0.25f * (at (x - 1) + 2.f * at (x) + at (x + 1));
            }
            target[s].swap (tmpCols);
        }
        else std::fill (target[s].begin(), target[s].end(), -120.f);

        for (int x = 0; x < kBC; ++x)
        {
            const float st = state[s][(size_t) x], t = target[s][(size_t) x];
            state[s][(size_t) x] = st + (t - st) * (t > st ? atk : rel);
            accMax[s][(size_t) x] = juce::jmax (accMax[s][(size_t) x], state[s][(size_t) x]);
        }
    }

    int n = 0;
    if (! frozen)
    {
        rowAcc += (double) dt * ((double) kBR / (double) curSecs);
        n = juce::jmin ((int) rowAcc, kBR / 2); rowAcc -= (int) rowAcc;
        for (size_t s = 0; s < (size_t) kSrc; ++s)
        {
            int h = head;
            for (int k = 1; k <= n; ++k)
            {
                const float a = (float) k / (float) n;
                float* row = &hist[s][(size_t) h * (size_t) kBC];
                for (int x = 0; x < kBC; ++x)
                {
                    float v = prevS[s][(size_t) x] + (state[s][(size_t) x] - prevS[s][(size_t) x]) * a;
                    if (k == n) v = juce::jmax (v, accMax[s][(size_t) x]);   // keep short peaks on slow settings
                    row[x] = v;
                }
                h = (h + 1) % kBR;
            }
        }
        head = (head + n) % kBR;
    }
    for (size_t s = 0; s < (size_t) kSrc; ++s)
    {
        if (n > 0 || frozen) accMax[s] = state[s];
        prevS[s] = state[s];
    }

    // Transport stopped / input silent: the live spectrum has already fallen to the floor, but the history
    // still holds the last seconds of sound. Fade that out quickly so nothing is left on screen.
    float peakNow = -200.f;
    for (size_t s = 0; s < (size_t) kSrc; ++s)
        for (int x = 0; x < kBC; ++x) peakNow = juce::jmax (peakNow, state[s][(size_t) x]);
    silentSec = (! have || peakNow < -100.f) ? silentSec + dt : 0.f;
    if (! frozen && silentSec > 0.3f && silentSec < 1.6f)
    {
        const float drop = 160.f * dt;      // dB per second
        for (auto& h : hist)
            for (auto& v : h) v = juce::jmax (-120.f, v - drop);
    }
}

// ═════════════════════════════════════════════════════════════ rendering
juce::Colour MeterBridge::lift (juce::Colour c)
{
    float h, s, b; c.getHSB (h, s, b);
    return juce::Colour::fromHSV (h, juce::jmin (1.f, s * 1.05f), juce::jmax (b, 0.82f), 1.f);
}

void MeterBridge::buildEntries()
{
    entries.clear(); entrySrc.clear();
    bool anyFocus = false;
    for (int i = 0; i < kSlots; ++i) if (slotShow[(size_t) i] && slotSrc[(size_t) i] >= 0 && slotFocus[(size_t) i]) anyFocus = true;
    for (int i = 0; i < kSlots; ++i)
    {
        const int src = slotSrc[(size_t) i];
        if (src < 0 || ! slotShow[(size_t) i]) continue;
        bridge::Entry e;
        e.hist = { hist[(size_t) src].data(), kBR, kBC, head };
        const auto c = lift (slotCol[(size_t) i]);
        e.r = c.getFloatRed() * 255.f; e.g = c.getFloatGreen() * 255.f; e.b = c.getFloatBlue() * 255.f;
        e.grey = anyFocus && ! slotFocus[(size_t) i];
        entries.push_back (e); entrySrc.push_back (src);
    }
}

void MeterBridge::upload (const bridge::Image& src, juce::Image& dst)
{
    if (! dst.isValid() || dst.getWidth() != src.w || dst.getHeight() != src.h)
        dst = juce::Image (juce::Image::ARGB, src.w, src.h, true, juce::SoftwareImageType());
    juce::Image::BitmapData bd (dst, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < src.h; ++y)
    {
        auto* line = reinterpret_cast<juce::PixelARGB*> (bd.getLinePointer (y));
        const uint32_t* in = src.px.data() + (size_t) y * (size_t) src.w;
        for (int x = 0; x < src.w; ++x)
        {
            const uint32_t p = in[x];
            line[x].setARGB ((juce::uint8) (p >> 24), (juce::uint8) (p >> 16), (juce::uint8) (p >> 8), (juce::uint8) p);
        }
    }
}

// Display range that follows the signal. Both ends come from the spread of the levels that are actually on
// screen (4th percentile .. just under the peak), so the bulk of the surface spans the whole colour scale
// instead of sitting at the dark end while a single loud bass peak uses all the bright colours.
void MeterBridge::autoRange (float& lo, float& hi)
{
    if (! frozen)
    {
        rangeScratch.clear();
        for (int src : entrySrc)
            for (int x = 0; x < kBC; ++x)
            {
                const float v = state[(size_t) src][(size_t) x];
                if (v > -118.f) rangeScratch.push_back (v);
            }
        if (rangeScratch.size() > 16)
        {
            std::sort (rangeScratch.begin(), rangeScratch.end());
            const size_t n = rangeScratch.size();
            const float p4 = rangeScratch[(size_t) ((float) (n - 1) * 0.04f)], p98 = rangeScratch[(size_t) ((float) (n - 1) * 0.98f)], mx = rangeScratch[n - 1];
            const float tHi = 0.5f * (p98 + mx) + 2.f, tLo = juce::jmin (p4, tHi - 40.f);
            autoHi += (tHi - autoHi) * (tHi > autoHi ? 0.35f : 0.02f);   // fast attack, slow release
            autoLo += (tLo - autoLo) * 0.04f;
        }
    }
    hi = juce::jlimit (-90.f, 6.f, autoHi);
    lo = juce::jlimit (hi - 90.f, hi - 40.f, autoLo);
}

void MeterBridge::render3D()
{
    const int W = juce::roundToInt (canvas.getWidth() * kScale3D), H = juce::roundToInt (canvas.getHeight() * kScale3D);
    if (img3.w != W || img3.h != H) img3.reset (W, H);

    bridge::Settings3D st;
    const int lod = juce::jlimit (0, 2, geti ("b_lod", 1));
    st.gridRows = lod == 0 ? 96 : lod == 1 ? 144 : 200;
    st.gridCols = lod == 0 ? 256 : lod == 1 ? 384 : 512;
    st.heightScale = (float) sHeight.getValue();
    autoRange (st.lo, st.hi);
    st.preset = juce::jlimit (0, 3, geti ("b_preset", 1));
    st.depth = kDepth;
    lastGridRows = st.gridRows; lastGridCols = st.gridCols;

    bridge::Camera c = cam; c.panX *= 1.f; c.panY *= 1.f;
    scene.render (img3, c, entries, st);
    upload (img3, jimg3);
}

void MeterBridge::layoutPanes (std::vector<bridge::Pane>& panes, int w, int h) const
{
    panes.clear();
    const int n = (int) entries.size();
    if (n == 0) return;
    const int view = geti ("b_view2", 0), gap = 3;
    if (view == 0) { bridge::Pane p; p.x = 0; p.y = 0; p.w = w; p.h = h; for (int i = 0; i < n; ++i) p.entries.push_back (i); panes.push_back (p); return; }
    const int cols = view == 1 ? (int) std::ceil (std::sqrt ((double) n)) : 1, rows = (n + cols - 1) / cols;
    const int pw = (w - gap * (cols - 1)) / cols, ph = (h - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
    {
        bridge::Pane p; p.x = (i % cols) * (pw + gap); p.y = (i / cols) * (ph + gap); p.w = pw; p.h = ph; p.entries.push_back (i);
        panes.push_back (p);
    }
}

void MeterBridge::render2D()
{
    area2D = { canvas.getX() + 50, canvas.getY() + 10, canvas.getWidth() - 62, canvas.getHeight() - 38 };
    const int W = juce::roundToInt (area2D.getWidth() * kScale2D), H = juce::roundToInt (area2D.getHeight() * kScale2D);
    if (img2.w != W || img2.h != H) img2.reset (W, H);
    layoutPanes (panes2D, W, H);
    float lo, hi; autoRange (lo, hi);
    bridge::render2D (img2, entries, panes2D, lo, hi, juce::jlimit (0, 3, geti ("b_preset", 1)));
    upload (img2, jimg2);
}

void MeterBridge::tickRender (float dt)
{
    const float k = 1.f - std::exp (-dt * 9.f);   // ease the camera towards its target
    cam.yaw   += (camTarget.yaw   - cam.yaw)   * k;
    cam.pitch += (camTarget.pitch - cam.pitch) * k;
    cam.zoom  += (camTarget.zoom  - cam.zoom)  * k;
    cam.panX  += (camTarget.panX  - cam.panX)  * k;
    cam.panY  += (camTarget.panY  - cam.panY)  * k;

    renderAcc += dt;
    if (renderAcc < 0.031f) return;      // draw at ~30 fps
    renderAcc = 0.f;
    buildEntries();
    if (is3D()) render3D(); else render2D();
    repaint (canvas);
}

// ═════════════════════════════════════════════════════════════ painting
void MeterBridge::drawAxes3D (juce::Graphics& g)
{
    const auto& pr = scene.proj;
    if (pr.w == 0) return;
    auto P = [&] (float x, float z, juce::Point<float>& o)
    {
        float sx, sy, iw;
        if (! bridge::Scene3D::project (pr, { x, 0.f, z }, sx, sy, iw)) return false;
        o = { (float) canvas.getX() + sx / kScale3D, (float) canvas.getY() + sy / kScale3D }; return true;
    };
    auto rotatedText = [&] (const juce::String& txt, juce::Point<float> p, float gap)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi, p.x, p.y));
        g.drawText (txt, juce::Rectangle<float> (p.x + gap, p.y - 6.f, 52.f, 12.f), juce::Justification::centredLeft);
    };

    const auto axisCol = juce::Colour (0xff7d8aa6);
    juce::Point<float> a, b;
    g.setFont (ui (10.f, false));
    if (P (-1.f, 0.f, a) && P (1.f, 0.f, b)) { g.setColour (axisCol.withAlpha (0.8f)); g.drawLine (a.x, a.y, b.x, b.y, 1.2f); }

    struct F { double f; const char* s; float len; };
    static const F freqs[] = { { 100, "100", .26f }, { 300, "300", .12f }, { 500, "500", .18f }, { 700, "700", .12f }, { 1000, "1000", .26f },
                               { 1500, "1500", .12f }, { 2000, "2000", .18f }, { 3000, "3000", .12f }, { 5000, "5000", .18f }, { 7000, "7000", .12f },
                               { 10000, "10000", .26f }, { 15000, "15000", .14f }, { 20000, "20000", .2f } };
    for (const auto& f : freqs)
    {
        const float t = freqToT (f.f);
        if (t < 0.f || t > 1.f) continue;
        const float x = -1.f + 2.f * t;
        juce::Point<float> p0, p1, pl;
        if (! (P (x, 0.f, p0) && P (x, f.len, p1) && P (x, -0.05f, pl))) continue;
        g.setColour (axisCol.withAlpha (0.55f)); g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.f);
        g.setColour (axisCol); rotatedText (f.s, pl, 1.f);
    }
    // dB scale up the front-left corner (same height curve as the surface)
    {
        auto PY = [&] (float y, float x, float z, juce::Point<float>& o)
        {
            float sx, sy, iw;
            if (! bridge::Scene3D::project (pr, { x, y, z }, sx, sy, iw)) return false;
            o = { (float) canvas.getX() + sx / kScale3D, (float) canvas.getY() + sy / kScale3D }; return true;
        };
        const float lo = scene.dispLo, hi = scene.dispHi, span = juce::jmax (1.f, hi - lo);
        juce::Point<float> base, top;
        if (PY (0.f, -1.f, 0.f, base) && PY (scene.worldHeight, -1.f, 0.f, top))
        { g.setColour (axisCol.withAlpha (0.45f)); g.drawLine (base.x, base.y, top.x, top.y, 1.f); }
        g.setFont (ui (10.f, false));
        for (float db = std::ceil (hi / 20.f) * 20.f; db > lo; db -= 20.f)
        {
            if (db > hi) continue;
            const float y = bridge::shapeV ((db - lo) / span) * scene.worldHeight;
            juce::Point<float> p0, p1;
            if (! (PY (y, -1.f, 0.f, p0) && PY (y, -1.f, 0.f, p1))) continue;
            g.setColour (axisCol.withAlpha (0.55f)); g.drawLine (p0.x - 6.f, p0.y, p0.x, p0.y, 1.f);
            g.setColour (axisCol);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (p0.x - 44.f, p0.y - 6.f, 34.f, 12.f), juce::Justification::centredRight);
        }
    }
    // time axis along the left edge: 0 s at the front, history length at the back
    if (P (-1.f, 0.f, a) && P (-1.f, kDepth, b))
    {
        g.setColour (axisCol.withAlpha (0.5f)); g.drawLine (a.x, a.y, b.x, b.y, 1.f);
        for (int i = 0; i <= 4; ++i)
        {
            juce::Point<float> t0, t1;
            if (P (-1.f, kDepth * (float) i / 4.f, t0) && P (-1.05f, kDepth * (float) i / 4.f, t1)) g.drawLine (t0.x, t0.y, t1.x, t1.y, 1.f);
        }
        g.setColour (axisCol);
        g.drawText ("0 s", juce::Rectangle<float> (a.x - 48.f, a.y - 6.f, 34.f, 12.f), juce::Justification::centredRight);
        g.drawText ("-" + juce::String (curSecs, curSecs == std::floor (curSecs) ? 0 : 1) + " s", juce::Rectangle<float> (b.x - 56.f, b.y - 6.f, 44.f, 12.f), juce::Justification::centredRight);
    }
    if (P (-1.f, -0.05f, a)) { g.setColour (axisCol); rotatedText ("Hz", { a.x - 22.f, a.y }, 1.f); }
}

void MeterBridge::drawAxes2D (juce::Graphics& g)
{
    g.setFont (ui (10.f, false));
    const int total = juce::jmax (1, (int) entries.size());
    juce::ignoreUnused (total);
    for (const auto& p : panes2D)
    {
        const juce::Rectangle<float> r ((float) area2D.getX() + p.x / kScale2D, (float) area2D.getY() + p.y / kScale2D, p.w / kScale2D, p.h / kScale2D);
        for (double f : { 30.0, 100.0, 300.0, 1000.0, 3000.0, 10000.0 })
        {
            const float t = freqToT (f);
            if (t <= 0.02f || t >= 0.98f) continue;
            const float y = r.getBottom() - t * r.getHeight();
            if (r.getHeight() < 120.f && (f == 30.0 || f == 300.0 || f == 3000.0)) continue;
            g.setColour (juce::Colour (0x22ffffff)); g.drawHorizontalLine ((int) y, r.getX(), r.getX() + 6.f);
            g.setColour (juce::Colour (0xaaffffff));
            g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f), (int) r.getX() + 8, (int) y - 6, 40, 12, juce::Justification::centredLeft);
        }
        // pane titles
        float tx = r.getX() + 8.f;
        for (int ei : p.entries)
        {
            const auto& e = entries[(size_t) ei];
            g.setColour (juce::Colour::fromFloatRGBA (e.r / 255.f, e.g / 255.f, e.b / 255.f, 1.f).withAlpha (e.grey ? 0.5f : 1.f));
            const juce::String name = kSrcNames[entrySrc[(size_t) ei]];
            g.setFont (ui (10.5f));
            g.drawText (name, (int) tx, (int) r.getY() + 4, 90, 14, juce::Justification::centredLeft);
            tx += 18.f + juce::GlyphArrangement::getStringWidth (ui (10.5f), name);
        }
    }
    // time labels
    g.setFont (ui (10.f, false)); g.setColour (kMuted);
    for (int k = 0; k <= 4; ++k)
    {
        const float v = curSecs * (1.f - (float) k / 4.f);
        const int x = area2D.getX() + k * area2D.getWidth() / 4;
        const juce::String txt = k == 4 ? juce::String ("now") : "-" + juce::String (v, v == std::floor (v) ? 0 : 1) + " s";
        g.drawText (txt, x - 24, area2D.getBottom() + 4, 48, 14, k == 0 ? juce::Justification::centredLeft : k == 4 ? juce::Justification::centredRight : juce::Justification::centred);
    }
}

void MeterBridge::drawHover (juce::Graphics& g)
{
    if (hoverId == 0 || ! is3D()) return;
    const int ei = (hoverId >> 20) - 1, row = (hoverId >> 10) & 1023, col = hoverId & 1023;
    if (ei < 0 || ei >= (int) entrySrc.size()) return;
    const int age = juce::roundToInt ((float) row / (float) (lastGridRows - 1) * (float) (kBR - 1));
    const int c = juce::roundToInt ((float) col / (float) (lastGridCols - 1) * (float) (kBC - 1));
    const int src = entrySrc[(size_t) ei];
    const int ring = ((head - 1 - age) % kBR + kBR) % kBR;
    const float db = hist[(size_t) src][(size_t) ring * (size_t) kBC + (size_t) c];
    const double f = tToFreq ((float) c / (float) (kBC - 1));
    const float sec = (float) age * curSecs / (float) kBR;
    const juce::String txt = juce::String (kSrcNames[src]) + "   " + (f >= 1000.0 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String ((int) f) + " Hz")
                           + "   " + juce::String (db, 1) + " dB   " + (age == 0 ? juce::String ("now") : "-" + juce::String (sec, 1) + " s");
    const auto font = ui (11.f);
    const float w = juce::GlyphArrangement::getStringWidth (font, txt) + 18.f;
    juce::Rectangle<float> r ((float) hoverPos.x + 14.f, (float) hoverPos.y - 30.f, w, 20.f);
    r = r.constrainedWithin (canvas.toFloat().reduced (4.f));
    g.setColour (juce::Colour (0xe0141822)); g.fillRoundedRectangle (r, 6.f);
    g.setColour (kLine.brighter (0.3f)); g.drawRoundedRectangle (r, 6.f, 1.f);
    g.setColour (kText); g.setFont (font); g.drawText (txt, r, juce::Justification::centred);
}

void MeterBridge::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141822), 0, 0, juce::Colour (0xff0d1017), 0, 840.f, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.12f), 150, -40, juce::Colours::transparentBlack, 150, 420, true));
    g.fillRect (0, 0, 1024, 460);

    // top strip labels
    g.setColour (kMuted); g.setFont (ui (10.5f));
    g.drawText ("VIEW", 16, 14, 44, 28, juce::Justification::centredLeft);
    g.drawText ("VISIBILITY", 738, 14, 70, 28, juce::Justification::centredRight);

    // canvas
    g.setColour (juce::Colour (0xff050709)); g.fillRoundedRectangle (canvas.toFloat(), 10.f);
    g.setColour (kLine); g.drawRoundedRectangle (canvas.toFloat(), 10.f, 1.f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addRoundedRectangle (canvas.toFloat(), 10.f);
        g.reduceClipRegion (clip);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        // drawImage() uses the current colour's alpha as the image opacity. The border colour set just above
        // (kLine) is ~12 % opaque, which is what made the whole 3D / 2D picture look faded and ghosted.
        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.f, 1.f, (float) sVis.getValue() / 100.f)));   // VISIBILITY slider: 0 = invisible, 100 = full
        if (entries.empty())
        {
            g.setColour (kMuted); g.setFont (ui (13.f));
            g.drawText ("Enable a Meter Tap (Meter Taps tab) to see its spectrogram", canvas, juce::Justification::centred);
        }
        else if (is3D()) { if (jimg3.isValid()) g.drawImage (jimg3, canvas.toFloat()); }
        else if (jimg2.isValid()) g.drawImage (jimg2, area2D.toFloat());
    }
    if (! entries.empty())
    {
        if (is3D()) { drawAxes3D (g); drawHover (g); } else drawAxes2D (g);
    }

    // bottom card
    g.setColour (kPanel); g.fillRoundedRectangle (14.f, 574.f, 996.f, 168.f, 12.f);
    g.setColour (kLine);  g.drawRoundedRectangle (14.f, 574.f, 996.f, 168.f, 12.f, 1.f);
    g.setColour (kMuted); g.setFont (ui (10.5f));
    if (geti ("b_tab", 0) == 0)
    {
        const int colX[3] = { 30, 360, 690 }, rowY[3] = { 598, 642, 686 };
        const char* labels[3][3] = { { "MOUSE", "COLOR PRESET", "FFT SIZE" }, { "FFT OVERLAP", "3D DETAIL", "2D SCROLL" }, { "ZOOM", "HEIGHT", "HISTORY" } };
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r)
                g.drawText (labels[c][r], colX[c], rowY[r], 104, 30, juce::Justification::centredLeft);
    }
    else
    {
        for (int grp = 0; grp < 2; ++grp)
        {
            const int x0 = grp == 0 ? 30 : 520;
            g.drawText ("METER TAP", x0, 584, 190, 16, juce::Justification::centred);
            g.drawText ("SHOW",  x0 + 206, 584, 28, 16, juce::Justification::centred);
            g.drawText ("FOCUS", x0 + 252, 584, 48, 16, juce::Justification::centred);
            g.drawText ("COLOR", x0 + 318, 584, 80, 16, juce::Justification::centred);
        }
    }
    g.setColour (kMuted.withAlpha (0.8f)); g.setFont (ui (11.f, false));
    g.drawText (is3D() ? "Drag: orbit / pan   |   Wheel: zoom   |   Double-click: reset view   |   Hover the surface for dB read-out"
                       : "Tap colors overlay, tile or stack   |   Freeze to inspect",
                356, 750, 650, 28, juce::Justification::centredLeft);
}

// ═════════════════════════════════════════════════════════════ mouse
void MeterBridge::mouseDown (const juce::MouseEvent& e)
{
    if (! is3D() || ! canvas.contains (e.getPosition())) return;
    lastDrag = e.getPosition(); dragging = true;
    const bool rightBtn = e.mods.isRightButtonDown() || e.mods.isPopupMenu();
    const bool leftRotates = geti ("b_mouse", 0) == 0;
    dragRotate = leftRotates ? ! rightBtn : rightBtn;
    if (e.mods.isShiftDown()) dragRotate = ! dragRotate;
}

void MeterBridge::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || ! is3D()) return;
    const auto d = e.getPosition() - lastDrag; lastDrag = e.getPosition();
    if (dragRotate)
    {
        camTarget.yaw -= (float) d.x * 0.45f;
        camTarget.pitch = juce::jlimit (3.f, 88.f, camTarget.pitch + (float) d.y * 0.35f);
        cam.yaw = camTarget.yaw; cam.pitch = camTarget.pitch;
        seti ("b_view3", -1); refreshButtons();   // no preset is active any more
    }
    else
    {
        camTarget.panX += (float) d.x * kScale3D; camTarget.panY += (float) d.y * kScale3D;
        cam.panX = camTarget.panX; cam.panY = camTarget.panY;
    }
    hoverId = 0;
}

void MeterBridge::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! is3D() || ! canvas.contains (e.getPosition())) return;
    dragging = false;
    camTarget.zoom = 1.3f; sZoom.setValue (1.3, juce::sendNotificationSync);
    applyPreset (0);
}

void MeterBridge::mouseMove (const juce::MouseEvent& e)
{
    dragging = false;
    if (! is3D() || ! canvas.contains (e.getPosition()) || scene.ids.empty()) { if (hoverId != 0) { hoverId = 0; repaint (canvas); } return; }
    const int px = juce::roundToInt ((float) (e.x - canvas.getX()) * kScale3D), py = juce::roundToInt ((float) (e.y - canvas.getY()) * kScale3D);
    int id = 0;
    if (px >= 0 && py >= 0 && px < img3.w && py < img3.h) id = scene.ids[(size_t) py * (size_t) img3.w + (size_t) px];
    if (id != hoverId || id != 0) { hoverId = id; hoverPos = e.getPosition(); repaint (canvas); }
}

void MeterBridge::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (is3D() && canvas.contains (e.getPosition()))
    {
        const float z = juce::jlimit (0.4f, 3.f, camTarget.zoom * std::exp (wheel.deltaY * 0.8f));
        sZoom.setValue ((double) z, juce::sendNotificationSync);
    }
}
