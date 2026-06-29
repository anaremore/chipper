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
constexpr auto opnaRhythmRomStateTag = "CHIPPER_OPNA_RHYTHM_ROM";
constexpr auto opnaAdpcmBSampleStateTag = "CHIPPER_OPNA_ADPCM_B_SAMPLE";
constexpr auto opnbAdpcmASampleStateTag = "CHIPPER_OPNB_ADPCM_A_SAMPLE";
constexpr auto opnbAdpcmBSampleStateTag = "CHIPPER_OPNB_ADPCM_B_SAMPLE";
constexpr auto unmappedDmcSampleSlot = -2;
constexpr auto opnaRhythmRomBytes = 8192;
constexpr auto opnaAdpcmBMemoryBytes = 262144;
constexpr auto opnbAdpcmAMemoryBytes = 1048576;
constexpr auto opnbAdpcmBMemoryBytes = 16777216;

juce::String midiNoteName(int note)
{
    static constexpr std::array<const char*, 12> names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto clampedNote = std::clamp(note, 0, 127);
    return juce::String(names[static_cast<size_t>(clampedNote % 12)]) + juce::String((clampedNote / 12) - 2);
}

int jsonIntValue(const std::string& json, const std::string& key, int fallback = 0)
{
    const auto marker = "\"" + key + "\":";
    const auto markerPosition = json.find(marker);
    if (markerPosition == std::string::npos)
        return fallback;

    const auto valueStart = markerPosition + marker.size();
    const auto valueEnd = json.find_first_of(",}", valueStart);
    if (valueEnd == std::string::npos)
        return fallback;

    try
    {
        return std::stoi(json.substr(valueStart, valueEnd - valueStart));
    }
    catch (...)
    {
        return fallback;
    }
}

std::vector<float> decodeDmcPreview(const std::vector<uint8_t>& bytes)
{
    std::vector<float> decoded;
    decoded.reserve(bytes.size() * 8u);
    auto level = 64;
    for (const auto byte : bytes)
    {
        for (auto bitIndex = 0u; bitIndex < 8u; ++bitIndex)
        {
            const auto bitSet = ((byte >> bitIndex) & 1u) != 0u;
            level = std::clamp(level + (bitSet ? 2 : -2), 0, 127);
            decoded.push_back((static_cast<float>(level) - 64.0f) / 64.0f);
        }
    }
    return decoded;
}

std::vector<float> decodePcm8Preview(const std::vector<uint8_t>& bytes)
{
    std::vector<float> decoded;
    decoded.reserve(bytes.size());
    for (const auto byte : bytes)
        decoded.push_back(std::clamp((static_cast<float>(byte) - 128.0f) / 128.0f, -1.0f, 1.0f));
    return decoded;
}

std::vector<float> decodeBrrPreview(const std::vector<uint8_t>& bytes, size_t& loopStart)
{
    std::vector<float> decoded;
    loopStart = 0;
    if (bytes.size() < 9u || (bytes.size() % 9u) != 0u)
        return decoded;

    decoded.reserve((bytes.size() / 9u) * 16u);
    auto previous1 = 0;
    auto previous2 = 0;
    auto loopSeen = false;

    for (size_t block = 0; block < bytes.size() / 9u; ++block)
    {
        const auto offset = block * 9u;
        const auto header = bytes[offset];
        const auto range = static_cast<int>((header >> 4u) & 0x0fu);
        const auto filter = static_cast<int>((header >> 2u) & 0x03u);
        if ((header & 0x02u) != 0u && ! loopSeen)
        {
            loopStart = decoded.size();
            loopSeen = true;
        }

        for (size_t packedIndex = 0; packedIndex < 8u; ++packedIndex)
        {
            const auto packed = bytes[offset + 1u + packedIndex];
            for (const auto nibble : { static_cast<uint8_t>(packed >> 4u), static_cast<uint8_t>(packed & 0x0fu) })
            {
                auto sample = nibble >= 8u ? static_cast<int>(nibble) - 16 : static_cast<int>(nibble);
                if (range <= 12)
                    sample <<= range;
                else
                    sample = sample < 0 ? -2048 : 0;

                auto predicted = sample;
                switch (filter)
                {
                    case 1: predicted += (previous1 * 15) / 16; break;
                    case 2: predicted += ((previous1 * 61) / 32) - ((previous2 * 15) / 16); break;
                    case 3: predicted += ((previous1 * 115) / 64) - ((previous2 * 13) / 16); break;
                    case 0:
                    default: break;
                }

                predicted = std::clamp(predicted, -32768, 32767);
                previous2 = previous1;
                previous1 = predicted;
                decoded.push_back(static_cast<float>(predicted) / 32768.0f);
            }
        }

        if ((header & 0x01u) != 0u)
            break;
    }

    return decoded;
}

template <typename SampleGenerator>
std::vector<float> generatedSamplePreview(size_t count, SampleGenerator&& generator)
{
    std::vector<float> decoded;
    decoded.reserve(count);
    for (size_t i = 0; i < count; ++i)
        decoded.push_back(std::clamp(static_cast<float>(generator(i)) / 127.0f, -1.0f, 1.0f));
    return decoded;
}

ChipperAudioProcessor::SampleWaveformPreview waveformPreviewFromSamples(const std::vector<float>& decoded)
{
    ChipperAudioProcessor::SampleWaveformPreview preview {};
    if (decoded.empty())
        return preview;

    if (decoded.size() <= preview.size())
    {
        for (size_t i = 0; i < decoded.size(); ++i)
            preview[i] = decoded[i];
        return preview;
    }

    for (size_t i = 0; i < preview.size(); ++i)
    {
        const auto start = (i * decoded.size()) / preview.size();
        const auto end = std::max(start + 1u, ((i + 1u) * decoded.size()) / preview.size());
        auto minValue = 1.0f;
        auto maxValue = -1.0f;
        for (size_t j = start; j < std::min(end, decoded.size()); ++j)
        {
            minValue = std::min(minValue, decoded[j]);
            maxValue = std::max(maxValue, decoded[j]);
        }
        preview[i] = std::abs(maxValue) >= std::abs(minValue) ? maxValue : minValue;
    }

    return preview;
}

float normalizedLoopStart(size_t loopStart, size_t sampleCount)
{
    if (sampleCount <= 1u)
        return 0.0f;

    return std::clamp(static_cast<float>(loopStart) / static_cast<float>(sampleCount - 1u), 0.0f, 1.0f);
}

float normalizedLoopEnd(size_t loopEnd, size_t sampleCount)
{
    if (sampleCount <= 1u)
        return 1.0f;

    return std::clamp(static_cast<float>(loopEnd) / static_cast<float>(sampleCount - 1u), 0.0f, 1.0f);
}

bool sampleSlotHasValidLoop(const ChipperAudioProcessor::DmcSampleSlot& slot)
{
    return slot.hasLoop
        && slot.bytes.size() > 1u
        && slot.loopStart < slot.loopEnd
        && slot.loopEnd <= slot.bytes.size();
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
        && a.nesDmcOnly == b.nesDmcOnly
        && std::abs(a.spc700LoopStart - b.spc700LoopStart) < tolerance
        && std::abs(a.spc700LoopEnd - b.spc700LoopEnd) < tolerance
        && a.spc700VoiceSampleSlots == b.spc700VoiceSampleSlots
        && a.fmOperatorLevels == b.fmOperatorLevels
        && a.fmOperatorMultipliers == b.fmOperatorMultipliers
        && a.fmOperatorAttackRates == b.fmOperatorAttackRates
        && a.fmOperatorDecayRates == b.fmOperatorDecayRates
        && a.fmOperatorSustainRates == b.fmOperatorSustainRates
        && a.fmOperatorReleaseRates == b.fmOperatorReleaseRates;
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
        && a.nesDmcOnly == b.nesDmcOnly
        && std::abs(a.spc700LoopStart - b.spc700LoopStart) < tolerance
        && std::abs(a.spc700LoopEnd - b.spc700LoopEnd) < tolerance
        && a.spc700VoiceSampleSlots == b.spc700VoiceSampleSlots
        && a.fmOperatorLevels == b.fmOperatorLevels
        && a.fmOperatorMultipliers == b.fmOperatorMultipliers
        && a.fmOperatorAttackRates == b.fmOperatorAttackRates
        && a.fmOperatorDecayRates == b.fmOperatorDecayRates
        && a.fmOperatorSustainRates == b.fmOperatorSustainRates
        && a.fmOperatorReleaseRates == b.fmOperatorReleaseRates;
}

int samplePlaybackModeForMacroTemplate(chipper::ChipMode mode, const chipper::MacroTemplate& templ)
{
    if (mode == chipper::ChipMode::paula)
    {
        switch (templ.macro)
        {
            case chipper::MacroKind::coin:
            case chipper::MacroKind::drum:
            case chipper::MacroKind::hit:
            case chipper::MacroKind::jump:
            case chipper::MacroKind::laser:
            case chipper::MacroKind::powerUp:
                return 2;

            case chipper::MacroKind::bass:
            case chipper::MacroKind::lead:
            case chipper::MacroKind::arp:
                return 1;

            case chipper::MacroKind::manual:
            default:
                return 0;
        }
    }

    if (mode != chipper::ChipMode::spc700)
        return 0;

    const auto patch = chipper::makePatchConfig(mode,
                                                templ.macro,
                                                templ.controls[0],
                                                templ.controls[1],
                                                templ.controls[2],
                                                templ.controls[3],
                                                chipper::PlayMode::stack,
                                                { true, true, true, true, true, true, true, true, true },
                                                { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
                                                templ.stereoSpread,
                                                templ.envelopeDecay,
                                                templ.waveShape,
                                                0,
                                                0,
                                                templ.dmgStereoRoute,
                                                templ.ymEnvelopeShape,
                                                0,
                                                0,
                                                0,
                                                templ.snNoiseMode);
    return static_cast<int>(chipper::spc700SamplePlaybackModeForPatch(patch));
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
    slot.encoding = chipper::ExternalSampleEncoding::rawBytes;
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    return juce::Result::ok();
}

juce::Result readOpnaRhythmRomFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("OPNA rhythm ROM file does not exist: " + file.getFullPathName());

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read OPNA rhythm ROM file: " + file.getFullPathName());

    if (block.getSize() == 0u)
        return juce::Result::fail("OPNA rhythm ROM file is empty: " + file.getFullPathName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.encoding = chipper::ExternalSampleEncoding::rawBytes;
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    slot.included = true;
    return juce::Result::ok();
}

juce::Result readOpnaAdpcmBSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("OPNA ADPCM-B sample file does not exist: " + file.getFullPathName());

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read OPNA ADPCM-B sample file: " + file.getFullPathName());

    if (block.getSize() == 0u)
        return juce::Result::fail("OPNA ADPCM-B sample file is empty: " + file.getFullPathName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.encoding = chipper::ExternalSampleEncoding::rawBytes;
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    slot.included = true;
    return juce::Result::ok();
}

juce::Result readOpnbAdpcmSampleFile(const juce::File& file,
                                     ChipperAudioProcessor::DmcSampleSlot& slot,
                                     const juce::String& label)
{
    if (! file.existsAsFile())
        return juce::Result::fail("OPNB " + label + " sample file does not exist: " + file.getFullPathName());

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read OPNB " + label + " sample file: " + file.getFullPathName());

    if (block.getSize() == 0u)
        return juce::Result::fail("OPNB " + label + " sample file is empty: " + file.getFullPathName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.encoding = chipper::ExternalSampleEncoding::rawBytes;
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    slot.included = true;
    return juce::Result::ok();
}

juce::Result readOpnbAdpcmASampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    return readOpnbAdpcmSampleFile(file, slot, "ADPCM-A");
}

juce::Result readOpnbAdpcmBSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    return readOpnbAdpcmSampleFile(file, slot, "ADPCM-B");
}

