#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdint>

namespace
{
    constexpr int currentStateFormatVersion = 1;

    float blep (float t, float dt)
    {
        // Keep the correction well-defined for the highest notes and for
        // hosts running at unusual sample rates.
        dt = juce::jlimit (1.0e-5f, 0.49f, dt);
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    float oscillatorSample (float phase, float increment, int waveform, float pulseWidth)
    {
        phase = phase - std::floor (phase);
        const auto dt = juce::jlimit (1.0e-5f, 0.49f, increment);

        switch (juce::jlimit (0, 3, waveform))
        {
            case 1: // PolyBLEP pulse
            {
                const float width = juce::jlimit (0.05f, 0.95f, pulseWidth);
                return (phase < width ? 1.0f : -1.0f)
                     + blep (phase, dt)
                     - blep (std::fmod (phase + 1.0f - width, 1.0f), dt);
            }

            case 2: // Symmetric triangle; continuous at both turning points
                return 1.0f - 4.0f * std::abs (phase - 0.5f);

            case 3: // Sine
                return std::sin (juce::MathConstants<float>::twoPi * phase);

            case 0: // PolyBLEP saw
            default:
                return 2.0f * phase - 1.0f + blep (phase, dt);
        }
    }

    // Small allocation-free PRNG for the per-voice white-noise oscillator.
    // The state is owned by EonVoice and is never shared with the UI thread.
    float whiteNoiseSample (std::uint32_t& state) noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float> (state) * 4.6566128730773925781e-10f - 1.0f;
    }

    float modulationSourceValue (int source, float lfo, float envelope,
                                 float velocity) noexcept
    {
        switch (juce::jlimit (0, 4, source))
        {
            case 1: return lfo;
            case 2: return envelope;
            case 3: return velocity;
            default: return 0.0f;
        }
    }

    constexpr std::array<const char*, 14> removedParameterIds
    {
        "wavetableMix", "wavetableIndex",
        "timeMode", "timeDivision", "timeMix", "gateDepth",
        "gateCurve0", "gateCurve1", "gateCurve2", "gateCurve3",
        "gateCurve4", "gateCurve5", "gateCurve6", "gateCurve7"
    };

    bool isRemovedParameterId (const juce::String& id) noexcept
    {
        for (const auto* removedId : removedParameterIds)
            if (id == removedId)
                return true;

        return false;
    }

    void sanitizeLegacyState (juce::ValueTree state)
    {
        for (int property = state.getNumProperties(); --property >= 0;)
        {
            const auto name = state.getPropertyName (property);
            if (name.toString() == "userWavetablePath"
                || name.toString().startsWith ("midiCC")
                || isRemovedParameterId (name.toString()))
                state.removeProperty (name, nullptr);
        }

        for (int childIndex = state.getNumChildren(); --childIndex >= 0;)
        {
            auto child = state.getChild (childIndex);
            const auto childId = child.getProperty ("id").toString();
            if (child.hasType (juce::Identifier { "PARAM" })
                && (isRemovedParameterId (childId) || childId.startsWith ("midiCC")))
            {
                state.removeChild (childIndex, nullptr);
                continue;
            }

            sanitizeLegacyState (child);
        }
    }

