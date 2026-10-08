#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <string>

// Headless contract for the sample-offset MIDI path in NoteOnlySynthesiser.
// Wheel centre is 8192 and the range is fixed at +/-2 semitones. CC 64 holds
// a released note only at value >= 64. The processor is the unit under test;
// this file does not reimplement the DSP.

namespace
{
struct Rendered
{
    juce::AudioBuffer<float> buffer;
    int latency = 0;
};

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

void prepareSine (EonMiniEEFProcessor& processor)
{
    setChoice (processor.apvts, ParamIDs::osc1Wave, 3); // Sine
    setFloat (processor.apvts, ParamIDs::osc1Level, 0.8f);
    setFloat (processor.apvts, ParamIDs::osc2Level, 0.0f);
    setFloat (processor.apvts, ParamIDs::osc3Level, 0.0f);
    setFloat (processor.apvts, ParamIDs::osc4Level, 0.0f);
    setFloat (processor.apvts, ParamIDs::noiseMix, 0.0f);
    setFloat (processor.apvts, ParamIDs::unisonVoices, 1.0f);
    setFloat (processor.apvts, ParamIDs::unisonDetune, 0.0f);
    setFloat (processor.apvts, ParamIDs::unisonDrift, 0.0f);
    setFloat (processor.apvts, ParamIDs::lfoDepth, 0.0f);
    setFloat (processor.apvts, ParamIDs::lfoPitch, 0.0f);
    setFloat (processor.apvts, ParamIDs::attack, 0.001f);
    setFloat (processor.apvts, ParamIDs::decay, 0.05f);
    setFloat (processor.apvts, ParamIDs::sustain, 1.0f);
    setFloat (processor.apvts, ParamIDs::release, 0.03f);
    setFloat (processor.apvts, ParamIDs::envCurve, 0.0f);
    setFloat (processor.apvts, ParamIDs::cutoff, 12000.0f);
    setFloat (processor.apvts, ParamIDs::filterEnvAmount, 0.0f);
    setFloat (processor.apvts, ParamIDs::drive, 0.0f);
    setFloat (processor.apvts, ParamIDs::ampSat, 0.0f);
    setFloat (processor.apvts, ParamIDs::fxWet, 0.0f);
    setFloat (processor.apvts, ParamIDs::delayFeedback, 0.0f);
    setFloat (processor.apvts, ParamIDs::chorusMix, 0.0f);
    setFloat (processor.apvts, ParamIDs::reverbMix, 0.0f);
    setChoice (processor.apvts, ParamIDs::oversampling, 0);
}

Rendered render (const juce::MidiBuffer& midi, double sampleRate, int blockSize, int total)
{
    EonMiniEEFProcessor processor;
    prepareSine (processor);
    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    Rendered rendered;
    rendered.latency = processor.getLatencySamples();
    rendered.buffer.setSize (2, total);
    rendered.buffer.clear();
    for (int offset = 0; offset < total; offset += blockSize)
    {
        const int len = juce::jmin (blockSize, total - offset);
        juce::MidiBuffer slice;
        for (const auto metadata : midi)
            if (metadata.samplePosition >= offset && metadata.samplePosition < offset + len)
                slice.addEvent (metadata.getMessage(), metadata.samplePosition - offset);
        float* const* data = rendered.buffer.getArrayOfWritePointers();
        juce::AudioBuffer<float> sub (data, 2, offset, len);
        processor.processBlock (sub, slice);
    }
    return rendered;
}

float goertzelPower (const juce::AudioBuffer<float>& buffer, int start, int num,
                     double sampleRate, float frequency)
{
    // Hann window: first sidelobe is about -31 dB, so a bin 2 semitones away
    // does not inherit the rectangular-window leak of the fundamental.
    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;
    const double coeff = 2.0 * std::cos (omega);
    const double denom = juce::jmax (1, num - 1);
    double q0 = 0.0, q1 = 0.0, q2 = 0.0;
    const float* samples = buffer.getReadPointer (0);
    for (int i = 0; i < num; ++i)
    {
        const double window = 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * i / denom));
        q0 = coeff * q1 - q2 + window * samples[start + i];
        q2 = q1;
        q1 = q0;
    }
    return static_cast<float> (q1 * q1 + q2 * q2 - coeff * q1 * q2);
}

bool fundamentalDominates (const juce::AudioBuffer<float>& buffer, int start, int num,
                           double sampleRate, float frequency)
{
    const float fundamental = goertzelPower (buffer, start, num, sampleRate, frequency);
    const float second = goertzelPower (buffer, start, num, sampleRate, frequency * 2.0f);
    const float third = goertzelPower (buffer, start, num, sampleRate, frequency * 3.0f);
    return fundamental > second * 20.0f && fundamental > third * 20.0f;
}

float rms (const juce::AudioBuffer<float>& buffer, int start, int num)
{
    double sum = 0.0;
    for (int i = 0; i < num; ++i)
    {
        const float sample = buffer.getSample (0, start + i);
        sum += static_cast<double> (sample) * sample;
    }
    return static_cast<float> (std::sqrt (sum / juce::jmax (1, num)));
}

