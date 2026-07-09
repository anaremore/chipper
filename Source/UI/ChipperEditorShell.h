#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>

#include "ChipperWorkflowBar.h"

enum class ChipperEditorWorkspace
{
    play,
    edit,
    inspect
};

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
        juce::TextButton& browser;
        juce::TextButton& favorite;
        juce::TextButton& load;
        juce::TextButton& save;
        juce::TextButton& saveAs;
        juce::ComboBox& chipMode;
        juce::ComboBox& strictness;
        juce::ComboBox& macro;
        juce::ComboBox& playMode;
        ChipperWorkflowBar& workflow;
    };

    explicit ChipperEditorShell(Controls controlsToUse);

    void resized() override;
    void attachExternalControlsTo(juce::Component& parent);
    bool isExternalControl(const juce::Component* component) const noexcept;
    void setWorkspace(ChipperEditorWorkspace workspaceToUse, juce::NotificationType notification = juce::dontSendNotification);
    ChipperEditorWorkspace workspace() const noexcept { return selectedWorkspace; }
    void setTheme(juce::Colour primary,
                  juce::Colour accent,
                  juce::Colour outline,
                  juce::Colour text,
                  juce::Colour mutedText,
                  juce::Colour darkText);

    std::function<void(ChipperEditorWorkspace)> onWorkspaceChanged;

    juce::Rectangle<int> workspaceBoundsForTest() const noexcept { return workspaceBounds; }
    juce::Rectangle<int> workflowBoundsForTest() const noexcept { return controls.workflow.getBounds(); }
    juce::Rectangle<int> workspaceButtonBoundsForTest(ChipperEditorWorkspace workspaceToFind) const;

private:
    Controls controls;
    juce::Label workspaceLabel;
    std::array<juce::TextButton, 3> workspaceButtons;
    ChipperEditorWorkspace selectedWorkspace = ChipperEditorWorkspace::edit;
    juce::Rectangle<int> workspaceBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperEditorShell)
};
