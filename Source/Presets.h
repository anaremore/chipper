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
};

const std::vector<PresetInfo>& presetCatalog();
std::vector<const PresetInfo*> presetsForChip(ChipMode chip);
const PresetInfo* presetById(std::string_view idOrName);
PatchConfig patchConfigForPreset(const PresetInfo& preset);

} // namespace chipper
