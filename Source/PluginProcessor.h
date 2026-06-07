#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

class ChipperAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr size_t outputScopeSampleCount = 256;
    using OutputScopeSnapshot = std::array<float, outputScopeSampleCount>;
    struct DmcSampleSlot
    {
        juce::String name;
        juce::String path;
        std::vector<uint8_t> bytes;
        bool included = true;
    };

    struct DmcSampleEntryInfo
    {
        juce::String name;
        juce::String path;
        int byteCount = 0;
        bool included = true;
        bool activeSlot = false;
    };

    struct DmcSamplePlaybackInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        int activeSlot = -1;
        int activeSlotCount = 0;
        int byteCount = 0;
        int bitCount = 0;
        int rateIndex = 15;
        double bitRateHz = 0.0;
        double durationMs = 0.0;
    };

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
    std::string currentCoreStatusDetail() const;
    chipper::ChipMode currentChipMode() const { return activeMode; }
    OutputScopeSnapshot outputScopeSnapshot() const;
    juce::Result loadNesDmcSampleFile(const juce::File& file);
    juce::Result loadNesDmcSampleDirectory(const juce::File& directory);
    juce::String nesDmcSampleBankStatus() const;
    DmcSamplePlaybackInfo nesDmcSamplePlaybackInfo() const;
    juce::StringArray nesDmcSampleNames() const;
    std::vector<DmcSampleEntryInfo> nesDmcSampleEntryInfo() const;
    void setNesDmcSampleIncluded(int index, bool shouldBeIncluded);
    uint64_t nesDmcSampleRevision() const;

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
    void pushOutputScopeSample(float sample) noexcept;
    void applySelectedDmcSampleToCore();
    void applyDmcSampleSlotToCore(int requestedSlot);
    void applyMappedDmcSampleForMidiNote(int midiNote);
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
    mutable std::mutex dmcSampleMutex;
    std::vector<DmcSampleSlot> dmcSampleBank;
    uint64_t dmcSampleBankRevision = 0;
    uint64_t activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    int activeDmcSampleSlot = -1;
    chipper::PatchConfig activePatch;
    std::vector<chipper::RegisterWrite> pendingRegisterState;
    chipper::PatchConfig lastObservedMacroPatch;
    int lastObservedMacroModeChoice = -1;
    int lastObservedMacroChoice = -1;
    bool hasObservedMacroSnapshot = false;
    std::array<std::atomic<float>, outputScopeSampleCount> outputScopeBuffer {};
    std::atomic<size_t> outputScopeWriteIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessor)
};
