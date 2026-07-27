#pragma once

#include <JuceHeader.h>

namespace chipper::state
{
inline constexpr auto schemaVersionAttribute = "stateSchemaVersion";
inline constexpr int currentSchemaVersion = 5;
inline constexpr size_t maxRestoredRegisterWrites = 4096u;

juce::Result validateAndMigrate(juce::XmlElement& xml, const juce::Identifier& expectedRootType);
juce::File resolvePortableAssetPath(const juce::XmlElement& assetState, const juce::File& presetDirectory);
void annotatePortableAssetReferences(juce::XmlElement& xml, const juce::File& presetDirectory);
}
