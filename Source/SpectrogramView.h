#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include "PluginProcessor.h"

// Scrolling spectrogram. Frequency runs left -> right, time runs top -> bottom.
// History lives in a fixed-size image that is stretched to the component, so the
// view can be resized freely without losing data.
class SpectrogramView : public juce::Component, private juce::Timer
{
public:
    explicit SpectrogramView (SpectroProcessor&);
    ~SpectrogramView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverX = hoverY = -1; repaint(); }

    juce::Rectangle<int> plotArea() const;

private:
    void timerCallback() override;
    void rebuildIfNeeded();
    void resetHistory();
    void computeRow();
    float freqAtFraction (float f) const;
    float weightingDb (float hz, int type) const;

    SpectroProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr int cols = 1024;
    static constexpr int rows = 480;

    juce::Image history { juce::Image::ARGB, cols, rows, true };
    int writeRow = 0;   // next row to write (ring)
    int rowsWritten = 0;

    int fftOrder = 11;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> windowFn;
    std::vector<float> ring, work, window, weightCache, colBin;
    int ringPos = 0;
    double sinceRow = 0.0;
    float coherentGain = 0.5f;

    int  lastSize = -1, lastWin = -1, lastWeight = -1, lastRange = -1;
    bool lastLog = true;
    float lastLo = -1.0f, lastHi = -1.0f;

    std::vector<float> chunk = std::vector<float> (8192);
    float hoverX = -1.0f, hoverY = -1.0f;
};
