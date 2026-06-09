#include "PluginProcessor.h"

#include "Engine/ChipDescriptors.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <string_view>

namespace
{
constexpr auto coreStateTag = "CHIPPER_CORE_REGISTERS";
constexpr auto registerTag = "REG";
constexpr auto dmcBankStateTag = "CHIPPER_DMC_BANK";
constexpr auto dmcSampleStateTag = "DMC_SAMPLE";
constexpr auto spc700BrrStateTag = "CHIPPER_SPC700_BRR";
constexpr auto spc700BrrBankStateTag = "CHIPPER_SPC700_BRR_BANK";
constexpr auto spc700BrrSampleStateTag = "BRR_SAMPLE";
constexpr auto paulaSampleBankStateTag = "CHIPPER_PAULA_SAMPLE_BANK";
constexpr auto paulaSampleStateTag = "PAULA_SAMPLE";
constexpr auto unmappedDmcSampleSlot = -2;

juce::String midiNoteName(int note)
{
    static constexpr std::array<const char*, 12> names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto clampedNote = std::clamp(note, 0, 127);
    return juce::String(names[static_cast<size_t>(clampedNote % 12)]) + juce::String((clampedNote / 12) - 2);
}

bool sourceLevelsMatch(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    for (size_t i = 0; i < a.sourceLevels.size(); ++i)
    {
        if (std::abs(a.sourceLevels[i] - b.sourceLevels[i]) >= tolerance)
            return false;
    }

    return true;
}

bool patchMatches(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    return a.macro == b.macro
        && std::abs(a.control1 - b.control1) < tolerance
        && std::abs(a.control2 - b.control2) < tolerance
        && std::abs(a.control3 - b.control3) < tolerance
        && std::abs(a.control4 - b.control4) < tolerance
        && a.playMode == b.playMode
        && a.sourceEnabled == b.sourceEnabled
        && sourceLevelsMatch(a, b)
        && std::abs(a.stereoSpread - b.stereoSpread) < tolerance
        && a.sidFilterRouting == b.sidFilterRouting
        && std::abs(a.envelopeDecay - b.envelopeDecay) < tolerance
        && a.sidAttack == b.sidAttack
        && a.sidDecay == b.sidDecay
        && a.sidSustain == b.sidSustain
        && a.sidRelease == b.sidRelease
        && a.sidVoice2Attack == b.sidVoice2Attack
        && a.sidVoice2Decay == b.sidVoice2Decay
        && a.sidVoice2Sustain == b.sidVoice2Sustain
        && a.sidVoice2Release == b.sidVoice2Release
        && a.sidVoice3Attack == b.sidVoice3Attack
        && a.sidVoice3Decay == b.sidVoice3Decay
        && a.sidVoice3Sustain == b.sidVoice3Sustain
        && a.sidVoice3Release == b.sidVoice3Release
        && a.waveShape == b.waveShape
        && a.pulse2Duty == b.pulse2Duty
        && a.dmgWaveLevel == b.dmgWaveLevel
        && a.dmgStereoRoute == b.dmgStereoRoute
        && a.ymEnvelopeShape == b.ymEnvelopeShape
        && a.ymChannelAMix == b.ymChannelAMix
        && a.ymChannelBMix == b.ymChannelBMix
        && a.ymChannelCMix == b.ymChannelCMix
        && a.snNoiseMode == b.snNoiseMode
        && a.sidVoice2WaveShape == b.sidVoice2WaveShape
        && a.sidVoice3WaveShape == b.sidVoice3WaveShape
        && std::abs(a.sidVoice2PulseWidth - b.sidVoice2PulseWidth) < tolerance
        && std::abs(a.sidVoice3PulseWidth - b.sidVoice3PulseWidth) < tolerance
        && std::abs(a.nesDmcDirectLevel - b.nesDmcDirectLevel) < tolerance
        && a.nesDmcRateIndex == b.nesDmcRateIndex
        && a.nesDmcLoop == b.nesDmcLoop
        && a.nesDmcOnly == b.nesDmcOnly;
}

bool patchControlsMatch(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    return std::abs(a.control1 - b.control1) < tolerance
        && std::abs(a.control2 - b.control2) < tolerance
        && std::abs(a.control3 - b.control3) < tolerance
        && std::abs(a.control4 - b.control4) < tolerance
        && a.playMode == b.playMode
        && a.sourceEnabled == b.sourceEnabled
        && sourceLevelsMatch(a, b)
        && std::abs(a.stereoSpread - b.stereoSpread) < tolerance
        && a.sidFilterRouting == b.sidFilterRouting
        && std::abs(a.envelopeDecay - b.envelopeDecay) < tolerance
        && a.sidAttack == b.sidAttack
        && a.sidDecay == b.sidDecay
        && a.sidSustain == b.sidSustain
        && a.sidRelease == b.sidRelease
        && a.sidVoice2Attack == b.sidVoice2Attack
        && a.sidVoice2Decay == b.sidVoice2Decay
        && a.sidVoice2Sustain == b.sidVoice2Sustain
        && a.sidVoice2Release == b.sidVoice2Release
        && a.sidVoice3Attack == b.sidVoice3Attack
        && a.sidVoice3Decay == b.sidVoice3Decay
        && a.sidVoice3Sustain == b.sidVoice3Sustain
        && a.sidVoice3Release == b.sidVoice3Release
        && a.waveShape == b.waveShape
        && a.pulse2Duty == b.pulse2Duty
        && a.dmgWaveLevel == b.dmgWaveLevel
        && a.dmgStereoRoute == b.dmgStereoRoute
        && a.ymEnvelopeShape == b.ymEnvelopeShape
        && a.ymChannelAMix == b.ymChannelAMix
        && a.ymChannelBMix == b.ymChannelBMix
        && a.ymChannelCMix == b.ymChannelCMix
        && a.snNoiseMode == b.snNoiseMode
        && a.sidVoice2WaveShape == b.sidVoice2WaveShape
        && a.sidVoice3WaveShape == b.sidVoice3WaveShape
        && std::abs(a.sidVoice2PulseWidth - b.sidVoice2PulseWidth) < tolerance
        && std::abs(a.sidVoice3PulseWidth - b.sidVoice3PulseWidth) < tolerance
        && std::abs(a.nesDmcDirectLevel - b.nesDmcDirectLevel) < tolerance
        && a.nesDmcRateIndex == b.nesDmcRateIndex
        && a.nesDmcLoop == b.nesDmcLoop
        && a.nesDmcOnly == b.nesDmcOnly;
}

juce::Result readDmcSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("DMC sample file does not exist: " + file.getFullPathName());

    if (! file.hasFileExtension(".dmc"))
        return juce::Result::fail("Only .dmc files are supported in this build. WAV-to-DMC import is planned.");

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read DMC sample file: " + file.getFullPathName());

    if (block.getSize() == 0u)
        return juce::Result::fail("DMC sample file is empty: " + file.getFullPathName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    return juce::Result::ok();
}

juce::Result readSpc700BrrSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("BRR sample file does not exist: " + file.getFullPathName());

    if (! file.hasFileExtension(".brr"))
        return juce::Result::fail("Only .brr files are supported for SPC700 sample import in this build.");

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read BRR sample file: " + file.getFullPathName());

    if (block.getSize() == 0u)
        return juce::Result::fail("BRR sample file is empty: " + file.getFullPathName());

    if ((block.getSize() % 9u) != 0u)
        return juce::Result::fail("BRR sample size must be a whole number of 9-byte BRR blocks: " + file.getFileName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    return juce::Result::ok();
}

bool fileLooksLikePaulaImport(const juce::File& file)
{
    return file.hasFileExtension(".wav;.aif;.aiff");
}

juce::Result readPaulaSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("Paula sample file does not exist: " + file.getFullPathName());

    if (! fileLooksLikePaulaImport(file))
        return juce::Result::fail("Paula sample import supports WAV/AIFF files in this build. IFF/8SVX/MOD import is planned.");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        return juce::Result::fail("Could not decode Paula sample file: " + file.getFullPathName());

    if (reader->lengthInSamples <= 0)
        return juce::Result::fail("Paula sample file is empty: " + file.getFullPathName());