    void migrateLegacyState (juce::ValueTree state)
    {
        for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
        {
            auto child = state.getChild (childIndex);
            if (child.hasType (juce::Identifier { "PARAM" }))
            {
                const auto id = child.getProperty ("id").toString();
                if (id == "osc2Detune")
                {
                    const float legacySemitones = juce::jlimit (-24.99f, 24.99f,
                        static_cast<float> (child.getProperty ("value", 0.0f)));
                    const float coarse = juce::jlimit (-24.0f, 24.0f,
                        static_cast<float> (juce::roundToInt (legacySemitones)));
                    const float fine = juce::jlimit (-100.0f, 100.0f,
                        (legacySemitones - coarse) * 100.0f);
                    child.setProperty ("id", ParamIDs::osc2Coarse, nullptr);
                    child.setProperty ("value", coarse, nullptr);

                    bool updatedFine = false;
                    for (int siblingIndex = 0; siblingIndex < state.getNumChildren(); ++siblingIndex)
                    {
                        auto sibling = state.getChild (siblingIndex);
                        if (sibling.hasType (juce::Identifier { "PARAM" })
                            && sibling.getProperty ("id").toString() == ParamIDs::osc2Fine)
                        {
                            sibling.setProperty ("value", fine, nullptr);
                            updatedFine = true;
                            break;
                        }
                    }
                    if (! updatedFine)
                    {
                        juce::ValueTree fineParameter { "PARAM" };
                        fineParameter.setProperty ("id", ParamIDs::osc2Fine, nullptr);
                        fineParameter.setProperty ("value", fine, nullptr);
                        state.addChild (fineParameter, -1, nullptr);
                    }
                }
                if (id == "osc3Mix") child.setProperty ("id", ParamIDs::osc3Level, nullptr);
                if (id == "osc4Mix") child.setProperty ("id", ParamIDs::osc4Level, nullptr);
                if (id.startsWith (ParamIDs::modSourcePrefix))
                {
                    const int source = juce::roundToInt ((float) child.getProperty ("value", 0.0f));
                    child.setProperty ("value", source == 6 ? 4.0f : source >= 4 ? 0.0f : (float) source, nullptr);
                }
            }
            else
                migrateLegacyState (child);
        }
    }
}
class EonVoice : public juce::SynthesiserVoice
{
public:
    explicit EonVoice (EonMiniEEFProcessor& processor) : p (processor) {}

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<juce::SynthesiserSound*> (sound) != nullptr;
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        note = midiNote;
        vel = velocity;
        // A stolen/reused voice must never carry the previous note's filter
        // charge into the new attack.
        state.fill (0.0f);
        svfIc1.fill (0.0f);
        svfIc2.fill (0.0f);
        constexpr std::array<const char*, 4> phaseIds {
            ParamIDs::osc1Phase, ParamIDs::osc2Phase, ParamIDs::osc3Phase, ParamIDs::osc4Phase
        };
        constexpr std::array<const char*, 4> levelIds {
            ParamIDs::osc1Level, ParamIDs::osc2Level, ParamIDs::osc3Level, ParamIDs::osc4Level
        };
        constexpr std::array<const char*, 4> fineIds {
            ParamIDs::osc1Fine, ParamIDs::osc2Fine, ParamIDs::osc3Fine, ParamIDs::osc4Fine
        };
        constexpr std::array<const char*, 4> panIds {
            ParamIDs::osc1Pan, ParamIDs::osc2Pan, ParamIDs::osc3Pan, ParamIDs::osc4Pan
        };
        constexpr std::array<const char*, 4> pulseWidthIds {
            ParamIDs::osc1PulseWidth, ParamIDs::osc2PulseWidth, ParamIDs::osc3PulseWidth, ParamIDs::osc4PulseWidth
        };
        const auto* phaseParameter = p.apvts.getRawParameterValue (ParamIDs::unisonPhase);
        const float phaseScale = phaseParameter != nullptr
            ? juce::jlimit (0.0f, 1.0f, phaseParameter->load()) : 1.0f;
        std::array<float, 4> phaseStart {};
        for (size_t oscillator = 0; oscillator < phaseStart.size(); ++oscillator)
        {
            if (auto* parameter = p.apvts.getRawParameterValue (phaseIds[oscillator]))
                phaseStart[oscillator] = juce::jlimit (0.0f, 1.0f, parameter->load());
            levelSmooth[oscillator].setCurrentAndTargetValue (p.apvts.getRawParameterValue (levelIds[oscillator])->load());
            fineSmooth[oscillator].setCurrentAndTargetValue (p.apvts.getRawParameterValue (fineIds[oscillator])->load());
            panSmooth[oscillator].setCurrentAndTargetValue (p.apvts.getRawParameterValue (panIds[oscillator])->load());
            pulseWidthSmooth[oscillator].setCurrentAndTargetValue (p.apvts.getRawParameterValue (pulseWidthIds[oscillator])->load());
        }
        const auto currentValue = [this] (const char* id, float fallback) noexcept
        {
            if (const auto* parameter = p.apvts.getRawParameterValue (id))
                return parameter->load();
            return fallback;
        };
        cutoffSmooth.setCurrentAndTargetValue (juce::jmax (20.0f,
            currentValue (ParamIDs::cutoff, 12000.0f)));
        resonanceSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::resonance, 0.15f));
        gainSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::gain, 0.7f));
        driveSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::drive, 0.0f));
        noiseSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::noiseMix, 0.0f));
        unisonDetuneSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::unisonDetune, 0.0f));
        unisonSpreadSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::unisonSpread, 0.0f));
        amDepthSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::amDepth, 0.0f));
        for (int i = 0; i < maxUnisonVoices; ++i)
        {
            // Layer zero retains the old phase. Other layers receive stable,
            // independent offsets so retriggering remains deterministic.
            ph1[static_cast<size_t> (i)] = std::fmod (phaseStart[0] + 0.173f * i * phaseScale, 1.0f);
            ph2[static_cast<size_t> (i)] = std::fmod (phaseStart[1] + 0.311f * i * phaseScale, 1.0f);
            ph3[static_cast<size_t> (i)] = std::fmod (phaseStart[2] + 0.457f * i * phaseScale, 1.0f);
            ph4[static_cast<size_t> (i)] = std::fmod (phaseStart[3] + 0.619f * i * phaseScale, 1.0f);
        }
        // Non-zero deterministic seed avoids the xorshift zero lock-up while
        // keeping note retriggers reproducible for offline rendering.
        noiseState = 0x9e3779b9u ^ (static_cast<std::uint32_t> (midiNote + 1) * 0x85ebca6bu);
        if (noiseState == 0)
            noiseState = 0x6d2b79f5u;
        updateEnvelopeParameters();
        env.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        env.noteOff();
        if (! allowTailOff)
            clearCurrentNote();
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void channelPressureChanged (int) override {}

    void resetForStateLoad() noexcept
    {
        env.reset();
        setKeyDown (false);
        setSustainPedalDown (false);
        setSostenutoPedalDown (false);
        clearCurrentNote();
        state.fill (0.0f);
        svfIc1.fill (0.0f);
        svfIc2.fill (0.0f);
        ph1.fill (0.0f);
        ph2.fill (0.0f);
        ph3.fill (0.0f);
        ph4.fill (0.0f);
        lfoPhase = 0.0f;
        noiseState = 0x6d2b79f5u;
    }

    void setSR (double newSampleRate)
    {
        sr = newSampleRate;
        env.setSampleRate (newSampleRate);
        cutoffSmooth.reset (newSampleRate, 0.005);
        resonanceSmooth.reset (newSampleRate, 0.005);
        gainSmooth.reset (newSampleRate, 0.005);
        driveSmooth.reset (newSampleRate, 0.005);
        noiseSmooth.reset (newSampleRate, 0.005);
        unisonDetuneSmooth.reset (newSampleRate, 0.005);
        unisonSpreadSmooth.reset (newSampleRate, 0.005);
        amDepthSmooth.reset (newSampleRate, 0.005);
        cutoffSmooth.setCurrentAndTargetValue (12000.0f);
        resonanceSmooth.setCurrentAndTargetValue (0.15f);
        gainSmooth.setCurrentAndTargetValue (0.7f);
        driveSmooth.setCurrentAndTargetValue (0.0f);
        noiseSmooth.setCurrentAndTargetValue (0.0f);
        unisonDetuneSmooth.setCurrentAndTargetValue (0.0f);
        unisonSpreadSmooth.setCurrentAndTargetValue (0.0f);
        amDepthSmooth.setCurrentAndTargetValue (0.0f);
        for (auto& smoother : levelSmooth) { smoother.reset (newSampleRate, 0.005); smoother.setCurrentAndTargetValue (0.0f); }
        for (auto& smoother : fineSmooth) { smoother.reset (newSampleRate, 0.005); smoother.setCurrentAndTargetValue (0.0f); }
        for (auto& smoother : panSmooth) { smoother.reset (newSampleRate, 0.005); smoother.setCurrentAndTargetValue (0.0f); }
        for (auto& smoother : pulseWidthSmooth) { smoother.reset (newSampleRate, 0.005); smoother.setCurrentAndTargetValue (0.5f); }
        updateEnvelopeParameters();
    }

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
    {
        // Reading atomics once per block keeps automation responsive without
        // repeatedly touching the parameter tree in the sample loop.
        updateEnvelopeParameters();

        auto* w1 = p.apvts.getRawParameterValue (ParamIDs::osc1Wave);
        auto* w2 = p.apvts.getRawParameterValue (ParamIDs::osc2Wave);
        auto* w3 = p.apvts.getRawParameterValue (ParamIDs::osc3Wave);
        auto* w4 = p.apvts.getRawParameterValue (ParamIDs::osc4Wave);
        constexpr std::array<const char*, 4> levelIds {
            ParamIDs::osc1Level, ParamIDs::osc2Level, ParamIDs::osc3Level, ParamIDs::osc4Level
        };
        constexpr std::array<const char*, 4> coarseIds {
            ParamIDs::osc1Coarse, ParamIDs::osc2Coarse, ParamIDs::osc3Coarse, ParamIDs::osc4Coarse
        };
        constexpr std::array<const char*, 4> fineIds {
            ParamIDs::osc1Fine, ParamIDs::osc2Fine, ParamIDs::osc3Fine, ParamIDs::osc4Fine
        };
        constexpr std::array<const char*, 4> panIds {
            ParamIDs::osc1Pan, ParamIDs::osc2Pan, ParamIDs::osc3Pan, ParamIDs::osc4Pan
        };
        constexpr std::array<const char*, 4> pulseWidthIds {
            ParamIDs::osc1PulseWidth, ParamIDs::osc2PulseWidth, ParamIDs::osc3PulseWidth, ParamIDs::osc4PulseWidth
        };
        std::array<std::atomic<float>*, 4> levelParams {}, coarseParams {}, fineParams {}, panParams {}, pulseWidthParams {};
        for (size_t oscillator = 0; oscillator < 4; ++oscillator)
        {
            levelParams[oscillator] = p.apvts.getRawParameterValue (levelIds[oscillator]);
            coarseParams[oscillator] = p.apvts.getRawParameterValue (coarseIds[oscillator]);
            fineParams[oscillator] = p.apvts.getRawParameterValue (fineIds[oscillator]);
            panParams[oscillator] = p.apvts.getRawParameterValue (panIds[oscillator]);
            pulseWidthParams[oscillator] = p.apvts.getRawParameterValue (pulseWidthIds[oscillator]);
            levelSmooth[oscillator].setTargetValue (levelParams[oscillator]->load());
            fineSmooth[oscillator].setTargetValue (fineParams[oscillator]->load());
            panSmooth[oscillator].setTargetValue (panParams[oscillator]->load());
            pulseWidthSmooth[oscillator].setTargetValue (pulseWidthParams[oscillator]->load());
        }
        auto* noiseMixParam = p.apvts.getRawParameterValue (ParamIDs::noiseMix);
        auto* unisonVoicesParam = p.apvts.getRawParameterValue (ParamIDs::unisonVoices);
        auto* unisonDetuneParam = p.apvts.getRawParameterValue (ParamIDs::unisonDetune);
        auto* unisonSpreadParam = p.apvts.getRawParameterValue (ParamIDs::unisonSpread);
        auto* cutoff = p.apvts.getRawParameterValue (ParamIDs::cutoff);
        auto* resonance = p.apvts.getRawParameterValue (ParamIDs::resonance);
        auto* filterMode = p.apvts.getRawParameterValue (ParamIDs::filterMode);
        auto* gain = p.apvts.getRawParameterValue (ParamIDs::gain);
        auto* drive = p.apvts.getRawParameterValue (ParamIDs::drive);
        auto* lfoRate = p.apvts.getRawParameterValue (ParamIDs::lfoRate);
        auto* lfoDepth = p.apvts.getRawParameterValue (ParamIDs::lfoDepth);
        auto* lfoPitch = p.apvts.getRawParameterValue (ParamIDs::lfoPitch);
        auto* amDepthParam = p.apvts.getRawParameterValue (ParamIDs::amDepth);
        auto* velocityAmount = p.apvts.getRawParameterValue (ParamIDs::velocityAmount);
        auto* lfoSync = p.apvts.getRawParameterValue (ParamIDs::lfoSync);
        auto* lfoDivision = p.apvts.getRawParameterValue (ParamIDs::lfoDivision);
        auto* keyTracking = p.apvts.getRawParameterValue (ParamIDs::keyTracking);
        std::array<std::atomic<float>*, 4> modSourceParams {}, modDestinationParams {}, modAmountParams {};
        for (int slot = 0; slot < 4; ++slot)
        {
            modSourceParams[static_cast<size_t> (slot)] = p.apvts.getRawParameterValue (ParamIDs::modSource (slot));
            modDestinationParams[static_cast<size_t> (slot)] = p.apvts.getRawParameterValue (ParamIDs::modDestination (slot));
            modAmountParams[static_cast<size_t> (slot)] = p.apvts.getRawParameterValue (ParamIDs::modAmount (slot));
        }
        // Smooth the two parameters that most often receive rapid host
        // automation.  The target is captured once per block and the
        // SmoothedValue ramps sample-by-sample, avoiding zipper noise and
        // keeping the filter coefficient bounded during fast sweeps.
        cutoffSmooth.setTargetValue (cutoff->load());
        resonanceSmooth.setTargetValue (resonance->load());
        gainSmooth.setTargetValue (gain->load());
        driveSmooth.setTargetValue (drive->load());
        noiseSmooth.setTargetValue (noiseMixParam != nullptr ? noiseMixParam->load() : 0.0f);
        unisonDetuneSmooth.setTargetValue (unisonDetuneParam->load());
        unisonSpreadSmooth.setTargetValue (unisonSpreadParam != nullptr ? unisonSpreadParam->load() : 0.0f);
        amDepthSmooth.setTargetValue (amDepthParam != nullptr ? amDepthParam->load() : 0.0f);
        const float baseFrequency = juce::MidiMessage::getMidiNoteInHertz (note);
        // In sync mode, use the host quarter-note clock.  If no playhead or
        // BPM is available, the processor's stable 120 BPM fallback keeps the
        // oscillator running instead of producing a zero/NaN increment.
        const int division = juce::jlimit (0, 4, juce::roundToInt (lfoDivision != nullptr ? lfoDivision->load() : 2.0f));
        constexpr float beats[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
        const float syncedHz = p.getHostBpm() / (60.0f * beats[division]);
        const float lfoHz = juce::jlimit (0.01f, 30.0f,
            (lfoSync != nullptr && lfoSync->load() > 0.5f) ? syncedHz : lfoRate->load());
        const float lfoStep = lfoHz / static_cast<float> (sr);
        const int activeUnisonVoices = juce::jlimit (1, maxUnisonVoices,
            juce::roundToInt (unisonVoicesParam->load()));
        for (int i = 0; i < numSamples; ++i)
        {
            const float unisonDetuneCents = juce::jlimit (0.0f, 24.0f,
                unisonDetuneSmooth.getNextValue());
            const float unisonSpreadAmount = juce::jlimit (0.0f, 1.0f,
                unisonSpreadSmooth.getNextValue());
            const float noiseMix = juce::jlimit (0.0f, 1.0f,
                noiseSmooth.getNextValue());
            const float amDepth = juce::jlimit (0.0f, 1.0f,
                amDepthSmooth.getNextValue());
            const float outputGain = juce::jlimit (0.0f, 1.0f,
                gainSmooth.getNextValue());
            const float driveAmount = juce::jlimit (0.0f, 1.0f,
                driveSmooth.getNextValue());
            // A per-voice, never-reset phase gives free-running LFO behaviour.
            const float lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
            lfoPhase = std::fmod (lfoPhase + lfoStep, 1.0f);
            const float envelope = env.getNextSample();
            float matrixPitch = 0.0f, matrixCutoff = 0.0f, matrixAmp = 0.0f,
                  matrixFm = 0.0f, osc1FmAmount = 0.0f;
            for (int slot = 0; slot < 4; ++slot)
            {
                if (modSourceParams[static_cast<size_t> (slot)] == nullptr
                    || modDestinationParams[static_cast<size_t> (slot)] == nullptr
                    || modAmountParams[static_cast<size_t> (slot)] == nullptr)
                    continue;
                const int source = juce::jlimit (0, 4, juce::roundToInt (modSourceParams[static_cast<size_t> (slot)]->load()));
                const int destination = juce::jlimit (0, 4, juce::roundToInt (modDestinationParams[static_cast<size_t> (slot)]->load()));
                const float amount = juce::jlimit (-1.0f, 1.0f, modAmountParams[static_cast<size_t> (slot)]->load());
                // Oscillator 1 is an audio-rate source and is handled in the
                // oscillator loop below to provide bounded phase modulation.
                if (source == 4)
                {
                    if (destination == 4)
                        osc1FmAmount += 0.5f * amount;
                    continue;
                }
                const float sourceValue = modulationSourceValue (source, lfo, envelope, vel);
                switch (destination)
                {
                    case 1: matrixPitch += 12.0f * amount * sourceValue; break;
                    case 2: matrixCutoff += amount * sourceValue; break;
                    case 3: matrixAmp += amount * sourceValue; break;
                    case 4: matrixFm += 0.5f * amount * sourceValue; break;
                    default: break;
                }
            }
            matrixPitch = juce::jlimit (-24.0f, 24.0f, matrixPitch);
            matrixCutoff = juce::jlimit (-0.95f, 4.0f, matrixCutoff);
            matrixAmp = juce::jlimit (-1.0f, 1.0f, matrixAmp);
            matrixFm = juce::jlimit (-0.5f, 0.5f, matrixFm);
            osc1FmAmount = juce::jlimit (-0.5f, 0.5f, osc1FmAmount);
            const float modulatedFrequency = baseFrequency * std::pow (2.0f, matrixPitch / 12.0f)
                * std::pow (2.0f, lfo * lfoPitch->load() / 12.0f);
            // Limit each oscillator below Nyquist.  Apart from avoiding
            // invalid PolyBLEP increments, this gives a predictable mute-ish
            // behaviour instead of phase folding on the very top notes.
            const float nyquist = 0.49f * static_cast<float> (sr);
            std::array<float, 4> oscillatorFrequency {}, oscillatorLevel {}, oscillatorPan {}, oscillatorPulseWidth {};
            for (size_t oscillator = 0; oscillator < 4; ++oscillator)
            {
                const float semitones = juce::jlimit (-24.0f, 24.0f, coarseParams[oscillator]->load())
                                      + juce::jlimit (-100.0f, 100.0f, fineSmooth[oscillator].getNextValue()) / 100.0f;
                oscillatorFrequency[oscillator] = juce::jmax (0.01f,
                    modulatedFrequency * std::pow (2.0f, semitones / 12.0f));
                oscillatorLevel[oscillator] = juce::jlimit (0.0f, 1.0f, levelSmooth[oscillator].getNextValue());
                oscillatorPan[oscillator] = juce::jlimit (-1.0f, 1.0f, panSmooth[oscillator].getNextValue());
                oscillatorPulseWidth[oscillator] = juce::jlimit (0.05f, 0.95f, pulseWidthSmooth[oscillator].getNextValue());
            }
            const float baseCutoff = cutoffSmooth.getNextValue();
            const float keyTrack = juce::jlimit (0.0f, 1.0f,
                keyTracking != nullptr ? keyTracking->load() : 0.0f);
            const float keyRatio = std::pow (2.0f,
                keyTrack * juce::jlimit (-2.0f, 2.0f, (static_cast<float> (note) - 60.0f) / 24.0f));
            const float modulatedCutoff = juce::jlimit (20.0f, 20000.0f,
                baseCutoff * keyRatio * (1.0f + lfo * lfoDepth->load() + matrixCutoff));
            const int wave1 = static_cast<int> (std::lround (w1->load()));
            const int wave2 = static_cast<int> (std::lround (w2->load()));
            const int wave3 = w3 != nullptr ? static_cast<int> (std::lround (w3->load())) : 0;
            const int wave4 = w4 != nullptr ? static_cast<int> (std::lround (w4->load())) : 0;
            float unisonLeft = 0.0f;
            float unisonRight = 0.0f;
            for (int unison = 0; unison < activeUnisonVoices; ++unison)
            {
                // The control is the outer-to-outer detune in cents, split
                // symmetrically around the played pitch.
                const float position = static_cast<float> (unison)
                                      - 0.5f * static_cast<float> (activeUnisonVoices - 1);
                const float cents = activeUnisonVoices > 1
                    ? position * unisonDetuneCents
                        / static_cast<float> (activeUnisonVoices - 1) : 0.0f;
                const float ratio = std::pow (2.0f, cents / 1200.0f);
                std::array<float, 4> increment {};
                for (size_t oscillator = 0; oscillator < 4; ++oscillator)
                {
                    const float requestedFrequency = oscillatorFrequency[oscillator] * ratio;
                    increment[oscillator] = juce::jmin (requestedFrequency, nyquist)
                                          / static_cast<float> (sr);
                }
                auto& phase1 = ph1[static_cast<size_t> (unison)];
                auto& phase2 = ph2[static_cast<size_t> (unison)];
                auto& phase3 = ph3[static_cast<size_t> (unison)];
                auto& phase4 = ph4[static_cast<size_t> (unison)];
                const float osc1Phase = phase1;
                const float osc1 = oscillatorSample (osc1Phase, increment[0], wave1, oscillatorPulseWidth[0]);
                const float osc2Phase = phase2 + matrixFm + osc1 * osc1FmAmount;
                const std::array<float, 4> samples {
                    osc1,
                    oscillatorSample (osc2Phase, increment[1], wave2, oscillatorPulseWidth[1]),
                    oscillatorSample (phase3, increment[2], wave3, oscillatorPulseWidth[2]),
                    oscillatorSample (phase4, increment[3], wave4, oscillatorPulseWidth[3])
                };
                float oscillatorLeft = 0.0f, oscillatorRight = 0.0f;
                for (size_t oscillator = 0; oscillator < 4; ++oscillator)
                {
                    const float angle = (oscillatorPan[oscillator] + 1.0f)
                                      * juce::MathConstants<float>::pi * 0.25f;
                    const float left = std::cos (angle);
                    const float right = std::sin (angle);
                    // Smoothly fade oscillators approaching Nyquist instead of
                    // pinning all higher pitches to one aliased frequency.
                    const float frequencyRatio = oscillatorFrequency[oscillator] * ratio / nyquist;
                    const float nyquistGain = 1.0f - juce::jlimit (0.0f, 1.0f,
                        (frequencyRatio - 0.8f) / 0.2f);
                    oscillatorLeft += samples[oscillator] * oscillatorLevel[oscillator] * left * nyquistGain;
                    oscillatorRight += samples[oscillator] * oscillatorLevel[oscillator] * right * nyquistGain;
                }
                const float noise = noiseMix * whiteNoiseSample (noiseState);
                // A fixed conservative bus gain preserves each Level knob's
                // meaning.  Dividing by the sum of active levels made one
                // oscillator sound equally loud at every non-zero setting.
                constexpr float oscillatorBusGain = 0.35f;
                oscillatorLeft = (oscillatorLeft + noise * 0.70710678f) * oscillatorBusGain;
                oscillatorRight = (oscillatorRight + noise * 0.70710678f) * oscillatorBusGain;
                // Equal-power-ish gains with a unity centre preserve the old
                // mono result at spread=0 while keeping wide layers bounded.
                const float positionNorm = activeUnisonVoices > 1
                    ? (2.0f * position / static_cast<float> (activeUnisonVoices - 1)) : 0.0f;
                const float pan = juce::jlimit (-1.0f, 1.0f,
                    positionNorm * unisonSpreadAmount);
                const float panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                const float panGainLeft = std::cos (panAngle);
                const float panGainRight = std::sin (panAngle);
                unisonLeft += oscillatorLeft * panGainLeft;
                unisonRight += oscillatorRight * panGainRight;
                phase1 = std::fmod (phase1 + increment[0], 1.0f);
                phase2 = std::fmod (phase2 + increment[1], 1.0f);
                phase3 = std::fmod (phase3 + increment[2], 1.0f);
                phase4 = std::fmod (phase4 + increment[3], 1.0f);
            }
            const float unisonScale = 1.0f / static_cast<float> (activeUnisonVoices);
            unisonLeft *= unisonScale;
            unisonRight *= unisonScale;
            const float velocityGain = juce::jlimit (0.0f, 1.0f,
                (1.0f - velocityAmount->load()) + velocityAmount->load() * vel);
            // Dedicated LFO AM depth.  At full depth the carrier is reduced
            // to zero at the negative LFO peak, while depth=0 is bit-identical
            // to the previous signal path.
            const float amGain = 1.0f - 0.5f * amDepth + 0.5f * amDepth * lfo;
            const float voiceGain = envelope * velocityGain * outputGain * amGain
                                  * juce::jlimit (0.0f, 2.0f, 1.0f + matrixAmp);
            // TPT state-variable filter.  The integrator states are local to
            // each voice, so fast cutoff automation and high resonance remain
            // independent across notes.  Limiting the frequency below Nyquist
            // keeps tan() well-conditioned at the top of the range.
            // Well below Nyquist keeps the tan()-based g coefficient accurate
            // (at 0.45*Nyquist the bilinear warp already compresses the audible
            // top octave) while the resonance self-peak stays tame.
            const float safeCutoff = juce::jlimit (20.0f,
                juce::jmin (20000.0f, 0.40f * static_cast<float> (sr)), modulatedCutoff);
            const float g = std::tan (juce::MathConstants<float>::pi * safeCutoff
                                      / static_cast<float> (sr));
            const float damping = juce::jlimit (0.08f, 2.0f,
                2.0f - 1.92f * juce::jlimit (0.0f, 1.0f,
                                                resonanceSmooth.getNextValue()));
            const float denominator = 1.0f + g * (g + damping);
            const int selectedFilter = filterMode != nullptr
                ? juce::jlimit (0, 2, juce::roundToInt (filterMode->load())) : 0;
            const std::array<float, 2> stereoInput { unisonLeft, unisonRight };
            const int channels = juce::jmin (2, buffer.getNumChannels());
            for (int channel = 0; channel < channels; ++channel)
            {
                const float filterInput = stereoInput[static_cast<size_t> (channel)];
                // When the oversampled global stage is active it owns the
                // saturation; saturating here first would alias at the host
                // rate before the upsampler can lift the band limit.
                const float hostRateDrive = p.oversamplingBypassed() ? driveAmount : 0.0f;
                const float x = hostRateDrive > 1.0e-5f
                    ? p.applyDriveCurve (filterInput, 1.0f + 4.0f * hostRateDrive,
                                         p.selectedDriveCurve())
                    : filterInput;
                const size_t index = static_cast<size_t> (channel);
                const float v1 = (g * (x - svfIc2[index]) + svfIc1[index]) / denominator;
                const float v2 = svfIc2[index] + g * v1;
                svfIc1[index] = 2.0f * v1 - svfIc1[index];
                svfIc2[index] = 2.0f * v2 - svfIc2[index];
                const float low = v2;
                const float band = v1;
                const float high = x - damping * band - low;
                state[index] = selectedFilter == 1 ? high : selectedFilter == 2 ? band : low;
                state[index] = std::isfinite (state[index]) ? juce::jlimit (-8.0f, 8.0f, state[index]) : 0.0f;
                // Filter first, then apply the amp envelope/output gain.  This
                // preserves the filter's natural ring without feeding envelope
                // discontinuities into its integrators.
                buffer.addSample (channel, startSample + i, state[index] * voiceGain);
            }
        }

        if (! env.isActive())
            clearCurrentNote();
    }

private:
    void updateEnvelopeParameters()
    {
        juce::ADSR::Parameters parameters;
        parameters.attack = p.apvts.getRawParameterValue (ParamIDs::attack)->load();
        parameters.decay = p.apvts.getRawParameterValue (ParamIDs::decay)->load();
        parameters.sustain = p.apvts.getRawParameterValue (ParamIDs::sustain)->load();
        parameters.release = p.apvts.getRawParameterValue (ParamIDs::release)->load();
        env.setParameters (parameters);
    }

    EonMiniEEFProcessor& p;
    juce::ADSR env;
    double sr = 44100.0;
    int note = 60;
    static constexpr int maxUnisonVoices = 8;
    float vel = 0.0f, lfoPhase = 0.0f;
    std::array<float, 2> state {}, svfIc1 {}, svfIc2 {};
    std::uint32_t noiseState = 0x6d2b79f5u;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> resonanceSmooth;
    juce::SmoothedValue<float> gainSmooth, driveSmooth, noiseSmooth,
                               unisonDetuneSmooth, unisonSpreadSmooth, amDepthSmooth;
    std::array<juce::SmoothedValue<float>, 4> levelSmooth, fineSmooth, panSmooth, pulseWidthSmooth;
    std::array<float, maxUnisonVoices> ph1 {}, ph2 {}, ph3 {}, ph4 {};
};
struct EonSound:juce::SynthesiserSound{bool appliesToNote(int)override{return true;}bool appliesToChannel(int)override{return true;}};
struct NoteOnlySynthesiser final : juce::Synthesiser
{
    explicit NoteOnlySynthesiser (EonMiniEEFProcessor& processor) : p (processor) {}

