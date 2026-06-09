#include "Engine/ControlRegistry.h"

namespace chipper
{

const MidiCcMappingList& midiCcMappings()
{
    static constexpr MidiCcMappingList mappings {{
        { 70, parameter_ids::chipMode, "Chip Mode" },
        { 71, parameter_ids::accuracy, "Accuracy" },
        { 72, parameter_ids::clockHz, "Clock Rate Override" },
        { 73, parameter_ids::outputDb, "Output Level" },
        { 74, parameter_ids::macro, "Musical Macro" },
        { 75, parameter_ids::playMode, "Play Mode" },
        { 76, parameter_ids::macroControl1, "Native Control 1" },
        { 77, parameter_ids::macroControl2, "Native Control 2" },
        { 78, parameter_ids::macroControl3, "Native Control 3" },
        { 79, parameter_ids::macroControl4, "Native Control 4" },
        { 80, parameter_ids::source1Enabled, "Source 1 Enabled" },
        { 81, parameter_ids::source2Enabled, "Source 2 Enabled" },
        { 82, parameter_ids::source3Enabled, "Source 3 Enabled" },
        { 83, parameter_ids::source4Enabled, "Source 4 Enabled" },
        { 84, parameter_ids::source1Level, "Source 1 Level" },
        { 85, parameter_ids::source2Level, "Source 2 Level" },
        { 86, parameter_ids::source3Level, "Source 3 Level" },
        { 87, parameter_ids::source4Level, "Source 4 Level" },
        { 88, parameter_ids::envelopeDecay, "Envelope Decay" },
        { 89, parameter_ids::waveShape, "Wave Shape" },
        { 90, parameter_ids::ymEnvelopeShape, "YM Envelope / SID Filter Mode / SPC700 Envelope / OPLL Rhythm" },
        { 91, parameter_ids::snNoiseMode, "Noise Mode / SID Osc Interaction / OPN2 DAC Mode" },
        { 92, parameter_ids::dmgWaveLevel, "DMG Wave Level" },
        { 93, parameter_ids::stereoSpread, "Stereo Spread / SID Resonance" },
        { 94, parameter_ids::dmgStereoRoute, "DMG Stereo Routing / SID Model" },
        { 95, parameter_ids::sidVoice2WaveShape, "SID Voice 2 Waveform" },
        { 96, parameter_ids::sidVoice3WaveShape, "SID Voice 3 Waveform" },
        { 97, parameter_ids::sidAttack, "SID Voice 1 Attack" },
        { 98, parameter_ids::sidRelease, "SID Voice 1 Release" },
        { 99, parameter_ids::sidDecay, "SID Voice 1 Decay" },
        { 100, parameter_ids::sidSustain, "SID Voice 1 Sustain" },
        { 101, parameter_ids::ymChannelAMix, "YM Channel A Mix" },
        { 102, parameter_ids::ymChannelBMix, "YM Channel B Mix" },
        { 103, parameter_ids::ymChannelCMix, "YM Channel C Mix" },
        { 104, parameter_ids::pulse2Duty, "Pulse 2 Duty" },
        { 105, parameter_ids::sidVoice2Attack, "SID Voice 2 Attack" },
        { 106, parameter_ids::sidVoice2Decay, "SID Voice 2 Decay" },
        { 107, parameter_ids::sidVoice2Sustain, "SID Voice 2 Sustain" },
        { 108, parameter_ids::sidVoice2Release, "SID Voice 2 Release" },
        { 109, parameter_ids::sidVoice3Attack, "SID Voice 3 Attack" },
        { 110, parameter_ids::sidVoice3Decay, "SID Voice 3 Decay" },
        { 111, parameter_ids::sidVoice3Sustain, "SID Voice 3 Sustain" },
        { 112, parameter_ids::sidVoice3Release, "SID Voice 3 Release" },
        { 113, parameter_ids::sidFilterRouting, "SID Filter Routing" },
        { 114, parameter_ids::sidVoice2PulseWidth, "SID Voice 2 Pulse Width" },
        { 115, parameter_ids::sidVoice3PulseWidth, "SID Voice 3 Pulse Width" },
        { 116, parameter_ids::nesDmcDirectLevel, "NES DMC Direct Level" },
        { 117, parameter_ids::nesDmcSampleSlot, "NES DMC Sample Slot" },
        { 118, parameter_ids::nesDmcRateIndex, "NES DMC Rate Index" },
        { 119, parameter_ids::nesDmcPlaybackMode, "NES DMC Playback Mode" },
        { 64, parameter_ids::source5Enabled, "Source 5 Enabled" },
        { 65, parameter_ids::source6Enabled, "Source 6 Enabled" },
        { 66, parameter_ids::source5Level, "Source 5 Level" },
        { 67, parameter_ids::source6Level, "Source 6 Level" },
        { 60, parameter_ids::source7Enabled, "Source 7 Enabled" },
        { 61, parameter_ids::source8Enabled, "Source 8 Enabled" },
        { 62, parameter_ids::source7Level, "Source 7 Level" },
        { 63, parameter_ids::source8Level, "Source 8 Level" },
        { 58, parameter_ids::source9Enabled, "Source 9 Enabled" },
        { 59, parameter_ids::source9Level, "Source 9 Level" },
        { 69, parameter_ids::nesDmcMapRoot, "NES DMC Map Root" },
        { 68, parameter_ids::nesDmcLoop, "NES DMC Loop" }
    }};

    return mappings;
}

const char* parameterIdForMidiController(int controllerNumber)
{
    for (const auto& mapping : midiCcMappings())
    {
        if (mapping.controller == controllerNumber)
            return mapping.parameterId;
    }

    return nullptr;
}

int midiControllerForParameterId(std::string_view parameterId)
{
    for (const auto& mapping : midiCcMappings())
    {
        if (mapping.parameterId == parameterId)
            return mapping.controller;
    }

    return -1;
}

const char* parameterIdForChipParameterRole(ChipParameterRole role)
{
    switch (role)
    {
        case ChipParameterRole::macroControl1: return parameter_ids::macroControl1;
        case ChipParameterRole::macroControl2: return parameter_ids::macroControl2;
        case ChipParameterRole::macroControl3: return parameter_ids::macroControl3;
        case ChipParameterRole::macroControl4: return parameter_ids::macroControl4;
        case ChipParameterRole::source1Enabled: return parameter_ids::source1Enabled;
        case ChipParameterRole::source2Enabled: return parameter_ids::source2Enabled;
        case ChipParameterRole::source3Enabled: return parameter_ids::source3Enabled;
        case ChipParameterRole::source4Enabled: return parameter_ids::source4Enabled;
        case ChipParameterRole::source5Enabled: return parameter_ids::source5Enabled;
        case ChipParameterRole::source6Enabled: return parameter_ids::source6Enabled;
        case ChipParameterRole::source7Enabled: return parameter_ids::source7Enabled;
        case ChipParameterRole::source8Enabled: return parameter_ids::source8Enabled;
        case ChipParameterRole::source9Enabled: return parameter_ids::source9Enabled;
        case ChipParameterRole::source1Level: return parameter_ids::source1Level;
        case ChipParameterRole::source2Level: return parameter_ids::source2Level;
        case ChipParameterRole::source3Level: return parameter_ids::source3Level;
        case ChipParameterRole::source4Level: return parameter_ids::source4Level;
        case ChipParameterRole::source5Level: return parameter_ids::source5Level;
        case ChipParameterRole::source6Level: return parameter_ids::source6Level;
        case ChipParameterRole::source7Level: return parameter_ids::source7Level;
        case ChipParameterRole::source8Level: return parameter_ids::source8Level;
        case ChipParameterRole::source9Level: return parameter_ids::source9Level;
        case ChipParameterRole::stereoSpread: return parameter_ids::stereoSpread;
        case ChipParameterRole::sidFilterRouting: return parameter_ids::sidFilterRouting;
        case ChipParameterRole::sidVoice2PulseWidth: return parameter_ids::sidVoice2PulseWidth;
        case ChipParameterRole::sidVoice3PulseWidth: return parameter_ids::sidVoice3PulseWidth;
        case ChipParameterRole::envelopeDecay: return parameter_ids::envelopeDecay;
        case ChipParameterRole::sidAttack: return parameter_ids::sidAttack;
        case ChipParameterRole::sidDecay: return parameter_ids::sidDecay;
        case ChipParameterRole::sidSustain: return parameter_ids::sidSustain;
        case ChipParameterRole::sidRelease: return parameter_ids::sidRelease;
        case ChipParameterRole::sidVoice2Attack: return parameter_ids::sidVoice2Attack;
        case ChipParameterRole::sidVoice2Decay: return parameter_ids::sidVoice2Decay;
        case ChipParameterRole::sidVoice2Sustain: return parameter_ids::sidVoice2Sustain;
        case ChipParameterRole::sidVoice2Release: return parameter_ids::sidVoice2Release;
        case ChipParameterRole::sidVoice3Attack: return parameter_ids::sidVoice3Attack;
        case ChipParameterRole::sidVoice3Decay: return parameter_ids::sidVoice3Decay;
        case ChipParameterRole::sidVoice3Sustain: return parameter_ids::sidVoice3Sustain;
        case ChipParameterRole::sidVoice3Release: return parameter_ids::sidVoice3Release;
        case ChipParameterRole::waveShape: return parameter_ids::waveShape;
        case ChipParameterRole::sidVoice2WaveShape: return parameter_ids::sidVoice2WaveShape;
        case ChipParameterRole::sidVoice3WaveShape: return parameter_ids::sidVoice3WaveShape;
        case ChipParameterRole::pulse2Duty: return parameter_ids::pulse2Duty;
        case ChipParameterRole::dmgWaveLevel: return parameter_ids::dmgWaveLevel;
        case ChipParameterRole::dmgStereoRoute: return parameter_ids::dmgStereoRoute;
        case ChipParameterRole::ymEnvelopeShape: return parameter_ids::ymEnvelopeShape;
        case ChipParameterRole::ymChannelAMix: return parameter_ids::ymChannelAMix;
        case ChipParameterRole::ymChannelBMix: return parameter_ids::ymChannelBMix;
        case ChipParameterRole::ymChannelCMix: return parameter_ids::ymChannelCMix;
        case ChipParameterRole::snNoiseMode: return parameter_ids::snNoiseMode;
        case ChipParameterRole::nesDmcDirectLevel: return parameter_ids::nesDmcDirectLevel;
        case ChipParameterRole::nesDmcSampleSlot: return parameter_ids::nesDmcSampleSlot;
        case ChipParameterRole::nesDmcRateIndex: return parameter_ids::nesDmcRateIndex;
        case ChipParameterRole::nesDmcPlaybackMode: return parameter_ids::nesDmcPlaybackMode;
        case ChipParameterRole::nesDmcMapRoot: return parameter_ids::nesDmcMapRoot;
        case ChipParameterRole::nesDmcLoop: return parameter_ids::nesDmcLoop;
        case ChipParameterRole::clockHz: return parameter_ids::clockHz;
        case ChipParameterRole::outputDb: return parameter_ids::outputDb;
    }

    return nullptr;
}

int midiControllerForChipParameterRole(ChipParameterRole role)
{
    const auto* parameterId = parameterIdForChipParameterRole(role);
    return parameterId == nullptr ? -1 : midiControllerForParameterId(parameterId);
}

} // namespace chipper
