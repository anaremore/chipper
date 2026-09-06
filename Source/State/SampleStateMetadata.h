#pragma once
#include "SampleAsset.h"
#include "PluginStateSchema.h"
namespace chipper::state::assets
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
constexpr auto opn2DacSampleStateTag = "CHIPPER_OPN2_DAC_SAMPLE";
constexpr auto opnaRhythmRomStateTag = "CHIPPER_OPNA_RHYTHM_ROM";
constexpr auto opnaAdpcmBSampleStateTag = "CHIPPER_OPNA_ADPCM_B_SAMPLE";
constexpr auto opnbAdpcmASampleStateTag = "CHIPPER_OPNB_ADPCM_A_SAMPLE";
constexpr auto opnbAdpcmBSampleStateTag = "CHIPPER_OPNB_ADPCM_B_SAMPLE";
constexpr auto adpcmARegionStateTag = "CHIPPER_ADPCM_A_REGION";
constexpr auto adpcmARegionsBankMode = "regions";
constexpr size_t maxEmbeddedOpn2Bytes = 1u * 1024u * 1024u;
constexpr size_t maxEmbeddedOpnaRhythmBytes = 64u * 1024u;
constexpr size_t maxEmbeddedOpnaAdpcmBBytes = 1u * 1024u * 1024u;
constexpr size_t maxEmbeddedOpnbAdpcmABytes = 2u * 1024u * 1024u;
constexpr size_t maxEmbeddedOpnbAdpcmBBytes = 16u * 1024u * 1024u;

struct EmbeddedSampleBudget
{
    size_t payloadCount = 0u;
    size_t totalBytes = 0u;
};

inline juce::String externalSampleEncodingToken(chipper::ExternalSampleEncoding encoding)
{
    switch (encoding)
    {
        case chipper::ExternalSampleEncoding::spc700Brr: return "spc700Brr";
        case chipper::ExternalSampleEncoding::signedPcm8: return "signedPcm8";
        case chipper::ExternalSampleEncoding::rawBytes:
        default: return "rawBytes";
    }
}

inline uint32_t embeddedSampleChecksum(const uint8_t* bytes, size_t byteCount)
{
    auto hash = uint32_t { 2166136261u };
    for (size_t index = 0; index < byteCount; ++index)
    {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

inline size_t sampleSourceByteCount(const SampleAsset& slot)
{
    return slot.sourceByteCount > 0u ? slot.sourceByteCount : slot.bytes.size();
}

inline void addSampleReferenceMetadata(juce::XmlElement& sampleState,
                                const SampleAsset& slot)
{
    sampleState.setAttribute("name", slot.name);
    sampleState.setAttribute("encoding", externalSampleEncodingToken(slot.encoding));
    sampleState.setAttribute("hasLoop", slot.hasLoop ? 1 : 0);
    sampleState.setAttribute("loopStart", juce::String(static_cast<juce::int64>(slot.loopStart)));
    sampleState.setAttribute("loopEnd", juce::String(static_cast<juce::int64>(slot.loopEnd)));
    sampleState.setAttribute("sourceSampleIndex", slot.sourceSampleIndex);
    sampleState.setAttribute("sourceByteCount",
                             juce::String(static_cast<juce::int64>(sampleSourceByteCount(slot))));
    if (slot.sourceSampleCount > 0u)
        sampleState.setAttribute("sourceSampleCount",
                                 juce::String(static_cast<juce::int64>(slot.sourceSampleCount)));
}

inline void addOpn2DacPlaybackMetadata(juce::XmlElement& sampleState,
                                const SampleAsset& slot)
{
    sampleState.setAttribute("sourceRateHz", slot.sourceRateHz);
    sampleState.setAttribute("rootNote", slot.rootNote);
    sampleState.setAttribute("trimStart", juce::String(static_cast<juce::int64>(slot.trimStart)));
    sampleState.setAttribute("trimEnd", juce::String(static_cast<juce::int64>(slot.trimEnd)));
    sampleState.setAttribute("tailBehavior", slot.holdLastValue ? "hold" : "center");
}

inline bool addEmbeddedSamplePayload(juce::XmlElement& sampleState,
                              const SampleAsset& slot,
                              size_t maxPayloadBytes,
                              EmbeddedSampleBudget& budget)
{
    if (slot.bytes.empty()
        || slot.bytes.size() > maxPayloadBytes
        || budget.payloadCount >= chipper::state::maxEmbeddedSamplePayloads
        || slot.bytes.size() > chipper::state::maxEmbeddedProjectBytes - budget.totalBytes)
        return false;

    auto* embedded = new juce::XmlElement(chipper::state::embeddedSampleStateTag);
    embedded->setAttribute("formatVersion", chipper::state::embeddedSampleFormatVersion);
    embedded->setAttribute("byteCount", juce::String(static_cast<juce::int64>(slot.bytes.size())));
    embedded->setAttribute("checksum",
                           "fnv1a32:" + juce::String(static_cast<juce::int64>(
                               embeddedSampleChecksum(slot.bytes.data(), slot.bytes.size()))));
    const juce::MemoryBlock payload(slot.bytes.data(), slot.bytes.size());
    embedded->addTextElement(payload.toBase64Encoding());
    sampleState.addChildElement(embedded);
    ++budget.payloadCount;
    budget.totalBytes += slot.bytes.size();
    return true;
}

}
