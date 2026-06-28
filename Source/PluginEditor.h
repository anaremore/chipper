#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace chipper
{
struct PresetInfo;
}

enum class ChipWaveformPreviewShape
{
    off,
    pulse,
    triangle,
    saw,
    noise,
    stepped,
    combined,
    toneNoise
};

class ChipWaveformPreview final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    ChipWaveformPreview() = default;

    void setShape(ChipWaveformPreviewShape newShape, float pulseWidthRatio = 0.5f, bool shouldBeActive = true);
    void setSidWaveform(uint8_t bits, float pulseWidthRatio, bool shouldBeActive = true);
    void paint(juce::Graphics& g) override;

private:
    ChipWaveformPreviewShape shape = ChipWaveformPreviewShape::off;
    float pulseWidth = 0.5f;
    bool active = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipWaveformPreview)
};

class ChipEnvelopePreview final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    ChipEnvelopePreview() = default;

    void setSidAdsr(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release);
    void setYmEnvelope(uint8_t shapeCode, bool envelopeEnabled);
    void paint(juce::Graphics& g) override;

private:
    enum class Mode
    {
        sidAdsr,
        ymEnvelope
    };

    Mode mode = Mode::sidAdsr;
    uint8_t attackNibble = 0;
    uint8_t decayNibble = 0;
    uint8_t sustainNibble = 0;
    uint8_t releaseNibble = 0;
    uint8_t ymShapeCode = 0x09u;
    bool ymEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipEnvelopePreview)
};

class FmAlgorithmPreview final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    FmAlgorithmPreview() = default;

    void setAlgorithm(int newAlgorithm, bool shouldFollow);
    void paint(juce::Graphics& g) override;

private:
    int algorithm = 0;
    bool follow = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FmAlgorithmPreview)
};

class OplWaveformPreview final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    OplWaveformPreview() = default;

    void setWaveform(int newWaveform, bool shouldFollow);
    void paint(juce::Graphics& g) override;

private:
    int waveform = 0;
    bool follow = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OplWaveformPreview)
};

class OutputScopePreview final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    OutputScopePreview() = default;

    void setSamples(const ChipperAudioProcessor::OutputScopeSnapshot& newSamples);
    void paint(juce::Graphics& g) override;

private:
    ChipperAudioProcessor::OutputScopeSnapshot samples {};
    float peak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputScopePreview)
};

class SampleWaveformPreview final : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    SampleWaveformPreview() = default;

    void setSnapshot(const ChipperAudioProcessor::SampleWaveformSnapshot& newSnapshot);
    void paint(juce::Graphics& g) override;

private:
    ChipperAudioProcessor::SampleWaveformSnapshot snapshot {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleWaveformPreview)
};

class ChipperAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit ChipperAudioProcessorEditor(ChipperAudioProcessor& processor);
    ~ChipperAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    juce::Rectangle<int> getSourceChannelBoundsForLayoutTest(size_t channel) const
    {
        return channel < sourceChannelBounds.size() ? sourceChannelBounds[channel] : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getSourceLevelBoundsForLayoutTest(size_t channel) const
    {
        return channel < sourceLevelSliders.size() ? sourceLevelSliders[channel].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getSourceWaveSelectorBoundsForLayoutTest(size_t channel) const
    {
        return channel < hucVoiceWaveBoxes.size() ? hucVoiceWaveBoxes[channel].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getPresetFilterBoundsForLayoutTest() const { return presetFilterBox.getBounds(); }
    juce::String getPresetFilterTextForLayoutTest() const { return presetFilterBox.getText(); }
    juce::Rectangle<int> getPresetSearchBoundsForLayoutTest() const { return presetSearchBox.getBounds(); }
    juce::String getPresetSearchTextForLayoutTest() const { return presetSearchBox.getText(); }
    bool selectPresetFilterForLayoutTest(const juce::String& kind, const juce::String& value);
    void setPresetSearchTextForLayoutTest(const juce::String& text)
    {
        presetSearchBox.setText(text, false);
        updatePresetChoices(displayedMode);
    }
    int getDisplayedFactoryPresetCountForLayoutTest() const { return static_cast<int>(displayedPresets.size()); }
    juce::String getFirstDisplayedFactoryPresetNameForLayoutTest() const;
    bool userPresetMetadataMatchesFilterForLayoutTest(const juce::String& kind,
                                                      const juce::String& value,
                                                      const juce::String& role,
                                                      const juce::String& engine,
                                                      const juce::String& tags);
    juce::Rectangle<int> getPaulaSourceSampleSelectorBoundsForLayoutTest(size_t channel) const
    {
        return channel < paulaVoiceSampleBoxes.size() ? paulaVoiceSampleBoxes[channel].getBounds() : juce::Rectangle<int> {};
    }

    juce::Rectangle<int> getPulseDutyBoundsForLayoutTest() const { return pulseDutySegmentBounds; }
    juce::Rectangle<int> getPulse2DutyBoundsForLayoutTest() const { return pulse2DutySegmentBounds; }
    juce::Rectangle<int> getDmgWaveLevelBoundsForLayoutTest() const { return dmgWaveLevelSegmentBounds; }
    juce::Rectangle<int> getDmgStereoRouteBoundsForLayoutTest() const
    {
        return ! dmgStereoRouteSegmentBounds.isEmpty() ? dmgStereoRouteSegmentBounds : dmgStereoRouteBox.getBounds();
    }
    juce::Rectangle<int> getSnNoiseModeBoundsForLayoutTest() const { return snNoiseModeSegmentBounds; }
    juce::Rectangle<int> getSnNoiseModeMenuBoundsForLayoutTest() const { return snNoiseModeBox.getBounds(); }
    juce::Rectangle<int> getYmChannelMixBoundsForLayoutTest(size_t channel) const
    {
        return channel < ymChannelMixBoxes.size() ? ymChannelMixBoxes[channel].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getToneNoiseMixBoundsForLayoutTest() const { return toneNoiseMixSegmentBounds; }
    juce::Rectangle<int> getYmEnvelopeShapeBoundsForLayoutTest() const
    {
        return ! ymEnvelopeShapeSegmentBounds.isEmpty() ? ymEnvelopeShapeSegmentBounds : sidFilterModeBox.getBounds();
    }
    juce::Rectangle<int> getYmEnvelopePreviewBoundsForLayoutTest() const { return ymEnvelopePreview.getBounds(); }
    juce::Rectangle<int> getFmOperatorNameBoundsForLayoutTest(size_t index) const
    {
        return index < fmOperatorNameLabels.size() ? fmOperatorNameLabels[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getFmOperatorValueBoundsForLayoutTest(size_t index) const
    {
        return index < fmOperatorValueLabels.size() ? fmOperatorValueLabels[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::String getFmOperatorNameTextForLayoutTest(size_t index) const
    {
        return index < fmOperatorNameLabels.size() ? fmOperatorNameLabels[index].getText() : juce::String {};
    }
    juce::String getFmOperatorValueTextForLayoutTest(size_t index) const
    {
        return index < fmOperatorValueLabels.size() ? fmOperatorValueLabels[index].getText() : juce::String {};
    }
    juce::Rectangle<int> getModuleBoundsForLayoutTest(size_t module) const
    {
        return module < moduleBounds.size() ? moduleBounds[module] : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getPerformanceBoundsForLayoutTest() const { return globalStripBounds; }
    juce::Rectangle<int> getSampleBankBoundsForLayoutTest() const { return moduleBounds[5]; }
    juce::Rectangle<int> getSampleFileButtonBoundsForLayoutTest() const { return dmcSampleFileButton.getBounds(); }
    juce::Rectangle<int> getSampleFolderButtonBoundsForLayoutTest() const { return dmcSampleFolderButton.getBounds(); }
    juce::Rectangle<int> getSampleBankButtonBoundsForLayoutTest() const { return dmcSampleBankButton.getBounds(); }
    juce::Rectangle<int> getSamplePlaybackModeBoundsForLayoutTest() const { return dmcPlaybackModeBox.getBounds(); }
    juce::Rectangle<int> getSampleSlotBoundsForLayoutTest() const { return dmcSampleSlotBox.getBounds(); }
    juce::Rectangle<int> getSampleRootBoundsForLayoutTest() const { return dmcMapRootBox.getBounds(); }
    juce::Rectangle<int> getSampleLoopToggleBoundsForLayoutTest() const { return spc700LoopModeButton.getBounds(); }
    juce::Rectangle<int> getDmcLoopToggleBoundsForLayoutTest() const { return dmcLoopButton.getBounds(); }
    juce::Rectangle<int> getDmcRateBoundsForLayoutTest() const { return dmcRateBox.getBounds(); }
    juce::Rectangle<int> getSampleWaveformBoundsForLayoutTest() const { return sampleWaveformPreview.getBounds(); }
    juce::Rectangle<int> getEnvelopeDecayBoundsForLayoutTest() const { return envelopeDecaySlider.getBounds(); }
    juce::Rectangle<int> getStereoSpreadBoundsForLayoutTest() const { return stereoSpreadSlider.getBounds(); }
    juce::Rectangle<int> getOutputSliderBoundsForLayoutTest() const { return outputSlider.getBounds(); }
    juce::Rectangle<int> getOutputScopeBoundsForLayoutTest() const { return outputScopePreview.getBounds(); }
    juce::Rectangle<int> getNativeSliderBoundsForLayoutTest(size_t index) const
    {
        return index < nativeSliders.size() ? nativeSliders[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getNativeGroupLabelBoundsForLayoutTest(size_t index) const
    {
        return index < nativeGroupLabels.size() ? nativeGroupLabels[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getNativeLabelBoundsForLayoutTest(size_t index) const
    {
        return index < nativeLabels.size() ? nativeLabels[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getNativeValueLabelBoundsForLayoutTest(size_t index) const
    {
        return index < controlValueLabels.size() ? controlValueLabels[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getSidAdsrContentBoundsForLayoutTest() const;
    juce::Rectangle<int> getSidAdsrSliderBoundsForLayoutTest(size_t index) const
    {
        return index < sidAdsrSliders.size() ? sidAdsrSliders[index].getBounds() : juce::Rectangle<int> {};
    }
    juce::Rectangle<int> getSidEnvelopePreviewBoundsForLayoutTest(size_t voice) const
    {
        return voice < sidEnvelopePreviews.size() ? sidEnvelopePreviews[voice].getBounds() : juce::Rectangle<int> {};
    }
    void runEditorUpdateForLayoutTest() { timerCallback(); }

private:
    static constexpr size_t uiModuleCount = 6;
    static constexpr size_t uiModuleRows = 4;
    static constexpr size_t sourceChannelCount = 9;
    static constexpr size_t liveControlCount = 6;
    static constexpr size_t waveShapeCount = 5;
    static constexpr size_t pulse2DutyCount = 5;
    static constexpr size_t sidVoiceWaveCount = 3;
    static constexpr size_t hucVoiceWaveCount = 8;
    static constexpr size_t dmgWaveLevelCount = 5;
    static constexpr size_t dmgStereoRouteCount = 5;
    static constexpr size_t ymEnvelopeShapeCount = 21;
    static constexpr size_t snNoiseModeCount = 5;
    static constexpr size_t toneNoiseMixCount = 3;
    static constexpr size_t ymChannelMixCount = 3;
    static constexpr size_t sidAdsrChoiceCount = 17;
    static constexpr size_t sidAdsrFieldCount = 4;
    static constexpr size_t sidAdsrVoiceCount = 3;
    static constexpr size_t sidAdsrOverrideCount = sidAdsrFieldCount * sidAdsrVoiceCount;
    static constexpr size_t fmOperatorReadoutRows = 4;
    static constexpr size_t sidFilterRoutingChoiceCount = 9;

    struct UserPresetFile;

    void timerCallback() override;
    void updateDescriptorText();
    void updateLiveControlReadouts();
    void applyChipTheme();
    void addLabeledSlider(juce::Slider& slider, juce::Label& label, const juce::String& fallbackText);
    void placeLabeledSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);
    void placeGroupedSlider(juce::Slider& slider,
                            juce::Label& groupLabel,
                            juce::Label& label,
                            juce::Label& valueLabel,
                            juce::Rectangle<int> bounds);
    void placeLabeledSliderWithReadout(juce::Slider& slider, juce::Label& label, juce::Label& valueLabel, juce::Rectangle<int> bounds);
    void placeSidAdsrControls(juce::Rectangle<int> bounds);
    void placeCompactFmOperatorSlider(juce::Slider& slider, juce::Label& label, juce::Label& valueLabel, juce::Rectangle<int> bounds);
    void placeFmOperatorEditSurface(chipper::ChipMode mode, juce::Rectangle<int> bounds);
    void placeFmOperatorRegisterSurface(chipper::ChipMode mode, juce::Rectangle<int> bounds);
    void placePulseDutySegment(juce::Rectangle<int> bounds);
    void placePulse2DutySegment(juce::Rectangle<int> bounds);
    void placeWaveShapeSegment(juce::Rectangle<int> bounds);
    void placeFmAlgorithmControl(juce::Rectangle<int> bounds);
    void placeOplWaveformControl(juce::Rectangle<int> bounds);
    void placeOpllInstrumentControl(juce::Rectangle<int> bounds);
    void placeSidVoiceWaveControls(juce::Rectangle<int> bounds);
    void placeHucVoiceWaveControls(juce::Rectangle<int> bounds);
    void placeDmgWaveLevelSegment(juce::Rectangle<int> bounds);
    void placeDmgStereoRouteSegment(juce::Rectangle<int> bounds);
    void placeYmEnvelopeShapeSegment(juce::Rectangle<int> bounds);
    void placeSidFilterRoutingControl(juce::Rectangle<int> bounds);
    void placeYmChannelMixControls(juce::Rectangle<int> bounds);
    void placeSnNoiseModeSegment(juce::Rectangle<int> bounds);
    void placeToneNoiseMixSegment(juce::Rectangle<int> bounds);
    float parameterValue(const char* parameterId) const;
    void setParameterValueFromUi(const char* parameterId, float plainValue);
    void setPlainParameterValueFromUi(const char* parameterId, float plainValue);
    void setChoiceParameterFromUi(const char* parameterId, int choiceIndex);
    void updateMacroChoices(chipper::ChipMode mode);
    void updatePresetChoices(chipper::ChipMode mode);
    void updatePresetFilterChoices(chipper::ChipMode mode);
    enum class PresetFilterKind
    {
        all,
        role,
        engine,
        tag
    };
    struct PresetFilterChoice
    {
        PresetFilterKind kind = PresetFilterKind::all;
        juce::String value;
    };
    PresetFilterChoice activePresetFilterChoice() const;
    juce::String activePresetFilterDescription() const;
    juce::String activePresetSearchText() const;
    bool factoryPresetMatchesActiveFilter(const chipper::PresetInfo& preset) const;
    bool factoryPresetMatchesActiveSearch(const chipper::PresetInfo& preset) const;
    bool userPresetMatchesActiveFilter(const UserPresetFile& preset) const;
    bool userPresetMatchesActiveSearch(const UserPresetFile& preset) const;
    void refreshPresetBrowserReadout(chipper::ChipMode mode);
    void reloadUserPresetFiles(chipper::ChipMode mode);
    void updateSegmentedControlSpecs(chipper::ChipMode mode);
    void storeChipSettingsSnapshot(chipper::ChipMode mode);
    bool restoreChipSettingsSnapshot(chipper::ChipMode mode);
    void chooseUserPresetToLoad();
    void chooseUserPresetToSave();
    void chooseUserPresetToSaveAs();
    void loadUserPresetFile(const juce::File& file);
    void saveUserPresetFile(const juce::File& file);
    void applySelectedMacroTemplate();
    void applySelectedPreset();
    void applyInitPreset();
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
    juce::String performanceMacroDestination(chipper::ChipMode mode, size_t index) const;
    juce::String performanceMacroReadout(chipper::ChipMode mode, size_t index, juce::String readout) const;
    juce::String sourceLaneExposureReadout(chipper::ChipMode mode) const;
    juce::String pulseDutyReadout(chipper::ChipMode mode, float value) const;
    juce::String pulse2DutyReadout(const chipper::PatchConfig& patch) const;
    juce::String compactPulse2DutyReadout(const chipper::PatchConfig& patch) const;
    juce::String waveShapeReadout(chipper::ChipMode mode, int choice) const;
    int pokeyCardMidiNote(const chipper::PatchConfig& patch, size_t index) const;
    juce::String pokeySourceRegisterReadout(const chipper::PatchConfig& patch, size_t index) const;
    juce::String pokeySourceLevelReadout(const chipper::PatchConfig& patch, size_t index) const;
    juce::String pokeyRegisterReadout(const chipper::PatchConfig& patch) const;
    juce::String pokeyAudctlReadout(const chipper::PatchConfig& patch) const;
    juce::String pokeyAudctlFilterReadout(const chipper::PatchConfig& patch) const;
    juce::String sampleChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const;
    juce::String spc700PitchMotionReadout(const chipper::PatchConfig& patch) const;
    juce::String spc700EnvelopeReadout(const chipper::PatchConfig& patch) const;
    juce::String spc700EchoReadout(const chipper::PatchConfig& patch) const;
    juce::String paulaOutputFilterReadout(const chipper::PatchConfig& patch) const;
    juce::String sampleSourceCardLabel(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index) const;
    juce::String sampleSourceRegisterReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index) const;
    juce::String wavetableChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const;
    juce::String wavetableSourceCardLabel(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index) const;
    juce::String wavetableSourceRegisterReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index) const;
    juce::String fmChipReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const;
    juce::String fmFeedbackReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const;
    juce::String fmOperatorRegisterReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t op) const;
    juce::String fmOperatorRegisterTooltip(chipper::ChipMode mode, size_t op) const;
    juce::String ym2612DacModeReadout(const chipper::PatchConfig& patch) const;
    juce::String fmSourceRegisterReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index) const;
    juce::String sourceCardNativeLabel(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index, juce::String fallback) const;
    juce::String dmgWaveLevelReadout(const chipper::PatchConfig& patch) const;
    juce::String dmgStereoRouteReadout(const chipper::PatchConfig& patch) const;
    juce::String spc700SamplePlaybackReadout(const chipper::PatchConfig& patch) const;
    juce::String spc700NoiseReadout(const chipper::PatchConfig& patch) const;
    juce::String ym2151NoiseReadout(const chipper::PatchConfig& patch) const;
    juce::String ym2612PanReadout(const chipper::PatchConfig& patch) const;
    juce::String ym2151PanReadout(const chipper::PatchConfig& patch) const;
    juce::String sidModelReadout(const chipper::PatchConfig& patch) const;
    juce::String ymEnvelopeShapeReadout(int choice) const;
    juce::String sidFilterModeReadout(const chipper::PatchConfig& patch) const;
    juce::String sidFilterRoutingReadout(const chipper::PatchConfig& patch) const;
    juce::String sidVoiceWaveSummary(const chipper::PatchConfig& patch) const;
    juce::String sidVoicePulseWidthReadout(const chipper::PatchConfig& patch, size_t voice) const;
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
    juce::String snSourceCardLabel(const chipper::PatchConfig& patch, size_t index) const;
    juce::String snSourceCardTooltip(const chipper::PatchConfig& patch, size_t index, const chipper::ChipParameterSpec* spec) const;
    juce::String nesDmcDirectReadout(float value) const;
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
    void setHucVoiceWaveControlsVisible(bool shouldBeVisible);
    void setSidVoicePulseWidthControlsVisible(bool shouldBeVisible);
    void setDmgWaveLevelSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setDmgStereoRouteSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setYmEnvelopeShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setSidFilterRoutingControlVisible(bool shouldBeVisible);
    void setYmChannelMixControlsVisible(bool shouldBeVisible);
    void setSnNoiseModeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setEnvelopeDecayControlVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void setSidAdsrControlsVisible(bool shouldBeVisible);
    void setFmOperatorRegisterSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible);
    void updateSourceChannelButtons(chipper::ChipMode mode);
    void updateSourcePreviewScope(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t index, bool shouldBeVisible);
    void updateStereoSpreadReadout(chipper::ChipMode mode);
    void updatePulseDutyButtons(float value, bool shouldBeVisible);
    void updatePulse2DutyButtons(const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateWaveShapeButtons(int choice, bool shouldBeVisible);
    void updateFmAlgorithmControl(chipper::ChipMode mode, int choice, bool shouldBeVisible);
    void updateOplWaveformControl(chipper::ChipMode mode, int choice, bool shouldBeVisible);
    void updateOpllInstrumentControl(chipper::ChipMode mode, int choice, bool shouldBeVisible);
    void updateSidVoiceWaveControls(bool shouldBeVisible);
    void updateHucVoiceWaveControls(bool shouldBeVisible);
    void updateSidVoicePulseWidthControls(const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateDmgWaveLevelButtons(const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateDmgStereoRouteButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateYmEnvelopeShapeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateYmEnvelopePreview(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateSidFilterRoutingControl(bool shouldBeVisible);
    void updateYmChannelMixControls(bool shouldBeVisible);
    void updateSnNoiseModeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateToneNoiseMixButtons(float value, bool shouldBeVisible);
    void updateEnvelopeDecayReadout(chipper::ChipMode mode);
    void updateSidAdsrControls(bool shouldBeVisible);
    void updateFmOperatorRegisterSurface(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible);
    void updateDmcSampleControls();
    void updateSpc700BrrSampleControls();
    void updatePaulaSampleControls();
    void updateSampleWaveformPreview(chipper::ChipMode mode);
    void updateSamplePlaybackModeChoices(chipper::ChipMode mode);
    void chooseDmcSampleFile();
    void chooseDmcSampleDirectory();
    void chooseSpc700BrrSampleFile();
    void chooseSpc700BrrSampleDirectory();
    void choosePaulaSampleFile();
    void choosePaulaSampleDirectory();
    void showDmcSampleBankEditor();
    void handleDmcSampleLoadResult(const juce::Result& result);
    juce::String envelopeDecayReadout(chipper::ChipMode mode, float value) const;

    ChipperAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label buildLabel;
    juce::Label midiCcLabel;
    juce::Label chipSummaryLabel;
    juce::Label macroSummaryLabel;
    juce::Label globalStripLabel;
    juce::Label clockLabel;
    juce::Label dmcDirectLabel;
    juce::Label dmcRateLabel;
    juce::Label dmcSampleLabel;
    juce::Label dmcSampleStatusLabel;
    juce::Label sampleLoopStartLabel;
    juce::Label sampleLoopEndLabel;
    juce::Label sampleLoopStartValueLabel;
    juce::Label sampleLoopEndValueLabel;
    SampleWaveformPreview sampleWaveformPreview;
    juce::Label dmcPlaybackModeLabel;
    juce::Label dmcMapRootLabel;
    juce::Label outputLabel;
    OutputScopePreview outputScopePreview;
    juce::Label stereoSpreadLabel;
    juce::Label stereoSpreadValueLabel;
    juce::Label envelopeDecayLabel;
    juce::Label envelopeDecayValueLabel;
    std::array<juce::Label, sidAdsrVoiceCount> sidEnvelopeVoiceLabels;
    std::array<ChipEnvelopePreview, sidAdsrVoiceCount> sidEnvelopePreviews;
    ChipEnvelopePreview ymEnvelopePreview;
    std::array<juce::Label, sidAdsrFieldCount> sidAdsrHeaderLabels;
    std::array<juce::Label, sidAdsrOverrideCount> sidAdsrLabels;
    std::array<juce::Label, sidAdsrOverrideCount> sidAdsrValueLabels;
    std::array<juce::ComboBox, sidAdsrOverrideCount> sidAdsrBoxes;
    std::array<juce::Slider, sidAdsrOverrideCount> sidAdsrSliders;
    std::array<juce::Label, fmOperatorReadoutRows> fmOperatorNameLabels;
    std::array<juce::Label, fmOperatorReadoutRows> fmOperatorValueLabels;
    juce::Label waveShapeLabel;
    juce::Label waveShapeValueLabel;
    juce::Label pulse2DutyLabel;
    juce::Label pulse2DutyValueLabel;
    std::array<juce::Label, sidVoiceWaveCount> sidVoiceWaveLabels;
    std::array<juce::Label, sidVoiceWaveCount> sidVoicePulseWidthLabels;
    std::array<juce::Label, sidVoiceWaveCount> sidVoicePulseWidthValueLabels;
    std::array<juce::Label, hucVoiceWaveCount> hucVoiceWaveLabels;
    std::array<juce::Label, 4> paulaVoiceSampleLabels;
    std::array<juce::ComboBox, 4> paulaVoiceSampleBoxes;
    std::array<ChipWaveformPreview, sourceChannelCount> sourcePreviewScopes;
    juce::Label dmgWaveLevelLabel;
    juce::Label dmgWaveLevelValueLabel;
    juce::Label dmgStereoRouteLabel;
    juce::Label dmgStereoRouteValueLabel;
    juce::Label ymEnvelopeShapeLabel;
    juce::Label ymEnvelopeShapeValueLabel;
    juce::Label sidFilterRoutingLabel;
    juce::Label sidFilterRoutingValueLabel;
    juce::Label ymChannelMixLabel;
    juce::Label ymChannelMixValueLabel;
    std::array<juce::Label, ymChannelMixCount> ymChannelMixLabels;
    std::array<juce::ComboBox, ymChannelMixCount> ymChannelMixBoxes;
    juce::Label snNoiseModeLabel;
    juce::Label snNoiseModeValueLabel;
    std::array<juce::Label, 5> headerControlLabels;
    std::array<juce::Label, uiModuleCount> moduleTitleLabels;
    std::array<juce::Label, uiModuleCount> moduleSummaryLabels;
    std::array<std::array<juce::Label, uiModuleRows>, uiModuleCount> moduleItemLabels;
    std::array<juce::TextButton, sourceChannelCount> sourceChannelButtons;
    std::array<juce::Label, liveControlCount> controlValueLabels;
    std::array<juce::TextButton, 4> pulseDutyButtons;
    std::array<juce::TextButton, pulse2DutyCount> pulse2DutyButtons;
    std::array<juce::TextButton, waveShapeCount> waveShapeButtons;
    juce::ComboBox fmAlgorithmBox;
    FmAlgorithmPreview fmAlgorithmPreview;
    juce::ComboBox oplWaveformBox;
    OplWaveformPreview oplWaveformPreview;
    juce::ComboBox opllInstrumentBox;
    std::array<juce::ComboBox, sidVoiceWaveCount> sidVoiceWaveBoxes;
    std::array<juce::ComboBox, hucVoiceWaveCount> hucVoiceWaveBoxes;
    std::array<juce::Slider, sidVoiceWaveCount> sidVoicePulseWidthSliders;
    std::array<juce::TextButton, dmgWaveLevelCount> dmgWaveLevelButtons;
    std::array<juce::TextButton, dmgStereoRouteCount> dmgStereoRouteButtons;
    juce::ComboBox dmgStereoRouteBox;
    std::array<juce::TextButton, ymEnvelopeShapeCount> ymEnvelopeShapeButtons;
    juce::ComboBox sidFilterModeBox;
    juce::ComboBox sidFilterRoutingBox;
    std::array<juce::TextButton, snNoiseModeCount> snNoiseModeButtons;
    juce::ComboBox snNoiseModeBox;
    std::array<juce::TextButton, toneNoiseMixCount> toneNoiseMixButtons;
    std::array<juce::Slider, sourceChannelCount> sourceLevelSliders;
    std::array<juce::Label, sourceChannelCount> sourceLevelLabels;
    std::array<juce::Label, sourceChannelCount> sourceLevelValueLabels;

    juce::ComboBox chipModeBox;
    juce::ComboBox accuracyBox;
    juce::ComboBox presetFilterBox;
    juce::TextEditor presetSearchBox;
    juce::ComboBox presetBox;
    juce::TextButton userPresetLoadButton;
    juce::TextButton userPresetSaveButton;
    juce::TextButton userPresetSaveAsButton;
    juce::ComboBox macroBox;
    juce::ComboBox playModeBox;

    struct UserPresetFile
    {
        juce::File file;
        juce::String name;
        juce::String role;
        juce::String engine;
        juce::StringArray tags;
        juce::String note;
        juce::String source;
    };

    struct ChipSettingsSnapshot
    {
        bool valid = false;
        std::vector<float> parameterValues;
        int presetSelectedId = 0;
        juce::File userPresetFile;
    };

    juce::Slider clockSlider;
    juce::Slider dmcDirectSlider;
    juce::ComboBox dmcRateBox;
    juce::TextButton dmcSampleFileButton;
    juce::TextButton dmcSampleFolderButton;
    juce::TextButton dmcSampleBankButton;
    juce::ComboBox dmcSampleSlotBox;
    juce::ComboBox dmcPlaybackModeBox;
    juce::ComboBox dmcMapRootBox;
    juce::ToggleButton dmcLoopButton;
    juce::ToggleButton spc700LoopModeButton;
    juce::Slider sampleLoopStartSlider;
    juce::Slider sampleLoopEndSlider;
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
    std::unique_ptr<SliderAttachment> dmcDirectAttachment;
    std::unique_ptr<ComboBoxAttachment> dmcRateAttachment;
    std::unique_ptr<ComboBoxAttachment> dmcPlaybackModeAttachment;
    std::unique_ptr<ComboBoxAttachment> dmcMapRootAttachment;
    std::unique_ptr<ButtonAttachment> dmcLoopAttachment;
    std::unique_ptr<SliderAttachment> sampleLoopStartAttachment;
    std::unique_ptr<SliderAttachment> sampleLoopEndAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<SliderAttachment> stereoSpreadAttachment;
    std::unique_ptr<ComboBoxAttachment> sidFilterRoutingAttachment;
    std::unique_ptr<SliderAttachment> envelopeDecayAttachment;
    std::array<std::unique_ptr<SliderAttachment>, 4> nativeAttachments;
    std::array<std::unique_ptr<SliderAttachment>, sidVoiceWaveCount> sidVoicePulseWidthAttachments;
    std::array<std::unique_ptr<SliderAttachment>, sourceChannelCount> sourceLevelAttachments;
    std::array<std::unique_ptr<ButtonAttachment>, sourceChannelCount> sourceEnableAttachments;
    std::unique_ptr<juce::FileChooser> dmcSampleChooser;
    std::unique_ptr<juce::FileChooser> userPresetChooser;
    juce::File currentUserPresetFile;

    chipper::ChipMode displayedMode = chipper::ChipMode::nes;
    std::vector<const chipper::PresetInfo*> allDisplayedPresets;
    std::vector<const chipper::PresetInfo*> displayedPresets;
    std::vector<UserPresetFile> allDisplayedUserPresets;
    std::vector<UserPresetFile> displayedUserPresets;
    std::vector<PresetFilterChoice> presetFilterChoices;
    std::vector<ChipSettingsSnapshot> chipSettingsSnapshots;
    bool descriptorTextInitialized = false;
    int displayedDmcSampleCount = -1;
    uint64_t displayedDmcSampleRevision = std::numeric_limits<uint64_t>::max();
    chipper::ChipMode displayedSampleListMode = chipper::ChipMode::nes;
    int displayedSamplePlaybackMode = -1;
    int displayedSampleMapRoot = -1;
    bool suppressMacroTemplateApply = false;
    bool suppressPresetApply = false;
    bool suppressPresetFilterChange = false;
    bool suppressManualChoiceCallbacks = false;
    bool applyingFactoryPreset = false;
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
