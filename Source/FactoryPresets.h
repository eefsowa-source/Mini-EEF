#pragma once

#include <JuceHeader.h>

class EonMiniEEFProcessor;

// Built-in patches are data-driven APVTS snapshots rather than copies of a
// third-party preset bank.  They remain available in every plug-in instance
// and use the same state path as user-saved .eonpreset files.
namespace FactoryPresets
{
const juce::StringArray& names();
void apply (EonMiniEEFProcessor&, int presetIndex);
}