bool fileLooksLikePcmImport(const juce::File& file)
{
    return file.hasFileExtension(".wav;.aif;.aiff");
}

bool fileLooksLikeIff8svxImport(const juce::File& file)
{
    return file.hasFileExtension(".8svx;.iff");
}

bool fileLooksLikeModImport(const juce::File& file)
{
    return file.hasFileExtension(".mod");
}

uint16_t readBigEndian16(const uint8_t* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8u) | static_cast<uint16_t>(data[1]));
}

uint32_t readLittleEndian32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0])
        | (static_cast<uint32_t>(data[1]) << 8u)
        | (static_cast<uint32_t>(data[2]) << 16u)
        | (static_cast<uint32_t>(data[3]) << 24u);
}

uint32_t readBigEndian32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 24u)
        | (static_cast<uint32_t>(data[1]) << 16u)
        | (static_cast<uint32_t>(data[2]) << 8u)
        | static_cast<uint32_t>(data[3]);
}

bool readWavSmplLoopMetadata(const juce::File& file, size_t sampleCount, size_t& loopStart, size_t& loopEnd)
{
    loopStart = 0u;
    loopEnd = 0u;
    if (sampleCount <= 1u || ! file.hasFileExtension(".wav"))
        return false;

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return false;

    const auto* bytes = static_cast<const uint8_t*>(block.getData());
    const auto size = block.getSize();
    if (size < 12u || std::memcmp(bytes, "RIFF", 4u) != 0 || std::memcmp(bytes + 8u, "WAVE", 4u) != 0)
        return false;

    auto offset = static_cast<size_t>(12u);
    while (offset + 8u <= size)
    {
        const auto* chunkId = bytes + offset;
        const auto chunkSize = static_cast<size_t>(readLittleEndian32(bytes + offset + 4u));
        const auto chunkDataOffset = offset + 8u;
        if (chunkDataOffset + chunkSize > size)
            return false;

        if (std::memcmp(chunkId, "smpl", 4u) == 0)
        {
            if (chunkSize < 36u)
                return false;

            const auto loopCount = readLittleEndian32(bytes + chunkDataOffset + 28u);
            auto loopOffset = chunkDataOffset + 36u;
            for (uint32_t i = 0; i < loopCount && loopOffset + 24u <= chunkDataOffset + chunkSize; ++i, loopOffset += 24u)
            {
                const auto type = readLittleEndian32(bytes + loopOffset + 4u);
                const auto start = static_cast<size_t>(readLittleEndian32(bytes + loopOffset + 8u));
                const auto inclusiveEnd = static_cast<size_t>(readLittleEndian32(bytes + loopOffset + 12u));
                if (type == 0u && start < inclusiveEnd && start < sampleCount)
                {
                    loopStart = std::min(start, sampleCount - 1u);
                    loopEnd = std::min(sampleCount, inclusiveEnd + 1u);
                    return loopEnd > loopStart + 1u;
                }
            }

            return false;
        }

        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    return false;
}

bool readAiffInstLoopMetadata(const juce::File& file, size_t sampleCount, size_t& loopStart, size_t& loopEnd)
{
    loopStart = 0u;
    loopEnd = 0u;
    if (sampleCount <= 1u || ! file.hasFileExtension(".aif;.aiff"))
        return false;

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return false;

    const auto* bytes = static_cast<const uint8_t*>(block.getData());
    const auto size = block.getSize();
    if (size < 12u || std::memcmp(bytes, "FORM", 4u) != 0)
        return false;

    const auto isAiff = std::memcmp(bytes + 8u, "AIFF", 4u) == 0 || std::memcmp(bytes + 8u, "AIFC", 4u) == 0;
    if (! isAiff)
        return false;

    struct MarkerPosition
    {
        uint16_t id = 0u;
        size_t position = 0u;
    };

    std::vector<MarkerPosition> markers;
    auto sustainPlayMode = uint16_t { 0u };
    auto sustainBeginMarker = uint16_t { 0u };
    auto sustainEndMarker = uint16_t { 0u };
    auto sawInstrumentChunk = false;
    const auto formSize = static_cast<size_t>(readBigEndian32(bytes + 4u));
    const auto formEnd = std::min(size, static_cast<size_t>(8u + formSize));
    auto offset = static_cast<size_t>(12u);
    while (offset + 8u <= formEnd)
    {
        const auto* chunkId = bytes + offset;
        const auto chunkSize = static_cast<size_t>(readBigEndian32(bytes + offset + 4u));
        const auto chunkDataOffset = offset + 8u;
        if (chunkDataOffset + chunkSize > size)
            return false;

        if (std::memcmp(chunkId, "MARK", 4u) == 0)
        {
            if (chunkSize < 2u)
                return false;

            const auto markerCount = readBigEndian16(bytes + chunkDataOffset);
            auto markerOffset = chunkDataOffset + 2u;
            const auto markerEnd = chunkDataOffset + chunkSize;
            markers.reserve(static_cast<size_t>(markerCount));
            for (uint16_t i = 0; i < markerCount && markerOffset + 7u <= markerEnd; ++i)
            {
                const auto markerId = readBigEndian16(bytes + markerOffset);
                const auto markerPosition = static_cast<size_t>(readBigEndian32(bytes + markerOffset + 2u));
                const auto nameLength = static_cast<size_t>(bytes[markerOffset + 6u]);
                auto nameFieldSize = static_cast<size_t>(1u) + nameLength;
                if ((nameFieldSize & 1u) != 0u)
                    ++nameFieldSize;

                if (markerOffset + 6u + nameFieldSize > markerEnd)
                    return false;

                markers.push_back({ markerId, markerPosition });
                markerOffset += 6u + nameFieldSize;
            }
        }
        else if (std::memcmp(chunkId, "INST", 4u) == 0)
        {
            if (chunkSize < 20u)
                return false;

            sawInstrumentChunk = true;
            sustainPlayMode = readBigEndian16(bytes + chunkDataOffset + 8u);
            sustainBeginMarker = readBigEndian16(bytes + chunkDataOffset + 10u);
            sustainEndMarker = readBigEndian16(bytes + chunkDataOffset + 12u);
        }

        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    if (! sawInstrumentChunk || sustainPlayMode != 1u || sustainBeginMarker == 0u || sustainEndMarker == 0u)
        return false;

    const auto markerPositionForId = [&markers](uint16_t markerId, size_t& position)
    {
        for (const auto& marker : markers)
        {
            if (marker.id == markerId)
            {
                position = marker.position;
                return true;
            }
        }
        return false;
    };

    size_t markerStart = 0u;
    size_t markerEnd = 0u;
    if (! markerPositionForId(sustainBeginMarker, markerStart) || ! markerPositionForId(sustainEndMarker, markerEnd))
        return false;

    if (markerStart >= markerEnd || markerStart >= sampleCount)
        return false;

    loopStart = std::min(markerStart, sampleCount - 1u);
    loopEnd = std::min(markerEnd, sampleCount);
    return loopEnd > loopStart + 1u;
}

juce::File resolvePresetSamplePath(const juce::XmlElement& sampleState, const juce::File& presetDirectory)
{
    const auto relativePath = sampleState.getStringAttribute("relativePath").trim();
    if (relativePath.isNotEmpty() && presetDirectory.isDirectory())
    {
        const auto relativeFile = presetDirectory.getChildFile(relativePath);
        if (relativeFile.existsAsFile())
            return relativeFile;
    }

    const auto originalPath = sampleState.getStringAttribute("path").trim();
    if (originalPath.isEmpty())
        return {};

    const juce::File originalFile(originalPath);
    if (originalFile.existsAsFile())
        return originalFile;

    if (presetDirectory.isDirectory())
    {
        const auto siblingFile = presetDirectory.getChildFile(originalFile.getFileName());
        if (siblingFile.existsAsFile())
            return siblingFile;

        const auto samplesFile = presetDirectory.getChildFile("Samples").getChildFile(originalFile.getFileName());
        if (samplesFile.existsAsFile())
            return samplesFile;

        const auto lowercaseSamplesFile = presetDirectory.getChildFile("samples").getChildFile(originalFile.getFileName());
        if (lowercaseSamplesFile.existsAsFile())
            return lowercaseSamplesFile;
    }

    return originalFile;
}

juce::String restoreWarningLine(const juce::String& label, const juce::StringArray& issues)
{
    if (issues.isEmpty())
        return {};

    auto line = label + " sample restore issue";
    if (issues.size() > 1)
        line += "s (" + juce::String(issues.size()) + ")";

    line += ": " + issues[0];
    if (issues.size() > 1)
        line += " (+" + juce::String(issues.size() - 1) + " more)";

    return line;
}

void appendRestoreWarning(juce::String& statusLine, const juce::String& warning)
{
    if (warning.isEmpty())
        return;

    if (statusLine.isEmpty())
        statusLine = warning;
    else
        statusLine += " | " + warning;
}

juce::Result readPcm8SampleFile(const juce::File& file,
                                ChipperAudioProcessor::DmcSampleSlot& slot,
                                const juce::String& label,
                                const juce::String& plannedFormats)
{
    if (! file.existsAsFile())
        return juce::Result::fail(label + " sample file does not exist: " + file.getFullPathName());

    if (! fileLooksLikePcmImport(file))
        return juce::Result::fail(label + " sample import supports WAV/AIFF files in this build. " + plannedFormats + " import is planned.");

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        return juce::Result::fail("Could not decode " + label + " sample file: " + file.getFullPathName());

    if (reader->lengthInSamples <= 0)
        return juce::Result::fail(label + " sample file is empty: " + file.getFullPathName());

    constexpr int maxImportedSamples = 262144;
    const auto sampleCount = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, maxImportedSamples));
    juce::AudioBuffer<float> decoded(static_cast<int>(reader->numChannels), sampleCount);
    if (! reader->read(&decoded, 0, sampleCount, 0, true, true))
        return juce::Result::fail("Could not read Paula sample audio: " + file.getFullPathName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.encoding = chipper::ExternalSampleEncoding::signedPcm8;
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
        return juce::Result::fail(label + " sample import produced no usable sample bytes: " + file.getFileName());

    size_t loopStart = 0u;
    size_t loopEnd = 0u;
    if (readWavSmplLoopMetadata(file, slot.bytes.size(), loopStart, loopEnd)
        || readAiffInstLoopMetadata(file, slot.bytes.size(), loopStart, loopEnd))
    {
        slot.hasLoop = true;
        slot.loopStart = loopStart;
        slot.loopEnd = loopEnd;
    }

    return juce::Result::ok();
}

juce::Result readIff8svxSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("Paula sample file does not exist: " + file.getFullPathName());

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read Paula 8SVX sample file: " + file.getFullPathName());

    const auto* bytes = static_cast<const uint8_t*>(block.getData());
    const auto size = block.getSize();
    if (size < 12u || std::memcmp(bytes, "FORM", 4u) != 0 || std::memcmp(bytes + 8u, "8SVX", 4u) != 0)
        return juce::Result::fail("Paula IFF import only supports FORM 8SVX files: " + file.getFileName());

    const auto formSize = static_cast<size_t>(readBigEndian32(bytes + 4u));
    const auto formEnd = std::min(size, static_cast<size_t>(8u + formSize));
    auto offset = static_cast<size_t>(12u);
    bool sawVhdr = false;
    bool isCompressed = false;
    uint32_t oneShotSamples = 0;
    uint32_t repeatSamples = 0;
    const uint8_t* body = nullptr;
    size_t bodySize = 0u;

    while (offset + 8u <= formEnd)
    {
        const auto* chunkId = bytes + offset;
        const auto chunkSize = static_cast<size_t>(readBigEndian32(bytes + offset + 4u));
        const auto chunkDataOffset = offset + 8u;
        if (chunkDataOffset + chunkSize > size)
            return juce::Result::fail("Paula 8SVX chunk extends past end of file: " + file.getFileName());

        if (std::memcmp(chunkId, "VHDR", 4u) == 0)
        {
            if (chunkSize < 16u)
                return juce::Result::fail("Paula 8SVX VHDR chunk is too short: " + file.getFileName());

            sawVhdr = true;
            oneShotSamples = readBigEndian32(bytes + chunkDataOffset);
            repeatSamples = readBigEndian32(bytes + chunkDataOffset + 4u);
            const auto sampleRate = readBigEndian16(bytes + chunkDataOffset + 12u);
            const auto compression = bytes[chunkDataOffset + 15u];
            if (sampleRate == 0u)
                return juce::Result::fail("Paula 8SVX sample rate is invalid: " + file.getFileName());

            isCompressed = compression != 0u;
        }
        else if (std::memcmp(chunkId, "BODY", 4u) == 0)
        {
            body = bytes + chunkDataOffset;
            bodySize = chunkSize;
        }

        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    if (! sawVhdr)
        return juce::Result::fail("Paula 8SVX file is missing VHDR metadata: " + file.getFileName());

    if (isCompressed)
        return juce::Result::fail("Paula 8SVX import supports uncompressed BODY data only in this build: " + file.getFileName());

    if (body == nullptr || bodySize == 0u)
        return juce::Result::fail("Paula 8SVX file has no BODY sample data: " + file.getFileName());

    constexpr size_t maxImportedSamples = 262144u;
    const auto sampleCount = std::min(bodySize, maxImportedSamples);
    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.encoding = chipper::ExternalSampleEncoding::signedPcm8;
    slot.bytes.resize(sampleCount);
    if (repeatSamples > 1u && oneShotSamples < sampleCount)
    {
        slot.hasLoop = true;
        slot.loopStart = static_cast<size_t>(std::min<uint32_t>(oneShotSamples, static_cast<uint32_t>(sampleCount - 1u)));
        slot.loopEnd = std::min(sampleCount, static_cast<size_t>(oneShotSamples) + static_cast<size_t>(repeatSamples));
        if (slot.loopEnd <= slot.loopStart + 1u)
            slot.hasLoop = false;
    }
    for (size_t i = 0; i < sampleCount; ++i)
        slot.bytes[i] = static_cast<uint8_t>(static_cast<int>(static_cast<int8_t>(body[i])) + 128);

    return juce::Result::ok();
}

juce::String fixedAsciiString(const uint8_t* data, size_t size)
{
    juce::String text;
    for (size_t i = 0; i < size; ++i)
    {
        const auto byte = data[i];
        if (byte == 0u)
            break;

        text += juce::String::charToString(static_cast<juce_wchar>(byte >= 32u && byte < 127u ? byte : '?'));
    }
    return text.trim();
}

bool isSupportedProTrackerSignature(const uint8_t* signature)
{
    return std::memcmp(signature, "M.K.", 4u) == 0
        || std::memcmp(signature, "M!K!", 4u) == 0
        || std::memcmp(signature, "4CHN", 4u) == 0
        || std::memcmp(signature, "FLT4", 4u) == 0;
}

juce::Result readProTrackerModSampleFile(const juce::File& file, std::vector<ChipperAudioProcessor::DmcSampleSlot>& slots)
{
    if (! file.existsAsFile())
        return juce::Result::fail("Paula MOD sample file does not exist: " + file.getFullPathName());

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read Paula MOD sample file: " + file.getFullPathName());

    const auto* bytes = static_cast<const uint8_t*>(block.getData());
    const auto size = block.getSize();
    constexpr auto headerSize = size_t { 1084u };
    constexpr auto sampleHeaderOffset = size_t { 20u };
    constexpr auto sampleHeaderSize = size_t { 30u };
    constexpr auto sampleCount = size_t { 31u };
    if (size < headerSize || ! isSupportedProTrackerSignature(bytes + 1080u))
        return juce::Result::fail("Paula MOD import supports 31-sample ProTracker 4-channel .mod files in this build: " + file.getFileName());

    const auto songLength = std::clamp(static_cast<int>(bytes[950]), 1, 128);
    auto highestPattern = uint8_t { 0u };
    for (int i = 0; i < songLength; ++i)
        highestPattern = std::max(highestPattern, bytes[952u + static_cast<size_t>(i)]);

    const auto patternCount = static_cast<size_t>(highestPattern) + 1u;
    const auto sampleDataOffset = headerSize + patternCount * 1024u;
    if (sampleDataOffset > size)
        return juce::Result::fail("Paula MOD pattern data extends past end of file: " + file.getFileName());

    struct ModSampleHeader
    {
        juce::String name;
        size_t length = 0u;
        size_t loopStart = 0u;
        size_t loopLength = 0u;
    };

    std::array<ModSampleHeader, sampleCount> headers;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        const auto headerOffset = sampleHeaderOffset + i * sampleHeaderSize;
        headers[i].name = fixedAsciiString(bytes + headerOffset, 22u);
        headers[i].length = static_cast<size_t>(readBigEndian16(bytes + headerOffset + 22u)) * 2u;
        headers[i].loopStart = static_cast<size_t>(readBigEndian16(bytes + headerOffset + 26u)) * 2u;
        headers[i].loopLength = static_cast<size_t>(readBigEndian16(bytes + headerOffset + 28u)) * 2u;
    }

    auto sampleOffset = sampleDataOffset;
    constexpr size_t maxImportedSamples = 262144u;
    for (size_t i = 0; i < sampleCount; ++i)
    {
        const auto declaredLength = headers[i].length;
        if (sampleOffset > size)
            break;

        const auto available = size - sampleOffset;
        const auto sourceLength = std::min(declaredLength, available);
        if (sourceLength > 0u)
        {
            const auto importedLength = std::min(sourceLength, maxImportedSamples);
            ChipperAudioProcessor::DmcSampleSlot slot;
            const auto label = headers[i].name.isNotEmpty()
                ? headers[i].name
                : ("Sample " + juce::String(static_cast<int>(i + 1)).paddedLeft('0', 2));
            slot.name = file.getFileNameWithoutExtension() + " #" + juce::String(static_cast<int>(i + 1)).paddedLeft('0', 2) + " " + label;
            slot.path = file.getFullPathName();
            slot.encoding = chipper::ExternalSampleEncoding::signedPcm8;
            slot.sourceSampleIndex = static_cast<int>(i);
            slot.bytes.resize(importedLength);
            for (size_t sample = 0; sample < importedLength; ++sample)
                slot.bytes[sample] = static_cast<uint8_t>(static_cast<int>(static_cast<int8_t>(bytes[sampleOffset + sample])) + 128);

            if (headers[i].loopLength > 2u && headers[i].loopStart < importedLength)
            {
                slot.hasLoop = true;
                slot.loopStart = std::min(headers[i].loopStart, importedLength - 1u);
                slot.loopEnd = std::min(importedLength, headers[i].loopStart + headers[i].loopLength);
                if (slot.loopEnd <= slot.loopStart + 1u)
                    slot.hasLoop = false;
            }

            slots.push_back(std::move(slot));
        }

        sampleOffset += declaredLength;
    }

    if (slots.empty())
        return juce::Result::fail("Paula MOD file has no extractable sample data: " + file.getFileName());

    return juce::Result::ok();
}

