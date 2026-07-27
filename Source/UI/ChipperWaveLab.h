#pragma once

#include <JuceHeader.h>

#include "Engine/WavetableData.h"

#include <functional>
#include <memory>

class ChipperAudioProcessor;

class ChipperWaveLab final : public juce::Component
{
public:
    struct Theme
    {
        juce::Colour panel;
        juce::Colour canvas;
        juce::Colour outline;
        juce::Colour primary;
        juce::Colour accent;
        juce::Colour text;
        juce::Colour mutedText;
    };

    explicit ChipperWaveLab(ChipperAudioProcessor& processor);

    void setMode(chipper::ChipMode mode);
    void setTheme(Theme theme);
    void refresh();
    void selectLane(size_t lane);

    juce::Rectangle<int> canvasBoundsForTest() const { return canvas.getBounds(); }
    juce::Rectangle<int> laneSelectorBoundsForTest() const { return laneBox.getBounds(); }
    juce::Rectangle<int> copyButtonBoundsForTest() const { return copyButton.getBounds(); }
    juce::Rectangle<int> pasteButtonBoundsForTest() const { return pasteButton.getBounds(); }
    juce::Rectangle<int> importButtonBoundsForTest() const { return importButton.getBounds(); }
    juce::Rectangle<int> resetButtonBoundsForTest() const { return resetButton.getBounds(); }
    juce::String statusTextForTest() const { return statusLabel.getText(); }
    int laneCountForTest() const { return laneBox.getNumItems(); }
    size_t selectedLaneForTest() const { return selectedLane; }
    void setSampleForTest(size_t sampleIndex, uint8_t value);
    void copyForTest() { copyLane(); }
    void pasteForTest() { pasteLane(); }
    void resetForTest();
    void importAudioFileForTest(const juce::File& file) { importAudioFile(file); }

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    class WaveCanvas final : public juce::Component,
                             public juce::SettableTooltipClient
    {
    public:
        WaveCanvas();

        void setTheme(const Theme& theme);
        void setSamples(const chipper::WavetableLane& samples,
                        uint8_t maximumSampleValue,
                        bool custom);
        void setSampleForTest(size_t sampleIndex, uint8_t value);

        std::function<void(const chipper::WavetableLane&)> onSamplesChanged;

        void paint(juce::Graphics& graphics) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        bool keyPressed(const juce::KeyPress& key) override;

    private:
        juce::Rectangle<float> plotBounds() const;
        size_t sampleIndexForX(float x) const;
        uint8_t sampleValueForY(float y) const;
        void applyPointer(float x, float y);
        void publishIfChanged(const chipper::WavetableLane& previous);

        Theme theme;
        chipper::WavetableLane samples {};
        uint8_t maximumSampleValue = 1u;
        bool custom = false;
        bool dragging = false;
        int lastDragSample = -1;
        size_t keyboardSample = 0u;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveCanvas)
    };

    void copyLane();
    void pasteLane();
    void resetLane();
    void chooseAudioFile();
    void importAudioFile(const juce::File& file);
    void commitLane(const chipper::WavetableLane& lane);
    void updateStatus(const juce::String& prefix = {});
    juce::String laneNoun() const;

    ChipperAudioProcessor& processor;
    chipper::ChipMode mode;
    chipper::WavetableSpec spec {};
    size_t selectedLane = 0u;
    bool currentLaneIsCustom = false;
    Theme theme;

    juce::Label titleLabel;
    juce::ComboBox laneBox;
    juce::Label statusLabel;
    juce::TextButton copyButton { "Copy" };
    juce::TextButton pasteButton { "Paste" };
    juce::TextButton importButton { "Import" };
    juce::TextButton resetButton { "Reset" };
    WaveCanvas canvas;

    chipper::WavetableLane copiedLane {};
    uint8_t copiedMaximumSampleValue = 0u;
    bool hasCopiedLane = false;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperWaveLab)
};
