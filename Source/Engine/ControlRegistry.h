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
inline constexpr auto stereoSpread = "stereoSpread";
inline constexpr auto sidFilterRouting = "sidFilterRouting";
inline constexpr auto sidVoice2PulseWidth = "sidVoice2PulseWidth";
inline constexpr auto sidVoice3PulseWidth = "sidVoice3PulseWidth";
inline constexpr auto envelopeDecay = "envelopeDecay";
inline constexpr auto sidAttack = "sidAttack";
inline constexpr auto sidDecay = "sidDecay";
inline constexpr auto sidSustain = "sidSustain";
inline constexpr auto sidRelease = "sidRelease";
inline constexpr auto sidVoice2Attack = "sidVoice2Attack";
inline constexpr auto sidVoice2Decay = "sidVoice2Decay";
inline constexpr auto sidVoice2Sustain = "sidVoice2Sustain";
inline constexpr auto sidVoice2Release = "sidVoice2Release";
inline constexpr auto sidVoice3Attack = "sidVoice3Attack";
inline constexpr auto sidVoice3Decay = "sidVoice3Decay";
inline constexpr auto sidVoice3Sustain = "sidVoice3Sustain";
inline constexpr auto sidVoice3Release = "sidVoice3Release";
inline constexpr auto waveShape = "waveShape";
inline constexpr auto sidVoice2WaveShape = "sidVoice2WaveShape";
inline constexpr auto sidVoice3WaveShape = "sidVoice3WaveShape";
inline constexpr auto pulse2Duty = "pulse2Duty";
inline constexpr auto dmgWaveLevel = "dmgWaveLevel";
inline constexpr auto dmgStereoRoute = "dmgStereoRoute";
inline constexpr auto ymEnvelopeShape = "ymEnvelopeShape";
inline constexpr auto ymChannelAMix = "ymChannelAMix";
inline constexpr auto ymChannelBMix = "ymChannelBMix";
inline constexpr auto ymChannelCMix = "ymChannelCMix";
inline constexpr auto snNoiseMode = "snNoiseMode";
inline constexpr auto nesDmcDirectLevel = "nesDmcDirectLevel";
inline constexpr auto nesDmcSampleSlot = "nesDmcSampleSlot";
inline constexpr auto nesDmcRateIndex = "nesDmcRateIndex";
inline constexpr auto nesDmcPlaybackMode = "nesDmcPlaybackMode";
}

struct MidiCcMapping
{
    int controller = 0;
    const char* parameterId = nullptr;
    const char* label = nullptr;
};

inline constexpr auto midiCcMappingCount = 50u;
using MidiCcMappingList = std::array<MidiCcMapping, midiCcMappingCount>;

const MidiCcMappingList& midiCcMappings();
const char* parameterIdForMidiController(int controllerNumber);
int midiControllerForParameterId(std::string_view parameterId);
const char* parameterIdForChipParameterRole(ChipParameterRole role);
int midiControllerForChipParameterRole(ChipParameterRole role);

} // namespace chipper
