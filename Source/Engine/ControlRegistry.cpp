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
        { 90, parameter_ids::ymEnvelopeShape, "YM Envelope / SID Filter Mode" },
        { 91, parameter_ids::snNoiseMode, "Noise Mode / SID Osc Mod" },
        { 92, parameter_ids::dmgWaveLevel, "DMG Wave Level" },
        { 93, parameter_ids::stereoSpread, "Stereo Spread / SID Resonance" },
        { 94, parameter_ids::dmgStereoRoute, "DMG Stereo Routing" }
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
        case ChipParameterRole::source1Level: return parameter_ids::source1Level;
        case ChipParameterRole::source2Level: return parameter_ids::source2Level;
        case ChipParameterRole::source3Level: return parameter_ids::source3Level;
        case ChipParameterRole::source4Level: return parameter_ids::source4Level;
        case ChipParameterRole::stereoSpread: return parameter_ids::stereoSpread;
        case ChipParameterRole::envelopeDecay: return parameter_ids::envelopeDecay;
        case ChipParameterRole::waveShape: return parameter_ids::waveShape;
        case ChipParameterRole::dmgWaveLevel: return parameter_ids::dmgWaveLevel;
        case ChipParameterRole::dmgStereoRoute: return parameter_ids::dmgStereoRoute;
        case ChipParameterRole::ymEnvelopeShape: return parameter_ids::ymEnvelopeShape;
        case ChipParameterRole::snNoiseMode: return parameter_ids::snNoiseMode;
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
