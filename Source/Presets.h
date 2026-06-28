#pragma once

#include "Engine/ChipCore.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace chipper
{

struct PresetInfo
{
    std::string id;
    std::string category;
    std::string name;
    std::string note;
    ChipMode chip = ChipMode::nes;
    AccuracyMode accuracy = AccuracyMode::hybrid;
    MacroKind macro = MacroKind::manual;
    PlayMode playMode = PlayMode::stack;
    std::array<float, 4> controls { 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<bool, 4> sourceEnabled { true, true, true, true };
    float envelopeDecay = 0.0f;
    int waveShape = 0;
    int ymEnvelopeShape = 0;
    int snNoiseMode = 0;
    float outputDb = -9.0f;
    double clockHz = 0.0;
    float stereoSpread = 0.0f;
    int dmgStereoRoute = 0;
    int sidVoice2WaveShape = 0;
    int sidVoice3WaveShape = 0;
    int sidFilterRouting = 0;
    int sidAttack = 0;
    int sidDecay = 0;
    int sidSustain = 0;
    int sidRelease = 0;
    int sidVoice2Attack = 0;
    int sidVoice2Decay = 0;
    int sidVoice2Sustain = 0;
    int sidVoice2Release = 0;
    int sidVoice3Attack = 0;
    int sidVoice3Decay = 0;
    int sidVoice3Sustain = 0;
    int sidVoice3Release = 0;
    float source1Level = 1.0f;
    float source2Level = 1.0f;
    float source3Level = 1.0f;
    float source4Level = 1.0f;
    int dmgWaveLevel = 0;
    float nesDmcDirectLevel = 0.0f;
};

struct PresetQualityTarget
{
    ChipMode chip = ChipMode::nes;
    std::vector<std::string> requiredRoles;
    std::vector<std::string> referenceTags;
    std::string note;
    int minimumPresetCount = 10;
};

const std::vector<PresetInfo>& presetCatalog();
std::vector<const PresetInfo*> presetsForChip(ChipMode chip);
std::vector<const PresetInfo*> presetBrowserCatalog(ChipMode preferredChip);
std::string presetRoleFor(const PresetInfo& preset);
std::string presetEngineFor(const PresetInfo& preset);
std::vector<std::string> presetTagsFor(const PresetInfo& preset);
const std::vector<std::string>& presetCoreRoleTargets();
const std::vector<PresetQualityTarget>& presetQualityTargets();
const PresetQualityTarget* presetQualityTargetForChip(ChipMode chip);
PresetInfo initPresetForChip(ChipMode chip);
const PresetInfo* presetById(std::string_view idOrName);
PatchConfig patchConfigForPreset(const PresetInfo& preset);

} // namespace chipper
