#include "../Source/FactoryPresets.h"
#include "../Source/PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <iostream>

int main (int argc, char** argv)
{
    const int preset = argc > 1 ? std::atoi (argv[1]) : 0;
    const juce::File out = argc > 2 ? juce::File (argv[2])
                                    : juce::File::getCurrentWorkingDirectory()
                                          .getChildFile ("eef-preset-" + juce::String (preset) + ".wav");
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 256;
    constexpr double seconds = 2.2;
    const int total = static_cast<int> (sampleRate * seconds);

    EonMiniEEFProcessor processor;
    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    FactoryPresets::apply (processor, preset);

    juce::MidiBuffer midi;
    const double noteOn = 0.15, noteDur = 0.9;
    const std::array<int, 4> notes { 48, 55, 60, 67 };
    int event = 0;
    for (int n = 0; n < 4; ++n)
    {
        const int start = static_cast<int> ((noteOn + 0.0) * sampleRate) + n * static_cast<int> (0.28 * sampleRate);
        const int end = start + static_cast<int> (noteDur * sampleRate);
        midi.addEvent (juce::MidiMessage::noteOn (1, notes[static_cast<size_t> (n)], 0.9f), start);
        midi.addEvent (juce::MidiMessage::noteOff (1, notes[static_cast<size_t> (n)]), end);
        (void) event;
    }

    juce::AudioBuffer<float> buffer (2, total);
    buffer.clear();
    const int blocks = (total + blockSize - 1) / blockSize;
    for (int b = 0; b < blocks; ++b)
    {
        const int len = juce::jmin (blockSize, total - b * blockSize);
        juce::MidiBuffer slice;
        for (const auto& m : midi)
            if (m.samplePosition >= b * blockSize && m.samplePosition < (b + 1) * blockSize)
                slice.addEvent (m.getMessage(), m.samplePosition - b * blockSize);
        float* const* data = buffer.getArrayOfWritePointers();
        juce::AudioBuffer<float> sub (data, 2, b * blockSize, len);
        processor.processBlock (sub, slice);
    }

    juce::WavAudioFormat wav;
    auto stream = out.createOutputStream();
    if (stream == nullptr)
    {
        std::cerr << "Could not create output stream for " << out.getFullPathName() << "\n";
        return 1;
    }
    auto* writer = wav.createWriterFor (stream.release(), sampleRate, 2, 24, {}, 0);
    if (writer == nullptr)
    {
        std::cerr << "Could not create WAV writer\n";
        return 1;
    }
    writer->writeFromAudioSampleBuffer (buffer, 0, total);
    delete writer;
    std::cout << "Wrote " << out.getFullPathName() << "\n";
    return 0;
}
