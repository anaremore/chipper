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
        { 84, parameter_ids::envelopeDecay, "Envelope Decay" },
        { 85, parameter_ids::waveShape, "Wave Shape" },
        { 86, parameter_ids::ymEnvelopeShape, "YM Envelope Shape" },
        { 87, parameter_ids::snNoiseMode, "Noise Mode" }
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
        case ChipParameterRole::envelopeDecay: return parameter_ids::envelopeDecay;
        case ChipParameterRole::waveShape: return parameter_ids::waveShape;
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
