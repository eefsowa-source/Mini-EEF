#include "FactoryPresets.h"
#include "Identifiers/ParamIDs.h"
#include "PluginProcessor.h"

namespace
{
void setValue (EonMiniEEFProcessor& processor, const char* id, float rawValue)
{
    if (auto* parameter = processor.apvts.getParameter (id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (rawValue));
        parameter->endChangeGesture();
    }
}

void reset (EonMiniEEFProcessor& processor)
{
    for (auto* baseParameter : processor.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (baseParameter))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
}
}

namespace FactoryPresets
{
const juce::StringArray& names()
{
    static const juce::StringArray presetNames {
        "Init", "Supersaw Pad", "Trance Pluck", "Arena Lead",
        "Sub Mono Bass", "Acid Bass", "Glass Keys", "Warm Poly",
        "Velvet Strings", "Neon Bell", "Pulse Sequence", "Digital Pluck",
        "Wide Brass", "Soft Organ", "Juno Choir", "Motion Pad",
        "FMish Bass", "Rubber Mono", "Resonant Sweep", "Noise SFX",
        "Lo-Fi Keys", "Dream Lead", "Octave Stab", "Deep Drone",
        "Percussive Click", "Classic PWM"
        , "Minimoog Lead", "Moog Bass", "Diva Saw Pad", "Juno Pad"
        , "Prophet Brass", "Minifreak Pluck", "Sync Sweep Lead"
        , "Analog Strings"
    };
    return presetNames;
}

void apply (EonMiniEEFProcessor& processor, int presetIndex)
{
    const int index = juce::jlimit (0, names().size() - 1, presetIndex);
    reset (processor);
    const auto set = [&processor] (const char* id, float rawValue)
    {
        setValue (processor, id, rawValue);
    };

    switch (index)
    {
        case 0:
            break;

        case 1:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc4Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.52f);
            set (ParamIDs::osc4Level, 0.18f);
            set (ParamIDs::noiseMix, 0.025f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 8.0f);
            set (ParamIDs::unisonVoices, 7.0f);
            set (ParamIDs::unisonDetune, 16.0f);
            set (ParamIDs::unisonSpread, 0.86f);
            set (ParamIDs::unisonPhase, 0.74f);
            set (ParamIDs::cutoff, 4700.0f);
            set (ParamIDs::resonance, 0.18f);
            set (ParamIDs::attack, 0.65f);
            set (ParamIDs::decay, 1.4f);
            set (ParamIDs::sustain, 0.82f);
            set (ParamIDs::release, 2.6f);
            set (ParamIDs::gain, 0.64f);
            set (ParamIDs::drive, 0.035f);
            set (ParamIDs::lfoRate, 0.14f);
            set (ParamIDs::lfoDepth, 0.13f);
            set (ParamIDs::velocityAmount, 0.45f);
            set (ParamIDs::keyTracking, 0.38f);
            set (ParamIDs::fxWet, 0.54f);
            set (ParamIDs::delayTime, 0.42f);
            set (ParamIDs::delayFeedback, 0.32f);
            set (ParamIDs::chorusDepth, 0.008f);
            set (ParamIDs::chorusRate, 0.19f);
            set (ParamIDs::chorusMix, 0.32f);
            set (ParamIDs::reverbMix, 0.42f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.13f);
            set (ParamIDs::modSource (1), 3.0f);
            set (ParamIDs::modDestination (1), 3.0f);
            set (ParamIDs::modAmount (1), 0.16f);
            break;

        case 2:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.22f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 5.5f);
            set (ParamIDs::unisonVoices, 5.0f);
            set (ParamIDs::unisonDetune, 10.0f);
            set (ParamIDs::unisonSpread, 0.60f);
            set (ParamIDs::cutoff, 6500.0f);
            set (ParamIDs::resonance, 0.36f);
            set (ParamIDs::attack, 0.001f);
            set (ParamIDs::decay, 0.35f);
            set (ParamIDs::sustain, 0.05f);
            set (ParamIDs::release, 0.26f);
            set (ParamIDs::drive, 0.08f);
            set (ParamIDs::lfoRate, 1.5f);
            set (ParamIDs::lfoDepth, 0.025f);
            set (ParamIDs::velocityAmount, 0.90f);
            set (ParamIDs::keyTracking, 0.42f);
            set (ParamIDs::fxWet, 0.45f);
            set (ParamIDs::delayTime, 0.23f);
            set (ParamIDs::delayFeedback, 0.36f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.34f);
            set (ParamIDs::chorusMix, 0.12f);
            set (ParamIDs::reverbMix, 0.18f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 3.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.35f);
            set (ParamIDs::modSource (1), 3.0f);
            set (ParamIDs::modDestination (1), 3.0f);
            set (ParamIDs::modAmount (1), 0.20f);
            break;

