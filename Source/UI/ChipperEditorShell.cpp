#include "ChipperEditorShell.h"

#include <algorithm>

ChipperEditorShell::ChipperEditorShell(Controls controlsToUse)
    : controls(controlsToUse)
{
    setInterceptsMouseClicks(false, true);

    addAndMakeVisible(controls.title);
    for (auto& label : controls.headerLabels)
        addAndMakeVisible(label);
    addAndMakeVisible(controls.presetFilter);
    addAndMakeVisible(controls.presetSearch);
    addAndMakeVisible(controls.preset);
    addAndMakeVisible(controls.browser);
    addAndMakeVisible(controls.favorite);
    addAndMakeVisible(controls.load);
    addAndMakeVisible(controls.save);
    addAndMakeVisible(controls.saveAs);
    addAndMakeVisible(controls.chipMode);
    addAndMakeVisible(controls.strictness);
    addAndMakeVisible(controls.macro);
    addAndMakeVisible(controls.playMode);
    addAndMakeVisible(controls.workflow);
    addAndMakeVisible(controls.chipSummary);
    addAndMakeVisible(controls.status);
    addAndMakeVisible(controls.midiCc);
    addAndMakeVisible(controls.build);

    workspaceLabel.setText("Workspace", juce::dontSendNotification);
    workspaceLabel.setJustificationType(juce::Justification::centredRight);
    workspaceLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(workspaceLabel);

    static constexpr std::array<const char*, 3> workspaceNames { "Play", "Edit", "Inspect" };
    static constexpr std::array<const char*, 3> workspaceHelp {
        "Play workspace: essential sources, musical macros, and output. Shortcut: Ctrl/Cmd+1.",
        "Edit workspace: the full chip-native editor. Shortcut: Ctrl/Cmd+2.",
        "Inspect workspace: verification, implementation evidence, gaps, and control contract. Shortcut: Ctrl/Cmd+3."
    };
    for (size_t i = 0; i < workspaceButtons.size(); ++i)
    {
        auto& button = workspaceButtons[i];
        button.setButtonText(workspaceNames[i]);
        button.setComponentID("workspace." + juce::String(workspaceNames[i]).toLowerCase());
        button.setName(juce::String(workspaceNames[i]) + " workspace");
        button.setTooltip(workspaceHelp[i]);
        button.setClickingTogglesState(true);
        button.setRadioGroupId(0x43485052);
        button.setWantsKeyboardFocus(true);
        button.onClick = [this, i]
        {
            setWorkspace(static_cast<ChipperEditorWorkspace>(i), juce::sendNotification);
        };
        addAndMakeVisible(button);
    }
    int focusOrder = 1;
    for (auto* component : std::array<juce::Component*, 11> {
             &controls.browser, &controls.preset, &controls.favorite, &controls.load,
             &controls.save, &controls.saveAs, &controls.chipMode, &controls.strictness,
             &controls.playMode, &workspaceButtons[0], &workspaceButtons[1] })
        component->setExplicitFocusOrder(focusOrder++);
    workspaceButtons[2].setExplicitFocusOrder(focusOrder);
    setWorkspace(selectedWorkspace);
}

