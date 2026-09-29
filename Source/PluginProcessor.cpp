#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdint>

namespace
{
    constexpr int currentStateFormatVersion = 1;

    // Odd-symmetric soft clip for the Analog wave.  It replaces a per-sample
    // std::tanh, which measured as the single largest cost when this wave was
    // first added: the 8-unison worst case went from 61% to 78% of the block
    // budget with tanh, and back to 58% with this.  Note the asymptote is 3,
    // not 1, so the caller scales by the 0.92 factor rather than treating this
    // as a unity-gain clip.
    //
    // The same substitution was tried on the filter drive's tanh and reverted:
    // it bought 1.5 percentage points but moved one preset's RMS by 1.6%, and
    // a 1.5% CPU trade is not worth a measurable output change.
    static float softClip (float x) noexcept
    {
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

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

    inline float cubicInterpolate (float y0, float y1, float y2, float y3,
                                   float fraction) noexcept
    {
        // Catmull-Rom form: zero-order phase error at the centre sample and
        // smooth first derivative across the four-point read window.
        const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float a2 = -0.5f * y0 + 0.5f * y2;
        return ((a0 * fraction + a1) * fraction + a2) * fraction + y1;
    }

    // Waveform 4, the default.  It is not a copy of any one instrument: it is
    // the set of analogue-oscillator traits common to the transistor
    // oscillators worth imitating, applied to this engine's existing shapes.
    float analogOscillatorSample (float phase, float drive) noexcept
    {
        // An asymmetric triangle core.  A real triangle's rise and fall slopes
        // differ, so its even harmonics do not cancel the way the symmetric
        // trapezoid in wave 2 cancels them.  The mean stays at zero, so a
        // tilted core does not pump the filter and amp stage with DC.
        constexpr float riseFraction = 0.42f;
        const float core = phase < riseFraction
            ? -1.0f + 2.0f * phase / riseFraction
            : 1.0f - 2.0f * (phase - riseFraction) / (1.0f - riseFraction);

        // The soft clip is what moves the spectrum off the ideal 1/n series.
        // Waves 0-3 measure within 0.1 dB of the analytic series, which is
        // mathematically right and sounds synthetic; this is the knob that
        // gives the default wave a life of its own.  The 0.92 factor is a
        // per-shape peak, so this wave does not sit at the same level as the
        // mathematically normalised ones.
        //
        // The clip is a rational curve rather than std::tanh: the measured CPU
        // cost of a per-sample tanh pushed the 8-unison worst case from 61% to
        // 78% of the block budget, and this form has the same odd symmetry and
        // bounded range for a few multiplies.  `drive` arrives already scaled.
        const float x = drive * core;
        const float x2 = x * x;
        return 0.92f * x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    float oscillatorSample (float phase, float increment, int waveform, float pulseWidth,
                            float analogDrive)
    {
        phase = phase - std::floor (phase);
        const auto dt = juce::jlimit (1.0e-5f, 0.49f, increment);

        switch (juce::jlimit (0, 4, waveform))
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
                return 2.0f * phase - 1.0f + blep (phase, dt);

            case 4: return analogOscillatorSample (phase, analogDrive);

            default:
                return 2.0f * phase - 1.0f + blep (phase, dt);
        }
    }

    float processTptSvf (float input, float g, float damping, float denominator,
                         int filterMode, float& ic1, float& ic2) noexcept
    {
        const float v1 = (g * (input - ic2) + ic1) / denominator;
        const float v2 = ic2 + g * v1;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        const float low = v2;
        const float band = v1;
        const float high = input - damping * band - low;
        return filterMode == 1 ? high : filterMode == 2 ? band : low;
    }

    template <size_t Capacity>
    float processReverbAllpass (float input, std::array<float, Capacity>& buffer,
                                int& position, int length) noexcept
    {
        constexpr float allpassGain = 0.5f;
        const float delayed = buffer[static_cast<size_t> (position)];
        const float output = delayed - allpassGain * input;
        buffer[static_cast<size_t> (position)] = input + allpassGain * output;
        // Wrapping by comparison rather than by % : this runs four times per
        // sample, and the modulo is a real integer division where a compare
        // and a select is a handful of cycles.
        position = position + 1 == length ? 0 : position + 1;
        return output;
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
                                 float velocity, float filterEnvelope) noexcept
    {
        switch (juce::jlimit (0, 5, source))
        {
            case 1: return lfo;
            case 2: return envelope;
            case 3: return velocity;
            case 5: return filterEnvelope;
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
        svfIc3.fill (0.0f);
        svfIc4.fill (0.0f);
        filterInputPrev.fill (0.0f);
        // Key tracking depends only on the held note, so its exponential is
        // evaluated here rather than once per sample inside the render loop.
        {
            const auto* keyTrackingParameter =
                p.apvts.getRawParameterValue (ParamIDs::keyTracking);
            const float keyTrack = juce::jlimit (0.0f, 1.0f,
                keyTrackingParameter != nullptr ? keyTrackingParameter->load() : 0.0f);
            keyRatioForNote = std::exp2f (keyTrack * juce::jlimit (-2.0f, 2.0f,
                (static_cast<float> (midiNote) - 60.0f) / 24.0f));
        }
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
        const auto* driftParameter = p.apvts.getRawParameterValue (ParamIDs::unisonDrift);
        driftLevel = driftParameter != nullptr
            ? juce::jlimit (0.0f, 1.0f, driftParameter->load()) : 0.0f;
        // Non-zero deterministic seed avoids the xorshift zero lock-up while
        // keeping note retriggers reproducible for offline rendering.
        noiseState = 0x9e3779b9u ^ (static_cast<std::uint32_t> (midiNote + 1) * 0x85ebca6bu);
        if (noiseState == 0)
            noiseState = 0x6d2b79f5u;
        // Roadmap P2: per-voice analog tolerance.  One deterministic seed per
        // note fixes this voice's cutoff / envelope-time / level offsets, so
        // a poly chord spreads the way separate voice cards would while the
        // same note keeps reproducing exactly across offline renders.
        varianceAmount = 0.0f;
        if (const auto* varianceParameter = p.apvts.getRawParameterValue (ParamIDs::voiceVariance))
            varianceAmount = juce::jlimit (0.0f, 1.0f, varianceParameter->load());
        std::uint32_t varianceState = noiseState ^ 0xa136aaadu;
        if (varianceState == 0)
            varianceState = 0x6d2b79f5u;
        // Zero amount keeps every multiplier at exactly 1.0 / +0.0, so legacy
        // presets remain bit-identical instead of merely close.
        cutoffVariance = 1.0f;
        envelopeVariance = 1.0f;
        levelVariance = 1.0f;
        pitchVarianceRatio = 1.0f;
        if (varianceAmount > 1.0e-5f)
        {
            constexpr float cutoffCentsRange = 40.0f;   // +/-0.40 semitones
            constexpr float envelopeTimeRange = 0.25f;  // +/-25% stage times
            constexpr float levelRange = 0.15f;         // +/-15% voice level
            constexpr float pitchCentsRange = 12.0f;    // +/-12 cents, Diva-style
                                                        // voice-card tuning offset
            const float cutoffCents = cutoffCentsRange * varianceAmount
                * (2.0f * unitRandomFromState (varianceState) - 1.0f);
            cutoffVariance = std::pow (2.0f, cutoffCents / 1200.0f);
            envelopeVariance = 1.0f + envelopeTimeRange * varianceAmount
                * (2.0f * unitRandomFromState (varianceState) - 1.0f);
            levelVariance = 1.0f + levelRange * varianceAmount
                * (2.0f * unitRandomFromState (varianceState) - 1.0f);
            // Pitch is drawn last so the three offsets above keep their
            // seeded values for existing variance>0 presets.
            const float pitchCents = pitchCentsRange * varianceAmount
                * (2.0f * unitRandomFromState (varianceState) - 1.0f);
            pitchVarianceRatio = std::pow (2.0f, pitchCents / 1200.0f);
        }
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
        filterDriveSmooth.setCurrentAndTargetValue (currentValue (ParamIDs::filterDrive, 0.0f));
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
        // Per-layer drift wanderers restart from the same deterministic seed
        // chain as the noise source, so offline renders stay reproducible.
        for (int i = 0; i < maxUnisonVoices; ++i)
        {
            driftState[static_cast<size_t> (i)]
                = noiseState ^ (static_cast<std::uint32_t> (i + 1) * 0x9e3779b9u);
            if (driftState[static_cast<size_t> (i)] == 0)
                driftState[static_cast<size_t> (i)] = 0x6d2b79f5u;
            driftValue[static_cast<size_t> (i)] = 0.0f;
        }
        sahState = noiseState ^ 0x19326465u;
        if (sahState == 0)
            sahState = 0x6d2b79f5u;
        // Roadmap P2: an independent right-channel seed decorrelates the two
        // sides of the noise layer, which previously fed the identical sample
        // to both channels.  The left seed chain is unchanged, so existing
        // left-channel measurements keep their baseline.
        noiseStateRight = noiseState ^ 0x27d4eb2fu;
        if (noiseStateRight == 0)
            noiseStateRight = 0x6d2b79f5u;
        // Per-layer level drift, seeded like the detune wanderers.  Zero amount
        // keeps every layer at unity gain.
        for (int i = 0; i < maxUnisonVoices; ++i)
        {
            levelDriftState[static_cast<size_t> (i)]
                = noiseState ^ (static_cast<std::uint32_t> (i + 1) * 0x85ebca6bu);
            if (levelDriftState[static_cast<size_t> (i)] == 0)
                levelDriftState[static_cast<size_t> (i)] = 0x6d2b79f5u;
            levelDriftValue[static_cast<size_t> (i)] = 0.0f;
        }
        previousLfoPhase = lfoPhase;
        lfoSample = 0.0f;
        // A stolen voice carries the previous note's pan gains.  Mark every
        // cached entry stale so the first sample of the new note recomputes
        // them instead of reusing the old note's image.
        panCacheValid.fill (false);
        unisonPanCacheValid.fill (false);
        ampEnvelopeState = EnvelopeCurveState {};
        updateEnvelopeParameters();
        env.noteOn();
        filterEnvelopeState.releasing = false;
        filterEnvelopeState.previousRaw = 0.0f;
        filterEnvelopeState.releaseReference = 0.0f;
        filterEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        // Remember where the release starts so the curve falls away from the
        // level that was actually held, not from a fixed reference.
        ampEnvelopeState.releasing = true;
        ampEnvelopeState.releaseReference = ampEnvelopeState.previousRaw;
        env.noteOff();
        filterEnvelopeState.releasing = true;
        filterEnvelopeState.releaseReference = filterEnvelopeState.previousRaw;
        filterEnv.noteOff();
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
        svfIc3.fill (0.0f);
        svfIc4.fill (0.0f);
        filterInputPrev.fill (0.0f);
        panCacheValid.fill (false);
        unisonPanCacheValid.fill (false);
        ph1.fill (0.0f);
        ph2.fill (0.0f);
        ph3.fill (0.0f);
        ph4.fill (0.0f);
        lfoPhase = 0.0f;
        previousLfoPhase = 0.0f;
        lfoSample = 0.0f;
        driftLevel = 0.0f;
        driftValue.fill (0.0f);
        levelDriftValue.fill (0.0f);
        noiseState = 0x6d2b79f5u;
        noiseStateRight = 0x6d2b79f5u;
        varianceAmount = 0.0f;
        cutoffVariance = 1.0f;
        envelopeVariance = 1.0f;
        levelVariance = 1.0f;
        pitchVarianceRatio = 1.0f;
        ampEnvelopeState = EnvelopeCurveState {};
        filterEnvelopeState = EnvelopeCurveState {};
    }

    void setSR (double newSampleRate)
    {
        sr = newSampleRate;
        env.setSampleRate (newSampleRate);
        filterEnv.setSampleRate (newSampleRate);
        cutoffSmooth.reset (newSampleRate, 0.005);
        resonanceSmooth.reset (newSampleRate, 0.005);
        gainSmooth.reset (newSampleRate, 0.005);
        driveSmooth.reset (newSampleRate, 0.005);
        filterDriveSmooth.reset (newSampleRate, 0.005);
        noiseSmooth.reset (newSampleRate, 0.005);
        unisonDetuneSmooth.reset (newSampleRate, 0.005);
        unisonSpreadSmooth.reset (newSampleRate, 0.005);
        amDepthSmooth.reset (newSampleRate, 0.005);
        cutoffSmooth.setCurrentAndTargetValue (12000.0f);
        resonanceSmooth.setCurrentAndTargetValue (0.15f);
        gainSmooth.setCurrentAndTargetValue (0.7f);
        driveSmooth.setCurrentAndTargetValue (0.0f);
        filterDriveSmooth.setCurrentAndTargetValue (0.0f);
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
        auto* filterDriveParam = p.apvts.getRawParameterValue (ParamIDs::filterDrive);
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
        auto* lfoShape = p.apvts.getRawParameterValue (ParamIDs::lfoShape);
        auto* unisonDriftParam = p.apvts.getRawParameterValue (ParamIDs::unisonDrift);
        auto* keyTracking = p.apvts.getRawParameterValue (ParamIDs::keyTracking);
        auto* envCurveParam = p.apvts.getRawParameterValue (ParamIDs::envCurve);
        auto* filterEnvAmountParam = p.apvts.getRawParameterValue (ParamIDs::filterEnvAmount);
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
        filterDriveSmooth.setTargetValue (filterDriveParam != nullptr ? filterDriveParam->load() : 0.0f);
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
        const int lfoShapeIndex = lfoShape != nullptr
            ? juce::jlimit (0, 2, juce::roundToInt (lfoShape->load())) : 0;
        driftLevel = unisonDriftParam != nullptr
            ? juce::jlimit (0.0f, 1.0f, unisonDriftParam->load()) : 0.0f;
        envCurveAmount = envCurveParam != nullptr
            ? juce::jlimit (0.0f, 1.0f, envCurveParam->load()) : 0.0f;
        filterEnvAmount = filterEnvAmountParam != nullptr
            ? juce::jlimit (-1.0f, 1.0f, filterEnvAmountParam->load()) : 0.0f;
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
            // Triangle keeps the sine's free-running phase; sample & hold
            // draws one bounded random value per LFO cycle so the wander
            // stays musical instead of noisy.
            float lfo = 0.0f;
            if (lfoShapeIndex == 1)
            {
                lfo = 2.0f * std::abs (2.0f * lfoPhase - 1.0f) - 1.0f;
            }
            else if (lfoShapeIndex == 2)
            {
                if (lfoPhase < previousLfoPhase)
                    lfoSample = -0.8f + 1.6f * unitRandomFromState (sahState);
                lfo = lfoSample;
            }
            else
            {
                lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase);
            }
            previousLfoPhase = lfoPhase;
            lfoPhase = std::fmod (lfoPhase + lfoStep, 1.0f);
            const float rawEnvelope = env.getNextSample();
            const float envelope = shapeEnvelope (rawEnvelope, ampEnvelopeState, sustainLevel);
            ampEnvelopeState.previousRaw = rawEnvelope;
            const float rawFilterEnvelope = filterEnv.getNextSample();
            const float filterEnvelope = shapeEnvelope (rawFilterEnvelope, filterEnvelopeState,
                                                        filterSustainLevel);
            filterEnvelopeState.previousRaw = rawFilterEnvelope;
            float matrixPitch = 0.0f, matrixCutoff = 0.0f, matrixAmp = 0.0f,
                  matrixFm = 0.0f, matrixPwm = 0.0f, osc1FmAmount = 0.0f;
            for (int slot = 0; slot < 4; ++slot)
            {
                if (modSourceParams[static_cast<size_t> (slot)] == nullptr
                    || modDestinationParams[static_cast<size_t> (slot)] == nullptr
                    || modAmountParams[static_cast<size_t> (slot)] == nullptr)
                    continue;
                const int source = juce::jlimit (0, 5, juce::roundToInt (modSourceParams[static_cast<size_t> (slot)]->load()));
                const int destination = juce::jlimit (0, 5, juce::roundToInt (modDestinationParams[static_cast<size_t> (slot)]->load()));
                const float amount = juce::jlimit (-1.0f, 1.0f, modAmountParams[static_cast<size_t> (slot)]->load());
                // Oscillator 1 is an audio-rate source and is handled in the
                // oscillator loop below to provide bounded phase modulation.
                if (source == 4)
                {
                    if (destination == 4)
                        osc1FmAmount += 0.5f * amount;
                    continue;
                }
                const float sourceValue = modulationSourceValue (source, lfo, envelope, vel,
                                                                 filterEnvelope);
                switch (destination)
                {
                    case 1: matrixPitch += 12.0f * amount * sourceValue; break;
                    case 2: matrixCutoff += amount * sourceValue; break;
                    case 3: matrixAmp += amount * sourceValue; break;
                    case 4: matrixFm += 0.5f * amount * sourceValue; break;
                    case 5: matrixPwm += amount * sourceValue; break;
                    default: break;
                }
            }
            matrixPitch = juce::jlimit (-24.0f, 24.0f, matrixPitch);
            matrixCutoff = juce::jlimit (-0.95f, 4.0f, matrixCutoff);
            matrixAmp = juce::jlimit (-1.0f, 1.0f, matrixAmp);
            matrixFm = juce::jlimit (-0.5f, 0.5f, matrixFm);
            // Pulse width is clamped again per oscillator below, so this only
            // needs to keep the modulation itself inside a sane band.
            matrixPwm = juce::jlimit (-0.45f, 0.45f, matrixPwm);
            osc1FmAmount = juce::jlimit (-0.5f, 0.5f, osc1FmAmount);
            const float modulatedFrequency = baseFrequency * std::exp2f (matrixPitch / 12.0f)
                * std::exp2f (lfo * lfoPitch->load() / 12.0f)
                * pitchVarianceRatio;
            // Limit each oscillator below Nyquist.  Apart from avoiding
            // invalid PolyBLEP increments, this gives a predictable mute-ish
            // behaviour instead of phase folding on the very top notes.
            const float nyquist = 0.49f * static_cast<float> (sr);
            std::array<float, 4> oscillatorFrequency {}, oscillatorLevel {}, oscillatorPan {}, oscillatorPulseWidth {};
            // Resolved once per sample rather than inside the oscillator: the
            // Analog wave's soft-clip drive depends only on the pulse width,
            // and a per-sample tanh here dominated the CPU probe.
            std::array<float, 4> analogDrive {};
            for (size_t oscillator = 0; oscillator < 4; ++oscillator)
            {
                const float semitones = juce::jlimit (-24.0f, 24.0f, coarseParams[oscillator]->load())
                                      + juce::jlimit (-100.0f, 100.0f, fineSmooth[oscillator].getNextValue()) / 100.0f;
                oscillatorFrequency[oscillator] = juce::jmax (0.01f,
                    modulatedFrequency * std::exp2f (semitones / 12.0f));
                oscillatorLevel[oscillator] = juce::jlimit (0.0f, 1.0f, levelSmooth[oscillator].getNextValue());
                oscillatorPan[oscillator] = juce::jlimit (-1.0f, 1.0f, panSmooth[oscillator].getNextValue());
                // The PWM matrix destination rides on top of the per-oscillator
                // pulse width and is re-clamped to the legal duty range.
                oscillatorPulseWidth[oscillator] = juce::jlimit (0.05f, 0.95f,
                    pulseWidthSmooth[oscillator].getNextValue() + matrixPwm);
                analogDrive[oscillator] = 2.0f + 4.8f
                    * (0.5f - std::abs (oscillatorPulseWidth[oscillator] - 0.5f));
            }
            const float baseCutoff = cutoffSmooth.getNextValue();
            const float keyTrack = juce::jlimit (0.0f, 1.0f,
                keyTracking != nullptr ? keyTracking->load() : 0.0f);
            // The key-tracking ratio depends only on the held note, so it is
            // resolved once in startNote rather than exp2'd every sample.
            const float keyRatio = keyRatioForNote;
            const float modulatedCutoff = juce::jlimit (20.0f, 20000.0f,
                baseCutoff * cutoffVariance * keyRatio
                * (1.0f + lfo * lfoDepth->load() + matrixCutoff)
                * filterEnvGain (filterEnvelope));
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
                // Deterministic random walk toward +/-1 around the static
                // detune. One-pole smoothing keeps the motion in the "analog
                // slow drift" band instead of flutter.
                const float drift = driftLevel
                    * driftValue[static_cast<size_t> (unison)];
                const float cents = activeUnisonVoices > 1
                    ? (position + drift * (0.5f + 0.5f * std::abs (position)))
                        * unisonDetuneCents
                        / static_cast<float> (activeUnisonVoices - 1)
                    : drift * driftDetuneFallbackCents;
                const float ratio = std::exp2f (juce::jlimit (-48.0f, 48.0f, cents) / 1200.0f);
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
                // A muted oscillator still advances its phase further down, so
                // its waveform evaluation can be skipped outright.  With the
                // default patch three of the four sit at level 0, which makes
                // this the largest single saving in the loop.  Oscillator 1 is
                // the exception: it doubles as the audio-rate FM source for
                // oscillator 2, so a mute there does not silence it.
                const bool osc1Audible = oscillatorLevel[0] > 1.0e-6f
                                       || std::abs (osc1FmAmount) > 1.0e-6f;
                const float osc1 = osc1Audible
                    ? oscillatorSample (osc1Phase, increment[0], wave1,
                                        oscillatorPulseWidth[0], analogDrive[0])
                    : 0.0f;
                const float osc2Phase = phase2 + matrixFm + osc1 * osc1FmAmount;
                const std::array<float, 4> samples {
                    osc1,
                    oscillatorLevel[1] > 1.0e-6f
                        ? oscillatorSample (osc2Phase, increment[1], wave2,
                                            oscillatorPulseWidth[1], analogDrive[1])
                        : 0.0f,
                    oscillatorLevel[2] > 1.0e-6f
                        ? oscillatorSample (phase3, increment[2], wave3,
                                            oscillatorPulseWidth[2], analogDrive[2])
                        : 0.0f,
                    oscillatorLevel[3] > 1.0e-6f
                        ? oscillatorSample (phase4, increment[3], wave4,
                                            oscillatorPulseWidth[3], analogDrive[3])
                        : 0.0f
                };
                float oscillatorLeft = 0.0f, oscillatorRight = 0.0f;
                for (size_t oscillator = 0; oscillator < 4; ++oscillator)
                {
                    // A muted oscillator contributed only a zero sample, so the
                    // mix can skip its pan lookup and Nyquist fade entirely.
                    if (oscillatorLevel[oscillator] <= 1.0e-6f
                        && !(oscillator == 0 && std::abs (osc1FmAmount) > 1.0e-6f))
                        continue;
                    // Reuse the cached equal-power gains unless this
                    // oscillator's pan actually moved since the last sample.
                    if (! panCacheValid[oscillator]
                        || cachedPanValue[oscillator] != oscillatorPan[oscillator])
                    {
                        const float angle = (oscillatorPan[oscillator] + 1.0f)
                                          * juce::MathConstants<float>::pi * 0.25f;
                        panGainLeftCache[oscillator] = std::cos (angle);
                        panGainRightCache[oscillator] = std::sin (angle);
                        cachedPanValue[oscillator] = oscillatorPan[oscillator];
                        panCacheValid[oscillator] = true;
                    }
                    const float left = panGainLeftCache[oscillator];
                    const float right = panGainRightCache[oscillator];
                    // Smoothly fade oscillators approaching Nyquist instead of
                    // pinning all higher pitches to one aliased frequency.
                    const float frequencyRatio = oscillatorFrequency[oscillator] * ratio / nyquist;
                    const float nyquistGain = 1.0f - juce::jlimit (0.0f, 1.0f,
                        (frequencyRatio - 0.8f) / 0.2f);
                    oscillatorLeft += samples[oscillator] * oscillatorLevel[oscillator] * left * nyquistGain;
                    oscillatorRight += samples[oscillator] * oscillatorLevel[oscillator] * right * nyquistGain;
                }
                // Independent right-channel noise: the two sides no longer
                // carry the identical sample, which is what gave the noise
                // layer its unnaturally focused phantom centre.  The two
                // generators run even when the layer is silent: skipping them
                // would advance one and not the other, and a preset that
                // automates the mix back up would start from a different point
                // in the sequence.
                const float noise = noiseMix * whiteNoiseSample (noiseState);
                const float noiseRight = noiseMix * whiteNoiseSample (noiseStateRight);
                // A fixed conservative bus gain preserves each Level knob's
                // meaning.  Dividing by the sum of active levels made one
                // oscillator sound equally loud at every non-zero setting.
                constexpr float oscillatorBusGain = 0.35f;
                oscillatorLeft = (oscillatorLeft + noise * 0.70710678f) * oscillatorBusGain;
                oscillatorRight = (oscillatorRight + noiseRight * 0.70710678f) * oscillatorBusGain;
                // Equal-power-ish gains with a unity centre preserve the old
                // mono result at spread=0 while keeping wide layers bounded.
                const float positionNorm = activeUnisonVoices > 1
                    ? (2.0f * position / static_cast<float> (activeUnisonVoices - 1)) : 0.0f;
                const float pan = juce::jlimit (-1.0f, 1.0f,
                    positionNorm * unisonSpreadAmount);
                const auto layer = static_cast<size_t> (unison);
                if (! unisonPanCacheValid[layer] || cachedUnisonPanValue[layer] != pan)
                {
                    const float panAngle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                    unisonPanLeftCache[layer] = std::cos (panAngle);
                    unisonPanRightCache[layer] = std::sin (panAngle);
                    cachedUnisonPanValue[layer] = pan;
                    unisonPanCacheValid[layer] = true;
                }
                const float panGainLeft = unisonPanLeftCache[layer];
                const float panGainRight = unisonPanRightCache[layer];
                // Per-layer level drift, the amplitude counterpart of the
                // existing detune wanderer.  At drift 0 the multiplier is
                // exactly 1.0, so the unison mix stays bit-identical.
                const float layerLevel = 1.0f + driftLevel * 0.35f
                    * levelDriftValue[static_cast<size_t> (unison)];
                unisonLeft += oscillatorLeft * panGainLeft * layerLevel;
                unisonRight += oscillatorRight * panGainRight * layerLevel;
                // Every increment is clamped below 0.5 before it gets here, so
                // the phase can cross the wrap point at most once and a
                // compare replaces the fmod.  At 4 oscillators and 8 unison
                // layers this is 32 fmod calls per sample, the widest
                // transcendental in the voice.
                phase1 += increment[0]; if (phase1 >= 1.0f) phase1 -= 1.0f;
                phase2 += increment[1]; if (phase2 >= 1.0f) phase2 -= 1.0f;
                phase3 += increment[2]; if (phase3 >= 1.0f) phase3 -= 1.0f;
                phase4 += increment[3]; if (phase4 >= 1.0f) phase4 -= 1.0f;
                driftValue[static_cast<size_t> (unison)] += driftLevel
                    * 0.0003f * (unitRandomFromState (driftState[static_cast<size_t> (unison)]) - 0.5f);
                driftValue[static_cast<size_t> (unison)] = juce::jlimit (-1.0f, 1.0f,
                    driftValue[static_cast<size_t> (unison)]);
                levelDriftValue[static_cast<size_t> (unison)] = juce::jlimit (-1.0f, 1.0f,
                    levelDriftValue[static_cast<size_t> (unison)]
                    + driftLevel * 0.0004f
                        * (unitRandomFromState (levelDriftState[static_cast<size_t> (unison)]) - 0.5f));
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
            const float voiceGain = envelope * velocityGain * outputGain * amGain * levelVariance
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
            const float resonanceAmount = juce::jlimit (0.0f, 1.0f,
                resonanceSmooth.getNextValue());
            const float damping = juce::jlimit (0.08f, 2.0f,
                2.0f - 1.92f * resonanceAmount);
            // Dedicated per-voice filter drive: a bounded tanh colouration
            // evaluated inside the selective 2x path below.  Zero keeps the
            // legacy filter input bit-identical.
            const float filterDriveAmount = juce::jlimit (0.0f, 1.0f,
                filterDriveSmooth.getNextValue());
            const bool filterDriveActive = filterDriveAmount > 1.0e-5f;
            // The TPT state-variable filter remains at the host rate for the
            // broad, low-cost region.  Near the top octave, at high Q, or when
            // the filter drive is active, two half-rate updates reduce
            // coefficient warping and sample the drive non-linearity at
            // twice the host rate, halving the fold depth of its products.
            const bool filterOversample = safeCutoff > 0.28f * static_cast<float> (sr)
                                       || resonanceAmount > 0.72f
                                       || filterDriveActive;
            const float filterRate = filterOversample
                ? 2.0f * static_cast<float> (sr) : static_cast<float> (sr);
            const float g = std::tan (juce::MathConstants<float>::pi * safeCutoff / filterRate);
            const float denominator = 1.0f + g * (g + damping);
            // LPF24 cascades a second flat TPT stage (damping 2.0) behind the
            // resonant stage: 24 dB/oct rolloff with the resonance still owned
            // by stage one.  Each stage stays the proven unconditionally
            // stable TPT integrator, so no new stability envelope is needed.
            const float cascadeDenominator = 1.0f + g * (g + 2.0f);
            const int selectedFilter = filterMode != nullptr
                ? juce::jlimit (0, 3, juce::roundToInt (filterMode->load())) : 0;
            const int stageOneMode = selectedFilter == 3 ? 0 : selectedFilter;
            const float filterDriveGain = 1.0f + 5.0f * filterDriveAmount;
            const float filterDriveNorm = std::tanh (filterDriveGain);
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
                // With filter drive active the two sub-steps evaluate the
                // curve on linear-interpolated inputs (midpoint, endpoint) so
                // the non-linearity is sampled at twice the host rate.
                // Without drive both steps share the input, preserving the
                // established half-step filter behaviour bit-for-bit.
                float firstStepInput = x;
                float secondStepInput = x;
                if (filterDriveActive)
                {
                    firstStepInput = std::tanh (0.5f * (x + filterInputPrev[index])
                                                * filterDriveGain) / filterDriveNorm;
                    secondStepInput = std::tanh (x * filterDriveGain) / filterDriveNorm;
                }
                filterInputPrev[index] = x;
                float filtered = processTptSvf (firstStepInput, g, damping, denominator,
                                                stageOneMode,
                                                svfIc1[index], svfIc2[index]);
                if (selectedFilter == 3)
                    filtered = processTptSvf (filtered, g, 2.0f, cascadeDenominator, 0,
                                              svfIc3[index], svfIc4[index]);
                if (filterOversample)
                {
                    filtered = processTptSvf (secondStepInput, g, damping, denominator,
                                              stageOneMode,
                                              svfIc1[index], svfIc2[index]);
                    if (selectedFilter == 3)
                        filtered = processTptSvf (filtered, g, 2.0f, cascadeDenominator, 0,
                                                  svfIc3[index], svfIc4[index]);
                }
                state[index] = filtered;
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
    // Curve-shaping state for one envelope (roadmap P1.2/P1.3).  Declared
    // before the helpers that take it as a parameter.
    struct EnvelopeCurveState
    {
        float previousRaw = 0.0f;
        float releaseReference = 0.0f;
        bool releasing = false;
    };

    void updateEnvelopeParameters()
    {
        juce::ADSR::Parameters parameters;
        parameters.attack = p.apvts.getRawParameterValue (ParamIDs::attack)->load();
        parameters.decay = p.apvts.getRawParameterValue (ParamIDs::decay)->load();
        parameters.sustain = p.apvts.getRawParameterValue (ParamIDs::sustain)->load();
        parameters.release = p.apvts.getRawParameterValue (ParamIDs::release)->load();
        // Roadmap P2: the seeded envelope-time offset scales every stage of
        // this voice only.  At variance 0 the multiplier is exactly 1.0 and
        // the four timings reach juce::ADSR unchanged.
        if (envelopeVariance != 1.0f)
        {
            parameters.attack *= envelopeVariance;
            parameters.decay *= envelopeVariance;
            parameters.release *= envelopeVariance;
        }
        env.setParameters (parameters);
        sustainLevel = juce::jlimit (0.0f, 1.0f, parameters.sustain);

        const auto readOr = [this] (const char* id, float fallback) noexcept
        {
            if (const auto* parameter = p.apvts.getRawParameterValue (id))
                return parameter->load();
            return fallback;
        };
        juce::ADSR::Parameters filterParameters;
        filterParameters.attack = readOr (ParamIDs::filterAttack, 0.01f);
        filterParameters.decay = readOr (ParamIDs::filterDecay, 0.3f);
        filterParameters.sustain = readOr (ParamIDs::filterSustain, 0.8f);
        filterParameters.release = readOr (ParamIDs::filterRelease, 0.4f);
        if (envelopeVariance != 1.0f)
        {
            filterParameters.attack *= envelopeVariance;
            filterParameters.decay *= envelopeVariance;
            filterParameters.release *= envelopeVariance;
        }
        filterEnv.setParameters (filterParameters);
        filterSustainLevel = juce::jlimit (0.0f, 1.0f, filterParameters.sustain);
    }

    // Roadmap P1.2: exponential (RC-style) amp envelope.  The juce::ADSR state
    // machine and every stage timing stay exactly as they were; only the value
    // presented to the voice is curved.  amount = 0 returns the raw value
    // untouched, so legacy presets keep their linear release bit-for-bit.
    // Attack eases into the peak, decay approaches the sustain level from
    // above, and release falls away from the level held when the note ended.
    float shapeEnvelope (float raw, EnvelopeCurveState& state, float sustain) noexcept
    {
        if (envCurveAmount <= 1.0e-4f)
            return raw;

        const float gamma = 1.0f + 3.0f * envCurveAmount;
        const float value = juce::jlimit (0.0f, 1.0f, raw);

        if (value > state.previousRaw)
            return 1.0f - std::pow (1.0f - value, gamma);

        if (value < state.previousRaw)
        {
            if (state.releasing)
            {
                if (state.releaseReference > 1.0e-6f)
                    return state.releaseReference
                         * std::pow (value / state.releaseReference, gamma);
                return value;
            }

            const float span = 1.0f - sustain;
            if (span > 1.0e-6f && value > sustain)
                return sustain + span * std::pow ((value - sustain) / span, gamma);
        }

        return value;
    }

    // Roadmap P1.3: dedicated filter envelope depth.  An amount of zero
    // returns exactly 1.0 so the cutoff path stays bit-identical, and the full
    // swing covers +/-5 octaves, which is the usual pluck-to-sweep range.
    float filterEnvGain (float envelope) const noexcept
    {
        if (std::abs (filterEnvAmount) <= 1.0e-5f)
            return 1.0f;
        return std::exp2f (filterEnvAmount * envelope * 5.0f);
    }

    EonMiniEEFProcessor& p;
    juce::ADSR env;
    double sr = 44100.0;
    int note = 60;
    static constexpr int maxUnisonVoices = 8;
    float vel = 0.0f, lfoPhase = 0.0f;
    float previousLfoPhase = 0.0f, lfoSample = 0.0f, driftLevel = 0.0f;
    static constexpr float driftDetuneFallbackCents = 12.0f;
    std::array<float, maxUnisonVoices> driftValue {};
    std::array<std::uint32_t, maxUnisonVoices> driftState {};
    // Roadmap P2: per-layer level wander and the decorrelated right-channel
    // noise seed, both seeded from the same deterministic note chain.
    std::array<float, maxUnisonVoices> levelDriftValue {};
    std::array<std::uint32_t, maxUnisonVoices> levelDriftState {};
    std::uint32_t sahState = 0x6d2b79f5u;
    static float unitRandomFromState (std::uint32_t& stateValue) noexcept
    {
        stateValue ^= stateValue << 13;
        stateValue ^= stateValue >> 17;
        stateValue ^= stateValue << 5;
        return static_cast<float> (stateValue >> 8) / 16777216.0f;
    }
    std::array<float, 2> state {}, svfIc1 {}, svfIc2 {};
    // Stage two of the LPF24 cascade and the previous raw filter input used
    // by the drive midpoint evaluation.  Both reset with the legacy states.
    std::array<float, 2> svfIc3 {}, svfIc4 {};
    std::array<float, 2> filterInputPrev {};
    std::uint32_t noiseState = 0x6d2b79f5u;
    std::uint32_t noiseStateRight = 0x6d2b79f5u;
    // Roadmap P2 per-voice tolerance.  The three multipliers stay at exactly
    // 1.0 while the variance amount is zero, so the legacy path is unchanged.
    float varianceAmount = 0.0f;
    float cutoffVariance = 1.0f, envelopeVariance = 1.0f, levelVariance = 1.0f;
    float pitchVarianceRatio = 1.0f;
    // Key tracking is a function of the held note only, so the exponential is
    // evaluated once per note instead of once per sample.
    float keyRatioForNote = 1.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> resonanceSmooth;
    juce::SmoothedValue<float> gainSmooth, driveSmooth, noiseSmooth,
                               unisonDetuneSmooth, unisonSpreadSmooth, amDepthSmooth;
    juce::SmoothedValue<float> filterDriveSmooth;
    // Envelope curve state shared by the amp and filter envelopes
    // (roadmap P1.2/P1.3).
    EnvelopeCurveState ampEnvelopeState {}, filterEnvelopeState {};
    float envCurveAmount = 0.0f, sustainLevel = 0.8f, filterSustainLevel = 0.8f;
    juce::ADSR filterEnv;
    float filterEnvAmount = 0.0f;
    std::array<juce::SmoothedValue<float>, 4> levelSmooth, fineSmooth, panSmooth, pulseWidthSmooth;
    // Cached equal-power pan gains.  panSmooth ramps over 5 ms, so after the
    // ramp settles the value stops changing and the sin/cos pair can be
    // reused.  The oscillator pan gains sit inside the unison loop, which
    // made them the most repeated transcendental in the whole voice: 12 per
    // sample at 4 oscillators and 8 unison layers, 1536 per sample at the
    // 16-voice worst case.  Each entry recomputes only when its own input
    // value actually moved.
    std::array<float, 4> panGainLeftCache {}, panGainRightCache {};
    std::array<float, 4> cachedPanValue {};
    std::array<bool, 4> panCacheValid {};
    std::array<float, maxUnisonVoices> unisonPanLeftCache {}, unisonPanRightCache {};
    std::array<float, maxUnisonVoices> cachedUnisonPanValue {};
    std::array<bool, maxUnisonVoices> unisonPanCacheValid {};
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
            id, name, juce::StringArray { "Saw", "Square", "Triangle", "Sine", "Analog" }, 4));
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
    addFloat (ParamIDs::unisonDrift, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::voiceVariance, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::cutoff, 20.0f, 20000.0f, 12000.0f);
    addFloat (ParamIDs::resonance, 0.0f, 1.0f, 0.15f);
    addFloat (ParamIDs::filterDrive, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::filterMode, "Filter Mode",
        juce::StringArray { "LPF", "HPF", "BPF", "LPF24" }, 0));
    addFloat (ParamIDs::attack, 0.001f, 5.0f, 0.01f);
    addFloat (ParamIDs::decay, 0.001f, 5.0f, 0.3f);
    addFloat (ParamIDs::sustain, 0.0f, 1.0f, 0.8f);
    addFloat (ParamIDs::release, 0.001f, 8.0f, 0.4f);
    addFloat (ParamIDs::envCurve, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::filterAttack, 0.001f, 5.0f, 0.01f);
    addFloat (ParamIDs::filterDecay, 0.001f, 5.0f, 0.3f);
    addFloat (ParamIDs::filterSustain, 0.0f, 1.0f, 0.8f);
    addFloat (ParamIDs::filterRelease, 0.001f, 8.0f, 0.4f);
    addFloat (ParamIDs::filterEnvAmount, -1.0f, 1.0f, 0.0f);
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
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::lfoShape, "LFO Shape",
        juce::StringArray { "Sine", "Triangle", "Sample & Hold" }, 0));
    addFloat (ParamIDs::keyTracking, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::voiceMode, "Voice Mode", juce::StringArray { "Poly", "Mono" }, 0));
    addFloat (ParamIDs::fxWet, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::delayTime, 0.01f, 2.0f, 0.35f);
    addFloat (ParamIDs::delayFeedback, 0.0f, 0.9f, 0.25f);
    addFloat (ParamIDs::delayStereo, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::chorusDepth, 0.0f, 0.02f, 0.004f);
    addFloat (ParamIDs::chorusRate, 0.05f, 8.0f, 0.25f);
    addFloat (ParamIDs::chorusMix, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::reverbMix, 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::reverbModulation, 0.0f, 1.0f, 0.0f);
    parameters.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParamIDs::oversampling, "Oversampling",
        juce::StringArray { "1x Eco", "2x Quality", "4x High" }, 0));

    const juce::StringArray modSources {
        "Off", "LFO", "Amp Env", "Velocity", "Osc 1", "Filter Env"
    };
    const juce::StringArray modDestinations {
        "Off", "Pitch", "Cutoff", "Amp", "Osc 2 FM", "PWM"
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

        // Roadmap P3.3: the amp stage and the safety limiter run here, above
        // the host rate, whenever a quality mode is selected.  The host-rate
        // loop in processBlock skips both so the same signal is shaped exactly
        // once.  1x mode never reaches this block, keeping the legacy order.
        const auto* ampSatParameter = apvts.getRawParameterValue (ParamIDs::ampSat);
        const float ampSatAmount = ampSatParameter != nullptr
            ? juce::jlimit (0.0f, 1.0f, ampSatParameter->load()) : 0.0f;
        for (size_t channel = 0; channel < highRateBlock.getNumChannels(); ++channel)
        {
            auto* samples = highRateBlock.getChannelPointer (channel);
            for (size_t sample = 0; sample < highRateBlock.getNumSamples(); ++sample)
            {
                const float saturated = applyAmpSaturation (samples[sample], ampSatAmount);
                // Same bounded knee as the 1x path, evaluated per high-rate
                // sample so the fold products stay inside the band the
                // oversampler's stopband filter can still remove.
                const float magnitude = std::abs (saturated);
                const float limited = magnitude <= 1.0f
                    ? saturated
                    : std::copysign (1.0f + std::tanh (magnitude - 1.0f), saturated);
                samples[sample] = std::isfinite (limited) ? limited : 0.0f;
            }
        }

        oversampler.processSamplesDown (block);
    }

    // Report one fixed latency to the host and delay the 1x/2x paths to match
    // the 4x path.  This avoids timing jumps and comb filtering when quality
    // changes without calling host-notification APIs on the audio thread.
    const int compensation = juce::jlimit (0, latencyBufferCapacity - 1,
        fixedLatencySamples - activeOversamplingLatency);
    const int latencyChannels = juce::jmin (2, buffer.getNumChannels());
    // The write always happens: a host can switch to 2x or 4x mid-stream, and
    // the compensation read needs the samples that were written while 1x was
    // active.  Only the read is skipped when there is nothing to compensate.
    const bool needsRead = compensation != 0;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        int readPosition = latencyWritePosition - compensation;
        if (readPosition < 0)
            readPosition += latencyBufferCapacity;
        for (int channel = 0; channel < latencyChannels; ++channel)
        {
            auto& line = latencyBuffer[static_cast<size_t> (channel)];
            const float input = buffer.getSample (channel, sample);
            line[static_cast<size_t> (latencyWritePosition)] = input;
            if (needsRead)
                buffer.setSample (channel, sample, line[static_cast<size_t> (readPosition)]);
        }
        latencyWritePosition = latencyWritePosition + 1 == latencyBufferCapacity
            ? 0 : latencyWritePosition + 1;
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