juce::Result readSpc700BrrSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (! file.existsAsFile())
        return juce::Result::fail("SPC700 sample file does not exist: " + file.getFullPathName());

    if (fileLooksLikePcmImport(file))
        return readPcm8SampleFile(file, slot, "SPC700", "true WAV-to-BRR");

    if (! file.hasFileExtension(".brr"))
        return juce::Result::fail("SPC700 sample import supports .brr, WAV, and AIFF files in this build.");

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
        return juce::Result::fail("Could not read BRR sample file: " + file.getFullPathName());

    if (block.getSize() == 0u)
        return juce::Result::fail("BRR sample file is empty: " + file.getFullPathName());

    if ((block.getSize() % 9u) != 0u)
        return juce::Result::fail("BRR sample size must be a whole number of 9-byte BRR blocks: " + file.getFileName());

    slot.name = file.getFileName();
    slot.path = file.getFullPathName();
    slot.encoding = chipper::ExternalSampleEncoding::spc700Brr;
    slot.bytes.resize(block.getSize());
    std::memcpy(slot.bytes.data(), block.getData(), block.getSize());
    return juce::Result::ok();
}

juce::Result readPaulaSampleFile(const juce::File& file, ChipperAudioProcessor::DmcSampleSlot& slot)
{
    if (fileLooksLikeIff8svxImport(file))
        return readIff8svxSampleFile(file, slot);

    return readPcm8SampleFile(file, slot, "Paula", "compressed 8SVX/full tracker playback");
}

