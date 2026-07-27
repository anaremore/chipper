#include "WavetableState.h"

#include "Engine/ChipCore.h"

#include <array>

namespace chipper::state
{
namespace
{
constexpr std::array<ChipMode, 3> editableModes {
    ChipMode::huc6280,
    ChipMode::namcoWsg,
    ChipMode::scc
};

const char* stateIdForMode(ChipMode mode) noexcept
{
    switch (mode)
    {
        case ChipMode::huc6280: return "huc6280";
        case ChipMode::namcoWsg: return "namcoWsg";
        case ChipMode::scc: return "scc";
        default: return "";
    }
}

bool modeForStateId(const juce::String& id, ChipMode& mode) noexcept
{
    if (id == "huc6280")
        mode = ChipMode::huc6280;
    else if (id == "namcoWsg")
        mode = ChipMode::namcoWsg;
    else if (id == "scc")
        mode = ChipMode::scc;
    else
        return false;
    return true;
}

juce::String encodeLane(const WavetableLane& lane)
{
    static constexpr char digits[] = "0123456789ABCDEF";
    juce::String encoded;
    encoded.preallocateBytes(static_cast<int>(lane.size() * 2u));
    for (const auto sample : lane)
    {
        encoded += juce::String::charToString(digits[(sample >> 4u) & 0x0fu]);
        encoded += juce::String::charToString(digits[sample & 0x0fu]);
    }
    return encoded;
}

int hexDigitValue(juce::juce_wchar character) noexcept
{
    if (character >= '0' && character <= '9')
        return static_cast<int>(character - '0');
    if (character >= 'a' && character <= 'f')
        return 10 + static_cast<int>(character - 'a');
    if (character >= 'A' && character <= 'F')
        return 10 + static_cast<int>(character - 'A');
    return -1;
}

juce::Result decodeLane(ChipMode mode, const juce::String& encoded, WavetableLane& lane)
{
    if (encoded.length() != static_cast<int>(wavetableSampleCount * 2u))
        return juce::Result::fail("A custom Wave RAM lane must contain exactly 32 samples.");

    const auto spec = wavetableSpecForMode(mode);
    for (size_t index = 0; index < lane.size(); ++index)
    {
        const auto high = hexDigitValue(encoded[static_cast<int>(index * 2u)]);
        const auto low = hexDigitValue(encoded[static_cast<int>(index * 2u + 1u)]);
        if (high < 0 || low < 0)
            return juce::Result::fail("A custom Wave RAM lane contains invalid hexadecimal data.");

        const auto value = static_cast<uint8_t>((high << 4) | low);
        if (value > spec.maximumSampleValue)
            return juce::Result::fail("A custom Wave RAM sample exceeds the native chip bit depth.");
        lane[index] = value;
    }
    return juce::Result::ok();
}
}

WavetableMemory* WavetableState::memoryForMode(ChipMode mode) noexcept
{
    switch (mode)
    {
        case ChipMode::huc6280: return &huc6280;
        case ChipMode::namcoWsg: return &namcoWsg;
        case ChipMode::scc: return &scc;
        default: return nullptr;
    }
}

const WavetableMemory* WavetableState::memoryForMode(ChipMode mode) const noexcept
{
    switch (mode)
    {
        case ChipMode::huc6280: return &huc6280;
        case ChipMode::namcoWsg: return &namcoWsg;
        case ChipMode::scc: return &scc;
        default: return nullptr;
    }
}

std::unique_ptr<juce::XmlElement> createWavetableStateXml(const WavetableState& state)
{
    auto xml = std::make_unique<juce::XmlElement>(wavetableStateTag);
    auto count = 0;
    for (const auto mode : editableModes)
    {
        const auto* memory = state.memoryForMode(mode);
        const auto spec = wavetableSpecForMode(mode);
        if (memory == nullptr)
            continue;

        for (size_t lane = 0; lane < spec.laneCount; ++lane)
        {
            if (! memory->customLanes[lane])
                continue;

            auto* wave = new juce::XmlElement(wavetableLaneStateTag);
            wave->setAttribute("mode", stateIdForMode(mode));
            wave->setAttribute("lane", static_cast<int>(lane));
            wave->setAttribute("data", encodeLane(memory->lanes[lane]));
            xml->addChildElement(wave);
            ++count;
        }
    }

    xml->setAttribute("count", count);
    return count > 0 ? std::move(xml) : nullptr;
}

juce::Result restoreWavetableStateXml(const juce::XmlElement& xml, WavetableState& state)
{
    if (! xml.hasTagName(wavetableStateTag))
        return juce::Result::fail("This state does not contain Chipper Wave RAM.");

    WavetableState restored;
    size_t laneCount = 0u;
    for (const auto* child : xml.getChildIterator())
    {
        if (child == nullptr || ! child->hasTagName(wavetableLaneStateTag))
            continue;
        if (++laneCount > maxRestoredWavetableLanes)
            return juce::Result::fail("This Chipper state contains too many custom Wave RAM lanes.");

        ChipMode mode {};
        if (! modeForStateId(child->getStringAttribute("mode"), mode))
            return juce::Result::fail("This Chipper state contains an unknown Wave RAM chip mode.");

        const auto laneText = child->getStringAttribute("lane").trim();
        if (laneText.isEmpty() || laneText.length() > 2 || ! laneText.containsOnly("0123456789"))
            return juce::Result::fail("This Chipper state contains an invalid Wave RAM lane index.");
        const auto lane = static_cast<size_t>(laneText.getIntValue());
        const auto spec = wavetableSpecForMode(mode);
        if (lane >= spec.laneCount)
            return juce::Result::fail("This Chipper state contains an out-of-range Wave RAM lane.");

        auto* memory = restored.memoryForMode(mode);
        if (memory == nullptr)
            return juce::Result::fail("This Chipper state contains unsupported Wave RAM data.");
        if (memory->customLanes[lane])
            return juce::Result::fail("This Chipper state contains a duplicate Wave RAM lane.");

        if (const auto result = decodeLane(mode, child->getStringAttribute("data"), memory->lanes[lane]); result.failed())
            return result;
        memory->customLanes[lane] = true;
    }

    state = std::move(restored);
    return juce::Result::ok();
}

} // namespace chipper::state
