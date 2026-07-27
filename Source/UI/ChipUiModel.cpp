#include "ChipUiModel.h"

#include <algorithm>

namespace chipper::ui
{
namespace
{
bool isNesFamily(ChipMode mode)
{
    return mode == ChipMode::nes
        || mode == ChipMode::nesVrc6
        || mode == ChipMode::nesFds
        || mode == ChipMode::nesSunsoft5b
        || mode == ChipMode::nesMmc5
        || mode == ChipMode::nesVrc7;
}

bool isFourOperatorFm(ChipMode mode)
{
    return mode == ChipMode::ym2612
        || mode == ChipMode::ym2151
        || mode == ChipMode::ym2203
        || mode == ChipMode::ym2608
        || mode == ChipMode::ym2610
        || mode == ChipMode::ym2610b;
}

bool isOpllOperatorEdit(ChipMode mode)
{
    return mode == ChipMode::ym2413 || mode == ChipMode::nesVrc7;
}

ChipUiFamily familyFor(ChipMode mode)
{
    switch (mode)
    {
        case ChipMode::ym2149:
        case ChipMode::sn76489:
        case ChipMode::pokey:
        case ChipMode::saa1099:
            return ChipUiFamily::psg;

        case ChipMode::ym2612:
        case ChipMode::opl3:
        case ChipMode::ym2151:
        case ChipMode::ym2413:
        case ChipMode::ym2203:
        case ChipMode::ym2608:
        case ChipMode::ym2610:
        case ChipMode::ym2610b:
            return ChipUiFamily::fm;

        case ChipMode::spc700:
        case ChipMode::paula:
            return ChipUiFamily::sampler;

        case ChipMode::huc6280:
        case ChipMode::namcoWsg:
        case ChipMode::scc:
            return ChipUiFamily::wavetable;

        case ChipMode::pcSpeaker:
        case ChipMode::zxSpectrumBeeper:
            return ChipUiFamily::oneBit;

        case ChipMode::nes:
        case ChipMode::dmg:
        case ChipMode::sid:
        case ChipMode::nesVrc6:
        case ChipMode::nesFds:
        case ChipMode::nesSunsoft5b:
        case ChipMode::nesMmc5:
        case ChipMode::nesVrc7:
            return ChipUiFamily::consoleApu;
    }

    return ChipUiFamily::consoleApu;
}

ChipBrowserGroup browserGroupFor(ChipMode mode)
{
    if (mode == ChipMode::nesVrc6
        || mode == ChipMode::nesFds
        || mode == ChipMode::nesSunsoft5b
        || mode == ChipMode::nesMmc5
        || mode == ChipMode::nesVrc7)
        return ChipBrowserGroup::nesExpansion;

    switch (familyFor(mode))
    {
        case ChipUiFamily::consoleApu: return ChipBrowserGroup::consoleApu;
        case ChipUiFamily::psg: return ChipBrowserGroup::psg;
        case ChipUiFamily::fm: return ChipBrowserGroup::fm;
        case ChipUiFamily::sampler: return ChipBrowserGroup::sampler;
        case ChipUiFamily::wavetable: return ChipBrowserGroup::wavetable;
        case ChipUiFamily::oneBit: return ChipBrowserGroup::oneBit;
    }

    return ChipBrowserGroup::consoleApu;
}

int playSourceColumns(size_t count)
{
    if (count <= 4u)
        return static_cast<int>(std::max<size_t>(1u, count));
    if (count <= 6u)
        return 3;
    if (count <= 8u)
        return 4;
    return 5;
}
}

std::string_view labelFor(ChipUiFamily family)
{
    switch (family)
    {
        case ChipUiFamily::consoleApu: return "Console APU";
        case ChipUiFamily::psg: return "PSG";
        case ChipUiFamily::fm: return "FM";
        case ChipUiFamily::sampler: return "Sampler";
        case ChipUiFamily::wavetable: return "Wavetable";
        case ChipUiFamily::oneBit: return "One-bit";
    }

    return "Console APU";
}

std::string_view labelFor(ChipBrowserGroup group)
{
    switch (group)
    {
        case ChipBrowserGroup::consoleApu: return "Console APUs";
        case ChipBrowserGroup::nesExpansion: return "NES expansions";
        case ChipBrowserGroup::psg: return "PSG & arcade";
        case ChipBrowserGroup::fm: return "FM";
        case ChipBrowserGroup::sampler: return "Sample engines";
        case ChipBrowserGroup::wavetable: return "Wavetable";
        case ChipBrowserGroup::oneBit: return "One-bit";
    }

    return "Console APUs";
}

ChipUiProfile profileFor(ChipMode mode)
{
    const auto visibleSources = visibleSourceCountForMode(mode);
    const auto family = familyFor(mode);
    const auto browserGroup = browserGroupFor(mode);
    const auto nes = isNesFamily(mode);
    const auto spc700 = mode == ChipMode::spc700;
    const auto paula = mode == ChipMode::paula;
    const auto sid = mode == ChipMode::sid;
    const auto dmg = mode == ChipMode::dmg;
    const auto sn76489 = mode == ChipMode::sn76489;
    const auto ym2149 = mode == ChipMode::ym2149;
    const auto saa1099 = mode == ChipMode::saa1099;
    const auto pokey = mode == ChipMode::pokey;
    const auto pcSpeaker = mode == ChipMode::pcSpeaker;
    const auto zxSpectrumBeeper = mode == ChipMode::zxSpectrumBeeper;
    const auto oneBitHardwarePath = pcSpeaker || zxSpectrumBeeper;
    const auto huc6280 = mode == ChipMode::huc6280;
    const auto namcoWsg = mode == ChipMode::namcoWsg;
    const auto scc = mode == ChipMode::scc;
    const auto ym2612 = mode == ChipMode::ym2612;
    const auto opl3 = mode == ChipMode::opl3;
    const auto ym2151 = mode == ChipMode::ym2151;
    const auto ym2413 = mode == ChipMode::ym2413;
    const auto ym2203 = mode == ChipMode::ym2203;
    const auto ym2608 = mode == ChipMode::ym2608;
    const auto ym2610 = mode == ChipMode::ym2610;
    const auto ym2610b = mode == ChipMode::ym2610b;
    const auto fourOp = isFourOperatorFm(mode);
    const auto wavetable = family == ChipUiFamily::wavetable;

    ChipUiProfile profile;
    profile.mode = mode;
    profile.family = family;
    profile.browserGroup = browserGroup;
    profile.familyLabel = labelFor(family);
    profile.browserGroupLabel = labelFor(browserGroup);
    profile.visibleSourceCount = visibleSources;
    profile.nativeSourceCount = nativeSourceCountForMode(mode);
    profile.playSourceColumns = playSourceColumns(visibleSources);
    if (paula)
        profile.performanceStripHeight = 184;
    else if (sid || dmg || sn76489 || ym2149 || saa1099 || pokey || oneBitHardwarePath || ym2413)
        profile.performanceStripHeight = 124;
    else if (nes)
        profile.performanceStripHeight = 236;
    else if (ym2612 || opl3 || ym2151 || ym2203 || ym2608 || ym2610 || ym2610b)
        profile.performanceStripHeight = 88;
    else if (spc700 || fourOp)
        profile.performanceStripHeight = 124;
    else if (huc6280 || namcoWsg || scc)
        profile.performanceStripHeight = 132;
    else
        profile.performanceStripHeight = wavetable ? 132 : 196;

    if (sid)
        profile.maximumModulesHeight = 666;
    else if (nes)
        profile.maximumModulesHeight = 436;
    else if (dmg || sn76489 || ym2149 || oneBitHardwarePath)
        profile.maximumModulesHeight = 410;
    else if (spc700)
        profile.maximumModulesHeight = 588;
    else if (paula)
        profile.maximumModulesHeight = 529;
    else if (ym2612 || opl3 || ym2151 || ym2203)
        profile.maximumModulesHeight = 586;
    else if (ym2608 || ym2610 || ym2610b)
        profile.maximumModulesHeight = 626;
    else if (fourOp || ym2413)
        profile.maximumModulesHeight = 564;
    else if (huc6280 || namcoWsg || scc)
        profile.maximumModulesHeight = 562;
    else if (pokey)
        profile.maximumModulesHeight = 550;
    else if (saa1099)
        profile.maximumModulesHeight = 470;
    else
        profile.maximumModulesHeight = wavetable ? 416 : 492;
    profile.nesFamily = nes;
    profile.nesExpansion = nes && mode != ChipMode::nes;
    profile.fourOperatorFm = fourOp;
    profile.opllOperatorEdit = isOpllOperatorEdit(mode);
    profile.opnb = mode == ChipMode::ym2610 || mode == ChipMode::ym2610b;
    profile.opnSsg = mode == ChipMode::ym2203 || mode == ChipMode::ym2608 || profile.opnb;
    profile.sampler = spc700 || paula;
    profile.wavetable = wavetable;
    profile.usesMasterDetailSources = visibleSources >= 7u || profile.sampler || profile.wavetable;
    return profile;
}

std::vector<ChipBrowserGroup> browserGroupOrder()
{
    return {
        ChipBrowserGroup::consoleApu,
        ChipBrowserGroup::nesExpansion,
        ChipBrowserGroup::psg,
        ChipBrowserGroup::fm,
        ChipBrowserGroup::sampler,
        ChipBrowserGroup::wavetable,
        ChipBrowserGroup::oneBit
    };
}

std::vector<ChipMode> modesInBrowserGroup(ChipBrowserGroup group)
{
    std::vector<ChipMode> result;
    for (const auto mode : chipModeOrder())
    {
        if (profileFor(mode).browserGroup == group)
            result.push_back(mode);
    }
    return result;
}

} // namespace chipper::ui