bool fail (const std::string& name, const std::string& detail)
{
    std::cerr << "midi-contract FAIL " << name << ": " << detail << "\n";
    return false;
}
}

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 64;
    constexpr int total = static_cast<int> (sampleRate);
    constexpr int note = 69;
    constexpr float centreHz = 440.0f;
    constexpr float upHz = 440.0f * 1.122462f;   // 2^(2/12), wheel 16383
    constexpr float downHz = 440.0f * 0.890899f; // 2^(-2/12), wheel 0
    const int analysisStart = static_cast<int> (0.08 * sampleRate);
    const int analysisNum = static_cast<int> (0.25 * sampleRate);

    juce::MidiBuffer centreMidi;
    centreMidi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 64);
    centreMidi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 96);
    const auto centre = render (centreMidi, sampleRate, blockSize, total);
    const int start = analysisStart + centre.latency;
    if (start + analysisNum >= total)
        return fail ("setup", "analysis window exceeds render") ? 0 : 1;

    const float centreAt440 = goertzelPower (centre.buffer, start, analysisNum, sampleRate, centreHz);
    const float centreAtUp = goertzelPower (centre.buffer, start, analysisNum, sampleRate, upHz);
    if (! fundamentalDominates (centre.buffer, start, analysisNum, sampleRate, centreHz))
        return fail ("harmonic-centre", "fundamental is not 20x H2/H3; sine setup is wrong") ? 0 : 1;
    if (! (centreAt440 > centreAtUp * 4.0f))
        return fail ("wheel-centre", "440 power " + std::to_string (centreAt440)
                     + " not above bent " + std::to_string (centreAtUp)) ? 0 : 1;

    juce::MidiBuffer upMidi;
    upMidi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 64);
    upMidi.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 128);
    const auto up = render (upMidi, sampleRate, blockSize, total);
    const float upAtUp = goertzelPower (up.buffer, start, analysisNum, sampleRate, upHz);
    const float upAt440 = goertzelPower (up.buffer, start, analysisNum, sampleRate, centreHz);
    if (! fundamentalDominates (up.buffer, start, analysisNum, sampleRate, upHz))
        return fail ("harmonic-up", "bent fundamental is not 20x H2/H3") ? 0 : 1;
    if (! (upAtUp > upAt440 * 2.0f))
        return fail ("wheel-up", "bent power " + std::to_string (upAtUp)
                     + " not above 440 " + std::to_string (upAt440)) ? 0 : 1;

    juce::MidiBuffer downMidi;
    downMidi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 64);
    downMidi.addEvent (juce::MidiMessage::pitchWheel (1, 0), 128);
    const auto down = render (downMidi, sampleRate, blockSize, total);
    const float downAtDown = goertzelPower (down.buffer, start, analysisNum, sampleRate, downHz);
    const float downAt440 = goertzelPower (down.buffer, start, analysisNum, sampleRate, centreHz);
    if (! (downAtDown > downAt440 * 2.0f))
        return fail ("wheel-down", "lower power " + std::to_string (downAtDown)
                     + " not above 440 " + std::to_string (downAt440)) ? 0 : 1;

    juce::MidiBuffer otherChannel;
    otherChannel.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 64);
    otherChannel.addEvent (juce::MidiMessage::pitchWheel (2, 16383), 128);
    const auto isolated = render (otherChannel, sampleRate, blockSize, total);
    const float isolated440 = goertzelPower (isolated.buffer, start, analysisNum, sampleRate, centreHz);
    const float isolatedUp = goertzelPower (isolated.buffer, start, analysisNum, sampleRate, upHz);
    if (! (isolated440 > isolatedUp * 4.0f))
        return fail ("channel-isolation", "channel 2 wheel bent channel 1") ? 0 : 1;

    const int noteOff = static_cast<int> (0.30 * sampleRate);
    const int tailStart = noteOff + static_cast<int> (0.12 * sampleRate) + centre.latency;
    const int tailNum = static_cast<int> (0.08 * sampleRate);
    auto sustainCase = [&] (int controllerValue)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 100);
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, controllerValue), 200);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), noteOff);
        return render (midi, sampleRate, blockSize, total);
    };
    const auto held = sustainCase (64);
    const auto boundary = sustainCase (63);
    const float heldRms = rms (held.buffer, tailStart, tailNum);
    const float boundaryRms = rms (boundary.buffer, tailStart, tailNum);
    if (! (heldRms > 0.01f && heldRms > boundaryRms * 4.0f))
        return fail ("sustain-boundary", "cc64 " + std::to_string (heldRms)
                     + " cc63 " + std::to_string (boundaryRms)) ? 0 : 1;

    std::cout << "midi-contract: PASS latency=" << centre.latency
              << " centre440=" << centreAt440
              << " up=" << upAtUp
              << " down=" << downAtDown
              << " sustain=" << heldRms << "\n";
    return 0;
}
