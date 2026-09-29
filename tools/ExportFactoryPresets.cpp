#include "../Source/FactoryPresets.h"
#include "../Source/PluginProcessor.h"

#include <cmath>
#include <iostream>

namespace
{
const juce::StringArray fileNames {
    "01_init.eonpreset",
    "02_supersaw_pad.eonpreset",
    "03_trance_pluck.eonpreset",
    "04_arena_lead.eonpreset",
    "05_sub_mono_bass.eonpreset",
    "06_acid_bass.eonpreset",
    "07_glass_keys.eonpreset",
    "08_warm_poly.eonpreset",
    "09_velvet_strings.eonpreset",
    "10_neon_bell.eonpreset",
    "11_pulse_sequence.eonpreset",
    "12_digital_pluck.eonpreset",
    "13_wide_brass.eonpreset",
    "14_soft_organ.eonpreset",
    "15_juno_choir.eonpreset",
    "16_motion_pad.eonpreset",
    "17_fmish_bass.eonpreset",
    "18_rubber_mono.eonpreset",
    "19_resonant_sweep.eonpreset",
    "20_noise_sfx.eonpreset",
    "21_lo_fi_keys.eonpreset",
    "22_dream_lead.eonpreset",
    "23_octave_stab.eonpreset",
    "24_deep_drone.eonpreset",
    "25_percussive_click.eonpreset",
    "26_classic_pwm.eonpreset",
    "27_minimoog_lead.eonpreset",
    "28_moog_bass.eonpreset",
    "29_diva_saw_pad.eonpreset",
    "30_juno_pad.eonpreset",
    "31_prophet_brass.eonpreset",
    "32_minifreak_pluck.eonpreset",
    "33_sync_sweep_lead.eonpreset",
    "34_analog_strings.eonpreset"
};

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
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const auto destination = argc > 1 ? juce::File (argv[1])
                                      : juce::File::getCurrentWorkingDirectory().getChildFile ("Presets");

    if (! destination.createDirectory())
    {
        std::cerr << "Could not create preset directory: " << destination.getFullPathName() << '\n';
        return 1;
    }

    if (FactoryPresets::names().size() != fileNames.size())
    {
        std::cerr << "Factory preset names and file names are out of sync\n";
        return 1;
    }

    for (int index = 0; index < FactoryPresets::names().size(); ++index)
    {
        EonMiniEEFProcessor original;
        FactoryPresets::apply (original, index);

        juce::MemoryBlock state;
        original.getStateInformation (state);
        const auto output = destination.getChildFile (fileNames[index]);
        if (! output.replaceWithData (state.getData(), state.getSize()))
        {
            std::cerr << "Could not save: " << output.getFullPathName() << '\n';
            return 1;
        }

        juce::MemoryBlock fromDisk;
        if (! output.loadFileAsData (fromDisk))
        {
            std::cerr << "Could not reload: " << output.getFullPathName() << '\n';
            return 1;
        }

        EonMiniEEFProcessor restored;
        restored.setStateInformation (fromDisk.getData(), static_cast<int> (fromDisk.getSize()));
        if (! hasMatchingParameterValues (original, restored))
        {
            std::cerr << "Disk state round trip failed for " << FactoryPresets::names()[index] << '\n';
            return 1;
        }

        std::cout << output.getFullPathName() << '\n';
    }

    return 0;
}
