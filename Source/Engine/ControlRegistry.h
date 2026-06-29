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
inline constexpr auto source5Enabled = "source5Enabled";
inline constexpr auto source6Enabled = "source6Enabled";
inline constexpr auto source7Enabled = "source7Enabled";
inline constexpr auto source8Enabled = "source8Enabled";
inline constexpr auto source9Enabled = "source9Enabled";
inline constexpr auto source1Level = "source1Level";
inline constexpr auto source2Level = "source2Level";
inline constexpr auto source3Level = "source3Level";
inline constexpr auto source4Level = "source4Level";
inline constexpr auto source5Level = "source5Level";
inline constexpr auto source6Level = "source6Level";
inline constexpr auto source7Level = "source7Level";
inline constexpr auto source8Level = "source8Level";
inline constexpr auto source9Level = "source9Level";
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
inline constexpr auto nesDmcMapRoot = "nesDmcMapRoot";
inline constexpr auto nesDmcLoop = "nesDmcLoop";
inline constexpr auto spc700LoopStart = "spc700LoopStart";
inline constexpr auto spc700LoopEnd = "spc700LoopEnd";
inline constexpr auto spc700Voice1SampleSlot = "spc700Voice1SampleSlot";
inline constexpr auto spc700Voice2SampleSlot = "spc700Voice2SampleSlot";
inline constexpr auto spc700Voice3SampleSlot = "spc700Voice3SampleSlot";
inline constexpr auto spc700Voice4SampleSlot = "spc700Voice4SampleSlot";
inline constexpr auto spc700Voice5SampleSlot = "spc700Voice5SampleSlot";
inline constexpr auto spc700Voice6SampleSlot = "spc700Voice6SampleSlot";
inline constexpr auto spc700Voice7SampleSlot = "spc700Voice7SampleSlot";
inline constexpr auto spc700Voice8SampleSlot = "spc700Voice8SampleSlot";
inline constexpr auto fmOperator1Level = "fmOperator1Level";
inline constexpr auto fmOperator2Level = "fmOperator2Level";
inline constexpr auto fmOperator3Level = "fmOperator3Level";
inline constexpr auto fmOperator4Level = "fmOperator4Level";
inline constexpr auto fmOperator1Multiplier = "fmOperator1Multiplier";
inline constexpr auto fmOperator2Multiplier = "fmOperator2Multiplier";
inline constexpr auto fmOperator3Multiplier = "fmOperator3Multiplier";
inline constexpr auto fmOperator4Multiplier = "fmOperator4Multiplier";
}

struct MidiCcMapping
{
    int controller = 0;
    const char* parameterId = nullptr;
    const char* label = nullptr;
};

inline constexpr auto midiCcMappingCount = 80u;
using MidiCcMappingList = std::array<MidiCcMapping, midiCcMappingCount>;

const MidiCcMappingList& midiCcMappings();
const char* parameterIdForMidiController(int controllerNumber);
int midiControllerForParameterId(std::string_view parameterId);
const char* parameterIdForChipParameterRole(ChipParameterRole role);
int midiControllerForChipParameterRole(ChipParameterRole role);

} // namespace chipper
