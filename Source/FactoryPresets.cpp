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
        "Sub Mono Bass", "Acid Bass"
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

        default:
            break;
    }
}
}
