#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "editor_size_smoke: " << message << '\n';

    return condition;
}

bool setChoiceParameter(ChipperAudioProcessor& processor, const juce::String& parameterId, int choice)
{
    auto* parameter = processor.getValueTreeState().getParameter(parameterId);
    auto* choiceParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);
    if (choiceParameter == nullptr)
    {
        std::cerr << "editor_size_smoke: missing choice parameter " << parameterId << '\n';
        return false;
    }

    const auto choiceCount = choiceParameter->choices.size();
    if (choice < 0 || choice >= choiceCount)
    {
        std::cerr << "editor_size_smoke: choice out of range for " << parameterId << '\n';
        return false;
    }

    const auto normalised = choiceCount > 1 ? static_cast<float>(choice) / static_cast<float>(choiceCount - 1) : 0.0f;
    choiceParameter->beginChangeGesture();
    choiceParameter->setValueNotifyingHost(normalised);
    choiceParameter->endChangeGesture();
    return true;
}

int expectedHeightForChipMode(int chipMode)
{
    const auto mode = chipper::parameters::chipModeFromChoice(chipMode);
    return (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula) ? 720 : 660;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);

    bool ok = true;
    ok &= expect(editor.getWidth() == 1240, "unexpected default width");
    ok &= expect(editor.getHeight() == 660, "unexpected default height");

    editor.setSize(1240, 1200);
    ok &= expect(editor.getHeight() == 660, "host-restored default editor height was not clamped to its chip budget");

    editor.setSize(1000, 600);
    ok &= expect(editor.getWidth() >= 1180, "editor width was not clamped to minimum");
    ok &= expect(editor.getHeight() >= 620, "editor height was not clamped to minimum");

    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        ok &= expect(chipEditor.getWidth() == 1240, "chip default width changed");
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip default height changed");
        ok &= expect(chipEditor.getHeight() <= 720, "chip default height exceeded DAW-friendly cap");
        chipEditor.setSize(1240, 1200);
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip-switched editor height was not clamped to its chip budget");
        ok &= expect(chipEditor.getWidth() == 1240, "chip-switched editor width unexpectedly changed");
    }

    return ok ? 0 : 1;
}
