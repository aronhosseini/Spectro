#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <numeric>
#include <cmath>

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout SpectroProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "weighting", 1 }, "Weighting", juce::StringArray { "A", "B", "B2", "C" }, 3));   // C-weighting by default
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "fftSize", 1 }, "FFT Size", juce::StringArray { "512", "1024", "2048", "4096", "8192" }, 2));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "window", 1 }, "Window", juce::StringArray { "Hann", "Hamming", "Blackman", "Rectangular" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "overlap", 1 }, "FFT Overlap", juce::StringArray { "50 %", "75 %", "87.5 %" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "channel", 3 }, "Channel", juce::StringArray { "L", "R", "LR", "M", "S", "MS" }, 2)); // LR by default
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "time", 1 }, "Time", juce::StringArray { "2 s", "5 s", "10 s", "20 s", "30 s", "60 s" }, 1)); // 5 s
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
    pOverlap   = apvts.getRawParameterValue ("overlap");
    pSize      = apvts.getRawParameterValue ("fftSize");
    pWindow    = apvts.getRawParameterValue ("window");
    pWeighting = apvts.getRawParameterValue ("weighting");
    pGain      = apvts.getRawParameterValue ("gain");

    using WF = juce::dsp::WindowingFunction<float>;
    const WF::WindowingMethod methods[] = { WF::hann, WF::hamming, WF::blackman, WF::rectangular };

    for (int s = 0; s < numSizes; ++s)
    {
        const int size = 512 << s;
        ffts[(size_t) s] = std::make_unique<juce::dsp::FFT> (9 + s);

        for (int w = 0; w < numWindows; ++w)
        {
            auto& table = windows[(size_t) w][(size_t) s];
            table.assign ((size_t) size, 0.f);
            WF::fillWindowingTables (table.data(), (size_t) size, methods[w], false);
            windowSum[(size_t) w][(size_t) s] = std::accumulate (table.begin(), table.end(), 0.f);
        }
    }
    for (auto& f : frame) f.fill (-120.f);
}

bool SpectroProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == l.getMainInputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void SpectroProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr; fifoIndex = 0; weightOrder = -1;
}

// A / B / C weighting curves (dB), per FFT bin. Recomputed only when the settings change.
static float weightingDb (char type, float f)
{
    if (f < 10.f) return -80.f;
    const double f2 = (double) f * f;
    auto ra = [&] { const double n = 12194.0 * 12194.0 * f2 * f2;
                    const double d = (f2 + 20.6 * 20.6) * std::sqrt ((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + 12194.0 * 12194.0);
                    return 20.0 * std::log10 (n / d) + 2.0; };
    auto rc = [&] { const double n = 12194.0 * 12194.0 * f2;
                    const double d = (f2 + 20.6 * 20.6) * (f2 + 12194.0 * 12194.0);
                    return 20.0 * std::log10 (n / d) + 0.06; };
    if (type == 'A') return (float) ra();
    if (type == 'C') return (float) rc();
    return (float) (0.5 * (ra() + rc()));   // B-style approximation
}

void SpectroProcessor::refreshWeighting()
{
    const int bins = (1 << fftOrder) / 2;
    const char type = "ABBC"[juce::jlimit (0, 3, curWeighting)];
    for (int i = 0; i < bins; ++i)
        weightDb[(size_t) i] = weightingDb (type, (float) (i * sampleRate / (double) (1 << fftOrder)));
    weightOrder = fftOrder; weightSr = sampleRate;
}

void SpectroProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), ch = buf.getNumChannels();

    // pick up setting changes once per block
    const int wantOrder = 9 + juce::jlimit (0, numSizes - 1, (int) pSize->load());
    if (wantOrder != fftOrder) { fftOrder = wantOrder; fifoIndex = 0; }
    curWindow = juce::jlimit (0, numWindows - 1, (int) pWindow->load());
    const int wIdx = (int) pWeighting->load();
    if (wIdx != curWeighting || weightOrder != fftOrder || weightSr != sampleRate)
    { curWeighting = wIdx; refreshWeighting(); }

    const float* l = buf.getReadPointer (0);
    const float* r = buf.getReadPointer (ch > 1 ? 1 : 0);

    peakL = juce::Decibels::gainToDecibels (buf.getMagnitude (0, 0, n), -100.f);
    peakR = juce::Decibels::gainToDecibels (buf.getMagnitude (ch > 1 ? 1 : 0, 0, n), -100.f);

    for (int i = 0; i < n; ++i)
        pushSample (l[i], r[i]);
    // audio passes through untouched
}

void SpectroProcessor::pushSample (float left, float right)
{
    const int size = 1 << fftOrder;
    fifo[0][(size_t) fifoIndex] = left;
    fifo[1][(size_t) fifoIndex] = right;
    fifo[2][(size_t) fifoIndex] = 0.5f * (left + right);    // mid
    fifo[3][(size_t) fifoIndex] = 0.5f * (left - right);    // side
    ++fifoIndex;
    if (fifoIndex >= size)
    {
        computeFrame();
        const int hop = size >> (1 + juce::jlimit (0, 2, (int) pOverlap->load()));   // 50 / 75 / 87.5 % overlap
        for (auto& f : fifo) std::memmove (f.data(), f.data() + hop, (size_t) (size - hop) * sizeof (float));
        fifoIndex = size - hop;
    }
}

void SpectroProcessor::computeFrame()
{
    const int size = 1 << fftOrder, bins = size / 2, si = fftOrder - 9;
    const auto& win = windows[(size_t) curWindow][(size_t) si];

    // scale so a full-scale sine reads 0 dBFS regardless of window
    const float norm = 2.f / windowSum[(size_t) curWindow][(size_t) si];
    const float gain = pGain->load();

    for (size_t c = 0; c < 4; ++c)
    {
        for (int i = 0; i < size; ++i) work[(size_t) i] = fifo[c][(size_t) i] * win[(size_t) i];
        std::fill (work.begin() + size, work.begin() + 2 * size, 0.f);
        ffts[(size_t) si]->performFrequencyOnlyForwardTransform (work.data());
        for (int i = 0; i < bins; ++i)
            scratch[c][(size_t) i] = 20.f * std::log10 (juce::jmax (work[(size_t) i] * norm, 1.0e-6f)) + gain + weightDb[(size_t) i];
    }

    const juce::SpinLock::ScopedTryLockType sl (frameLock);
    if (sl.isLocked())
    {
        for (size_t c = 0; c < 4; ++c) std::copy (scratch[c].begin(), scratch[c].begin() + bins, frame[c].begin());
        frameBins = bins; frameSr = sampleRate; frameNew = true;
    }
}

bool SpectroProcessor::fetchFrame (std::array<std::vector<float>, 4>& taps, int& numBins, double& sr)
{
    const juce::SpinLock::ScopedTryLockType sl (frameLock);
    if (! sl.isLocked() || ! frameNew) return false;
    for (size_t c = 0; c < 4; ++c) taps[c].assign (frame[c].begin(), frame[c].begin() + frameBins);
    numBins = frameBins; sr = frameSr; frameNew = false;
    return true;
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
