#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"
#include "Engine/MotionData.h"
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

    struct WavetableSnapshot
    {
        chipper::WavetableLane samples {};
        size_t lane = 0u;
        uint8_t bitDepth = 0u;
        uint8_t maximumSampleValue = 0u;
        bool custom = false;
        uint64_t revision = 0u;
    };

    struct MotionSnapshot
    {
        chipper::MotionPattern pattern;
        chipper::ChipMode mode = chipper::ChipMode::nes;
        uint64_t revision = 0u;
        int activeStep = -1;
        double bpm = 120.0;
        bool hostTempo = false;
        bool bypassedForChipPoly = false;
    };

    enum class StateAssetPolicy
    {
        referencesOnly,
        embedProjectAssets
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
        size_t sourceByteCount = 0u;
        size_t sourceSampleCount = 0u;
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

    struct Opn2DacSampleInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        juce::String path;
        int byteCount = 0;
        int copiedByteCount = 0;
        int memoryByteCount = 262144;
        bool loaded = false;
        bool truncated = false;
    };

    struct OpnaRhythmRomInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        juce::String path;
        int byteCount = 0;
        int copiedByteCount = 0;
        int romByteCount = 8192;
        bool loaded = false;
        bool truncated = false;
    };

    struct OpnaAdpcmBSampleInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        juce::String path;
        int byteCount = 0;
        int copiedByteCount = 0;
        int decodedSampleCount = 0;
        int memoryByteCount = 262144;
        bool loaded = false;
        bool truncated = false;
        bool convertedFromPcm = false;
    };

    struct OpnbAdpcmSampleInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        juce::String path;
        int byteCount = 0;
        int copiedByteCount = 0;
        int decodedSampleCount = 0;
        int memoryByteCount = 1048576;
        bool loaded = false;
        bool truncated = false;
        bool convertedFromPcm = false;
    };

    struct AdpcmARegionInfo
    {
        juce::String statusLine;
        juce::String sampleName;
        juce::String path;
        juce::String roleName;
        int regionIndex = 0;
        int sourceByteCount = 0;
        int encodedByteCount = 0;
        int decodedSampleCount = 0;
        int capacityByteCount = 0;
        int startByte = 0;
        int endByteInclusive = 0;
        int packedBankByteCount = 0;
        int loadedRegionCount = 0;
        double sampleRateHz = 0.0;
        bool loaded = false;
        bool convertedFromPcm = false;
        bool editableBankActive = false;
        bool legacyBankActive = false;
        bool legacyBankMissing = false;
        juce::String legacyBankName;
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
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    std::unique_ptr<juce::XmlElement> createStateXml(StateAssetPolicy assetPolicy = StateAssetPolicy::referencesOnly);
    juce::Result restoreStateXml(const juce::XmlElement& sourceXml);
    juce::Result restoreStateXml(const juce::XmlElement& sourceXml, const juce::File& presetDirectory);

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const { return apvts; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    std::string currentCoreStatus() const;
    std::string currentCoreStatusDetail() const;
    std::string currentCoreDebugStateJson() const;
    chipper::ChipMode currentChipMode() const { return publishedActiveMode.load(std::memory_order_acquire); }
    OutputScopeSnapshot outputScopeSnapshot() const;
    SampleWaveformSnapshot sampleWaveformSnapshot(chipper::ChipMode mode) const;
    WavetableSnapshot wavetableSnapshot(chipper::ChipMode mode, size_t lane) const;
    bool setWavetableLane(chipper::ChipMode mode, size_t lane, const chipper::WavetableLane& samples);
    bool resetWavetableLane(chipper::ChipMode mode, size_t lane);
    uint64_t wavetableRevision(chipper::ChipMode mode) const noexcept;
    MotionSnapshot motionSnapshot(chipper::ChipMode mode) const;
    bool setMotionPattern(chipper::ChipMode mode, const chipper::MotionPattern& pattern);
    bool resetMotionPattern(chipper::ChipMode mode);
    uint64_t motionRevision(chipper::ChipMode mode) const noexcept;
    juce::Result loadNesDmcSampleFile(const juce::File& file);
    juce::Result loadNesDmcSampleDirectory(const juce::File& directory);
    juce::Result loadSpc700BrrSampleFile(const juce::File& file);
    juce::Result loadSpc700BrrSampleDirectory(const juce::File& directory);
    juce::Result loadPaulaSampleFile(const juce::File& file);
    juce::Result loadPaulaSampleDirectory(const juce::File& directory);
    juce::Result loadOpn2DacSampleFile(const juce::File& file);
    juce::Result loadOpnaRhythmRomFile(const juce::File& file);
    juce::Result loadOpnaAdpcmBSampleFile(const juce::File& file);
    juce::Result loadOpnbAdpcmASampleFile(const juce::File& file);
    juce::Result loadOpnbAdpcmBSampleFile(const juce::File& file);
    juce::Result loadOpnaAdpcmARegionFile(int regionIndex, const juce::File& file, bool replaceLegacyBank = false);
    juce::Result loadOpnbAdpcmARegionFile(int regionIndex, const juce::File& file, bool replaceLegacyBank = false);
    void clearOpnaAdpcmARegion(int regionIndex);
    void clearOpnbAdpcmARegion(int regionIndex);
    juce::String nesDmcSampleBankStatus() const;
    DmcSamplePlaybackInfo nesDmcSamplePlaybackInfo() const;
    Spc700BrrSampleInfo spc700BrrSampleInfo() const;
    Spc700BrrSampleInfo paulaSampleInfo() const;
    Opn2DacSampleInfo opn2DacSampleInfo() const;
    OpnaRhythmRomInfo opnaRhythmRomInfo() const;
    OpnaAdpcmBSampleInfo opnaAdpcmBSampleInfo() const;
    OpnbAdpcmSampleInfo opnbAdpcmASampleInfo() const;
    OpnbAdpcmSampleInfo opnbAdpcmBSampleInfo() const;
    juce::StringArray nesDmcSampleNames() const;
    AdpcmARegionInfo adpcmARegionInfo(chipper::ChipMode mode, int regionIndex) const;
    SampleWaveformSnapshot adpcmARegionWaveformSnapshot(chipper::ChipMode mode, int regionIndex) const;
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
    static constexpr size_t chipModeCount = static_cast<size_t>(chipper::ChipMode::ym2610b) + 1u;
    static constexpr size_t corePoolSize = chipModeCount;
    static constexpr size_t editableWavetableModeCount = 3u;

    struct PublishedWavetableMemory
    {
        PublishedWavetableMemory() noexcept;
        chipper::WavetableMemory load() const noexcept;
        void store(const chipper::WavetableMemory& memory) noexcept;

        std::array<chipper::WavetableMemory, 2> slots;
        mutable std::array<std::atomic<uint32_t>, 2> readerCounts {};
        std::atomic<uint8_t> activeSlot { 0u };
        std::atomic<uint64_t> revision { 0u };
    };

    struct PublishedMotionPattern
    {
        PublishedMotionPattern() noexcept;
        chipper::MotionPattern load() const noexcept;
        void store(const chipper::MotionPattern& pattern) noexcept;

        std::array<chipper::MotionPattern, 2> slots;
        mutable std::array<std::atomic<uint32_t>, 2> readerCounts {};
        std::atomic<uint8_t> activeSlot { 0u };
        std::atomic<uint64_t> revision { 0u };
    };

    struct HeldMidiNote
    {
        int note = -1;
        float velocity = 0.0f;
        int holdCount = 1;
    };

    void ensureCore();
    juce::Result restoreStateXmlInternal(const juce::XmlElement& sourceXml,
                                         const juce::File& presetDirectory,
                                         bool allowEmbeddedProjectAssets);
    void initializeCorePool();
    void primeYamahaAdpcmACores();
    void synchronizeActiveExternalAssets(chipper::ChipMode mode);
    static size_t corePoolIndex(chipper::ChipMode mode) noexcept;
    static int editableWavetableIndex(chipper::ChipMode mode) noexcept;
    chipper::WavetableMemory wavetableMemory(chipper::ChipMode mode) const noexcept;
    void publishWavetableMemory(chipper::ChipMode mode, const chipper::WavetableMemory& memory) noexcept;
    chipper::MotionPattern motionPattern(chipper::ChipMode mode) const noexcept;
    chipper::PatchConfig currentPatchFromParameters() const;
    void replayPendingRegisterState();
    void replayHeldNotes();
    void renderRange(juce::AudioBuffer<float>& buffer, int startSample, int endSample, float outputGain);
    void renderRangeWithMotion(juce::AudioBuffer<float>& buffer, int startSample, int endSample, float outputGain);
    void synchronizeActiveMotion(bool replayNotesOnChange);
    void updateMotionTempoFromPlayhead();
    void resetMotionPlayback() noexcept;
    void beginMotionSequence() noexcept;
    void advanceMotionStep();
    bool motionPlaybackEnabled() const noexcept;
    int motionMidiNote(int midiNote) const noexcept;
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
    void applyOpn2DacSampleToCore();
    void applyOpnaRhythmRomToCore();
    void applyOpnaAdpcmBSampleToCore();
    void applyOpnbAdpcmASampleToCore();
    void applyOpnbAdpcmBSampleToCore();
    void handleMidiMessage(const juce::MidiMessage& message);
    bool handleMidiController(const juce::MidiMessage& message);
    bool setParameterFromMidiCc(const char* parameterId, int controllerValue);
    bool setPlainParameterValue(const char* parameterId, float plainValue);
    void synchronizeMacroTemplateFromParameters();
    void applyCurrentMacroTemplateToParameters();
    void rememberHeldNote(int note, float velocity);
    int releaseHeldNote(int note);

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<chipper::ChipCore> core;
    std::array<std::unique_ptr<chipper::ChipCore>, corePoolSize> corePool;
    std::array<uint64_t, corePoolSize> pooledDmcRevisions {};
    std::array<uint64_t, corePoolSize> pooledSpc700Revisions {};
    std::array<uint64_t, corePoolSize> pooledPaulaRevisions {};
    std::array<uint64_t, corePoolSize> pooledOpn2DacRevisions {};
    std::array<uint64_t, corePoolSize> pooledOpnaRhythmRevisions {};
    std::array<uint64_t, corePoolSize> pooledOpnaAdpcmBRevisions {};
    std::array<uint64_t, corePoolSize> pooledOpnbAdpcmARevisions {};
    std::array<uint64_t, corePoolSize> pooledOpnbAdpcmBRevisions {};
    chipper::ChipMode activeMode = chipper::ChipMode::nes;
    chipper::AccuracyMode activeAccuracy = chipper::AccuracyMode::hybrid;
    double activeClock = 1789773.0;
    double currentSampleRate = 48000.0;
    std::vector<HeldMidiNote> heldNotes;
    mutable std::mutex dmcSampleMutex;
    std::vector<DmcSampleSlot> dmcSampleBank;
    juce::String dmcSampleRestoreWarning;
    std::atomic<uint64_t> dmcSampleBankRevision { 0 };
    uint64_t activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    std::atomic<int> activeDmcSampleSlot { -1 };
    int activeDmcSampleSlotCount = 0;
    int lastRequestedDmcSampleSlot = std::numeric_limits<int>::min();
    mutable std::mutex spc700SampleMutex;
    DmcSampleSlot spc700BrrSample;
    std::vector<DmcSampleSlot> spc700BrrSampleBank;
    juce::String spc700SampleRestoreWarning;
    std::atomic<uint64_t> spc700BrrSampleBankRevision { 0 };
    uint64_t activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    std::atomic<int> activeSpc700BrrSampleSlot { -1 };
    std::atomic<int> activeSpc700BrrManualSlot { -1 };
    int activeSpc700BrrSampleSlotCount = 0;
    mutable std::mutex paulaSampleMutex;
    DmcSampleSlot paulaSample;
    std::vector<DmcSampleSlot> paulaSampleBank;
    juce::String paulaSampleRestoreWarning;
    std::atomic<uint64_t> paulaSampleBankRevision { 0 };
    uint64_t activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    std::atomic<int> activePaulaSampleSlot { -1 };
    std::atomic<int> activePaulaManualSlot { -1 };
    int activePaulaSampleSlotCount = 0;
    mutable std::mutex opn2DacSampleMutex;
    DmcSampleSlot opn2DacSample;
    juce::String opn2DacSampleRestoreWarning;
    std::atomic<uint64_t> opn2DacSampleRevision { 0 };
    uint64_t activeOpn2DacSampleRevision = std::numeric_limits<uint64_t>::max();
    mutable std::mutex opnaRhythmRomMutex;
    DmcSampleSlot opnaRhythmRom;
    juce::String opnaRhythmRomRestoreWarning;
    std::atomic<uint64_t> opnaRhythmRomRevision { 0 };
    std::vector<DmcSampleSlot> opnaAdpcmARegions;
    uint64_t activeOpnaRhythmRomRevision = std::numeric_limits<uint64_t>::max();
    mutable std::mutex opnaAdpcmBSampleMutex;
    DmcSampleSlot opnaAdpcmBSample;
    juce::String opnaAdpcmBSampleRestoreWarning;
    std::atomic<uint64_t> opnaAdpcmBSampleRevision { 0 };
    uint64_t activeOpnaAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    mutable std::mutex opnbAdpcmASampleMutex;
    DmcSampleSlot opnbAdpcmASample;
    juce::String opnbAdpcmASampleRestoreWarning;
    std::atomic<uint64_t> opnbAdpcmASampleRevision { 0 };
    std::vector<DmcSampleSlot> opnbAdpcmARegions;
    uint64_t activeOpnbAdpcmASampleRevision = std::numeric_limits<uint64_t>::max();
    mutable std::mutex opnbAdpcmBSampleMutex;
    DmcSampleSlot opnbAdpcmBSample;
    juce::String opnbAdpcmBSampleRestoreWarning;
    std::atomic<uint64_t> opnbAdpcmBSampleRevision { 0 };
    uint64_t activeOpnbAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    mutable std::mutex wavetableWriteMutex;
    std::array<PublishedWavetableMemory, editableWavetableModeCount> wavetableMemories;
    mutable std::mutex motionWriteMutex;
    std::array<PublishedMotionPattern, chipModeCount> motionPatterns;
    chipper::MotionPattern activeMotionPattern;
    uint64_t activeMotionRevision = std::numeric_limits<uint64_t>::max();
    chipper::ChipMode activeMotionMode = chipper::ChipMode::nes;
    bool activeMotionInitialized = false;
    int activeMotionStep = -1;
    int activeMotionPitch = 0;
    float activeMotionGain = 1.0f;
    bool activeMotionCut = false;
    double motionSamplesUntilNextStep = 0.0;
    double activeMotionStepSamples = 0.0;
    double activeMotionBpm = 120.0;
    std::atomic<int> publishedMotionStep { -1 };
    std::atomic<double> publishedMotionBpm { 120.0 };
    std::atomic<bool> publishedMotionHostTempo { false };
    std::atomic<bool> publishedMotionChipPolyBypass { false };
    chipper::PatchConfig activePatch;
    std::vector<chipper::RegisterWrite> pendingRegisterState;
    chipper::PatchConfig lastObservedMacroPatch;
    int lastObservedMacroModeChoice = -1;
    int lastObservedMacroChoice = -1;
    bool hasObservedMacroSnapshot = false;
    std::array<std::atomic<float>, outputScopeSampleCount> outputScopeBuffer {};
    std::atomic<size_t> outputScopeWriteIndex { 0 };
    std::atomic<chipper::ChipMode> publishedActiveMode { chipper::ChipMode::nes };
    std::atomic<bool> publishedCoreReady { false };
    std::atomic<bool> publishedDmcSampleActive { false };
    std::atomic<bool> publishedDmcSampleCompleted { false };
    std::atomic<int> publishedDmcSampleBitsPlayed { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperAudioProcessor)
};