    constexpr int maxImportedSamples = 262144;
    const auto sampleCount = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, maxImportedSamples));
    juce::AudioBuffer<float> decoded(static_cast<int>(reader->numChannels), sampleCount);
    if (! reader->read(&decoded, 0, sampleCount, 0, true, true))
        return juce::Result::fail("Could not read Paula sample audio: " + file.getFullPathName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.bytes.resize(static_cast<size_t>(sampleCount));
    const auto channelCount = std::max(1, decoded.getNumChannels());
    for (int i = 0; i < sampleCount; ++i)
    {
        auto mixed = 0.0f;
        for (int channel = 0; channel < channelCount; ++channel)
            mixed += decoded.getSample(channel, i);
        mixed /= static_cast<float>(channelCount);

        const auto quantized = std::clamp(static_cast<int>(std::round(juce::jlimit(-1.0f, 1.0f, mixed) * 127.0f)), -128, 127);
        slot.bytes[static_cast<size_t>(i)] = static_cast<uint8_t>(quantized + 128);
    }

    if (slot.bytes.empty())
        return juce::Result::fail("Paula sample import produced no usable sample bytes: " + file.getFileName());

    return juce::Result::ok();
}
}

ChipperAudioProcessor::ChipperAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ChipperState", chipper::parameters::createLayout())
{
}

void ChipperAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    core.reset();
    hasObservedMacroSnapshot = false;
    for (auto& sample : outputScopeBuffer)
        sample.store(0.0f, std::memory_order_relaxed);
    outputScopeWriteIndex.store(0u, std::memory_order_release);
    ensureCore();
}

void ChipperAudioProcessor::releaseResources()
{
}

juce::Result ChipperAudioProcessor::loadNesDmcSampleFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readDmcSampleFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        dmcSampleBank.clear();
        dmcSampleBank.push_back(std::move(slot));
        ++dmcSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadNesDmcSampleDirectory(const juce::File& directory)
{
    if (! directory.isDirectory())
        return juce::Result::fail("DMC sample directory does not exist: " + directory.getFullPathName());

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.dmc");
    files.sort();

    std::vector<DmcSampleSlot> loaded;
    loaded.reserve(static_cast<size_t>(std::min(128, files.size())));
    for (const auto& file : files)
    {
        if (loaded.size() >= 128u)
            break;

        DmcSampleSlot slot;
        if (readDmcSampleFile(file, slot).wasOk())
        {
            slot.included = loaded.size() < 32u;
            loaded.push_back(std::move(slot));
        }
    }

    if (loaded.empty())
        return juce::Result::fail("No readable .dmc files found in: " + directory.getFullPathName());

    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        dmcSampleBank = std::move(loaded);
        ++dmcSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadSpc700BrrSampleFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readSpc700BrrSampleFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        spc700BrrSampleBank.clear();
        spc700BrrSampleBank.push_back(std::move(slot));
        spc700BrrSample = spc700BrrSampleBank.front();
        ++spc700BrrSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activeSpc700BrrManualSlot = -1;
    applySpc700BrrSampleToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadSpc700BrrSampleDirectory(const juce::File& directory)
{
    if (! directory.isDirectory())
        return juce::Result::fail("BRR sample directory does not exist: " + directory.getFullPathName());

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.brr");
    files.sort();

    std::vector<DmcSampleSlot> loaded;
    loaded.reserve(static_cast<size_t>(std::min(32, files.size())));
    for (const auto& file : files)
    {
        if (loaded.size() >= 32u)
            break;

        DmcSampleSlot slot;
        if (readSpc700BrrSampleFile(file, slot).wasOk())
            loaded.push_back(std::move(slot));
    }

    if (loaded.empty())
        return juce::Result::fail("No readable .brr files found in: " + directory.getFullPathName());

    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        spc700BrrSampleBank = std::move(loaded);
        spc700BrrSample = spc700BrrSampleBank.front();
        ++spc700BrrSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    applySpc700BrrSampleToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadPaulaSampleFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readPaulaSampleFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        paulaSampleBank.clear();
        paulaSampleBank.push_back(std::move(slot));
        paulaSample = paulaSampleBank.front();
        ++paulaSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadPaulaSampleDirectory(const juce::File& directory)
{
    if (! directory.isDirectory())
        return juce::Result::fail("Paula sample directory does not exist: " + directory.getFullPathName());

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff");
    files.sort();

    std::vector<DmcSampleSlot> loaded;
    loaded.reserve(static_cast<size_t>(std::min(128, files.size())));
    for (const auto& file : files)
    {
        if (loaded.size() >= 128u)
            break;

        DmcSampleSlot slot;
        if (readPaulaSampleFile(file, slot).wasOk())
        {
            slot.included = loaded.size() < 32u;
            loaded.push_back(std::move(slot));
        }
    }

    if (loaded.empty())
        return juce::Result::fail("No readable WAV/AIFF files found in: " + directory.getFullPathName());

    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        paulaSampleBank = std::move(loaded);
        paulaSample = paulaSampleBank.front();
        ++paulaSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
    return juce::Result::ok();
}

ChipperAudioProcessor::DmcSamplePlaybackInfo ChipperAudioProcessor::nesDmcSamplePlaybackInfo() const
{
    static constexpr std::array<int, 16> ntscDmcPeriods {
        428, 380, 340, 320, 286, 254, 226, 214,
        190, 160, 142, 128, 106, 85, 72, 54
    };

    DmcSamplePlaybackInfo info;
    const auto manualSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
    info.playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    info.mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    info.loopEnabled = apvts.getRawParameterValue(chipper::parameters::id::nesDmcLoop)->load() >= 0.5f;
    const auto selectedSlot = info.playbackMode != 0 && activeDmcSampleSlot >= 0 ? activeDmcSampleSlot : manualSlot;
    info.rateIndex = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcRateIndex)->load()));
    info.rateIndex = std::clamp(info.rateIndex, 0, 15);

    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto selectedMode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto clockOverride = apvts.getRawParameterValue(chipper::parameters::id::clockHz)->load();
    const auto selectedClock = clockOverride > 0.0f ? static_cast<double>(clockOverride) : chipper::parameters::defaultClockForMode(selectedMode);
    info.bitRateHz = selectedClock / static_cast<double>(ntscDmcPeriods[static_cast<size_t>(info.rateIndex)]);

    const std::lock_guard<std::mutex> lock(dmcSampleMutex);
    if (dmcSampleBank.empty())
    {
        info.statusLine = "No DMC samples loaded";
        return info;
    }

    std::vector<const DmcSampleSlot*> activeSlots;
    activeSlots.reserve(32u);
    for (const auto& slot : dmcSampleBank)
    {
        if (slot.included)
            activeSlots.push_back(&slot);
        if (activeSlots.size() >= 32u)
            break;
    }

    if (activeSlots.empty())
    {
        info.statusLine = "No DMC samples checked";
        return info;
    }

    info.activeSlotCount = static_cast<int>(activeSlots.size());
    info.mapHighNote = std::clamp(info.mapRootNote + info.activeSlotCount - 1, 0, 127);
    if (info.playbackMode != 0 && activeDmcSampleSlot == unmappedDmcSampleSlot)
    {
        info.activeSlot = -1;
        info.statusLine = "No mapped DMC sample | Map " + midiNoteName(info.mapRootNote) + "-" + midiNoteName(info.mapHighNote);
        return info;
    }

    const auto safeSlot = static_cast<size_t>(std::clamp(selectedSlot, 0, static_cast<int>(activeSlots.size() - 1u)));
    const auto& slot = *activeSlots[safeSlot];
    info.activeSlot = static_cast<int>(safeSlot);
    info.sampleName = slot.name;
    info.byteCount = static_cast<int>(slot.bytes.size());
    info.bitCount = info.byteCount * 8;
    info.durationMs = info.bitRateHz > 0.0 ? (static_cast<double>(info.bitCount) / info.bitRateHz) * 1000.0 : 0.0;
    info.statusLine = "Slot " + juce::String(info.activeSlot + 1) + "/" + juce::String(info.activeSlotCount)
        + ": " + info.sampleName + " (" + juce::String(info.byteCount) + " bytes, "
        + juce::String(info.durationMs, info.durationMs < 10.0 ? 1 : 0) + " ms @ rate "
        + juce::String(info.rateIndex) + (info.loopEnabled ? ", Loop)" : ", One-shot)");
    if (info.playbackMode != 0)
        info.statusLine += " | Map " + midiNoteName(info.mapRootNote) + "-" + midiNoteName(info.mapHighNote);
    return info;
}

