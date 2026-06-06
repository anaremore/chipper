#include "Presets.h"

#include "Engine/ChipDescriptors.h"

#include <algorithm>
#include <cctype>

namespace chipper
{
namespace
{

std::string presetKey(std::string_view text)
{
    std::string key(text.begin(), text.end());
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    key.erase(std::remove_if(key.begin(), key.end(), [](char c) { return c == '_' || c == '-' || c == '/' || c == ' '; }), key.end());
    return key;
}

} // namespace

const std::vector<PresetInfo>& presetCatalog()
{
    static const std::vector<PresetInfo> presets {
        {
            "nes-hero-pulse",
            "NES Leads",
            "NES Hero Pulse",
            "Forward pulse lead with pulse stack and triangle support.",
            ChipMode::nes,
            AccuracyMode::hybrid,
            MacroKind::lead,
            PlayMode::stack,
            { 0.67f, 0.38f, 0.12f, 0.78f },
            { true, true, true, false },
            0.0f,
            0,
            0,
            0,
            -9.0f,
            1789773.0
        },
        {
            "nes-triangle-bass",
            "NES Bass",
            "NES Triangle Bass",
            "Triangle-weighted bass with restrained pulse support.",
            ChipMode::nes,
            AccuracyMode::hybrid,
            MacroKind::bass,
            PlayMode::stack,
            { 0.50f, 0.16f, 0.05f, 0.12f },
            { true, false, true, false },
            0.0f,
            0,
            0,
            0,
            -8.0f,
            1789773.0
        },
        {
            "nes-noise-snare",
            "NES Drums",
            "NES Noise Snare",
            "Short RP2A03 noise burst with triangle transient.",
            ChipMode::nes,
            AccuracyMode::hybrid,
            MacroKind::drum,
            PlayMode::stack,
            { 0.33f, 0.12f, 0.86f, 0.42f },
            { false, false, true, true },
            0.78f,
            0,
            0,
            0,
            -10.0f,
            1789773.0
        },
        {
            "nes-coin-blip",
            "Arcade UI",
            "NES Coin Blip",
            "Bright short pulse pop for UI and pickup sounds.",
            ChipMode::nes,
            AccuracyMode::hybrid,
            MacroKind::coin,
            PlayMode::stack,
            { 0.16f, 0.90f, 0.06f, 0.82f },
            { true, true, false, false },
            0.45f,
            0,
            0,
            0,
            -11.0f,
            1789773.0
        },
        {
            "nes-boss-damage",
            "Classic Game SFX",
            "NES Boss Damage",
            "Noisy RP2A03 impact with triangle thump and one pulse edge.",
            ChipMode::nes,
            AccuracyMode::hybrid,
            MacroKind::hit,
            PlayMode::stack,
            { 0.45f, 0.25f, 0.76f, 0.64f },
            { true, false, true, true },
            0.60f,
            0,
            0,
            2,
            -12.0f,
            1789773.0
        },
        {
            "nes-power-up-rise",
            "Classic Game SFX",
            "NES Power-Up Rise",
            "Optimistic RP2A03 pulse stack with triangle lift.",
            ChipMode::nes,
            AccuracyMode::hybrid,
            MacroKind::powerUp,
            PlayMode::stack,
            { 0.70f, 0.90f, 0.20f, 0.90f },
            { true, true, true, false },
            0.0f,
            0,
            0,
            0,
            -10.0f,
            1789773.0
        },
        {
            "dmg-wave-bass",
            "DMG Wave",
            "DMG Wave Bass",
            "Wave-channel bass body using a written triangle table.",
            ChipMode::dmg,
            AccuracyMode::hybrid,
            MacroKind::bass,
            PlayMode::stack,
            { 0.50f, 0.18f, 0.08f, 0.92f },
            { false, false, true, false },
            0.0f,
            1,
            0,
            0,
            -8.0f,
            4194304.0
        },
        {
            "dmg-pocket-arp",
            "DMG Pulse",
            "DMG Pocket Arp",
            "Pulse and wave stack arranged for fast handheld arps.",
            ChipMode::dmg,
            AccuracyMode::hybrid,
            MacroKind::arp,
            PlayMode::chipPoly,
            { 0.66f, 0.78f, 0.18f, 0.72f },
            { true, true, true, false },
            0.0f,
            4,
            0,
            0,
            -10.0f,
            4194304.0
        },
        {
            "dmg-noise-hat",
            "DMG Drums",
            "DMG Noise Hat",
            "Narrow polynomial-noise percussion with short hardware decay.",
            ChipMode::dmg,
            AccuracyMode::hybrid,
            MacroKind::drum,
            PlayMode::stack,
            { 0.20f, 0.12f, 0.96f, 0.74f },
            { false, false, false, true },
            0.88f,
            0,
            0,
            0,
            -12.0f,
            4194304.0
        },
        {
            "dmg-power-up-rise",
            "Classic Game SFX",
            "DMG Power-Up Rise",
            "Handheld pulse and wave stack for rising pickup gestures.",
            ChipMode::dmg,
            AccuracyMode::hybrid,
            MacroKind::powerUp,
            PlayMode::stack,
            { 0.70f, 0.90f, 0.18f, 0.86f },
            { true, true, true, false },
            0.0f,
            2,
            0,
            0,
            -10.0f,
            4194304.0
        },
        {
            "ym-three-voice-arp",
            "YM Arps",
            "YM Three-Voice Arp",
            "A/B/C fake-chord spread for demo-scene arpeggios.",
            ChipMode::ym2149,
            AccuracyMode::hybrid,
            MacroKind::arp,
            PlayMode::chipPoly,
            { 0.70f, 0.70f, 0.12f, 0.52f },
            { true, true, true, false },
            0.0f,
            0,
            0,
            0,
            -10.0f,
            2000000.0
        },
        {
            "ym-fake-chord-stack",
            "YM Arps",
            "YM Fake Chord Stack",
            "Three YM/AY tone channels voiced as a bright fake chord.",
            ChipMode::ym2149,
            AccuracyMode::hybrid,
            MacroKind::arp,
            PlayMode::stack,
            { 0.95f, 0.76f, 0.12f, 0.50f },
            { true, true, true, false },
            0.0f,
            0,
            0,
            0,
            -10.0f,
            2000000.0
        },
        {
            "ym-menu-beep",
            "YM Beeps",
            "YM Menu Beep",
            "Single-channel square beep with bright pitch gesture.",
            ChipMode::ym2149,
            AccuracyMode::hybrid,
            MacroKind::coin,
            PlayMode::stack,
            { 0.15f, 0.82f, 0.08f, 0.50f },
            { true, false, false, false },
            0.0f,
            0,
            0,
            0,
            -11.0f,
            2000000.0
        },
        {
            "ym-noise-hat",
            "YM Noise Percussion",
            "YM Noise Hat",
            "Shared-noise percussion through channel A.",
            ChipMode::ym2149,
            AccuracyMode::hybrid,
            MacroKind::drum,
            PlayMode::stack,
            { 0.25f, 0.10f, 0.92f, 0.18f },
            { true, false, false, true },
            0.0f,
            0,
            1,
            0,
            -12.0f,
            2000000.0
        },
        {
            "sn-psg-lead",
            "SN76489 / Sega PSG",
            "Sega PSG Lead",
            "Forward tone-channel lead across the SN76489 tone lanes.",
            ChipMode::sn76489,
            AccuracyMode::hybrid,
            MacroKind::lead,
            PlayMode::stack,
            { 0.52f, 0.45f, 0.12f, 0.22f },
            { true, true, true, false },
            0.0f,
            0,
            0,
            0,
            -9.0f,
            3579545.0
        },
        {
            "sn-psg-coin",
            "Arcade UI",
            "Sega PSG Coin",
            "Bright tone-only arcade pop.",
            ChipMode::sn76489,
            AccuracyMode::hybrid,
            MacroKind::coin,
            PlayMode::stack,
            { 0.15f, 0.92f, 0.05f, 0.10f },
            { true, false, false, false },
            0.0f,
            0,
            0,
            0,
            -11.0f,
            3579545.0
        },
        {
            "sn-arcade-laser",
            "Arcade UI",
            "Sega PSG Arcade Laser",
            "SN76489 tone sweep with white-noise bite.",
            ChipMode::sn76489,
            AccuracyMode::hybrid,
            MacroKind::laser,
            PlayMode::stack,
            { 0.20f, 1.00f, 0.70f, 0.95f },
            { true, true, false, true },
            0.0f,
            0,
            0,
            4,
            -11.0f,
            3579545.0
        },
        {
            "sn-noise-hit",
            "SN76489 / Sega PSG",
            "PSG Noise Hit",
            "SN76489 white-noise impact with tone lanes muted.",
            ChipMode::sn76489,
            AccuracyMode::hybrid,
            MacroKind::drum,
            PlayMode::stack,
            { 0.20f, 0.15f, 0.82f, 1.0f },
            { false, false, false, true },
            0.0f,
            0,
            0,
            3,
            -12.0f,
            3579545.0
        }
    };

    return presets;
}

std::vector<const PresetInfo*> presetsForChip(ChipMode chip)
{
    std::vector<const PresetInfo*> matches;
    for (const auto& preset : presetCatalog())
    {
        if (preset.chip == chip)
            matches.push_back(&preset);
    }

    return matches;
}

const PresetInfo* presetById(std::string_view idOrName)
{
    const auto key = presetKey(idOrName);
    const auto& presets = presetCatalog();
    const auto iter = std::find_if(presets.begin(), presets.end(), [&key](const auto& preset)
    {
        return presetKey(preset.id) == key || presetKey(preset.name) == key;
    });

    return iter != presets.end() ? &*iter : nullptr;
}

PatchConfig patchConfigForPreset(const PresetInfo& preset)
{
    return makePatchConfig(preset.chip,
                           preset.macro,
                           preset.controls[0],
                           preset.controls[1],
                           preset.controls[2],
                           preset.controls[3],
                           preset.playMode,
                           preset.sourceEnabled,
                           { 1.0f, 1.0f, 1.0f, 1.0f },
                           preset.stereoSpread,
                           preset.envelopeDecay,
                           preset.waveShape,
                           0,
                           preset.ymEnvelopeShape,
                           preset.snNoiseMode);
}

} // namespace chipper