juce::Result readPaulaSampleFileSlots(const juce::File& file, std::vector<ChipperAudioProcessor::DmcSampleSlot>& slots)
{
    if (fileLooksLikeModImport(file))
        return readProTrackerModSampleFile(file, slots);

    ChipperAudioProcessor::DmcSampleSlot slot;
    if (auto result = readPaulaSampleFile(file, slot); result.failed())
        return result;

    slots.push_back(std::move(slot));
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
        dmcSampleRestoreWarning = {};
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
        dmcSampleRestoreWarning = {};
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
        spc700SampleRestoreWarning = {};
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
        return juce::Result::fail("SPC700 sample directory does not exist: " + directory.getFullPathName());

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.brr;*.wav;*.aif;*.aiff");
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
        return juce::Result::fail("No readable .brr, WAV, or AIFF files found in: " + directory.getFullPathName());

    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        spc700BrrSampleBank = std::move(loaded);
        spc700BrrSample = spc700BrrSampleBank.front();
        spc700SampleRestoreWarning = {};
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
    std::vector<DmcSampleSlot> slots;
    if (auto result = readPaulaSampleFileSlots(file, slots); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        paulaSampleBank = std::move(slots);
        for (size_t i = 0; i < paulaSampleBank.size(); ++i)
            paulaSampleBank[i].included = i < 32u;
        paulaSample = paulaSampleBank.front();
        paulaSampleRestoreWarning = {};
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
    directory.findChildFiles(files, juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.8svx;*.iff;*.mod");
    files.sort();

    std::vector<DmcSampleSlot> loaded;
    loaded.reserve(static_cast<size_t>(std::min(128, files.size())));
    for (const auto& file : files)
    {
        if (loaded.size() >= 128u)
            break;

        std::vector<DmcSampleSlot> fileSlots;
        if (readPaulaSampleFileSlots(file, fileSlots).wasOk())
        {
            for (auto& slot : fileSlots)
            {
                if (loaded.size() >= 128u)
                    break;

                slot.included = loaded.size() < 32u;
                loaded.push_back(std::move(slot));
            }
        }
    }

    if (loaded.empty())
        return juce::Result::fail("No readable WAV/AIFF/8SVX/MOD files found in: " + directory.getFullPathName());

    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        paulaSampleBank = std::move(loaded);
        paulaSample = paulaSampleBank.front();
        paulaSampleRestoreWarning = {};
        ++paulaSampleBankRevision;
    }

    setPlainParameterValue(chipper::parameters::id::nesDmcSampleSlot, 0.0f);
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    applyPaulaSampleToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadOpnaRhythmRomFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readOpnaRhythmRomFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(opnaRhythmRomMutex);
        opnaRhythmRom = std::move(slot);
        opnaRhythmRomRestoreWarning = {};
        ++opnaRhythmRomRevision;
    }

    activeOpnaRhythmRomRevision = std::numeric_limits<uint64_t>::max();
    applyOpnaRhythmRomToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadOpnaAdpcmBSampleFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readOpnaAdpcmBSampleFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(opnaAdpcmBSampleMutex);
        opnaAdpcmBSample = std::move(slot);
        opnaAdpcmBSampleRestoreWarning = {};
        ++opnaAdpcmBSampleRevision;
    }

    activeOpnaAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    applyOpnaAdpcmBSampleToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadOpnbAdpcmASampleFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readOpnbAdpcmASampleFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmASampleMutex);
        opnbAdpcmASample = std::move(slot);
        opnbAdpcmASampleRestoreWarning = {};
        ++opnbAdpcmASampleRevision;
    }

    activeOpnbAdpcmASampleRevision = std::numeric_limits<uint64_t>::max();
    applyOpnbAdpcmASampleToCore();
    return juce::Result::ok();
}

juce::Result ChipperAudioProcessor::loadOpnbAdpcmBSampleFile(const juce::File& file)
{
    DmcSampleSlot slot;
    if (auto result = readOpnbAdpcmBSampleFile(file, slot); result.failed())
        return result;

    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmBSampleMutex);
        opnbAdpcmBSample = std::move(slot);
        opnbAdpcmBSampleRestoreWarning = {};
        ++opnbAdpcmBSampleRevision;
    }

    activeOpnbAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    applyOpnbAdpcmBSampleToCore();
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
    if (core != nullptr)
    {
        const auto debugState = core->debugStateJson();
        info.sampleActive = jsonIntValue(debugState, "dmcSampleActive") != 0;
        info.sampleCompleted = jsonIntValue(debugState, "dmcSampleCompleted") != 0;
        info.bitsPlayed = jsonIntValue(debugState, "dmcSampleBitsPlayed");
    }
    const auto selectedSlot = info.playbackMode != 0 && activeDmcSampleSlot >= 0 ? activeDmcSampleSlot : manualSlot;
    info.rateIndex = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcRateIndex)->load()));
    info.rateIndex = std::clamp(info.rateIndex, 0, 15);

    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto selectedMode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto clockOverride = apvts.getRawParameterValue(chipper::parameters::id::clockHz)->load();
    const auto selectedClock = clockOverride > 0.0f ? static_cast<double>(clockOverride) : chipper::parameters::defaultClockForMode(selectedMode);
    info.bitRateHz = selectedClock / static_cast<double>(ntscDmcPeriods[static_cast<size_t>(info.rateIndex)]);

    const std::lock_guard<std::mutex> lock(dmcSampleMutex);
    const auto restoreWarning = dmcSampleRestoreWarning;
    if (dmcSampleBank.empty())
    {
        info.statusLine = restoreWarning.isNotEmpty() ? restoreWarning : "No DMC samples loaded";
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
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    info.activeSlotCount = static_cast<int>(activeSlots.size());
    info.mapHighNote = std::clamp(info.mapRootNote + info.activeSlotCount - 1, 0, 127);
    if (info.playbackMode != 0 && activeDmcSampleSlot == unmappedDmcSampleSlot)
    {
        info.activeSlot = -1;
        info.statusLine = "No mapped DMC sample | Map " + midiNoteName(info.mapRootNote) + "-" + midiNoteName(info.mapHighNote);
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    const auto safeSlot = static_cast<size_t>(std::clamp(selectedSlot, 0, static_cast<int>(activeSlots.size() - 1u)));
    const auto& slot = *activeSlots[safeSlot];
    info.activeSlot = static_cast<int>(safeSlot);
    info.sampleName = slot.name;
    info.byteCount = static_cast<int>(slot.bytes.size());
    info.bitCount = info.byteCount * 8;
    info.durationMs = info.bitRateHz > 0.0 ? (static_cast<double>(info.bitCount) / info.bitRateHz) * 1000.0 : 0.0;
    auto runState = juce::String(info.loopEnabled ? "Looping sample" : "One-shot, no loop; DAC holds");
    if (! info.loopEnabled)
    {
        if (info.sampleCompleted)
            runState += " (stopped)";
        else if (info.sampleActive)
            runState += " (playing)";
        else
            runState += " (ready)";
    }
    info.statusLine = "Slot " + juce::String(info.activeSlot + 1) + "/" + juce::String(info.activeSlotCount)
        + ": " + info.sampleName + " (" + juce::String(info.byteCount) + " bytes, "
        + juce::String(info.durationMs, info.durationMs < 10.0 ? 1 : 0) + " ms @ rate "
        + juce::String(info.rateIndex) + ", " + runState + ")";
    if (info.playbackMode != 0)
        info.statusLine += " | Map " + midiNoteName(info.mapRootNote) + "-" + midiNoteName(info.mapHighNote);
    appendRestoreWarning(info.statusLine, restoreWarning);
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
    const auto restoreWarning = spc700SampleRestoreWarning;
    if (spc700BrrSample.bytes.empty())
    {
        info.statusLine = "Generated SPC700 template active; load File/Folder for external samples";
        appendRestoreWarning(info.statusLine, restoreWarning);
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
        info.statusLine = "No SPC700 samples checked";
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    const auto bankCount = static_cast<int>(activeSlots.size());
    auto safeSlot = std::clamp(selectedSlot, 0, std::max(0, bankCount - 1));
    auto bankByteCount = 0;
    auto bankBrrBlockCount = 0;
    for (const auto* slot : activeSlots)
    {
        bankByteCount += static_cast<int>(slot->bytes.size());
        if (slot->encoding == chipper::ExternalSampleEncoding::spc700Brr)
            bankBrrBlockCount += static_cast<int>(slot->bytes.size() / 9u);
    }

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
    if (playbackMode != 0 && activeSpc700BrrSampleSlot < 0)
    {
        info.selectedSlot = -1;
        info.bankByteCount = bankByteCount;
        info.bankBrrBlockCount = bankBrrBlockCount;
        info.bankCount = bankCount;
        info.playbackMode = playbackMode;
        info.mapRootNote = mapRootNote;
        info.mapHighNote = std::clamp(mapRootNote + std::max(0, bankCount - 1), 0, 127);
        info.nearAramBudget = info.bankByteCount >= static_cast<int>(static_cast<double>(info.aramBudgetBytes) * 0.75);
        info.exceedsAramBudget = info.bankByteCount > info.aramBudgetBytes;
        info.statusLine = "No mapped SPC700 sample | Map " + midiNoteName(info.mapRootNote) + "-" + midiNoteName(info.mapHighNote);
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    const auto& selected = *activeSlots[static_cast<size_t>(safeSlot)];
    info.sampleName = selected.name;
    info.path = selected.path;
    info.byteCount = static_cast<int>(selected.bytes.size());
    info.blockCount = selected.encoding == chipper::ExternalSampleEncoding::spc700Brr ? info.byteCount / 9 : 0;
    info.bankByteCount = bankByteCount;
    info.bankBrrBlockCount = bankBrrBlockCount;
    info.bankCount = bankCount;
    info.selectedSlot = safeSlot;
    info.playbackMode = playbackMode;
    info.mapRootNote = mapRootNote;
    info.mapHighNote = std::clamp(mapRootNote + std::max(0, bankCount - 1), 0, 127);
    info.nearAramBudget = info.bankByteCount >= static_cast<int>(static_cast<double>(info.aramBudgetBytes) * 0.75);
    info.exceedsAramBudget = info.bankByteCount > info.aramBudgetBytes;
    info.statusLine = "Slot " + juce::String(safeSlot + 1) + "/" + juce::String(bankCount)
        + ": " + info.sampleName + (selected.encoding == chipper::ExternalSampleEncoding::spc700Brr
            ? " (" + juce::String(info.byteCount) + " bytes, " + juce::String(info.blockCount) + " BRR blocks)"
            : " (" + juce::String(info.byteCount) + " imported 8-bit samples)");
    appendRestoreWarning(info.statusLine, restoreWarning);
    return info;
}

ChipperAudioProcessor::Spc700BrrSampleInfo ChipperAudioProcessor::paulaSampleInfo() const
{
    Spc700BrrSampleInfo info;
    const std::lock_guard<std::mutex> lock(paulaSampleMutex);
    const auto restoreWarning = paulaSampleRestoreWarning;
    if (paulaSample.bytes.empty())
    {
        info.statusLine = "No external Paula sample bank loaded";
        appendRestoreWarning(info.statusLine, restoreWarning);
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
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    const auto bankCount = static_cast<int>(activeSlots.size());
    auto safeSlot = std::clamp(selectedSlot, 0, std::max(0, bankCount - 1));
    auto bankByteCount = 0;
    for (const auto* slot : activeSlots)
        bankByteCount += static_cast<int>(slot->bytes.size());

    for (int i = 0; i < static_cast<int>(activeSlots.size()); ++i)
    {
        if (activeSlots[static_cast<size_t>(i)]->path == paulaSample.path
            && activeSlots[static_cast<size_t>(i)]->sourceSampleIndex == paulaSample.sourceSampleIndex)
        {
            safeSlot = i;
            break;
        }
    }

    const auto mapRootNote = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcMapRoot)->load())), 0, 127);
    const auto playbackMode = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load())), 0, 2);
    if (playbackMode != 0 && activePaulaSampleSlot < 0)
    {
        info.selectedSlot = -1;
        info.bankByteCount = bankByteCount;
        info.bankCount = bankCount;
        info.playbackMode = playbackMode;
        info.mapRootNote = mapRootNote;
        info.mapHighNote = std::clamp(mapRootNote + std::max(0, bankCount - 1), 0, 127);
        info.statusLine = "No mapped Paula sample | Map " + midiNoteName(info.mapRootNote) + "-" + midiNoteName(info.mapHighNote);
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    const auto& selected = *activeSlots[static_cast<size_t>(safeSlot)];
    info.sampleName = selected.name;
    info.path = selected.path;
    info.byteCount = static_cast<int>(selected.bytes.size());
    info.blockCount = 0;
    info.bankByteCount = bankByteCount;
    info.bankCount = bankCount;
    info.selectedSlot = safeSlot;
    info.playbackMode = playbackMode;
    info.mapRootNote = mapRootNote;
    info.mapHighNote = std::clamp(mapRootNote + std::max(0, bankCount - 1), 0, 127);
    if (sampleSlotHasValidLoop(selected))
    {
        info.hasLoop = true;
        info.loopStartSample = static_cast<int>(selected.loopStart);
        info.loopEndSample = static_cast<int>(selected.loopEnd);
    }
    info.statusLine = "Slot " + juce::String(safeSlot + 1) + "/" + juce::String(bankCount)
        + ": " + info.sampleName + " (" + juce::String(info.byteCount) + " 8-bit samples)";
    if (info.hasLoop)
        info.statusLine += " | Loop " + juce::String(info.loopStartSample) + "-" + juce::String(info.loopEndSample);
    appendRestoreWarning(info.statusLine, restoreWarning);
    return info;
}

