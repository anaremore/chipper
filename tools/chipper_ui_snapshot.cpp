#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

namespace
{
constexpr int compactEditorWidth = 1180;
constexpr int defaultEditorWidth = 1240;

struct Options
{
    juce::File outputDirectory;
    juce::String chip = "all";
    std::vector<int> widths { compactEditorWidth, defaultEditorWidth };
    bool manifestOnly = false;
};

void printUsage()
{
    std::cout
        << "Usage: chipper_ui_snapshot [--output <directory>] [--chip <name|all>]\n"
        << "                           [--width <1180|1240|both>] [--manifest-only]\n";
}

std::optional<Options> parseOptions(int argc, char** argv)
{
    Options options;
    options.outputDirectory = juce::File::getCurrentWorkingDirectory().getChildFile("ui-captures");

    for (int i = 1; i < argc; ++i)
    {
        const auto argument = juce::String::fromUTF8(argv[i]);
        const auto nextValue = [&]() -> std::optional<juce::String>
        {
            if (i + 1 >= argc)
                return std::nullopt;

            return juce::String::fromUTF8(argv[++i]);
        };

        if (argument == "--output")
        {
            const auto value = nextValue();
            if (! value.has_value())
                return std::nullopt;

            options.outputDirectory = juce::File::getCurrentWorkingDirectory().getChildFile(*value);
        }
        else if (argument == "--chip")
        {
            const auto value = nextValue();
            if (! value.has_value())
                return std::nullopt;

            options.chip = *value;
        }
        else if (argument == "--width")
        {
            const auto value = nextValue();
            if (! value.has_value())
                return std::nullopt;

            if (*value == "1180")
                options.widths = { compactEditorWidth };
            else if (*value == "1240")
                options.widths = { defaultEditorWidth };
            else if (*value == "both")
                options.widths = { compactEditorWidth, defaultEditorWidth };
            else
                return std::nullopt;
        }
        else if (argument == "--manifest-only")
        {
            options.manifestOnly = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            printUsage();
            return std::nullopt;
        }
        else
        {
            std::cerr << "Unknown argument: " << argument << '\n';
            return std::nullopt;
        }
    }

    return options;
}

bool setChipMode(ChipperAudioProcessor& processor, int choice)
{
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(
        processor.getValueTreeState().getParameter(chipper::parameters::id::chipMode));
    if (parameter == nullptr || choice < 0 || choice >= parameter->choices.size())
        return false;

    const auto normalised = parameter->choices.size() > 1
        ? static_cast<float>(choice) / static_cast<float>(parameter->choices.size() - 1)
        : 0.0f;
    parameter->setValueNotifyingHost(normalised);
    return true;
}

juce::String componentType(const juce::Component& component)
{
    if (dynamic_cast<const juce::ToggleButton*>(&component) != nullptr) return "ToggleButton";
    if (dynamic_cast<const juce::TextButton*>(&component) != nullptr) return "TextButton";
    if (dynamic_cast<const juce::Button*>(&component) != nullptr) return "Button";
    if (dynamic_cast<const juce::ComboBox*>(&component) != nullptr) return "ComboBox";
    if (dynamic_cast<const juce::Slider*>(&component) != nullptr) return "Slider";
    if (dynamic_cast<const juce::Label*>(&component) != nullptr) return "Label";
    if (dynamic_cast<const juce::TextEditor*>(&component) != nullptr) return "TextEditor";
    if (dynamic_cast<const ChipWaveformPreview*>(&component) != nullptr) return "ChipWaveformPreview";
    if (dynamic_cast<const ChipEnvelopePreview*>(&component) != nullptr) return "ChipEnvelopePreview";
    if (dynamic_cast<const FmAlgorithmPreview*>(&component) != nullptr) return "FmAlgorithmPreview";
    if (dynamic_cast<const OplWaveformPreview*>(&component) != nullptr) return "OplWaveformPreview";
    if (dynamic_cast<const OutputScopePreview*>(&component) != nullptr) return "OutputScopePreview";
    if (dynamic_cast<const SampleWaveformPreview*>(&component) != nullptr) return "SampleWaveformPreview";
    if (dynamic_cast<const ChipperAudioProcessorEditor*>(&component) != nullptr) return "ChipperAudioProcessorEditor";
    return "Component";
}

juce::String componentText(const juce::Component& component)
{
    if (const auto* button = dynamic_cast<const juce::Button*>(&component))
        return button->getButtonText();
    if (const auto* comboBox = dynamic_cast<const juce::ComboBox*>(&component))
        return comboBox->getText();
    if (const auto* label = dynamic_cast<const juce::Label*>(&component))
        return label->getText();
    if (const auto* editor = dynamic_cast<const juce::TextEditor*>(&component))
        return editor->getText();
    return {};
}

juce::var componentManifest(const juce::Component& component,
                            juce::Point<int> absolutePosition,
                            const juce::String& path,
                            int& componentCount,
                            int& visibleComponentCount,
                            int& focusableComponentCount)
{
    ++componentCount;
    if (component.isVisible())
        ++visibleComponentCount;
    if (component.getWantsKeyboardFocus())
        ++focusableComponentCount;

    auto* object = new juce::DynamicObject();
    object->setProperty("path", path);
    object->setProperty("type", componentType(component));
    object->setProperty("id", component.getComponentID());
    object->setProperty("name", component.getName());
    object->setProperty("text", componentText(component));
    object->setProperty("x", absolutePosition.x);
    object->setProperty("y", absolutePosition.y);
    object->setProperty("width", component.getWidth());
    object->setProperty("height", component.getHeight());
    object->setProperty("visible", component.isVisible());
    object->setProperty("enabled", component.isEnabled());
    object->setProperty("keyboardFocusable", component.getWantsKeyboardFocus());

    juce::Array<juce::var> children;
    for (auto childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
    {
        const auto* child = component.getChildComponent(childIndex);
        if (child == nullptr)
            continue;

        children.add(componentManifest(*child,
                                       absolutePosition + child->getPosition(),
                                       path + "/" + juce::String(childIndex),
                                       componentCount,
                                       visibleComponentCount,
                                       focusableComponentCount));
    }
    object->setProperty("children", juce::var(children));
    return juce::var(object);
}

juce::String fileKey(juce::String value)
{
    value = value.toLowerCase();
    juce::String result;
    bool previousWasDash = false;
    for (const auto character : value)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character))
        {
            result += character;
            previousWasDash = false;
        }
        else if (! previousWasDash && result.isNotEmpty())
        {
            result += '-';
            previousWasDash = true;
        }
    }

    return result.trimCharactersAtEnd("-");
}

