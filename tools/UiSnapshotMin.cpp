#include "../Source/PluginEditor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    EonMiniEEFProcessor processor;
    EonMiniEEFEditor editor (processor);
    editor.setSize (1200, 800);
    const auto snapshot = editor.createComponentSnapshot (editor.getLocalBounds(), true);
    const auto output = juce::File::getCurrentWorkingDirectory()
                            .getChildFile ("eon-mini-eef-ui-min.png");
    output.deleteFile();
    auto stream = output.createOutputStream();
    juce::PNGImageFormat png;
    const bool ok = stream != nullptr && png.writeImageToStream (snapshot, *stream);
    if (stream != nullptr)
        stream->flush();
    return ok ? 0 : 1;
}