juce::String ChipperAudioProcessor::nesDmcSampleBankStatus() const
{
    return nesDmcSamplePlaybackInfo().statusLine;
}

ChipperAudioProcessor::Spc700BrrSampleInfo ChipperAudioProcessor::spc700BrrSampleInfo() const
{
    Spc700BrrSampleInfo info;
    const std::lock_guard<std::mutex> lock(spc700SampleMutex);
    if (spc700BrrSample.bytes.empty())
    {
        info.statusLine = "No BRR sample loaded";
        return info;
    }

    info.loaded = true;
    const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
    std::vector<const DmcSampleSlot*> activeSlots;
    activeSlots.reserve(32u);
    if (! spc700BrrSampleBank.empty())
    {
        for (const auto& slot : spc700BrrSampleBank)
        {
            if (slot.included)
                activeSlots.push_back(&slot);
            if (activeSlots.size() >= 32u)
                break;
        }
    }
    else
    {
        activeSlots.push_back(&spc700BrrSample);
    }

    if (activeSlots.empty())
    {
        info.loaded = false;
        info.statusLine = "No BRR samples checked";
        return info;
    }

    const auto bankCount = static_cast<int>(activeSlots.size());
    auto safeSlot = std::clamp(selectedSlot, 0, std::max(0, bankCount - 1));
    for (int i = 0; i < static_cast<int>(activeSlots.size()); ++i)
    {
        if (activeSlots[static_cast<size_t>(i)]->path == spc700BrrSample.path)
        {
            safeSlot = i;
            break;
        }
    }
    const auto mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    const auto playbackMode = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load())), 0, 2);
    const auto& selected = *activeSlots[static_cast<size_t>(safeSlot)];
    info.sampleName = selected.name;
    info.path = selected.path;
    info.byteCount = static_cast<int>(selected.bytes.size());
    info.blockCount = info.byteCount / 9;
    info.bankCount = bankCount;
    info.selectedSlot = safeSlot;
    info.playbackMode = playbackMode;
    info.mapRootNote = mapRootNote;
    info.mapHighNote = std::clamp(mapRootNote + std::max(0, bankCount - 1), 0, 127);
    info.statusLine = "Slot " + juce::String(safeSlot + 1) + "/" + juce::String(bankCount)
        + ": " + info.sampleName + " (" + juce::String(info.byteCount) + " bytes, "
        + juce::String(info.blockCount) + " BRR blocks)";
    return info;
}

ChipperAudioProcessor::Spc700BrrSampleInfo ChipperAudioProcessor::paulaSampleInfo() const
{
    Spc700BrrSampleInfo info;
    const std::lock_guard<std::mutex> lock(paulaSampleMutex);
    if (paulaSample.bytes.empty())
    {
        info.statusLine = "No Paula samples loaded";
        return info;
    }

    info.loaded = true;
    const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
    std::vector<const DmcSampleSlot*> activeSlots;
    activeSlots.reserve(32u);
    if (! paulaSampleBank.empty())
    {
        for (const auto& slot : paulaSampleBank)
        {
            if (slot.included)
                activeSlots.push_back(&slot);
            if (activeSlots.size() >= 32u)
                break;
        }
    }
    else
    {
        activeSlots.push_back(&paulaSample);
    }

    if (activeSlots.empty())
    {
        info.loaded = false;
        info.statusLine = "No Paula samples checked";
        return info;
    }

    const auto bankCount = static_cast<int>(activeSlots.size());
    auto safeSlot = std::clamp(selectedSlot, 0, std::max(0, bankCount - 1));
    for (int i = 0; i < static_cast<int>(activeSlots.size()); ++i)
    {
        if (activeSlots[static_cast<size_t>(i)]->path == paulaSample.path)
        {
            safeSlot = i;
            break;
        }
    }

    const auto mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    const auto playbackMode = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load())), 0, 2);
    const auto& selected = *activeSlots[static_cast<size_t>(safeSlot)];
    info.sampleName = selected.name;
    info.path = selected.path;
    info.byteCount = static_cast<int>(selected.bytes.size());
    info.blockCount = 0;
    info.bankCount = bankCount;
    info.selectedSlot = safeSlot;
    info.playbackMode = playbackMode;
    info.mapRootNote = mapRootNote;
    info.mapHighNote = std::clamp(mapRootNote + std::max(0, bankCount - 1), 0, 127);
    info.statusLine = "Slot " + juce::String(safeSlot + 1) + "/" + juce::String(bankCount)
        + ": " + info.sampleName + " (" + juce::String(info.byteCount) + " 8-bit samples)";
    return info;
}

juce::StringArray ChipperAudioProcessor::nesDmcSampleNames() const
{
    juce::StringArray names;
    const std::lock_guard<std::mutex> lock(dmcSampleMutex);
    for (const auto& slot : dmcSampleBank)
    {
        if (! slot.included)
            continue;

        names.add(slot.name);
        if (names.size() >= 32)
            break;
    }

    return names;
}

juce::StringArray ChipperAudioProcessor::spc700BrrSampleNames() const
{
    juce::StringArray names;
    const std::lock_guard<std::mutex> lock(spc700SampleMutex);
    if (spc700BrrSampleBank.empty() && ! spc700BrrSample.bytes.empty())
    {
        names.add(spc700BrrSample.name);
        return names;
    }

    for (const auto& slot : spc700BrrSampleBank)
    {
        if (! slot.included)
            continue;
        names.add(slot.name);
        if (names.size() >= 32)
            break;
    }

    return names;
}

juce::StringArray ChipperAudioProcessor::paulaSampleNames() const
{
    juce::StringArray names;
    const std::lock_guard<std::mutex> lock(paulaSampleMutex);
    if (paulaSampleBank.empty() && ! paulaSample.bytes.empty())
    {
        names.add(paulaSample.name);
        return names;
    }

    for (const auto& slot : paulaSampleBank)
    {
        if (! slot.included)
            continue;
        names.add(slot.name);
        if (names.size() >= 32)
            break;
    }

    return names;
}

std::vector<ChipperAudioProcessor::DmcSampleEntryInfo> ChipperAudioProcessor::nesDmcSampleEntryInfo() const
{
    std::vector<DmcSampleEntryInfo> entries;
    const std::lock_guard<std::mutex> lock(dmcSampleMutex);
    entries.reserve(dmcSampleBank.size());

    int activeIndex = 0;
    for (const auto& slot : dmcSampleBank)
    {
        const auto participates = slot.included && activeIndex < 32;
        entries.push_back({ slot.name, slot.path, static_cast<int>(slot.bytes.size()), slot.included, participates });
        if (slot.included)
            ++activeIndex;
    }

    return entries;
}

std::vector<ChipperAudioProcessor::DmcSampleEntryInfo> ChipperAudioProcessor::spc700BrrSampleEntryInfo() const
{
    std::vector<DmcSampleEntryInfo> entries;
    const std::lock_guard<std::mutex> lock(spc700SampleMutex);
    entries.reserve(spc700BrrSampleBank.empty() && ! spc700BrrSample.bytes.empty() ? 1u : spc700BrrSampleBank.size());

    const auto appendEntry = [&entries](const DmcSampleSlot& slot, int& activeIndex)
    {
        const auto participates = slot.included && activeIndex < 32;
        entries.push_back({ slot.name, slot.path, static_cast<int>(slot.bytes.size()), slot.included, participates });
        if (slot.included)
            ++activeIndex;
    };

    int activeIndex = 0;
    if (spc700BrrSampleBank.empty())
    {
        if (! spc700BrrSample.bytes.empty())
            appendEntry(spc700BrrSample, activeIndex);
        return entries;
    }

    for (const auto& slot : spc700BrrSampleBank)
        appendEntry(slot, activeIndex);

    return entries;
}

