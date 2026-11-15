#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SpectroProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxFFT     = 8192;
    static constexpr int numSizes   = 5;   // 512 … 8192
    static constexpr int numWindows = 4;   // Hann, Hamming, Blackman, Rectangular

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

    // Newest spectra in dB per bin for the four analysis taps: 0 = Left, 1 = Right, 2 = Mid, 3 = Side.
    // Returns false if nothing new since the last call.
    bool fetchFrame (std::array<std::vector<float>, 4>& taps, int& numBins, double& sr);
    // Block peak level (dBFS) per channel, for the meters.
    std::atomic<float> peakL { -100.f }, peakR { -100.f };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pushSample (float left, float right);
    void computeFrame();
    void refreshWeighting();

    double sampleRate = 44100.0;
    int fftOrder = 11, curWindow = 0, curWeighting = -1, weightOrder = -1;
    double weightSr = 0.0;

    // everything below is allocated up-front: nothing is allocated on the audio thread
    std::array<std::unique_ptr<juce::dsp::FFT>, numSizes> ffts;
    std::array<std::array<std::vector<float>, numSizes>, numWindows> windows;
    std::array<std::array<float, numSizes>, numWindows> windowSum {};
    std::array<float, maxFFT / 2> weightDb {};

    std::array<std::array<float, maxFFT>, 4> fifo {};          // L, R, M, S
    std::array<float, maxFFT * 2> work {};
    std::array<std::array<float, maxFFT / 2>, 4> scratch {};
    int fifoIndex = 0;

    juce::SpinLock frameLock;
    std::array<std::array<float, maxFFT / 2>, 4> frame {};
    int frameBins = 0;
    double frameSr = 44100.0;
    bool frameNew = false;

    std::atomic<float> *pSize {}, *pWindow {}, *pWeighting {}, *pGain {}, *pOverlap {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectroProcessor)
};
