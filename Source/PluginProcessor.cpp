#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout SpectroProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "weighting", 1 }, "Weighting", juce::StringArray { "A", "B", "B2", "C" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "fftSize", 1 }, "FFT Size", juce::StringArray { "512", "1024", "2048", "4096", "8192" }, 2));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "window", 1 }, "Window", juce::StringArray { "Hann", "Hamming", "Blackman", "Rectangular" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "channel", 1 }, "Channel", juce::StringArray { "L", "R", "LR", "MIS" }, 3));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gain", 1 }, "Gain", juce::NormalisableRange<float> (-24.f, 24.f, 0.1f), 0.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "floor", 1 }, "Floor", juce::NormalisableRange<float> (-120.f, -40.f, 1.f), -100.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "memHi", 1 }, "Memory High", juce::NormalisableRange<float> (-100.f, 0.f, 1.f), 0.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "memLo", 1 }, "Memory Low", juce::NormalisableRange<float> (-100.f, 0.f, 1.f), -100.f));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "xLog", 1 }, "X Log", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "showGrid", 1 }, "Grid", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "showRead", 1 }, "Read", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "showX", 1 }, "X-Axis", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "showY", 1 }, "Y-Axis", false));
    return { p.begin(), p.end() };
}

SpectroProcessor::SpectroProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    frame.assign (maxFFT / 2, -100.f);
}

bool SpectroProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == l.getMainInputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void SpectroProcessor::prepareToPlay (double sr, int) { sampleRate = sr; fifoIndex = 0; lastSize = -1; }

void SpectroProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), ch = buf.getNumChannels();
    const int mode = (int) apvts.getRawParameterValue ("channel")->load();
    auto* l = buf.getReadPointer (0);
    auto* r = buf.getReadPointer (ch > 1 ? 1 : 0);

    peakL = juce::Decibels::gainToDecibels (buf.getMagnitude (0, 0, n), -100.f);
    peakR = juce::Decibels::gainToDecibels (buf.getMagnitude (ch > 1 ? 1 : 0, 0, n), -100.f);

    for (int i = 0; i < n; ++i)
    {
        float s = 0.f;
        switch (mode) { case 0: s = l[i]; break; case 1: s = r[i]; break;
                        case 2: s = 0.5f * (l[i] + r[i]); break; default: s = 0.5f * (l[i] - r[i]); }
        pushSample (s);
    }
    // audio passes through untouched
}

void SpectroProcessor::pushSample (float s)
{
    const int size = 1 << fftOrder;
    fifo[(size_t) fifoIndex++] = s;
    if (fifoIndex >= size) { computeFrame(); fifoIndex = size / 2; // 50% overlap
        std::copy (fifo.begin() + size / 2, fifo.begin() + size, fifo.begin()); }
}

static float weightDb (char type, float f)
{
    const double f2 = (double) f * f;
    auto ra = [&] { double n = 12194.0 * 12194.0 * f2 * f2;
                    double d = (f2 + 20.6 * 20.6) * std::sqrt ((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0);
                    return 20.0 * std::log10 (n / d) + 2.0; };
    auto rc = [&] { double n = 12194.0 * 12194.0 * f2;
                    double d = (f2 + 20.6 * 20.6) * (f2 + 12194.0 * 12194.0);
                    return 20.0 * std::log10 (n / d) + 0.06; };
    if (f < 10.f) return 0.f;
    if (type == 'A') return (float) ra();
    if (type == 'C') return (float) rc();
    return (float) (0.5 * (ra() + rc()));  // B-style approximation
}

void SpectroProcessor::computeFrame()
{
    const int sizeIdx = (int) apvts.getRawParameterValue ("fftSize")->load();
    const int winIdx  = (int) apvts.getRawParameterValue ("window")->load();
    const int wIdx    = (int) apvts.getRawParameterValue ("weighting")->load();
    const int wantOrder = 9 + sizeIdx;

    if (wantOrder != fftOrder || winIdx != lastWindow || ! fft)
    {
        fftOrder = wantOrder; lastWindow = winIdx;
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        using WT = juce::dsp::WindowingFunction<float>;
        const WT::WindowingMethod m[] = { WT::hann, WT::hamming, WT::blackman, WT::rectangular };
        window = std::make_unique<WT> ((size_t) (1 << fftOrder), m[winIdx]);
        fifoIndex = 0; return;
    }

    const int size = 1 << fftOrder, bins = size / 2;
    std::fill (work.begin(), work.end(), 0.f);
    std::copy (fifo.begin(), fifo.begin() + size, work.begin());
    window->multiplyWithWindowingTable (work.data(), (size_t) size);
    fft->performFrequencyOnlyForwardTransform (work.data());

    const float gain = apvts.getRawParameterValue ("gain")->load();
    const char wt = "ABBC"[wIdx];
    std::vector<float> out ((size_t) bins);
    for (int i = 0; i < bins; ++i)
    {
        const float f = (float) (i * sampleRate / size);
        out[(size_t) i] = juce::Decibels::gainToDecibels (work[(size_t) i] / (float) bins * 2.f, -120.f)
                          + gain + weightDb (wt, f);
    }
    const juce::SpinLock::ScopedLockType sl (frameLock);
    frame.swap (out); frameBins = bins; frameNew = true;
}

bool SpectroProcessor::fetchFrame (std::vector<float>& dbOut, int& numBins, double& sr)
{
    const juce::SpinLock::ScopedTryLockType sl (frameLock);
    if (! sl.isLocked() || ! frameNew) return false;
    dbOut = frame; numBins = frameBins; sr = sampleRate; frameNew = false; return true;
}

juce::AudioProcessorEditor* SpectroProcessor::createEditor() { return new SpectroEditor (*this); }

void SpectroProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, d);
}
void SpectroProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpectroProcessor(); }
