#pragma once

#include <JuceHeader.h>

#include <array>

class ChipperEditorShell final : public juce::Component
{
public:
    struct Controls
    {
        juce::Label& title;
        juce::Label& status;
        juce::Label& build;
        juce::Label& midiCc;
        juce::Label& chipSummary;
        std::array<juce::Label, 5>& headerLabels;
        juce::ComboBox& presetFilter;
        juce::TextEditor& presetSearch;
        juce::ComboBox& preset;
        juce::TextButton& favorite;
        juce::TextButton& load;
        juce::TextButton& save;
        juce::TextButton& saveAs;
        juce::ComboBox& chipMode;
        juce::ComboBox& strictness;
        juce::ComboBox& macro;
        juce::ComboBox& playMode;
    };

    explicit ChipperEditorShell(Controls controlsToUse);

    void resized() override;

private:
    Controls controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperEditorShell)
};
