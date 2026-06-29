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
    static constexpr size_t sampleWaveformPreviewCount = 256;
    using SampleWaveformPreview = std::array<float, sampleWaveformPreviewCount>;
    struct SampleWaveformSnapshot
    {
        SampleWaveformPreview samples {};
        juce::String label;
        int sourceSampleCount = 0;
        int selectedSlot = -1;
        bool loaded = false;
        bool hasLoop = false;
        float loopStart = 0.0f;
        float loopEnd = 1.0f;
    };

    struct DmcSampleSlot
    {
        juce::String name;
        juce::String path;
        std::vector<uint8_t> bytes;
        chipper::ExternalSampleEncoding encoding = chipper::ExternalSampleEncoding::rawBytes;
        bool hasLoop = false;
        size_t loopStart = 0;
        size_t loopEnd = 0;
        bool included = true;
        int sourceSampleIndex = -1;
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
        int playbackMode = 0;
        int mapRootNote = 36;
        int mapHighNote = 36;
        bool loopEnabled = false;
        bool sampleActive = false;
        bool sampleCompleted = false;
        int bitsPlayed = 0;
        double bitRateHz = 0.0;
        double durationMs = 0.0;
    };

    struct Spc700BrrSampleInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        juce::String path;
        int byteCount = 0;
        int blockCount = 0;
        int bankByteCount = 0;
        int bankBrrBlockCount = 0;
        int aramBudgetBytes = 65536;
        int bankCount = 0;
        int selectedSlot = -1;
        int playbackMode = 0;
        int mapRootNote = 36;
        int mapHighNote = 36;
        int loopStartSample = 0;
        int loopEndSample = 0;
        bool loaded = false;
        bool hasLoop = false;
        bool nearAramBudget = false;
        bool exceedsAramBudget = false;
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
    std::unique_ptr<juce::XmlElement> createStateXml();
    juce::Result restoreStateXml(const juce::XmlElement& sourceXml);
    juce::Result restoreStateXml(const juce::XmlElement& sourceXml, const juce::File& presetDirectory);

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const { return apvts; }
    std::string currentCoreStatus() const;
    std::string currentCoreStatusDetail() const;
    std::string currentCoreDebugStateJson() const;
    chipper::ChipMode currentChipMode() const { return activeMode; }
    OutputScopeSnapshot outputScopeSnapshot() const;
    SampleWaveformSnapshot sampleWaveformSnapshot(chipper::ChipMode mode) const;
    juce::Result loadNesDmcSampleFile(const juce::File& file);
    juce::Result loadNesDmcSampleDirectory(const juce::File& directory);
    juce::Result loadSpc700BrrSampleFile(const juce::File& file);
    juce::Result loadSpc700BrrSampleDirectory(const juce::File& directory);
    juce::Result loadPaulaSampleFile(const juce::File& file);
    juce::Result loadPaulaSampleDirectory(const juce::File& directory);
    juce::String nesDmcSampleBankStatus() const;
    DmcSamplePlaybackInfo nesDmcSamplePlaybackInfo() const;
    Spc700BrrSampleInfo spc700BrrSampleInfo() const;
    Spc700BrrSampleInfo paulaSampleInfo() const;
    juce::StringArray nesDmcSampleNames() const;
    juce::StringArray spc700BrrSampleNames() const;
    juce::StringArray paulaSampleNames() const;
    std::vector<DmcSampleEntryInfo> nesDmcSampleEntryInfo() const;
    std::vector<DmcSampleEntryInfo> spc700BrrSampleEntryInfo() const;
    std::vector<DmcSampleEntryInfo> paulaSampleEntryInfo() const;
    void setNesDmcSampleIncluded(int index, bool shouldBeIncluded);
    void setSpc700BrrSampleIncluded(int index, bool shouldBeIncluded);
    void setPaulaSampleIncluded(int index, bool shouldBeIncluded);
    void selectFirstNesDmcSamples(int maxCount);
    void selectFirstSpc700BrrSamples(int maxCount);
    void selectFirstPaulaSamples(int maxCount);
    void clearNesDmcSampleSelection();
    void clearSpc700BrrSampleSelection();
    void clearPaulaSampleSelection();
    void invertNesDmcSampleSelection();
    void invertSpc700BrrSampleSelection();
    void invertPaulaSampleSelection();
    uint64_t nesDmcSampleRevision() const;
    uint64_t spc700BrrSampleRevision() const;
    uint64_t paulaSampleRevision() const;

private:
    struct HeldMidiNote
    {
        int note = -1;
        float velocity = 0.0f;
        int holdCount = 1;
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
    void applySpc700BrrSampleToCore();
    void applySpc700BrrSampleSlotToCore(int requestedSlot);
    void applyMappedSpc700BrrSampleForMidiNote(int midiNote);
    void applyPaulaSampleToCore();
    void applyPaulaSampleSlotToCore(int requestedSlot);
    void applyMappedPaulaSampleForMidiNote(int midiNote);
    void handleMidiMessage(const juce::MidiMessage& message);
    bool handleMidiController(const juce::MidiMessage& message);
    bool setParameterFromMidiCc(const char* parameterId, int controllerValue);
    bool setPlainParameterValue(const char* parameterId, float plainValue);
    void synchronizeMacroTemplateFromParameters();
    void applyCurrentMacroTemplateToParameters();
    void rememberHeldNote(int note, float velocity);
    int releaseHeldNote(int note);

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<chipper::ChipCore> core;
    chipper::ChipMode activeMode = chipper::ChipMode::nes;
    chipper::AccuracyMode activeAccuracy = chipper::AccuracyMode::hybrid;
    double activeClock = 1789773.0;
    double currentSampleRate = 48000.0;
    std::vector<HeldMidiNote> heldNotes;
    mutable std::mutex dmcSampleMutex;
    std::vector<DmcSampleSlot> dmcSampleBank;
    juce::String dmcSampleRestoreWarning;
    uint64_t dmcSampleBankRevision = 0;
    uint64_t activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    int activeDmcSampleSlot = -1;
    mutable std::mutex spc700SampleMutex;
    DmcSampleSlot spc700BrrSample;
    std::vector<DmcSampleSlot> spc700BrrSampleBank;
    juce::String spc700SampleRestoreWarning;
    uint64_t spc700BrrSampleBankRevision = 0;
    uint64_t activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    int activeSpc700BrrSampleSlot = -1;
    int activeSpc700BrrManualSlot = -1;
    mutable std::mutex paulaSampleMutex;
    DmcSampleSlot paulaSample;
    std::vector<DmcSampleSlot> paulaSampleBank;
    juce::String paulaSampleRestoreWarning;
    uint64_t paulaSampleBankRevision = 0;
    uint64_t activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    int activePaulaSampleSlot = -1;
    int activePaulaManualSlot = -1;
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
