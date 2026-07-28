#pragma once

#include <JuceHeader.h>

namespace chipper::state
{
inline constexpr auto schemaVersionAttribute = "stateSchemaVersion";
inline constexpr int currentSchemaVersion = 9;
inline constexpr size_t maxRestoredRegisterWrites = 4096u;
inline constexpr auto embeddedSampleStateTag = "CHIPPER_EMBEDDED_SAMPLE";
inline constexpr int embeddedSampleFormatVersion = 1;
inline constexpr size_t maxEmbeddedSampleSlotsPerBank = 32u;
inline constexpr size_t maxEmbeddedBankSlotBytes = 262144u;
inline constexpr size_t maxEmbeddedSamplePayloads = 101u;
inline constexpr size_t maxEmbeddedProjectBytes = 16u * 1024u * 1024u;
inline constexpr size_t maxHostStateBytes = 32u * 1024u * 1024u;

juce::Result validateAndMigrate(juce::XmlElement& xml, const juce::Identifier& expectedRootType);
juce::File resolvePortableAssetPath(const juce::XmlElement& assetState, const juce::File& presetDirectory);
void annotatePortableAssetReferences(juce::XmlElement& xml, const juce::File& presetDirectory);
}