ChipperAudioProcessor::OpnaRhythmRomInfo ChipperAudioProcessor::opnaRhythmRomInfo() const
{
    OpnaRhythmRomInfo info;
    info.romByteCount = opnaRhythmRomBytes;
    const std::lock_guard<std::mutex> lock(opnaRhythmRomMutex);
    const auto restoreWarning = opnaRhythmRomRestoreWarning;
    if (opnaRhythmRom.bytes.empty())
    {
        info.statusLine = "Generated OPNA ADPCM-A rhythm ROM active";
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    info.loaded = true;
    info.sampleName = opnaRhythmRom.name;
    info.path = opnaRhythmRom.path;
    info.byteCount = static_cast<int>(opnaRhythmRom.bytes.size());
    info.copiedByteCount = std::min(info.byteCount, info.romByteCount);
    info.truncated = info.byteCount > info.romByteCount;
    info.statusLine = juce::String("User rhythm ROM: ") + info.sampleName + " ("
        + juce::String(info.byteCount) + " bytes";
    if (info.truncated)
        info.statusLine += ", first " + juce::String(info.copiedByteCount) + " copied";
    info.statusLine += ")";
    appendRestoreWarning(info.statusLine, restoreWarning);
    return info;
}

ChipperAudioProcessor::OpnaAdpcmBSampleInfo ChipperAudioProcessor::opnaAdpcmBSampleInfo() const
{
    OpnaAdpcmBSampleInfo info;
    info.memoryByteCount = opnaAdpcmBMemoryBytes;
    const std::lock_guard<std::mutex> lock(opnaAdpcmBSampleMutex);
    const auto restoreWarning = opnaAdpcmBSampleRestoreWarning;
    if (opnaAdpcmBSample.bytes.empty())
    {
        info.statusLine = "No OPNA ADPCM-B sample loaded";
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    info.loaded = true;
    info.sampleName = opnaAdpcmBSample.name;
    info.path = opnaAdpcmBSample.path;
    info.byteCount = static_cast<int>(opnaAdpcmBSample.bytes.size());
    info.copiedByteCount = std::min(info.byteCount, info.memoryByteCount);
    info.truncated = info.byteCount > info.memoryByteCount;
    info.statusLine = juce::String("ADPCM-B sample: ") + info.sampleName + " ("
        + juce::String(info.byteCount) + " encoded bytes";
    if (info.truncated)
        info.statusLine += ", first " + juce::String(info.copiedByteCount) + " copied";
    info.statusLine += ")";
    appendRestoreWarning(info.statusLine, restoreWarning);
    return info;
}

ChipperAudioProcessor::OpnbAdpcmSampleInfo ChipperAudioProcessor::opnbAdpcmASampleInfo() const
{
    OpnbAdpcmSampleInfo info;
    info.memoryByteCount = opnbAdpcmAMemoryBytes;
    const std::lock_guard<std::mutex> lock(opnbAdpcmASampleMutex);
    const auto restoreWarning = opnbAdpcmASampleRestoreWarning;
    if (opnbAdpcmASample.bytes.empty())
    {
        info.statusLine = "No OPNB ADPCM-A sample loaded";
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    info.loaded = true;
    info.sampleName = opnbAdpcmASample.name;
    info.path = opnbAdpcmASample.path;
    info.byteCount = static_cast<int>(opnbAdpcmASample.bytes.size());
    info.copiedByteCount = std::min(info.byteCount, info.memoryByteCount);
    info.truncated = info.byteCount > info.memoryByteCount;
    info.statusLine = juce::String("ADPCM-A sample: ") + info.sampleName + " ("
        + juce::String(info.byteCount) + " encoded bytes";
    if (info.truncated)
        info.statusLine += ", first " + juce::String(info.copiedByteCount) + " copied";
    info.statusLine += ")";
    appendRestoreWarning(info.statusLine, restoreWarning);
    return info;
}

ChipperAudioProcessor::OpnbAdpcmSampleInfo ChipperAudioProcessor::opnbAdpcmBSampleInfo() const
{
    OpnbAdpcmSampleInfo info;
    info.memoryByteCount = opnbAdpcmBMemoryBytes;
    const std::lock_guard<std::mutex> lock(opnbAdpcmBSampleMutex);
    const auto restoreWarning = opnbAdpcmBSampleRestoreWarning;
    if (opnbAdpcmBSample.bytes.empty())
    {
        info.statusLine = "No OPNB ADPCM-B sample loaded";
        appendRestoreWarning(info.statusLine, restoreWarning);
        return info;
    }

    info.loaded = true;
    info.sampleName = opnbAdpcmBSample.name;
    info.path = opnbAdpcmBSample.path;
    info.byteCount = static_cast<int>(opnbAdpcmBSample.bytes.size());
    info.copiedByteCount = std::min(info.byteCount, info.memoryByteCount);
    info.truncated = info.byteCount > info.memoryByteCount;
    info.statusLine = juce::String("ADPCM-B sample: ") + info.sampleName + " ("
        + juce::String(info.byteCount) + " encoded bytes";
    if (info.truncated)
        info.statusLine += ", first " + juce::String(info.copiedByteCount) + " copied";
    info.statusLine += ")";
    appendRestoreWarning(info.statusLine, restoreWarning);
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
        const auto shouldRestorePreviousMonoNote = activePatch.playMode != chipper::PlayMode::chipPoly
            && ! heldNotes.empty()
            && heldNotes.back().note == note;

        const auto remainingHolds = releaseHeldNote(note);
        if (remainingHolds != 0)
            return;

        core->noteOff(note);

        if (shouldRestorePreviousMonoNote && ! heldNotes.empty())
        {
            const auto held = heldNotes.back();
            applyMappedDmcSampleForMidiNote(held.note);
            applyMappedSpc700BrrSampleForMidiNote(held.note);
            applyMappedPaulaSampleForMidiNote(held.note);
            core->replayHeldNote(held.note, held.velocity);
        }
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
    const std::array<const char*, 4> fmOperatorLevelIds {
        chipper::parameters::id::fmOperator1Level,
        chipper::parameters::id::fmOperator2Level,
        chipper::parameters::id::fmOperator3Level,
        chipper::parameters::id::fmOperator4Level
    };
    const std::array<const char*, 4> fmOperatorMultiplierIds {
        chipper::parameters::id::fmOperator1Multiplier,
        chipper::parameters::id::fmOperator2Multiplier,
        chipper::parameters::id::fmOperator3Multiplier,
        chipper::parameters::id::fmOperator4Multiplier
    };
    const std::array<const char*, 4> fmOperatorAttackRateIds {
        chipper::parameters::id::fmOperator1AttackRate,
        chipper::parameters::id::fmOperator2AttackRate,
        chipper::parameters::id::fmOperator3AttackRate,
        chipper::parameters::id::fmOperator4AttackRate
    };
    const std::array<const char*, 4> fmOperatorDecayRateIds {
        chipper::parameters::id::fmOperator1DecayRate,
        chipper::parameters::id::fmOperator2DecayRate,
        chipper::parameters::id::fmOperator3DecayRate,
        chipper::parameters::id::fmOperator4DecayRate
    };
    const std::array<const char*, 4> fmOperatorSustainRateIds {
        chipper::parameters::id::fmOperator1SustainRate,
        chipper::parameters::id::fmOperator2SustainRate,
        chipper::parameters::id::fmOperator3SustainRate,
        chipper::parameters::id::fmOperator4SustainRate
    };
    const std::array<const char*, 4> fmOperatorReleaseRateIds {
        chipper::parameters::id::fmOperator1ReleaseRate,
        chipper::parameters::id::fmOperator2ReleaseRate,
        chipper::parameters::id::fmOperator3ReleaseRate,
        chipper::parameters::id::fmOperator4ReleaseRate
    };

    for (size_t i = 0; i < controlIds.size(); ++i)
        setPlainParameterValue(controlIds[i], templ.controls[i]);
    for (size_t i = 0; i < sourceIds.size(); ++i)
        setPlainParameterValue(sourceIds[i], templ.sourceEnabled[i] ? 1.0f : 0.0f);
    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setPlainParameterValue(sourceLevelIds[i], 1.0f);
    for (const auto* id : fmOperatorLevelIds)
        setPlainParameterValue(id, 0.5f);
    for (const auto* id : fmOperatorMultiplierIds)
        setPlainParameterValue(id, 0.0f);
    for (const auto* id : fmOperatorAttackRateIds)
        setPlainParameterValue(id, 0.0f);
    for (const auto* id : fmOperatorDecayRateIds)
        setPlainParameterValue(id, 0.0f);
    for (const auto* id : fmOperatorSustainRateIds)
        setPlainParameterValue(id, 0.0f);
    for (const auto* id : fmOperatorReleaseRateIds)
        setPlainParameterValue(id, 0.0f);
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
    setPlainParameterValue(chipper::parameters::id::nesDmcPlaybackMode, static_cast<float>(samplePlaybackModeForMacroTemplate(mode, templ)));
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
        applyOpnaRhythmRomToCore();
        applyOpnaAdpcmBSampleToCore();
        applyOpnbAdpcmASampleToCore();
        applyOpnbAdpcmBSampleToCore();
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
    activeOpnaRhythmRomRevision = std::numeric_limits<uint64_t>::max();
    activeOpnaAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    activeOpnbAdpcmASampleRevision = std::numeric_limits<uint64_t>::max();
    activeOpnbAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    applySelectedDmcSampleToCore();
    applySpc700BrrSampleToCore();
    applyPaulaSampleToCore();
    applyOpnaRhythmRomToCore();
    applyOpnaAdpcmBSampleToCore();
    applyOpnbAdpcmASampleToCore();
    applyOpnbAdpcmBSampleToCore();
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

    std::vector<chipper::ExternalSampleData> sampleBank;
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
                    sampleBank.push_back({ slot->bytes, slot->encoding });
            }
        }
        else if (! spc700BrrSample.bytes.empty())
        {
            resolvedSlot = 0;
            sampleBank.push_back({ spc700BrrSample.bytes, spc700BrrSample.encoding });
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

    std::vector<chipper::ExternalSampleData> sampleBank;
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
                {
                    sampleBank.push_back({
                        slot->bytes,
                        slot->encoding,
                        sampleSlotHasValidLoop(*slot),
                        slot->loopStart,
                        slot->loopEnd
                    });
                }
            }
        }
        else if (! paulaSample.bytes.empty())
        {
            resolvedSlot = 0;
            sampleBank.push_back({
                paulaSample.bytes,
                paulaSample.encoding,
                sampleSlotHasValidLoop(paulaSample),
                paulaSample.loopStart,
                paulaSample.loopEnd
            });
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

void ChipperAudioProcessor::applyOpnaRhythmRomToCore()
{
    if (core == nullptr || activeMode != chipper::ChipMode::ym2608)
        return;

    std::vector<uint8_t> selectedBytes;
    uint64_t revision = 0;
    {
        const std::lock_guard<std::mutex> lock(opnaRhythmRomMutex);
        revision = opnaRhythmRomRevision;
        selectedBytes = opnaRhythmRom.bytes;
    }

    if (revision == activeOpnaRhythmRomRevision)
        return;

    activeOpnaRhythmRomRevision = revision;
    core->setExternalSampleData(std::move(selectedBytes));
}

void ChipperAudioProcessor::applyOpnaAdpcmBSampleToCore()
{
    if (core == nullptr || activeMode != chipper::ChipMode::ym2608)
        return;

    std::vector<uint8_t> selectedBytes;
    uint64_t revision = 0;
    {
        const std::lock_guard<std::mutex> lock(opnaAdpcmBSampleMutex);
        revision = opnaAdpcmBSampleRevision;
        selectedBytes = opnaAdpcmBSample.bytes;
    }

    if (revision == activeOpnaAdpcmBSampleRevision)
        return;

    activeOpnaAdpcmBSampleRevision = revision;
    core->setExternalAdpcmBData(std::move(selectedBytes));
}

void ChipperAudioProcessor::applyOpnbAdpcmASampleToCore()
{
    if (core == nullptr || (activeMode != chipper::ChipMode::ym2610 && activeMode != chipper::ChipMode::ym2610b))
        return;

    std::vector<uint8_t> selectedBytes;
    uint64_t revision = 0;
    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmASampleMutex);
        revision = opnbAdpcmASampleRevision;
        selectedBytes = opnbAdpcmASample.bytes;
    }

    if (revision == activeOpnbAdpcmASampleRevision)
        return;

    activeOpnbAdpcmASampleRevision = revision;
    core->setExternalSampleData(std::move(selectedBytes));
}

