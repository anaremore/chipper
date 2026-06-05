#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"

namespace chipper::parameters
{

namespace id
{
inline constexpr auto chipMode = "chipMode";
inline constexpr auto accuracy = "accuracy";
inline constexpr auto clockHz = "clockHz";
inline constexpr auto outputDb = "outputDb";
inline constexpr auto macro = "macro";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
ChipMode chipModeFromChoice(int choice);
AccuracyMode accuracyFromChoice(int choice);
double defaultClockForMode(ChipMode mode);
juce::StringArray chipModeChoices();
juce::StringArray accuracyChoices();

} // namespace chipper::parameters
