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
    addAndMakeVisible(controls.favorite);
    addAndMakeVisible(controls.load);
    addAndMakeVisible(controls.save);
    addAndMakeVisible(controls.saveAs);
    addAndMakeVisible(controls.chipMode);
    addAndMakeVisible(controls.strictness);
    addAndMakeVisible(controls.macro);
    addAndMakeVisible(controls.playMode);
    addAndMakeVisible(controls.chipSummary);
    addAndMakeVisible(controls.status);
    addAndMakeVisible(controls.midiCc);
    addAndMakeVisible(controls.build);
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
        const auto filterWidth = std::clamp(presetRow.getWidth() / 4, 86, 98);
        const auto searchWidth = std::clamp(presetRow.getWidth() / 3, 86, 112);
        controls.presetFilter.setBounds(presetRow.removeFromLeft(filterWidth));
        presetRow.removeFromLeft(compactGap);
        controls.presetSearch.setBounds(presetRow.removeFromLeft(searchWidth));
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
    controls.chipSummary.setBounds(area.removeFromTop(28));

    auto footer = getLocalBounds().reduced(16).removeFromBottom(44);
    controls.build.setBounds(footer.removeFromRight(190));
    footer.removeFromRight(headerGap);
    controls.midiCc.setBounds(footer.removeFromRight(136));
    footer.removeFromRight(headerGap);
    controls.status.setBounds(footer);
}