    void renderNotesOnlyNoLock (juce::AudioBuffer<float>& output,
                                const juce::MidiBuffer& midi,
                                int startSample, int numSamples) noexcept
    {
        auto event = midi.findNextSamplePosition (startSample);
        const int endSample = startSample + numSamples;

        while (event != midi.cend())
        {
            const auto metadata = *event;
            if (metadata.samplePosition >= endSample)
                break;

            const int eventSample = juce::jmax (startSample, metadata.samplePosition);
            if (eventSample > startSample)
            {
                renderVoices (output, startSample, eventSample - startSample);
                startSample = eventSample;
            }

            handleNoteEventNoLock (metadata.getMessage());
            ++event;
        }

        if (startSample < endSample)
            renderVoices (output, startSample, endSample - startSample);
    }

    void resetVoicesNoLock() noexcept
    {
        for (auto* voice : voices)
            if (auto* eonVoice = dynamic_cast<EonVoice*> (voice))
                eonVoice->resetForStateLoad();
        for (auto& channel : heldVelocity)
            channel.fill (0.0f);
        for (auto& channel : heldSequence)
            channel.fill (0);
        nextSequence = 0;
    }

private:
    bool isMonoModeNoLock() const noexcept
    {
        if (const auto* mode = p.apvts.getRawParameterValue (ParamIDs::voiceMode))
            return mode->load (std::memory_order_relaxed) > 0.5f;
        return false;
    }

