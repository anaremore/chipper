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
inline constexpr auto playMode = "playMode";
inline constexpr auto macroControl1 = "macroControl1";
inline constexpr auto macroControl2 = "macroControl2";
inline constexpr auto macroControl3 = "macroControl3";
inline constexpr auto macroControl4 = "macroControl4";
inline constexpr auto source1Enabled = "source1Enabled";
inline constexpr auto source2Enabled = "source2Enabled";
inline constexpr auto source3Enabled = "source3Enabled";
inline constexpr auto source4Enabled = "source4Enabled";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
ChipMode chipModeFromChoice(int choice);
AccuracyMode accuracyFromChoice(int choice);
MacroKind macroFromChoice(int choice);
PlayMode playModeFromChoice(int choice);
double defaultClockForMode(ChipMode mode);
juce::StringArray chipModeChoices();
juce::StringArray accuracyChoices();
juce::StringArray macroChoices();
juce::StringArray playModeChoices();

} // namespace chipper::parameters
