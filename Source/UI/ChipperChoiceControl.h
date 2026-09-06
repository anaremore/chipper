#pragma once

#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

// A native discrete choice. Index zero may inherit a value from the patch.
// The parameter owns the choice; displaying its resolved value never writes it.
class ChipperChoiceControl final : public juce::Component
{
public:
    ChipperChoiceControl();
    void bind(juce::AudioProcessorValueTreeState&, const juce::String& parameterId,
              const juce::StringArray& choices, bool firstChoiceInherits = true);
    void setResolvedValue(const juce::String& value);
    void setHelp(const juce::String& name, const juce::String& help);
    void setSegmented(bool enabled);
    void setTheme(juce::Colour background, juce::Colour accent, juce::Colour outline,
                  juce::Colour text, juce::Colour darkText);
    void resized() override;
    void selectChoice(int index); // Same complete parameter gesture as a user click.
    int selectedChoice() const noexcept { return selected; }
    juce::String displayText() const;
    juce::ComboBox& menu() noexcept { return choiceMenu; }
    juce::Rectangle<int> choiceBounds(int index) const;
    bool showingSegments() const noexcept { return segmentsVisible; }
    std::function<void()> onUserChange;

private:
    void refreshSelection(float value);
    void refreshText();
    juce::ComboBox choiceMenu;
    juce::Label inheritedValue;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    juce::StringArray labels;
    juce::String boundId, resolvedValue, helpText, accessibleName;
    bool inherits = true, preferSegments = false, segmentsVisible = false;
    int selected = 0;
    juce::Colour backgroundColour, accentColour, outlineColour, textColour, darkTextColour;
    // Destroy the attachment before its widgets and the processor-owned state.
    std::unique_ptr<juce::ParameterAttachment> attachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperChoiceControl)
};