void EonMiniEEFProcessor::configureReverbDelays() noexcept
{
    constexpr std::array<int, reverbCombCount> combBase { 149, 211, 263, 293 };
    constexpr std::array<int, reverbAllpassCount> allpassBase { 31, 47 };
    const float scale = static_cast<float> (sampleRate / 48000.0);
    for (int index = 0; index < reverbCombCount; ++index)
        reverbCombLengths[static_cast<size_t> (index)] = juce::jlimit (1, reverbMaxDelaySamples,
            juce::roundToInt (combBase[static_cast<size_t> (index)] * scale));
    for (int index = 0; index < reverbAllpassCount; ++index)
        reverbAllpassLengths[static_cast<size_t> (index)] = juce::jlimit (1, reverbMaxDelaySamples,
            juce::roundToInt (allpassBase[static_cast<size_t> (index)] * scale));
}

void EonMiniEEFProcessor::resetReverbState() noexcept
{
    for (auto& line : reverbCombL)
        line.fill (0.0f);
    for (auto& line : reverbCombR)
        line.fill (0.0f);
    for (auto& line : reverbAllpassL)
        line.fill (0.0f);
    for (auto& line : reverbAllpassR)
        line.fill (0.0f);
    reverbCombPositions.fill (0);
    reverbAllpassPositionsL.fill (0);
    reverbAllpassPositionsR.fill (0);
    reverbCombDampL.fill (0.0f);
    reverbCombDampR.fill (0.0f);
    reverbModulation = 0.0f;
    reverbModulationPhase = 0.0;
}

