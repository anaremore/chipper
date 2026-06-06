#include "Engine/ChipDescriptors.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

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

std::vector<MacroTemplate> nesMacros()
{
    return {
        { MacroKind::manual, "NES Manual", "Neutral RP2A03 pulse/triangle/noise mapping.", { 0.5f, 0.5f, 0.5f, 0.5f }, { true, true, true, true } },
        { MacroKind::coin, "NES Coin Blip", "Bright pulse pop with short register-style pitch lift.", { 0.15f, 0.85f, 0.10f, 0.80f }, { true, true, false, false }, 0.45f },
        { MacroKind::bass, "NES Triangle Bass", "Triangle-weighted bass body with supporting pulse.", { 0.55f, 0.20f, 0.10f, 0.35f }, { true, false, true, false } },
        { MacroKind::lead, "NES Hero Pulse", "Forward pulse lead with square-duty emphasis.", { 0.70f, 0.50f, 0.20f, 0.55f }, { true, true, true, false } },
        { MacroKind::arp, "NES Fast Arp", "Pulse-stack setup for fast note patterns.", { 0.65f, 0.75f, 0.15f, 0.60f }, { true, true, true, false } },
        { MacroKind::drum, "NES Noise Drum", "Noise and triangle transient setup.", { 0.40f, 0.15f, 0.80f, 0.45f }, { false, false, true, true }, 0.78f },
        { MacroKind::hit, "NES Boss Hit", "Noisy impact with pulse edge.", { 0.45f, 0.25f, 0.70f, 0.65f }, { true, false, true, true }, 0.55f },
        { MacroKind::laser, "NES Laser", "Pitch drop with short noise bite.", { 0.25f, 1.00f, 0.35f, 0.95f }, { true, true, false, true }, 0.35f },
        { MacroKind::jump, "NES Jump", "Rising pulse blip.", { 0.25f, 0.70f, 0.05f, 0.80f }, { true, false, false, false }, 0.35f },
        { MacroKind::powerUp, "NES Power-Up", "Optimistic rising pulse stack.", { 0.70f, 0.90f, 0.20f, 0.90f }, { true, true, true, false } },
    };
}

std::vector<MacroTemplate> dmgMacros()
{
    return {
        { MacroKind::manual, "DMG Manual", "Neutral DMG pulse/wave/noise mapping.", { 0.5f, 0.5f, 0.5f, 0.5f }, { true, true, true, true } },
        { MacroKind::coin, "DMG Coin", "Handheld pulse pop with CH1 sweep flavor.", { 0.15f, 0.85f, 0.10f, 0.80f }, { true, true, false, false }, 0.35f },
        { MacroKind::bass, "DMG Wave Bass", "Wave-channel body with restrained noise.", { 0.55f, 0.20f, 0.10f, 0.35f }, { false, false, true, false }, 0.0f, 1 },
        { MacroKind::lead, "DMG Pulse Lead", "Compact dual-pulse melody setup.", { 0.70f, 0.50f, 0.20f, 0.55f }, { true, true, true, false } },
        { MacroKind::arp, "DMG Pocket Arp", "Pulse and wave channels arranged for fast patterns.", { 0.65f, 0.75f, 0.15f, 0.60f }, { true, true, true, false }, 0.0f, 4 },
        { MacroKind::drum, "DMG Noise Drum", "Polynomial-noise percussion setup.", { 0.40f, 0.15f, 0.80f, 0.45f }, { false, false, false, true }, 0.88f },
        { MacroKind::hit, "DMG Hit", "Short handheld impact.", { 0.45f, 0.25f, 0.70f, 0.65f }, { true, false, false, true }, 0.60f },
        { MacroKind::laser, "DMG Laser", "Sweep-heavy SFX tone with narrow-noise edge.", { 0.25f, 1.00f, 0.35f, 0.95f }, { true, true, false, true }, 0.40f },
        { MacroKind::jump, "DMG Jump", "Quick rising pocket-game blip.", { 0.25f, 0.70f, 0.05f, 0.80f }, { true, false, false, false }, 0.30f },
        { MacroKind::powerUp, "DMG Power-Up", "Longer rising handheld stack.", { 0.70f, 0.90f, 0.20f, 0.90f }, { true, true, true, false }, 0.0f, 2 },
    };
}

std::vector<MacroTemplate> ym2149Macros()
{
    return {
        { MacroKind::manual, "YM Manual", "Neutral AY/YM tone/noise mapping.", { 0.5f, 0.5f, 0.5f, 0.5f }, { true, true, true, true } },
        { MacroKind::coin, "YM Menu Beep", "Bright square beep with a short pitch gesture.", { 0.15f, 0.85f, 0.10f, 0.80f }, { true, false, false, false } },
        { MacroKind::bass, "YM Buzzy Bass", "Low square-channel stack.", { 0.55f, 0.20f, 0.10f, 0.35f }, { true, true, false, false } },
        { MacroKind::lead, "YM Beep Lead", "Forward tone-channel lead.", { 0.70f, 0.50f, 0.20f, 0.55f }, { true, true, true, false } },
        { MacroKind::arp, "YM Three-Voice Arp", "A/B/C fake-chord spread for fast arps.", { 0.65f, 0.75f, 0.15f, 0.60f }, { true, true, true, false } },
        { MacroKind::drum, "YM Noise Perc", "Shared-noise percussion setup.", { 0.40f, 0.15f, 0.80f, 0.45f }, { true, false, false, true }, 0.0f, 0, 1 },
        { MacroKind::hit, "YM Arcade Hit", "Noisy PSG impact.", { 0.45f, 0.25f, 0.70f, 0.65f }, { true, false, false, true }, 0.0f, 0, 1 },
        { MacroKind::laser, "YM Laser", "Pitch-motion arcade SFX.", { 0.25f, 1.00f, 0.35f, 0.95f }, { true, true, false, true }, 0.0f, 0, 3 },
        { MacroKind::jump, "YM UI Jump", "Rising computer beeper gesture.", { 0.25f, 0.70f, 0.05f, 0.80f }, { true, false, false, false } },
        { MacroKind::powerUp, "YM Power-Up", "Bright stepped rise for classic UI.", { 0.70f, 0.90f, 0.20f, 0.90f }, { true, true, true, false }, 0.0f, 0, 4 },
    };
}

