#pragma once

#include "Engine/ChipCore.h"

#include <array>
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
};

const ChipDescriptor& descriptorFor(ChipMode mode);
const MacroTemplate& macroTemplateFor(ChipMode mode, MacroKind macro);
bool supportsPlayMode(ChipMode mode, PlayMode playMode);
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
                            int ymEnvelopeShape = 0);

} // namespace chipper
