#include "../Source/PluginEditor.h"
#include <cstdlib>

// Renders the editor at a window size taken from the command line, so an
// arbitrary supported size can be checked without adding a target per size.
//
//   EonMiniEEF_UISnapshotAt <width> <height> [output.png]
//
// The editor refuses sizes below its resize limits, so a small argument is
// reported rather than silently clamped: a clamped render would look like a
// pass while proving nothing about the size that was asked for.
int main (int argc, char** argv)
{
    // The arguments are used as given: clamping them here would hide the very
    // mistake the tool exists to catch.
    const int width  = argc > 1 ? (int) std::atol (argv[1]) : 1200;
    const int height = argc > 2 ? (int) std::atol (argv[2]) : 800;
    if (width < 1 || height < 1)
    {
        std::cerr << "usage: EonMiniEEF_UISnapshotAt <width> <height> [output.png]\n";
        return 3;
    }
    const auto requested = juce::String (width) + "x" + juce::String (height);

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EonMiniEEFProcessor processor;
    EonMiniEEFEditor editor (processor);
    editor.setSize (width, height);

    // setSize is clamped by the editor's resize limits, so compare against what
    // was asked for instead of assuming it took.
    if (editor.getWidth() != width || editor.getHeight() != height)
    {
        juce::Logger::writeToLog ("requested " + requested + " but the editor clamped to "
                                  + juce::String (editor.getWidth()) + "x"
                                  + juce::String (editor.getHeight()));
        std::cerr << "unsupported window size: requested " << requested.toStdString()
                  << ", editor allows " << editor.getWidth() << "x" << editor.getHeight()
                  << "\n";
        return 2;
    }

    const auto snapshot = editor.createComponentSnapshot (editor.getLocalBounds(), true);
    const auto name = argc > 3 ? juce::String (argv[3])
                                : juce::String ("eon-mini-eef-ui-") + requested + ".png";
    const auto output = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    output.deleteFile();
    auto stream = output.createOutputStream();
    juce::PNGImageFormat png;
    const bool ok = stream != nullptr && png.writeImageToStream (snapshot, *stream);
    if (stream != nullptr)
        stream->flush();
    return ok ? 0 : 1;
}
