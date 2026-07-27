#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace chipper
{

enum class ChipMode;
struct PatchConfig;

inline constexpr size_t wavetableSampleCount = 32u;
inline constexpr size_t maximumWavetableLaneCount = 8u;

using WavetableLane = std::array<uint8_t, wavetableSampleCount>;

struct WavetableMemory
{
    std::array<WavetableLane, maximumWavetableLaneCount> lanes {};
    std::array<bool, maximumWavetableLaneCount> customLanes {};

    bool operator==(const WavetableMemory&) const = default;
};

struct WavetableSpec
{
    size_t laneCount = 0u;
    uint8_t bitDepth = 0u;
    uint8_t maximumSampleValue = 0u;
};

bool supportsDirectWavetableEditing(ChipMode mode) noexcept;
WavetableSpec wavetableSpecForMode(ChipMode mode) noexcept;
uint8_t quantizeWavetableSample(ChipMode mode, int sample) noexcept;
WavetableLane quantizeWavetableLane(ChipMode mode, const WavetableLane& lane) noexcept;
uint8_t wavetableWaveShapeForChannel(ChipMode mode, const PatchConfig& patch, size_t channel);
uint8_t generatedWavetableSampleForPatch(ChipMode mode,
                                         const PatchConfig& patch,
                                         size_t channel,
                                         size_t sampleIndex);
uint8_t wavetableRamSampleForPatch(ChipMode mode,
                                   const PatchConfig& patch,
                                   size_t channel,
                                   size_t sampleIndex);
WavetableLane wavetableLaneForPatch(ChipMode mode, const PatchConfig& patch, size_t channel);

} // namespace chipper
