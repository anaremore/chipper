#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace chipper
{

enum class ChipMode;

inline constexpr size_t motionStepCount = 8u;
inline constexpr int motionMinimumPitch = -24;
inline constexpr int motionMaximumPitch = 24;
inline constexpr uint8_t motionMaximumLevel = 15u;

enum class MotionGate : uint8_t
{
    hold = 0,
    retrigger,
    cut
};

enum class MotionRate : uint8_t
{
    eighth = 8,
    sixteenth = 16,
    thirtySecond = 32,
    sixtyFourth = 64
};

enum class MotionTemplate : uint8_t
{
    init = 0,
    majorArp,
    minorArp,
    rise,
    fall,
    pulse
};

struct MotionStep
{
    int8_t pitch = 0;
    uint8_t level = motionMaximumLevel;
    MotionGate gate = MotionGate::hold;
    // YM2149 shared noise register: 0 = preset, 1..32 = native period 0..31.
    uint8_t ymNoisePeriod = 0;

    bool operator==(const MotionStep&) const = default;
};

struct MotionPattern
{
    bool enabled = false;
    MotionRate rate = MotionRate::sixteenth;
    uint8_t length = static_cast<uint8_t>(motionStepCount);
    std::array<MotionStep, motionStepCount> steps {};

    bool operator==(const MotionPattern&) const = default;
};

MotionPattern sanitizeMotionPattern(const MotionPattern& pattern) noexcept;
MotionPattern motionPatternTemplate(MotionTemplate type) noexcept;
double motionStepSamples(MotionRate rate, double bpm, double sampleRate) noexcept;
std::string_view motionRateName(MotionRate rate) noexcept;
std::string_view motionGateName(MotionGate gate) noexcept;
std::string_view motionDestinationForMode(ChipMode mode) noexcept;

} // namespace chipper