std::vector<MacroTemplate> sn76489Macros()
{
    return {
        { MacroKind::manual, "Manual", "Neutral PSG tone/noise mapping.", { 0.5f, 0.5f, 0.5f, 0.5f }, { true, true, true, true } },
        { MacroKind::coin, "PSG Coin", "Bright tone-only arcade pop.", { 0.15f, 0.90f, 0.10f, 0.15f }, { true, false, false, false } },
        { MacroKind::bass, "PSG Bass", "Low tone stack with muted noise.", { 0.35f, 0.20f, 0.10f, 0.20f }, { true, true, false, false } },
        { MacroKind::lead, "PSG Lead", "Forward square lead across tone channels.", { 0.50f, 0.45f, 0.20f, 0.35f }, { true, true, true, false } },
        { MacroKind::arp, "PSG Arp", "Three-tone fake chord spread.", { 0.75f, 0.75f, 0.15f, 0.30f }, { true, true, true, false } },
        { MacroKind::drum, "PSG Drum", "Noise-channel percussion register setup.", { 0.20f, 0.15f, 0.80f, 1.00f }, { false, false, false, true } },
        { MacroKind::hit, "PSG Hit", "Short noisy impact with tone accent.", { 0.25f, 0.30f, 0.75f, 0.95f }, { true, false, false, true } },
        { MacroKind::laser, "PSG Laser", "Pitch motion with white-noise bite.", { 0.20f, 1.00f, 0.60f, 0.95f }, { true, true, false, true } },
        { MacroKind::jump, "PSG Jump", "Rising tone blip with noise muted.", { 0.20f, 0.70f, 0.05f, 0.15f }, { true, false, false, false } },
        { MacroKind::powerUp, "PSG Power-Up", "Wide triumphant tone stack.", { 0.85f, 0.90f, 0.20f, 0.25f }, { true, true, true, false } },
    };
}

ParameterChoiceSpec choice(std::string label, std::string help, float normalizedValue, int choiceValue)
{
    return { label, help, normalizedValue, choiceValue };
}

ChipParameterSpec sliderSpec(ChipParameterRole role,
                             std::string id,
                             std::string label,
                             std::string group,
                             std::string help,
                             ParameterKind kind = ParameterKind::macro,
                             float defaultValue = 0.5f)
{
    return { role, id, label, group, help, kind, ControlSurface::slider, {}, 0.0f, 1.0f, defaultValue };
}

ChipParameterSpec segmentedSpec(ChipParameterRole role,
                                std::string id,
                                std::string label,
                                std::string group,
                                std::string help,
                                std::vector<ParameterChoiceSpec> choices,
                                ParameterKind kind = ParameterKind::chipRegister,
                                float defaultValue = 0.0f)
{
    return { role, id, label, group, help, kind, ControlSurface::segmentedChoice, choices, 0.0f, 1.0f, defaultValue };
}

ChipParameterSpec sourceSpec(ChipParameterRole role, std::string id, std::string label, std::string help)
{
    return { role, id, label, "Sources", help, ParameterKind::booleanToggle, ControlSurface::sourceCards, {}, 0.0f, 1.0f, 1.0f };
}

ChipParameterSpec envelopeSpec(std::string id, std::string label, std::string help)
{
    return { ChipParameterRole::envelopeDecay, id, label, "Envelope", help, ParameterKind::chipRegister, ControlSurface::slider, {}, 0.0f, 1.0f, 0.0f };
}

std::vector<ParameterChoiceSpec> pulseDutyChoices(std::string thinHelp, std::string narrowHelp, std::string squareHelp, std::string wideHelp)
{
    return {
        choice("12.5%", thinHelp, 0.0f, 0),
        choice("25%", narrowHelp, 1.0f / 3.0f, 1),
        choice("50%", squareHelp, 2.0f / 3.0f, 2),
        choice("75%", wideHelp, 1.0f, 3)
    };
}

