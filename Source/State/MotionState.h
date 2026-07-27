#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"
#include "Engine/MotionData.h"

#include <array>

namespace chipper::state
{

inline constexpr auto motionStateTag = "CHIPPER_MOTION_STATE";
inline constexpr auto motionPatternStateTag = "PATTERN";
inline constexpr size_t motionStateModeCount = static_cast<size_t>(ChipMode::ym2610b) + 1u;

struct MotionState
{
    std::array<MotionPattern, motionStateModeCount> patterns {};

    MotionPattern* patternForMode(ChipMode mode) noexcept;
    const MotionPattern* patternForMode(ChipMode mode) const noexcept;
};

std::unique_ptr<juce::XmlElement> createMotionStateXml(const MotionState& state);
juce::Result restoreMotionStateXml(const juce::XmlElement& xml, MotionState& state);

} // namespace chipper::state