std::vector<ChipperAudioProcessor::DmcSampleEntryInfo> ChipperAudioProcessor::paulaSampleEntryInfo() const
{
    std::vector<DmcSampleEntryInfo> entries;
    const std::lock_guard<std::mutex> lock(paulaSampleMutex);
    entries.reserve(paulaSampleBank.empty() && ! paulaSample.bytes.empty() ? 1u : paulaSampleBank.size());

    const auto appendEntry = [&entries](const DmcSampleSlot& slot, int& activeIndex)
    {
        const auto participates = slot.included && activeIndex < 32;
        entries.push_back({ slot.name, slot.path, static_cast<int>(slot.bytes.size()), slot.included, participates });
        if (slot.included)
            ++activeIndex;
    };

    int activeIndex = 0;
    if (paulaSampleBank.empty())
    {
        if (! paulaSample.bytes.empty())
            appendEntry(paulaSample, activeIndex);
        return entries;
    }

    for (const auto& slot : paulaSampleBank)
        appendEntry(slot, activeIndex);

    return entries;
}

void ChipperAudioProcessor::setNesDmcSampleIncluded(int index, bool shouldBeIncluded)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        if (index >= 0 && index < static_cast<int>(dmcSampleBank.size()))
        {
            auto& slot = dmcSampleBank[static_cast<size_t>(index)];
            if (slot.included != shouldBeIncluded)
            {
                slot.included = shouldBeIncluded;
                changed = true;
                ++dmcSampleBankRevision;
            }
        }
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
}

void ChipperAudioProcessor::setSpc700BrrSampleIncluded(int index, bool shouldBeIncluded)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        if (index >= 0 && index < static_cast<int>(spc700BrrSampleBank.size()))
        {
            auto& slot = spc700BrrSampleBank[static_cast<size_t>(index)];
            if (slot.included != shouldBeIncluded)
            {
                slot.included = shouldBeIncluded;
                changed = true;
                ++spc700BrrSampleBankRevision;
            }
        }
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activeSpc700BrrManualSlot = -1;
    applySpc700BrrSampleToCore();
}

void ChipperAudioProcessor::setPaulaSampleIncluded(int index, bool shouldBeIncluded)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        if (index >= 0 && index < static_cast<int>(paulaSampleBank.size()))
        {
            auto& slot = paulaSampleBank[static_cast<size_t>(index)];
            if (slot.included != shouldBeIncluded)
            {
                slot.included = shouldBeIncluded;
                changed = true;
                ++paulaSampleBankRevision;
            }
        }
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
}

void ChipperAudioProcessor::selectFirstNesDmcSamples(int maxCount)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        const auto clampedCount = std::clamp(maxCount, 0, 32);
        for (size_t i = 0; i < dmcSampleBank.size(); ++i)
        {
            const auto shouldBeIncluded = static_cast<int>(i) < clampedCount;
            if (dmcSampleBank[i].included != shouldBeIncluded)
            {
                dmcSampleBank[i].included = shouldBeIncluded;
                changed = true;
            }
        }

        if (changed)
            ++dmcSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
}

void ChipperAudioProcessor::selectFirstSpc700BrrSamples(int maxCount)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        const auto clampedCount = std::clamp(maxCount, 0, 32);
        for (size_t i = 0; i < spc700BrrSampleBank.size(); ++i)
        {
            const auto shouldBeIncluded = static_cast<int>(i) < clampedCount;
            if (spc700BrrSampleBank[i].included != shouldBeIncluded)
            {
                spc700BrrSampleBank[i].included = shouldBeIncluded;
                changed = true;
            }
        }

        if (changed)
            ++spc700BrrSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activeSpc700BrrManualSlot = -1;
    applySpc700BrrSampleToCore();
}

void ChipperAudioProcessor::selectFirstPaulaSamples(int maxCount)
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        const auto clampedCount = std::clamp(maxCount, 0, 32);
        for (size_t i = 0; i < paulaSampleBank.size(); ++i)
        {
            const auto shouldBeIncluded = static_cast<int>(i) < clampedCount;
            if (paulaSampleBank[i].included != shouldBeIncluded)
            {
                paulaSampleBank[i].included = shouldBeIncluded;
                changed = true;
            }
        }

        if (changed)
            ++paulaSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
}

void ChipperAudioProcessor::clearNesDmcSampleSelection()
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        for (auto& slot : dmcSampleBank)
        {
            if (slot.included)
            {
                slot.included = false;
                changed = true;
            }
        }

        if (changed)
            ++dmcSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
}

void ChipperAudioProcessor::clearSpc700BrrSampleSelection()
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        for (auto& slot : spc700BrrSampleBank)
        {
            if (slot.included)
            {
                slot.included = false;
                changed = true;
            }
        }

        if (changed)
            ++spc700BrrSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activeSpc700BrrManualSlot = -1;
    applySpc700BrrSampleToCore();
}

void ChipperAudioProcessor::clearPaulaSampleSelection()
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        for (auto& slot : paulaSampleBank)
        {
            if (slot.included)
            {
                slot.included = false;
                changed = true;
            }
        }

        if (changed)
            ++paulaSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
}

void ChipperAudioProcessor::invertNesDmcSampleSelection()
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        for (auto& slot : dmcSampleBank)
        {
            slot.included = ! slot.included;
            changed = true;
        }

        if (changed)
            ++dmcSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
}

void ChipperAudioProcessor::invertSpc700BrrSampleSelection()
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        for (auto& slot : spc700BrrSampleBank)
        {
            slot.included = ! slot.included;
            changed = true;
        }

        if (changed)
            ++spc700BrrSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activeSpc700BrrManualSlot = -1;
    applySpc700BrrSampleToCore();
}

void ChipperAudioProcessor::invertPaulaSampleSelection()
{
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        for (auto& slot : paulaSampleBank)
        {
            slot.included = ! slot.included;
            changed = true;
        }

        if (changed)
            ++paulaSampleBankRevision;
    }

    if (! changed)
        return;

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
}

uint64_t ChipperAudioProcessor::nesDmcSampleRevision() const
{
    const std::lock_guard<std::mutex> lock(dmcSampleMutex);
    return dmcSampleBankRevision;
}

uint64_t ChipperAudioProcessor::spc700BrrSampleRevision() const
{
    const std::lock_guard<std::mutex> lock(spc700SampleMutex);
    return spc700BrrSampleBankRevision;
}

uint64_t ChipperAudioProcessor::paulaSampleRevision() const
{
    const std::lock_guard<std::mutex> lock(paulaSampleMutex);
    return paulaSampleBankRevision;
}

bool ChipperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void ChipperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    ensureCore();

    const auto readOutputGain = [this]
    {
        const auto outputDb = apvts.getRawParameterValue(chipper::parameters::id::outputDb)->load();
        return juce::Decibels::decibelsToGain(outputDb);
    };

    int renderedUntil = 0;
    for (const auto metadata : midiMessages)
    {
        const auto position = std::clamp(metadata.samplePosition, 0, buffer.getNumSamples());
        renderRange(buffer, renderedUntil, position, readOutputGain());
        handleMidiMessage(metadata.getMessage());
        renderedUntil = position;
    }

    renderRange(buffer, renderedUntil, buffer.getNumSamples(), readOutputGain());
}

void ChipperAudioProcessor::renderRange(juce::AudioBuffer<float>& buffer, int startSample, int endSample, float outputGain)
{
    if (core == nullptr || endSample <= startSample)
        return;

    const auto channels = buffer.getNumChannels();
    for (int sample = startSample; sample < endSample; ++sample)
    {
        const auto frame = core->renderSample();
        const auto left = juce::jlimit(-1.0f, 1.0f, frame.left * outputGain);
        const auto right = juce::jlimit(-1.0f, 1.0f, frame.right * outputGain);

        if (channels > 0)
            buffer.setSample(0, sample, left);
        if (channels > 1)
            buffer.setSample(1, sample, right);

        pushOutputScopeSample((left + right) * 0.5f);
    }
}

void ChipperAudioProcessor::pushOutputScopeSample(float sample) noexcept
{
    const auto index = outputScopeWriteIndex.fetch_add(1u, std::memory_order_relaxed);
    outputScopeBuffer[index % outputScopeSampleCount].store(juce::jlimit(-1.0f, 1.0f, sample), std::memory_order_relaxed);
}

