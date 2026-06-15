#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <iostream>
#include <string>
#include <typeinfo>

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
    return (mode == chipper::ChipMode::nes || mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula) ? 680 : 620;
}

bool checkVisibleChildGeometry(const juce::Component& root,
                               const juce::Component& component,
                               juce::Point<int> parentOrigin,
                               const std::string& path)
{
    bool ok = true;

    for (auto childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
    {
        const auto* child = component.getChildComponent(childIndex);
        if (child == nullptr || ! child->isVisible())
            continue;

        const auto bounds = child->getBounds();
        const auto absoluteBounds = bounds.translated(parentOrigin.x, parentOrigin.y);
        const auto childPath = path + "/" + std::to_string(childIndex);

        if (! bounds.isEmpty())
        {
            if (! root.getLocalBounds().expanded(2).contains(absoluteBounds))
            {
                std::cerr << "editor_size_smoke: visible child out of editor bounds at "
                          << childPath << " bounds " << absoluteBounds.toString() << '\n';
                ok = false;
            }

            if (dynamic_cast<const juce::ComboBox*>(child) != nullptr
                || dynamic_cast<const juce::TextButton*>(child) != nullptr)
            {
                if (bounds.getWidth() < 24 || bounds.getHeight() < 16)
                {
                    std::cerr << "editor_size_smoke: interactive control too small at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString() << '\n';
                    ok = false;
                }
            }
        }

        ok &= checkVisibleChildGeometry(root, *child, absoluteBounds.getPosition(), childPath);
    }

    return ok;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);

    bool ok = true;
    ok &= expect(editor.getWidth() == 1240, "unexpected default width");
    ok &= expect(editor.getHeight() == 680, "unexpected default height");

    editor.setSize(1240, 1200);
    ok &= expect(editor.getHeight() == 680, "host-restored default editor height was not clamped to its chip budget");

    editor.setSize(1000, 600);
    ok &= expect(editor.getWidth() >= 1180, "editor width was not clamped to minimum");
    ok &= expect(editor.getHeight() >= 560, "editor height was not clamped to minimum");

    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        const auto chipPath = chipper::parameters::chipModeChoices()[chipMode].toStdString();
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        ok &= expect(chipEditor.getWidth() == 1240, "chip default width changed");
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip default height changed");
        ok &= expect(chipEditor.getHeight() <= 680, "chip default height exceeded DAW-friendly cap");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/" + chipPath);
        chipEditor.setSize(1240, 1200);
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip-switched editor height was not clamped to its chip budget");
        ok &= expect(chipEditor.getWidth() == 1240, "chip-switched editor width unexpectedly changed");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/restored/" + chipPath);
    }

    return ok ? 0 : 1;
}
