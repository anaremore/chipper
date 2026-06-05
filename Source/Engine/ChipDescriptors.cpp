#include "Engine/ChipDescriptors.h"

#include <algorithm>

namespace chipper
{
namespace
{

std::vector<MacroTemplate> commonMacros()
{
    return {
        { MacroKind::manual, "Manual", "Neutral register mapping.", { 0.5f, 0.5f, 0.5f, 0.5f } },
        { MacroKind::coin, "Coin", "Bright short pitch pop.", { 0.15f, 0.85f, 0.10f, 0.80f } },
        { MacroKind::bass, "Bass", "Low focused body.", { 0.55f, 0.20f, 0.10f, 0.35f } },
        { MacroKind::lead, "Lead", "Forward melodic voice.", { 0.70f, 0.50f, 0.20f, 0.55f } },
        { MacroKind::arp, "Arp", "Stacked tones for fast patterns.", { 0.65f, 0.75f, 0.15f, 0.60f } },
        { MacroKind::drum, "Drum", "Noise and transient-weighted setup.", { 0.40f, 0.15f, 0.80f, 0.45f } },
        { MacroKind::hit, "Hit", "Short noisy impact.", { 0.45f, 0.25f, 0.70f, 0.65f } },
        { MacroKind::laser, "Laser", "Pitch drop with bite.", { 0.25f, 1.00f, 0.35f, 0.95f } },
        { MacroKind::jump, "Jump", "Rising blip.", { 0.25f, 0.70f, 0.05f, 0.80f } },
        { MacroKind::powerUp, "Power-Up", "Longer optimistic rise.", { 0.70f, 0.90f, 0.20f, 0.90f } },
    };
}

const ChipDescriptor& fallbackDescriptor()
{
    static const ChipDescriptor descriptor {
        ChipMode::custom,
        "Planned Chip Mode",
        "No audited core is integrated yet. Controls are placeholders until this chip has a register model.",
        {
            { "native1", "Native Control 1", "Planned", "Reserved for the audited chip core." },
            { "native2", "Native Control 2", "Planned", "Reserved for the audited chip core." },
            { "native3", "Native Control 3", "Planned", "Reserved for the audited chip core." },
            { "native4", "Native Control 4", "Planned", "Reserved for the audited chip core." },
        },
        commonMacros(),
        false
    };
    return descriptor;
}

const std::vector<ChipDescriptor>& descriptors()
{
    static const std::vector<ChipDescriptor> items {
        {
            ChipMode::nes,
            "NES / RP2A03",
            "Pulse, triangle, and noise controls map to the partial RP2A03 register model.",
            {
                { "duty", "Pulse Duty", "Channels", "Chooses the pulse duty family: 12.5%, 25%, 50%, or 75%." },
                { "motion", "Sweep Motion", "Pitch", "Scales macro pitch rise/drop behavior." },
                { "noise", "Noise Bite", "Noise", "Moves the noise period and level for drums, hits, and lasers." },
                { "mix", "Channel Focus", "Mixer", "Balances pulse stack, triangle bass, and noise emphasis." },
            },
            commonMacros(),
            true
        },
        {
            ChipMode::dmg,
            "Game Boy / DMG APU",
            "Two pulse channels, wave RAM, and noise controls map to the partial DMG APU register model.",
            {
                { "duty", "Pulse Duty", "Pulse", "Chooses the pulse duty family for channel 1 and 2." },
                { "sweep", "Sweep Shape", "Pitch", "Scales sweep-like macro pitch offsets." },
                { "noise", "Noise Clock", "Noise", "Moves the noise clock and narrow-noise behavior." },
                { "level", "Envelope Level", "Mixer", "Sets the initial envelope level used by macro templates." },
            },
            commonMacros(),
            true
        },
        {
            ChipMode::ym2149,
            "YM2149 / AY",
            "Three square channels plus shared noise and hardware-envelope-style behavior.",
            {
                { "spread", "Channel Spread", "Channels", "Sets A/B/C interval spread for chords and fake arps." },
                { "motion", "Pitch Motion", "Pitch", "Scales macro pitch movement." },
                { "noise", "Noise Pitch", "Noise", "Sets the shared noise generator period." },
                { "toneNoise", "Tone/Noise Mix", "Mixer", "Blends tone-only, noise-only, and combined channel behavior." },
            },
            commonMacros(),
            true
        },
        {
            ChipMode::sn76489,
            "SN76489 / Sega PSG",
            "Three tone channels and one noise channel map to SN76489-style latch/register behavior.",
            {
                { "stack", "Tone Stack", "Channels", "Sets interval spread across the three tone channels." },
                { "motion", "Pitch Motion", "Pitch", "Scales macro pitch movement." },
                { "noise", "Noise Mode", "Noise", "Moves between periodic and white-noise behaviors." },
                { "level", "Noise Level", "Mixer", "Balances the noise channel against tone channels." },
            },
            commonMacros(),
            true
        }
    };
    return items;
}

float clampControl(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

const ChipDescriptor& descriptorFor(ChipMode mode)
{
    const auto& items = descriptors();
    const auto iter = std::find_if(items.begin(), items.end(), [mode](const auto& item) { return item.mode == mode; });
    return iter != items.end() ? *iter : fallbackDescriptor();
}

const MacroTemplate& macroTemplateFor(ChipMode mode, MacroKind macro)
{
    const auto& descriptor = descriptorFor(mode);
    const auto iter = std::find_if(descriptor.macros.begin(), descriptor.macros.end(), [macro](const auto& item) { return item.macro == macro; });
    if (iter != descriptor.macros.end())
        return *iter;

    return descriptor.macros.front();
}

std::vector<MacroKind> macroOrder()
{
    return {
        MacroKind::manual,
        MacroKind::coin,
        MacroKind::bass,
        MacroKind::lead,
        MacroKind::arp,
        MacroKind::drum,
        MacroKind::hit,
        MacroKind::laser,
        MacroKind::jump,
        MacroKind::powerUp
    };
}

PatchConfig makePatchConfig(ChipMode mode,
                            MacroKind macro,
                            float control1,
                            float control2,
                            float control3,
                            float control4)
{
    const auto& templ = macroTemplateFor(mode, macro);
    const auto blend = [](float templated, float user)
    {
        return clampControl(templated + ((user - 0.5f) * 0.6f));
    };

    return {
        macro,
        blend(templ.controls[0], control1),
        blend(templ.controls[1], control2),
        blend(templ.controls[2], control3),
        blend(templ.controls[3], control4)
    };
}

} // namespace chipper
