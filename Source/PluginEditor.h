#pragma once
#include "PluginProcessor.h"
#include "Bridge.h"

// Dark, glassy look for chips / combos / knobs
struct SpectroLookAndFeel : public juce::LookAndFeel_V4
{
    SpectroLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.f, juce::Font::bold); }
};

// Everything is laid out at a fixed 1024x840 design size; the editor scales it.
class SpectroContent : public juce::Component
{
public:
    static constexpr int W = 1024, H = 840;
    explicit SpectroContent (SpectroProcessor&);
    ~SpectroContent() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
    // frequency zoom / pan (bar under the spectrogram, mouse wheel on the spectrogram)
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = Drag::none; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void exportImage();

private:
    // one scrolling history per lane (MS mode uses two: mid on top, side below)
    struct Lane { juce::Image img; std::vector<float> frameDb, target, state, prevState, tmp, accMax; };

    void tick();                                   // runs once per display refresh (VBlank)
    void setPage (int p);                          // 0 = spectrogram, 1 = 3D meter bridge
    void updateLanes (int mode);                   // pick the spectra for the spectrogram lanes out of the four analysis taps
    void clearHistory();
    juce::Rectangle<int> laneRect (int i) const;
    void rebuildColumnMap (int bins, double sr, bool logX);
    void sampleColumns (Lane& ln) const;
    juce::Image orderedSpec() const;
    static void axisRange (double sr, bool logX, double& fMin, double& fMax);
    juce::Rectangle<int> trackRect() const { return { 48, 556, 962, 14 }; }
    juce::Rectangle<int> thumbRect() const;
    float tToX (float t) const;
    float freqToT (double f) const;
    void setView (float lo, float hi);
    static juce::Colour heat (float v);

    SpectroProcessor& proc;
    SpectroLookAndFeel laf;
    MeterBridge bridge;                            // the 3D / 2D meter page (covers the main page while shown)
    juce::TextButton pageSpec { "2D" }, pageBridge { "3D" };
    std::vector<juce::Component*> mainControls;
    int page = 0;
    std::array<std::vector<float>, 4> taps;        // newest L, R, M, S spectra from the audio thread
    int tapBins = 0; double tapSr = 44100.0;
    // spectrogram ring buffer: kCols pixels across (2x oversampled), kRows of history, newest row = head-1
    static constexpr int   kCols = 4096, kRows = 768;
    static constexpr float kSeconds[6] { 2.f, 5.f, 10.f, 20.f, 30.f, 60.f };   // visible history per TIME option
    Lane lane[2];
    bool split = false;
    int modeIdx = -1;
    int head = 0, timeIdx = -1;
    // visible slice of the frequency axis (0..1). The history image always holds the full range, so zooming is instant.
    float viewLo = 0.f, viewHi = 1.f;
    static constexpr float kMinView = 0.03f;
    enum class Drag { none, pan, left, right };
    Drag drag = Drag::none;
    float dragLo = 0.f, dragHi = 1.f; int dragX = 0;
    double rowAcc = 0.0, lastMs = 0.0;
    float idleSec = 0.f;                       // time since the last audio frame arrived
    std::array<juce::PixelARGB, 1024> lut {};

    struct Span { float lo, hi; };
    std::vector<Span> spans;
    int mapBins = -1; double mapSr = 0.0; bool mapLog = true;

    int frameBins = 0; double frameSr = 44100.0;
    float meter[2] { -100.f, -100.f };
    juce::Rectangle<int> specArea { 14, 58, 912, 462 };

    juce::TextButton chL { "L" }, chR { "R" }, chLR { "LR" }, chM { "M" }, chS { "S" }, chMS { "MS" };
    juce::TextButton wA { "A" }, wB { "B" }, wB2 { "B" }, wC { "C" };
    juce::ComboBox fftSize, windowBox, timeBox;
    juce::Slider gain, floorS, memHi, memLo;
    juce::ToggleButton xAxis { "X-AXIS" }, yAxis { "Y-AXIS" }, grid { "GRID" }, read { "READ" }, xLog { "X: LOG" };
    juce::TextButton exportBtn { "EXPORT" };
    juce::HyperlinkButton aron { "ARON H", juce::URL ("https://www.instagram.com/aronh.official") };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> gainA, floorA, hiA, loA;
    std::unique_ptr<BA> xAxisA, yAxisA, gridA, readA, xLogA;
    std::unique_ptr<CA> sizeA, winA, timeA;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::VBlankAttachment vblank;   // keep last: starts calling tick() once constructed

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectroContent)
};

class SpectroEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpectroEditor (SpectroProcessor&);
    void resized() override;
private:
    SpectroContent content;
    juce::ComponentBoundsConstrainer limits;
};