    void rememberNoteOnNoLock (int channel, int noteNumber, float velocity) noexcept
    {
        const auto channelIndex = static_cast<size_t> (juce::jlimit (1, 16, channel) - 1);
        const auto noteIndex = static_cast<size_t> (juce::jlimit (0, 127, noteNumber));
        heldVelocity[channelIndex][noteIndex] = velocity;
        heldSequence[channelIndex][noteIndex] = ++nextSequence;
    }

    void forgetNoteNoLock (int channel, int noteNumber) noexcept
    {
        const auto channelIndex = static_cast<size_t> (juce::jlimit (1, 16, channel) - 1);
        const auto noteIndex = static_cast<size_t> (juce::jlimit (0, 127, noteNumber));
        heldVelocity[channelIndex][noteIndex] = 0.0f;
        heldSequence[channelIndex][noteIndex] = 0;
    }

    bool findLatestHeldNoteNoLock (int& channel, int& noteNumber, float& velocity) const noexcept
    {
        std::uint32_t latestSequence = 0;
        bool found = false;
        for (int channelIndex = 0; channelIndex < 16; ++channelIndex)
            for (int noteIndex = 0; noteIndex < 128; ++noteIndex)
            {
                const auto sequence = heldSequence[static_cast<size_t> (channelIndex)]
                    [static_cast<size_t> (noteIndex)];
                if (sequence > latestSequence)
                {
                    latestSequence = sequence;
                    channel = channelIndex + 1;
                    noteNumber = noteIndex;
                    velocity = heldVelocity[static_cast<size_t> (channelIndex)]
                        [static_cast<size_t> (noteIndex)];
                    found = true;
                }
            }
        return found;
    }

