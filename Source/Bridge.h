#pragma once
#include "PluginProcessor.h"
#include "BridgeRender.h"

// Spectro Meter Bridge: a full-window 3D / 2D spectrogram with Meter Taps.
//  • 3D mode: Front High / Front Low / Diagonal / Side High / Side Low views, orbit + pan + zoom, hover read-out
//  • 2D mode: Overlay / Tile / Stack
//  • Freeze, Zoom, Height, History Length, Level of Detail, Color Preset, FFT Size / Overlap, 2D Scrolling Speed
//  • Meter Taps: up to 8 slots (Left, Right, Left + Right, Mid, Side), each with Show / Focus / Color
class MeterBridge : public juce::Component
{
public:
    explicit MeterBridge (juce::AudioProcessorValueTreeState& state);
    ~MeterBridge() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverId = 0; repaint (canvas); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // newest spectra (0 = L, 1 = R, 2 = M, 3 = S; dB per bin). Call on every UI refresh, visible or not.
    void feed (const std::array<std::vector<float>, 4>& taps, int bins, double sr, float dt, bool live = true);   // live=false: audio stopped, decay to silence
    // draw the picture. Call on every UI refresh while the bridge is visible.
    void tickRender (float dt);

private:
    struct Swatch;
    struct Span { float lo, hi; };
    static constexpr int   kBR = 480, kBC = 640, kSrc = 5, kSlots = 8;   // history rows / columns, sources (L R L+R M S), tap slots
    static constexpr float kScale3D = 1.2f, kScale2D = 0.5f;
    const juce::Rectangle<int> canvas { 14, 54, 996, 512 };

    // ── properties kept in the plugin state (saved with the project)
    int  geti (const juce::String& k, int d) const { return (int) apvts.state.getProperty (k, d); }
    float getf (const juce::String& k, float d) const { return (float) (double) apvts.state.getProperty (k, (double) d); }
    void seti (const juce::String& k, int v) { apvts.state.setProperty (k, v, nullptr); }
    void setf (const juce::String& k, float v) { apvts.state.setProperty (k, (double) v, nullptr); }

    // ── data
    void clearHistory();
    void rebuildMap (int bins, double sr, bool logX);
    void sampleColumns (const float* db, int bins, std::vector<float>& out) const;
    float desiredSeconds (double sr) const;
    static void axisRange (double sr, bool logX, double& fMin, double& fMax);
    float freqToT (double f) const;
    double tToFreq (float t) const;

    // ── drawing
    void buildEntries();
    void render3D();
    void render2D();
    void drawAxes3D (juce::Graphics&);
    void drawAxes2D (juce::Graphics&);
    void drawHover (juce::Graphics&);
    void layoutPanes (std::vector<bridge::Pane>& panes, int w, int h) const;
    static void upload (const bridge::Image& src, juce::Image& dst);
    static juce::Colour lift (juce::Colour c);

    // ── UI state
    void applyPreset (int idx);
    void resetAll();                 // every 3D-panel setting back to its default
    void refreshButtons();
    void refreshTabs();
    bool is3D() const { return true; }   // the Meter Bridge page is 3D only (the 2D spectrogram is the other page)

    juce::AudioProcessorValueTreeState& apvts;

    std::array<std::vector<float>, kSrc> hist, state, prevS, accMax, target;
    std::vector<float> lrBuf, tmpCols;
    std::vector<Span> spans;
    int mapBins = -1; double mapSr = 0.0; bool mapLog = true;
    int head = 0; double rowAcc = 0.0; float curSecs = -1.f;
    float silentSec = 0.f;                      // how long the input has been silent / stopped
    float autoLo = -90.f;
    float autoHi = -30.f;                       // smoothed loudest level shown -> display range follows the signal
    void autoRange (float& lo, float& hi);
    std::vector<float> rangeScratch;
    bool frozen = false;

    // camera
    bridge::Camera cam, camTarget;
    bridge::Scene3D scene;
    bridge::Image img3, img2;
    juce::Image jimg3, jimg2;
    std::vector<bridge::Entry> entries;
    std::vector<int> entrySrc;
    std::vector<bridge::Pane> panes2D;
    juce::Rectangle<int> area2D;
    int lastGridRows = 72, lastGridCols = 192;
    float renderAcc = 0.f;

    // meter-tap slots (cached copies of the saved settings)
    std::array<int, kSlots> slotSrc {};                 // -1 = empty, else 0 L, 1 R, 2 L+R, 3 M, 4 S
    std::array<bool, kSlots> slotShow {}, slotFocus {};
    std::array<juce::Colour, kSlots> slotCol {};
    void loadSlots();
    void storeSlot (int i);

    // interaction
    juce::Point<int> lastDrag;
    bool dragRotate = true, dragging = false;
    int hoverId = 0; juce::Point<int> hoverPos;

    // controls
    juce::TextButton b2D { "2D" }, b3D { "3D" }, freezeBtn { "Freeze" }, tabOptions { "Options" }, tabTaps { "Meter Taps" }, tabReset { "Reset" };
    std::array<juce::TextButton, 5> view3;
    std::array<juce::TextButton, 3> view2;
    juce::ComboBox cMouse, cPreset, cFFT, cOverlap, cLOD, cScroll;
    juce::Slider sZoom, sHeight, sHist, sVis;   // sVis: 3D / 2D picture visibility 0..100 %
    std::array<juce::ComboBox, kSlots> tapSrc;
    std::array<juce::ToggleButton, kSlots> tapShow, tapFocus;
    std::array<std::unique_ptr<Swatch>, kSlots> tapCol;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<CA> fftA, ovA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterBridge)
};
