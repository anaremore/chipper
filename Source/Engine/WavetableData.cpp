#include "WavetableData.h"

#include "ChipCore.h"

#include <algorithm>
#include <cmath>

namespace chipper
{
namespace
{
constexpr auto twoPi = 6.283185307179586476925286766559;

size_t waveChoiceChannelCount(ChipMode mode) noexcept
{
    switch (mode)
    {
        case ChipMode::namcoWsg: return 8u;
        case ChipMode::huc6280: return 6u;
        case ChipMode::scc: return 5u;
        case ChipMode::paula: return 4u;
        default: return 1u;
    }
}

uint8_t generatedSample(ChipMode mode,
                        int choice,
                        size_t channel,
                        size_t sampleIndex,
                        double skew)
{
    const auto i = sampleIndex & (wavetableSampleCount - 1u);
    const auto phase = static_cast<double>(i) / static_cast<double>(wavetableSampleCount);

    if (mode == ChipMode::huc6280)
    {
        auto sample = 0;
        switch (choice)
        {
            case 1: sample = static_cast<int>(std::round(31.0 * phase)); break;
            case 2:
                sample = i < 16u
                    ? static_cast<int>(std::round(31.0 * (static_cast<double>(i) / 15.0)))
                    : static_cast<int>(std::round(31.0 * (1.0 - static_cast<double>(i - 16u) / 15.0)));
                break;
            case 3: sample = i < 16u ? 31 : 0; break;
            case 4: sample = (static_cast<int>(i) * 13 + static_cast<int>(channel) * 7) & 31; break;
            case 0:
            default: sample = static_cast<int>(std::round(15.5 + 15.5 * std::sin(twoPi * phase))); break;
        }
        return static_cast<uint8_t>(std::clamp(sample, 0, 31));
    }

    if (mode == ChipMode::namcoWsg)
    {
        auto sample = 8;
        switch (choice)
        {
            case 1: sample = static_cast<int>(std::round(15.0 * phase)); break;
            case 2:
                sample = i < 16u
                    ? static_cast<int>(std::round(15.0 * (static_cast<double>(i) / 15.0)))
                    : static_cast<int>(std::round(15.0 * (1.0 - static_cast<double>(i - 16u) / 15.0)));
                break;
            case 3:
                sample = i < static_cast<size_t>(std::round(4.0 + skew * 24.0)) ? 15 : 0;
                break;
            case 4: sample = (static_cast<int>(i) * 5 + static_cast<int>(channel) * 3) & 15; break;
            case 0:
            default: sample = static_cast<int>(std::round(7.5 + 7.5 * std::sin(twoPi * phase))); break;
        }
        return static_cast<uint8_t>(std::clamp(sample, 0, 15));
    }

    if (mode == ChipMode::scc)
    {
        auto sample = 128;
        switch (choice)
        {
            case 1: sample = static_cast<int>(std::round(255.0 * phase)); break;
            case 2:
                sample = i < 16u
                    ? static_cast<int>(std::round(255.0 * (static_cast<double>(i) / 15.0)))
                    : static_cast<int>(std::round(255.0 * (1.0 - static_cast<double>(i - 16u) / 15.0)));
                break;
            case 3:
                sample = i < static_cast<size_t>(std::round(4.0 + skew * 24.0)) ? 255 : 0;
                break;
            case 4: sample = (static_cast<int>(i) * 17 + static_cast<int>(channel) * 29) & 255; break;
            case 0:
            default: sample = static_cast<int>(std::round(128.0 + 127.0 * std::sin(twoPi * phase))); break;
        }
        return static_cast<uint8_t>(std::clamp(sample, 0, 255));
    }

    return 0u;
}
}

bool supportsDirectWavetableEditing(ChipMode mode) noexcept
{
    return wavetableSpecForMode(mode).laneCount > 0u;
}

WavetableSpec wavetableSpecForMode(ChipMode mode) noexcept
{
    switch (mode)
    {
        case ChipMode::huc6280: return { 6u, 5u, 31u };
        case ChipMode::namcoWsg: return { 8u, 4u, 15u };
        case ChipMode::scc: return { 5u, 8u, 255u };
        default: return {};
    }
}

uint8_t quantizeWavetableSample(ChipMode mode, int sample) noexcept
{
    const auto spec = wavetableSpecForMode(mode);
    if (spec.laneCount == 0u)
        return 0u;
    return static_cast<uint8_t>(std::clamp(sample, 0, static_cast<int>(spec.maximumSampleValue)));
}

WavetableLane quantizeWavetableLane(ChipMode mode, const WavetableLane& lane) noexcept
{
    auto quantized = lane;
    for (auto& sample : quantized)
        sample = quantizeWavetableSample(mode, sample);
    return quantized;
}

uint8_t wavetableWaveShapeForChannel(ChipMode mode, const PatchConfig& patch, size_t channel)
{
    const std::array<int, maximumWavetableLaneCount> choices {
        patch.waveShape,
        patch.sidVoice2WaveShape,
        patch.sidVoice3WaveShape,
        patch.pulse2Duty,
        patch.dmgWaveLevel,
        patch.snNoiseMode,
        patch.ymEnvelopeShape,
        patch.dmgStereoRoute
    };

    const auto channelCount = waveChoiceChannelCount(mode);
    auto baseChoice = std::clamp(patch.waveShape, 0, 4);
    if (mode == ChipMode::paula && baseChoice == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::bass: baseChoice = 2; break;
            case MacroKind::drum:
            case MacroKind::hit: baseChoice = 4; break;
            case MacroKind::lead:
            case MacroKind::arp: baseChoice = 1; break;
            default: baseChoice = 3; break;
        }
    }