void ChipperAudioProcessor::handleMidiMessage(const juce::MidiMessage& message)
{
    if (core == nullptr)
        return;

    if (message.isNoteOn())
    {
        const auto note = message.getNoteNumber();
        const auto velocity = message.getFloatVelocity();
        rememberHeldNote(note, velocity);
        applyMappedDmcSampleForMidiNote(note);
        applyMappedSpc700BrrSampleForMidiNote(note);
        applyMappedPaulaSampleForMidiNote(note);
        core->noteOn(note, velocity);
    }
    else if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        core->noteOff(note);
        forgetHeldNote(note);
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        for (const auto held : heldNotes)
            core->noteOff(held.note);
        heldNotes.clear();
    }
    else if (message.isController() && handleMidiController(message))
    {
        ensureCore();
    }
}

bool ChipperAudioProcessor::handleMidiController(const juce::MidiMessage& message)
{
    const auto* parameterId = chipper::parameters::parameterIdForMidiController(message.getControllerNumber());
    if (parameterId == nullptr)
        return false;

    return setParameterFromMidiCc(parameterId, message.getControllerValue());
}

bool ChipperAudioProcessor::setParameterFromMidiCc(const char* parameterId, int controllerValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        const auto normalized = juce::jlimit(0.0f, 1.0f, static_cast<float>(controllerValue) / 127.0f);
        parameter->setValueNotifyingHost(normalized);

        if (std::string_view(parameterId) == chipper::parameters::id::macro)
            applyCurrentMacroTemplateToParameters();

        return true;
    }

    return false;
}

bool ChipperAudioProcessor::setPlainParameterValue(const char* parameterId, float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        return true;
    }

    return false;
}

void ChipperAudioProcessor::applyCurrentMacroTemplateToParameters()
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);

    if (! chipper::descriptorFor(mode).implemented)
        return;

    const auto& templ = chipper::macroTemplateFor(mode, chipper::parameters::macroFromChoice(macroChoice));
    const std::array<const char*, 4> controlIds {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };
    const std::array<const char*, 4> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled
    };
    const std::array<const char*, 4> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level
    };

    for (size_t i = 0; i < controlIds.size(); ++i)
        setPlainParameterValue(controlIds[i], templ.controls[i]);
    for (size_t i = 0; i < sourceIds.size(); ++i)
        setPlainParameterValue(sourceIds[i], templ.sourceEnabled[i] ? 1.0f : 0.0f);
    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setPlainParameterValue(sourceLevelIds[i], 1.0f);
    const auto anyTemplateSourceEnabled = std::any_of(templ.sourceEnabled.begin(), templ.sourceEnabled.end(), [](bool enabled) { return enabled; });
    setPlainParameterValue(chipper::parameters::id::source5Enabled, anyTemplateSourceEnabled && chipper::nativeSourceCountForMode(mode) >= 5u ? 1.0f : 0.0f);
    setPlainParameterValue(chipper::parameters::id::source6Enabled, anyTemplateSourceEnabled && chipper::nativeSourceCountForMode(mode) >= 6u ? 1.0f : 0.0f);
    setPlainParameterValue(chipper::parameters::id::source7Enabled, anyTemplateSourceEnabled && chipper::nativeSourceCountForMode(mode) >= 7u ? 1.0f : 0.0f);
    setPlainParameterValue(chipper::parameters::id::source8Enabled, anyTemplateSourceEnabled && chipper::nativeSourceCountForMode(mode) >= 8u ? 1.0f : 0.0f);
    setPlainParameterValue(chipper::parameters::id::source9Enabled, anyTemplateSourceEnabled && chipper::nativeSourceCountForMode(mode) >= 9u ? 1.0f : 0.0f);
    setPlainParameterValue(chipper::parameters::id::source5Level, 1.0f);
    setPlainParameterValue(chipper::parameters::id::source6Level, 1.0f);
    setPlainParameterValue(chipper::parameters::id::source7Level, 1.0f);
    setPlainParameterValue(chipper::parameters::id::source8Level, 1.0f);
    setPlainParameterValue(chipper::parameters::id::source9Level, 1.0f);

    setPlainParameterValue(chipper::parameters::id::envelopeDecay, templ.envelopeDecay);
    setPlainParameterValue(chipper::parameters::id::sidAttack, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidDecay, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidSustain, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidRelease, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice2Attack, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice2Decay, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice2Sustain, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice2Release, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice3Attack, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice3Decay, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice3Sustain, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice3Release, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidFilterRouting, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice2PulseWidth, templ.controls[0]);
    setPlainParameterValue(chipper::parameters::id::sidVoice3PulseWidth, templ.controls[0]);
    setPlainParameterValue(chipper::parameters::id::stereoSpread, templ.stereoSpread);
    setPlainParameterValue(chipper::parameters::id::waveShape, static_cast<float>(templ.waveShape));
    setPlainParameterValue(chipper::parameters::id::sidVoice2WaveShape, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice3WaveShape, 0.0f);
    setPlainParameterValue(chipper::parameters::id::pulse2Duty, 0.0f);
    setPlainParameterValue(chipper::parameters::id::dmgWaveLevel, 0.0f);
    setPlainParameterValue(chipper::parameters::id::dmgStereoRoute, static_cast<float>(templ.dmgStereoRoute));
    setPlainParameterValue(chipper::parameters::id::ymEnvelopeShape, static_cast<float>(templ.ymEnvelopeShape));
    setPlainParameterValue(chipper::parameters::id::ymChannelAMix, 0.0f);
    setPlainParameterValue(chipper::parameters::id::ymChannelBMix, 0.0f);
    setPlainParameterValue(chipper::parameters::id::ymChannelCMix, 0.0f);
    setPlainParameterValue(chipper::parameters::id::snNoiseMode, static_cast<float>(templ.snNoiseMode));
    setPlainParameterValue(chipper::parameters::id::nesDmcDirectLevel, templ.nesDmcDirectLevel);
    setPlainParameterValue(chipper::parameters::id::nesDmcRateIndex, 15.0f);
    setPlainParameterValue(chipper::parameters::id::nesDmcPlaybackMode, 0.0f);
    setPlainParameterValue(chipper::parameters::id::nesDmcMapRoot, 36.0f);
    setPlainParameterValue(chipper::parameters::id::nesDmcLoop, 0.0f);
}

void ChipperAudioProcessor::synchronizeMacroTemplateFromParameters()
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    auto patch = currentPatchFromParameters();

    if (! hasObservedMacroSnapshot)
    {
        lastObservedMacroModeChoice = modeChoice;
        lastObservedMacroChoice = macroChoice;
        lastObservedMacroPatch = patch;
        hasObservedMacroSnapshot = true;
        return;
    }

    const auto changedTemplateSelector = modeChoice != lastObservedMacroModeChoice || macroChoice != lastObservedMacroChoice;
    if (changedTemplateSelector && patchControlsMatch(patch, lastObservedMacroPatch))
    {
        applyCurrentMacroTemplateToParameters();
        patch = currentPatchFromParameters();
    }

    lastObservedMacroModeChoice = modeChoice;
    lastObservedMacroChoice = macroChoice;
    lastObservedMacroPatch = patch;
}

void ChipperAudioProcessor::ensureCore()
{
    synchronizeMacroTemplateFromParameters();

    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto accuracyChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::accuracy)->load()));
    const auto selectedMode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto selectedAccuracy = chipper::parameters::accuracyFromChoice(accuracyChoice);
    const auto clockOverride = apvts.getRawParameterValue(chipper::parameters::id::clockHz)->load();
    const auto selectedClock = clockOverride > 0.0f ? static_cast<double>(clockOverride) : chipper::parameters::defaultClockForMode(selectedMode);
    const auto selectedPatch = currentPatchFromParameters();

    if (core != nullptr && selectedMode == activeMode && selectedAccuracy == activeAccuracy && std::abs(selectedClock - activeClock) < 0.5)
    {
        if (! patchMatches(selectedPatch, activePatch))
        {
            activePatch = selectedPatch;
            core->setPatch(activePatch);
            replayHeldNotes();
        }

        applySelectedDmcSampleToCore();
        applySpc700BrrSampleToCore();
        applyPaulaSampleToCore();
        return;
    }

    activeMode = selectedMode;
    activeAccuracy = selectedAccuracy;
    activeClock = selectedClock;
    activePatch = selectedPatch;
    core = chipper::createChipCore(activeMode, activeAccuracy);
    core->reset(currentSampleRate, activeClock);
    core->setPatch(activePatch);
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    activeDmcSampleSlot = -1;
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    applySelectedDmcSampleToCore();
    applySpc700BrrSampleToCore();
    applyPaulaSampleToCore();
    replayPendingRegisterState();
    replayHeldNotes();
}

