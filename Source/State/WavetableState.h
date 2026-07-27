#pragma once

#include <JuceHeader.h>

#include "Engine/WavetableData.h"

namespace chipper::state
{

inline constexpr auto wavetableStateTag = "CHIPPER_WAVETABLE_MEMORY";
inline constexpr auto wavetableLaneStateTag = "WAVE";
inline constexpr size_t maxRestoredWavetableLanes = 19u;

struct WavetableState
{
    WavetableMemory huc6280;
    WavetableMemory namcoWsg;
    WavetableMemory scc;

    WavetableMemory* memoryForMode(ChipMode mode) noexcept;
    const WavetableMemory* memoryForMode(ChipMode mode) const noexcept;
};

std::unique_ptr<juce::XmlElement> createWavetableStateXml(const WavetableState& state);
juce::Result restoreWavetableStateXml(const juce::XmlElement& xml, WavetableState& state);

} // namespace chipper::state