    void stopAllVoicesImmediatelyNoLock() noexcept
    {
        for (auto* voice : voices)
            if (voice->isVoiceActive())
                stopVoice (voice, 1.0f, false);
    }

    juce::SynthesiserVoice* oldestVoiceMatching (bool releasedOnly) const noexcept
    {
        juce::SynthesiserVoice* oldest = nullptr;
        for (auto* voice : voices)
        {
            if (releasedOnly && ! voice->isPlayingButReleased())
                continue;
            if (! releasedOnly && ! voice->isVoiceActive())
                continue;
            if (oldest == nullptr || voice->wasStartedBefore (*oldest))
                oldest = voice;
        }
        return oldest;
    }

    juce::SynthesiserVoice* chooseVoiceNoLock() const noexcept
    {
        for (auto* voice : voices)
            if (! voice->isVoiceActive())
                return voice;

        if (auto* released = oldestVoiceMatching (true))
            return released;
        return oldestVoiceMatching (false);
    }

    void handleNoteEventNoLock (const juce::MidiMessage& message) noexcept
    {
        if (message.isNoteOn())
        {
            const int channel = message.getChannel();
            const int noteNumber = message.getNoteNumber();
            rememberNoteOnNoLock (channel, noteNumber, message.getFloatVelocity());

            if (isMonoModeNoLock())
            {
                stopAllVoicesImmediatelyNoLock();
                auto* sound = sounds.isEmpty() ? nullptr : sounds.getUnchecked (0).get();
                startVoice (chooseVoiceNoLock(), sound, channel, noteNumber,
                            message.getFloatVelocity());
                return;
            }

            for (auto* voice : voices)
                if (voice->getCurrentlyPlayingNote() == noteNumber
                    && voice->isPlayingChannel (channel))
                    stopVoice (voice, 1.0f, true);

            auto* sound = sounds.isEmpty() ? nullptr : sounds.getUnchecked (0).get();
            startVoice (chooseVoiceNoLock(), sound, channel, noteNumber,
                        message.getFloatVelocity());
            return;
        }

        if (message.isNoteOff())
        {
            const int channel = message.getChannel();
            const int noteNumber = message.getNoteNumber();
            forgetNoteNoLock (channel, noteNumber);

            if (isMonoModeNoLock())
            {
                for (auto* voice : voices)
                {
                    if (voice->getCurrentlyPlayingNote() != noteNumber
                        || ! voice->isPlayingChannel (channel))
                        continue;

                    int fallbackChannel = 0;
                    int fallbackNote = 0;
                    float fallbackVelocity = 0.0f;
                    if (findLatestHeldNoteNoLock (fallbackChannel, fallbackNote, fallbackVelocity))
                    {
                        stopVoice (voice, 1.0f, false);
                        auto* sound = sounds.isEmpty() ? nullptr : sounds.getUnchecked (0).get();
                        startVoice (chooseVoiceNoLock(), sound, fallbackChannel, fallbackNote,
                                    fallbackVelocity);
                    }
                    else
                    {
                        voice->setKeyDown (false);
                        stopVoice (voice, message.getFloatVelocity(), true);
                    }
                    return;
                }
                return;
            }

            for (auto* voice : voices)
            {
                if (voice->getCurrentlyPlayingNote() != noteNumber
                    || ! voice->isPlayingChannel (channel))
                    continue;
                voice->setKeyDown (false);
                stopVoice (voice, message.getFloatVelocity(), true);
            }
        }
    }