void ChipperAudioProcessor::applyOpnbAdpcmBSampleToCore()
{
    if (core == nullptr || (activeMode != chipper::ChipMode::ym2610 && activeMode != chipper::ChipMode::ym2610b))
        return;

    std::vector<uint8_t> selectedBytes;
    uint64_t revision = 0;
    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmBSampleMutex);
        revision = opnbAdpcmBSampleRevision;
        selectedBytes = opnbAdpcmBSample.bytes;
    }

    if (revision == activeOpnbAdpcmBSampleRevision)
        return;

    activeOpnbAdpcmBSampleRevision = revision;
    core->setExternalAdpcmBData(std::move(selectedBytes));
}

chipper::PatchConfig ChipperAudioProcessor::currentPatchFromParameters() const
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    const auto playModeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::playMode)->load()));

    const auto selectedMode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto dmcPlaybackMode = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load()));
    const auto dmgStereoRoute = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::dmgStereoRoute)->load()));
    const auto sampleMapRequestsOneShot =
        (selectedMode == chipper::ChipMode::spc700 || selectedMode == chipper::ChipMode::paula)
        && dmcPlaybackMode == 2;
    const auto resolvedDmgStereoRoute = sampleMapRequestsOneShot && dmgStereoRoute == 0 ? 2 : dmgStereoRoute;

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
        resolvedDmgStereoRoute,
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
        selectedMode == chipper::ChipMode::nes && dmcPlaybackMode == 2,
        apvts.getRawParameterValue(chipper::parameters::id::spc700LoopStart)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::spc700LoopEnd)->load(),
        {
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice1SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice2SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice3SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice4SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice5SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice6SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice7SampleSlot)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::spc700Voice8SampleSlot)->load()))
        },
        {
            apvts.getRawParameterValue(chipper::parameters::id::fmOperator1Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::fmOperator2Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::fmOperator3Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::fmOperator4Level)->load()
        },
        {
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator1Multiplier)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator2Multiplier)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator3Multiplier)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator4Multiplier)->load()))
        },
        {
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator1AttackRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator2AttackRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator3AttackRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator4AttackRate)->load()))
        },
        {
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator1DecayRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator2DecayRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator3DecayRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator4DecayRate)->load()))
        },
        {
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator1SustainRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator2SustainRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator3SustainRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator4SustainRate)->load()))
        },
        {
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator1ReleaseRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator2ReleaseRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator3ReleaseRate)->load())),
            static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::fmOperator4ReleaseRate)->load()))
        });
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
        core->replayHeldNote(held.note, held.velocity);
}

void ChipperAudioProcessor::rememberHeldNote(int note, float velocity)
{
    const auto existing = std::find_if(heldNotes.begin(), heldNotes.end(), [note](const auto held) { return held.note == note; });
    if (existing != heldNotes.end())
    {
        auto held = *existing;
        held.velocity = velocity;
        held.holdCount = std::max(1, held.holdCount) + 1;
        heldNotes.erase(existing);
        heldNotes.push_back(held);
        return;
    }

    heldNotes.push_back({ note, velocity, 1 });
}

int ChipperAudioProcessor::releaseHeldNote(int note)
{
    const auto existing = std::find_if(heldNotes.begin(), heldNotes.end(), [note](const auto held) { return held.note == note; });
    if (existing == heldNotes.end())
        return -1;

    if (existing->holdCount > 1)
    {
        --existing->holdCount;
        return existing->holdCount;
    }

    heldNotes.erase(existing);
    return 0;
}

juce::AudioProcessorEditor* ChipperAudioProcessor::createEditor()
{
    return new ChipperAudioProcessorEditor(*this);
}

std::unique_ptr<juce::XmlElement> ChipperAudioProcessor::createStateXml()
{
    const auto state = apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr)
        return {};

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
    while (auto* existingOpnaRhythmRomState = xml->getChildByName(opnaRhythmRomStateTag))
        xml->removeChildElement(existingOpnaRhythmRomState, true);
    while (auto* existingOpnaAdpcmBState = xml->getChildByName(opnaAdpcmBSampleStateTag))
        xml->removeChildElement(existingOpnaAdpcmBState, true);
    while (auto* existingOpnbAdpcmAState = xml->getChildByName(opnbAdpcmASampleStateTag))
        xml->removeChildElement(existingOpnbAdpcmAState, true);
    while (auto* existingOpnbAdpcmBState = xml->getChildByName(opnbAdpcmBSampleStateTag))
        xml->removeChildElement(existingOpnbAdpcmBState, true);

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
                if (slot.sourceSampleIndex >= 0)
                    sample->setAttribute("sourceSampleIndex", slot.sourceSampleIndex);
                paulaBankState->addChildElement(sample);
            }

            xml->addChildElement(paulaBankState);
        }
    }

    {
        const std::lock_guard<std::mutex> lock(opnaRhythmRomMutex);
        if (! opnaRhythmRom.bytes.empty())
        {
            auto* rhythmRomState = new juce::XmlElement(opnaRhythmRomStateTag);
            rhythmRomState->setAttribute("path", opnaRhythmRom.path);
            xml->addChildElement(rhythmRomState);
        }
    }
    {
        const std::lock_guard<std::mutex> lock(opnaAdpcmBSampleMutex);
        if (! opnaAdpcmBSample.bytes.empty())
        {
            auto* adpcmBState = new juce::XmlElement(opnaAdpcmBSampleStateTag);
            adpcmBState->setAttribute("path", opnaAdpcmBSample.path);
            xml->addChildElement(adpcmBState);
        }
    }
    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmASampleMutex);
        if (! opnbAdpcmASample.bytes.empty())
        {
            auto* adpcmAState = new juce::XmlElement(opnbAdpcmASampleStateTag);
            adpcmAState->setAttribute("path", opnbAdpcmASample.path);
            xml->addChildElement(adpcmAState);
        }
    }
    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmBSampleMutex);
        if (! opnbAdpcmBSample.bytes.empty())
        {
            auto* adpcmBState = new juce::XmlElement(opnbAdpcmBSampleStateTag);
            adpcmBState->setAttribute("path", opnbAdpcmBSample.path);
            xml->addChildElement(adpcmBState);
        }
    }

    return xml;
}