        case 3:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.14f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 12.0f);
            set (ParamIDs::unisonVoices, 6.0f);
            set (ParamIDs::unisonDetune, 12.0f);
            set (ParamIDs::unisonSpread, 0.60f);
            set (ParamIDs::unisonPhase, 0.88f);
            set (ParamIDs::cutoff, 9000.0f);
            set (ParamIDs::resonance, 0.21f);
            set (ParamIDs::attack, 0.008f);
            set (ParamIDs::decay, 0.40f);
            set (ParamIDs::sustain, 0.65f);
            set (ParamIDs::release, 0.35f);
            set (ParamIDs::gain, 0.72f);
            set (ParamIDs::drive, 0.10f);
            set (ParamIDs::lfoRate, 5.2f);
            set (ParamIDs::lfoDepth, 0.05f);
            set (ParamIDs::lfoPitch, 0.18f);
            set (ParamIDs::velocityAmount, 0.62f);
            set (ParamIDs::keyTracking, 0.65f);
            set (ParamIDs::fxWet, 0.30f);
            set (ParamIDs::delayTime, 0.29f);
            set (ParamIDs::delayFeedback, 0.30f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.30f);
            set (ParamIDs::chorusMix, 0.10f);
            set (ParamIDs::reverbMix, 0.10f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 3.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.32f);
            set (ParamIDs::modSource (1), 1.0f);
            set (ParamIDs::modDestination (1), 1.0f);
            set (ParamIDs::modAmount (1), 0.06f);
            break;

        case 4:

            // osc2 carries this patch at its default level; without a pin it
            // would inherit the new Analog default and move.
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc1Wave, 1.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.65f);
            set (ParamIDs::osc2Coarse, -12.0f);
            set (ParamIDs::cutoff, 420.0f);
            set (ParamIDs::resonance, 0.23f);
            set (ParamIDs::attack, 0.002f);
            set (ParamIDs::decay, 0.16f);
            set (ParamIDs::sustain, 0.72f);
            set (ParamIDs::release, 0.10f);
            set (ParamIDs::gain, 0.78f);
            set (ParamIDs::drive, 0.22f);
            set (ParamIDs::lfoRate, 0.15f);
            set (ParamIDs::lfoDepth, 0.06f);
            set (ParamIDs::velocityAmount, 0.78f);
            set (ParamIDs::keyTracking, 0.25f);
            set (ParamIDs::voiceMode, 1.0f);
            set (ParamIDs::fxWet, 0.08f);
            set (ParamIDs::delayTime, 0.12f);
            set (ParamIDs::delayFeedback, 0.12f);
            set (ParamIDs::chorusDepth, 0.002f);
            set (ParamIDs::chorusRate, 0.21f);
            set (ParamIDs::chorusMix, 0.05f);
            set (ParamIDs::reverbMix, 0.02f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 3.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.13f);
            break;

        case 5:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.16f);
            set (ParamIDs::noiseMix, 0.012f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::cutoff, 700.0f);
            set (ParamIDs::resonance, 0.70f);
            set (ParamIDs::attack, 0.001f);
            set (ParamIDs::decay, 0.25f);
            set (ParamIDs::sustain, 0.18f);
            set (ParamIDs::release, 0.08f);
            set (ParamIDs::gain, 0.73f);
            set (ParamIDs::drive, 0.36f);
            set (ParamIDs::lfoRate, 0.25f);
            set (ParamIDs::lfoDepth, 0.05f);
            set (ParamIDs::velocityAmount, 0.85f);
            set (ParamIDs::keyTracking, 0.30f);
            set (ParamIDs::voiceMode, 1.0f);
            set (ParamIDs::fxWet, 0.10f);
            set (ParamIDs::delayTime, 0.17f);
            set (ParamIDs::delayFeedback, 0.18f);
            set (ParamIDs::chorusDepth, 0.002f);
            set (ParamIDs::chorusRate, 0.18f);
            set (ParamIDs::chorusMix, 0.04f);
            set (ParamIDs::reverbMix, 0.02f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 3.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.32f);
            set (ParamIDs::modSource (1), 1.0f);
            set (ParamIDs::modDestination (1), 2.0f);
            set (ParamIDs::modAmount (1), 0.22f);
            break;

        case 6:
            set (ParamIDs::osc1Wave, 3.0f);
            set (ParamIDs::osc1Level, 0.44f);
            set (ParamIDs::osc2Wave, 2.0f);
            set (ParamIDs::osc2Level, 0.28f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.08f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 3.0f);
            set (ParamIDs::unisonSpread, 0.25f);
            set (ParamIDs::cutoff, 7200.0f);
            set (ParamIDs::resonance, 0.12f);
            set (ParamIDs::attack, 0.008f);
            set (ParamIDs::decay, 0.80f);
            set (ParamIDs::sustain, 0.35f);
            set (ParamIDs::release, 1.80f);
            set (ParamIDs::gain, 0.66f);
            set (ParamIDs::drive, 0.02f);
            set (ParamIDs::lfoRate, 0.08f);
            set (ParamIDs::lfoDepth, 0.03f);
            set (ParamIDs::velocityAmount, 0.70f);
            set (ParamIDs::keyTracking, 0.55f);
            set (ParamIDs::fxWet, 0.62f);
            set (ParamIDs::delayTime, 0.36f);
            set (ParamIDs::delayFeedback, 0.22f);
            set (ParamIDs::chorusDepth, 0.006f);
            set (ParamIDs::chorusRate, 0.32f);
            set (ParamIDs::chorusMix, 0.24f);
            set (ParamIDs::reverbMix, 0.50f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.08f);
            break;

        case 7:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Fine, -7.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.22f);
            set (ParamIDs::osc3Coarse, -12.0f);
            set (ParamIDs::unisonVoices, 3.0f);
            set (ParamIDs::unisonDetune, 5.0f);
            set (ParamIDs::unisonSpread, 0.45f);
            set (ParamIDs::cutoff, 2200.0f);
            set (ParamIDs::resonance, 0.22f);
            set (ParamIDs::attack, 0.18f);
            set (ParamIDs::decay, 0.90f);
            set (ParamIDs::sustain, 0.70f);
            set (ParamIDs::release, 1.60f);
            set (ParamIDs::gain, 0.65f);
            set (ParamIDs::drive, 0.04f);
            set (ParamIDs::ampSat, 0.08f);
            set (ParamIDs::velocityAmount, 0.60f);
            set (ParamIDs::keyTracking, 0.42f);
            set (ParamIDs::fxWet, 0.44f);
            set (ParamIDs::delayTime, 0.31f);
            set (ParamIDs::delayFeedback, 0.20f);
            set (ParamIDs::chorusDepth, 0.005f);
            set (ParamIDs::chorusRate, 0.22f);
            set (ParamIDs::chorusMix, 0.22f);
            set (ParamIDs::reverbMix, 0.36f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 8:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc3Level, 0.18f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::unisonVoices, 5.0f);
            set (ParamIDs::unisonDetune, 6.0f);
            set (ParamIDs::unisonSpread, 0.70f);
            set (ParamIDs::unisonDrift, 0.30f);
            set (ParamIDs::cutoff, 3400.0f);
            set (ParamIDs::resonance, 0.10f);
            set (ParamIDs::attack, 0.80f);
            set (ParamIDs::decay, 1.30f);
            set (ParamIDs::sustain, 0.75f);
            set (ParamIDs::release, 3.20f);
            set (ParamIDs::gain, 0.58f);
            set (ParamIDs::drive, 0.015f);
            set (ParamIDs::lfoRate, 0.18f);
            set (ParamIDs::lfoDepth, 0.08f);
            set (ParamIDs::fxWet, 0.65f);
            set (ParamIDs::delayTime, 0.46f);
            set (ParamIDs::delayFeedback, 0.28f);
            set (ParamIDs::chorusDepth, 0.010f);
            set (ParamIDs::chorusRate, 0.16f);
            set (ParamIDs::chorusMix, 0.42f);
            set (ParamIDs::reverbMix, 0.55f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 9:
            set (ParamIDs::osc1Wave, 3.0f);
            set (ParamIDs::osc2Wave, 3.0f);
            set (ParamIDs::osc2Level, 0.32f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::osc2Fine, 3.0f);
            set (ParamIDs::osc3Wave, 2.0f);
            set (ParamIDs::osc3Level, 0.14f);
            set (ParamIDs::osc3Coarse, 19.0f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 2.0f);
            set (ParamIDs::cutoff, 10000.0f);
            set (ParamIDs::resonance, 0.28f);
            set (ParamIDs::attack, 0.001f);
            set (ParamIDs::decay, 0.70f);
            set (ParamIDs::sustain, 0.08f);
            set (ParamIDs::release, 1.80f);
            set (ParamIDs::gain, 0.62f);
            set (ParamIDs::drive, 0.02f);
            set (ParamIDs::lfoRate, 3.8f);
            set (ParamIDs::lfoDepth, 0.03f);
            set (ParamIDs::velocityAmount, 0.70f);
            set (ParamIDs::fxWet, 0.55f);
            set (ParamIDs::delayTime, 0.52f);
            set (ParamIDs::delayFeedback, 0.28f);
            set (ParamIDs::chorusDepth, 0.002f);
            set (ParamIDs::chorusRate, 0.70f);
            set (ParamIDs::chorusMix, 0.18f);
            set (ParamIDs::reverbMix, 0.50f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 10:

            // osc2/3 carries this patch at its default level; without a pin it
            // would inherit the new Analog default and move.
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc1Wave, 1.0f);
            set (ParamIDs::osc2Coarse, 7.0f);
            set (ParamIDs::osc3Level, 0.12f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 1.0f);
            set (ParamIDs::unisonSpread, 0.22f);
            set (ParamIDs::cutoff, 1800.0f);
            set (ParamIDs::resonance, 0.52f);
            set (ParamIDs::attack, 0.002f);
            set (ParamIDs::decay, 0.18f);
            set (ParamIDs::sustain, 0.28f);
            set (ParamIDs::release, 0.12f);
            set (ParamIDs::gain, 0.65f);
            set (ParamIDs::drive, 0.12f);
            set (ParamIDs::ampSat, 0.10f);
            set (ParamIDs::lfoRate, 6.0f);
            set (ParamIDs::lfoDepth, 0.12f);
            set (ParamIDs::lfoSync, 1.0f);
            set (ParamIDs::lfoDivision, 3.0f);
            set (ParamIDs::lfoShape, 1.0f);
            set (ParamIDs::velocityAmount, 0.55f);
            set (ParamIDs::keyTracking, 0.35f);
            set (ParamIDs::voiceMode, 1.0f);
            set (ParamIDs::fxWet, 0.24f);
            set (ParamIDs::delayTime, 0.16f);
            set (ParamIDs::delayFeedback, 0.20f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.28f);
            set (ParamIDs::chorusMix, 0.15f);
            set (ParamIDs::reverbMix, 0.06f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.25f);
            break;

        case 11:
            set (ParamIDs::osc1Wave, 1.0f);
            set (ParamIDs::osc2Wave, 2.0f);
            set (ParamIDs::osc2Level, 0.36f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.12f);
            set (ParamIDs::osc3Coarse, 19.0f);
            set (ParamIDs::unisonVoices, 3.0f);
            set (ParamIDs::unisonDetune, 4.0f);
            set (ParamIDs::unisonSpread, 0.36f);
            set (ParamIDs::cutoff, 7800.0f);
            set (ParamIDs::resonance, 0.38f);
            set (ParamIDs::attack, 0.001f);
            set (ParamIDs::decay, 0.22f);
            set (ParamIDs::sustain, 0.10f);
            set (ParamIDs::release, 0.35f);
            set (ParamIDs::gain, 0.64f);
            set (ParamIDs::drive, 0.09f);
            set (ParamIDs::driveCurve, 1.0f);
            set (ParamIDs::ampSat, 0.05f);
            set (ParamIDs::lfoRate, 0.90f);
            set (ParamIDs::lfoDepth, 0.04f);
            set (ParamIDs::lfoPitch, 0.08f);
            set (ParamIDs::velocityAmount, 0.90f);
            set (ParamIDs::keyTracking, 0.50f);
            set (ParamIDs::fxWet, 0.40f);
            set (ParamIDs::delayTime, 0.27f);
            set (ParamIDs::delayFeedback, 0.34f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.34f);
            set (ParamIDs::chorusMix, 0.12f);
            set (ParamIDs::reverbMix, 0.20f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 3.0f);
            set (ParamIDs::modDestination (0), 1.0f);
            set (ParamIDs::modAmount (0), 0.22f);
            break;

        case 12:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Fine, -8.0f);
            set (ParamIDs::osc3Level, 0.22f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::unisonVoices, 4.0f);
            set (ParamIDs::unisonDetune, 8.0f);
            set (ParamIDs::unisonSpread, 0.80f);
            set (ParamIDs::unisonPhase, 0.70f);
            set (ParamIDs::cutoff, 2800.0f);
            set (ParamIDs::resonance, 0.25f);
            set (ParamIDs::attack, 0.12f);
            set (ParamIDs::decay, 0.65f);
            set (ParamIDs::sustain, 0.76f);
            set (ParamIDs::release, 1.00f);
            set (ParamIDs::gain, 0.62f);
            set (ParamIDs::drive, 0.08f);
            set (ParamIDs::driveCurve, 2.0f);
            set (ParamIDs::ampSat, 0.12f);
            set (ParamIDs::lfoRate, 0.22f);
            set (ParamIDs::lfoDepth, 0.07f);
            set (ParamIDs::keyTracking, 0.45f);
            set (ParamIDs::fxWet, 0.50f);
            set (ParamIDs::delayTime, 0.32f);
            set (ParamIDs::delayFeedback, 0.24f);
            set (ParamIDs::chorusDepth, 0.007f);
            set (ParamIDs::chorusRate, 0.22f);
            set (ParamIDs::chorusMix, 0.35f);
            set (ParamIDs::reverbMix, 0.30f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.10f);
            break;

        case 13:
            set (ParamIDs::osc1Wave, 3.0f);
            set (ParamIDs::osc2Wave, 3.0f);
            set (ParamIDs::osc2Level, 0.36f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.18f);
            set (ParamIDs::osc3Coarse, 19.0f);
            set (ParamIDs::cutoff, 9000.0f);
            set (ParamIDs::resonance, 0.08f);
            set (ParamIDs::attack, 0.01f);
            set (ParamIDs::decay, 0.50f);
            set (ParamIDs::sustain, 0.92f);
            set (ParamIDs::release, 0.50f);
            set (ParamIDs::gain, 0.57f);
            set (ParamIDs::drive, 0.01f);
            set (ParamIDs::velocityAmount, 0.45f);
            set (ParamIDs::fxWet, 0.28f);
            set (ParamIDs::delayTime, 0.18f);
            set (ParamIDs::delayFeedback, 0.14f);
            set (ParamIDs::chorusDepth, 0.006f);
            set (ParamIDs::chorusRate, 0.26f);
            set (ParamIDs::chorusMix, 0.20f);
            set (ParamIDs::reverbMix, 0.12f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 14:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Fine, -7.0f);
            set (ParamIDs::osc3Level, 0.18f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::osc4Wave, 2.0f);
            set (ParamIDs::osc4Level, 0.08f);
            set (ParamIDs::osc4Coarse, 19.0f);
            set (ParamIDs::unisonVoices, 6.0f);
            set (ParamIDs::unisonDetune, 9.0f);
            set (ParamIDs::unisonSpread, 0.90f);
            set (ParamIDs::unisonPhase, 0.50f);
            set (ParamIDs::unisonDrift, 0.18f);
            set (ParamIDs::cutoff, 3600.0f);
            set (ParamIDs::resonance, 0.20f);
            set (ParamIDs::attack, 0.35f);
            set (ParamIDs::decay, 1.10f);
            set (ParamIDs::sustain, 0.72f);
            set (ParamIDs::release, 2.40f);
            set (ParamIDs::gain, 0.55f);
            set (ParamIDs::drive, 0.03f);
            set (ParamIDs::lfoRate, 0.35f);
            set (ParamIDs::lfoDepth, 0.10f);
            set (ParamIDs::amDepth, 0.05f);
            set (ParamIDs::fxWet, 0.70f);
            set (ParamIDs::delayTime, 0.47f);
            set (ParamIDs::delayFeedback, 0.30f);
            set (ParamIDs::chorusDepth, 0.009f);
            set (ParamIDs::chorusRate, 0.24f);
            set (ParamIDs::chorusMix, 0.50f);
            set (ParamIDs::reverbMix, 0.48f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.12f);
            set (ParamIDs::modSource (1), 2.0f);
            set (ParamIDs::modDestination (1), 1.0f);
            set (ParamIDs::modAmount (1), 0.08f);
            break;

        case 15:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Wave, 2.0f);
            set (ParamIDs::osc2Level, 0.34f);
            set (ParamIDs::osc2Coarse, 7.0f);
            set (ParamIDs::osc3Level, 0.20f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::unisonVoices, 7.0f);
            set (ParamIDs::unisonDetune, 12.0f);
            set (ParamIDs::unisonSpread, 0.92f);
            set (ParamIDs::unisonDrift, 0.40f);
            set (ParamIDs::cutoff, 1900.0f);
            set (ParamIDs::resonance, 0.14f);
            set (ParamIDs::attack, 1.10f);
            set (ParamIDs::decay, 1.80f);
            set (ParamIDs::sustain, 0.80f);
            set (ParamIDs::release, 4.50f);
            set (ParamIDs::gain, 0.50f);
            set (ParamIDs::drive, 0.02f);
            set (ParamIDs::lfoRate, 0.07f);
            set (ParamIDs::lfoDepth, 0.20f);
            set (ParamIDs::amDepth, 0.12f);
            set (ParamIDs::lfoSync, 1.0f);
            set (ParamIDs::lfoDivision, 0.0f);
            set (ParamIDs::velocityAmount, 0.45f);
            set (ParamIDs::keyTracking, 0.35f);
            set (ParamIDs::fxWet, 0.85f);
            set (ParamIDs::delayTime, 0.62f);
            set (ParamIDs::delayFeedback, 0.42f);
            set (ParamIDs::chorusDepth, 0.012f);
            set (ParamIDs::chorusRate, 0.11f);
            set (ParamIDs::chorusMix, 0.55f);
            set (ParamIDs::reverbMix, 0.60f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.18f);
            break;

        case 16:
            set (ParamIDs::osc1Wave, 3.0f);
            set (ParamIDs::osc1Level, 0.50f);
            set (ParamIDs::osc2Wave, 3.0f);
            set (ParamIDs::osc2Level, 0.35f);
            set (ParamIDs::osc3Wave, 2.0f);
            set (ParamIDs::osc3Level, 0.10f);
            set (ParamIDs::cutoff, 1000.0f);
            set (ParamIDs::resonance, 0.30f);
            set (ParamIDs::attack, 0.003f);
            set (ParamIDs::decay, 0.35f);
            set (ParamIDs::sustain, 0.45f);
            set (ParamIDs::release, 0.15f);
            set (ParamIDs::gain, 0.70f);
            set (ParamIDs::drive, 0.16f);
            set (ParamIDs::driveCurve, 2.0f);
            set (ParamIDs::ampSat, 0.08f);
            set (ParamIDs::velocityAmount, 0.75f);
            set (ParamIDs::keyTracking, 0.42f);
            set (ParamIDs::voiceMode, 1.0f);
            set (ParamIDs::fxWet, 0.18f);
            set (ParamIDs::delayTime, 0.10f);
            set (ParamIDs::delayFeedback, 0.10f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.20f);
            set (ParamIDs::chorusMix, 0.10f);
            set (ParamIDs::reverbMix, 0.04f);
            set (ParamIDs::oversampling, 2.0f);
            set (ParamIDs::modSource (0), 4.0f);
            set (ParamIDs::modDestination (0), 4.0f);
            set (ParamIDs::modAmount (0), 0.45f);
            break;

        case 17:

            // osc2 carries this patch at level 0.38; without a pin it would
            // inherit the new Analog default and move.
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc1Wave, 1.0f);
            set (ParamIDs::osc2Level, 0.38f);
            set (ParamIDs::osc2Coarse, 7.0f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.10f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::noiseMix, 0.02f);
            set (ParamIDs::cutoff, 520.0f);
            set (ParamIDs::resonance, 0.62f);
            set (ParamIDs::attack, 0.002f);
            set (ParamIDs::decay, 0.28f);
            set (ParamIDs::sustain, 0.25f);
            set (ParamIDs::release, 0.13f);
            set (ParamIDs::gain, 0.72f);
            set (ParamIDs::drive, 0.30f);
            set (ParamIDs::driveCurve, 1.0f);
            set (ParamIDs::ampSat, 0.16f);
            set (ParamIDs::lfoRate, 0.50f);
            set (ParamIDs::lfoDepth, 0.06f);
            set (ParamIDs::amDepth, 0.08f);
            set (ParamIDs::velocityAmount, 0.80f);
            set (ParamIDs::keyTracking, 0.28f);
            set (ParamIDs::voiceMode, 1.0f);
            set (ParamIDs::fxWet, 0.18f);
            set (ParamIDs::delayTime, 0.13f);
            set (ParamIDs::delayFeedback, 0.13f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.26f);
            set (ParamIDs::chorusMix, 0.08f);
            set (ParamIDs::reverbMix, 0.03f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 3.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.20f);
            break;

        case 18:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Fine, 5.0f);
            set (ParamIDs::osc3Level, 0.10f);
            set (ParamIDs::unisonVoices, 3.0f);
            set (ParamIDs::unisonDetune, 3.0f);
            set (ParamIDs::unisonSpread, 0.45f);
            set (ParamIDs::cutoff, 250.0f);
            set (ParamIDs::resonance, 0.72f);
            set (ParamIDs::attack, 0.03f);
            set (ParamIDs::decay, 1.40f);
            set (ParamIDs::sustain, 0.35f);
            set (ParamIDs::release, 0.90f);
            set (ParamIDs::gain, 0.58f);
            set (ParamIDs::drive, 0.10f);
            set (ParamIDs::ampSat, 0.05f);
            set (ParamIDs::lfoRate, 0.20f);
            set (ParamIDs::lfoDepth, 0.25f);
            set (ParamIDs::keyTracking, 0.50f);
            set (ParamIDs::fxWet, 0.52f);
            set (ParamIDs::delayTime, 0.38f);
            set (ParamIDs::delayFeedback, 0.26f);
            set (ParamIDs::chorusDepth, 0.008f);
            set (ParamIDs::chorusRate, 0.20f);
            set (ParamIDs::chorusMix, 0.25f);
            set (ParamIDs::reverbMix, 0.25f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.35f);
            break;

        case 19:
            set (ParamIDs::osc1Wave, 3.0f);
            set (ParamIDs::osc1Level, 0.18f);
            set (ParamIDs::osc2Wave, 3.0f);
            set (ParamIDs::osc2Level, 0.08f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::noiseMix, 0.32f);
            set (ParamIDs::cutoff, 9000.0f);
            set (ParamIDs::filterMode, 1.0f);
            set (ParamIDs::resonance, 0.08f);
            set (ParamIDs::attack, 0.004f);
            set (ParamIDs::decay, 0.45f);
            set (ParamIDs::sustain, 0.04f);
            set (ParamIDs::release, 0.28f);
            set (ParamIDs::gain, 0.50f);
            set (ParamIDs::drive, 0.04f);
            set (ParamIDs::lfoRate, 7.0f);
            set (ParamIDs::lfoDepth, 0.10f);
            set (ParamIDs::amDepth, 0.45f);
            set (ParamIDs::lfoShape, 2.0f);
            set (ParamIDs::velocityAmount, 0.50f);
            set (ParamIDs::fxWet, 0.65f);
            set (ParamIDs::delayTime, 0.24f);
            set (ParamIDs::delayFeedback, 0.40f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.90f);
            set (ParamIDs::chorusMix, 0.18f);
            set (ParamIDs::reverbMix, 0.42f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 20:
            set (ParamIDs::osc1Wave, 2.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Level, 0.28f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::osc3Wave, 2.0f);
            set (ParamIDs::osc3Level, 0.08f);
            set (ParamIDs::osc3Coarse, 19.0f);
            set (ParamIDs::noiseMix, 0.03f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 4.0f);
            set (ParamIDs::unisonSpread, 0.20f);
            set (ParamIDs::cutoff, 1700.0f);
            set (ParamIDs::resonance, 0.30f);
            set (ParamIDs::attack, 0.006f);
            set (ParamIDs::decay, 0.60f);
            set (ParamIDs::sustain, 0.32f);
            set (ParamIDs::release, 0.80f);
            set (ParamIDs::gain, 0.60f);
            set (ParamIDs::drive, 0.24f);
            set (ParamIDs::driveCurve, 1.0f);
            set (ParamIDs::ampSat, 0.10f);
            set (ParamIDs::lfoRate, 0.14f);
            set (ParamIDs::lfoDepth, 0.05f);
            set (ParamIDs::velocityAmount, 0.85f);
            set (ParamIDs::keyTracking, 0.60f);
            set (ParamIDs::fxWet, 0.38f);
            set (ParamIDs::delayTime, 0.22f);
            set (ParamIDs::delayFeedback, 0.22f);
            set (ParamIDs::chorusDepth, 0.005f);
            set (ParamIDs::chorusRate, 0.40f);
            set (ParamIDs::chorusMix, 0.18f);
            set (ParamIDs::reverbMix, 0.18f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 21:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Fine, -4.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.12f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::unisonVoices, 4.0f);
            set (ParamIDs::unisonDetune, 5.0f);
            set (ParamIDs::unisonSpread, 0.65f);
            set (ParamIDs::cutoff, 5400.0f);
            set (ParamIDs::resonance, 0.18f);
            set (ParamIDs::attack, 0.03f);
            set (ParamIDs::decay, 0.55f);
            set (ParamIDs::sustain, 0.68f);
            set (ParamIDs::release, 0.85f);
            set (ParamIDs::gain, 0.65f);
            set (ParamIDs::drive, 0.07f);
            set (ParamIDs::ampSat, 0.08f);
            set (ParamIDs::lfoRate, 4.20f);
            set (ParamIDs::lfoDepth, 0.04f);
            set (ParamIDs::lfoPitch, 0.12f);
            set (ParamIDs::velocityAmount, 0.70f);
            set (ParamIDs::keyTracking, 0.55f);
            set (ParamIDs::fxWet, 0.62f);
            set (ParamIDs::delayTime, 0.34f);
            set (ParamIDs::delayFeedback, 0.35f);
            set (ParamIDs::chorusDepth, 0.007f);
            set (ParamIDs::chorusRate, 0.27f);
            set (ParamIDs::chorusMix, 0.38f);
            set (ParamIDs::reverbMix, 0.45f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 1.0f);
            set (ParamIDs::modAmount (0), 0.08f);
            break;

        case 22:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc4Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.42f);
            set (ParamIDs::osc2Coarse, 12.0f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.18f);
            set (ParamIDs::osc3Coarse, -12.0f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 2.0f);
            set (ParamIDs::unisonSpread, 0.30f);
            set (ParamIDs::cutoff, 4800.0f);
            set (ParamIDs::resonance, 0.32f);
            set (ParamIDs::attack, 0.001f);
            set (ParamIDs::decay, 0.32f);
            set (ParamIDs::sustain, 0.18f);
            set (ParamIDs::release, 0.22f);
            set (ParamIDs::gain, 0.70f);
            set (ParamIDs::drive, 0.13f);
            set (ParamIDs::ampSat, 0.10f);
            set (ParamIDs::velocityAmount, 0.65f);
            set (ParamIDs::fxWet, 0.28f);
            set (ParamIDs::delayTime, 0.19f);
            set (ParamIDs::delayFeedback, 0.24f);
            set (ParamIDs::chorusDepth, 0.003f);
            set (ParamIDs::chorusRate, 0.21f);
            set (ParamIDs::chorusMix, 0.10f);
            set (ParamIDs::reverbMix, 0.08f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 23:

            // The default wave is Analog; this patch predates it and pins Saw.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Level, 0.40f);
            set (ParamIDs::osc2Coarse, -12.0f);
            set (ParamIDs::osc3Wave, 3.0f);
            set (ParamIDs::osc3Level, 0.20f);
            set (ParamIDs::osc3Coarse, -24.0f);
            set (ParamIDs::osc4Wave, 2.0f);
            set (ParamIDs::osc4Level, 0.12f);
            set (ParamIDs::osc4Coarse, 12.0f);
            set (ParamIDs::unisonVoices, 5.0f);
            set (ParamIDs::unisonDetune, 7.0f);
            set (ParamIDs::unisonSpread, 0.75f);
            set (ParamIDs::unisonDrift, 0.50f);
            set (ParamIDs::cutoff, 820.0f);
            set (ParamIDs::resonance, 0.18f);
            set (ParamIDs::attack, 1.60f);
            set (ParamIDs::decay, 0.90f);
            set (ParamIDs::sustain, 0.90f);
            set (ParamIDs::release, 5.00f);
            set (ParamIDs::gain, 0.45f);
            set (ParamIDs::drive, 0.12f);
            set (ParamIDs::ampSat, 0.10f);
            set (ParamIDs::lfoRate, 0.03f);
            set (ParamIDs::lfoDepth, 0.16f);
            set (ParamIDs::amDepth, 0.10f);
            set (ParamIDs::fxWet, 0.88f);
            set (ParamIDs::delayTime, 0.75f);
            set (ParamIDs::delayFeedback, 0.50f);
            set (ParamIDs::chorusDepth, 0.010f);
            set (ParamIDs::chorusRate, 0.08f);
            set (ParamIDs::chorusMix, 0.45f);
            set (ParamIDs::reverbMix, 0.70f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.22f);
            break;

        case 24:
            set (ParamIDs::osc1Wave, 2.0f);
            set (ParamIDs::osc1Level, 0.28f);
            set (ParamIDs::osc2Wave, 3.0f);
            set (ParamIDs::osc2Level, 0.32f);
            set (ParamIDs::osc2Coarse, 24.0f);
            set (ParamIDs::noiseMix, 0.05f);
            set (ParamIDs::cutoff, 12000.0f);
            set (ParamIDs::resonance, 0.15f);
            set (ParamIDs::attack, 0.001f);
            set (ParamIDs::decay, 0.08f);
            set (ParamIDs::sustain, 0.02f);
            set (ParamIDs::release, 0.08f);
            set (ParamIDs::gain, 0.60f);
            set (ParamIDs::drive, 0.06f);
            set (ParamIDs::velocityAmount, 1.0f);
            set (ParamIDs::fxWet, 0.12f);
            set (ParamIDs::delayTime, 0.08f);
            set (ParamIDs::delayFeedback, 0.05f);
            set (ParamIDs::chorusDepth, 0.002f);
            set (ParamIDs::chorusRate, 0.50f);
            set (ParamIDs::chorusMix, 0.05f);
            set (ParamIDs::reverbMix, 0.04f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 25:
            set (ParamIDs::osc1Wave, 1.0f);
            set (ParamIDs::osc1PulseWidth, 0.42f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Fine, 5.0f);
            set (ParamIDs::osc2PulseWidth, 0.58f);
            set (ParamIDs::osc3Wave, 2.0f);
            set (ParamIDs::osc3Level, 0.10f);
            set (ParamIDs::unisonVoices, 3.0f);
            set (ParamIDs::unisonDetune, 2.0f);
            set (ParamIDs::unisonSpread, 0.50f);
            set (ParamIDs::cutoff, 3200.0f);
            set (ParamIDs::resonance, 0.30f);
            set (ParamIDs::attack, 0.01f);
            set (ParamIDs::decay, 0.38f);
            set (ParamIDs::sustain, 0.65f);
            set (ParamIDs::release, 0.50f);
            set (ParamIDs::gain, 0.62f);
            set (ParamIDs::drive, 0.05f);
            set (ParamIDs::lfoRate, 0.75f);
            set (ParamIDs::lfoDepth, 0.22f);
            set (ParamIDs::lfoShape, 1.0f);
            set (ParamIDs::velocityAmount, 0.60f);
            set (ParamIDs::keyTracking, 0.40f);
            set (ParamIDs::fxWet, 0.40f);
            set (ParamIDs::delayTime, 0.31f);
            set (ParamIDs::delayFeedback, 0.25f);
            set (ParamIDs::chorusDepth, 0.007f);
            set (ParamIDs::chorusRate, 0.22f);
            set (ParamIDs::chorusMix, 0.30f);
            set (ParamIDs::reverbMix, 0.22f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.18f);
            break;

        case 26:
            // Minimoog-style lead: three saws into a resonant ladder-ish
            // lowpass, with the per-voice variance carrying the tuner slop
            // that a bank of Moog oscillators would have.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.52f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.42f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, -7.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc3Level, 0.28f);
            set (ParamIDs::osc3Coarse, 0.0f);
            set (ParamIDs::osc3Fine, 7.0f);
            set (ParamIDs::osc4Wave, 1.0f);
            set (ParamIDs::osc4Level, 0.09f);
            set (ParamIDs::osc4Coarse, -12.0f);
            set (ParamIDs::unisonVoices, 1.0f);
            set (ParamIDs::unisonDetune, 0.0f);
            set (ParamIDs::cutoff, 2400.0f);
            set (ParamIDs::resonance, 0.62f);
            set (ParamIDs::filterDrive, 0.22f);
            set (ParamIDs::attack, 0.010f);
            set (ParamIDs::decay, 0.90f);
            set (ParamIDs::sustain, 0.72f);
            set (ParamIDs::release, 0.45f);
            set (ParamIDs::envCurve, 0.35f);
            set (ParamIDs::gain, 0.26f);
            set (ParamIDs::drive, 0.06f);
            set (ParamIDs::driveCurve, 2.0f);
            set (ParamIDs::ampSat, 0.20f);
            set (ParamIDs::velocityAmount, 0.55f);
            set (ParamIDs::keyTracking, 0.30f);
            set (ParamIDs::voiceVariance, 0.55f);
            set (ParamIDs::fxWet, 0.16f);
            set (ParamIDs::reverbMix, 0.14f);
            set (ParamIDs::oversampling, 2.0f);
            break;

        case 27:
            // Moog-style mono bass: the filter envelope does the talking and
            // a sub square holds the bottom while the saws move.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.58f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.46f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, -5.0f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.42f);
            set (ParamIDs::osc3Coarse, -12.0f);
            set (ParamIDs::osc4Level, 0.0f);
            set (ParamIDs::voiceMode, 1.0f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 5.0f);
            set (ParamIDs::unisonSpread, 0.12f);
            set (ParamIDs::unisonDrift, 0.20f);
            set (ParamIDs::cutoff, 380.0f);
            set (ParamIDs::resonance, 0.55f);
            set (ParamIDs::filterEnvAmount, 0.62f);
            set (ParamIDs::filterAttack, 0.004f);
            set (ParamIDs::filterDecay, 0.34f);
            set (ParamIDs::filterSustain, 0.16f);
            set (ParamIDs::filterRelease, 0.30f);
            set (ParamIDs::attack, 0.004f);
            set (ParamIDs::decay, 0.40f);
            set (ParamIDs::sustain, 0.86f);
            set (ParamIDs::release, 0.22f);
            set (ParamIDs::envCurve, 0.45f);
            set (ParamIDs::gain, 0.58f);
            set (ParamIDs::drive, 0.14f);
            set (ParamIDs::driveCurve, 2.0f);
            set (ParamIDs::ampSat, 0.38f);
            set (ParamIDs::keyTracking, 0.20f);
            set (ParamIDs::voiceVariance, 0.45f);
            set (ParamIDs::fxWet, 0.0f);
            set (ParamIDs::oversampling, 2.0f);
            break;

        case 28:
            // Diva-style saw pad: wide unison, slow drift, and a gentle
            // filter envelope so the chord opens instead of sitting still.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.60f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.52f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 9.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc3Level, 0.30f);
            set (ParamIDs::osc3Coarse, 0.0f);
            set (ParamIDs::osc3Fine, -9.0f);
            set (ParamIDs::osc4Wave, 2.0f);
            set (ParamIDs::osc4Level, 0.14f);
            set (ParamIDs::osc4Coarse, 12.0f);
            set (ParamIDs::osc4Pan, -0.40f);
            set (ParamIDs::unisonVoices, 7.0f);
            set (ParamIDs::unisonDetune, 14.0f);
            set (ParamIDs::unisonSpread, 0.88f);
            set (ParamIDs::unisonPhase, 0.62f);
            set (ParamIDs::unisonDrift, 0.34f);
            set (ParamIDs::cutoff, 1600.0f);
            set (ParamIDs::resonance, 0.20f);
            set (ParamIDs::filterEnvAmount, 0.34f);
            set (ParamIDs::filterAttack, 0.60f);
            set (ParamIDs::filterDecay, 2.20f);
            set (ParamIDs::filterSustain, 0.62f);
            set (ParamIDs::filterRelease, 2.40f);
            set (ParamIDs::attack, 0.90f);
            set (ParamIDs::decay, 1.80f);
            set (ParamIDs::sustain, 0.84f);
            set (ParamIDs::release, 3.40f);
            set (ParamIDs::envCurve, 0.40f);
            set (ParamIDs::gain, 0.58f);
            set (ParamIDs::drive, 0.05f);
            set (ParamIDs::ampSat, 0.22f);
            set (ParamIDs::lfoRate, 0.11f);
            set (ParamIDs::lfoDepth, 0.16f);
            set (ParamIDs::velocityAmount, 0.30f);
            set (ParamIDs::keyTracking, 0.32f);
            set (ParamIDs::voiceVariance, 0.70f);
            set (ParamIDs::fxWet, 0.42f);
            set (ParamIDs::delayTime, 0.44f);
            set (ParamIDs::delayFeedback, 0.30f);
            set (ParamIDs::delayStereo, 0.62f);
            set (ParamIDs::chorusDepth, 0.010f);
            set (ParamIDs::chorusRate, 0.21f);
            set (ParamIDs::chorusMix, 0.38f);
            set (ParamIDs::reverbMix, 0.44f);
            set (ParamIDs::reverbModulation, 0.28f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.14f);
            set (ParamIDs::modSource (1), 5.0f);
            set (ParamIDs::modDestination (1), 2.0f);
            set (ParamIDs::modAmount (1), 0.22f);
            break;

        case 29:
            // Juno-106-style pad: the pulse wave under a slow chorus, a
            // high-passed bloom on the filter envelope, and a long tail.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.55f);
            set (ParamIDs::osc2Wave, 1.0f);
            set (ParamIDs::osc2Level, 0.44f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 12.0f);
            set (ParamIDs::osc2PulseWidth, 0.44f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.26f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::osc3PulseWidth, 0.62f);
            set (ParamIDs::osc4Level, 0.0f);
            set (ParamIDs::unisonVoices, 3.0f);
            set (ParamIDs::unisonDetune, 8.0f);
            set (ParamIDs::unisonSpread, 0.66f);
            set (ParamIDs::unisonPhase, 0.55f);
            set (ParamIDs::unisonDrift, 0.22f);
            set (ParamIDs::cutoff, 2400.0f);
            set (ParamIDs::resonance, 0.16f);
            set (ParamIDs::filterEnvAmount, 0.44f);
            set (ParamIDs::filterAttack, 0.35f);
            set (ParamIDs::filterDecay, 1.60f);
            set (ParamIDs::filterSustain, 0.70f);
            set (ParamIDs::filterRelease, 2.60f);
            set (ParamIDs::attack, 0.55f);
            set (ParamIDs::decay, 1.40f);
            set (ParamIDs::sustain, 0.86f);
            set (ParamIDs::release, 2.80f);
            set (ParamIDs::envCurve, 0.30f);
            set (ParamIDs::gain, 0.60f);
            set (ParamIDs::drive, 0.03f);
            set (ParamIDs::ampSat, 0.18f);
            set (ParamIDs::lfoRate, 0.42f);
            set (ParamIDs::lfoDepth, 0.05f);
            set (ParamIDs::velocityAmount, 0.35f);
            set (ParamIDs::keyTracking, 0.40f);
            set (ParamIDs::voiceVariance, 0.60f);
            set (ParamIDs::fxWet, 0.46f);
            set (ParamIDs::delayTime, 0.38f);
            set (ParamIDs::delayFeedback, 0.24f);
            set (ParamIDs::chorusDepth, 0.012f);
            set (ParamIDs::chorusRate, 0.36f);
            set (ParamIDs::chorusMix, 0.52f);
            set (ParamIDs::reverbMix, 0.34f);
            set (ParamIDs::reverbModulation, 0.18f);
            set (ParamIDs::oversampling, 1.0f);
            break;

        case 30:
            // Prophet-style brass: unison-detuned saws with the amp envelope
            // opening the filter fast, then a sample-and-hold LFO for motion.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.58f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.46f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, -6.0f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.18f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::osc3PulseWidth, 0.46f);
            set (ParamIDs::osc4Level, 0.0f);
            set (ParamIDs::unisonVoices, 5.0f);
            set (ParamIDs::unisonDetune, 11.0f);
            set (ParamIDs::unisonSpread, 0.58f);
            set (ParamIDs::unisonPhase, 0.48f);
            set (ParamIDs::unisonDrift, 0.26f);
            set (ParamIDs::cutoff, 900.0f);
            set (ParamIDs::resonance, 0.30f);
            set (ParamIDs::filterEnvAmount, 0.70f);
            set (ParamIDs::filterAttack, 0.070f);
            set (ParamIDs::filterDecay, 0.90f);
            set (ParamIDs::filterSustain, 0.52f);
            set (ParamIDs::filterRelease, 0.80f);
            set (ParamIDs::attack, 0.070f);
            set (ParamIDs::decay, 0.80f);
            set (ParamIDs::sustain, 0.78f);
            set (ParamIDs::release, 0.70f);
            set (ParamIDs::envCurve, 0.50f);
            set (ParamIDs::gain, 0.60f);
            set (ParamIDs::drive, 0.08f);
            set (ParamIDs::driveCurve, 1.0f);
            set (ParamIDs::ampSat, 0.26f);
            set (ParamIDs::lfoRate, 0.90f);
            set (ParamIDs::lfoDepth, 0.06f);
            set (ParamIDs::lfoShape, 2.0f);
            set (ParamIDs::velocityAmount, 0.62f);
            set (ParamIDs::keyTracking, 0.36f);
            set (ParamIDs::voiceVariance, 0.62f);
            set (ParamIDs::fxWet, 0.24f);
            set (ParamIDs::delayTime, 0.28f);
            set (ParamIDs::delayFeedback, 0.20f);
            set (ParamIDs::chorusDepth, 0.006f);
            set (ParamIDs::chorusRate, 0.30f);
            set (ParamIDs::chorusMix, 0.22f);
            set (ParamIDs::reverbMix, 0.26f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 2.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.24f);
            break;

        case 31:
            // Minifreak-style hybrid pluck: the digital wave under a fast
            // filter envelope, with oscillator-1 FM adding the edge that a
            // wavetable engine would otherwise supply.
            set (ParamIDs::osc1Wave, 2.0f);
            set (ParamIDs::osc1Level, 0.56f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.44f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 7.0f);
            set (ParamIDs::osc3Wave, 1.0f);
            set (ParamIDs::osc3Level, 0.20f);
            set (ParamIDs::osc3Coarse, 0.0f);
            set (ParamIDs::osc3PulseWidth, 0.38f);
            set (ParamIDs::osc4Level, 0.0f);
            set (ParamIDs::unisonVoices, 2.0f);
            set (ParamIDs::unisonDetune, 4.0f);
            set (ParamIDs::unisonSpread, 0.30f);
            set (ParamIDs::cutoff, 700.0f);
            set (ParamIDs::resonance, 0.44f);
            set (ParamIDs::filterEnvAmount, 0.78f);
            set (ParamIDs::filterAttack, 0.002f);
            set (ParamIDs::filterDecay, 0.26f);
            set (ParamIDs::filterSustain, 0.10f);
            set (ParamIDs::filterRelease, 0.28f);
            set (ParamIDs::attack, 0.003f);
            set (ParamIDs::decay, 0.34f);
            set (ParamIDs::sustain, 0.12f);
            set (ParamIDs::release, 0.36f);
            set (ParamIDs::envCurve, 0.60f);
            set (ParamIDs::gain, 0.58f);
            set (ParamIDs::drive, 0.12f);
            set (ParamIDs::driveCurve, 1.0f);
            set (ParamIDs::ampSat, 0.34f);
            set (ParamIDs::velocityAmount, 0.88f);
            set (ParamIDs::keyTracking, 0.45f);
            set (ParamIDs::voiceVariance, 0.50f);
            set (ParamIDs::fxWet, 0.26f);
            set (ParamIDs::delayTime, 0.24f);
            set (ParamIDs::delayFeedback, 0.28f);
            set (ParamIDs::chorusMix, 0.10f);
            set (ParamIDs::reverbMix, 0.20f);
            set (ParamIDs::oversampling, 2.0f);
            set (ParamIDs::modSource (0), 4.0f);
            set (ParamIDs::modDestination (0), 4.0f);
            set (ParamIDs::modAmount (0), 0.18f);
            break;

        case 32:
            // Sync-sweep lead built from what the voice already has: a fast
            // filter-envelope sweep plus heavy unison detune and drift.  The
            // name describes the sweep, not a hard-sync oscillator, which
            // this engine does not implement.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.60f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.48f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 19.0f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc3Level, 0.28f);
            set (ParamIDs::osc3Coarse, 0.0f);
            set (ParamIDs::osc3Fine, -19.0f);
            set (ParamIDs::osc4Wave, 2.0f);
            set (ParamIDs::osc4Level, 0.16f);
            set (ParamIDs::osc4Coarse, -12.0f);
            set (ParamIDs::unisonVoices, 6.0f);
            set (ParamIDs::unisonDetune, 18.0f);
            set (ParamIDs::unisonSpread, 0.72f);
            set (ParamIDs::unisonPhase, 0.30f);
            set (ParamIDs::unisonDrift, 0.50f);
            set (ParamIDs::cutoff, 1100.0f);
            set (ParamIDs::resonance, 0.48f);
            set (ParamIDs::filterEnvAmount, 0.66f);
            set (ParamIDs::filterAttack, 0.020f);
            set (ParamIDs::filterDecay, 0.70f);
            set (ParamIDs::filterSustain, 0.60f);
            set (ParamIDs::filterRelease, 0.55f);
            set (ParamIDs::attack, 0.016f);
            set (ParamIDs::decay, 0.60f);
            set (ParamIDs::sustain, 0.80f);
            set (ParamIDs::release, 0.60f);
            set (ParamIDs::envCurve, 0.30f);
            set (ParamIDs::gain, 0.56f);
            set (ParamIDs::drive, 0.16f);
            set (ParamIDs::driveCurve, 2.0f);
            set (ParamIDs::ampSat, 0.42f);
            set (ParamIDs::lfoRate, 4.20f);
            set (ParamIDs::lfoDepth, 0.10f);
            set (ParamIDs::lfoPitch, 2.0f);
            set (ParamIDs::velocityAmount, 0.50f);
            set (ParamIDs::keyTracking, 0.28f);
            set (ParamIDs::voiceVariance, 0.68f);
            set (ParamIDs::fxWet, 0.20f);
            set (ParamIDs::delayTime, 0.21f);
            set (ParamIDs::delayFeedback, 0.34f);
            set (ParamIDs::delayStereo, 0.40f);
            set (ParamIDs::reverbMix, 0.18f);
            set (ParamIDs::oversampling, 2.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.18f);
            break;

        case 33:
            // Analog strings: two saws a fifth apart, slow chorus, and a
            // wide stereo delay so the ensemble reads as a section.
            set (ParamIDs::osc1Wave, 0.0f);
            set (ParamIDs::osc1Level, 0.52f);
            set (ParamIDs::osc2Wave, 0.0f);
            set (ParamIDs::osc2Level, 0.44f);
            set (ParamIDs::osc2Coarse, 0.0f);
            set (ParamIDs::osc2Fine, 7.0f);
            set (ParamIDs::osc2Pan, 0.28f);
            set (ParamIDs::osc3Wave, 0.0f);
            set (ParamIDs::osc3Level, 0.24f);
            set (ParamIDs::osc3Coarse, 12.0f);
            set (ParamIDs::osc3Fine, -6.0f);
            set (ParamIDs::osc3Pan, -0.32f);
            set (ParamIDs::osc4Level, 0.0f);
            set (ParamIDs::unisonVoices, 4.0f);
            set (ParamIDs::unisonDetune, 9.0f);
            set (ParamIDs::unisonSpread, 0.80f);
            set (ParamIDs::unisonPhase, 0.40f);
            set (ParamIDs::unisonDrift, 0.30f);
            set (ParamIDs::cutoff, 2200.0f);
            set (ParamIDs::resonance, 0.18f);
            set (ParamIDs::filterEnvAmount, 0.26f);
            set (ParamIDs::filterAttack, 0.80f);
            set (ParamIDs::filterDecay, 1.80f);
            set (ParamIDs::filterSustain, 0.78f);
            set (ParamIDs::filterRelease, 1.60f);
            set (ParamIDs::attack, 0.70f);
            set (ParamIDs::decay, 1.20f);
            set (ParamIDs::sustain, 0.88f);
            set (ParamIDs::release, 1.90f);
            set (ParamIDs::envCurve, 0.32f);
            set (ParamIDs::gain, 0.56f);
            set (ParamIDs::drive, 0.04f);
            set (ParamIDs::ampSat, 0.16f);
            set (ParamIDs::lfoRate, 0.07f);
            set (ParamIDs::lfoDepth, 0.14f);
            set (ParamIDs::velocityAmount, 0.42f);
            set (ParamIDs::keyTracking, 0.38f);
            set (ParamIDs::voiceVariance, 0.65f);
            set (ParamIDs::fxWet, 0.50f);
            set (ParamIDs::delayTime, 0.36f);
            set (ParamIDs::delayFeedback, 0.32f);
            set (ParamIDs::delayStereo, 0.80f);
            set (ParamIDs::chorusDepth, 0.013f);
            set (ParamIDs::chorusRate, 0.17f);
            set (ParamIDs::chorusMix, 0.46f);
            set (ParamIDs::reverbMix, 0.40f);
            set (ParamIDs::reverbModulation, 0.22f);
            set (ParamIDs::oversampling, 1.0f);
            set (ParamIDs::modSource (0), 1.0f);
            set (ParamIDs::modDestination (0), 2.0f);
            set (ParamIDs::modAmount (0), 0.10f);
            set (ParamIDs::modSource (1), 3.0f);
            set (ParamIDs::modDestination (1), 1.0f);
            set (ParamIDs::modAmount (1), 0.12f);
            break;

        default:
            break;
    }
}
}
