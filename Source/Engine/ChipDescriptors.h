#pragma once

#include "Engine/ChipCore.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace chipper
{

struct ControlDescriptor
{
    std::string id;
    std::string label;
    std::string group;
    std::string help;
};

enum class ParameterKind
{
    continuous,
    discreteChoice,
    steppedNumeric,
    booleanToggle,
    macro,
    chipRegister,
    hiddenInternal
};

enum class ControlSurface
{
    slider,
    segmentedChoice,
    sourceCards,
    toggle,
    menu,
    hidden
};

enum class ChipParameterRole
{
    macroControl1,
    macroControl2,
    macroControl3,
    macroControl4,
    source1Enabled,
    source2Enabled,
    source3Enabled,
    source4Enabled,
    envelopeDecay,
    waveShape,
    ymEnvelopeShape,
    snNoiseMode,
    clockHz,
    outputDb
};

struct ParameterChoiceSpec
{
    std::string label;
    std::string help;
    float normalizedValue = 0.0f;
    int choiceValue = 0;
};

struct ChipParameterSpec
{
    ChipParameterRole role = ChipParameterRole::macroControl1;
    std::string id;
    std::string label;
    std::string group;
    std::string help;
    ParameterKind kind = ParameterKind::continuous;
    ControlSurface surface = ControlSurface::slider;
    std::vector<ParameterChoiceSpec> choices;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    bool automatable = true;
    bool authenticOnly = false;
    bool hybridOnly = false;
};

struct ModuleDescriptor
{
    std::string id;
    std::string title;
    std::string summary;
    std::vector<std::string> items;
};

struct MacroTemplate
{
    MacroKind macro = MacroKind::manual;
    std::string label;
    std::string help;
    std::array<float, 4> controls { 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<bool, 4> sourceEnabled { true, true, true, true };
    float envelopeDecay = 0.0f;
    int waveShape = 0;
    int ymEnvelopeShape = 0;
    int snNoiseMode = 0;
};

struct ChipDescriptor
{
    ChipMode mode = ChipMode::nes;
    std::string displayName;
    std::string summary;
    std::vector<ControlDescriptor> controls;
    std::array<ModuleDescriptor, 6> modules;
    std::vector<MacroTemplate> macros;
    bool implemented = false;
    bool supportsChipPoly = false;
    std::vector<ChipParameterSpec> parameters;
};

const ChipDescriptor& descriptorFor(ChipMode mode);
const MacroTemplate& macroTemplateFor(ChipMode mode, MacroKind macro);
const ChipParameterSpec* parameterSpecFor(ChipMode mode, ChipParameterRole role);
bool chipHasParameterSurface(ChipMode mode, ChipParameterRole role, ControlSurface surface);
bool supportsPlayMode(ChipMode mode, PlayMode playMode);
std::vector<ChipMode> chipModeOrder();
std::vector<MacroKind> macroOrder();
PatchConfig makePatchConfig(ChipMode mode,
                            MacroKind macro,
                            float control1,
                            float control2,
                            float control3,
                            float control4,
                            PlayMode playMode = PlayMode::stack,
                            std::array<bool, 4> sourceEnabled = { true, true, true, true },
                            float envelopeDecay = 0.0f,
                            int waveShape = 0,
                            int ymEnvelopeShape = 0,
                            int snNoiseMode = 0);
uint8_t nesNoiseRegisterForPatch(const PatchConfig& patch);
uint8_t dmgNoiseRegisterForPatch(const PatchConfig& patch);
uint8_t ym2149MixerRegisterForControl(float toneNoiseControl);
uint16_t ym2149EnvelopePeriodForControl(float envelopeControl);
uint8_t sn76489NoiseAttenuationForControl(float noiseLevelControl);
uint8_t sn76489NoiseControlForPatch(const PatchConfig& patch);

} // namespace chipper
