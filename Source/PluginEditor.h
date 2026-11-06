#pragma once
#include "PluginProcessor.h"

// Dark, glassy look for chips / combos / knobs
struct SpectroLookAndFeel : public juce::LookAndFeel_V4
{
    SpectroLookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.f, juce::Font::bold); }
};

// Everything is laid out at a fixed 1024x840 design size; the editor scales it.
class SpectroContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1024, H = 840;
    explicit SpectroContent (SpectroProcessor&);
    ~SpectroContent() override { setLookAndFeel (nullptr); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void exportImage();

private:
    void timerCallback() override;
    void pushRow (const std::vector<float>& db, int bins, double sr);
    static juce::Colour heat (float v);

    SpectroProcessor& proc;
    SpectroLookAndFeel laf;
    juce::Image spec { juce::Image::ARGB, 900, 380, true };
    juce::Rectangle<int> specArea { 14, 144, 912, 376 };

    juce::TextButton chL { "L" }, chR { "R" }, chLR { "LR" }, chMIS { "MIS" };
    juce::TextButton wA { "A" }, wB { "B" }, wB2 { "B" }, wC { "C" };
    juce::ComboBox fftSize, windowBox;
    juce::Slider gain, floorS, memHi, memLo;
    juce::ToggleButton xAxis { "X-AXIS" }, yAxis { "Y-AXIS" }, grid { "GRID" }, read { "READ" }, xLog { "X: LOG" };
    juce::TextButton exportBtn { "EXPORT" };
    juce::HyperlinkButton aron { "ARON H", juce::URL ("https://www.instagram.com/aronh.official") };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SA> gainA, floorA, hiA, loA;
    std::unique_ptr<BA> xAxisA, yAxisA, gridA, readA, xLogA;
    std::unique_ptr<CA> sizeA, winA;
    std::unique_ptr<juce::FileChooser> chooser;

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
