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
    void timerCallback() override;
    void updateDescriptorText();
    void addLabeledSlider(juce::Slider& slider, juce::Label& label, const juce::String& fallbackText);

    ChipperAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label chipSummaryLabel;
    juce::Label clockLabel;
    juce::Label outputLabel;

    juce::ComboBox chipModeBox;
    juce::ComboBox accuracyBox;
    juce::ComboBox macroBox;

    juce::Slider clockSlider;
    juce::Slider outputSlider;
    std::array<juce::Slider, 4> nativeSliders;
    std::array<juce::Label, 4> nativeLabels;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboBoxAttachment> chipModeAttachment;
    std::unique_ptr<ComboBoxAttachment> accuracyAttachment;
    std::unique_ptr<ComboBoxAttachment> macroAttachment;
    std::unique_ptr<SliderAttachment> clockAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 4> nativeAttachments;

    chipper::ChipMode displayedMode = chipper::ChipMode::custom;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessorEditor)
};
