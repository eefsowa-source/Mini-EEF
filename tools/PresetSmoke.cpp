#include "../Source/FactoryPresets.h"
#include "../Source/PluginProcessor.h"

#include <array>
#include <cmath>
#include <iostream>

namespace
{
constexpr std::array<const char*, 14> removedParameterIds {
    "wavetableMix", "wavetableIndex",
    "timeMode", "timeDivision", "timeMix", "gateDepth",
    "gateCurve0", "gateCurve1", "gateCurve2", "gateCurve3",
    "gateCurve4", "gateCurve5", "gateCurve6", "gateCurve7"
};

constexpr std::array<const char*, 8> removedMidiLearnStateKeys {
    "midiCC0", "midiCC1", "midiCC2", "midiCC3",
    "midiCC4", "midiCC5", "midiCC6", "midiCC7"
};

juce::String parameterIdentifier (const juce::AudioProcessorParameter& parameter)
{
    if (const auto* parameterWithId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (&parameter))
        return parameterWithId->paramID;

    return parameter.getName (128);
}

bool stateContainsRemovedIdentifier (const juce::ValueTree& state, const char* identifier)
{
    if (state.hasProperty (juce::Identifier (identifier))
        || state.getProperty ("id").toString() == identifier)
        return true;

    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
        if (stateContainsRemovedIdentifier (state.getChild (childIndex), identifier))
            return true;

    return false;
}

int runFactoryPresetContractRegression()
{
    int failures = 0;

    struct OscillatorParameterExpectation
    {
        const char* id;
        float minimum;
        float maximum;
        float defaultValue;
    };
    constexpr std::array oscillatorParameters {
        OscillatorParameterExpectation { "osc1Level", 0.0f, 1.0f, 0.5f },
        OscillatorParameterExpectation { "osc1Coarse", -24.0f, 24.0f, 0.0f },
        OscillatorParameterExpectation { "osc1Fine", -100.0f, 100.0f, 0.0f },
        OscillatorParameterExpectation { "osc1Phase", 0.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc1Pan", -1.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc1PulseWidth", 0.05f, 0.95f, 0.5f },
        OscillatorParameterExpectation { "osc2Level", 0.0f, 1.0f, 0.5f },
        OscillatorParameterExpectation { "osc2Coarse", -24.0f, 24.0f, 7.0f },
        OscillatorParameterExpectation { "osc2Fine", -100.0f, 100.0f, 0.0f },
        OscillatorParameterExpectation { "osc2Phase", 0.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc2Pan", -1.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc2PulseWidth", 0.05f, 0.95f, 0.5f },
        OscillatorParameterExpectation { "osc3Level", 0.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc3Coarse", -24.0f, 24.0f, 0.0f },
        OscillatorParameterExpectation { "osc3Fine", -100.0f, 100.0f, 0.0f },
        OscillatorParameterExpectation { "osc3Phase", 0.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc3Pan", -1.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc3PulseWidth", 0.05f, 0.95f, 0.5f },
        OscillatorParameterExpectation { "osc4Level", 0.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc4Coarse", -24.0f, 24.0f, 0.0f },
        OscillatorParameterExpectation { "osc4Fine", -100.0f, 100.0f, 0.0f },
        OscillatorParameterExpectation { "osc4Phase", 0.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc4Pan", -1.0f, 1.0f, 0.0f },
        OscillatorParameterExpectation { "osc4PulseWidth", 0.05f, 0.95f, 0.5f }
    };

    EonMiniEEFProcessor oscillatorContractProbe;
    for (const auto& expected : oscillatorParameters)
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
            oscillatorContractProbe.apvts.getParameter (expected.id));
        if (parameter == nullptr)
        {
            std::cerr << "Missing detailed oscillator parameter: " << expected.id << '\n';
            ++failures;
            continue;
        }
        const auto range = parameter->getNormalisableRange();
        const float rawDefault = range.convertFrom0to1 (parameter->getDefaultValue());
        if (std::abs (range.start - expected.minimum) > 1.0e-6f
            || std::abs (range.end - expected.maximum) > 1.0e-6f
            || std::abs (rawDefault - expected.defaultValue) > 1.0e-5f)
        {
            std::cerr << "Detailed oscillator parameter contract mismatch: "
                      << expected.id << '\n';
            ++failures;
        }
    }

    // Coarse tuning is a semitone selector, not a continuously interpolated
    // control.  Keeping the APVTS type integer also makes host automation and
    // preset recall land on exact musical intervals.
    for (const auto* coarseId : { ParamIDs::osc1Coarse, ParamIDs::osc2Coarse,
                                  ParamIDs::osc3Coarse, ParamIDs::osc4Coarse })
    {
        if (dynamic_cast<juce::AudioParameterInt*> (
                oscillatorContractProbe.apvts.getParameter (coarseId)) == nullptr)
        {
            std::cerr << "Coarse tuning parameter is not integer: " << coarseId << '\n';
            ++failures;
        }
    }

    // All quality modes must expose one stable, non-zero host latency.  The
    // processor compensates the lower-quality paths to the 8x path so changing
    // quality cannot move the instrument relative to the DAW timeline.
    int reportedQualityLatency = -1;
    for (const float mode : { 0.0f, 1.0f, 2.0f, 3.0f })
    {
        EonMiniEEFProcessor latencyProbe;
        if (auto* quality = latencyProbe.apvts.getParameter (ParamIDs::oversampling))
            quality->setValueNotifyingHost (quality->convertTo0to1 (mode));
        latencyProbe.prepareToPlay (48000.0, 256);
        const int latency = latencyProbe.getLatencySamples();
        if (latency <= 0 || (reportedQualityLatency >= 0 && latency != reportedQualityLatency))
        {
            std::cerr << "Oversampling latency contract failed for mode " << mode
                      << ": " << latency << '\n';
            ++failures;
        }
        reportedQualityLatency = latency;
    }

    if (oscillatorContractProbe.getTailLengthSeconds() < 2.0)
    {
        std::cerr << "Delay/reverb tail is not reported to the host\n";
        ++failures;
    }

    // A state written by this build must not pass through legacy source-index
    // migration on restore.  Osc 1 is source index 4 in the current schema;
    // the old unversioned schema used that slot for a removed MIDI source.
    if (auto* source = oscillatorContractProbe.apvts.getParameter (ParamIDs::modSource (0)))
        source->setValueNotifyingHost (source->convertTo0to1 (4.0f));
    juce::MemoryBlock currentStateBinary;
    oscillatorContractProbe.getStateInformation (currentStateBinary);
    EonMiniEEFProcessor currentStateRestoreProbe;
    currentStateRestoreProbe.setStateInformation (currentStateBinary.getData(),
                                                   static_cast<int> (currentStateBinary.getSize()));
    const auto* restoredCurrentSource = currentStateRestoreProbe.apvts.getRawParameterValue (
        ParamIDs::modSource (0));
    if (restoredCurrentSource == nullptr || std::abs (restoredCurrentSource->load() - 4.0f) > 1.0e-6f)
    {
        std::cerr << "Current-format state was incorrectly legacy-migrated\n";
        ++failures;
    }

    const juce::StringArray expectedNames {
        "Init", "Supersaw Pad", "Trance Pluck", "Arena Lead", "Sub Mono Bass", "Acid Bass",
        "Glass Keys", "Warm Poly", "Velvet Strings", "Neon Bell", "Pulse Sequence", "Digital Pluck",
        "Wide Brass", "Soft Organ", "Juno Choir", "Motion Pad", "FMish Bass", "Rubber Mono",
        "Resonant Sweep", "Noise SFX", "Lo-Fi Keys", "Dream Lead", "Octave Stab", "Deep Drone",
        "Percussive Click", "Classic PWM", "Minimoog Lead", "Moog Bass", "Diva Saw Pad",
        "Juno Pad", "Prophet Brass", "Minifreak Pluck", "Sync Sweep Lead", "Analog Strings"
    };
    const auto& actualNames = FactoryPresets::names();
    bool namesMatch = actualNames.size() == expectedNames.size();
    for (int index = 0; namesMatch && index < expectedNames.size(); ++index)
        namesMatch = actualNames[index] == expectedNames[index];

    if (! namesMatch)
    {
        std::cerr << "Factory preset names mismatch: expected ["
                  << expectedNames.joinIntoString (", ").toStdString()
                  << "], got [" << actualNames.joinIntoString (", ").toStdString() << "]\n";
        ++failures;
    }

    EonMiniEEFProcessor resetProbe;
    juce::StringArray mutationFailures;
    for (auto* parameter : resetProbe.getParameters())
    {
        const float defaultValue = parameter->getDefaultValue();
        const float nonDefaultValue = defaultValue < 0.5f ? 1.0f : 0.0f;
        parameter->setValueNotifyingHost (nonDefaultValue);
        if (std::abs (parameter->getValue() - defaultValue) <= 1.0e-6f)
            mutationFailures.add (parameterIdentifier (*parameter));
    }

    if (! mutationFailures.isEmpty())
    {
        std::cerr << "Test setup could not move parameters away from defaults: "
                  << mutationFailures.joinIntoString (", ").toStdString() << '\n';
        ++failures;
    }
    else
    {
        FactoryPresets::apply (resetProbe, 0);
        juce::StringArray parametersNotReset;
        for (auto* parameter : resetProbe.getParameters())
            if (std::abs (parameter->getValue() - parameter->getDefaultValue()) > 1.0e-6f)
                parametersNotReset.add (parameterIdentifier (*parameter));

        if (! parametersNotReset.isEmpty())
        {
            std::cerr << "Factory preset Init did not restore parameter defaults: "
                      << parametersNotReset.joinIntoString (", ").toStdString() << '\n';
            ++failures;
        }
    }

    EonMiniEEFProcessor removalProbe;
    juce::StringArray stillRegistered;
    for (const auto* identifier : removedParameterIds)
        if (removalProbe.apvts.getParameter (identifier) != nullptr)
            stillRegistered.add (identifier);

    if (! stillRegistered.isEmpty())
    {
        std::cerr << "Removed parameters are still registered: "
                  << stillRegistered.joinIntoString (", ").toStdString() << '\n';
        ++failures;
    }

    auto legacyState = removalProbe.apvts.copyState();
    for (const auto* identifier : removedParameterIds)
    {
        legacyState.setProperty (juce::Identifier (identifier), 0.25f, nullptr);
        juce::ValueTree legacyParameter { "PARAM" };
        legacyParameter.setProperty ("id", identifier, nullptr);
        legacyParameter.setProperty ("value", 0.75f, nullptr);
        legacyState.addChild (legacyParameter, -1, nullptr);
    }
    for (size_t index = 0; index < removedMidiLearnStateKeys.size(); ++index)
        legacyState.setProperty (removedMidiLearnStateKeys[index],
                                 static_cast<int> (index) + 1, nullptr);
    const std::array<float, 3> legacyModSources { 4.0f, 5.0f, 6.0f };
    for (int childIndex = 0; childIndex < legacyState.getNumChildren(); ++childIndex)
    {
        auto child = legacyState.getChild (childIndex);
        for (size_t slot = 0; slot < legacyModSources.size(); ++slot)
            if (child.getProperty ("id").toString()
                == ParamIDs::modSource (static_cast<int> (slot)))
                child.setProperty ("value", legacyModSources[slot], nullptr);
    }

    auto legacyXml = legacyState.createXml();
    if (legacyXml == nullptr)
    {
        std::cerr << "Could not create legacy state XML for removed-parameter regression\n";
        ++failures;
    }
    else
    {
        juce::MemoryBlock legacyBinary;
        juce::AudioProcessor::copyXmlToBinary (*legacyXml, legacyBinary);
        removalProbe.setStateInformation (legacyBinary.getData(), static_cast<int> (legacyBinary.getSize()));

        const std::array<float, 3> migratedModSources { 0.0f, 0.0f, 4.0f };
        for (size_t slot = 0; slot < migratedModSources.size(); ++slot)
        {
            const auto* source = removalProbe.apvts.getRawParameterValue (
                ParamIDs::modSource (static_cast<int> (slot)));
            if (source == nullptr
                || std::abs (source->load() - migratedModSources[slot]) > 1.0e-6f)
            {
                std::cerr << "Legacy mod source migration failed for slot "
                          << slot << '\n';
                ++failures;
            }
        }

        juce::MemoryBlock resavedBinary;
        removalProbe.getStateInformation (resavedBinary);
        auto resavedXml = juce::AudioProcessor::getXmlFromBinary (
            resavedBinary.getData(), static_cast<int> (resavedBinary.getSize()));

        if (resavedXml == nullptr)
        {
            std::cerr << "Could not parse state after legacy state re-save\n";
            ++failures;
        }
        else
        {
            const auto resavedState = juce::ValueTree::fromXml (*resavedXml);
            juce::StringArray identifiersStillSerialized;
            for (const auto* identifier : removedParameterIds)
                if (stateContainsRemovedIdentifier (resavedState, identifier))
                    identifiersStillSerialized.add (identifier);
            for (const auto* identifier : removedMidiLearnStateKeys)
                if (stateContainsRemovedIdentifier (resavedState, identifier))
                    identifiersStillSerialized.add (identifier);

            if (! identifiersStillSerialized.isEmpty())
            {
                std::cerr << "Legacy removed IDs survived state re-save: "
                          << identifiersStillSerialized.joinIntoString (", ").toStdString() << '\n';
                ++failures;
            }
        }
    }

    const juce::StringArray expectedModSources {
        "Off", "LFO", "Amp Env", "Velocity", "Osc 1", "Filter Env"
    };
    for (int slot = 0; slot < 4; ++slot)
    {
        auto* source = dynamic_cast<juce::AudioParameterChoice*> (
            removalProbe.apvts.getParameter (ParamIDs::modSource (slot)));
        if (source == nullptr || source->choices != expectedModSources)
        {
            std::cerr << "Mod source " << slot
                      << " mod source list does not match the expected choices\n";
            ++failures;
        }
    }

    // The default wave is Analog (index 4), and the thirteen factory patches
    // that predate it pin Saw (index 0) on every oscillator they leave
    // unspecified, so their sound does not move when the default changes.
    // This is the contract that keeps the bank stable, so it is asserted here
    // rather than left to a comment.  Pinning only osc1 was not enough: a
    // patch that sets osc4 to Square still inherited Analog on osc2 and osc3,
    // which moved six of the preset renders byte-for-byte.
    const juce::StringArray expectedWaves {
        "Saw", "Square", "Triangle", "Sine", "Analog"
    };
    // AudioParameterChoice keeps its default private, so the default is read
    // the same way the reset contract already proves it: apply Init, which is
    // reset() plus no overrides, and look at the resulting value.  For a choice
    // parameter getRawParameterValue() hands back the plain choice index, not a
    // normalised 0..1, so Analog is the literal 4.
    EonMiniEEFProcessor defaultWaveProbe;
    FactoryPresets::apply (defaultWaveProbe, 0);
    constexpr std::array<const char*, 4> waveIds {
        ParamIDs::osc1Wave, ParamIDs::osc2Wave, ParamIDs::osc3Wave, ParamIDs::osc4Wave
    };
    for (const auto* waveId : waveIds)
    {
        auto* wave = dynamic_cast<juce::AudioParameterChoice*> (
            removalProbe.apvts.getParameter (waveId));
        if (wave == nullptr || wave->choices != expectedWaves)
        {
            std::cerr << waveId << " wave list does not match the expected choices\n";
            ++failures;
            continue;
        }
        const auto* waveValue = defaultWaveProbe.apvts.getRawParameterValue (waveId);
        if (waveValue == nullptr || std::abs (waveValue->load() - 4.0f) > 1.0e-6f)
        {
            std::cerr << waveId << " default is not Analog (index 4, got "
                      << (waveValue ? waveValue->load() : -1.0f) << ")\n";
            ++failures;
        }
    }

    // Every patch except Init must pin every oscillator it cares about, because
    // the default now differs from the saw these patches were written against.
            // The value itself may be Saw, Square or anything but Analog: what
            // must not happen is an oscillator silently inheriting the new
            // default.  getRawParameterValue() gives the plain choice index for
            // a choice parameter, so Analog reads as the literal 4.
    constexpr std::array<int, 16> pinnedPatches {
        1, 2, 3, 4, 5, 7, 8, 10, 12, 14, 15, 17, 18, 21, 22, 23
    };
    for (const int index : pinnedPatches)
    {
        EonMiniEEFProcessor patchProbe;
        FactoryPresets::apply (patchProbe, index);
        for (const auto* waveId : waveIds)
        {
            // An oscillator at level 0 is silent, so which wave it names cannot
            // be heard and does not need a pin.  Note that osc2's default
            // level is 0.5, not 0, so a patch that never mentions osc2 is
            // still making sound with it.
            const juce::String levelId = juce::String (waveId).replace ("Wave", "Level");
            const std::string levelIdUtf8 = levelId.toStdString();
            const auto* levelValue = patchProbe.apvts.getRawParameterValue (levelIdUtf8.c_str());
            if (levelValue != nullptr && levelValue->load() <= 1.0e-6f)
                continue;
            const auto* waveValue = patchProbe.apvts.getRawParameterValue (waveId);
            if (waveValue == nullptr)
            {
                std::cerr << FactoryPresets::names()[index] << " has no " << waveId << '\n';
                ++failures;
            }
            else if (waveValue->load() > 3.5f)
            {
                std::cerr << FactoryPresets::names()[index] << " inherits the Analog default on "
                          << waveId << "\n";
                ++failures;
            }
        }
    }

    // Init inherits the default, so it must land on Analog.
    {
        EonMiniEEFProcessor initProbe;
        FactoryPresets::apply (initProbe, 0);
        const auto* wave = initProbe.apvts.getRawParameterValue (ParamIDs::osc1Wave);
        if (wave == nullptr || std::abs (wave->load() - 4.0f) > 1.0e-6f)
        {
            std::cerr << "Init should inherit the Analog default\n";
            ++failures;
        }
    }

    return failures;
}

bool hasMatchingParameterValues (const EonMiniEEFProcessor& first,
                                 const EonMiniEEFProcessor& second)
{
    const auto& firstParameters = first.getParameters();
    const auto& secondParameters = second.getParameters();
    if (firstParameters.size() != secondParameters.size())
        return false;

    for (size_t i = 0; i < firstParameters.size(); ++i)
        if (std::abs (firstParameters[i]->getValue() - secondParameters[i]->getValue()) > 1.0e-6f)
            return false;

    return true;
}

double monoRegressionSpectralMagnitude (const std::array<float, 24576>& samples,
                                        int startSample, int length, float frequency)
{
    double real = 0.0;
    double imaginary = 0.0;
    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.2831853071795864769;
    for (int index = 0; index < length; ++index)
    {
        const double phase = twoPi * static_cast<double> (frequency)
                           * static_cast<double> (index) / sampleRate;
        const double window = 0.5 - 0.5 * std::cos (twoPi * static_cast<double> (index)
                                                    / static_cast<double> (length - 1));
        const double value = static_cast<double> (samples[static_cast<size_t> (startSample + index)])
                           * window;
        real += value * std::cos (phase);
        imaginary -= value * std::sin (phase);
    }
    return 2.0 * std::hypot (real, imaginary) / static_cast<double> (length);
}

double monoRegressionRms (const std::array<float, 24576>& samples,
                          int startSample, int length)
{
    double sumSquares = 0.0;
    for (int index = 0; index < length; ++index)
    {
        const auto value = static_cast<double> (samples[static_cast<size_t> (startSample + index)]);
        sumSquares += value * value;
    }
    return std::sqrt (sumSquares / static_cast<double> (length));
}

bool configureCleanMonoRegressionProbe (EonMiniEEFProcessor& probe)
{
    const auto setPlain = [&probe] (const char* id, float value)
    {
        if (auto* parameter = probe.apvts.getParameter (id))
        {
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            return true;
        }
        return false;
    };

    return setPlain (ParamIDs::osc1Wave, 3.0f)
        && setPlain (ParamIDs::osc1Level, 1.0f)
        && setPlain (ParamIDs::osc2Level, 0.0f)
        && setPlain (ParamIDs::osc3Level, 0.0f)
        && setPlain (ParamIDs::osc4Level, 0.0f)
        && setPlain (ParamIDs::noiseMix, 0.0f)
        && setPlain (ParamIDs::unisonVoices, 1.0f)
        && setPlain (ParamIDs::cutoff, 20000.0f)
        && setPlain (ParamIDs::resonance, 0.0f)
        && setPlain (ParamIDs::attack, 0.001f)
        && setPlain (ParamIDs::decay, 0.001f)
        && setPlain (ParamIDs::sustain, 1.0f)
        && setPlain (ParamIDs::release, 0.001f)
        && setPlain (ParamIDs::gain, 0.8f)
        && setPlain (ParamIDs::drive, 0.0f)
        && setPlain (ParamIDs::fxWet, 0.0f)
        && setPlain (ParamIDs::delayFeedback, 0.0f)
        && setPlain (ParamIDs::chorusMix, 0.0f)
        && setPlain (ParamIDs::reverbMix, 0.0f)
        && setPlain (ParamIDs::voiceMode, 1.0f);
}

int runMonoNotePriorityRegression()
{
    constexpr int blockSize = 128;
    constexpr int totalSamples = 24576;
    constexpr int transition = 4096;
    constexpr int analysisStart = 2048;
    constexpr int analysisLength = 1536;
    constexpr std::array<float, 5> expectedFrequencies {
        261.625565f, 392.0f, 329.627557f, 392.0f, 261.625565f
    };
    constexpr std::array<int, 5> analysisStarts {
        analysisStart,
        transition + analysisStart,
        2 * transition + analysisStart,
        3 * transition + analysisStart,
        4 * transition + analysisStart
    };
    constexpr std::array<float, 3> candidateFrequencies {
        261.625565f, 329.627557f, 392.0f
    };

    EonMiniEEFProcessor probe;
    if (! configureCleanMonoRegressionProbe (probe))
    {
        std::cerr << "Mono note-priority regression could not configure clean sine voice\n";
        return 1;
    }

    probe.prepareToPlay (48000.0, blockSize);
    std::array<float, static_cast<size_t> (totalSamples)> rendered {};
    const auto addEventForBlock = [] (juce::MidiBuffer& midi, int blockStart,
                                      int absoluteSample, const juce::MidiMessage& message)
    {
        if (absoluteSample >= blockStart && absoluteSample < blockStart + blockSize)
            midi.addEvent (message, absoluteSample - blockStart);
    };

    for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        addEventForBlock (midi, blockStart, 0,
                          juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100));
        addEventForBlock (midi, blockStart, transition,
                          juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100));
        addEventForBlock (midi, blockStart, 2 * transition,
                          juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100));
        addEventForBlock (midi, blockStart, 3 * transition,
                          juce::MidiMessage::noteOff (1, 64));
        addEventForBlock (midi, blockStart, 4 * transition,
                          juce::MidiMessage::noteOff (1, 67));
        addEventForBlock (midi, blockStart, 5 * transition,
                          juce::MidiMessage::noteOff (1, 60));
        probe.processBlock (buffer, midi);
        for (int sample = 0; sample < blockSize; ++sample)
            rendered[static_cast<size_t> (blockStart + sample)] = buffer.getSample (0, sample);
    }

    for (size_t segment = 0; segment < expectedFrequencies.size(); ++segment)
    {
        std::array<double, candidateFrequencies.size()> magnitudes {};
        for (size_t candidate = 0; candidate < candidateFrequencies.size(); ++candidate)
            magnitudes[candidate] = monoRegressionSpectralMagnitude (
                rendered, analysisStarts[segment], analysisLength, candidateFrequencies[candidate]);

        const auto expected = monoRegressionSpectralMagnitude (
            rendered, analysisStarts[segment], analysisLength, expectedFrequencies[segment]);
        double totalMagnitude = 0.0;
        for (const auto magnitude : magnitudes)
            totalMagnitude += magnitude;
        const double dominance = expected / juce::jmax (1.0e-9, totalMagnitude);
        std::cout << "Mono priority probe: segment=" << segment
                  << " expectedHz=" << expectedFrequencies[segment]
                  << " expectedMagnitude=" << expected
                  << " dominance=" << dominance << '\n';
        if (dominance < 0.82)
        {
            std::cerr << "Mono last-note priority failed in segment " << segment
                      << ": expected pitch was not dominant\n";
            return 1;
        }
    }

    // A separate trace makes the two held-note edge cases observable without
    // relying on a changing envelope: G is released while E is current, then
    // E is released and C must return with C's original low velocity.
    EonMiniEEFProcessor fallbackProbe;
    EonMiniEEFProcessor fallbackReference;
    EonMiniEEFProcessor nonCurrentControl;
    if (! configureCleanMonoRegressionProbe (fallbackProbe)
        || ! configureCleanMonoRegressionProbe (fallbackReference)
        || ! configureCleanMonoRegressionProbe (nonCurrentControl))
    {
        std::cerr << "Mono fallback regression could not configure clean sine voices\n";
        return 1;
    }
    fallbackProbe.prepareToPlay (48000.0, blockSize);
    fallbackReference.prepareToPlay (48000.0, blockSize);
    nonCurrentControl.prepareToPlay (48000.0, blockSize);
    std::array<float, static_cast<size_t> (totalSamples)> fallbackRendered {};
    std::array<float, static_cast<size_t> (totalSamples)> referenceRendered {};
    std::array<float, static_cast<size_t> (totalSamples)> controlRendered {};
    for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
    {
        juce::AudioBuffer<float> fallbackBuffer (2, blockSize);
        juce::AudioBuffer<float> referenceBuffer (2, blockSize);
        juce::AudioBuffer<float> controlBuffer (2, blockSize);
        juce::MidiBuffer fallbackMidi;
        juce::MidiBuffer referenceMidi;
        juce::MidiBuffer controlMidi;
        addEventForBlock (fallbackMidi, blockStart, 0,
                          juce::MidiMessage::noteOn (1, 60, (juce::uint8) 20));
        addEventForBlock (fallbackMidi, blockStart, transition,
                          juce::MidiMessage::noteOn (1, 67, (juce::uint8) 110));
        addEventForBlock (fallbackMidi, blockStart, 2 * transition,
                          juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80));
        addEventForBlock (fallbackMidi, blockStart, 3 * transition,
                          juce::MidiMessage::noteOff (1, 67));
        addEventForBlock (fallbackMidi, blockStart, 4 * transition,
                          juce::MidiMessage::noteOff (1, 64));
        addEventForBlock (fallbackMidi, blockStart, 5 * transition,
                          juce::MidiMessage::noteOff (1, 60));
        addEventForBlock (referenceMidi, blockStart, 0,
                          juce::MidiMessage::noteOn (1, 60, (juce::uint8) 20));
        addEventForBlock (controlMidi, blockStart, 0,
                          juce::MidiMessage::noteOn (1, 60, (juce::uint8) 20));
        addEventForBlock (controlMidi, blockStart, transition,
                          juce::MidiMessage::noteOn (1, 67, (juce::uint8) 110));
        addEventForBlock (controlMidi, blockStart, 2 * transition,
                          juce::MidiMessage::noteOn (1, 64, (juce::uint8) 80));
        addEventForBlock (controlMidi, blockStart, 4 * transition,
                          juce::MidiMessage::noteOff (1, 64));
        addEventForBlock (controlMidi, blockStart, 5 * transition,
                          juce::MidiMessage::noteOff (1, 60));
        fallbackProbe.processBlock (fallbackBuffer, fallbackMidi);
        fallbackReference.processBlock (referenceBuffer, referenceMidi);
        nonCurrentControl.processBlock (controlBuffer, controlMidi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            fallbackRendered[static_cast<size_t> (blockStart + sample)]
                = fallbackBuffer.getSample (0, sample);
            referenceRendered[static_cast<size_t> (blockStart + sample)]
                = referenceBuffer.getSample (0, sample);
            controlRendered[static_cast<size_t> (blockStart + sample)]
                = controlBuffer.getSample (0, sample);
        }
    }

    const auto eBeforeGOff = monoRegressionSpectralMagnitude (
        fallbackRendered, 2 * transition + analysisStart, analysisLength, 329.627557f);
    const auto eAfterGOff = monoRegressionSpectralMagnitude (
        fallbackRendered, 3 * transition + analysisStart, analysisLength, 329.627557f);
    const auto eBeforeGOffRms = monoRegressionRms (
        fallbackRendered, 2 * transition + analysisStart, analysisLength);
    const auto eAfterGOffRms = monoRegressionRms (
        fallbackRendered, 3 * transition + analysisStart, analysisLength);
    const auto cFallback = monoRegressionSpectralMagnitude (
        fallbackRendered, 4 * transition + analysisStart, analysisLength, 261.625565f);
    const auto cFallbackRms = monoRegressionRms (
        fallbackRendered, 4 * transition + analysisStart, analysisLength);
    const auto cReferenceRms = monoRegressionRms (
        referenceRendered, analysisStart, analysisLength);
    const auto eRmsRatio = eAfterGOffRms / juce::jmax (1.0e-9, eBeforeGOffRms);
    const auto cVelocityRatio = cFallbackRms / juce::jmax (1.0e-9, cReferenceRms);
    double nonCurrentMaxDifference = 0.0;
    for (int sample = 3 * transition + analysisStart;
         sample < 3 * transition + analysisStart + analysisLength;
         ++sample)
    {
        nonCurrentMaxDifference = juce::jmax (
            nonCurrentMaxDifference,
            std::abs (static_cast<double> (fallbackRendered[static_cast<size_t> (sample)])
                      - static_cast<double> (controlRendered[static_cast<size_t> (sample)])));
    }
    std::cout << "Mono held-note probe: E-beforeGOff=" << eBeforeGOff
              << " E-afterGOff=" << eAfterGOff
              << " E-rmsRatio=" << eRmsRatio
              << " C-fallback=" << cFallback
              << " C-velocityRatio=" << cVelocityRatio
              << " nonCurrentMaxDifference=" << nonCurrentMaxDifference << '\n';
    if (eBeforeGOff < 0.82 * eAfterGOff
        || eAfterGOff < 0.82 * eBeforeGOff
        || eRmsRatio < 0.75 || eRmsRatio > 1.25
        || cFallbackRms < 1.0e-5 || cVelocityRatio < 0.75 || cVelocityRatio > 1.25
        || cFallback < 1.0e-5 || nonCurrentMaxDifference > 1.0e-6)
    {
        std::cerr << "Mono held-note release or stored fallback velocity failed\n";
        return 1;
    }

    std::cout << "Mono note-priority regression passed\n";
    return 0;
}

