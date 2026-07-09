#pragma once

#include "Engine/ChipDescriptors.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace chipper::ui
{

enum class ChipUiFamily
{
    consoleApu,
    psg,
    fm,
    sampler,
    wavetable,
    oneBit
};

enum class ChipBrowserGroup
{
    consoleApu,
    nesExpansion,
    psg,
    fm,
    sampler,
    wavetable,
    oneBit
};

struct ChipUiProfile
{
    ChipMode mode = ChipMode::nes;
    ChipUiFamily family = ChipUiFamily::consoleApu;
    ChipBrowserGroup browserGroup = ChipBrowserGroup::consoleApu;
    std::string_view familyLabel;
    std::string_view browserGroupLabel;
    size_t visibleSourceCount = 0;
    size_t nativeSourceCount = 0;
    int playSourceColumns = 1;
    int performanceStripHeight = 196;
    int maximumModulesHeight = 492;
    bool nesFamily = false;
    bool nesExpansion = false;
    bool fourOperatorFm = false;
    bool opllOperatorEdit = false;
    bool opnb = false;
    bool opnSsg = false;
    bool sampler = false;
    bool wavetable = false;
    bool usesMasterDetailSources = false;
};

ChipUiProfile profileFor(ChipMode mode);
std::string_view labelFor(ChipUiFamily family);
std::string_view labelFor(ChipBrowserGroup group);
std::vector<ChipBrowserGroup> browserGroupOrder();
std::vector<ChipMode> modesInBrowserGroup(ChipBrowserGroup group);

} // namespace chipper::ui
