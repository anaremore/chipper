#include "PluginEditor.h"

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor),
      genericEditor(processor)
{
    setSize(720, 560);

    addAndMakeVisible(genericEditor);

    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff24313f));
    statusLabel.setText("Loading chip core...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    startTimerHz(8);
}

void ChipperAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11161d));
}

void ChipperAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    statusLabel.setBounds(area.removeFromBottom(44));
    area.removeFromBottom(8);
    genericEditor.setBounds(area);
}

void ChipperAudioProcessorEditor::timerCallback()
{
    statusLabel.setText(audioProcessor.currentCoreStatus(), juce::dontSendNotification);
}
