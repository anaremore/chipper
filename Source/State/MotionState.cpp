#include "MotionState.h"

#include "Engine/ChipCore.h"

#include <array>
#include <optional>

namespace chipper::state
{

namespace
{
int rateValue(MotionRate rate) noexcept
{
    return static_cast<int>(static_cast<uint8_t>(rate));
}

std::optional<MotionRate> rateFromValue(int value) noexcept
{
    switch (value)
    {
        case 8: return MotionRate::eighth;
        case 16: return MotionRate::sixteenth;
        case 32: return MotionRate::thirtySecond;
        case 64: return MotionRate::sixtyFourth;
        default: return std::nullopt;
    }
}

bool modeIndex(ChipMode mode, size_t& index) noexcept
{
    index = static_cast<size_t>(mode);
    return index < motionStateModeCount;
}

const char* stateIdForMode(ChipMode mode) noexcept
{
    switch (mode)
    {
        case ChipMode::nes: return "nes";
        case ChipMode::nesVrc6: return "nesVrc6";
        case ChipMode::nesFds: return "nesFds";
        case ChipMode::nesSunsoft5b: return "nesSunsoft5b";
        case ChipMode::nesMmc5: return "nesMmc5";
        case ChipMode::nesVrc7: return "nesVrc7";
        case ChipMode::dmg: return "dmg";
        case ChipMode::sid: return "sid";
        case ChipMode::ym2149: return "ym2149";
        case ChipMode::sn76489: return "sn76489";
        case ChipMode::ym2612: return "ym2612";
        case ChipMode::opl3: return "opl3";
        case ChipMode::spc700: return "spc700";
        case ChipMode::pokey: return "pokey";
        case ChipMode::paula: return "paula";
        case ChipMode::huc6280: return "huc6280";
        case ChipMode::namcoWsg: return "namcoWsg";
        case ChipMode::ym2151: return "ym2151";
        case ChipMode::ym2413: return "ym2413";
        case ChipMode::scc: return "scc";
        case ChipMode::ym2203: return "ym2203";
        case ChipMode::ym2608: return "ym2608";
        case ChipMode::ym2610: return "ym2610";
        case ChipMode::ym2610b: return "ym2610b";
        case ChipMode::saa1099: return "saa1099";
        case ChipMode::pcSpeaker: return "pcSpeaker";
        case ChipMode::zxSpectrumBeeper: return "zxSpectrumBeeper";
    }
    return "unknown";
}
}

MotionPattern* MotionState::patternForMode(ChipMode mode) noexcept
{
    size_t index = 0u;
    return modeIndex(mode, index) ? &patterns[index] : nullptr;
}

const MotionPattern* MotionState::patternForMode(ChipMode mode) const noexcept
{
    size_t index = 0u;
    return modeIndex(mode, index) ? &patterns[index] : nullptr;
}

std::unique_ptr<juce::XmlElement> createMotionStateXml(const MotionState& state)
{
    auto root = std::make_unique<juce::XmlElement>(motionStateTag);
    const MotionPattern empty;
    for (size_t index = 0; index < state.patterns.size(); ++index)
    {
        const auto pattern = sanitizeMotionPattern(state.patterns[index]);
        if (pattern == empty)
            continue;

        auto* entry = root->createNewChildElement(motionPatternStateTag);
        entry->setAttribute("mode", stateIdForMode(static_cast<ChipMode>(index)));
        entry->setAttribute("enabled", pattern.enabled ? 1 : 0);
        entry->setAttribute("rate", rateValue(pattern.rate));
        entry->setAttribute("length", static_cast<int>(pattern.length));
        for (size_t step = 0; step < pattern.steps.size(); ++step)
        {
            const auto suffix = juce::String(static_cast<int>(step));
            entry->setAttribute("p" + suffix, static_cast<int>(pattern.steps[step].pitch));
            entry->setAttribute("v" + suffix, static_cast<int>(pattern.steps[step].level));
            entry->setAttribute("g" + suffix, static_cast<int>(pattern.steps[step].gate));
        }
    }
    return root->getNumChildElements() > 0 ? std::move(root) : nullptr;
}

juce::Result restoreMotionStateXml(const juce::XmlElement& xml, MotionState& state)
{
    if (! xml.hasTagName(motionStateTag))
        return juce::Result::fail("Unexpected tracker motion state tag");

    MotionState restored;
    std::array<bool, motionStateModeCount> seen {};
    auto entryCount = 0u;
    for (const auto* entry : xml.getChildIterator())
    {
        if (entry == nullptr || ! entry->hasTagName(motionPatternStateTag))
            continue;
        if (++entryCount > motionStateModeCount)
            return juce::Result::fail("Tracker motion state contains too many patterns");

        const auto parsedMode = parseChipMode(entry->getStringAttribute("mode").toStdString());
        if (! parsedMode.has_value())
            return juce::Result::fail("Tracker motion state contains an unknown chip mode");
        const auto index = static_cast<size_t>(*parsedMode);
        if (index >= restored.patterns.size() || seen[index])
            return juce::Result::fail("Tracker motion state contains a duplicate chip pattern");
        seen[index] = true;

        const auto parsedRate = rateFromValue(entry->getIntAttribute("rate", 0));
        const auto length = entry->getIntAttribute("length", 0);
        if (! parsedRate.has_value() || length < 1 || length > static_cast<int>(motionStepCount))
            return juce::Result::fail("Tracker motion state contains an invalid rate or length");

        MotionPattern pattern;
        pattern.enabled = entry->getBoolAttribute("enabled", false);
        pattern.rate = *parsedRate;
        pattern.length = static_cast<uint8_t>(length);
        for (size_t step = 0; step < pattern.steps.size(); ++step)
        {
            const auto suffix = juce::String(static_cast<int>(step));
            const auto pitch = entry->getIntAttribute("p" + suffix, 0);
            const auto level = entry->getIntAttribute("v" + suffix, motionMaximumLevel);
            const auto gate = entry->getIntAttribute("g" + suffix, 0);
            if (pitch < motionMinimumPitch || pitch > motionMaximumPitch
                || level < 0 || level > motionMaximumLevel
                || gate < static_cast<int>(MotionGate::hold) || gate > static_cast<int>(MotionGate::cut))
                return juce::Result::fail("Tracker motion state contains an invalid step");

            pattern.steps[step].pitch = static_cast<int8_t>(pitch);
            pattern.steps[step].level = static_cast<uint8_t>(level);
            pattern.steps[step].gate = static_cast<MotionGate>(gate);
        }
        restored.patterns[index] = pattern;
    }

    state = restored;
    return juce::Result::ok();
}

} // namespace chipper::state