void EonMiniEEFProcessor::prepareToPlay(double sr,int samplesPerBlock){sampleRate=sr;synth->setCurrentPlaybackSampleRate(sr);for(int i=0;i<synth->getNumVoices();++i)dynamic_cast<EonVoice*>(synth->getVoice(i))->setSR(sr);fxDelay.setSize(2,juce::jmax(1,(int)(sr*2.0)),false,true,true);fxDelay.clear();fxWritePosition=0;delayDampL=0.0f;delayDampR=0.0f;configureReverbDelays();resetReverbState();chorusPhase=0;chorusBufferL.fill(0.0f);chorusBufferR.fill(0.0f);chorusWritePosition=0;chorusLfoPhase=0.0;const auto initialiseChorusSmoother=[this,sr](juce::SmoothedValue<float>& smoother,const char* id,float fallback){smoother.reset(sr,0.015);const auto* parameter=apvts.getRawParameterValue(id);smoother.setCurrentAndTargetValue(parameter!=nullptr?parameter->load():fallback);};initialiseChorusSmoother(chorusDepthSmooth,ParamIDs::chorusDepth,0.004f);initialiseChorusSmoother(chorusRateSmooth,ParamIDs::chorusRate,0.25f);initialiseChorusSmoother(chorusMixSmooth,ParamIDs::chorusMix,0.0f);initialiseChorusSmoother(reverbModulationSmooth,ParamIDs::reverbModulation,0.0f);reverbModulationIncrement=0.31/(double)juce::jmax(1.0,sr);dcInput.fill(0.0f);dcOutput.fill(0.0f);resetDriveCurveState();oversamplingBlockSize=juce::jmax(1,samplesPerBlock);oversampling2x.initProcessing(static_cast<size_t>(oversamplingBlockSize));oversampling4x.initProcessing(static_cast<size_t>(oversamplingBlockSize));oversampling2x.reset();oversampling4x.reset();fixedLatencySamples=juce::jlimit(1,latencyBufferCapacity-1,juce::roundToInt(juce::jmax(oversampling2x.getLatencyInSamples(),oversampling4x.getLatencyInSamples())));latencyWritePosition=0;for(auto& channel:latencyBuffer)channel.fill(0.0f);setLatencySamples(fixedLatencySamples);}
bool EonMiniEEFProcessor::isBusesLayoutSupported(const BusesLayout&l)const{const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();return in.isDisabled()&&(out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo());}
void EonMiniEEFProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    juce::ScopedNoDenormals nd;
    if (dspResetRequested.exchange (false, std::memory_order_acq_rel))
    {
        static_cast<NoteOnlySynthesiser*> (synth.get())->resetVoicesNoLock();
        fxDelay.clear();
        fxWritePosition = 0;
        delayDampL = 0.0f;
        delayDampR = 0.0f;
        resetReverbState();
        chorusPhase = 0.0f;
        chorusBufferL.fill (0.0f);
        chorusBufferR.fill (0.0f);
        chorusWritePosition = 0;
        chorusLfoPhase = 0.0;
        const auto resetChorusSmoother = [this] (juce::SmoothedValue<float>& smoother,
                                                  const char* id, float fallback) noexcept
        {
            const auto* parameter = apvts.getRawParameterValue (id);
            smoother.setCurrentAndTargetValue (parameter != nullptr ? parameter->load() : fallback);
        };
        resetChorusSmoother (chorusDepthSmooth, ParamIDs::chorusDepth, 0.004f);
        resetChorusSmoother (chorusRateSmooth, ParamIDs::chorusRate, 0.25f);
        resetChorusSmoother (chorusMixSmooth, ParamIDs::chorusMix, 0.0f);
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
    const float wet=value(ParamIDs::fxWet,0.0f), delaySeconds=value(ParamIDs::delayTime,0.35f), feedback=value(ParamIDs::delayFeedback,0.25f), depthTarget=value(ParamIDs::chorusDepth,0.004f), rateTarget=value(ParamIDs::chorusRate,0.25f), chorusTarget=value(ParamIDs::chorusMix,0.0f), reverb=value(ParamIDs::reverbMix,0.0f);
    chorusDepthSmooth.setTargetValue (depthTarget);
    chorusRateSmooth.setTargetValue (rateTarget);
    chorusMixSmooth.setTargetValue (chorusTarget);
    reverbModulationSmooth.setTargetValue (value (ParamIDs::reverbModulation, 0.0f));
    delayStereoWidth = juce::jlimit (0.0f, 1.0f,
        value (ParamIDs::delayStereo, 0.0f));
    const float ampSatAmount=value(ParamIDs::ampSat,0.0f);
    const int delayLength=juce::jlimit(1,fxDelay.getNumSamples()-1,(int)(delaySeconds*(float)sampleRate));
    float blockPeakL = 0.0f, blockPeakR = 0.0f;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float dryL = b.getSample (0, i);
        const float dryR = b.getNumChannels() > 1 ? b.getSample (1, i) : dryL;
        const int delayReadPosition = (fxWritePosition - delayLength + fxDelay.getNumSamples())
            % fxDelay.getNumSamples();
        const float delayedL = fxDelay.getSample (0, delayReadPosition);
        const float delayedR = fxDelay.getSample (1, delayReadPosition);

        // Roadmap P3.2: stereo width as a true ping-pong.  Cross-feeding alone
        // cannot open the image, because a mono input feeds both channels the
        // same sample and any equal-weight mix of them stays correlated.  The
        // width control therefore makes the two taps read different delay
        // lengths, so consecutive repeats land on alternating sides.  At 0 the
        // offset is exactly zero and the established single-path send is
        // untouched.
        float wetDelayL = delayedL, wetDelayR = delayedR;
        float writeL = dryL, writeR = dryR;
        if (delayStereoWidth > 1.0e-5f)
        {
            // The spread is a fixed time offset rather than a fraction of the
            // delay time: scaling with delay length made the two taps land on
            // an exact number of periods of a low note, which re-correlates
            // the image instead of opening it.  30 ms is wide enough to read
            // as a stereo spread and short enough to stay a single echo.
            constexpr float maxStereoSpreadSeconds = 0.030f;
            const int offsetSamples = juce::jmax (1,
                juce::roundToInt (delayStereoWidth * maxStereoSpreadSeconds
                                  * (float) sampleRate));
            const int rightReadPosition = (delayReadPosition - offsetSamples
                                          + fxDelay.getNumSamples()) % fxDelay.getNumSamples();
            const float delayedROffset = fxDelay.getSample (1, rightReadPosition);
            // The right channel reads the offset tap on its own, so even a
            // mono source (which writes the same sample into both lines) is
            // delayed by two different amounts and the two sides carry
            // different parts of the waveform.  Blending both taps into both
            // channels instead would keep them perfectly correlated.
            wetDelayL = delayedL * (1.0f - juce::jlimit (0.0f, 1.0f, delayStereoWidth))
                      + delayedROffset * juce::jlimit (0.0f, 1.0f, delayStereoWidth);
            wetDelayR = delayedROffset;
        }

        // A mild one-pole loss in the feedback loop keeps repeated echoes
        // from retaining unlimited top-end energy.  The normalized tanh
        // stage is deliberately small and only acts on the feedback copy.
        constexpr float delayDamping = 0.45f;
        delayDampL += delayDamping * (delayedL - delayDampL);
        delayDampR += delayDamping * (delayedR - delayDampR);
        const float feedbackDrive = 1.0f + 0.35f * feedback;
        const float feedbackL = std::tanh (delayDampL * feedbackDrive) / feedbackDrive;
        const float feedbackR = std::tanh (delayDampR * feedbackDrive) / feedbackDrive;
        fxDelay.setSample (0, fxWritePosition, writeL + feedbackL * feedback);
        fxDelay.setSample (1, fxWritePosition, writeR + feedbackR * feedback);

        // Four short damped combs provide the decay field; two allpass stages
        // diffuse the summed field before it reaches the wet mix.  Delay
        // lengths are scaled in prepareToPlay so the texture is stable across
        // sample rates without allocating in this loop.
        constexpr float combFeedback = 0.78f;
        constexpr float combDamping = 0.25f;
        // Roadmap P3.1: a sub-audio oscillator drifts each comb line's
        // damping, so the four static delays no longer ring at one fixed
        // period.  The rate is deliberately slow (0.07-0.5 Hz, incommensurate
        // across the four lines) to smear the tail rather than add an audible
        // tremolo.  At modulation 0 the coefficient is untouched and the
        // comb network stays bit-identical to the established tail.
        const float reverbModulationAmount = reverbModulationSmooth.getNextValue();
        const float reverbLfo = std::sin (juce::MathConstants<float>::twoPi
                                          * (float) reverbModulationPhase);
        const float reverbInputL = 0.78f * dryL + 0.22f * dryR;
        const float reverbInputR = 0.78f * dryR + 0.22f * dryL;
        float combSumL = 0.0f, combSumR = 0.0f;
        for (int line = 0; line < reverbCombCount; ++line)
        {
            const auto index = static_cast<size_t> (line);
            const int position = reverbCombPositions[index];
            const float combOutL = reverbCombL[index][static_cast<size_t> (position)];
            const float combOutR = reverbCombR[index][static_cast<size_t> (position)];
            // Each line runs its own phase, a fixed irrational multiple apart,
            // so the four modulated dampings never realign into a periodic
            // pattern the ear can lock onto.
            const float linePhase = reverbLfo * (1.0f + 0.37f * (float) line);
            const float lineDamping = reverbModulationAmount > 1.0e-5f
                ? juce::jlimit (0.05f, 0.60f, combDamping * (1.0f + 1.2f
                                                            * reverbModulationAmount * linePhase))
                : combDamping;
            reverbCombDampL[index] += lineDamping * (combOutL - reverbCombDampL[index]);
            reverbCombDampR[index] += lineDamping * (combOutR - reverbCombDampR[index]);
            reverbCombL[index][static_cast<size_t> (position)]
                = reverbInputL + reverbCombDampL[index] * combFeedback;
            reverbCombR[index][static_cast<size_t> (position)]
                = reverbInputR + reverbCombDampR[index] * combFeedback;
            // See processReverbAllpass: a compare beats a modulo here, and
            // this runs four more times per sample.
            reverbCombPositions[index] = position + 1 == reverbCombLengths[index]
                ? 0 : position + 1;
            combSumL += combOutL * 0.25f;
            combSumR += combOutR * 0.25f;
        }
        reverbModulationPhase += reverbModulationIncrement;
        if (reverbModulationPhase >= 1.0)
            reverbModulationPhase -= 1.0;
        float rvL = processReverbAllpass (combSumL, reverbAllpassL[0],
            reverbAllpassPositionsL[0], reverbAllpassLengths[0]);
        rvL = processReverbAllpass (rvL, reverbAllpassL[1],
            reverbAllpassPositionsL[1], reverbAllpassLengths[1]);
        float rvR = processReverbAllpass (combSumR, reverbAllpassR[0],
            reverbAllpassPositionsR[0], reverbAllpassLengths[0]);
        rvR = processReverbAllpass (rvR, reverbAllpassR[1],
            reverbAllpassPositionsR[1], reverbAllpassLengths[1]);
        rvL *= 0.28f;
        rvR *= 0.28f;
        // Quad-tap Dimension-D-style chorus on a dedicated stereo buffer. Dry
        // stays unity; the wet quad mix rides on top with chorusMix as gain.
        // Four taps share one slow LFO at 0/90/180/270 degrees around a fixed
        // centre delay, panned with rotating weights like four BBD outputs
        // feeding the original's output network.
        float chorusWetL = 0.0f, chorusWetR = 0.0f;
        const float chorus = chorusMixSmooth.getNextValue();
        {
            constexpr float chorusCentreSeconds = 0.025f;
            const float depth = chorusDepthSmooth.getNextValue();
            const float rate = chorusRateSmooth.getNextValue();
            constexpr float tapWeightsL[4] { 1.0f, 0.8f, 0.6f, 0.4f };
            constexpr float tapWeightsR[4] { 0.4f, 0.6f, 0.8f, 1.0f };
            for (int tap = 0; tap < 4; ++tap)
            {
                const float lfoPhase = chorusLfoPhase
                    + (float) tap * juce::MathConstants<float>::halfPi;
                const float delaySamples = juce::jlimit (4.0f,
                    (float) (chorusBufferLength - 2),
                    (chorusCentreSeconds + depth * std::sin (lfoPhase)) * (float) sampleRate);
                const float readPosition = (float) chorusWritePosition - delaySamples;
                const int index1 = (int) std::floor (readPosition);
                const float fractional = readPosition - (float) index1;
                const int wrapped0 = ((index1 - 1) % chorusBufferLength + chorusBufferLength) % chorusBufferLength;
                const int wrapped1 = ((index1 % chorusBufferLength) + chorusBufferLength) % chorusBufferLength;
                const int wrapped2 = (wrapped1 + 1) % chorusBufferLength;
                const int wrapped3 = (wrapped1 + 2) % chorusBufferLength;
                chorusWetL += tapWeightsL[tap] * cubicInterpolate (chorusBufferL[(size_t) wrapped0],
                    chorusBufferL[(size_t) wrapped1], chorusBufferL[(size_t) wrapped2],
                    chorusBufferL[(size_t) wrapped3], fractional);
                chorusWetR += tapWeightsR[tap] * cubicInterpolate (chorusBufferR[(size_t) wrapped0],
                    chorusBufferR[(size_t) wrapped1], chorusBufferR[(size_t) wrapped2],
                    chorusBufferR[(size_t) wrapped3], fractional);
            }
            chorusWetL *= 1.0f / 2.8f;
            chorusWetR *= 1.0f / 2.8f;
            chorusBufferL[(size_t) chorusWritePosition] = dryL;
            chorusBufferR[(size_t) chorusWritePosition] = dryR;
            chorusWritePosition = (chorusWritePosition + 1) % chorusBufferLength;
            chorusLfoPhase = std::fmod (chorusLfoPhase
                + juce::MathConstants<double>::twoPi * (double) rate / sampleRate,
                juce::MathConstants<double>::twoPi);
        }
        float fxL=dryL+wet*wetDelayL+chorus*chorusWetL+reverb*(rvL-dryL),fxR=dryR+wet*wetDelayR+chorus*chorusWetR+reverb*(rvR-dryR);
        // Final safety stage: remove subsonic DC while retaining state across blocks,
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
            // Roadmap P3.3: when a quality mode owns the oversampled stage, the
            // amp saturation and the soft limiter move inside it so their fold
            // products land above the host rate instead of being decimated
            // straight back to it.  The 1x path keeps the established order
            // (saturation, then limiter) unchanged.
            float output = dcBlocked;
            if (oversamplingBypassed())
            {
                const float saturated = applyAmpSaturation (dcBlocked, ampSatAmount);
                // Preserve exact small-signal dynamics.  Only excursions above
                // the safety ceiling enter a smooth, bounded limiting knee.
                const float magnitude = std::abs (saturated);
                output = magnitude <= 1.0f
                    ? saturated
                    : std::copysign (1.0f + std::tanh (magnitude - 1.0f), saturated);
            }
            output = std::isfinite(output) ? output : 0.0f;
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