void ChipperAudioProcessor::applySelectedDmcSampleToCore()
{
    if (core == nullptr || activeMode != chipper::ChipMode::nes)
        return;

    const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
    const auto playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    applyDmcSampleSlotToCore(playbackMode != 0 && activeDmcSampleSlot >= 0 ? activeDmcSampleSlot : selectedSlot);
}

void ChipperAudioProcessor::applyDmcSampleSlotToCore(int requestedSlot)
{
    if (core == nullptr || activeMode != chipper::ChipMode::nes)
        return;

    std::vector<uint8_t> selectedBytes;
    uint64_t revision = 0;
    auto resolvedSlot = -1;
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        revision = dmcSampleBankRevision;
        if (requestedSlot == unmappedDmcSampleSlot)
        {
            resolvedSlot = unmappedDmcSampleSlot;
        }
        else if (! dmcSampleBank.empty())
        {
            std::vector<const DmcSampleSlot*> activeSlots;
            activeSlots.reserve(32u);
            for (const auto& slot : dmcSampleBank)
            {
                if (slot.included)
                    activeSlots.push_back(&slot);
                if (activeSlots.size() >= 32u)
                    break;
            }

            if (! activeSlots.empty())
            {
                resolvedSlot = std::clamp(requestedSlot, 0, static_cast<int>(activeSlots.size() - 1u));
                const auto safeSlot = static_cast<size_t>(resolvedSlot);
                selectedBytes = activeSlots[safeSlot]->bytes;
            }
        }
    }

    if (revision == activeDmcSampleBankRevision && resolvedSlot == activeDmcSampleSlot)
        return;

    activeDmcSampleBankRevision = revision;
    activeDmcSampleSlot = resolvedSlot;
    core->setExternalSampleData(std::move(selectedBytes));
}

void ChipperAudioProcessor::applyMappedDmcSampleForMidiNote(int midiNote)
{
    if (core == nullptr || activeMode != chipper::ChipMode::nes)
        return;

    const auto playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    if (playbackMode == 0)
        return;

    auto activeSlotCount = 0;
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        for (const auto& slot : dmcSampleBank)
        {
            if (slot.included)
                ++activeSlotCount;
            if (activeSlotCount >= 32)
                break;
        }
    }

    if (activeSlotCount <= 0)
        return;

    const auto mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    const auto mappedSlot = midiNote - mapRootNote;
    if (mappedSlot < 0 || mappedSlot >= activeSlotCount)
    {
        applyDmcSampleSlotToCore(unmappedDmcSampleSlot);
        return;
    }

    applyDmcSampleSlotToCore(mappedSlot);
}

void ChipperAudioProcessor::applySpc700BrrSampleToCore()
{
    if (core == nullptr || activeMode != chipper::ChipMode::spc700)
        return;

    std::vector<std::vector<uint8_t>> sampleBank;
    uint64_t revision = 0;
    auto resolvedSlot = -1;
    const auto playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        revision = spc700BrrSampleBankRevision;
        if (! spc700BrrSampleBank.empty())
        {
            std::vector<const DmcSampleSlot*> activeSlots;
            activeSlots.reserve(32u);
            for (const auto& slot : spc700BrrSampleBank)
            {
                if (slot.included)
                    activeSlots.push_back(&slot);
                if (activeSlots.size() >= 32u)
                    break;
            }

            if (activeSlots.empty())
            {
                spc700BrrSample = {};
                resolvedSlot = -1;
            }
            else
            {
                const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
                resolvedSlot = std::clamp(selectedSlot, 0, static_cast<int>(activeSlots.size() - 1u));
                spc700BrrSample = *activeSlots[static_cast<size_t>(resolvedSlot)];
                sampleBank.reserve(activeSlots.size());
                for (const auto* slot : activeSlots)
                    sampleBank.push_back(slot->bytes);
            }
        }
        else if (! spc700BrrSample.bytes.empty())
        {
            resolvedSlot = 0;
            sampleBank.push_back(spc700BrrSample.bytes);
        }
    }

    if (revision == activeSpc700BrrSampleRevision)
    {
        if (resolvedSlot < 0)
            return;

        activeSpc700BrrManualSlot = resolvedSlot;
        if (playbackMode != 0 || resolvedSlot == activeSpc700BrrSampleSlot)
            return;

        activeSpc700BrrSampleSlot = resolvedSlot;
        core->setExternalSampleSlot(resolvedSlot);
        return;
    }

    activeSpc700BrrSampleRevision = revision;
    activeSpc700BrrSampleSlot = resolvedSlot;
    activeSpc700BrrManualSlot = resolvedSlot;
    core->setExternalSampleBank(std::move(sampleBank), resolvedSlot);
}

void ChipperAudioProcessor::applySpc700BrrSampleSlotToCore(int requestedSlot)
{
    if (core == nullptr || activeMode != chipper::ChipMode::spc700)
        return;

    uint64_t revision = 0;
    auto resolvedSlot = -1;
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        revision = spc700BrrSampleBankRevision;
        if (! spc700BrrSampleBank.empty())
        {
            std::vector<const DmcSampleSlot*> activeSlots;
            activeSlots.reserve(32u);
            for (const auto& slot : spc700BrrSampleBank)
            {
                if (slot.included)
                    activeSlots.push_back(&slot);
                if (activeSlots.size() >= 32u)
                    break;
            }

            if (requestedSlot >= 0)
            {
                if (! activeSlots.empty())
                {
                    resolvedSlot = std::clamp(requestedSlot, 0, static_cast<int>(activeSlots.size() - 1u));
                    spc700BrrSample = *activeSlots[static_cast<size_t>(resolvedSlot)];
                }
                else
                {
                    spc700BrrSample = {};
                }
            }
        }
        else if (! spc700BrrSample.bytes.empty())
        {
            resolvedSlot = requestedSlot >= 0 ? 0 : -1;
        }
    }

    if (revision != activeSpc700BrrSampleRevision)
    {
        activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
        applySpc700BrrSampleToCore();
    }

    if (resolvedSlot < 0)
    {
        if (activeSpc700BrrSampleSlot == -1)
            return;

        activeSpc700BrrSampleSlot = -1;
        core->setExternalSampleSlot(-1);
        return;
    }

    if (resolvedSlot == activeSpc700BrrSampleSlot)
        return;

    activeSpc700BrrSampleSlot = resolvedSlot;
    core->setExternalSampleSlot(resolvedSlot);
}

void ChipperAudioProcessor::applyMappedSpc700BrrSampleForMidiNote(int midiNote)
{
    if (core == nullptr || activeMode != chipper::ChipMode::spc700)
        return;

    auto activeSlotCount = 0;
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        if (spc700BrrSampleBank.empty())
        {
            activeSlotCount = spc700BrrSample.bytes.empty() ? 0 : 1;
        }
        else
        {
            for (const auto& slot : spc700BrrSampleBank)
            {
                if (slot.included)
                    ++activeSlotCount;
                if (activeSlotCount >= 32)
                    break;
            }
        }
    }

    if (activeSlotCount <= 1)
        return;

    const auto playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    if (playbackMode == 0)
        return;

    const auto mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    const auto mappedSlot = midiNote - mapRootNote;
    if (mappedSlot < 0 || mappedSlot >= activeSlotCount)
    {
        applySpc700BrrSampleSlotToCore(-1);
        return;
    }

    applySpc700BrrSampleSlotToCore(mappedSlot);
}