    auto choice = choices[std::min(channel, channelCount - 1u)];
    choice = std::clamp(choice, 0, 4);
    return static_cast<uint8_t>(choice == 0 ? baseChoice : choice);
}

bool wavetablePatchContentMatches(const PatchConfig& first, const PatchConfig& second) noexcept
{
    constexpr auto tolerance = 0.0001f;
    return std::abs(first.control3 - second.control3) < tolerance
        && first.waveShape == second.waveShape
        && first.sidVoice2WaveShape == second.sidVoice2WaveShape
        && first.sidVoice3WaveShape == second.sidVoice3WaveShape
        && first.pulse2Duty == second.pulse2Duty
        && first.dmgWaveLevel == second.dmgWaveLevel
        && first.snNoiseMode == second.snNoiseMode
        && first.ymEnvelopeShape == second.ymEnvelopeShape
        && first.dmgStereoRoute == second.dmgStereoRoute
        && first.wavetableMemory == second.wavetableMemory;
}

uint8_t generatedWavetableSampleForPatch(ChipMode mode,
                                         const PatchConfig& patch,
                                         size_t channel,
                                         size_t sampleIndex)
{
    const auto choice = static_cast<int>(wavetableWaveShapeForChannel(mode, patch, channel));
    const auto skew = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
    return generatedSample(mode, choice, channel, sampleIndex, skew);
}

uint8_t wavetableRamSampleForPatch(ChipMode mode,
                                   const PatchConfig& patch,
                                   size_t channel,
                                   size_t sampleIndex)
{
    const auto spec = wavetableSpecForMode(mode);
    if (channel >= spec.laneCount)
        return 0u;

    const auto sample = sampleIndex & (wavetableSampleCount - 1u);
    if (patch.wavetableMemory.customLanes[channel])
        return quantizeWavetableSample(mode, patch.wavetableMemory.lanes[channel][sample]);

    return generatedWavetableSampleForPatch(mode, patch, channel, sample);
}

WavetableLane wavetableLaneForPatch(ChipMode mode, const PatchConfig& patch, size_t channel)
{
    WavetableLane lane {};
    for (size_t sample = 0; sample < lane.size(); ++sample)
        lane[sample] = wavetableRamSampleForPatch(mode, patch, channel, sample);
    return lane;
}

} // namespace chipper
