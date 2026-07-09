#pragma once

#include <JuceHeader.h>

#include "Engine/ChipDescriptors.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "UI/ChipUiModel.h"
#include "UI/ChipperEditorShell.h"
#include "UI/ChipperRelationshipMap.h"

#include <array>
#include <functional>
#include <memory>

struct ChipperWorkspaceTheme
{
    juce::Colour background;
    juce::Colour panel;
    juce::Colour sourceCard;
    juce::Colour outline;
    juce::Colour primary;
    juce::Colour accent;
    juce::Colour text;
    juce::Colour mutedText;
    juce::Colour darkText;
};

class ChipperPlayWorkspace final : public juce::Component
{
public:
    explicit ChipperPlayWorkspace(ChipperAudioProcessor& processor);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void refresh(chipper::ChipMode mode, const ChipperWorkspaceTheme& themeToUse);
    void focusInitialControl();
    std::function<void()> onOpenEditRequested;

    size_t visibleSourceCountForTest() const noexcept { return visibleSourceCount; }
    juce::Rectangle<int> sourceButtonBoundsForTest(size_t index) const;
    bool usesMasterDetailForTest() const noexcept { return usesMasterDetail; }
    size_t selectedSourceForTest() const noexcept { return selectedSourceIndex; }
    void selectSourceForTest(size_t index) { selectSource(index); }
    juce::Rectangle<int> sourceDetailBoundsForTest() const noexcept { return sourceDetailPanelBounds; }
    juce::Rectangle<int> sourceDetailLevelBoundsForTest() const noexcept { return detailLevelSlider.getBounds(); }
    juce::String sourceDetailTitleForTest() const { return detailTitleLabel.getText(); }
    juce::Rectangle<int> sourceDetailAssetBoundsForTest() const { return detailAssetBox.getBounds(); }
    juce::String sourceDetailAssetStatusForTest() const { return detailAssetStatusLabel.getText(); }
    juce::Rectangle<int> sourceDetailEditButtonBoundsForTest() const { return detailOpenEditButton.getBounds(); }
    void requestOpenEditForTest()
    {
        if (onOpenEditRequested)
            onOpenEditRequested();
    }
    juce::Rectangle<int> macroSliderBoundsForTest(size_t index) const;
    juce::Rectangle<int> relationshipBoundsForTest() const { return relationshipMap.getBounds(); }
    juce::String relationshipSummaryForTest() const { return relationshipMap.summaryForTest(); }
    juce::Rectangle<int> outputBoundsForTest() const noexcept { return outputSlider.getBounds(); }

private:
    static constexpr size_t sourceCount = 9;
    static constexpr size_t macroCount = 4;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void selectSource(size_t index);
    void bindSelectedSource();
    void updateSelectedAssetStatus(float plainValue);

    ChipperAudioProcessor& audioProcessor;
    ChipperWorkspaceTheme theme;
    chipper::ChipMode displayedMode = chipper::ChipMode::nes;
    size_t visibleSourceCount = 0;
    size_t selectedSourceIndex = 0;
    bool usesMasterDetail = false;
    bool usesAssetDetail = false;
    juce::Label titleLabel;
    juce::Label summaryLabel;
    juce::Label sourceSectionLabel;
    ChipperRelationshipMap relationshipMap;
    juce::Label macroSectionLabel;
    juce::Label outputSectionLabel;
    std::array<juce::TextButton, sourceCount> sourceButtons;
    std::array<juce::TextButton, sourceCount> sourceSelectButtons;
    std::array<juce::Label, sourceCount> sourceLevelLabels;
    std::array<juce::Slider, sourceCount> sourceLevelSliders;
    std::array<std::unique_ptr<ButtonAttachment>, sourceCount> sourceButtonAttachments;
    std::array<std::unique_ptr<SliderAttachment>, sourceCount> sourceLevelAttachments;
    juce::Label detailTitleLabel;
    juce::Label detailSummaryLabel;
    juce::ToggleButton detailEnableButton;
    juce::Label detailLevelLabel;
    juce::Slider detailLevelSlider;
    juce::Label detailAssetLabel;
    juce::ComboBox detailAssetBox;
    juce::Label detailAssetStatusLabel;
    juce::TextButton detailOpenEditButton;
    std::unique_ptr<ButtonAttachment> detailEnableAttachment;
    std::unique_ptr<SliderAttachment> detailLevelAttachment;
    std::unique_ptr<juce::ParameterAttachment> detailAssetAttachment;
    std::array<juce::Label, macroCount> macroLabels;
    std::array<juce::Slider, macroCount> macroSliders;
    std::array<std::unique_ptr<SliderAttachment>, macroCount> macroAttachments;
    juce::Slider outputSlider;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::array<juce::Rectangle<int>, sourceCount> sourceCardBounds;
    juce::Rectangle<int> sourcePanelBounds;
    juce::Rectangle<int> sourceDetailPanelBounds;
    juce::Rectangle<int> macroPanelBounds;
    juce::Rectangle<int> outputPanelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperPlayWorkspace)
};

class ChipperInspectWorkspace final : public juce::Component
{
public:
    ChipperInspectWorkspace();

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void refresh(chipper::ChipMode mode, const ChipperWorkspaceTheme& themeToUse);
    void focusInitialControl();

    juce::String verificationTextForTest() const { return verificationText.getText(); }
    juce::String gapsTextForTest() const { return gapsText.getText(); }

private:
    void configureHeading(juce::Label& label, const juce::String& text);
    void configureReadOnlyText(juce::TextEditor& editor);

    ChipperWorkspaceTheme theme;
    juce::Label titleLabel;
    juce::Label badgeLabel;
    juce::Label implementationHeading;
    juce::Label evidenceHeading;
    juce::Label gapsHeading;
    juce::Label controlsHeading;
    juce::TextEditor implementationText;
    juce::TextEditor verificationText;
    juce::TextEditor gapsText;
    juce::TextEditor controlsText;
    std::array<juce::Rectangle<int>, 4> panelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperInspectWorkspace)
};

class ChipperWorkspaceDeck final : public juce::Component
{
public:
    explicit ChipperWorkspaceDeck(ChipperAudioProcessor& processor);

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void setWorkspace(ChipperEditorWorkspace workspaceToUse);
    ChipperEditorWorkspace workspace() const noexcept { return selectedWorkspace; }
    void refresh(chipper::ChipMode mode, const ChipperWorkspaceTheme& themeToUse);
    void focusInitialControl();
    std::function<void()> onOpenEditRequested;

    ChipperPlayWorkspace& playWorkspaceForTest() noexcept { return playWorkspace; }
    const ChipperPlayWorkspace& playWorkspaceForTest() const noexcept { return playWorkspace; }
    ChipperInspectWorkspace& inspectWorkspaceForTest() noexcept { return inspectWorkspace; }
    const ChipperInspectWorkspace& inspectWorkspaceForTest() const noexcept { return inspectWorkspace; }

private:
    ChipperWorkspaceTheme theme;
    ChipperEditorWorkspace selectedWorkspace = ChipperEditorWorkspace::edit;
    ChipperPlayWorkspace playWorkspace;
    ChipperInspectWorkspace inspectWorkspace;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperWorkspaceDeck)
};
