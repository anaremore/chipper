#include "Parameters.h"

namespace chipper::parameters
{

juce::StringArray chipModeChoices()
{
    return {
        "NES / RP2A03",
        "Game Boy / DMG",
        "SID / C64",
        "YM2149 / AY",
        "SN76489 / Sega PSG",
        "YM2612 / Genesis FM",
        "OPL2/OPL3 / DOS FM",
        "SNES SPC700-style",
        "Atari POKEY",
        "Amiga Paula",
        "PC Engine HuC6280",
        "Namco arcade WSG",
        "YM2151 arcade FM",
        "YM2413 / OPLL",
        "Konami SCC",
        "Arcade Hybrid",
        "Custom"
    };
}

juce::StringArray accuracyChoices()
{
    return { "Inspired", "Hybrid", "Authentic" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::chipMode, 1 },
        "Chip Mode",
        chipModeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::accuracy, 1 },
        "Accuracy",
        accuracyChoices(),
        1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::clockHz, 1 },
        "Clock Rate Override",
        juce::NormalisableRange<float> { 0.0f, 8000000.0f, 1.0f, 0.35f },
        0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id::outputDb, 1 },
        "Output Level",
        juce::NormalisableRange<float> { -48.0f, 6.0f, 0.01f },
        -9.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id::macro, 1 },
        "Musical Macro",
        juce::StringArray { "Manual", "Coin", "Bass", "Lead", "Arp", "Drum", "Hit", "Laser", "Jump", "Power-Up" },
        0));

    return { params.begin(), params.end() };
}

ChipMode chipModeFromChoice(int choice)
{
    switch (choice)
    {
        case 0: return ChipMode::nes;
        case 1: return ChipMode::dmg;
        case 2: return ChipMode::sid;
        case 3: return ChipMode::ym2149;
        case 4: return ChipMode::sn76489;
        case 5: return ChipMode::ym2612;
        case 6: return ChipMode::opl3;
        case 7: return ChipMode::spc700;
        case 8: return ChipMode::pokey;
        case 9: return ChipMode::paula;
        case 10: return ChipMode::huc6280;
        case 11: return ChipMode::namcoWsg;
        case 12: return ChipMode::ym2151;
        case 13: return ChipMode::ym2413;
        case 14: return ChipMode::scc;
        case 15: return ChipMode::arcade;
        case 16: return ChipMode::custom;
        default: return ChipMode::nes;
    }
}

AccuracyMode accuracyFromChoice(int choice)
{
    switch (choice)
    {
        case 0: return AccuracyMode::inspired;
        case 2: return AccuracyMode::authentic;
        case 1:
        default: return AccuracyMode::hybrid;
    }
}

double defaultClockForMode(ChipMode mode)
{
    switch (mode)
    {
        case ChipMode::nes: return 1789773.0;
        case ChipMode::dmg: return 4194304.0;
        case ChipMode::sid: return 985248.0;
        case ChipMode::ym2149: return 2000000.0;
        case ChipMode::sn76489: return 3579545.0;
        case ChipMode::ym2612: return 7670454.0;
        case ChipMode::opl3: return 14318180.0;
        case ChipMode::spc700: return 32000.0;
        case ChipMode::pokey: return 1789790.0;
        case ChipMode::paula: return 3546895.0;
        case ChipMode::huc6280: return 3579545.0;
        case ChipMode::namcoWsg: return 96000.0;
        case ChipMode::ym2151: return 3579545.0;
        case ChipMode::ym2413: return 3579545.0;
        case ChipMode::scc: return 3579545.0;
        case ChipMode::arcade: return 3579545.0;
        case ChipMode::custom: return 3579545.0;
    }

    return 3579545.0;
}

} // namespace chipper::parameters