    EonMiniEEFProcessor& p;
    std::array<std::array<float, 128>, 16> heldVelocity {};
    std::array<std::array<std::uint32_t, 128>, 16> heldSequence {};
    std::uint32_t nextSequence = 0;
};
EonMiniEEFProcessor::EonMiniEEFProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),apvts(*this,nullptr,"PARAMETERS",createParameterLayout()),synth(std::make_unique<NoteOnlySynthesiser>(*this)){synth->setMinimumRenderingSubdivisionSize(1,false);synth->addSound(new EonSound());for(int i=0;i<16;++i)synth->addVoice(new EonVoice(*this));setLatencySamples(6);}
juce::AudioProcessorValueTreeState::ParameterLayout EonMiniEEFProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    const auto addWave = [&parameters] (const char* id, const char* name)
    {
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            id, name, juce::StringArray { "Saw", "Square", "Triangle", "Sine" }, 0));
    };
    const auto addFloat = [&parameters] (const char* id, float minimum,
                                         float maximum, float defaultValue)
    {
        parameters.push_back (std::make_unique<juce::AudioParameterFloat> (
            id, id, juce::NormalisableRange<float> (minimum, maximum), defaultValue));
    };

    addWave (ParamIDs::osc1Wave, "Osc 1 Wave");
    addWave (ParamIDs::osc2Wave, "Osc 2 Wave");
    addWave (ParamIDs::osc3Wave, "Osc 3 Wave");
    addWave (ParamIDs::osc4Wave, "Osc 4 Wave");
    const auto addOscillator = [&parameters, &addFloat] (const char* level, const char* coarse,
                                            const char* fine, const char* phase,
                                            const char* pan, const char* pulseWidth,
                                            float defaultLevel, float defaultCoarse)
    {
        addFloat (level, 0.0f, 1.0f, defaultLevel);
        parameters.push_back (std::make_unique<juce::AudioParameterInt> (
            coarse, coarse, -24, 24, juce::roundToInt (defaultCoarse)));
        addFloat (fine, -100.0f, 100.0f, 0.0f);
        addFloat (phase, 0.0f, 1.0f, 0.0f);
        addFloat (pan, -1.0f, 1.0f, 0.0f);
        addFloat (pulseWidth, 0.05f, 0.95f, 0.5f);
    };
    addOscillator (ParamIDs::osc1Level, ParamIDs::osc1Coarse, ParamIDs::osc1Fine,
                   ParamIDs::osc1Phase, ParamIDs::osc1Pan, ParamIDs::osc1PulseWidth, 0.5f, 0.0f);
    addOscillator (ParamIDs::osc2Level, ParamIDs::osc2Coarse, ParamIDs::osc2Fine,
                   ParamIDs::osc2Phase, ParamIDs::osc2Pan, ParamIDs::osc2PulseWidth, 0.5f, 7.0f);
    addOscillator (ParamIDs::osc3Level, ParamIDs::osc3Coarse, ParamIDs::osc3Fine,
                   ParamIDs::osc3Phase, ParamIDs::osc3Pan, ParamIDs::osc3PulseWidth, 0.0f, 0.0f);
    addOscillator (ParamIDs::osc4Level, ParamIDs::osc4Coarse, ParamIDs::osc4Fine,
                   ParamIDs::osc4Phase, ParamIDs::osc4Pan, ParamIDs::osc4PulseWidth, 0.0f, 0.0f);
    addFloat (ParamIDs::noiseMix, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamIDs::unisonVoices, "Unison Voices", 1, 8, 1));
    addFloat (ParamIDs::unisonDetune, 0.0f, 24.0f, 0.0f);
    addFloat (ParamIDs::unisonSpread, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::unisonPhase, 0.0f, 1.0f, 1.0f);
    addFloat (ParamIDs::cutoff, 20.0f, 20000.0f, 12000.0f);
    addFloat (ParamIDs::resonance, 0.0f, 1.0f, 0.15f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::filterMode, "Filter Mode", juce::StringArray { "LPF", "HPF", "BPF" }, 0));
    addFloat (ParamIDs::attack, 0.001f, 5.0f, 0.01f);
    addFloat (ParamIDs::decay, 0.001f, 5.0f, 0.3f);
    addFloat (ParamIDs::sustain, 0.0f, 1.0f, 0.8f);
    addFloat (ParamIDs::release, 0.001f, 8.0f, 0.4f);
    addFloat (ParamIDs::gain, 0.0f, 1.0f, 0.7f);
    addFloat (ParamIDs::drive, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::driveCurve, "Drive Curve",
        juce::StringArray { "Symmetric", "Asymmetric", "Tube" }, 0));
    addFloat (ParamIDs::ampSat, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::lfoRate, 0.01f, 30.0f, 2.0f);
    addFloat (ParamIDs::lfoDepth, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::lfoPitch, 0.0f, 12.0f, 0.0f);
    addFloat (ParamIDs::amDepth, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::velocityAmount, 0.0f, 1.0f, 1.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterBool> (
        ParamIDs::lfoSync, "LFO Sync", false));
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lfoDivision, "LFO Division",
        juce::StringArray { "4/1", "2/1", "1/1", "1/2", "1/4" }, 2));
    addFloat (ParamIDs::keyTracking, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::voiceMode, "Voice Mode", juce::StringArray { "Poly", "Mono" }, 0));
    addFloat (ParamIDs::fxWet, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::delayTime, 0.01f, 2.0f, 0.35f);
    addFloat (ParamIDs::delayFeedback, 0.0f, 0.9f, 0.25f);
    addFloat (ParamIDs::chorusDepth, 0.0f, 0.02f, 0.004f);
    addFloat (ParamIDs::chorusRate, 0.05f, 8.0f, 0.25f);
    addFloat (ParamIDs::chorusMix, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::reverbMix, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::oversampling, "Oversampling",
        juce::StringArray { "1x Eco", "2x Quality", "4x High" }, 0));

    const juce::StringArray modSources {
        "Off", "LFO", "Amp Env", "Velocity", "Osc 1"
    };
    const juce::StringArray modDestinations {
        "Off", "Pitch", "Cutoff", "Amp", "Osc 2 FM"
    };
    for (int slot = 0; slot < 4; ++slot)
    {
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::modSource (slot), "Mod Source " + juce::String (slot + 1),
            modSources, 0));
        parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
            ParamIDs::modDestination (slot), "Mod Destination " + juce::String (slot + 1),
            modDestinations, 0));
        addFloat (ParamIDs::modAmount (slot), -1.0f, 1.0f, 0.0f);
    }

    return { parameters.begin(), parameters.end() };
}
void EonMiniEEFProcessor::processOversampledOutput (juce::AudioBuffer<float>& buffer,
                                                    float driveAmount) noexcept
{
    const auto* modeParameter = apvts.getRawParameterValue (ParamIDs::oversampling);
    const int mode = modeParameter != nullptr
        ? juce::jlimit (0, 2, juce::roundToInt (modeParameter->load())) : 0;
    const bool willOversample = mode != 0;
    oversamplingActive.store (willOversample, std::memory_order_relaxed);

    // A host may legally deliver a block larger than the prepareToPlay hint.
    // Bypass the optional stage in that case rather than resizing on the audio
    // thread; the normal 1x path remains fully compatible and allocation-free.
    int activeOversamplingLatency = 0;
    if (willOversample && buffer.getNumSamples() > 0
        && buffer.getNumSamples() <= oversamplingBlockSize)
    {
        auto block = juce::dsp::AudioBlock<float> (buffer);
        auto& oversampler = mode == 1 ? oversampling2x : oversampling4x;
        activeOversamplingLatency = juce::roundToInt (oversampler.getLatencyInSamples());
        auto highRateBlock = oversampler.processSamplesUp (block);

        // The global stage must be the only saturation when oversampling is
        // active: driving at the host rate first would alias before the
        // upsampler and defeat the purpose of the quality mode.  The voice
        // loop skips its tanh() when an oversampled stage will run, and this
        // stage applies the full drive amount above the host rate.
        const float amount = juce::jlimit (0.0f, 1.0f, driveAmount);
        if (amount > 0.0f)
        {
            const float gain = 1.0f + 4.0f * amount;
            const auto* curveParameter = apvts.getRawParameterValue (ParamIDs::driveCurve);
            const int curveMode = curveParameter != nullptr
                ? juce::jlimit (0, 2, juce::roundToInt (curveParameter->load())) : 0;
            for (size_t channel = 0; channel < highRateBlock.getNumChannels(); ++channel)
            {
                auto* samples = highRateBlock.getChannelPointer (channel);
                for (size_t sample = 0; sample < highRateBlock.getNumSamples(); ++sample)
                {
                    const float input = std::isfinite (samples[sample]) ? samples[sample] : 0.0f;
                    const float shaped = applyDriveCurve (input, gain, curveMode);
                    samples[sample] = std::isfinite (shaped) ? shaped : 0.0f;
                }
            }
        }

        oversampler.processSamplesDown (block);
    }

    // Report one fixed latency to the host and delay the 1x/2x paths to match
    // the 4x path.  This avoids timing jumps and comb filtering when quality
    // changes without calling host-notification APIs on the audio thread.
    const int compensation = juce::jlimit (0, latencyBufferCapacity - 1,
        fixedLatencySamples - activeOversamplingLatency);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const int readPosition = (latencyWritePosition - compensation
                                  + latencyBufferCapacity) % latencyBufferCapacity;
        for (int channel = 0; channel < juce::jmin (2, buffer.getNumChannels()); ++channel)
        {
            auto& line = latencyBuffer[static_cast<size_t> (channel)];
            const float input = buffer.getSample (channel, sample);
            line[static_cast<size_t> (latencyWritePosition)] = input;
            buffer.setSample (channel, sample,
                              compensation == 0 ? input : line[static_cast<size_t> (readPosition)]);
        }
        latencyWritePosition = (latencyWritePosition + 1) % latencyBufferCapacity;
    }
}

