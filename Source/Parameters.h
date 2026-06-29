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
juce::StringArray sidFilterRoutingChoices();
juce::StringArray pulse2DutyChoices();
juce::StringArray dmgWaveLevelChoices();
juce::StringArray dmgStereoRouteChoices();
juce::StringArray ymEnvelopeShapeChoices();
juce::StringArray ymChannelMixChoices();
juce::StringArray snNoiseModeChoices();
juce::StringArray fmOperatorMultiplierChoices();
juce::StringArray fmOperatorAttackRateChoices();
juce::StringArray fmOperatorDecayRateChoices();
juce::StringArray fmOperatorSustainRateChoices();
juce::StringArray fmOperatorReleaseRateChoices();
juce::StringArray nesDmcRateChoices();
juce::StringArray nesDmcPlaybackModeChoices();
juce::StringArray samplePlaybackModeChoices(ChipMode mode);
juce::StringArray midiNoteChoices();

} // namespace chipper::parameters
