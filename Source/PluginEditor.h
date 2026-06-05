#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>

class ChipperAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit ChipperAudioProcessorEditor(ChipperAudioProcessor& processor);
    ~ChipperAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr size_t uiModuleCount = 6;
    static constexpr size_t uiModuleRows = 4;

    void timerCallback() override;
    void updateDescriptorText();
    void addLabeledSlider(juce::Slider& slider, juce::Label& label, const juce::String& fallbackText);
    void placeLabeledSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);

    ChipperAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label buildLabel;
    juce::Label chipSummaryLabel;
    juce::Label coreReadinessLabel;
    juce::Label globalStripLabel;
    juce::Label clockLabel;
    juce::Label outputLabel;
    std::array<juce::Label, uiModuleCount> moduleTitleLabels;
    std::array<juce::Label, uiModuleCount> moduleSummaryLabels;
    std::array<std::array<juce::Label, uiModuleRows>, uiModuleCount> moduleItemLabels;

    juce::ComboBox chipModeBox;
    juce::ComboBox accuracyBox;
    juce::ComboBox macroBox;
    juce::ComboBox playModeBox;

    juce::Slider clockSlider;
    juce::Slider outputSlider;
    std::array<juce::Slider, 4> nativeSliders;
    std::array<juce::Label, 4> nativeLabels;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboBoxAttachment> chipModeAttachment;
    std::unique_ptr<ComboBoxAttachment> accuracyAttachment;
    std::unique_ptr<ComboBoxAttachment> macroAttachment;
    std::unique_ptr<ComboBoxAttachment> playModeAttachment;
    std::unique_ptr<SliderAttachment> clockAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 4> nativeAttachments;

    chipper::ChipMode displayedMode = chipper::ChipMode::custom;
    bool descriptorTextInitialized = false;
    std::array<juce::Rectangle<int>, uiModuleCount> moduleBounds;
    juce::Rectangle<int> globalStripBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessorEditor)
};
