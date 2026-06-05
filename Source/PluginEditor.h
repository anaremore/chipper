#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

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

    ChipperAudioProcessor& audioProcessor;
    juce::GenericAudioProcessorEditor genericEditor;
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessorEditor)
};