std::vector<ChipParameterSpec> nesParameterSpecs()
{
    return {
        segmentedSpec(ChipParameterRole::macroControl1,
                      "nes.pulseDuty",
                      "Pulse Duty",
                      "Channels",
                      "Maps to the RP2A03 pulse duty register field.",
                      pulseDutyChoices("Thin 1/8 pulse.", "Narrow 1/4 pulse.", "Square 1/2 pulse.", "Wide 3/4 inverted pulse."),
                      ParameterKind::chipRegister,
                      2.0f / 3.0f),
        sliderSpec(ChipParameterRole::macroControl2, "nes.sweepMotion", "Sweep Motion", "Pitch", "Scales musical pitch gestures into RP2A03 sweep/timer behavior."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "nes.noisePeriod",
                   "Noise Period",
                   "Noise",
                   "Maps to the RP2A03 $400E noise period nibble; macros may also use it for percussion level.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4, "nes.channelFocus", "Channel Focus", "Mixer", "Balances pulse stack, triangle bass, and noise emphasis."),
        sourceSpec(ChipParameterRole::source1Enabled, "nes.pulse1.enabled", "Pulse 1", "Enable the first RP2A03 pulse source."),
        sourceSpec(ChipParameterRole::source2Enabled, "nes.pulse2.enabled", "Pulse 2", "Enable the second RP2A03 pulse source."),
        sourceSpec(ChipParameterRole::source3Enabled, "nes.triangle.enabled", "Triangle", "Enable the triangle bass source."),
        sourceSpec(ChipParameterRole::source4Enabled, "nes.noise.enabled", "Noise", "Enable the RP2A03 noise source."),
        envelopeSpec("nes.envelopeDecay", "Envelope Decay", "Maps musical decay to APU envelope period values."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "nes.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to RP2A03 $400E bit 7: long LFSR noise or short-loop metallic noise. Macro resolves from the selected musical template.",
                      {
                          choice("Macro", "Use the selected NES macro to choose long or short noise.", 0.0f, 0),
                          choice("Long", "Bit 7 = 0, long 15-bit LFSR noise for softer hats and static.", 0.5f, 1),
                          choice("Short", "Bit 7 = 1, short-loop metallic noise for snares and pitched grit.", 1.0f, 2)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> dmgParameterSpecs()
{
    return {
        segmentedSpec(ChipParameterRole::macroControl1,
                      "dmg.pulseDuty",
                      "Pulse Duty",
                      "Pulse",
                      "Maps to the DMG pulse duty register field for channels 1 and 2.",
                      pulseDutyChoices("Thin 1/8 handheld pulse.", "Narrow 1/4 handheld pulse.", "Square 1/2 pulse.", "Wide 3/4 handheld pulse."),
                      ParameterKind::chipRegister,
                      2.0f / 3.0f),
        sliderSpec(ChipParameterRole::macroControl2,
                   "dmg.sweepShift",
                   "Sweep Shift",
                   "Pitch",
                   "Maps to the DMG NR10 sweep-shift bits for channel 1; macros may also use it for pitch gestures.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl3,
                   "dmg.noiseClock",
                   "Noise Clock",
                   "Noise",
                   "Maps to the DMG NR43 clock shift used by the musical noise control while Noise Mode selects the LFSR width.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "dmg.envelopeLevel",
                   "Envelope Level",
                   "Envelope",
                   "Maps to the DMG NRx2 initial volume register nibble for pulse channels.",
                   ParameterKind::chipRegister),
        sourceSpec(ChipParameterRole::source1Enabled, "dmg.pulse1.enabled", "Pulse 1", "Enable DMG pulse channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "dmg.pulse2.enabled", "Pulse 2", "Enable DMG pulse channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "dmg.wave.enabled", "Wave", "Enable the DMG Wave RAM channel."),
        sourceSpec(ChipParameterRole::source4Enabled, "dmg.noise.enabled", "Noise", "Enable the DMG polynomial-noise channel."),
        envelopeSpec("dmg.envelopeDecay", "Envelope Decay", "Maps musical decay to DMG 64 Hz hardware envelope periods."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "dmg.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to NR43 bit 3: 15-bit LFSR noise or 7-bit metallic short noise. Macro resolves from the selected DMG template.",
                      {
                          choice("Macro", "Use the selected DMG macro to choose the noise width.", 0.0f, 0),
                          choice("15-bit", "NR43 bit 3 = 0, standard wide LFSR noise.", 0.5f, 1),
                          choice("7-bit", "NR43 bit 3 = 1, narrow metallic LFSR noise.", 1.0f, 2)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::waveShape,
                      "dmg.waveShape",
                      "Wave Shape",
                      "Wave RAM",
                      "Writes a helper shape into the DMG wave table when not preserving register-trace RAM.",
                      {
                          choice("RAM", "Preserve current/register-trace Wave RAM.", 0.0f, 0),
                          choice("Tri", "Write a 32-sample triangle into Wave RAM.", 0.25f, 1),
                          choice("Saw", "Write a 32-sample saw ramp into Wave RAM.", 0.5f, 2),
                          choice("Pulse", "Write a 50% pulse into Wave RAM.", 0.75f, 3),
                          choice("Steps", "Write a stepped 4-level table into Wave RAM.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> ym2149ParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1, "ym2149.channelSpread", "Channel Spread", "Channels", "Sets A/B/C interval spread for chords and fake arps."),
        sliderSpec(ChipParameterRole::macroControl2, "ym2149.pitchMotion", "Pitch Motion", "Pitch", "Scales macro pitch movement."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "ym2149.noisePitch",
                   "Noise Pitch",
                   "Noise",
                   "Maps to YM/AY register 6, the 5-bit shared noise period.",
                   ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::macroControl4,
                      "ym2149.toneNoiseMix",
                      "Tone/Noise Mix",
                      "Mixer",
                      "Maps to YM/AY register 7 mixer bits for tone-only, noise-only, or combined tone+noise output.",
                      {
                          choice("Noise", "Register 7 = 0x07: tone disabled and shared noise enabled on A/B/C.", 0.0f, 0),
                          choice("Tone", "Register 7 = 0x38: tone enabled and shared noise disabled on A/B/C.", 0.5f, 1),
                          choice("Both", "Register 7 = 0x00: tone and shared noise both enabled on A/B/C.", 1.0f, 2)
                      },
                      ParameterKind::chipRegister,
                      0.5f),
        sourceSpec(ChipParameterRole::source1Enabled, "ym2149.channelA.enabled", "Channel A", "Enable YM/AY channel A."),
        sourceSpec(ChipParameterRole::source2Enabled, "ym2149.channelB.enabled", "Channel B", "Enable YM/AY channel B."),
        sourceSpec(ChipParameterRole::source3Enabled, "ym2149.channelC.enabled", "Channel C", "Enable YM/AY channel C."),
        sourceSpec(ChipParameterRole::source4Enabled, "ym2149.noise.enabled", "Shared Noise", "Enable the shared YM/AY noise source."),
        envelopeSpec("ym2149.envelopeSpeed", "Envelope Speed", "Maps musical envelope speed to YM/AY registers 11 and 12. Zero uses the default register period."),
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "ym2149.envelopeShape",
                      "Envelope Shape",
                      "Envelope",
                      "Maps directly to the YM/AY hardware envelope shape register.",
                      {
                          choice("Fixed", "Use fixed volume registers; envelope bit off.", 0.0f, 0),
                          choice("Fall", "Register 13 = 0x09, fall then hold low.", 0.25f, 1),
                          choice("Rise", "Register 13 = 0x0D, rise then hold high.", 0.5f, 2),
                          choice("Saw", "Register 13 = 0x08, repeating saw down.", 0.75f, 3),
                          choice("Tri", "Register 13 = 0x0E, repeating triangle.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> sn76489ParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1, "sn76489.toneStack", "Tone Stack", "Channels", "Sets interval spread across the three tone channels."),
        sliderSpec(ChipParameterRole::macroControl2, "sn76489.pitchMotion", "Pitch Motion", "Pitch", "Scales macro pitch movement."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "sn76489.noiseBias",
                   "Noise Bias",
                   "Noise",
                   "Biases the macro-selected SN76489 noise-control register bits when Noise Mode is Macro; explicit Noise Mode choices override it.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "sn76489.noiseLevel",
                   "Noise Level",
                   "Mixer",
                   "Maps to the SN76489 4-bit noise attenuation register; higher level means lower attenuation.",
                   ParameterKind::chipRegister),
        sourceSpec(ChipParameterRole::source1Enabled, "sn76489.tone1.enabled", "Tone 1", "Enable SN76489 tone channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "sn76489.tone2.enabled", "Tone 2", "Enable SN76489 tone channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "sn76489.tone3.enabled", "Tone 3", "Enable SN76489 tone channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "sn76489.noise.enabled", "Noise", "Enable the SN76489 noise channel."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "sn76489.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to the SN76489 noise-control register. Macro resolves from the current musical template.",
                      {
                          choice("Macro", "Use the chip-specific macro to choose the noise register bits.", 0.0f, 0),
                          choice("P-Lo", "Periodic noise, low rate.", 0.25f, 1),
                          choice("P-Hi", "Periodic noise, high rate.", 0.5f, 2),
                          choice("W-Lo", "White noise, low rate.", 0.75f, 3),
                          choice("W-T3", "White noise clocked from tone 3.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

ModuleDescriptor makeModule(std::string id, std::string title, std::string summary, std::initializer_list<const char*> items)
{
    ModuleDescriptor module { id, title, summary, {} };
    for (const auto* item : items)
        module.items.emplace_back(item);
    return module;
}

std::array<ModuleDescriptor, 6> plannedModules(std::string sourceTitle, std::string toneTitle)
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "Awaiting an audited core or clean-room model.", { "Hybrid default", "Authentic hidden until verified", "Clock profile reserved", "License audit required" }),
        makeModule("sources", sourceTitle, "Chip-specific source layout is planned.", { "Native channels planned", "Register adapter required", "Preset mappings reserved", "Silent until implemented" }),
        makeModule("tone", toneTitle, "Tone controls are placeholders until the core exists.", { "Native shaping planned", "Chip limits preserved", "Hybrid helper planned", "No accuracy claim yet" }),
        makeModule("envelope", "Envelope", "Envelope behavior must come from chip-native timing.", { "Hardware envelope planned", "Length behavior planned", "Hybrid helper planned", "Regression tests required" }),
        makeModule("motion", "Motion", "Musical gestures will map to native registers.", { "Arp mapping planned", "Pitch motion planned", "Retrigger planned", "SFX templates planned" }),
        makeModule("output", "Output", "Output model waits for source validation.", { "Output level", "Variant coloration planned", "Golden references required", "Known differences documented" })
    };
}

std::array<ModuleDescriptor, 6> nesModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "RP2A03-inspired clean-room APU model.", { "2A03 family", "NTSC/PAL clock override", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Native channel layout exposed musically.", { "Pulse 1", "Pulse 2", "Triangle / Chip Poly", "Noise / DMC planned" }),
        makeModule("tone", "Shape / Mixer", "Pulse, triangle, noise, and nonlinear mixer behavior.", { "Pulse duty", "Pitch sweep macro", "Noise mode", "Nonlinear mixer" }),
        makeModule("envelope", "Envelope", "APU envelope and duration behavior.", { "Simple envelope", "Length counters", "Triangle linear planned", "Drum decay" }),
        makeModule("motion", "Motion", "Macro gestures write chip-like register templates.", { "Coin blip", "Jump rise", "Laser sweep", "Fast arps" }),
        makeModule("output", "Output", "Bright direct mono chip output.", { "Output gain", "Dry mono", "Bit/sample grit", "SFX templates" })
    };
}

std::array<ModuleDescriptor, 6> dmgModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "DMG APU clean-room register model.", { "DMG profile", "CGB quirks planned", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Four hardware sound generators.", { "Pulse 1 / sweep", "Pulse 2", "Wave RAM", "Chip Poly ready" }),
        makeModule("tone", "Wave / Noise", "Duty, wave RAM, and polynomial noise behavior.", { "Pulse duty", "Wave shape", "Noise clock", "Narrow noise" }),
        makeModule("envelope", "Envelope", "Hardware envelope, length, and sweep groundwork.", { "64 Hz envelope", "256 Hz length", "DAC gating", "128 Hz CH1 sweep" }),
        makeModule("motion", "Motion", "Portable-game gesture templates.", { "Arp stack", "Pitch rise/drop", "Retrigger", "Coin/noise SFX" }),
        makeModule("output", "Output", "Compact handheld output character.", { "Output gain", "NR50 volume", "NR51 routing", "Speaker color planned" })
    };
}

std::array<ModuleDescriptor, 6> ym2149Modules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "AY/YM PSG clean-room register model.", { "AY / YM2149", "Clock override", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Three tone channels plus shared noise.", { "Channel A", "Channel B", "Channel C", "Shared noise" }),
        makeModule("tone", "Mixer / Envelope", "Tone/noise mixer and hardware envelope shapes.", { "Tone/noise bits", "Noise period", "Envelope shape", "Crunch" }),
        makeModule("envelope", "Envelope", "Hardware-style volume and envelope timing.", { "Volume registers", "Env speed", "Shape select", "Hybrid helper" }),
        makeModule("motion", "Motion", "Classic fake-chord and demo-scene movement.", { "Fast arps", "Fake chords", "Pattern retrigger", "Pitch motion" }),
        makeModule("output", "Output", "Bright buzzy computer output.", { "Output gain", "Stereo spread", "Alias character", "Preset suggestions" })
    };
}

std::array<ModuleDescriptor, 6> sn76489Modules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "SN76489 PSG clean-room register model.", { "Sega PSG", "Clock override", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Three tone channels and one noise channel.", { "Tone 1", "Tone 2", "Tone 3", "Noise channel" }),
        makeModule("tone", "Tone / Crunch", "Tone periods, noise mode, and attenuation.", { "Tone stack", "White noise", "Periodic noise", "Attenuation" }),
        makeModule("envelope", "Envelope", "Volume envelopes are Chipper helpers over PSG attenuation.", { "Level steps", "Retrigger decay", "Hybrid ADSR", "SFX decay" }),
        makeModule("motion", "Motion", "Sega-style beeps and fake chord motion.", { "Arp stack", "Fake chords", "Pitch sweep", "UI blips" }),
        makeModule("output", "Output", "Clean PSG output with optional modern spread.", { "Output gain", "Stereo spread", "Crunch", "Preset suggestions" })
    };
}

std::array<ModuleDescriptor, 6> sidModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "SID strategy is planned; no accurate core is integrated.", { "6581 planned", "8580 planned", "PAL/NTSC planned", "License decision required" }),
        makeModule("sources", "Voices", "Three SID oscillator voices.", { "Voice 1", "Voice 2", "Voice 3", "Noise / pulse / saw / tri" }),
        makeModule("tone", "Filter", "Analog-style multimode filter controls.", { "Cutoff", "Resonance", "LP / BP / HP", "Drive / drift" }),
        makeModule("envelope", "Envelope", "SID ADSR behavior and quirks.", { "Attack", "Decay", "Sustain", "Release" }),
        makeModule("motion", "Motion", "Classic SID modulation gestures.", { "PWM", "Sync", "Ring mod", "Vibrato / arp" }),
        makeModule("output", "Output", "Warm driven C64 output character.", { "Output gain", "6581/8580 color", "Filter drive", "Known differences" })
    };
}

std::array<ModuleDescriptor, 6> fmModules(std::string profile, std::string sourceTitle, std::string toneTitle)
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", profile, { "Chip variant", "Clock profile", "Hybrid default", "Core audit required" }),
        makeModule("sources", sourceTitle, "FM voice selection and routing.", { "Voice select", "Algorithm", "Feedback", "Voice level" }),
        makeModule("tone", toneTitle, "Operator-level tone shaping.", { "Operator ratios", "Operator levels", "Feedback", "Waveforms where native" }),
        makeModule("envelope", "Envelope", "Native operator envelope controls.", { "Attack", "Decay", "Sustain", "Release" }),
        makeModule("motion", "Motion", "FM modulation and performance helpers.", { "LFO", "Pitch mod", "Arp/glide", "Macro morph" }),
        makeModule("output", "Output", "Chip output and stereo behavior.", { "Pan / stereo", "DAC grit", "Output gain", "Reference tests needed" })
    };
}

std::array<ModuleDescriptor, 6> sampleModules(std::string profile, std::string sourceTitle, std::string toneTitle)
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", profile, { "Model profile", "Clock/rate profile", "Hybrid default", "Validation required" }),
        makeModule("sources", sourceTitle, "Sample or wavetable sources.", { "Channel select", "Sample/wave slot", "Loop behavior", "Level" }),
        makeModule("tone", toneTitle, "Native playback coloration.", { "Pitch/period", "Rate reduction", "Aliasing", "Crunch" }),
        makeModule("envelope", "Envelope", "Tracker-style level shaping.", { "Gate length", "Volume steps", "Loop decay", "Hybrid helper" }),
        makeModule("motion", "Motion", "Tracker and arcade gestures.", { "Retrigger", "Pitch slide", "Wave scan", "Pattern macro" }),
        makeModule("output", "Output", "Machine-specific output coloration.", { "Output gain", "Stereo/voice pan", "Drive", "Reference tests needed" })
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
        plannedModules("Sources", "Tone"),
        commonMacros(),
        false,
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
                { "noise", "Noise Period", "Noise", "Maps musical noise color to the RP2A03 $400E period nibble." },
                { "mix", "Channel Focus", "Mixer", "Balances pulse stack, triangle bass, and noise emphasis." },
            },
            nesModules(),
            nesMacros(),
            true,
            true,
            nesParameterSpecs()
        },
        {
            ChipMode::dmg,
            "Game Boy / DMG APU",
            "Two pulse channels, wave RAM, and noise controls map to the partial DMG APU register model.",
            {
                { "duty", "Pulse Duty", "Pulse", "Chooses the pulse duty family for channel 1 and 2." },
                { "sweep", "Sweep Shift", "Pitch", "Maps pitch gestures to the DMG NR10 sweep shift." },
                { "noise", "Noise Clock", "Noise", "Moves the noise clock and narrow-noise behavior." },
                { "level", "Envelope Level", "Mixer", "Sets the initial envelope level used by macro templates." },
            },
            dmgModules(),
            dmgMacros(),
            true,
            true,
            dmgParameterSpecs()
        },
        {
            ChipMode::sid,
            "SID / C64",
            "Planned SID voice, PWM, filter, and ADSR UI. Audio remains silent until an audited SID strategy is implemented.",
            {
                { "voiceMix", "Voice Mix", "Voices", "Planned control for the three SID voices." },
                { "pwm", "PWM Depth", "Motion", "Planned control for pulse-width modulation amount." },
                { "filter", "Filter Drive", "Filter", "Planned control for cutoff/resonance/drive mapping." },
                { "drift", "Drift", "Character", "Planned control for chip instability and variant color." },
            },
            sidModules(),
            commonMacros(),
            false
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
            ym2149Modules(),
            ym2149Macros(),
            true,
            true,
            ym2149ParameterSpecs()
        },
        {
            ChipMode::sn76489,
            "SN76489 / Sega PSG",
            "Three tone channels and one noise channel map to SN76489-style latch/register behavior.",
            {
                { "stack", "Tone Stack", "Channels", "Sets interval spread across the three tone channels." },
                { "motion", "Pitch Motion", "Pitch", "Scales macro pitch movement." },
                { "noise", "Noise Bias", "Noise", "Biases the macro-selected noise register. Explicit SN Noise Mode choices override it." },
                { "level", "Noise Level", "Mixer", "Balances the noise channel against tone channels." },
            },
            sn76489Modules(),
            sn76489Macros(),
            true,
            true,
            sn76489ParameterSpecs()
        },
        {
            ChipMode::ym2612,
            "YM2612 / Genesis FM",
            "Planned six-voice FM mode. No OPN2 core is integrated yet.",
            {
                { "algorithm", "Algorithm", "FM", "Planned OPN2 algorithm selection." },
                { "feedback", "Feedback", "FM", "Planned feedback amount." },
                { "operator", "Operator Tone", "Operators", "Planned operator ratio/level macro." },
                { "dac", "DAC Grit", "Output", "Planned DAC and stereo output character." },
            },
            fmModules("YM2612/OPN2 strategy planned.", "FM Voices", "Operators"),
            commonMacros(),
            false
        },
        {
            ChipMode::opl3,
            "OPL2/OPL3 / DOS FM",
            "Planned two-operator FM mode. No OPL core is integrated yet.",
            {
                { "algorithm", "Operator Pair", "FM", "Planned modulator/carrier balance." },
                { "feedback", "Feedback", "FM", "Planned feedback amount." },
                { "rhythm", "Rhythm Mode", "Drums", "Planned OPL rhythm-mode helper." },
                { "tremolo", "Tremolo", "Motion", "Planned tremolo/vibrato helper." },
            },
            fmModules("OPL2/OPL3 strategy planned.", "FM Voices", "Operator Pairs"),
            commonMacros(),
            false
        },
        {
            ChipMode::spc700,
            "SNES SPC700-style",
            "Planned lo-fi sample playback mode. No SPC700/DSP core is integrated yet.",
            {
                { "sample", "Sample Slot", "Sources", "Planned sample source selector." },
                { "rate", "Playback Rate", "Tone", "Planned lo-fi rate control." },
                { "envelope", "Envelope", "Envelope", "Planned ADSR/gain helper." },
                { "echo", "Echo Color", "Output", "Planned SNES-style ambience helper." },
            },
            sampleModules("SNES sample-style strategy planned.", "Sample Voices", "Lo-fi Sample"),
            commonMacros(),
            false
        },
        {
            ChipMode::pokey,
            "Atari POKEY",
            "Planned polynomial-counter tone/noise mode. No POKEY core is integrated yet.",
            {
                { "channels", "Channel Mix", "Sources", "Planned four-channel mix control." },
                { "distortion", "Distortion", "Tone", "Planned POKEY distortion selector." },
                { "poly", "Poly Noise", "Noise", "Planned polynomial noise control." },
                { "filter", "High-pass", "Output", "Planned channel filter helper." },
            },
            plannedModules("POKEY Channels", "Distortion / Noise"),
            commonMacros(),
            false
        },
        {
            ChipMode::paula,
            "Amiga Paula",
            "Planned tracker sampler mode. No Paula model is integrated yet.",
            {
                { "sample", "Sample Mix", "Sources", "Planned four-channel sample mix." },
                { "period", "Period", "Tone", "Planned Paula period control." },
                { "loop", "Loop", "Envelope", "Planned loop and one-shot behavior." },
                { "tracker", "Tracker Grit", "Output", "Planned tracker coloration." },
            },
            sampleModules("Paula tracker-sampler strategy planned.", "Tracker Channels", "Sample Period"),
            commonMacros(),
            false
        },
        {
            ChipMode::huc6280,
            "PC Engine HuC6280",
            "Planned wavetable/noise mode. No HuC6280 core is integrated yet.",
            {
                { "wave", "Wave Blend", "Sources", "Planned wavetable channel blend." },
                { "noise", "Noise", "Tone", "Planned native noise helper." },
                { "lfo", "LFO", "Motion", "Planned channel LFO helper." },
                { "stereo", "Stereo", "Output", "Planned modern stereo spread." },
            },
            sampleModules("HuC6280 wavetable strategy planned.", "Wavetable Voices", "Wave / Noise"),
            commonMacros(),
            false
        },
        {
            ChipMode::namcoWsg,
            "Namco arcade WSG",
            "Planned arcade wavetable mode. No Namco WSG core is integrated yet.",
            {
                { "voices", "Voice Count", "Sources", "Planned WSG voice stack." },
                { "wave", "Wave Step", "Tone", "Planned waveform step control." },
                { "bright", "Brightness", "Tone", "Planned arcade brightness control." },
                { "grit", "Arcade Grit", "Output", "Planned cabinet/output helper." },
            },
            sampleModules("Namco WSG strategy planned.", "WSG Voices", "Wave Shape"),
            commonMacros(),
            false
        },
        {
            ChipMode::ym2151,
            "YM2151 arcade FM",
            "Planned arcade/X68000 four-operator FM mode. No OPM core is integrated yet.",
            {
                { "algorithm", "Algorithm", "FM", "Planned OPM algorithm selection." },
                { "feedback", "Feedback", "FM", "Planned feedback amount." },
                { "operator", "Operator Tone", "Operators", "Planned four-operator macro." },
                { "lfo", "LFO", "Motion", "Planned PM/AM LFO helper." },
            },
            fmModules("YM2151/OPM strategy planned.", "FM Voices", "Operators"),
            commonMacros(),
            false
        },
        {
            ChipMode::ym2413,
            "YM2413 / OPLL",
            "Planned preset-FM mode. No OPLL core is integrated yet.",
            {
                { "instrument", "Instrument", "FM", "Planned preset instrument selector." },
                { "modulator", "Modulator", "Operators", "Planned modulator macro." },
                { "carrier", "Carrier", "Operators", "Planned carrier macro." },
                { "tremolo", "Tremolo", "Motion", "Planned vibrato/tremolo helper." },
            },
            fmModules("YM2413/OPLL strategy planned.", "Preset FM Voices", "Operators"),
            commonMacros(),
            false
        },
        {
            ChipMode::scc,
            "Konami SCC",
            "Planned five-channel wavetable mode. No SCC core is integrated yet.",
            {
                { "stack", "Wave Stack", "Sources", "Planned SCC channel stack." },
                { "spread", "Channel Spread", "Motion", "Planned interval spread helper." },
                { "morph", "Wave Morph", "Tone", "Planned wave morph helper." },
                { "crunch", "Crunch", "Output", "Planned output crunch." },
            },
            sampleModules("Konami SCC wavetable strategy planned.", "Wavetable Channels", "Wave Shape"),
            commonMacros(),
            false
        },
        {
            ChipMode::arcade,
            "Arcade Hybrid",
            "Hybrid arcade SFX mode planned around verified chip building blocks.",
            {
                { "blend", "Source Blend", "Sources", "Planned blend across implemented chip models." },
                { "coin", "Coin Sweep", "Motion", "Planned arcade UI gesture." },
                { "noise", "Noise Burst", "Noise", "Planned impact/noise helper." },
                { "cabinet", "Cabinet Bite", "Output", "Planned output coloration." },
            },
            plannedModules("Arcade Sources", "SFX Shape"),
            commonMacros(),
            false
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

const ChipParameterSpec* parameterSpecFor(ChipMode mode, ChipParameterRole role)
{
    const auto& descriptor = descriptorFor(mode);
    const auto iter = std::find_if(descriptor.parameters.begin(), descriptor.parameters.end(), [role](const auto& item) { return item.role == role; });
    return iter != descriptor.parameters.end() ? &*iter : nullptr;
}

bool chipHasParameterSurface(ChipMode mode, ChipParameterRole role, ControlSurface surface)
{
    const auto* spec = parameterSpecFor(mode, role);
    return spec != nullptr && spec->surface == surface;
}

bool supportsPlayMode(ChipMode mode, PlayMode playMode)
{
    if (playMode == PlayMode::stack)
        return true;

    const auto& descriptor = descriptorFor(mode);
    if (playMode == PlayMode::chipPoly)
        return descriptor.implemented && descriptor.supportsChipPoly;

    return false;
}

std::vector<ChipMode> chipModeOrder()
{
    std::vector<ChipMode> modes;
    const auto& items = descriptors();
    modes.reserve(items.size() + 1u);
    for (const auto& item : items)
        modes.push_back(item.mode);
    modes.push_back(ChipMode::custom);
    return modes;
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
                            float control4,
                            PlayMode playMode,
                            std::array<bool, 4> sourceEnabled,
                            float envelopeDecay,
                            int waveShape,
                            int ymEnvelopeShape,
                            int snNoiseMode)
{
    const auto effectivePlayMode = supportsPlayMode(mode, playMode) ? playMode : PlayMode::stack;

    return {
        macro,
        clampControl(control1),
        clampControl(control2),
        clampControl(control3),
        clampControl(control4),
        effectivePlayMode,
        sourceEnabled,
        clampControl(envelopeDecay),
        std::clamp(waveShape, 0, 4),
        std::clamp(ymEnvelopeShape, 0, 4),
        std::clamp(snNoiseMode, 0, 4)
    };
}

uint8_t nesNoisePeriodForControl(float noisePeriodControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - clampControl(noisePeriodControl)) * 15.0f)), 0, 15));
}

