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
    static constexpr size_t sourceChannelCount = 4;
    static constexpr size_t liveControlCount = 6;
    static constexpr size_t waveShapeCount = 5;

    void timerCallback() override;
    void updateDescriptorText();
    void updateLiveControlReadouts();
    void addLabeledSlider(juce::Slider& slider, juce::Label& label, const juce::String& fallbackText);
    void placeLabeledSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void placeGroupedSlider(juce::Slider& slider,
                            juce::Label& groupLabel,
                            juce::Label& label,
                            juce::Label& valueLabel,
                            juce::Rectangle<int> bounds);
    void placeLabeledSliderWithReadout(juce::Slider& slider, juce::Label& label, juce::Label& valueLabel, juce::Rectangle<int> bounds);
    void placePulseDutySegment(juce::Rectangle<int> bounds);
    void placeWaveShapeSegment(juce::Rectangle<int> bounds);
    float parameterValue(const char* parameterId) const;
    void setParameterValueFromUi(const char* parameterId, float plainValue);
    void setChoiceParameterFromUi(const char* parameterId, int choiceIndex);
    bool usesPulseDutySegment(chipper::ChipMode mode) const;
    bool usesWaveShapeSegment(chipper::ChipMode mode) const;
    juce::String pulseDutyReadout(chipper::ChipMode mode, float value) const;
    juce::String waveShapeReadout(int choice) const;
    juce::String nesSweepReadout(float value) const;
    juce::String nesNoiseReadout(float value) const;
    juce::String nesFocusReadout(float value) const;
    juce::String dmgSweepReadout(float value) const;
    juce::String dmgNoiseReadout(float value) const;
    juce::String dmgEnvelopeReadout(float value) const;
    bool usesSourceChannelSurface(chipper::ChipMode mode) const;
    bool usesEnvelopeDecayControl(chipper::ChipMode mode) const;
    void setSourceChannelSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setWaveShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setEnvelopeDecayControlVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void updateSourceChannelButtons(chipper::ChipMode mode);
    void updatePulseDutyButtons(float value, bool shouldBeVisible);
    void updateWaveShapeButtons(int choice, bool shouldBeVisible);
    void updateEnvelopeDecayReadout(chipper::ChipMode mode);
    juce::String envelopeDecayReadout(chipper::ChipMode mode, float value) const;

    ChipperAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label buildLabel;
    juce::Label chipSummaryLabel;
    juce::Label coreReadinessLabel;
    juce::Label globalStripLabel;
    juce::Label clockLabel;
    juce::Label outputLabel;
    juce::Label envelopeDecayLabel;
    juce::Label envelopeDecayValueLabel;
    juce::Label waveShapeLabel;
    juce::Label waveShapeValueLabel;
    std::array<juce::Label, 4> headerControlLabels;
    std::array<juce::Label, uiModuleCount> moduleNumberLabels;
    std::array<juce::Label, uiModuleCount> moduleTitleLabels;
    std::array<juce::Label, uiModuleCount> moduleSummaryLabels;
    std::array<std::array<juce::Label, uiModuleRows>, uiModuleCount> moduleItemLabels;
    std::array<juce::TextButton, sourceChannelCount> sourceChannelButtons;
    std::array<juce::Label, liveControlCount> controlValueLabels;
    std::array<juce::TextButton, 4> pulseDutyButtons;
    std::array<juce::TextButton, waveShapeCount> waveShapeButtons;

    juce::ComboBox chipModeBox;
    juce::ComboBox accuracyBox;
    juce::ComboBox macroBox;
    juce::ComboBox playModeBox;

    juce::Slider clockSlider;
    juce::Slider outputSlider;
    juce::Slider envelopeDecaySlider;
    std::array<juce::Slider, 4> nativeSliders;
    std::array<juce::Label, 4> nativeGroupLabels;
    std::array<juce::Label, 4> nativeLabels;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> chipModeAttachment;
    std::unique_ptr<ComboBoxAttachment> accuracyAttachment;
    std::unique_ptr<ComboBoxAttachment> macroAttachment;
    std::unique_ptr<ComboBoxAttachment> playModeAttachment;
    std::unique_ptr<SliderAttachment> clockAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<SliderAttachment> envelopeDecayAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 4> nativeAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, sourceChannelCount> sourceEnableAttachments;

    chipper::ChipMode displayedMode = chipper::ChipMode::custom;
    bool descriptorTextInitialized = false;
    std::array<juce::Rectangle<int>, uiModuleCount> moduleBounds;
    std::array<juce::Rectangle<int>, sourceChannelCount> sourceChannelBounds;
    juce::Rectangle<int> pulseDutySegmentBounds;
    juce::Rectangle<int> waveShapeSegmentBounds;
    juce::Rectangle<int> globalStripBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessorEditor)
};
