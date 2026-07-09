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
    const auto huc6280 = mode == ChipMode::huc6280;
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
    profile.performanceStripHeight = sid ? 124
                                         : (nes ? 236
                                                : (spc700 || paula || fourOp ? 124
                                                                             : (wavetable ? 132 : 196)));
    profile.maximumModulesHeight = sid ? 666
                                       : (nes ? 436
                                              : (spc700 ? 548
                                                        : (paula ? 580
                                                                 : (fourOp ? 564
                                                                           : (huc6280 ? 478
                                                                                      : (wavetable ? 416 : 492))))));
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
