#include <JuceHeader.h>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include "../Source/PluginProcessor.h"

namespace
{
float rms (const juce::AudioBuffer<float>& buffer, int start, int num) noexcept
{
    double sum = 0.0;
    int count = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < num; ++i)
        {
            const float sample = buffer.getSample (channel, start + i);
            sum += static_cast<double> (sample) * sample;
            ++count;
        }
    return count > 0 ? static_cast<float> (std::sqrt (sum / count)) : 0.0f;
}

void setFloat (juce::AudioProcessorValueTreeState& apvts, const char* id, float value)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

void setChoice (juce::AudioProcessorValueTreeState& apvts, const char* id, int index)
{
    if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
}

juce::AudioBuffer<float> render (EonMiniEEFProcessor& processor, const juce::MidiBuffer& midi,
                                 double sampleRate, int blockSize, int total)
{
    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    juce::AudioBuffer<float> buffer (2, total);
    buffer.clear();
    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int len = juce::jmin (blockSize, total - offset);
        juce::MidiBuffer slice;
        for (const auto metadata : midi)
            if (metadata.samplePosition >= offset && metadata.samplePosition < offset + len)
                slice.addEvent (metadata.getMessage(), metadata.samplePosition - offset);
        float* const* data = buffer.getArrayOfWritePointers();
        juce::AudioBuffer<float> sub (data, 2, offset, len);
        processor.processBlock (sub, slice);
    }
    return buffer;
}

float dominantHz (const juce::AudioBuffer<float>& buffer, int start, int num, double sampleRate)
{
    // Autocorrelation peak in the musical fundamental band. Ratio, not absolute
    // tuning, is the contract, so a slightly biased estimator still catches a
    // missing wheel.
    int bestLag = 0;
    double best = -1.0;
    const int minLag = static_cast<int> (sampleRate / 2000.0);
    const int maxLag = static_cast<int> (sampleRate / 50.0);
    const float* samples = buffer.getReadPointer (0);
    for (int lag = minLag; lag <= maxLag && lag < num; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i < num - lag; ++i)
            sum += static_cast<double> (samples[start + i]) * samples[start + i + lag];
        if (sum > best)
        {
            best = sum;
            bestLag = lag;
        }
    }
    return bestLag > 0 ? static_cast<float> (sampleRate / bestLag) : 0.0f;
}
}

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 64;
    constexpr int total = static_cast<int> (sampleRate);

    auto prepare = [] (EonMiniEEFProcessor& processor)
    {
        setChoice (processor.apvts, ParamIDs::osc1Wave, 3);
        setFloat (processor.apvts, ParamIDs::osc1Level, 0.8f);
        setFloat (processor.apvts, ParamIDs::osc2Level, 0.0f);
        setFloat (processor.apvts, ParamIDs::osc3Level, 0.0f);
        setFloat (processor.apvts, ParamIDs::osc4Level, 0.0f);
        setFloat (processor.apvts, ParamIDs::noiseMix, 0.0f);
        setFloat (processor.apvts, ParamIDs::release, 0.05f);
        setFloat (processor.apvts, ParamIDs::attack, 0.001f);
        setFloat (processor.apvts, ParamIDs::cutoff, 12000.0f);
        setFloat (processor.apvts, ParamIDs::drive, 0.0f);
    };

    EonMiniEEFProcessor centre;
    prepare (centre);
    juce::MidiBuffer centreMidi;
    centreMidi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 64);
    const auto centreBuffer = render (centre, centreMidi, sampleRate, blockSize, total);
    const float centreHz = dominantHz (centreBuffer, 8000, 16000, sampleRate);

    EonMiniEEFProcessor bent;
    prepare (bent);
    juce::MidiBuffer bentMidi;
    bentMidi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 64);
    bentMidi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 128);
    const auto bentBuffer = render (bent, bentMidi, sampleRate, blockSize, total);
    const float bentHz = dominantHz (bentBuffer, 8000, 16000, sampleRate);
    const float ratio = centreHz > 1.0f ? bentHz / centreHz : 0.0f;
    const float expected = std::pow (2.0f, 2.0f / 12.0f);
    if (std::abs (ratio - expected) / expected > 0.02f)
    {
        std::cerr << "pitch-bend ratio " << ratio << " expected " << expected << "\n";
        return 1;
    }

    auto sustainRender = [&] (bool pedal)
    {
        EonMiniEEFProcessor processor;
        prepare (processor);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        if (pedal)
            midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 200);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), static_cast<int> (0.30 * sampleRate));
        return render (processor, midi, sampleRate, blockSize, total);
    };
    const auto held = sustainRender (true);
    const auto released = sustainRender (false);
    const int tailStart = static_cast<int> (0.45 * sampleRate);
    const float heldRms = rms (held, tailStart, 4000);
    const float releasedRms = rms (released, tailStart, 4000);
    if (! (heldRms > releasedRms * 4.0f && heldRms > 0.01f))
    {
        std::cerr << "sustain rms held=" << heldRms << " released=" << releasedRms << "\n";
        return 1;
    }

    std::cout << "midi-contract: PASS ratio=" << ratio << " sustain-rms=" << heldRms << "\n";
    return 0;
}
