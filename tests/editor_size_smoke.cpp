#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "editor_size_smoke: " << message << '\n';

    return condition;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);

    bool ok = true;
    ok &= expect(editor.getWidth() == 1240, "unexpected default width");
    ok &= expect(editor.getHeight() == 760, "unexpected default height");

    editor.setSize(1240, 1200);
    ok &= expect(editor.getHeight() <= 820, "host-restored editor height was not clamped");

    editor.setSize(1000, 600);
    ok &= expect(editor.getWidth() >= 1180, "editor width was not clamped to minimum");
    ok &= expect(editor.getHeight() >= 700, "editor height was not clamped to minimum");

    return ok ? 0 : 1;
}
