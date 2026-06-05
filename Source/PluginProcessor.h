#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"
#include "Parameters.h"

class ChipperAudioProcessor final : public juce::AudioProcessor
{
public:
    ChipperAudioProcessor();
    ~ChipperAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const { return apvts; }
    std::string currentCoreStatus() const;
    chipper::ChipMode currentChipMode() const { return activeMode; }

private:
    void ensureCore();
    chipper::PatchConfig currentPatchFromParameters() const;
    void renderRange(juce::AudioBuffer<float>& buffer, int startSample, int endSample, float outputGain);
    void handleMidiMessage(const juce::MidiMessage& message);

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<chipper::ChipCore> core;
    chipper::ChipMode activeMode = chipper::ChipMode::nes;
    chipper::AccuracyMode activeAccuracy = chipper::AccuracyMode::hybrid;
    double activeClock = 1789773.0;
    double currentSampleRate = 48000.0;
    int activeNote = -1;
    float activeVelocity = 0.0f;
    chipper::PatchConfig activePatch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessor)
};