void ChipperAudioProcessor::applyPaulaSampleToCore()
{
    if (core == nullptr || activeMode != chipper::ChipMode::paula)
        return;

    std::vector<std::vector<uint8_t>> sampleBank;
    uint64_t revision = 0;
    auto resolvedSlot = -1;
    const auto playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        revision = paulaSampleBankRevision;
        if (! paulaSampleBank.empty())
        {
            std::vector<const DmcSampleSlot*> activeSlots;
            activeSlots.reserve(32u);
            for (const auto& slot : paulaSampleBank)
            {
                if (slot.included)
                    activeSlots.push_back(&slot);
                if (activeSlots.size() >= 32u)
                    break;
            }

            if (activeSlots.empty())
            {
                paulaSample = {};
                resolvedSlot = -1;
            }
            else
            {
                const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
                resolvedSlot = std::clamp(selectedSlot, 0, static_cast<int>(activeSlots.size() - 1u));
                paulaSample = *activeSlots[static_cast<size_t>(resolvedSlot)];
                sampleBank.reserve(activeSlots.size());
                for (const auto* slot : activeSlots)
                    sampleBank.push_back(slot->bytes);
            }
        }
        else if (! paulaSample.bytes.empty())
        {
            resolvedSlot = 0;
            sampleBank.push_back(paulaSample.bytes);
        }
    }

    if (revision == activePaulaSampleRevision)
    {
        if (resolvedSlot < 0)
            return;

        activePaulaManualSlot = resolvedSlot;
        if (playbackMode != 0 || resolvedSlot == activePaulaSampleSlot)
            return;

        activePaulaSampleSlot = resolvedSlot;
        core->setExternalSampleSlot(resolvedSlot);
        return;
    }

    activePaulaSampleRevision = revision;
    activePaulaSampleSlot = resolvedSlot;
    activePaulaManualSlot = resolvedSlot;
    core->setExternalSampleBank(std::move(sampleBank), resolvedSlot);
}

void ChipperAudioProcessor::applyPaulaSampleSlotToCore(int requestedSlot)
{
    if (core == nullptr || activeMode != chipper::ChipMode::paula)
        return;

    uint64_t revision = 0;
    auto resolvedSlot = -1;
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        revision = paulaSampleBankRevision;
        if (! paulaSampleBank.empty())
        {
            std::vector<const DmcSampleSlot*> activeSlots;
            activeSlots.reserve(32u);
            for (const auto& slot : paulaSampleBank)
            {
                if (slot.included)
                    activeSlots.push_back(&slot);
                if (activeSlots.size() >= 32u)
                    break;
            }

            if (requestedSlot >= 0)
            {
                if (! activeSlots.empty())
                {
                    resolvedSlot = std::clamp(requestedSlot, 0, static_cast<int>(activeSlots.size() - 1u));
                    paulaSample = *activeSlots[static_cast<size_t>(resolvedSlot)];
                }
                else
                {
                    paulaSample = {};
                }
            }
        }
        else if (! paulaSample.bytes.empty())
        {
            resolvedSlot = requestedSlot >= 0 ? 0 : -1;
        }
    }

    if (revision != activePaulaSampleRevision)
    {
        activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
        applyPaulaSampleToCore();
    }

    if (resolvedSlot < 0)
    {
        if (activePaulaSampleSlot == -1)
            return;

        activePaulaSampleSlot = -1;
        core->setExternalSampleSlot(-1);
        return;
    }

    if (resolvedSlot == activePaulaSampleSlot)
        return;

    activePaulaSampleSlot = resolvedSlot;
    core->setExternalSampleSlot(resolvedSlot);
}

void ChipperAudioProcessor::applyMappedPaulaSampleForMidiNote(int midiNote)
{
    if (core == nullptr || activeMode != chipper::ChipMode::paula)
        return;

    auto activeSlotCount = 0;
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        if (paulaSampleBank.empty())
        {
            activeSlotCount = paulaSample.bytes.empty() ? 0 : 1;
        }
        else
        {
            for (const auto& slot : paulaSampleBank)
            {
                if (slot.included)
                    ++activeSlotCount;
                if (activeSlotCount >= 32)
                    break;
            }
        }
    }

    if (activeSlotCount <= 1)
        return;

    const auto playbackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    if (playbackMode == 0)
        return;

    const auto mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    const auto mappedSlot = midiNote - mapRootNote;
    if (mappedSlot < 0 || mappedSlot >= activeSlotCount)
    {
        applyPaulaSampleSlotToCore(-1);
        return;
    }

    applyPaulaSampleSlotToCore(mappedSlot);
}

chipper::PatchConfig ChipperAudioProcessor::currentPatchFromParameters() const
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    const auto playModeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::playMode)->load()));

    const auto selectedMode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto dmcPlaybackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));

    return chipper::makePatchConfig(
        selectedMode,
        chipper::parameters::macroFromChoice(macroChoice),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl1)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl2)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl3)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl4)->load(),
        chipper::parameters::playModeFromChoice(playModeChoice),
        {
            apvts.getRawParameterValue(chipper::parameters::id::source1Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source2Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source3Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source4Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source5Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source6Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source7Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source8Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source9Enabled)->load() >= 0.5f
        },
        {
            apvts.getRawParameterValue(chipper::parameters::id::source1Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source2Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source3Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source4Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source5Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source6Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source7Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source8Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source9Level)->load()
        },
        apvts.getRawParameterValue(chipper::parameters::id::stereoSpread)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::envelopeDecay)->load(),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::waveShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::pulse2Duty)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::dmgWaveLevel)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::dmgStereoRoute)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::ymEnvelopeShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::ymChannelAMix)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::ymChannelBMix)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::ymChannelCMix)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::snNoiseMode)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice2WaveShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice3WaveShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidAttack)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidDecay)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidSustain)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidRelease)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice2Attack)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice2Decay)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice2Sustain)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice2Release)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice3Attack)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice3Decay)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice3Sustain)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice3Release)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidFilterRouting)->load())),
        apvts.getRawParameterValue(chipper::parameters::id::sidVoice2PulseWidth)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::sidVoice3PulseWidth)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::nesDmcDirectLevel)->load(),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcRateIndex)->load())),
        apvts.getRawParameterValue(chipper::parameters::id::nesDmcLoop)->load() >= 0.5f,
        selectedMode == chipper::ChipMode::nes && dmcPlaybackMode == 2);
}

void ChipperAudioProcessor::replayPendingRegisterState()
{
    if (core == nullptr || pendingRegisterState.empty())
        return;

    for (const auto& write : pendingRegisterState)
        core->writeRegister(write.address, write.value);

    pendingRegisterState.clear();
}

void ChipperAudioProcessor::replayHeldNotes()
{
    if (core == nullptr)
        return;

    for (const auto held : heldNotes)
        core->noteOn(held.note, held.velocity);
}

void ChipperAudioProcessor::rememberHeldNote(int note, float velocity)
{
    const auto existing = std::find_if(heldNotes.begin(), heldNotes.end(), [note](const auto held) { return held.note == note; });
    if (existing != heldNotes.end())
    {
        existing->velocity = velocity;
        return;
    }

    heldNotes.push_back({ note, velocity });
}

void ChipperAudioProcessor::forgetHeldNote(int note)
{
    heldNotes.erase(std::remove_if(heldNotes.begin(), heldNotes.end(), [note](const auto held) { return held.note == note; }),
                    heldNotes.end());
}

juce::AudioProcessorEditor* ChipperAudioProcessor::createEditor()
{
    return new ChipperAudioProcessorEditor(*this);
}

void ChipperAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml(state.createXml());
    while (auto* existingCoreState = xml->getChildByName(coreStateTag))
        xml->removeChildElement(existingCoreState, true);
    while (auto* existingDmcBankState = xml->getChildByName(dmcBankStateTag))
        xml->removeChildElement(existingDmcBankState, true);
    while (auto* existingSpcBrrState = xml->getChildByName(spc700BrrStateTag))
        xml->removeChildElement(existingSpcBrrState, true);
    while (auto* existingSpcBrrBankState = xml->getChildByName(spc700BrrBankStateTag))
        xml->removeChildElement(existingSpcBrrBankState, true);
    while (auto* existingPaulaSampleBankState = xml->getChildByName(paulaSampleBankStateTag))
        xml->removeChildElement(existingPaulaSampleBankState, true);

    if (core != nullptr)
    {
        auto* coreState = new juce::XmlElement(coreStateTag);
        coreState->setAttribute("mode", core->modeName());
        coreState->setAttribute("implementedAccuracy", core->implementedAccuracy());

        const auto writes = core->exportRegisterState();
        coreState->setAttribute("count", static_cast<int>(writes.size()));
        for (const auto& write : writes)
        {
            auto* reg = new juce::XmlElement(registerTag);
            reg->setAttribute("address", static_cast<int>(write.address));
            reg->setAttribute("value", static_cast<int>(write.value));
            coreState->addChildElement(reg);
        }

        xml->addChildElement(coreState);
    }

    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        if (! dmcSampleBank.empty())
        {
            auto* dmcBankState = new juce::XmlElement(dmcBankStateTag);
            dmcBankState->setAttribute("count", static_cast<int>(dmcSampleBank.size()));
            for (const auto& slot : dmcSampleBank)
            {
                auto* sample = new juce::XmlElement(dmcSampleStateTag);
                sample->setAttribute("path", slot.path);
                sample->setAttribute("included", slot.included ? 1 : 0);
                dmcBankState->addChildElement(sample);
            }

            xml->addChildElement(dmcBankState);
        }
    }

    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        if (! spc700BrrSampleBank.empty())
        {
            auto* spcBrrBankState = new juce::XmlElement(spc700BrrBankStateTag);
            spcBrrBankState->setAttribute("count", static_cast<int>(spc700BrrSampleBank.size()));
            for (const auto& slot : spc700BrrSampleBank)
            {
                auto* sample = new juce::XmlElement(spc700BrrSampleStateTag);
                sample->setAttribute("path", slot.path);
                sample->setAttribute("included", slot.included ? 1 : 0);
                spcBrrBankState->addChildElement(sample);
            }

            xml->addChildElement(spcBrrBankState);
        }
        else if (! spc700BrrSample.bytes.empty())
        {
            auto* spcBrrState = new juce::XmlElement(spc700BrrStateTag);
            spcBrrState->setAttribute("path", spc700BrrSample.path);
            xml->addChildElement(spcBrrState);
        }
    }

    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        if (! paulaSampleBank.empty())
        {
            auto* paulaBankState = new juce::XmlElement(paulaSampleBankStateTag);
            paulaBankState->setAttribute("count", static_cast<int>(paulaSampleBank.size()));
            for (const auto& slot : paulaSampleBank)
            {
                auto* sample = new juce::XmlElement(paulaSampleStateTag);
                sample->setAttribute("path", slot.path);
                sample->setAttribute("included", slot.included ? 1 : 0);
                paulaBankState->addChildElement(sample);
            }

            xml->addChildElement(paulaBankState);
        }
    }

    copyXmlToBinary(*xml, destData);
}

void ChipperAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        pendingRegisterState.clear();
        std::vector<DmcSampleSlot> restoredDmcBank;
        DmcSampleSlot restoredSpcBrrSample;
        std::vector<DmcSampleSlot> restoredSpcBrrBank;
        DmcSampleSlot restoredPaulaSample;
        std::vector<DmcSampleSlot> restoredPaulaBank;
        if (auto* coreState = xml->getChildByName(coreStateTag))
        {
            for (const auto* child : coreState->getChildIterator())
            {
                if (child != nullptr && child->hasTagName(registerTag))
                {
                    pendingRegisterState.push_back({
                        0,
                        static_cast<uint16_t>(child->getIntAttribute("address") & 0xffff),
                        static_cast<uint8_t>(child->getIntAttribute("value") & 0xff)
                    });
                }
            }
            xml->removeChildElement(coreState, true);
        }

        if (auto* dmcBankState = xml->getChildByName(dmcBankStateTag))
        {
            restoredDmcBank.reserve(static_cast<size_t>(std::max(0, dmcBankState->getIntAttribute("count"))));
            for (const auto* child : dmcBankState->getChildIterator())
            {
                if (child == nullptr || ! child->hasTagName(dmcSampleStateTag))
                    continue;

                DmcSampleSlot slot;
                if (readDmcSampleFile(juce::File(child->getStringAttribute("path")), slot).wasOk())
                {
                    slot.included = child->getBoolAttribute("included", true);
                    restoredDmcBank.push_back(std::move(slot));
                }
            }
            xml->removeChildElement(dmcBankState, true);
        }

        if (auto* spcBrrBankState = xml->getChildByName(spc700BrrBankStateTag))
        {
            restoredSpcBrrBank.reserve(static_cast<size_t>(std::max(0, spcBrrBankState->getIntAttribute("count"))));
            for (const auto* child : spcBrrBankState->getChildIterator())
            {
                if (child == nullptr || ! child->hasTagName(spc700BrrSampleStateTag))
                    continue;

                DmcSampleSlot slot;
                if (readSpc700BrrSampleFile(juce::File(child->getStringAttribute("path")), slot).wasOk())
                {
                    slot.included = child->getBoolAttribute("included", true);
                    restoredSpcBrrBank.push_back(std::move(slot));
                }
            }
            if (! restoredSpcBrrBank.empty())
                restoredSpcBrrSample = restoredSpcBrrBank.front();
            xml->removeChildElement(spcBrrBankState, true);
        }

        if (restoredSpcBrrBank.empty())
        {
            if (auto* spcBrrState = xml->getChildByName(spc700BrrStateTag))
            {
                readSpc700BrrSampleFile(juce::File(spcBrrState->getStringAttribute("path")), restoredSpcBrrSample);
                if (! restoredSpcBrrSample.bytes.empty())
                    restoredSpcBrrBank.push_back(restoredSpcBrrSample);
                xml->removeChildElement(spcBrrState, true);
            }
        }

        if (auto* paulaBankState = xml->getChildByName(paulaSampleBankStateTag))
        {
            restoredPaulaBank.reserve(static_cast<size_t>(std::max(0, paulaBankState->getIntAttribute("count"))));
            for (const auto* child : paulaBankState->getChildIterator())
            {
                if (child == nullptr || ! child->hasTagName(paulaSampleStateTag))
                    continue;

                DmcSampleSlot slot;
                if (readPaulaSampleFile(juce::File(child->getStringAttribute("path")), slot).wasOk())
                {
                    slot.included = child->getBoolAttribute("included", true);
                    restoredPaulaBank.push_back(std::move(slot));
                }
            }
            if (! restoredPaulaBank.empty())
                restoredPaulaSample = restoredPaulaBank.front();
            xml->removeChildElement(paulaBankState, true);
        }

        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        {
            const std::lock_guard<std::mutex> lock(dmcSampleMutex);
            dmcSampleBank = std::move(restoredDmcBank);
            ++dmcSampleBankRevision;
        }
        {
            const std::lock_guard<std::mutex> lock(spc700SampleMutex);
            spc700BrrSampleBank = std::move(restoredSpcBrrBank);
            spc700BrrSample = std::move(restoredSpcBrrSample);
            ++spc700BrrSampleBankRevision;
        }
        {
            const std::lock_guard<std::mutex> lock(paulaSampleMutex);
            paulaSampleBank = std::move(restoredPaulaBank);
            paulaSample = std::move(restoredPaulaSample);
            ++paulaSampleBankRevision;
        }
        activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
        activeDmcSampleSlot = -1;
        activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
        activeSpc700BrrSampleSlot = -1;
        activeSpc700BrrManualSlot = -1;
        activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
        activePaulaSampleSlot = -1;
        activePaulaManualSlot = -1;
        core.reset();
        hasObservedMacroSnapshot = false;
    }
}

std::string ChipperAudioProcessor::currentCoreStatus() const
{
    if (core == nullptr)
        return "No core loaded";

    const auto& descriptor = chipper::descriptorFor(activeMode);
    return descriptor.displayName + " - " + descriptor.verification.badge + " - active";
}

std::string ChipperAudioProcessor::currentCoreStatusDetail() const
{
    if (core == nullptr)
        return "No core loaded";

    const auto& descriptor = chipper::descriptorFor(activeMode);
    return descriptor.displayName + ": " + descriptor.verification.badge + ". "
        + descriptor.verification.summary + " "
        + descriptor.summary + " "
        + core->limitations();
}

ChipperAudioProcessor::OutputScopeSnapshot ChipperAudioProcessor::outputScopeSnapshot() const
{
    OutputScopeSnapshot snapshot {};
    const auto writeIndex = outputScopeWriteIndex.load(std::memory_order_acquire);
    const auto validSampleCount = std::min(writeIndex, outputScopeSampleCount);
    const auto padding = outputScopeSampleCount - validSampleCount;

    for (size_t i = 0; i < validSampleCount; ++i)
    {
        const auto logicalIndex = writeIndex - validSampleCount + i;
        snapshot[padding + i] = outputScopeBuffer[logicalIndex % outputScopeSampleCount].load(std::memory_order_relaxed);
    }

    return snapshot;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipperAudioProcessor();
}