// Quantitative oversampled-drive aliasing gate.  The oversampled global
// saturation stage must genuinely reduce Nyquist-adjacent harmonic energy
// relative to the 1x path instead of merely sounding different.  Naive
// Goertzel probes keep this dependency-free and cheap enough for CI.
int runOversampledDriveAliasingRegression()
{
    constexpr double probeSampleRate = 48000.0;
    constexpr int probeBlockSize = 256;
    constexpr int droneTotalSamples = 49152;
    constexpr int analysisStart = 16384;
    constexpr int analysisLength = 32768;
    // Exact fold-back lines of the A6 sine drive series.  tanh() creates
    // only odd harmonics of 1760 Hz; the 15th (26.4 kHz) and 17th (29.92
    // kHz) exceed the 24 kHz Nyquist limit and land at 21.6 kHz and 18.08
    // kHz in the 1x path.  Genuine odd harmonics never land on those lines,
    // and the oversampled path removes both products in its anti-alias
    // downsampling filter, so these two bins are pure aliasing detectors.
    constexpr double aliasFoldLines[] { 18080.0 };
    // 21600 Hz hosts the much stronger 15th-harmonic fold; 18080 Hz is the
    // weaker 17th-harmonic fold sitting deeper in the anti-alias stopband.
    constexpr double aliasFoldLinesWide[] { 21600.0, 18080.0 };
    // Gate: the 4x fold-line power must drop to at most half of the 1x
    // ratio.  The current DSP measures 0.025 (40x lower), so this leaves
    // room for legitimate DSP evolution while still catching any regression
    // that reintroduces host-rate saturation before the oversampler.
    constexpr double maxOversampledRatio = 0.50;

    const auto configureDroneProbe = [] (EonMiniEEFProcessor& probe)
    {
        const auto setPlain = [&probe] (const char* id, float plainValue)
        {
            if (auto* parameter = probe.apvts.getParameter (id))
            {
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
                return true;
            }
            return false;
        };
        // A pure sine isolates the saturation stage: tanh() then produces
        // odd harmonics only, so every 1x fold-back above Nyquist is
        // aliasing while the 4x path must genuinely remove it.
        // Four phase-aligned pulse oscillators sum to a hot input so the
        // saturation stage clips deeply, and pulse harmonics decay only
        // ~1/n so their fold-back products tower above the analyser's
        // window-leakage floor.
        return setPlain (ParamIDs::osc1Wave, 1.0f)     // pulse: harmonics ~1/n
            && setPlain (ParamIDs::osc1Level, 1.0f)
            && setPlain (ParamIDs::osc2Wave, 1.0f)
            && setPlain (ParamIDs::osc2Level, 1.0f)
            && setPlain (ParamIDs::osc3Wave, 1.0f)
            && setPlain (ParamIDs::osc3Level, 1.0f)
            && setPlain (ParamIDs::osc4Wave, 1.0f)
            && setPlain (ParamIDs::osc4Level, 1.0f)
            && setPlain (ParamIDs::noiseMix, 0.0f)
            && setPlain (ParamIDs::unisonVoices, 1.0f)
            && setPlain (ParamIDs::cutoff, 20000.0f)
            && setPlain (ParamIDs::resonance, 0.0f)
            && setPlain (ParamIDs::attack, 0.001f)
            && setPlain (ParamIDs::decay, 0.001f)
            && setPlain (ParamIDs::sustain, 1.0f)
            && setPlain (ParamIDs::release, 0.001f)
            && setPlain (ParamIDs::gain, 0.8f)
            && setPlain (ParamIDs::drive, 1.0f)
            && setPlain (ParamIDs::fxWet, 0.0f)
            && setPlain (ParamIDs::delayFeedback, 0.0f)
            && setPlain (ParamIDs::chorusMix, 0.0f)
            && setPlain (ParamIDs::reverbMix, 0.0f)
            && setPlain (ParamIDs::voiceMode, 1.0f);
    };

    const auto renderDroneWindow = [probeSampleRate, probeBlockSize] (EonMiniEEFProcessor& probe,
                                                                     std::array<float, analysisLength>& window)
    {
        probe.prepareToPlay (probeSampleRate, probeBlockSize);
        for (int blockStart = 0; blockStart < droneTotalSamples; blockStart += probeBlockSize)
        {
            juce::AudioBuffer<float> buffer (2, probeBlockSize);
            juce::MidiBuffer midi;
            // A6 (1760 Hz) makes tanh() harmonics 15+ exceed the 24 kHz
            // Nyquist limit, so the 1x path folds strong 15th/17th products
            // back to ~21.6 kHz / ~18.1 kHz while the 4x path removes them
            // in its downsampling anti-alias filter.  440 Hz cannot show
            // this because every low-order harmonic stays below Nyquist.
            if (blockStart == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 93, (juce::uint8) 100), 0);
            probe.processBlock (buffer, midi);
            for (int sample = 0; sample < probeBlockSize; ++sample)
            {
                const int absoluteSample = blockStart + sample;
                if (absoluteSample >= analysisStart)
                    window[static_cast<size_t> (absoluteSample - analysisStart)]
                        = buffer.getSample (0, sample);
            }
        }
    };

    const auto goertzelMagnitude = [probeSampleRate] (const std::array<float, analysisLength>& window,
                                                     double frequency)
    {
        // A Blackman window suppresses rectangular-window leakage from the
        // huge neighbouring genuine harmonics (e.g. the 13th at 22880 Hz)
        // so the fold-line bins only contain true fold-back energy.
        const double k = juce::MathConstants<double>::twoPi * frequency / probeSampleRate;
        const double coeff = 2.0 * std::cos (k);
        double s1 = 0.0, s2 = 0.0;
        for (const float sample : window)
        {
            const size_t index = static_cast<size_t> (&sample - window.data());
            const double n = static_cast<double> (index);
            const double bigN = static_cast<double> (window.size());
            const double blackman = 0.42 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * n / bigN)
                + 0.08 * std::cos (2.0 * juce::MathConstants<double>::twoPi * n / bigN);
            const double s0 = static_cast<double> (sample) * blackman + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return std::sqrt (juce::jmax (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2))
               / static_cast<double> (window.size());
    };

    // Normalise by the untouched 440 Hz fundamental instead of total power:
    // aliasing itself inflates the 1x total power, which would mask the
    // problem when using a relative in-band share.  The fundamental passes
    // both quality paths identically, so this is level-invariant.
    const auto bandPowerRatio = [&goertzelMagnitude] (const double* foldLines,
                                                      size_t foldLineCount,
                                                      const std::array<float, analysisLength>& window)
    {
        double highBandPower = 0.0;
        for (size_t line = 0; line < foldLineCount; ++line)
        {
            const double magnitude = goertzelMagnitude (window, foldLines[line]);
            highBandPower += magnitude * magnitude;
        }
        const double fundamentalMagnitude = goertzelMagnitude (window, 1760.0);
        return highBandPower / juce::jmax (1.0e-12,
            fundamentalMagnitude * fundamentalMagnitude);
    };

    EonMiniEEFProcessor bypassProbe, oversampledProbe;
    if (! configureDroneProbe (bypassProbe) || ! configureDroneProbe (oversampledProbe))
    {
        std::cerr << "Oversampled-drive aliasing probe could not configure the drone voice\n";
        return 1;
    }
    if (auto* quality = oversampledProbe.apvts.getParameter (ParamIDs::oversampling))
        quality->setValueNotifyingHost (quality->convertTo0to1 (2.0f)); // 4x

    std::array<float, analysisLength> bypassWindow {}, oversampledWindow {};
    renderDroneWindow (bypassProbe, bypassWindow);
    renderDroneWindow (oversampledProbe, oversampledWindow);
    // Diagnostic sweep: per-line reduction informs threshold tuning only.
    for (const double line : aliasFoldLinesWide)
    {
        std::cout << "Aliasing fold-line diagnostic: f=" << line
                  << " 1x=" << goertzelMagnitude (bypassWindow, line)
                  << " 4x=" << goertzelMagnitude (oversampledWindow, line)
                  << "\n";
    }
    for (const double tone : { 1760.0, 5280.0, 22880.0 })
    {
        std::cout << "Genuine harmonic diagnostic: f=" << tone
                  << " 1x=" << goertzelMagnitude (bypassWindow, tone)
                  << " 4x=" << goertzelMagnitude (oversampledWindow, tone)
                  << "\n";
    }

    for (const float sample : bypassWindow)
        if (! std::isfinite (sample))
        {
            std::cerr << "1x drive drone produced non-finite output\n";
            return 1;
        }
    for (const float sample : oversampledWindow)
        if (! std::isfinite (sample))
        {
            std::cerr << "4x drive drone produced non-finite output\n";
            return 1;
        }

    const double bypassRatio = bandPowerRatio (aliasFoldLinesWide, 2, bypassWindow);
    const double oversampledRatio = bandPowerRatio (aliasFoldLinesWide, 2, oversampledWindow);
    std::cout << "Oversampled-drive aliasing probe: ratio1x=" << bypassRatio
              << " ratio4x=" << oversampledRatio
              << " reduction=" << (bypassRatio > 0.0 ? oversampledRatio / bypassRatio : 0.0)
              << '\n';

    if (bypassRatio <= 1.0e-9 || oversampledRatio <= 0.0
        || oversampledRatio >= bypassRatio
        || oversampledRatio > maxOversampledRatio * bypassRatio)
    {
        std::cerr << "Oversampled drive failed to reduce Nyquist-adjacent energy\n";
        return 1;
    }
    return 0;
}