float EonMiniEEFProcessor::applyDriveCurve (float input, float gainMultiplier,
                                            int curveMode) noexcept
{
    // Mode 0 preserves the legacy symmetric tanh() so drive=0 presets and
    // the THD baseline report stay bit-comparable.
    if (curveMode == 1)
    {
        // Asymmetric: 78% positive / 122% negative conduction. The DC offset
        // this introduces is measured (not assumed) by the THD probe's DC
        // column. The one-pole highpass below removes the measured DC before
        // the downsampling anti-alias filter can smear it into the spectrum;
        // a stateless probe window (ThdProbe) cannot average it away, so this
        // curve must clean itself.
        const float biased = input + 0.09f;
        const float shaped = std::tanh (biased * gainMultiplier * 0.95f)
                           - std::tanh (0.09f * gainMultiplier * 0.95f);
        // Renormalise so quiet passages keep roughly the legacy loudness.
        float output = shaped * 1.06f;
        // Per-curve one-pole highpass at ~5 Hz. Coefficient computed inline:
        // y[n] = 0.9993 * (y[n-1] + x[n] - x[n-1]) is a standard DC killer at
        // 48 kHz (and slightly higher at 96k+, which is acceptable here).
        const float hp = asymHpY * 0.9993f + output - asymHpX;
        asymHpX = output;
        asymHpY = hp;
        return hp;
    }
    if (curveMode == 2)
    {
        // "Tube": symmetric tanh followed by a soft second-stage knee. The
        // cascade emphasises 3rd-order products slightly over deep clipping,
        // which keeps loud chords dense instead of raspy.
        const float first = std::tanh (input * gainMultiplier);
        return std::tanh (first * 1.25f) * 0.86f;
    }
    return std::tanh (input * gainMultiplier);
}

void EonMiniEEFProcessor::resetDriveCurveState() noexcept
{
    asymHpX = 0.0f;
    asymHpY = 0.0f;
}

float EonMiniEEFProcessor::applyAmpSaturation (float input, float amount) noexcept
{
    // Very gentle knee that starts only above 0.7 so clean playing (which
    // peaks around 0.4 with factory presets) stays untouched while loud
    // chords get a slight density lift.  amount is user-controlled 0..1 and
    // 0 keeps the path bit-identical to the legacy signal.
    if (amount <= 1.0e-4f)
        return input;
    const float magnitude = std::abs (input);
    if (magnitude <= 0.7f)
        return input;
    const float excess = magnitude - 0.7f;
    const float knee = std::tanh (excess * (0.6f + 1.8f * amount)) / (0.6f + 1.8f * amount);
    const float shapedMagnitude = 0.7f + knee;
    return std::copysign (juce::jmin (magnitude, shapedMagnitude), input);
}