uint8_t nesNoiseRegisterForPatch(const PatchConfig& patch)
{
    const auto period = nesNoisePeriodForControl(patch.control3);
    auto mode = uint8_t { 0x00u };

    switch (std::clamp(patch.snNoiseMode, 0, 2))
    {
        case 1:
            mode = 0x00u;
            break;
        case 2:
            mode = 0x80u;
            break;
        case 0:
        default:
            if ((patch.macro == MacroKind::drum && patch.control3 > 0.55f)
                || (patch.macro == MacroKind::hit && patch.control3 > 0.50f))
                mode = 0x80u;
            break;
    }

    return static_cast<uint8_t>(mode | period);
}

uint8_t dmgInitialEnvelopeLevelForControl(float envelopeLevelControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(envelopeLevelControl) * 15.0f)), 1, 15));
}

uint8_t dmgSweepShiftForControl(float sweepControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(sweepControl) * 7.0f)), 0, 7));
}

uint8_t dmgSweepRegisterForControl(float sweepControl)
{
    return static_cast<uint8_t>(0x18u | dmgSweepShiftForControl(sweepControl));
}

uint8_t dmgNoiseClockShiftForControl(float noiseClockControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - clampControl(noiseClockControl)) * 7.0f)), 0, 7));
}

uint8_t dmgNoiseRegisterForPatch(const PatchConfig& patch)
{
    const auto shift = dmgNoiseClockShiftForControl(patch.control3);
    auto width = uint8_t { 0x00u };

    switch (std::clamp(patch.snNoiseMode, 0, 2))
    {
        case 1:
            width = 0x00u;
            break;
        case 2:
            width = 0x08u;
            break;
        case 0:
        default:
            if (patch.control3 > 0.55f)
                width = 0x08u;
            break;
    }

    return static_cast<uint8_t>((shift << 4u) | width | 0x02u);
}

