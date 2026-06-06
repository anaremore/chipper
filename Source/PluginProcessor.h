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
    struct HeldMidiNote
    {
        int note = -1;
        float velocity = 0.0f;
    };

    void ensureCore();
    chipper::PatchConfig currentPatchFromParameters() const;
    void replayPendingRegisterState();
    void replayHeldNotes();
    void renderRange(juce::AudioBuffer<float>& buffer, int startSample, int endSample, float outputGain);
    void handleMidiMessage(const juce::MidiMessage& message);
    bool handleMidiController(const juce::MidiMessage& message);
    bool setParameterFromMidiCc(const char* parameterId, int controllerValue);
    bool setPlainParameterValue(const char* parameterId, float plainValue);
    void synchronizeMacroTemplateFromParameters();
    void applyCurrentMacroTemplateToParameters();
    void rememberHeldNote(int note, float velocity);
    void forgetHeldNote(int note);

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<chipper::ChipCore> core;
    chipper::ChipMode activeMode = chipper::ChipMode::nes;
    chipper::AccuracyMode activeAccuracy = chipper::AccuracyMode::hybrid;
    double activeClock = 1789773.0;
    double currentSampleRate = 48000.0;
    std::vector<HeldMidiNote> heldNotes;
    chipper::PatchConfig activePatch;
    std::vector<chipper::RegisterWrite> pendingRegisterState;
    chipper::PatchConfig lastObservedMacroPatch;
    int lastObservedMacroModeChoice = -1;
    int lastObservedMacroChoice = -1;
    bool hasObservedMacroSnapshot = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessor)
};
