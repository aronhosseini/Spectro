#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SpectroProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxFFT = 8192;

    SpectroProcessor();
    ~SpectroProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Spectro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Latest spectrum frame (dB per bin) for the editor. Returns false if no new frame.
    bool fetchFrame (std::vector<float>& dbOut, int& numBins, double& sr);
    // Peak level (dBFS) per channel, for the meters.
    std::atomic<float> peakL { -100.f }, peakR { -100.f };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushSample (float s);
    void computeFrame();

    double sampleRate = 44100.0;
    int fftOrder = 11;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    int lastSize = -1, lastWindow = -1;

    std::array<float, maxFFT> fifo {};
    std::array<float, maxFFT * 2> work {};
    int fifoIndex = 0;

    juce::SpinLock frameLock;
    std::vector<float> frame;
    int frameBins = 0;
    bool frameNew = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectroProcessor)
};