uint8_t ym2149NoisePeriodForControl(float noisePitchControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - clampControl(noisePitchControl)) * 30.0f)) + 1, 1, 31));
}

uint8_t ym2149MixerRegisterForControl(float toneNoiseControl)
{
    const auto value = clampControl(toneNoiseControl);
    if (value < 0.33f)
        return 0x07u;
    if (value > 0.66f)
        return 0x00u;
    return 0x38u;
}

uint16_t ym2149EnvelopePeriodForControl(float envelopeControl)
{
    const auto value = clampControl(envelopeControl);
    if (value <= 0.01f)
        return 0x0280u;

    constexpr auto slowPeriod = 16384.0f;
    constexpr auto fastPeriod = 16.0f;
    const auto period = slowPeriod * std::pow(fastPeriod / slowPeriod, value);
    return static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(period)), 1, 0xffff));
}

uint8_t sn76489NoiseAttenuationForControl(float noiseLevelControl)
{
    const auto levelStep = static_cast<int>(std::floor(clampControl(noiseLevelControl) * 15.0f));
    return static_cast<uint8_t>(std::clamp(15 - levelStep, 0, 15));
}

uint8_t sn76489NoiseControlForPatch(const PatchConfig& patch)
{
    switch (std::clamp(patch.snNoiseMode, 0, 4))
    {
        case 1: return 0x00u;
        case 2: return 0x02u;
        case 3: return 0x04u;
        case 4: return 0x07u;
        case 0:
        default:
            break;
    }

    switch (patch.macro)
    {
        case MacroKind::drum:
            return patch.control3 > 0.5f ? 0x04u : 0x00u;
        case MacroKind::hit:
        case MacroKind::laser:
            return static_cast<uint8_t>(0x04u | static_cast<unsigned>(std::round(patch.control3 * 2.0f)));
        case MacroKind::lead:
        case MacroKind::manual:
        default:
            return patch.control3 > 0.66f ? 0x04u : 0x03u;
    }
}

} // namespace chipper
