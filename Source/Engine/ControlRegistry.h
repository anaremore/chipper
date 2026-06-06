#pragma once

#include "Engine/ChipDescriptors.h"

#include <array>
#include <string_view>

namespace chipper
{

namespace parameter_ids
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
inline constexpr auto source1Level = "source1Level";
inline constexpr auto source2Level = "source2Level";
inline constexpr auto source3Level = "source3Level";
inline constexpr auto source4Level = "source4Level";
inline constexpr auto envelopeDecay = "envelopeDecay";
inline constexpr auto waveShape = "waveShape";
inline constexpr auto dmgWaveLevel = "dmgWaveLevel";
inline constexpr auto ymEnvelopeShape = "ymEnvelopeShape";
inline constexpr auto snNoiseMode = "snNoiseMode";
}

struct MidiCcMapping
{
    int controller = 0;
    const char* parameterId = nullptr;
    const char* label = nullptr;
};

inline constexpr auto midiCcMappingCount = 23u;
using MidiCcMappingList = std::array<MidiCcMapping, midiCcMappingCount>;

const MidiCcMappingList& midiCcMappings();
const char* parameterIdForMidiController(int controllerNumber);
int midiControllerForParameterId(std::string_view parameterId);
const char* parameterIdForChipParameterRole(ChipParameterRole role);
int midiControllerForChipParameterRole(ChipParameterRole role);

} // namespace chipper
