#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"
#include "Engine/MotionData.h"

#include <array>
#include <functional>
#include <limits>

class ChipperAudioProcessor;

class ChipperMotionLab final : public juce::Component
{
public:
    struct Theme
    {
        juce::Colour background;
        juce::Colour panel;
        juce::Colour card;
        juce::Colour outline;
        juce::Colour primary;
        juce::Colour accent;
        juce::Colour text;
        juce::Colour mutedText;
        juce::Colour darkText;
    };

    explicit ChipperMotionLab(ChipperAudioProcessor& processor);

    void setTheme(const Theme& theme);
    void setMode(chipper::ChipMode mode);
    void refresh();
    void open(chipper::ChipMode mode);
    void close();

    std::function<void()> onClose;

    void paint(juce::Graphics& graphics) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    juce::Rectangle<int> enableBoundsForTest() const { return enableButton.getBounds(); }
    juce::Rectangle<int> rateBoundsForTest() const { return rateBox.getBounds(); }
    juce::Rectangle<int> lengthBoundsForTest() const { return lengthBox.getBounds(); }
    juce::Rectangle<int> templateBoundsForTest(size_t index) const;
    juce::Rectangle<int> stepBoundsForTest(size_t index) const;
    juce::Rectangle<int> pitchBoundsForTest(size_t index) const;
    juce::Rectangle<int> levelBoundsForTest(size_t index) const;
    juce::Rectangle<int> gateBoundsForTest(size_t index) const;
    juce::String statusTextForTest() const { return statusLabel.getText(); }
    juce::String destinationTextForTest() const { return destinationLabel.getText(); }
    void applyTemplateForTest(chipper::MotionTemplate type) { applyTemplate(type); }
    juce::String stepTextForTest(size_t index) const;
    void setLengthForTest(int length);
    bool stepEnabledForTest(size_t index) const;

private:
    class StepColumn final : public juce::Component
    {
    public:
        StepColumn();

        void setTheme(const Theme& theme);
        void setStep(size_t index,
                     const chipper::MotionStep& step,
                     bool inPatternLength,
                     bool active, bool nativeNoise);
        chipper::MotionStep step() const;

        std::function<void()> onChanged;

        void paint(juce::Graphics& graphics) override;
        void resized() override;

        juce::Rectangle<int> pitchBoundsForTest() const { return pitchSlider.getBounds(); }
        juce::Rectangle<int> levelBoundsForTest() const { return levelSlider.getBounds(); }
        juce::Rectangle<int> gateBoundsForTest() const { return gateBox.getBounds(); }
        bool controlsEnabledForTest() const { return pitchSlider.isEnabled() && levelSlider.isEnabled() && gateBox.isEnabled(); }
        juce::String stepTextForTest() const { return stepLabel.getText(); }

    private:
        Theme theme;
        size_t stepIndex = 0u;
        bool inPatternLength = true;
        bool active = false;
        bool nativeNoise = false;
        bool updating = false;
        juce::Label stepLabel;
        juce::Label pitchLabel;
        juce::Slider pitchSlider;
        juce::Label levelLabel;
        juce::Slider levelSlider;
        juce::Label gateLabel;
        juce::ComboBox gateBox;
        juce::Label noiseLabel;
        juce::ComboBox noiseBox;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StepColumn)
    };

    void configureLabel(juce::Label& label, float fontSize, bool bold);
    void updatePatternControls();
    void updateRuntimeReadout(double bpm, bool hostTempo, bool bypassedForChipPoly);
    void commitPattern();
    void applyTemplate(chipper::MotionTemplate type);
    static chipper::MotionRate rateForId(int id) noexcept;
    static int idForRate(chipper::MotionRate rate) noexcept;

    ChipperAudioProcessor& processor;
    chipper::ChipMode mode = chipper::ChipMode::nes;
    chipper::MotionPattern pattern;
    Theme theme;
    uint64_t lastRevision = std::numeric_limits<uint64_t>::max();
    int lastActiveStep = std::numeric_limits<int>::min();
    bool updating = false;

    juce::Label titleLabel;
    juce::Label destinationLabel;
    juce::Label statusLabel;
    juce::TextButton closeButton { "Close" };
    juce::ToggleButton enableButton { "Enabled" };
    juce::Label rateLabel;
    juce::ComboBox rateBox;
    juce::Label lengthLabel;
    juce::ComboBox lengthBox;
    juce::Label templateLabel;
    std::array<juce::TextButton, 6> templateButtons;
    std::array<StepColumn, chipper::motionStepCount> stepColumns;
    juce::Rectangle<int> headerPanelBounds;
    juce::Rectangle<int> toolbarPanelBounds;
    juce::Rectangle<int> stepGridBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperMotionLab)
};