juce::Result ChipperAudioProcessor::restoreStateXml(const juce::XmlElement& sourceXml)
{
    return restoreStateXml(sourceXml, {});
}

juce::Result ChipperAudioProcessor::restoreStateXml(const juce::XmlElement& sourceXml, const juce::File& presetDirectory)
{
    auto xml = std::make_unique<juce::XmlElement>(sourceXml);
    if (xml == nullptr || ! xml->hasTagName(apvts.state.getType()))
        return juce::Result::fail("This file does not contain Chipper plugin state.");

    pendingRegisterState.clear();
    std::vector<DmcSampleSlot> restoredDmcBank;
    DmcSampleSlot restoredSpcBrrSample;
    std::vector<DmcSampleSlot> restoredSpcBrrBank;
    DmcSampleSlot restoredPaulaSample;
    std::vector<DmcSampleSlot> restoredPaulaBank;
    DmcSampleSlot restoredOpnaRhythmRom;
    DmcSampleSlot restoredOpnaAdpcmBSample;
    DmcSampleSlot restoredOpnbAdpcmASample;
    DmcSampleSlot restoredOpnbAdpcmBSample;
    juce::StringArray dmcSampleRestoreIssues;
    juce::StringArray spc700SampleRestoreIssues;
    juce::StringArray paulaSampleRestoreIssues;
    juce::StringArray opnaRhythmRomRestoreIssues;
    juce::StringArray opnaAdpcmBSampleRestoreIssues;
    juce::StringArray opnbAdpcmASampleRestoreIssues;
    juce::StringArray opnbAdpcmBSampleRestoreIssues;
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
            if (auto result = readDmcSampleFile(resolvePresetSamplePath(*child, presetDirectory), slot); result.wasOk())
            {
                slot.included = child->getBoolAttribute("included", true);
                restoredDmcBank.push_back(std::move(slot));
            }
            else
            {
                dmcSampleRestoreIssues.add(result.getErrorMessage());
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
            if (auto result = readSpc700BrrSampleFile(resolvePresetSamplePath(*child, presetDirectory), slot); result.wasOk())
            {
                slot.included = child->getBoolAttribute("included", true);
                restoredSpcBrrBank.push_back(std::move(slot));
            }
            else
            {
                spc700SampleRestoreIssues.add(result.getErrorMessage());
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
            if (auto result = readSpc700BrrSampleFile(resolvePresetSamplePath(*spcBrrState, presetDirectory), restoredSpcBrrSample); result.wasOk())
            {
                restoredSpcBrrBank.push_back(restoredSpcBrrSample);
            }
            else
            {
                spc700SampleRestoreIssues.add(result.getErrorMessage());
            }
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

            std::vector<DmcSampleSlot> slots;
            if (auto result = readPaulaSampleFileSlots(resolvePresetSamplePath(*child, presetDirectory), slots); result.wasOk())
            {
                const auto sourceSampleIndex = child->getIntAttribute("sourceSampleIndex", -1);
                const auto included = child->getBoolAttribute("included", true);
                if (sourceSampleIndex >= 0)
                {
                    for (auto& slot : slots)
                    {
                        if (slot.sourceSampleIndex == sourceSampleIndex)
                        {
                            slot.included = included;
                            restoredPaulaBank.push_back(std::move(slot));
                            break;
                        }
                    }
                }
                else
                {
                    for (auto& slot : slots)
                    {
                        slot.included = included;
                        restoredPaulaBank.push_back(std::move(slot));
                    }
                }
            }
            else
            {
                paulaSampleRestoreIssues.add(result.getErrorMessage());
            }
        }
        if (! restoredPaulaBank.empty())
            restoredPaulaSample = restoredPaulaBank.front();
        xml->removeChildElement(paulaBankState, true);
    }

    if (auto* opnaRhythmRomState = xml->getChildByName(opnaRhythmRomStateTag))
    {
        if (auto result = readOpnaRhythmRomFile(resolvePresetSamplePath(*opnaRhythmRomState, presetDirectory), restoredOpnaRhythmRom); result.failed())
            opnaRhythmRomRestoreIssues.add(result.getErrorMessage());
        xml->removeChildElement(opnaRhythmRomState, true);
    }
    if (auto* opnaAdpcmBState = xml->getChildByName(opnaAdpcmBSampleStateTag))
    {
        if (auto result = readOpnaAdpcmBSampleFile(resolvePresetSamplePath(*opnaAdpcmBState, presetDirectory), restoredOpnaAdpcmBSample); result.failed())
            opnaAdpcmBSampleRestoreIssues.add(result.getErrorMessage());
        xml->removeChildElement(opnaAdpcmBState, true);
    }
    if (auto* opnbAdpcmAState = xml->getChildByName(opnbAdpcmASampleStateTag))
    {
        if (auto result = readOpnbAdpcmASampleFile(resolvePresetSamplePath(*opnbAdpcmAState, presetDirectory), restoredOpnbAdpcmASample); result.failed())
            opnbAdpcmASampleRestoreIssues.add(result.getErrorMessage());
        xml->removeChildElement(opnbAdpcmAState, true);
    }
    if (auto* opnbAdpcmBState = xml->getChildByName(opnbAdpcmBSampleStateTag))
    {
        if (auto result = readOpnbAdpcmBSampleFile(resolvePresetSamplePath(*opnbAdpcmBState, presetDirectory), restoredOpnbAdpcmBSample); result.failed())
            opnbAdpcmBSampleRestoreIssues.add(result.getErrorMessage());
        xml->removeChildElement(opnbAdpcmBState, true);
    }

    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    {
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);
        dmcSampleBank = std::move(restoredDmcBank);
        dmcSampleRestoreWarning = restoreWarningLine("DMC", dmcSampleRestoreIssues);
        ++dmcSampleBankRevision;
    }
    {
        const std::lock_guard<std::mutex> lock(spc700SampleMutex);
        spc700BrrSampleBank = std::move(restoredSpcBrrBank);
        spc700BrrSample = std::move(restoredSpcBrrSample);
        spc700SampleRestoreWarning = restoreWarningLine("SPC700", spc700SampleRestoreIssues);
        ++spc700BrrSampleBankRevision;
    }
    {
        const std::lock_guard<std::mutex> lock(paulaSampleMutex);
        paulaSampleBank = std::move(restoredPaulaBank);
        paulaSample = std::move(restoredPaulaSample);
        paulaSampleRestoreWarning = restoreWarningLine("Paula", paulaSampleRestoreIssues);
        ++paulaSampleBankRevision;
    }
    {
        const std::lock_guard<std::mutex> lock(opnaRhythmRomMutex);
        opnaRhythmRom = std::move(restoredOpnaRhythmRom);
        opnaRhythmRomRestoreWarning = restoreWarningLine("OPNA rhythm ROM", opnaRhythmRomRestoreIssues);
        ++opnaRhythmRomRevision;
    }
    {
        const std::lock_guard<std::mutex> lock(opnaAdpcmBSampleMutex);
        opnaAdpcmBSample = std::move(restoredOpnaAdpcmBSample);
        opnaAdpcmBSampleRestoreWarning = restoreWarningLine("OPNA ADPCM-B sample", opnaAdpcmBSampleRestoreIssues);
        ++opnaAdpcmBSampleRevision;
    }
    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmASampleMutex);
        opnbAdpcmASample = std::move(restoredOpnbAdpcmASample);
        opnbAdpcmASampleRestoreWarning = restoreWarningLine("OPNB ADPCM-A", opnbAdpcmASampleRestoreIssues);
        ++opnbAdpcmASampleRevision;
    }
    {
        const std::lock_guard<std::mutex> lock(opnbAdpcmBSampleMutex);
        opnbAdpcmBSample = std::move(restoredOpnbAdpcmBSample);
        opnbAdpcmBSampleRestoreWarning = restoreWarningLine("OPNB ADPCM-B", opnbAdpcmBSampleRestoreIssues);
        ++opnbAdpcmBSampleRevision;
    }
    activeDmcSampleBankRevision = std::numeric_limits<uint64_t>::max();
    activeDmcSampleSlot = -1;
    activeSpc700BrrSampleRevision = std::numeric_limits<uint64_t>::max();
    activeSpc700BrrSampleSlot = -1;
    activeSpc700BrrManualSlot = -1;
    activePaulaSampleRevision = std::numeric_limits<uint64_t>::max();
    activePaulaSampleSlot = -1;
    activePaulaManualSlot = -1;
    activeOpnaRhythmRomRevision = std::numeric_limits<uint64_t>::max();
    activeOpnaAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    activeOpnbAdpcmASampleRevision = std::numeric_limits<uint64_t>::max();
    activeOpnbAdpcmBSampleRevision = std::numeric_limits<uint64_t>::max();
    core.reset();
    hasObservedMacroSnapshot = false;

    return juce::Result::ok();
}

void ChipperAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = createStateXml())
        copyXmlToBinary(*xml, destData);
}

void ChipperAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr)
        restoreStateXml(*xml);
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
    auto detail = descriptor.displayName + ": " + descriptor.verification.badge + ". "
        + descriptor.verification.summary + " "
        + descriptor.summary + " "
        + core->limitations()
        + "\nEvidence: " + descriptor.verification.evidence;

    if (! descriptor.verification.verifiedBehaviors.empty())
    {
        detail += "\nVerified:";
        for (const auto& behavior : descriptor.verification.verifiedBehaviors)
            detail += "\n- " + behavior;
    }

    if (! descriptor.verification.knownGaps.empty())
    {
        detail += "\nKnown gaps:";
        for (const auto& gap : descriptor.verification.knownGaps)
            detail += "\n- " + gap;
    }

    detail += descriptor.verification.cycleAccurate
        ? "\nCycle accuracy: claimed."
        : "\nCycle accuracy: not claimed.";
    detail += descriptor.verification.hardwareValidated
        ? "\nHardware validation: complete for the documented scope."
        : "\nHardware validation: not complete.";

    return detail;
}