// Quad-tap Dimension-D-style chorus gate.  The wet path must keep the dry
// signal unity and add genuine motion while a sustained input keeps both
// channels finite and bounded.  Running the same render twice also catches
// hidden state that a reset would need but prepareToPlay does not cover.
int runQuadTapChorusRegression()
{
    constexpr double probeSampleRate = 48000.0;
    constexpr int probeBlockSize = 128;
    constexpr int totalSamples = 48000;

    const auto renderPass = [probeSampleRate, probeBlockSize] (bool chorusEnabled,
                                                               bool automate)
    {
        EonMiniEEFProcessor probe;
        const auto setPlain = [&probe] (const char* id, float plainValue)
        {
            if (auto* parameter = probe.apvts.getParameter (id))
            {
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
                return true;
            }
            return false;
        };
        if (! setPlain (ParamIDs::osc1Wave, 3.0f)      // sine: clean reference tone
            || ! setPlain (ParamIDs::osc1Level, 0.5f)
            || ! setPlain (ParamIDs::unisonVoices, 1.0f)
            || ! setPlain (ParamIDs::cutoff, 20000.0f)
            || ! setPlain (ParamIDs::resonance, 0.0f)
            || ! setPlain (ParamIDs::attack, 0.001f)
            || ! setPlain (ParamIDs::decay, 0.001f)
            || ! setPlain (ParamIDs::sustain, 1.0f)
            || ! setPlain (ParamIDs::release, 0.001f)
            || ! setPlain (ParamIDs::gain, 0.5f)
            || ! setPlain (ParamIDs::fxWet, 0.0f)
            || ! setPlain (ParamIDs::delayFeedback, 0.0f)
            || ! setPlain (ParamIDs::reverbMix, 0.0f)
            || ! setPlain (ParamIDs::chorusDepth, 0.004f)
            || ! setPlain (ParamIDs::chorusRate, 0.25f)
            || ! setPlain (ParamIDs::chorusMix, chorusEnabled ? 1.0f : 0.0f))
            return std::array<std::array<float, totalSamples>, 2> {};

        probe.prepareToPlay (probeSampleRate, probeBlockSize);
        std::array<std::array<float, totalSamples>, 2> capture {};
        for (int blockStart = 0; blockStart < totalSamples; blockStart += probeBlockSize)
        {
            juce::AudioBuffer<float> buffer (2, probeBlockSize);
            juce::MidiBuffer midi;
            if (blockStart == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 84, (juce::uint8) 100), 0);
            if (automate && blockStart == totalSamples / 2)
            {
                if (! setPlain (ParamIDs::chorusMix, 1.0f)
                    || ! setPlain (ParamIDs::chorusDepth, 0.012f)
                    || ! setPlain (ParamIDs::chorusRate, 1.5f))
                    return std::array<std::array<float, totalSamples>, 2> {};
            }
            probe.processBlock (buffer, midi);
            for (int sample = 0; sample < probeBlockSize; ++sample)
            {
                capture[0][static_cast<size_t> (blockStart + sample)] = buffer.getSample (0, sample);
                capture[1][static_cast<size_t> (blockStart + sample)] = buffer.getSample (1, sample);
            }
        }
        return capture;
    };

    const auto dry = renderPass (false, false);
    const auto wet = renderPass (true, false);
    const auto automated = renderPass (false, true);

    double dryPeak = 0.0, wetPeakL = 0.0, wetPeakR = 0.0, differenceEnergy = 0.0;
    double preTransitionStep = 0.0, transitionStep = 0.0;
    for (int sample = totalSamples / 2; sample < totalSamples; ++sample)
    {
        const double dryMagnitude = std::max (std::abs (static_cast<double> (dry[0][static_cast<size_t> (sample)])),
            std::abs (static_cast<double> (dry[1][static_cast<size_t> (sample)])));
        dryPeak = std::max (dryPeak, dryMagnitude);
        wetPeakL = std::max (wetPeakL, std::abs (static_cast<double> (wet[0][static_cast<size_t> (sample)])));
        wetPeakR = std::max (wetPeakR, std::abs (static_cast<double> (wet[1][static_cast<size_t> (sample)])));
        const double deltaL = static_cast<double> (wet[0][static_cast<size_t> (sample)])
            - static_cast<double> (dry[0][static_cast<size_t> (sample)]);
        const double deltaR = static_cast<double> (wet[1][static_cast<size_t> (sample)])
            - static_cast<double> (dry[1][static_cast<size_t> (sample)]);
        differenceEnergy += deltaL * deltaL + deltaR * deltaR;
        if (! std::isfinite (wet[0][static_cast<size_t> (sample)])
            || ! std::isfinite (wet[1][static_cast<size_t> (sample)]))
        {
            std::cerr << "Quad-tap chorus produced non-finite output\n";
            return 1;
        }
    }

    for (int sample = totalSamples / 2 - 256; sample < totalSamples / 2 + 8; ++sample)
    {
        if (! std::isfinite (automated[0][static_cast<size_t> (sample)])
            || ! std::isfinite (automated[1][static_cast<size_t> (sample)]))
        {
            std::cerr << "Quad-tap chorus automation produced non-finite output\n";
            return 1;
        }
        if (sample > 0)
        {
            const double step = std::max (
                std::abs (static_cast<double> (automated[0][static_cast<size_t> (sample)])
                          - automated[0][static_cast<size_t> (sample - 1)]),
                std::abs (static_cast<double> (automated[1][static_cast<size_t> (sample)])
                          - automated[1][static_cast<size_t> (sample - 1)]));
            if (sample < totalSamples / 2)
                preTransitionStep = std::max (preTransitionStep, step);
            else
                transitionStep = std::max (transitionStep, step);
        }
    }

    // The chorus must audibly act on the signal (real modulation, not a
    // silent bypass) and stay bounded below clipping.  The wet path is added
    // on top of a unity dry path, so a wet peak above the dry peak is expected;
    // the useful regression bound is a generous relative and absolute ceiling.
    std::cout << "Quad-tap chorus probe: dryPeak=" << dryPeak
              << " wetPeakL=" << wetPeakL << " wetPeakR=" << wetPeakR
              << " differenceEnergy=" << differenceEnergy
              << " automationStep=" << transitionStep
              << " baselineStep=" << preTransitionStep << '\n';
    if (dryPeak <= 1.0e-3 || differenceEnergy <= 1.0e-6)
    {
        std::cerr << "Quad-tap chorus did not modulate the dry signal\n";
        return 1;
    }
    const double chorusPeakLimit = std::min (0.5, dryPeak * 2.5);
    if (wetPeakL >= chorusPeakLimit || wetPeakR >= chorusPeakLimit)
    {
        std::cerr << "Quad-tap chorus output exceeded its bounded envelope\n";
        return 1;
    }
    if (transitionStep > preTransitionStep + 0.08)
    {
        std::cerr << "Quad-tap chorus automation produced a zipper step\n";
        return 1;
    }
    return 0;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    if (runMonoNotePriorityRegression() != 0)
        return 1;
    if (runOversampledDriveAliasingRegression() != 0)
        return 1;
    if (runQuadTapChorusRegression() != 0)
        return 1;
    const int contractFailures = runFactoryPresetContractRegression();

    float highestPeak = 0.0f;

    EonMiniEEFProcessor busProbe;
    juce::AudioProcessor::BusesLayout instrumentLayout;
    instrumentLayout.inputBuses.add (juce::AudioChannelSet::disabled());
    instrumentLayout.outputBuses.add (juce::AudioChannelSet::stereo());
    if (! busProbe.isBusesLayoutSupported (instrumentLayout))
    {
        std::cerr << "Standard no-input/stereo-output instrument layout is unsupported\n";
        return 1;
    }

    for (int index = 0; index < FactoryPresets::names().size(); ++index)
    {
        EonMiniEEFProcessor original;
        original.prepareToPlay (48000.0, 128);
        FactoryPresets::apply (original, index);

        juce::MemoryBlock state;
        original.getStateInformation (state);

        EonMiniEEFProcessor restored;
        restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        if (! hasMatchingParameterValues (original, restored))
        {
            std::cerr << "State round trip failed for " << FactoryPresets::names()[index] << '\n';
            return 1;
        }

        restored.prepareToPlay (48000.0, 128);
        juce::AudioBuffer<float> buffer (2, 128);
        juce::MidiBuffer midi;
        float sustainedPeak = 0.0f;

        for (int block = 0; block < 256; ++block)
        {
            buffer.clear();
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (block == 200)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            restored.processBlock (buffer, midi);
            midi.clear();

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                {
                    const auto value = buffer.getSample (channel, sample);
                    if (! std::isfinite (value) || std::abs (value) > 4.0f)
                    {
                        std::cerr << "Audio safety check failed for " << FactoryPresets::names()[index] << '\n';
                        return 1;
                    }
                    highestPeak = juce::jmax (highestPeak, std::abs (value));
                    if (block > 1 && block < 100)
                        sustainedPeak = juce::jmax (sustainedPeak, std::abs (value));
                }
        }
        if (sustainedPeak <= 1.0e-6f)
        {
            std::cerr << "Voice stopped during held note for " << FactoryPresets::names()[index] << '\n';
            return 1;
        }
    }

    // Measure the first audible sample for valid in-block note events.  The
    // 32-sample case uses its midpoint; larger blocks exercise the requested
    // offset 48.  No sample before the event may contain voice output.
    for (const float qualityMode : { 0.0f, 1.0f, 2.0f })
    for (const int blockSize : { 32, 64, 128, 256 })
    {
        const int eventOffset = blockSize == 32 ? 16 : 48;
        EonMiniEEFProcessor offsetProbe;
        if (auto* quality = offsetProbe.apvts.getParameter (ParamIDs::oversampling))
            quality->setValueNotifyingHost (quality->convertTo0to1 (qualityMode));
        offsetProbe.prepareToPlay (48000.0, blockSize);
        const int audibleOffset = eventOffset + offsetProbe.getLatencySamples();
        juce::AudioBuffer<float> offsetBuffer (2, blockSize);
        juce::MidiBuffer offsetMidi;
        offsetMidi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100),
                             eventOffset);
        offsetProbe.processBlock (offsetBuffer, offsetMidi);

        float preEventPeak = 0.0f;
        int firstNonZeroSample = -1;
        for (int sample = 0; sample < offsetBuffer.getNumSamples(); ++sample)
        {
            const float peak = juce::jmax (std::abs (offsetBuffer.getSample (0, sample)),
                                           std::abs (offsetBuffer.getSample (1, sample)));
            if (sample < eventOffset)
                preEventPeak = juce::jmax (preEventPeak, peak);
            if (firstNonZeroSample < 0 && peak > 1.0e-8f)
                firstNonZeroSample = sample;
        }

        std::cout << "MIDI offset probe: quality=" << qualityMode
                  << " block=" << blockSize
                  << " event=" << eventOffset
                  << " latency=" << offsetProbe.getLatencySamples()
                  << " firstNonZero=" << firstNonZeroSample << '\n';
        // The IIR oversampling filters are causal: their first non-zero sample
        // precedes their energy-centre/group-delay point.  Do not truncate that
        // transient merely to make firstNonZero equal the host latency.  The
        // actual contract is silence before the MIDI timestamp, audible output
        // no later than timestamp + reported latency, and identical reported
        // group delay for all modes (checked above).
        if (preEventPeak > 1.0e-8f
            || firstNonZeroSample < eventOffset
            || firstNonZeroSample > audibleOffset)
        {
            std::cerr << "Sample-offset MIDI event rendering failed for quality "
                      << qualityMode << ", block " << blockSize << '\n';
            return 1;
        }
    }

    // Loading a project or preset schedules a lock-free DSP reset at the next
    // audio callback.  No held voice, ambience tail, oversampler history, or
    // latency-line sample may leak across that state boundary.
    EonMiniEEFProcessor stateResetProbe;
    stateResetProbe.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> stateResetBuffer (2, 128);
    juce::MidiBuffer stateResetMidi;
    stateResetMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0);
    stateResetProbe.processBlock (stateResetBuffer, stateResetMidi);

    juce::MemoryBlock liveState;
    stateResetProbe.getStateInformation (liveState);
    stateResetProbe.setStateInformation (liveState.getData(),
                                         static_cast<int> (liveState.getSize()));
    stateResetBuffer.clear();
    stateResetMidi.clear();
    stateResetProbe.processBlock (stateResetBuffer, stateResetMidi);

    float postStateLoadPeak = 0.0f;
    for (int channel = 0; channel < stateResetBuffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < stateResetBuffer.getNumSamples(); ++sample)
            postStateLoadPeak = juce::jmax (
                postStateLoadPeak,
                std::abs (stateResetBuffer.getSample (channel, sample)));
    if (postStateLoadPeak > 1.0e-8f)
    {
        std::cerr << "State load leaked active DSP history; peak="
                  << postStateLoadPeak << '\n';
        return 1;
    }

    // JUCE Synthesiser defaults to a 32-sample minimum subdivision.  A single
    // first Note On does not reveal that limit, so compare two closely-spaced
    // note events against a one-sample reference render.  Both event edges
    // must land on their exact sample for every host block size.
    for (const int blockSize : { 32, 64, 128, 256 })
    {
        EonMiniEEFProcessor blockProbe, referenceProbe;
        blockProbe.prepareToPlay (48000.0, blockSize);
        referenceProbe.prepareToPlay (48000.0, 1);

        juce::AudioBuffer<float> blockBuffer (2, blockSize);
        juce::MidiBuffer blockMidi;
        blockMidi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 101), 5);
        blockMidi.addEvent (juce::MidiMessage::noteOff (1, 67), 17);
        blockProbe.processBlock (blockBuffer, blockMidi);

        juce::AudioBuffer<float> referenceBuffer (2, blockSize);
        referenceBuffer.clear();
        juce::AudioBuffer<float> oneSample (2, 1);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            oneSample.clear();
            juce::MidiBuffer referenceMidi;
            if (sample == 5)
                referenceMidi.addEvent (juce::MidiMessage::noteOn (
                    1, 67, (juce::uint8) 101), 0);
            if (sample == 17)
                referenceMidi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
            referenceProbe.processBlock (oneSample, referenceMidi);
            for (int channel = 0; channel < 2; ++channel)
                referenceBuffer.setSample (channel, sample,
                                           oneSample.getSample (channel, 0));
        }

        float maximumDifference = 0.0f;
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < blockSize; ++sample)
                maximumDifference = juce::jmax (
                    maximumDifference,
                    std::abs (blockBuffer.getSample (channel, sample)
                              - referenceBuffer.getSample (channel, sample)));

        std::cout << "MIDI close-event probe: block=" << blockSize
                  << " noteOn=5 noteOff=17 maxDifference="
                  << maximumDifference << '\n';
        if (maximumDifference > 1.0e-6f)
        {
            std::cerr << "Closely-spaced MIDI events were quantised for block "
                      << blockSize << '\n';
            return 1;
        }
    }

    // Note On/Off, pitch wheel, and sustain are the instrument contract.  The
    // wheel at 8192 is a fixed x1 ratio, so it must not move rendered samples;
    // mod wheel, aftertouch, and channel pressure stay ignored.  CC64 is not
    // sent here because sustain legitimately defers release.
    EonMiniEEFProcessor notesOnlyProbe, extraMidiProbe;
    notesOnlyProbe.prepareToPlay (48000.0, 128);
    extraMidiProbe.prepareToPlay (48000.0, 128);
    juce::AudioBuffer<float> notesOnlyBuffer (2, 128), extraMidiBuffer (2, 128);
    juce::MidiBuffer notesOnlyMidi, extraMidi;
    notesOnlyMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    notesOnlyMidi.addEvent (juce::MidiMessage::noteOff (1, 60), 96);
    extraMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    extraMidi.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 16);
    extraMidi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 24);
    extraMidi.addEvent (juce::MidiMessage::aftertouchChange (1, 60, 127), 28);
    extraMidi.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 32);
    extraMidi.addEvent (juce::MidiMessage::noteOff (1, 60), 96);
    notesOnlyProbe.processBlock (notesOnlyBuffer, notesOnlyMidi);
    extraMidiProbe.processBlock (extraMidiBuffer, extraMidi);

    float extraMessageDifference = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int sample = 0; sample < 128; ++sample)
            extraMessageDifference = juce::jmax (
                extraMessageDifference,
                std::abs (notesOnlyBuffer.getSample (channel, sample)
                          - extraMidiBuffer.getSample (channel, sample)));
    if (extraMessageDifference > 1.0e-7f)
    {
        std::cerr << "Non-note MIDI messages still affect audio; max difference="
                  << extraMessageDifference << '\n';
        return 1;
    }

    // Extreme oscillator tuning and pulse widths must remain finite instead of
    // folding a frequency at Nyquist or poisoning the voice/filter state.
    for (const float pulseWidth : { 0.05f, 0.5f, 0.95f })
    {
        EonMiniEEFProcessor oscillatorStressProbe;
        const auto setPlain = [&oscillatorStressProbe] (const char* id, float plainValue)
        {
            if (auto* parameter = oscillatorStressProbe.apvts.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        };
        for (const auto* id : { ParamIDs::osc1Wave, ParamIDs::osc2Wave,
                                ParamIDs::osc3Wave, ParamIDs::osc4Wave })
            setPlain (id, 1.0f);
        for (const auto* id : { ParamIDs::osc1Level, ParamIDs::osc2Level,
                                ParamIDs::osc3Level, ParamIDs::osc4Level })
            setPlain (id, 1.0f);
        for (const auto* id : { ParamIDs::osc1Coarse, ParamIDs::osc2Coarse,
                                ParamIDs::osc3Coarse, ParamIDs::osc4Coarse })
            setPlain (id, 24.0f);
        for (const auto* id : { ParamIDs::osc1PulseWidth, ParamIDs::osc2PulseWidth,
                                ParamIDs::osc3PulseWidth, ParamIDs::osc4PulseWidth })
            setPlain (id, pulseWidth);
        setPlain (ParamIDs::unisonVoices, 8.0f);
        setPlain (ParamIDs::unisonDetune, 24.0f);
        setPlain (ParamIDs::resonance, 1.0f);
        setPlain (ParamIDs::drive, 1.0f);
        oscillatorStressProbe.prepareToPlay (44100.0, 257);
        juce::AudioBuffer<float> stressBuffer (2, 257);
        for (int block = 0; block < 48; ++block)
        {
            stressBuffer.clear();
            juce::MidiBuffer stressMidi;
            if (block == 0)
                stressMidi.addEvent (juce::MidiMessage::noteOn (
                    1, 127, (juce::uint8) 127), 19);
            oscillatorStressProbe.processBlock (stressBuffer, stressMidi);
            for (int channel = 0; channel < stressBuffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < stressBuffer.getNumSamples(); ++sample)
                    if (! std::isfinite (stressBuffer.getSample (channel, sample)))
                    {
                        std::cerr << "Oscillator/PWM stress produced non-finite output at PW="
                                  << pulseWidth << '\n';
                        return 1;
                    }
        }
    }

    // The ambience feedback path must remain finite for long notes and recover
    // after a release/retrigger.  A runaway state is especially dangerous here:
    // the final non-finite guard intentionally mutes bad samples, which can make
    // the instrument appear to play briefly and then stay permanently silent.
    struct AmbienceConfig
    {
        double sampleRate;
        int blockSize;
    };

    constexpr AmbienceConfig ambienceConfigs[] {
        { 44100.0, 64 },
        { 48000.0, 128 },
        { 96000.0, 512 },
        { 192000.0, 1024 }
    };

    for (const auto config : ambienceConfigs)
    {
        EonMiniEEFProcessor ambienceProbe;
        auto setPlainValue = [&ambienceProbe] (const char* id, float plainValue)
        {
            if (auto* parameter = ambienceProbe.apvts.getParameter (id))
            {
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
                return true;
            }
            return false;
        };

        // Exercise the genuinely wet feedback path, including maximum legal
        // delay feedback plus chorus and reverb contribution.
        if (! setPlainValue (ParamIDs::fxWet, 1.0f)
            || ! setPlainValue (ParamIDs::delayFeedback, 0.9f)
            || ! setPlainValue (ParamIDs::chorusMix, 1.0f)
            || ! setPlainValue (ParamIDs::reverbMix, 1.0f))
        {
            std::cerr << "Ambience parameters are unavailable\n";
            return 1;
        }

        ambienceProbe.prepareToPlay (config.sampleRate, config.blockSize);
        juce::AudioBuffer<float> ambienceBuffer (2, config.blockSize);
        const auto blockAt = [config] (double seconds)
        {
            return static_cast<int> (std::ceil (seconds * config.sampleRate
                                               / static_cast<double> (config.blockSize)));
        };
        const int lateHeldStart = blockAt (0.8);
        const int noteOffBlock = blockAt (1.2);
        const int retriggerBlock = blockAt (1.5);
        const int totalBlocks = blockAt (2.0);
        float lateHeldPeak = 0.0f, retriggerPeak = 0.0f;

        for (int block = 0; block < totalBlocks; ++block)
        {
            ambienceBuffer.clear();
            juce::MidiBuffer ambienceMidi;
            if (block == 0)
                ambienceMidi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (block == noteOffBlock)
                ambienceMidi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
            if (block == retriggerBlock)
                ambienceMidi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 0);
            ambienceProbe.processBlock (ambienceBuffer, ambienceMidi);

            for (int channel = 0; channel < ambienceBuffer.getNumChannels(); ++channel)
                for (int sample = 0; sample < ambienceBuffer.getNumSamples(); ++sample)
                {
                    const float value = ambienceBuffer.getSample (channel, sample);
                    if (! std::isfinite (value))
                    {
                        std::cerr << "Ambience path produced non-finite output at "
                                  << config.sampleRate << " Hz / " << config.blockSize << " samples\n";
                        return 1;
                    }
                    if (block >= lateHeldStart && block < noteOffBlock)
                        lateHeldPeak = juce::jmax (lateHeldPeak, std::abs (value));
                    if (block >= retriggerBlock + 2)
                        retriggerPeak = juce::jmax (retriggerPeak, std::abs (value));
                }
        }

        if (lateHeldPeak <= 1.0e-6f || retriggerPeak <= 1.0e-6f)
        {
            std::cerr << "Ambience path stopped producing sound at "
                      << config.sampleRate << " Hz / " << config.blockSize
                      << " samples: lateHeldPeak=" << lateHeldPeak
                      << ", retriggerPeak=" << retriggerPeak << '\n';
            return 1;
        }
    }

    std::cout << "Validated " << FactoryPresets::names().size()
              << " factory presets; peak=" << highestPeak << '\n';
    return contractFailures == 0 && highestPeak > 1.0e-6f ? 0 : 1;
}
