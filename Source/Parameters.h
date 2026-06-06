#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/ControlRegistry.h"
#include "Engine/ChipCore.h"

#include <array>
#include <string_view>

namespace chipper::parameters
{

namespace id = ::chipper::parameter_ids;

using MidiCcMapping = ::chipper::MidiCcMapping;
using MidiCcMappingList = ::chipper::MidiCcMappingList;
inline constexpr auto midiCcMappingCount = ::chipper::midiCcMappingCount;

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
const MidiCcMappingList& midiCcMappings();
const char* parameterIdForMidiController(int controllerNumber);
int midiControllerForParameterId(std::string_view parameterId);
const char* parameterIdForChipParameterRole(ChipParameterRole role);
int midiControllerForChipParameterRole(ChipParameterRole role);
ChipMode chipModeFromChoice(int choice);
AccuracyMode accuracyFromChoice(int choice);
MacroKind macroFromChoice(int choice);
PlayMode playModeFromChoice(int choice);
double defaultClockForMode(ChipMode mode);
juce::StringArray chipModeChoices();
juce::StringArray accuracyChoices();
juce::StringArray macroChoices();
juce::StringArray playModeChoices();
juce::StringArray waveShapeChoices();
juce::StringArray sidVoiceWaveShapeChoices();
juce::StringArray sidAdsrNibbleChoices();
juce::StringArray dmgWaveLevelChoices();
juce::StringArray dmgStereoRouteChoices();
juce::StringArray ymEnvelopeShapeChoices();
juce::StringArray snNoiseModeChoices();

} // namespace chipper::parameters