void EonMiniEEFProcessor::prepareToPlay(double sr,int samplesPerBlock){sampleRate=sr;synth->setCurrentPlaybackSampleRate(sr);for(int i=0;i<synth->getNumVoices();++i)dynamic_cast<EonVoice*>(synth->getVoice(i))->setSR(sr);fxDelay.setSize(2,juce::jmax(1,(int)(sr*2.0)),false,true,true);fxDelay.clear();fxWritePosition=0;chorusPhase=0;reverbL.fill(0);reverbR.fill(0);dcInput.fill(0.0f);dcOutput.fill(0.0f);resetDriveCurveState();oversamplingBlockSize=juce::jmax(1,samplesPerBlock);oversampling2x.initProcessing(static_cast<size_t>(oversamplingBlockSize));oversampling4x.initProcessing(static_cast<size_t>(oversamplingBlockSize));oversampling2x.reset();oversampling4x.reset();fixedLatencySamples=juce::jlimit(1,latencyBufferCapacity-1,juce::roundToInt(juce::jmax(oversampling2x.getLatencyInSamples(),oversampling4x.getLatencyInSamples())));latencyWritePosition=0;for(auto& channel:latencyBuffer)channel.fill(0.0f);setLatencySamples(fixedLatencySamples);}
bool EonMiniEEFProcessor::isBusesLayoutSupported(const BusesLayout&l)const{const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();return in.isDisabled()&&(out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo());}
void EonMiniEEFProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    juce::ScopedNoDenormals nd;
    if (dspResetRequested.exchange (false, std::memory_order_acq_rel))
    {
        static_cast<NoteOnlySynthesiser*> (synth.get())->resetVoicesNoLock();
        fxDelay.clear();
        fxWritePosition = 0;
        chorusPhase = 0.0f;
        reverbL.fill (0.0f);
        reverbR.fill (0.0f);
        dcInput.fill (0.0f);
        dcOutput.fill (0.0f);
        resetDriveCurveState();
        latencyWritePosition = 0;
        for (auto& channel : latencyBuffer)
            channel.fill (0.0f);
        oversampling2x.reset();
        oversampling4x.reset();
    }
    if (auto* playhead = getPlayHead())
        if (auto position = playhead->getPosition())
            if (auto bpm = position->getBpm();
                bpm.hasValue() && std::isfinite (*bpm) && *bpm > 0.0)
                hostBpm.store (static_cast<float> (juce::jlimit (20.0, 300.0, *bpm)),
                               std::memory_order_relaxed);
    // This is an instrument: start from silence and render the MIDI-driven
    // voices before the global ambience effects process the block.
    b.clear();
    static_cast<NoteOnlySynthesiser*> (synth.get())->renderNotesOnlyNoLock (
        b, m, 0, b.getNumSamples());
    auto value=[this](const char* id,float fallback){auto* p=apvts.getRawParameterValue(id);return p!=nullptr?p->load():fallback;};
    const float wet=value(ParamIDs::fxWet,0.0f), delaySeconds=value(ParamIDs::delayTime,0.35f), feedback=value(ParamIDs::delayFeedback,0.25f), depth=value(ParamIDs::chorusDepth,0.004f), rate=value(ParamIDs::chorusRate,0.25f), chorus=value(ParamIDs::chorusMix,0.0f), reverb=value(ParamIDs::reverbMix,0.0f);
    const float ampSatAmount=value(ParamIDs::ampSat,0.0f);
    const int delayLength=juce::jlimit(1,fxDelay.getNumSamples()-1,(int)(delaySeconds*(float)sampleRate));
    float blockPeakL = 0.0f, blockPeakR = 0.0f;
    for(int i=0;i<b.getNumSamples();++i){float dryL=b.getSample(0,i),dryR=b.getNumChannels()>1?b.getSample(1,i):dryL;float delayedL=fxDelay.getSample(0,(fxWritePosition-delayLength+fxDelay.getNumSamples())%fxDelay.getNumSamples()),delayedR=fxDelay.getSample(1,(fxWritePosition-delayLength+fxDelay.getNumSamples())%fxDelay.getNumSamples());fxDelay.setSample(0,fxWritePosition,dryL+delayedL*feedback);fxDelay.setSample(1,fxWritePosition,dryR+delayedR*feedback);float lfo=std::sin(chorusPhase),chorusL=fxDelay.getSample(0,(fxWritePosition-(int)((0.0125f+depth*lfo)*sampleRate)+fxDelay.getNumSamples()*2)%fxDelay.getNumSamples()),chorusR=fxDelay.getSample(1,(fxWritePosition-(int)((0.0125f-depth*lfo)*sampleRate)+fxDelay.getNumSamples()*2)%fxDelay.getNumSamples());chorusPhase=std::fmod(chorusPhase+juce::MathConstants<float>::twoPi*rate/(float)sampleRate,juce::MathConstants<float>::twoPi);constexpr float reverbTapScale=1.0f/1.75f;float rvL=(reverbL[0]*0.7f+reverbL[1]*0.5f+reverbL[2]*0.35f+reverbL[3]*0.2f)*reverbTapScale;float rvR=(reverbR[0]*0.7f+reverbR[1]*0.5f+reverbR[2]*0.35f+reverbR[3]*0.2f)*reverbTapScale;for(int j=3;j>0;--j){reverbL[j]=reverbL[j-1];reverbR[j]=reverbR[j-1];}reverbL[0]=dryL*0.35f+rvL*0.65f;reverbR[0]=dryR*0.35f+rvR*0.65f;float fxL=dryL+wet*(delayedL+chorus*(chorusL-dryL)+reverb*(rvL-dryL)),fxR=dryR+wet*(delayedR+chorus*(chorusR-dryR)+reverb*(rvR-dryR));// Final safety stage: remove subsonic DC while retaining state across blocks,
        // then apply a bounded soft limiter.  Non-finite values are muted before
        // entering the stateful stages so one bad sample cannot poison the stream.
        const float raw[2] = { fxL, fxR };
        for (int channel = 0; channel < b.getNumChannels() && channel < 2; ++channel)
        {
            float sample = std::isfinite(raw[channel]) ? raw[channel] : 0.0f;
            sample = juce::jlimit(-4.0f, 4.0f, sample);
            const float blocked = sample - dcInput[static_cast<size_t>(channel)]
                                + 0.995f * dcOutput[static_cast<size_t>(channel)];
            dcInput[static_cast<size_t>(channel)] = sample;
            dcOutput[static_cast<size_t>(channel)] = std::isfinite(blocked) ? blocked : 0.0f;
            const float dcBlocked = dcOutput[static_cast<size_t>(channel)];
            const float saturated = applyAmpSaturation (dcBlocked, ampSatAmount);
            // Preserve exact small-signal dynamics.  Only excursions above the
            // safety ceiling enter a smooth, bounded limiting knee.
            const float magnitude = std::abs (saturated);
            const float limited = magnitude <= 1.0f
                ? saturated
                : std::copysign (1.0f + std::tanh (magnitude - 1.0f), saturated);
            const float output = std::isfinite(limited) ? limited : 0.0f;
            b.setSample(channel, i, output);
            if (channel == 0) blockPeakL = juce::jmax (blockPeakL, std::abs (output));
            else blockPeakR = juce::jmax (blockPeakR, std::abs (output));
        }
        fxWritePosition=(fxWritePosition+1)%fxDelay.getNumSamples();}
    processOversampledOutput (b, value (ParamIDs::drive, 0.0f));
    peakLeft.store (blockPeakL);
    peakRight.store (blockPeakR);
}
juce::AudioProcessorEditor* EonMiniEEFProcessor::createEditor() { return new EonMiniEEFEditor (*this); }

void EonMiniEEFProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();
    sanitizeLegacyState (state);
    state.setProperty ("stateFormatVersion", currentStateFormatVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void EonMiniEEFProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        const int stateVersion = static_cast<int> (
            state.getProperty ("stateFormatVersion", 0));
        if (stateVersion < currentStateFormatVersion)
            migrateLegacyState (state);
        sanitizeLegacyState (state);
        state.setProperty ("stateFormatVersion", currentStateFormatVersion, nullptr);
        apvts.replaceState (state);
        dspResetRequested.store (true, std::memory_order_release);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EonMiniEEFProcessor(); }
