#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <cstdint>
#include <vector>

namespace chipper
{
struct PresetInfo;
}

class ChipWaveformPreview final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    ChipWaveformPreview() = default;

    void setSidWaveform(uint8_t bits, float pulseWidthRatio);
    void paint(juce::Graphics& g) override;

private:
    uint8_t waveformBits = 0;
    float pulseWidth = 0.5f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipWaveformPreview)
};

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
    static constexpr size_t pulse2DutyCount = 5;
    static constexpr size_t sidVoiceWaveCount = 3;
    static constexpr size_t dmgWaveLevelCount = 5;
    static constexpr size_t dmgStereoRouteCount = 5;
    static constexpr size_t ymEnvelopeShapeCount = 5;
    static constexpr size_t snNoiseModeCount = 5;
    static constexpr size_t toneNoiseMixCount = 3;
    static constexpr size_t ymChannelMixCount = 3;
    static constexpr size_t sidAdsrChoiceCount = 17;
    static constexpr size_t sidAdsrOverrideCount = 4;

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
    void placeSidAdsrControls(juce::Rectangle<int> bounds);
    void placePulseDutySegment(juce::Rectangle<int> bounds);
    void placePulse2DutySegment(juce::Rectangle<int> bounds);
    void placeWaveShapeSegment(juce::Rectangle<int> bounds);
    void placeSidVoiceWaveControls(juce::Rectangle<int> bounds);
    void placeDmgWaveLevelSegment(juce::Rectangle<int> bounds);
    void placeDmgStereoRouteSegment(juce::Rectangle<int> bounds);
    void placeYmEnvelopeShapeSegment(juce::Rectangle<int> bounds);
    void placeYmChannelMixControls(juce::Rectangle<int> bounds);
    void placeSnNoiseModeSegment(juce::Rectangle<int> bounds);
    void placeToneNoiseMixSegment(juce::Rectangle<int> bounds);
    float parameterValue(const char* parameterId) const;
    void setParameterValueFromUi(const char* parameterId, float plainValue);
    void setPlainParameterValueFromUi(const char* parameterId, float plainValue);
    void setChoiceParameterFromUi(const char* parameterId, int choiceIndex);
    void updateMacroChoices(chipper::ChipMode mode);
    void updatePresetChoices(chipper::ChipMode mode);
    void updateSegmentedControlSpecs(chipper::ChipMode mode);
    void applySelectedMacroTemplate();
    void applySelectedPreset();
    void applyFactoryPreset(const chipper::PresetInfo& preset);
    chipper::PatchConfig currentUiPatch(chipper::ChipMode mode,
                                         float control1,
                                         float control2,
                                         float control3,
                                         float control4,
                                         int waveShape,
                                         int dmgWaveLevel,
                                         int dmgStereoRoute,
                                         int ymEnvelopeShape,
                                         int snNoiseMode,
                                         float stereoSpread) const;
    bool usesPulseDutySegment(chipper::ChipMode mode) const;
    bool usesPulse2DutySegment(chipper::ChipMode mode) const;
    bool usesWaveShapeSegment(chipper::ChipMode mode) const;
    bool usesDmgWaveLevelSegment(chipper::ChipMode mode) const;
    bool usesDmgStereoRouteSegment(chipper::ChipMode mode) const;
    bool usesYmEnvelopeShapeSegment(chipper::ChipMode mode) const;
    bool usesYmChannelMixControls(chipper::ChipMode mode) const;
    bool usesSnNoiseModeSegment(chipper::ChipMode mode) const;
    bool usesToneNoiseMixSegment(chipper::ChipMode mode) const;
    juce::String macroTemplateReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const;
    juce::String pulseDutyReadout(chipper::ChipMode mode, float value) const;
    juce::String pulse2DutyReadout(const chipper::PatchConfig& patch) const;
    juce::String waveShapeReadout(chipper::ChipMode mode, int choice) const;
    juce::String dmgWaveLevelReadout(const chipper::PatchConfig& patch) const;
    juce::String dmgStereoRouteReadout(const chipper::PatchConfig& patch) const;
    juce::String sidModelReadout(const chipper::PatchConfig& patch) const;
    juce::String ymEnvelopeShapeReadout(int choice) const;
    juce::String sidFilterModeReadout(const chipper::PatchConfig& patch) const;
    juce::String sidVoiceWaveSummary(const chipper::PatchConfig& patch) const;
    juce::String sidModModeReadout(const chipper::PatchConfig& patch) const;
    juce::String noiseModeReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const;
    juce::String nesNoiseModeReadout(const chipper::PatchConfig& patch) const;
    juce::String dmgNoiseModeReadout(const chipper::PatchConfig& patch) const;
    juce::String snNoiseModeReadout(const chipper::PatchConfig& patch) const;
    juce::String snNoiseRegisterLabel(uint8_t noiseControl) const;
    juce::String nesSweepReadout(float value) const;
    juce::String nesNoiseReadout(float value) const;
    juce::String nesFocusReadout(float value) const;
    juce::String dmgSweepReadout(float value) const;
    juce::String dmgNoiseReadout(float value) const;
    juce::String dmgEnvelopeReadout(float value) const;
    juce::String ymSpreadReadout(float value) const;
    juce::String ymMotionReadout(float value) const;
    juce::String ymNoiseReadout(float value) const;
    juce::String ymToneNoiseReadout(float value) const;
    juce::String ymChannelMixReadout(const chipper::PatchConfig& patch) const;
    juce::String snStackReadout(float value) const;
    juce::String snMotionReadout(float value) const;
    juce::String snLevelReadout(float value) const;
    juce::String sidPulseWidthReadout(float value) const;
    juce::String sidDetuneReadout(float value) const;
    juce::String sidCutoffReadout(float value) const;
    juce::String sidSustainReadout(const chipper::PatchConfig& patch) const;
    juce::String sourceLevelReadout(size_t index) const;
    juce::String stereoSpreadReadout(chipper::ChipMode mode, float value) const;
    bool usesSourceChannelSurface(chipper::ChipMode mode) const;
    bool usesEnvelopeDecayControl(chipper::ChipMode mode) const;
    bool usesStereoSpreadControl(chipper::ChipMode mode) const;
    void setSourceChannelSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setStereoSpreadControlVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setPulse2DutySegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setWaveShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setSidVoiceWaveControlsVisible(bool shouldBeVisible);
    void setDmgWaveLevelSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setDmgStereoRouteSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setYmEnvelopeShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setYmChannelMixControlsVisible(bool shouldBeVisible);
    void setSnNoiseModeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setEnvelopeDecayControlVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setSidAdsrControlsVisible(bool shouldBeVisible);
    void updateSourceChannelButtons(chipper::ChipMode mode);
    void updateStereoSpreadReadout(chipper::ChipMode mode);
    void updatePulseDutyButtons(float value, bool shouldBeVisible);
    void updatePulse2DutyButtons(const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateWaveShapeButtons(int choice, bool shouldBeVisible);
    void updateSidVoiceWaveControls(bool shouldBeVisible);
    void updateDmgWaveLevelButtons(const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateDmgStereoRouteButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateYmEnvelopeShapeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateYmChannelMixControls(bool shouldBeVisible);
    void updateSnNoiseModeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateToneNoiseMixButtons(float value, bool shouldBeVisible);
    void updateEnvelopeDecayReadout(chipper::ChipMode mode);
    void updateSidAdsrControls(bool shouldBeVisible);
    juce::String envelopeDecayReadout(chipper::ChipMode mode, float value) const;

    ChipperAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label buildLabel;
    juce::Label midiCcLabel;
    juce::Label chipSummaryLabel;
    juce::Label macroSummaryLabel;
    juce::Label coreReadinessLabel;
    juce::Label globalStripLabel;
    juce::Label clockLabel;
    juce::Label outputLabel;
    juce::Label stereoSpreadLabel;
    juce::Label stereoSpreadValueLabel;
    juce::Label envelopeDecayLabel;
    juce::Label envelopeDecayValueLabel;
    std::array<juce::Label, sidAdsrOverrideCount> sidAdsrLabels;
    std::array<juce::ComboBox, sidAdsrOverrideCount> sidAdsrBoxes;
    juce::Label waveShapeLabel;
    juce::Label waveShapeValueLabel;
    juce::Label pulse2DutyLabel;
    juce::Label pulse2DutyValueLabel;
    std::array<juce::Label, sidVoiceWaveCount> sidVoiceWaveLabels;
    std::array<ChipWaveformPreview, sidVoiceWaveCount> sidVoicePreviewScopes;
    juce::Label dmgWaveLevelLabel;
    juce::Label dmgWaveLevelValueLabel;
    juce::Label dmgStereoRouteLabel;
    juce::Label dmgStereoRouteValueLabel;
    juce::Label ymEnvelopeShapeLabel;
    juce::Label ymEnvelopeShapeValueLabel;
    juce::Label ymChannelMixLabel;
    juce::Label ymChannelMixValueLabel;
    std::array<juce::Label, ymChannelMixCount> ymChannelMixLabels;
    std::array<juce::ComboBox, ymChannelMixCount> ymChannelMixBoxes;
    juce::Label snNoiseModeLabel;
    juce::Label snNoiseModeValueLabel;
    std::array<juce::Label, 5> headerControlLabels;
    std::array<juce::Label, uiModuleCount> moduleNumberLabels;
    std::array<juce::Label, uiModuleCount> moduleTitleLabels;
    std::array<juce::Label, uiModuleCount> moduleSummaryLabels;
    std::array<std::array<juce::Label, uiModuleRows>, uiModuleCount> moduleItemLabels;
    std::array<juce::TextButton, sourceChannelCount> sourceChannelButtons;
    std::array<juce::Label, liveControlCount> controlValueLabels;
    std::array<juce::TextButton, 4> pulseDutyButtons;
    std::array<juce::TextButton, pulse2DutyCount> pulse2DutyButtons;
    std::array<juce::TextButton, waveShapeCount> waveShapeButtons;
    std::array<juce::ComboBox, sidVoiceWaveCount> sidVoiceWaveBoxes;
    std::array<juce::TextButton, dmgWaveLevelCount> dmgWaveLevelButtons;
    std::array<juce::TextButton, dmgStereoRouteCount> dmgStereoRouteButtons;
    std::array<juce::TextButton, ymEnvelopeShapeCount> ymEnvelopeShapeButtons;
    std::array<juce::TextButton, snNoiseModeCount> snNoiseModeButtons;
    std::array<juce::TextButton, toneNoiseMixCount> toneNoiseMixButtons;
    std::array<juce::Slider, sourceChannelCount> sourceLevelSliders;
    std::array<juce::Label, sourceChannelCount> sourceLevelValueLabels;

    juce::ComboBox chipModeBox;
    juce::ComboBox accuracyBox;
    juce::ComboBox presetBox;
    juce::ComboBox macroBox;
    juce::ComboBox playModeBox;

    juce::Slider clockSlider;
    juce::Slider outputSlider;
    juce::Slider stereoSpreadSlider;
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
    std::unique_ptr<SliderAttachment> stereoSpreadAttachment;
    std::unique_ptr<SliderAttachment> envelopeDecayAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 4> nativeAttachments;
    std::array<std::unique_ptr<SliderAttachment>, sourceChannelCount> sourceLevelAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, sourceChannelCount> sourceEnableAttachments;

    chipper::ChipMode displayedMode = chipper::ChipMode::custom;
    std::vector<const chipper::PresetInfo*> displayedPresets;
    bool descriptorTextInitialized = false;
    bool suppressMacroTemplateApply = false;
    bool suppressPresetApply = false;
    std::array<juce::Rectangle<int>, uiModuleCount> moduleBounds;
    std::array<juce::Rectangle<int>, sourceChannelCount> sourceChannelBounds;
    juce::Rectangle<int> pulseDutySegmentBounds;
    juce::Rectangle<int> pulse2DutySegmentBounds;
    juce::Rectangle<int> waveShapeSegmentBounds;
    juce::Rectangle<int> dmgWaveLevelSegmentBounds;
    juce::Rectangle<int> dmgStereoRouteSegmentBounds;
    juce::Rectangle<int> ymEnvelopeShapeSegmentBounds;
    juce::Rectangle<int> snNoiseModeSegmentBounds;
    juce::Rectangle<int> toneNoiseMixSegmentBounds;
    juce::Rectangle<int> globalStripBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessorEditor)
};