bool writePng(ChipperAudioProcessorEditor& editor, const juce::File& destination)
{
    auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    if (! image.isValid())
        return false;

    juce::FileOutputStream stream(destination);
    if (stream.failedToOpen())
        return false;

    juce::PNGImageFormat png;
    return png.writeImageToStream(image, stream);
}

std::optional<int> requestedChipChoice(const juce::String& requested)
{
    if (requested == "all")
        return std::nullopt;

    if (const auto mode = chipper::parseChipMode(requested.toStdString()))
    {
        const auto choiceCount = chipper::parameters::chipModeChoices().size();
        for (int choice = 0; choice < choiceCount; ++choice)
        {
            if (chipper::parameters::chipModeFromChoice(choice) == *mode)
                return choice;
        }
    }

    return -1;
}
}

int main(int argc, char** argv)
{
    const auto parsedOptions = parseOptions(argc, argv);
    if (! parsedOptions.has_value())
        return argc > 1 && (juce::String::fromUTF8(argv[1]) == "--help" || juce::String::fromUTF8(argv[1]) == "-h") ? 0 : 2;

    const auto options = *parsedOptions;
    if (const auto result = options.outputDirectory.createDirectory(); result.failed())
    {
        std::cerr << result.getErrorMessage() << '\n';
        return 1;
    }

    const auto selectedChoice = requestedChipChoice(options.chip);
    if (selectedChoice.has_value() && *selectedChoice < 0)
    {
        std::cerr << "Unknown chip: " << options.chip << '\n';
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    juce::Array<juce::var> snapshots;
    const auto chipModeCount = chipper::parameters::chipModeChoices().size();

    for (int choice = 0; choice < chipModeCount; ++choice)
    {
        if (selectedChoice.has_value() && choice != *selectedChoice)
            continue;

        for (const auto width : options.widths)
        {
            ChipperAudioProcessor processor;
            if (! setChipMode(processor, choice))
            {
                std::cerr << "Could not select chip choice " << choice << '\n';
                return 1;
            }

            ChipperAudioProcessorEditor editor(processor);
            editor.setSize(width, editor.getHeight());
            editor.runEditorUpdateForLayoutTest();
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);

            const auto displayName = chipper::parameters::chipModeChoices()[choice];
            const auto snapshotKey = juce::String(choice).paddedLeft('0', 2)
                + "-" + fileKey(displayName)
                + "-" + juce::String(width);
            const auto imageName = snapshotKey + ".png";

            if (! options.manifestOnly && ! writePng(editor, options.outputDirectory.getChildFile(imageName)))
            {
                std::cerr << "Could not write " << imageName << '\n';
                return 1;
            }

            int componentCount = 0;
            int visibleComponentCount = 0;
            int focusableComponentCount = 0;
            auto* snapshot = new juce::DynamicObject();
            snapshot->setProperty("chipChoice", choice);
            snapshot->setProperty("chip", displayName);
            snapshot->setProperty("key", snapshotKey);
            snapshot->setProperty("image", options.manifestOnly ? juce::String() : imageName);
            snapshot->setProperty("width", editor.getWidth());
            snapshot->setProperty("height", editor.getHeight());
            snapshot->setProperty("components",
                                  componentManifest(editor,
                                                    {},
                                                    "editor",
                                                    componentCount,
                                                    visibleComponentCount,
                                                    focusableComponentCount));
            snapshot->setProperty("componentCount", componentCount);
            snapshot->setProperty("visibleComponentCount", visibleComponentCount);
            snapshot->setProperty("focusableComponentCount", focusableComponentCount);
            snapshots.add(juce::var(snapshot));
        }
    }

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty("schema", "chipper-ui-captures-v1");
    manifest->setProperty("manifestOnly", options.manifestOnly);
    manifest->setProperty("snapshotCount", snapshots.size());
    manifest->setProperty("snapshots", juce::var(snapshots));

    const auto manifestFile = options.outputDirectory.getChildFile("manifest.json");
    if (! manifestFile.replaceWithText(juce::JSON::toString(juce::var(manifest), true)))
    {
        std::cerr << "Could not write " << manifestFile.getFullPathName() << '\n';
        return 1;
    }

    std::cout << "Wrote " << snapshots.size() << " UI snapshot manifest entries to "
              << manifestFile.getFullPathName() << '\n';
    return snapshots.isEmpty() ? 1 : 0;
}