std::string ChipperAudioProcessor::currentCoreDebugStateJson() const
{
    if (core == nullptr)
        return "{}";

    return core->debugStateJson();
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

ChipperAudioProcessor::SampleWaveformSnapshot ChipperAudioProcessor::sampleWaveformSnapshot(chipper::ChipMode mode) const
{
    SampleWaveformSnapshot snapshot;
    std::vector<float> decoded;
    auto loopStartSample = size_t { 0 };

    if (mode == chipper::ChipMode::nes
        || mode == chipper::ChipMode::nesVrc6
        || mode == chipper::ChipMode::nesFds
        || mode == chipper::ChipMode::nesSunsoft5b
        || mode == chipper::ChipMode::nesMmc5)
    {
        const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
        const auto playbackMode = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load())), 0, 2);
        const auto loopEnabled = apvts.getRawParameterValue(chipper::parameters::id::nesDmcLoop)->load() >= 0.5f;
        const std::lock_guard<std::mutex> lock(dmcSampleMutex);

        std::vector<const DmcSampleSlot*> activeSlots;
        activeSlots.reserve(32u);
        for (const auto& slot : dmcSampleBank)
        {
            if (slot.included)
                activeSlots.push_back(&slot);
            if (activeSlots.size() >= 32u)
                break;
        }

        if (activeSlots.empty() || (playbackMode != 0 && activeDmcSampleSlot == unmappedDmcSampleSlot))
        {
            snapshot.label = activeSlots.empty() ? "No DMC samples loaded" : "No mapped DMC sample";
            if (activeSlots.empty() && dmcSampleRestoreWarning.isNotEmpty())
                snapshot.label = dmcSampleRestoreWarning;
            else
                appendRestoreWarning(snapshot.label, dmcSampleRestoreWarning);
            return snapshot;
        }

        const auto resolvedSlot = playbackMode != 0 && activeDmcSampleSlot >= 0 ? activeDmcSampleSlot : selectedSlot;
        const auto safeSlot = static_cast<size_t>(std::clamp(resolvedSlot, 0, static_cast<int>(activeSlots.size() - 1u)));
        const auto& slot = *activeSlots[safeSlot];
        decoded = decodeDmcPreview(slot.bytes);
        snapshot.label = "DMC " + juce::String(static_cast<int>(safeSlot + 1u)) + ": " + slot.name;
        snapshot.sourceSampleCount = static_cast<int>(decoded.size());
        snapshot.selectedSlot = static_cast<int>(safeSlot);
        snapshot.loaded = ! decoded.empty();
        snapshot.hasLoop = loopEnabled && snapshot.loaded;
        snapshot.loopStart = 0.0f;
        snapshot.loopEnd = 1.0f;
        appendRestoreWarning(snapshot.label, dmcSampleRestoreWarning);
    }
    else if (mode == chipper::ChipMode::spc700)
    {
        const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
        const auto playbackMode = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load())), 0, 2);
        const auto loopStartControl = apvts.getRawParameterValue(chipper::parameters::id::spc700LoopStart)->load();
        const auto loopEndControl = apvts.getRawParameterValue(chipper::parameters::id::spc700LoopEnd)->load();
        juce::String restoreWarning;
        {
            const std::lock_guard<std::mutex> lock(spc700SampleMutex);
            restoreWarning = spc700SampleRestoreWarning;
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
            else if (! spc700BrrSample.bytes.empty())
            {
                activeSlots.push_back(&spc700BrrSample);
            }

            if (! activeSlots.empty() && (playbackMode == 0 || activeSpc700BrrSampleSlot >= 0))
            {
                auto resolvedSlot = playbackMode != 0 && activeSpc700BrrSampleSlot >= 0 ? activeSpc700BrrSampleSlot : selectedSlot;
                for (int i = 0; i < static_cast<int>(activeSlots.size()); ++i)
                {
                    if (activeSlots[static_cast<size_t>(i)]->path == spc700BrrSample.path)
                    {
                        resolvedSlot = i;
                        break;
                    }
                }
                const auto safeSlot = static_cast<size_t>(std::clamp(resolvedSlot, 0, static_cast<int>(activeSlots.size() - 1u)));
                const auto& slot = *activeSlots[safeSlot];
                if (slot.encoding == chipper::ExternalSampleEncoding::spc700Brr)
                    decoded = decodeBrrPreview(slot.bytes, loopStartSample);
                else
                    decoded = decodePcm8Preview(slot.bytes);
                snapshot.label = "SPC700 " + juce::String(static_cast<int>(safeSlot + 1u)) + ": " + slot.name;
                snapshot.selectedSlot = static_cast<int>(safeSlot);
                snapshot.loaded = ! decoded.empty();
            }
        }
        appendRestoreWarning(snapshot.label, restoreWarning);

        if (decoded.empty())
        {
            const auto patch = currentPatchFromParameters();
            decoded = generatedSamplePreview(64u, [patch](size_t i)
            {
                return chipper::generatedSampleValueForPatch(chipper::ChipMode::spc700, patch, 0u, i);
            });
            snapshot.label = "Generated SPC700 sample";
            appendRestoreWarning(snapshot.label, restoreWarning);
            snapshot.loaded = ! decoded.empty();
        }

        snapshot.sourceSampleCount = static_cast<int>(decoded.size());
        snapshot.hasLoop = snapshot.loaded;
        const auto defaultLoopStart = normalizedLoopStart(loopStartSample, decoded.size());
        const auto explicitLoop = loopStartControl > 0.0005f || loopEndControl < 0.9995f;
        snapshot.loopStart = explicitLoop ? std::clamp(loopStartControl, 0.0f, 1.0f) : defaultLoopStart;
        snapshot.loopEnd = std::clamp(loopEndControl, 0.0f, 1.0f);
        if (snapshot.loopEnd <= snapshot.loopStart)
            snapshot.loopEnd = std::min(1.0f, snapshot.loopStart + 0.01f);
    }
    else if (mode == chipper::ChipMode::paula)
    {
        const auto selectedSlot = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcSampleSlot)->load()));
        const auto playbackMode = std::clamp(static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::nesDmcPlaybackMode)->load())), 0, 2);
        juce::String restoreWarning;
        {
            const std::lock_guard<std::mutex> lock(paulaSampleMutex);
            restoreWarning = paulaSampleRestoreWarning;
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
            else if (! paulaSample.bytes.empty())
            {
                activeSlots.push_back(&paulaSample);
            }

            if (! activeSlots.empty() && (playbackMode == 0 || activePaulaSampleSlot >= 0))
            {
                auto resolvedSlot = playbackMode != 0 && activePaulaSampleSlot >= 0 ? activePaulaSampleSlot : selectedSlot;
                for (int i = 0; i < static_cast<int>(activeSlots.size()); ++i)
                {
                    if (activeSlots[static_cast<size_t>(i)]->path == paulaSample.path
                        && activeSlots[static_cast<size_t>(i)]->sourceSampleIndex == paulaSample.sourceSampleIndex)
                    {
                        resolvedSlot = i;
                        break;
                    }
                }
                const auto safeSlot = static_cast<size_t>(std::clamp(resolvedSlot, 0, static_cast<int>(activeSlots.size() - 1u)));
                const auto& slot = *activeSlots[safeSlot];
                decoded = decodePcm8Preview(slot.bytes);
                snapshot.label = "Paula " + juce::String(static_cast<int>(safeSlot + 1u)) + ": " + slot.name;
                snapshot.selectedSlot = static_cast<int>(safeSlot);
                snapshot.loaded = ! decoded.empty();
                if (sampleSlotHasValidLoop(slot))
                {
                    snapshot.hasLoop = true;
                    snapshot.loopStart = normalizedLoopStart(slot.loopStart, decoded.size());
                    snapshot.loopEnd = normalizedLoopEnd(slot.loopEnd, decoded.size());
                    if (snapshot.loopEnd <= snapshot.loopStart)
                        snapshot.loopEnd = std::min(1.0f, snapshot.loopStart + 0.01f);
                }
            }
        }
        appendRestoreWarning(snapshot.label, restoreWarning);

        if (decoded.empty())
        {
            const auto patch = currentPatchFromParameters();
            decoded = generatedSamplePreview(64u, [patch](size_t i)
            {
                return chipper::generatedSampleValueForPatch(chipper::ChipMode::paula, patch, 0u, i);
            });
            snapshot.label = "Generated Paula sample";
            appendRestoreWarning(snapshot.label, restoreWarning);
            snapshot.loaded = ! decoded.empty();
        }

        snapshot.sourceSampleCount = static_cast<int>(decoded.size());
    }
    else if (mode == chipper::ChipMode::ym2608)
    {
        {
            const std::lock_guard<std::mutex> lock(opnaAdpcmBSampleMutex);
            if (! opnaAdpcmBSample.bytes.empty())
            {
                decoded = decodePcm8Preview(opnaAdpcmBSample.bytes);
                snapshot.label = "OPNA ADPCM-B sample: " + opnaAdpcmBSample.name;
                snapshot.sourceSampleCount = static_cast<int>(decoded.size());
                snapshot.loaded = ! decoded.empty();
                appendRestoreWarning(snapshot.label, opnaAdpcmBSampleRestoreWarning);
            }
        }

        if (decoded.empty())
        {
            const std::lock_guard<std::mutex> lock(opnaRhythmRomMutex);
            if (opnaRhythmRom.bytes.empty())
            {
                snapshot.label = "Generated OPNA ADPCM-A rhythm ROM";
                appendRestoreWarning(snapshot.label, opnaRhythmRomRestoreWarning);
                return snapshot;
            }

            decoded = decodePcm8Preview(opnaRhythmRom.bytes);
            snapshot.label = "OPNA rhythm ROM: " + opnaRhythmRom.name;
            snapshot.sourceSampleCount = static_cast<int>(decoded.size());
            snapshot.loaded = ! decoded.empty();
            appendRestoreWarning(snapshot.label, opnaRhythmRomRestoreWarning);
        }
    }
    else if (mode == chipper::ChipMode::ym2610 || mode == chipper::ChipMode::ym2610b)
    {
        {
            const std::lock_guard<std::mutex> lock(opnbAdpcmBSampleMutex);
            if (! opnbAdpcmBSample.bytes.empty())
            {
                decoded = decodePcm8Preview(opnbAdpcmBSample.bytes);
                snapshot.label = "OPNB ADPCM-B sample: " + opnbAdpcmBSample.name;
                snapshot.sourceSampleCount = static_cast<int>(decoded.size());
                snapshot.loaded = ! decoded.empty();
                appendRestoreWarning(snapshot.label, opnbAdpcmBSampleRestoreWarning);
            }
        }

        if (decoded.empty())
        {
            const std::lock_guard<std::mutex> lock(opnbAdpcmASampleMutex);
            if (opnbAdpcmASample.bytes.empty())
            {
                snapshot.label = "No OPNB ADPCM sample loaded";
                appendRestoreWarning(snapshot.label, opnbAdpcmASampleRestoreWarning);
                appendRestoreWarning(snapshot.label, opnbAdpcmBSampleRestoreWarning);
                return snapshot;
            }

            decoded = decodePcm8Preview(opnbAdpcmASample.bytes);
            snapshot.label = "OPNB ADPCM-A sample: " + opnbAdpcmASample.name;
            snapshot.sourceSampleCount = static_cast<int>(decoded.size());
            snapshot.loaded = ! decoded.empty();
            appendRestoreWarning(snapshot.label, opnbAdpcmASampleRestoreWarning);
        }
    }

    snapshot.samples = waveformPreviewFromSamples(decoded);
    return snapshot;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipperAudioProcessor();
}