void ChipperEditorShell::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto top = area.removeFromTop(56);
    const auto placeHeaderCombo = [this](size_t index, juce::ComboBox& comboBox, juce::Rectangle<int> bounds)
    {
        controls.headerLabels[index].setBounds(bounds.removeFromTop(16));
        comboBox.setBounds(bounds.reduced(0, 4));
    };

    controls.title.setBounds(top.removeFromLeft(230));
    top.removeFromLeft(8);

    constexpr auto headerGap = 8;
    constexpr auto compactGap = 4;
    constexpr auto favoriteButtonWidth = 42;
    constexpr auto loadButtonWidth = 46;
    constexpr auto saveButtonWidth = 56;
    constexpr auto saveAsButtonWidth = 72;
    constexpr auto chipModeWidth = 184;
    constexpr auto strictnessWidth = 108;
    constexpr auto playModeWidth = 122;
    constexpr auto presetMinWidth = 148;
    constexpr auto presetMaxWidth = 330;

    const auto fixedHeaderWidth = compactGap + favoriteButtonWidth
        + compactGap + loadButtonWidth
        + compactGap + saveButtonWidth
        + compactGap + saveAsButtonWidth
        + headerGap + chipModeWidth
        + headerGap + strictnessWidth
        + headerGap + playModeWidth;
    const auto presetWidth = std::clamp(top.getWidth() - fixedHeaderWidth, presetMinWidth, presetMaxWidth);

    {
        auto presetArea = top.removeFromLeft(presetWidth);
        controls.headerLabels[0].setBounds(presetArea.removeFromTop(16));
        auto presetRow = presetArea.reduced(0, 4);
        controls.presetFilter.setBounds({});
        controls.presetSearch.setBounds({});
        controls.browser.setBounds(presetRow.removeFromLeft(std::min(76, presetRow.getWidth())));
        presetRow.removeFromLeft(compactGap);
        controls.preset.setBounds(presetRow);
    }
    top.removeFromLeft(compactGap);
    controls.favorite.setBounds(top.removeFromLeft(favoriteButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(compactGap);
    controls.load.setBounds(top.removeFromLeft(loadButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(compactGap);
    controls.save.setBounds(top.removeFromLeft(saveButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(compactGap);
    controls.saveAs.setBounds(top.removeFromLeft(saveAsButtonWidth).withTrimmedTop(20).reduced(0, 4));
    top.removeFromLeft(headerGap);
    placeHeaderCombo(1, controls.chipMode, top.removeFromLeft(chipModeWidth));
    top.removeFromLeft(headerGap);
    placeHeaderCombo(2, controls.strictness, top.removeFromLeft(strictnessWidth));
    top.removeFromLeft(headerGap);
    placeHeaderCombo(4, controls.playMode, top.removeFromLeft(playModeWidth));

    area.removeFromTop(6);
    auto summaryRow = area.removeFromTop(28);
    workspaceBounds = summaryRow.removeFromRight(std::min(286, summaryRow.getWidth() / 3));
    summaryRow.removeFromRight(8);
    const auto workflowWidth = std::min(346, std::max(250, summaryRow.getWidth() / 2));
    controls.workflow.setBounds(summaryRow.removeFromRight(workflowWidth).reduced(0, 2));
    summaryRow.removeFromRight(8);
    controls.chipSummary.setBounds(summaryRow);

    auto workspaceRow = workspaceBounds;
    workspaceLabel.setBounds(workspaceRow.removeFromLeft(68));
    workspaceRow.removeFromLeft(6);
    constexpr auto workspaceGap = 4;
    const auto buttonWidth = std::max(48, (workspaceRow.getWidth() - (workspaceGap * 2)) / 3);
    for (auto& button : workspaceButtons)
    {
        button.setBounds(workspaceRow.removeFromLeft(std::min(buttonWidth, workspaceRow.getWidth())).reduced(0, 2));
        workspaceRow.removeFromLeft(std::min(workspaceGap, workspaceRow.getWidth()));
    }

    auto footer = getLocalBounds().reduced(16).removeFromBottom(44);
    controls.build.setBounds(footer.removeFromRight(190));
    footer.removeFromRight(headerGap);
    controls.midiCc.setBounds(footer.removeFromRight(136));
    footer.removeFromRight(headerGap);
    controls.status.setBounds(footer);
}

void ChipperEditorShell::attachExternalControlsTo(juce::Component& parent)
{
    const auto attach = [&parent](juce::Component& component)
    {
        parent.addChildComponent(&component);
    };

    attach(controls.title);
    for (auto& label : controls.headerLabels)
        attach(label);
    attach(controls.presetFilter);
    attach(controls.presetSearch);
    attach(controls.preset);
    attach(controls.browser);
    attach(controls.favorite);
    attach(controls.load);
    attach(controls.save);
    attach(controls.saveAs);
    attach(controls.chipMode);
    attach(controls.strictness);
    attach(controls.macro);
    attach(controls.playMode);
    attach(controls.workflow);
    attach(controls.chipSummary);
    attach(controls.status);
    attach(controls.midiCc);
    attach(controls.build);
}

bool ChipperEditorShell::isExternalControl(const juce::Component* component) const noexcept
{
    if (component == nullptr)
        return false;

    if (component == &controls.title
        || component == &controls.presetFilter
        || component == &controls.presetSearch
        || component == &controls.preset
        || component == &controls.browser
        || component == &controls.favorite
        || component == &controls.load
        || component == &controls.save
        || component == &controls.saveAs
        || component == &controls.chipMode
        || component == &controls.strictness
        || component == &controls.macro
        || component == &controls.playMode
        || component == &controls.workflow
        || component == &controls.chipSummary
        || component == &controls.status
        || component == &controls.midiCc
        || component == &controls.build)
        return true;

    return std::any_of(controls.headerLabels.begin(), controls.headerLabels.end(),
                       [component](const juce::Label& label) { return component == &label; });
}

void ChipperEditorShell::setWorkspace(ChipperEditorWorkspace workspaceToUse, juce::NotificationType notification)
{
    selectedWorkspace = workspaceToUse;
    for (size_t i = 0; i < workspaceButtons.size(); ++i)
        workspaceButtons[i].setToggleState(static_cast<size_t>(selectedWorkspace) == i, juce::dontSendNotification);

    if (notification != juce::dontSendNotification && onWorkspaceChanged)
        onWorkspaceChanged(selectedWorkspace);
}

void ChipperEditorShell::setTheme(juce::Colour primary,
                                  juce::Colour accent,
                                  juce::Colour outline,
                                  juce::Colour text,
                                  juce::Colour mutedText,
                                  juce::Colour darkText)
{
    workspaceLabel.setColour(juce::Label::textColourId, mutedText);
    for (auto& button : workspaceButtons)
    {
        button.setColour(juce::TextButton::buttonColourId, outline.darker(0.45f).interpolatedWith(accent, 0.08f));
        button.setColour(juce::TextButton::buttonOnColourId, primary);
        button.setColour(juce::TextButton::textColourOffId, text);
        button.setColour(juce::TextButton::textColourOnId, darkText);
    }
}

juce::Rectangle<int> ChipperEditorShell::workspaceButtonBoundsForTest(ChipperEditorWorkspace workspaceToFind) const
{
    const auto index = static_cast<size_t>(workspaceToFind);
    return index < workspaceButtons.size() ? workspaceButtons[index].getBounds() : juce::Rectangle<int> {};
}
