#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <iostream>
#include <string>
#include <typeinfo>

namespace
{
constexpr int expectedEditorHeight = 880;
constexpr int expectedEditorMinimumHeight = 720;

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

int chipModeChoiceFor(chipper::ChipMode target)
{
    const auto choiceCount = chipper::parameters::chipModeChoices().size();
    for (int choice = 0; choice < choiceCount; ++choice)
    {
        if (chipper::parameters::chipModeFromChoice(choice) == target)
            return choice;
    }

    return -1;
}

int expectedHeightForChipMode(int chipMode)
{
    const auto mode = chipper::parameters::chipModeFromChoice(chipMode);
    switch (mode)
    {
    case chipper::ChipMode::sid:
    case chipper::ChipMode::spc700:
    case chipper::ChipMode::paula:
        return expectedEditorHeight;

    case chipper::ChipMode::nes:
        return 820;

    default:
        return 820;
    }
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

            const auto* comboBox = dynamic_cast<const juce::ComboBox*>(child);
            const auto* textButton = dynamic_cast<const juce::TextButton*>(child);
            const auto* slider = dynamic_cast<const juce::Slider*>(child);
            if (comboBox != nullptr)
            {
                if (bounds.getWidth() < 48 || bounds.getHeight() < 24)
                {
                    std::cerr << "editor_size_smoke: dropdown below readable standard size at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString()
                              << " text \"" << comboBox->getText() << "\"\n";
                    ok = false;
                }
            }

            if (textButton != nullptr)
            {
                if (bounds.getWidth() < 24 || bounds.getHeight() < 18)
                {
                    std::cerr << "editor_size_smoke: button below readable minimum size at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString()
                              << " text \"" << textButton->getButtonText() << "\"\n";
                    ok = false;
                }
            }

            if (slider != nullptr)
            {
                const auto isHorizontalSlider = bounds.getWidth() >= bounds.getHeight();
                const auto minimumWidth = isHorizontalSlider ? 48 : 12;
                const auto minimumHeight = isHorizontalSlider ? 6 : 48;
                if (bounds.getWidth() < minimumWidth || bounds.getHeight() < minimumHeight)
                {
                    std::cerr << "editor_size_smoke: slider below readable minimum size at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString() << "\n";
                    ok = false;
                }
            }
        }

        ok &= checkVisibleChildGeometry(root, *child, absoluteBounds.getPosition(), childPath);
    }

    return ok;
}

bool expectControlOwnedBySourceChannel(const ChipperAudioProcessorEditor& editor,
                                       size_t channel,
                                       juce::Rectangle<int> controlBounds,
                                       const char* controlName)
{
    const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
    auto ok = true;

    if (sourceBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: missing source channel bounds for " << controlName << '\n';
        return false;
    }

    if (controlBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: missing channel-owned control bounds for " << controlName << '\n';
        return false;
    }

    if (controlBounds.getWidth() < 40 || controlBounds.getHeight() < 18)
    {
        std::cerr << "editor_size_smoke: channel-owned control below readable size for "
                  << controlName << " bounds " << controlBounds.toString() << '\n';
        ok = false;
    }

    if (! sourceBounds.expanded(2).contains(controlBounds))
    {
        std::cerr << "editor_size_smoke: " << controlName
                  << " is not owned by its channel card; source "
                  << sourceBounds.toString() << " control "
                  << controlBounds.toString() << '\n';
        ok = false;
    }

    return ok;
}

bool checkChannelOwnedControlLayout(chipper::ChipMode mode)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: chip mode choice unavailable for ownership layout check\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));

    switch (mode)
    {
    case chipper::ChipMode::nes:
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getPulseDutyBoundsForLayoutTest(), "NES pulse 1 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 1, editor.getPulse2DutyBoundsForLayoutTest(), "NES pulse 2 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeBoundsForLayoutTest(), "NES noise mode");
        break;

    case chipper::ChipMode::dmg:
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getPulseDutyBoundsForLayoutTest(), "DMG pulse 1 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 1, editor.getPulse2DutyBoundsForLayoutTest(), "DMG pulse 2 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 2, editor.getDmgWaveLevelBoundsForLayoutTest(), "DMG wave level");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeBoundsForLayoutTest(), "DMG noise mode");
        break;

    case chipper::ChipMode::sn76489:
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeMenuBoundsForLayoutTest(), "SN76489 noise mode");
        break;

    default:
        break;
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
    ok &= expect(editor.getHeight() == expectedHeightForChipMode(0), "unexpected default height");

    editor.setSize(1240, 1200);
    ok &= expect(editor.getHeight() == expectedEditorHeight, "host-restored default editor height was not clamped to the DAW-friendly cap");

    editor.setSize(1000, 600);
    ok &= expect(editor.getWidth() >= 1180, "editor width was not clamped to minimum");
    ok &= expect(editor.getHeight() >= expectedEditorMinimumHeight, "editor height was not clamped to minimum");

    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        const auto chipPath = chipper::parameters::chipModeChoices()[chipMode].toStdString();
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        ok &= expect(chipEditor.getWidth() == 1240, "chip default width changed");
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip default height changed");
        ok &= expect(chipEditor.getHeight() <= expectedEditorHeight, "chip default height exceeded DAW-friendly cap");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/" + chipPath);
        chipEditor.setSize(1240, 1200);
        ok &= expect(chipEditor.getHeight() == expectedEditorHeight, "chip-switched editor height was not clamped to the DAW-friendly cap");
        ok &= expect(chipEditor.getWidth() == 1240, "chip-switched editor width unexpectedly changed");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/restored/" + chipPath);
    }

    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nes);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::dmg);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::sn76489);

    return ok ? 0 : 1;
}
