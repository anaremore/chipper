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
    auto macros = std::vector<MacroTemplate> {
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

    for (auto& macro : macros)
    {
        switch (macro.macro)
        {
            case MacroKind::drum:
                macro.nesDmcDirectLevel = 0.32f;
                break;
            case MacroKind::hit:
                macro.nesDmcDirectLevel = 0.62f;
                break;
            case MacroKind::laser:
                macro.nesDmcDirectLevel = 0.24f;
                break;
            default:
                macro.nesDmcDirectLevel = 0.0f;
                break;
        }
    }

    return macros;
}

std::vector<MacroTemplate> dmgMacros()
{
    return {
        { MacroKind::manual, "DMG Manual", "Neutral DMG pulse/wave/noise mapping.", { 0.5f, 0.5f, 0.5f, 0.5f }, { true, true, true, true } },
        { MacroKind::coin, "DMG Coin", "Handheld pulse pop with CH1 sweep flavor.", { 0.15f, 0.85f, 0.10f, 0.80f }, { true, true, false, false }, 0.35f, 0, 0, 0, 1 },
        { MacroKind::bass, "DMG Wave Bass", "Wave-channel body with restrained noise.", { 0.55f, 0.20f, 0.10f, 0.35f }, { false, false, true, false }, 0.0f, 1, 0, 0, 1 },
        { MacroKind::lead, "DMG Pulse Lead", "Compact dual-pulse melody setup.", { 0.70f, 0.50f, 0.20f, 0.55f }, { true, true, true, false }, 0.0f, 0, 0, 0, 4 },
        { MacroKind::arp, "DMG Pocket Arp", "Pulse and wave channels arranged for fast patterns.", { 0.65f, 0.75f, 0.15f, 0.60f }, { true, true, true, false }, 0.0f, 4, 0, 0, 4 },
        { MacroKind::drum, "DMG Noise Drum", "Polynomial-noise percussion setup.", { 0.40f, 0.15f, 0.80f, 0.45f }, { false, false, false, true }, 0.88f, 0, 0, 0, 1 },
        { MacroKind::hit, "DMG Hit", "Short handheld impact.", { 0.45f, 0.25f, 0.70f, 0.65f }, { true, false, false, true }, 0.60f, 0, 0, 0, 1 },
        { MacroKind::laser, "DMG Laser", "Sweep-heavy SFX tone with narrow-noise edge.", { 0.25f, 1.00f, 0.35f, 0.95f }, { true, true, false, true }, 0.40f, 0, 0, 0, 4 },
        { MacroKind::jump, "DMG Jump", "Quick rising pocket-game blip.", { 0.25f, 0.70f, 0.05f, 0.80f }, { true, false, false, false }, 0.30f, 0, 0, 0, 1 },
        { MacroKind::powerUp, "DMG Power-Up", "Longer rising handheld stack.", { 0.70f, 0.90f, 0.20f, 0.90f }, { true, true, true, false }, 0.0f, 2, 0, 0, 4 },
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
        { MacroKind::manual, "PSG Manual", "Neutral SN76489 PSG tone/noise mapping.", { 0.5f, 0.5f, 0.5f, 0.5f }, { true, true, true, true } },
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

std::vector<MacroTemplate> sidMacros()
{
    return {
        { MacroKind::manual, "SID Manual", "Neutral three-voice SID oscillator mapping.", { 0.50f, 0.25f, 0.70f, 0.70f }, { true, true, true, false }, 0.35f, 0, 1, 1, 0, 0.25f },
        { MacroKind::coin, "SID Coin", "Short gated pulse pop with a bright C64 edge.", { 0.35f, 0.80f, 0.90f, 0.45f }, { true, false, false, false }, 0.70f, 3, 3, 1, 0, 0.15f },
        { MacroKind::bass, "SID Dirty Bass", "Pulse-heavy dual voice bass with a low-pass-style cutoff macro.", { 0.48f, 0.18f, 0.40f, 0.82f }, { true, true, false, false }, 0.28f, 3, 1, 1, 0, 0.58f },
        { MacroKind::lead, "SID PWM Lead", "Three-voice pulse stack ready for animated width and detune.", { 0.58f, 0.40f, 0.72f, 0.70f }, { true, true, true, false }, 0.32f, 3, 1, 1, 0, 0.36f },
        { MacroKind::arp, "SID Robot Arp", "Saw stack for hard-edged robotic arps.", { 0.50f, 0.78f, 0.85f, 0.60f }, { true, true, true, false }, 0.25f, 2, 2, 1, 0, 0.42f },
        { MacroKind::drum, "SID Noise Drum", "Noise voice with fast hardware-style decay.", { 0.50f, 0.10f, 0.80f, 0.20f }, { false, false, true, false }, 0.95f, 4, 1, 1, 0, 0.18f },
        { MacroKind::hit, "SID Grit Hit", "Short noisy impact with a pulse transient.", { 0.42f, 0.30f, 0.65f, 0.28f }, { true, false, true, false }, 0.88f, 4, 2, 3, 0, 0.50f },
        { MacroKind::laser, "SID Sync Sweep", "Pitch-spread saw/pulse sweep gesture.", { 0.30f, 1.00f, 0.92f, 0.45f }, { true, true, false, false }, 0.55f, 2, 2, 2, 0, 0.65f },
        { MacroKind::jump, "SID Jump", "Quick triangle/pulse blip.", { 0.38f, 0.62f, 0.78f, 0.42f }, { true, false, false, false }, 0.50f, 1, 3, 1, 0, 0.20f },
        { MacroKind::powerUp, "SID Filter Rise", "Bright stacked rise with cutoff opened by the macro.", { 0.62f, 0.88f, 0.95f, 0.65f }, { true, true, true, false }, 0.22f, 2, 1, 2, 0, 0.48f },
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

ChipParameterSpec sourceLevelSpec(ChipParameterRole role, std::string id, std::string label, std::string help)
{
    return { role, id, label, "Sources", help, ParameterKind::continuous, ControlSurface::slider, {}, 0.0f, 1.0f, 1.0f };
}

ChipParameterSpec sidPulseWidthSpec(ChipParameterRole role, std::string id, std::string label, std::string help)
{
    return { role, id, label, "Voices", help, ParameterKind::chipRegister, ControlSurface::slider, {}, 0.0f, 1.0f, 0.5f };
}

ChipParameterSpec stereoSpreadSpec(std::string id, std::string help)
{
    return { ChipParameterRole::stereoSpread, id, "Stereo Spread", "Output", help, ParameterKind::continuous, ControlSurface::slider, {}, 0.0f, 1.0f, 0.0f };
}

AccuracyDisclosure verifiedPartial(std::initializer_list<std::string> verifiedBehaviors,
                                   std::initializer_list<std::string> knownGaps)
{
    return {
        "Verified partial",
        "Automated renderer regression coverage backs this partial clean-room register-level model.",
        "CMake renderer tests assert descriptor metadata, register-derived debug JSON, source gating, preset snapshots, and MIDI CC mappings for the implemented behavior.",
        std::vector<std::string>(verifiedBehaviors),
        std::vector<std::string>(knownGaps),
        false,
        false
    };
}

ChipParameterSpec envelopeSpec(std::string id, std::string label, std::string help)
{
    return { ChipParameterRole::envelopeDecay, id, label, "Envelope", help, ParameterKind::chipRegister, ControlSurface::slider, {}, 0.0f, 1.0f, 0.0f };
}

std::vector<ParameterChoiceSpec> sidWaveformChoices()
{
    return {
        choice("Follow", "Resolve this voice from the selected SID macro or Voice 1 waveform.", 0.0f, 0),
        choice("Tri", "Control bit 0x10: triangle waveform.", 0.125f, 1),
        choice("Saw", "Control bit 0x20: sawtooth waveform.", 0.25f, 2),
        choice("Pulse", "Control bit 0x40: pulse waveform using the Pulse Width control.", 0.375f, 3),
        choice("Noise", "Control bit 0x80: SID noise waveform.", 0.5f, 4),
        choice("Tri+Saw", "Control bits 0x30: triangle and sawtooth selected together; output is a musical approximation of SID combined waveform behavior.", 0.625f, 5),
        choice("Tri+Pulse", "Control bits 0x50: triangle and pulse selected together; output is a musical approximation of SID combined waveform behavior.", 0.75f, 6),
        choice("Saw+Pulse", "Control bits 0x60: sawtooth and pulse selected together; output is a musical approximation of SID combined waveform behavior.", 0.875f, 7),
        choice("Tri+Saw+Pulse", "Control bits 0x70: triangle, sawtooth, and pulse selected together; output is a musical approximation of SID combined waveform behavior.", 1.0f, 8)
    };
}

ChipParameterSpec sidVoiceWaveSpec(ChipParameterRole role, std::string id, std::string label, std::string help)
{
    return { role, id, label, "Voices", help, ParameterKind::chipRegister, ControlSurface::menu, sidWaveformChoices(), 0.0f, 1.0f, 0.0f };
}

std::vector<ParameterChoiceSpec> sidAdsrNibbleChoices(std::string fieldName)
{
    std::vector<ParameterChoiceSpec> choices;
    choices.reserve(17);
    choices.push_back(choice("Follow", "Resolve " + fieldName + " from ADSR Speed and the selected SID macro.", 0.0f, 0));
    for (int nibble = 0; nibble <= 15; ++nibble)
    {
        const auto normalized = static_cast<float>(nibble + 1) / 16.0f;
        choices.push_back(choice(std::to_string(nibble),
                                 "Use SID " + fieldName + " nibble " + std::to_string(nibble) + " directly.",
                                 normalized,
                                 nibble + 1));
    }

    return choices;
}

ChipParameterSpec sidAdsrNibbleSpec(ChipParameterRole role, std::string id, std::string label, std::string help)
{
    return { role, id, label, "Envelope", help, ParameterKind::chipRegister, ControlSurface::steppedSlider, sidAdsrNibbleChoices(label), 0.0f, 1.0f, 0.0f };
}

std::vector<ParameterChoiceSpec> ymChannelMixChoices(std::string channelName)
{
    return {
        choice("Follow", "Follow the global Tone/Noise Mix for YM/AY channel " + channelName + ".", 0.0f, 0),
        choice("Tone", "Enable tone and disable shared noise for YM/AY channel " + channelName + ".", 0.25f, 1),
        choice("Noise", "Disable tone and enable shared noise for YM/AY channel " + channelName + ".", 0.5f, 2),
        choice("Both", "Enable tone and shared noise together for YM/AY channel " + channelName + ".", 0.75f, 3),
        choice("Off", "Disable both tone and shared noise for YM/AY channel " + channelName + ".", 1.0f, 4)
    };
}

ChipParameterSpec ymChannelMixSpec(ChipParameterRole role, std::string id, std::string label, std::string channelName)
{
    return { role,
             id,
             label,
             "Mixer",
             "Overrides YM/AY register 7 tone/noise enable bits for channel " + channelName + ". Follow uses the global Tone/Noise Mix.",
             ParameterKind::chipRegister,
             ControlSurface::menu,
             ymChannelMixChoices(channelName),
             0.0f,
             1.0f,
             0.0f };
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

std::vector<ParameterChoiceSpec> pulse2DutyChoices(std::string macroHelp)
{
    return {
        choice("Follow", macroHelp, 0.0f, 0),
        choice("12.5%", "Write duty bits 00 to pulse channel 2.", 0.25f, 1),
        choice("25%", "Write duty bits 01 to pulse channel 2.", 0.5f, 2),
        choice("50%", "Write duty bits 10 to pulse channel 2.", 0.75f, 3),
        choice("75%", "Write duty bits 11 to pulse channel 2.", 1.0f, 4)
    };
}

std::vector<ChipParameterSpec> nesParameterSpecs()
{
    return {
        segmentedSpec(ChipParameterRole::macroControl1,
                      "nes.pulse1Duty",
                      "Pulse 1 Duty",
                      "Channels",
                      "Maps to the RP2A03 pulse 1 duty register field.",
                      pulseDutyChoices("Thin 1/8 pulse.", "Narrow 1/4 pulse.", "Square 1/2 pulse.", "Wide 3/4 inverted pulse."),
                      ParameterKind::chipRegister,
                      2.0f / 3.0f),
        segmentedSpec(ChipParameterRole::pulse2Duty,
                      "nes.pulse2Duty",
                      "Pulse 2 Duty",
                      "Channels",
                      "Overrides the RP2A03 pulse 2 duty register field. Follow uses the selected NES template.",
                      pulse2DutyChoices("Keep the current musical template: stacked Big Mono offsets Pulse 2 by one duty step; Chip Poly follows Pulse 1."),
                      ParameterKind::chipRegister),
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
        sourceLevelSpec(ChipParameterRole::source1Level, "nes.pulse1.level", "Pulse 1 Level", "Modern trim for the first pulse source."),
        sourceLevelSpec(ChipParameterRole::source2Level, "nes.pulse2.level", "Pulse 2 Level", "Modern trim for the second pulse source."),
        sourceLevelSpec(ChipParameterRole::source3Level, "nes.triangle.level", "Triangle Level", "Modern trim for the triangle source."),
        sourceLevelSpec(ChipParameterRole::source4Level, "nes.noise.level", "Noise Level", "Modern trim for the noise source."),
        sliderSpec(ChipParameterRole::nesDmcDirectLevel,
                   "nes.dmcDirectLevel",
                   "DMC Direct Level",
                   "DMC",
                   "Maps to the RP2A03 $4011 7-bit DMC direct DAC load. This is direct-level grit, not full DPCM sample playback.",
                   ParameterKind::chipRegister),
        envelopeSpec("nes.envelopeDecay", "Envelope Decay", "Maps musical decay to APU envelope period values."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "nes.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to RP2A03 $400E bit 7: long LFSR noise or short-loop metallic noise. Follow resolves from the selected musical template.",
                      {
                          choice("Follow", "Use the selected NES macro to choose long or short noise.", 0.0f, 0),
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
        sourceLevelSpec(ChipParameterRole::source1Level, "dmg.pulse1.level", "Pulse 1 Level", "Modern trim for DMG pulse channel 1."),
        sourceLevelSpec(ChipParameterRole::source2Level, "dmg.pulse2.level", "Pulse 2 Level", "Modern trim for DMG pulse channel 2."),
        sourceLevelSpec(ChipParameterRole::source3Level, "dmg.wave.level", "Wave Level", "Modern trim for the Wave RAM channel."),
        sourceLevelSpec(ChipParameterRole::source4Level, "dmg.noise.level", "Noise Level", "Modern trim for the noise channel."),
        envelopeSpec("dmg.envelopeDecay", "Envelope Decay", "Maps musical decay to DMG 64 Hz hardware envelope periods."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "dmg.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to NR43 bit 3: 15-bit LFSR noise or 7-bit metallic short noise. Follow resolves from the selected DMG template.",
                      {
                          choice("Follow", "Use the selected DMG macro to choose the noise width.", 0.0f, 0),
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
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::dmgWaveLevel,
                      "dmg.waveLevel",
                      "Wave Level",
                      "Wave RAM",
                      "Maps to DMG NR32 channel-3 output level bits. Follow preserves velocity/macro defaults.",
                      {
                          choice("Follow", "Use the selected DMG macro or Chip Poly velocity to choose NR32 output level.", 0.0f, 0),
                          choice("Mute", "NR32 bits 00: wave DAC on but channel-3 output muted.", 0.25f, 1),
                          choice("100%", "NR32 bits 01: full wave output level.", 0.5f, 2),
                          choice("50%", "NR32 bits 10: half wave output level.", 0.75f, 3),
                          choice("25%", "NR32 bits 11: quarter wave output level.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::dmgStereoRoute,
                      "dmg.stereoRoute",
                      "Stereo Route",
                      "Output",
                      "Maps to DMG NR51 output-routing bits for each hardware channel. Follow uses the selected DMG template.",
                      {
                          choice("Follow", "Use the selected DMG macro's NR51 routing choice.", 0.0f, 0),
                          choice("Both", "NR51 = 0xFF: route all channels to left and right.", 0.25f, 1),
                          choice("Left", "NR51 = 0xF0: route all channels to left only.", 0.5f, 2),
                          choice("Right", "NR51 = 0x0F: route all channels to right only.", 0.75f, 3),
                          choice("Split", "NR51 = 0x5A: pulse 1 and wave left; pulse 2 and noise right.", 1.0f, 4)
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
        sourceLevelSpec(ChipParameterRole::source1Level, "ym2149.channelA.level", "Channel A Level", "Trims YM/AY channel A after its native volume register."),
        sourceLevelSpec(ChipParameterRole::source2Level, "ym2149.channelB.level", "Channel B Level", "Trims YM/AY channel B after its native volume register."),
        sourceLevelSpec(ChipParameterRole::source3Level, "ym2149.channelC.level", "Channel C Level", "Trims YM/AY channel C after its native volume register."),
        sourceLevelSpec(ChipParameterRole::source4Level, "ym2149.noise.level", "Noise Level", "Scales the shared noise gate where the mixer uses noise."),
        ymChannelMixSpec(ChipParameterRole::ymChannelAMix, "ym2149.channelA.mix", "A Mix", "A"),
        ymChannelMixSpec(ChipParameterRole::ymChannelBMix, "ym2149.channelB.mix", "B Mix", "B"),
        ymChannelMixSpec(ChipParameterRole::ymChannelCMix, "ym2149.channelC.mix", "C Mix", "C"),
        stereoSpreadSpec("ym2149.stereoSpread", "Modern stereo convenience that spreads A/B/C across the stereo field; zero preserves mono chip output."),
        envelopeSpec("ym2149.envelopeSpeed", "Envelope Speed", "Maps musical envelope speed to YM/AY registers 11 and 12. Zero uses the default register period."),
        { ChipParameterRole::ymEnvelopeShape,
          "ym2149.envelopeShape",
          "Envelope Shape",
          "Envelope",
          "Maps directly to the YM/AY hardware envelope shape register.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          {
              choice("Fixed", "Use fixed volume registers; envelope bit off.", 0.0f, 0),
              choice("Fall", "Register 13 = 0x09, fall then hold low.", 0.25f, 1),
              choice("Rise", "Register 13 = 0x0D, rise then hold high.", 0.5f, 2),
              choice("Saw", "Register 13 = 0x08, repeating saw down.", 0.75f, 3),
              choice("Tri", "Register 13 = 0x0E, repeating triangle.", 1.0f, 4),
              choice("0x00", "Write exact envelope shape code 0x00.", 0.0f, 5),
              choice("0x01", "Write exact envelope shape code 0x01.", 0.0f, 6),
              choice("0x02", "Write exact envelope shape code 0x02.", 0.0f, 7),
              choice("0x03", "Write exact envelope shape code 0x03.", 0.0f, 8),
              choice("0x04", "Write exact envelope shape code 0x04.", 0.0f, 9),
              choice("0x05", "Write exact envelope shape code 0x05.", 0.0f, 10),
              choice("0x06", "Write exact envelope shape code 0x06.", 0.0f, 11),
              choice("0x07", "Write exact envelope shape code 0x07.", 0.0f, 12),
              choice("0x08", "Write exact envelope shape code 0x08.", 0.0f, 13),
              choice("0x09", "Write exact envelope shape code 0x09.", 0.0f, 14),
              choice("0x0A", "Write exact envelope shape code 0x0A.", 0.0f, 15),
              choice("0x0B", "Write exact envelope shape code 0x0B.", 0.0f, 16),
              choice("0x0C", "Write exact envelope shape code 0x0C.", 0.0f, 17),
              choice("0x0D", "Write exact envelope shape code 0x0D.", 0.0f, 18),
              choice("0x0E", "Write exact envelope shape code 0x0E.", 0.0f, 19),
              choice("0x0F", "Write exact envelope shape code 0x0F.", 0.0f, 20)
          },
          0.0f,
          1.0f,
          0.0f }
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
                   "Biases the macro-selected SN76489 noise-control register bits when Noise Mode is Follow; explicit Noise Mode choices override it.",
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
        sourceLevelSpec(ChipParameterRole::source1Level, "sn76489.tone1.level", "Tone 1 Level", "Trims SN76489 tone channel 1 after attenuation."),
        sourceLevelSpec(ChipParameterRole::source2Level, "sn76489.tone2.level", "Tone 2 Level", "Trims SN76489 tone channel 2 after attenuation."),
        sourceLevelSpec(ChipParameterRole::source3Level, "sn76489.tone3.level", "Tone 3 Level", "Trims SN76489 tone channel 3 after attenuation."),
        sourceLevelSpec(ChipParameterRole::source4Level, "sn76489.noise.level", "Noise Level", "Trims the SN76489 noise channel after attenuation."),
        stereoSpreadSpec("sn76489.stereoSpread", "Modern stereo convenience that spreads tone channels across the stereo field; zero preserves mono PSG output."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "sn76489.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to the SN76489 noise-control register. Follow resolves from the current musical template.",
                      {
                          choice("Follow", "Use the chip-specific macro to choose the noise register bits.", 0.0f, 0),
                          choice("P-Lo", "Periodic noise, low rate.", 0.25f, 1),
                          choice("P-Hi", "Periodic noise, high rate.", 0.5f, 2),
                          choice("W-Lo", "White noise, low rate.", 0.75f, 3),
                          choice("W-T3", "White noise clocked from tone 3.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> sidParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "sid.pulseWidth",
                   "Pulse Width",
                   "Voices",
                   "Maps to the SID 12-bit pulse-width registers for pulse waveform voices.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl2,
                   "sid.voiceDetune",
                   "Voice Detune",
                   "Pitch",
                   "Offsets voices 2 and 3 in semitone steps for stacked SID patches."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "sid.filterCutoff",
                   "Cutoff",
                   "Filter",
                   "Writes SID filter cutoff registers and drives Chipper's first-pass multimode filter approximation. Exact 6581/8580 analog filter behavior is not yet verified.",
                   ParameterKind::chipRegister,
                   0.75f),
        { ChipParameterRole::stereoSpread,
          "sid.filterResonance",
          "Resonance",
          "Filter",
          "Maps to the SID filter resonance nibble in $D417. This is a register-backed control, not modern stereo spread in SID mode.",
          ParameterKind::chipRegister,
          ControlSurface::slider,
          {},
          0.0f,
          1.0f,
          0.25f },
        { ChipParameterRole::sidFilterRouting,
          "sid.filterRouting",
          "Filter Routing",
          "Filter",
          "Maps to the SID $D417 voice-routing bits. Follow uses the enabled SID voices; explicit choices write the filter input bits directly.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          {
              choice("Follow", "Route the currently enabled SID voices through the filter.", 0.0f, 0),
              choice("All", "$D417 bits 0-2 set: voices 1, 2, and 3 enter the filter.", 0.125f, 1),
              choice("V1", "$D417 bit 0: route voice 1 only.", 0.25f, 2),
              choice("V2", "$D417 bit 1: route voice 2 only.", 0.375f, 3),
              choice("V3", "$D417 bit 2: route voice 3 only.", 0.5f, 4),
              choice("V1+V2", "$D417 bits 0 and 1.", 0.625f, 5),
              choice("V1+V3", "$D417 bits 0 and 2.", 0.75f, 6),
              choice("V2+V3", "$D417 bits 1 and 2.", 0.875f, 7),
              choice("None", "$D417 routing bits cleared; voices bypass the filter.", 1.0f, 8)
          },
          0.0f,
          1.0f,
          0.0f },
        sliderSpec(ChipParameterRole::macroControl4,
                   "sid.sustain",
                   "Sustain",
                   "Envelope",
                   "Maps to the SID sustain nibble in each voice's SR register.",
                   ParameterKind::chipRegister,
                   0.7f),
        sourceSpec(ChipParameterRole::source1Enabled, "sid.voice1.enabled", "Voice 1", "Enable SID voice 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "sid.voice2.enabled", "Voice 2", "Enable SID voice 2."),
        sourceSpec(ChipParameterRole::source3Enabled,
                   "sid.voice3.enabled",
                   "Voice 3",
                   "Enable SID voice 3 audio output. OSC3/ENV3 utility-read and silent modulation-source behavior are planned separately."),
        sourceLevelSpec(ChipParameterRole::source1Level, "sid.voice1.level", "Voice 1 Level", "Modern trim for SID voice 1 after its envelope."),
        sourceLevelSpec(ChipParameterRole::source2Level, "sid.voice2.level", "Voice 2 Level", "Modern trim for SID voice 2 after its envelope."),
        sourceLevelSpec(ChipParameterRole::source3Level,
                        "sid.voice3.level",
                        "Voice 3 Level",
                        "Modern trim for audible SID voice 3 after its envelope; it does not model OSC3/ENV3 utility routing yet."),
        sidPulseWidthSpec(ChipParameterRole::sidVoice2PulseWidth,
                          "sid.voice2.pulseWidth",
                          "Voice 2 PW",
                          "Writes SID voice 2's independent 12-bit pulse-width register pair at $D409/$D40A."),
        sidPulseWidthSpec(ChipParameterRole::sidVoice3PulseWidth,
                          "sid.voice3.pulseWidth",
                          "Voice 3 PW",
                          "Writes SID voice 3's independent 12-bit pulse-width register pair at $D410/$D411."),
        segmentedSpec(ChipParameterRole::dmgStereoRoute,
                      "sid.model",
                      "SID Model",
                      "Profile",
                      "Selects the partial SID chip-variant profile used by Chipper's filter curve and output drive model. Auto resolves from the selected SID template.",
                      {
                          choice("Auto", "Resolve 6581/8580 from the selected SID template.", 0.0f, 0),
                          choice("6581", "Warmer, rougher 6581-style filter/output profile.", 0.25f, 1),
                          choice("8580", "Cleaner, brighter 8580-style filter/output profile.", 0.5f, 2)
                      },
                      ParameterKind::chipRegister),
        envelopeSpec("sid.adsrSpeed", "ADSR Speed", "Maps musical envelope speed to SID attack/decay/release nibbles while Sustain uses the dedicated control."),
        sidAdsrNibbleSpec(ChipParameterRole::sidAttack,
                          "sid.attack",
                          "V1 Attack",
                          "Overrides SID voice 1 attack nibble in its AD register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidDecay,
                          "sid.decay",
                          "V1 Decay",
                          "Overrides SID voice 1 decay nibble in its AD register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidSustain,
                          "sid.sustainOverride",
                          "V1 Sustain",
                          "Overrides SID voice 1 sustain nibble in its SR register. Follow uses the Sustain control."),
        sidAdsrNibbleSpec(ChipParameterRole::sidRelease,
                          "sid.release",
                          "V1 Release",
                          "Overrides SID voice 1 release nibble in its SR register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice2Attack,
                          "sid.voice2.attack",
                          "V2 Attack",
                          "Overrides SID voice 2 attack nibble in its AD register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice2Decay,
                          "sid.voice2.decay",
                          "V2 Decay",
                          "Overrides SID voice 2 decay nibble in its AD register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice2Sustain,
                          "sid.voice2.sustain",
                          "V2 Sustain",
                          "Overrides SID voice 2 sustain nibble in its SR register. Follow uses the Sustain control."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice2Release,
                          "sid.voice2.release",
                          "V2 Release",
                          "Overrides SID voice 2 release nibble in its SR register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice3Attack,
                          "sid.voice3.attack",
                          "V3 Attack",
                          "Overrides SID voice 3 attack nibble in its AD register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice3Decay,
                          "sid.voice3.decay",
                          "V3 Decay",
                          "Overrides SID voice 3 decay nibble in its AD register. Follow uses ADSR Speed."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice3Sustain,
                          "sid.voice3.sustain",
                          "V3 Sustain",
                          "Overrides SID voice 3 sustain nibble in its SR register. Follow uses the Sustain control."),
        sidAdsrNibbleSpec(ChipParameterRole::sidVoice3Release,
                          "sid.voice3.release",
                          "V3 Release",
                          "Overrides SID voice 3 release nibble in its SR register. Follow uses ADSR Speed."),
        segmentedSpec(ChipParameterRole::waveShape,
                      "sid.waveform",
                      "Voice 1 Wave",
                      "Voices",
                      "Maps Voice 1 to SID control-register waveform bits. Voice 2 and Voice 3 can follow the template or choose their own waveform.",
                      sidWaveformChoices(),
                      ParameterKind::chipRegister),
        sidVoiceWaveSpec(ChipParameterRole::sidVoice2WaveShape,
                         "sid.voice2.waveform",
                         "Voice 2 Wave",
                         "Maps Voice 2 to its own SID control-register waveform bits. Follow uses Voice 1 or the selected SID template."),
        sidVoiceWaveSpec(ChipParameterRole::sidVoice3WaveShape,
                         "sid.voice3.waveform",
                         "Voice 3 Wave",
                         "Maps audible Voice 3 to its own SID control-register waveform bits. Follow uses Voice 1 or the selected SID template; OSC3 readback is not modeled yet."),
        { ChipParameterRole::ymEnvelopeShape,
          "sid.filterMode",
          "Filter Mode",
          "Filter",
          "Maps to SID $D418 filter-mode bits, including combined mode-bit outputs. Follow uses the selected SID musical template.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          {
              choice("Follow", "Resolve LP/BP/HP from the selected SID macro.", 0.0f, 0),
              choice("LP", "$D418 bit 0x10: low-pass filter output.", 0.125f, 1),
              choice("BP", "$D418 bit 0x20: band-pass filter output.", 0.25f, 2),
              choice("HP", "$D418 bit 0x40: high-pass filter output.", 0.375f, 3),
              choice("Off", "$D418 filter bits cleared; bypass Chipper's SID filter approximation.", 0.5f, 4),
              choice("Notch", "$D418 bits 0x50: combined low-pass and high-pass output.", 0.625f, 5),
              choice("LP+BP", "$D418 bits 0x30: combined low-pass and band-pass output.", 0.75f, 6),
              choice("BP+HP", "$D418 bits 0x60: combined band-pass and high-pass output.", 0.875f, 7),
              choice("All", "$D418 bits 0x70: low-pass, band-pass, and high-pass outputs summed.", 1.0f, 8)
          },
          0.0f,
          1.0f,
          0.0f },
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "sid.oscMod",
                      "Osc Interaction",
                      "Motion",
                      "Maps to SID control-register sync/ring bits on voices 2 and 3. Follow uses the selected SID musical template.",
                      {
                          choice("Follow", "Resolve oscillator modulation from the selected SID macro.", 0.0f, 0),
                          choice("Off", "Clear SID sync and ring bits.", 0.25f, 1),
                          choice("Sync", "Set the SID hard-sync bit on stacked follower voices.", 0.5f, 2),
                          choice("Ring", "Set the SID ring-mod bit on stacked follower voices.", 0.75f, 3),
                          choice("Both", "Set both SID sync and ring bits on stacked follower voices.", 1.0f, 4)
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
        makeModule("sources", "Channels", "Native channel layout exposed musically.", { "Pulse 1", "Pulse 2", "Triangle / Chip Poly", "Noise + DMC direct" }),
        makeModule("tone", "Shape / Mixer", "Pulse, triangle, noise, and nonlinear mixer behavior.", { "Pulse duty", "Pitch sweep macro", "Noise mode", "Nonlinear mixer" }),
        makeModule("envelope", "Envelope", "APU envelope and duration behavior.", { "Simple envelope", "Length counters", "Triangle linear planned", "Drum decay" }),
        makeModule("motion", "Motion", "Musical gestures write chip-like register templates.", { "Coin blip", "Jump rise", "Laser sweep", "Fast arps" }),
        makeModule("output", "Output", "Bright direct mono chip output.", { "Output gain", "Dry mono", "Bit/sample grit", "SFX templates" })
    };
}

std::array<ModuleDescriptor, 6> dmgModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "DMG APU clean-room register model.", { "DMG profile", "CGB quirks planned", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Four hardware sound generators.", { "Pulse 1 / sweep", "Pulse 2", "Wave RAM", "Chip Poly ready" }),
        makeModule("tone", "Wave / Noise", "Duty, wave RAM, and polynomial noise behavior.", { "Pulse duty", "Wave shape", "Wave level", "Noise clock" }),
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
        makeModule("profile", "Profile", "SID clean-room voice-core groundwork.", { "6581 / 8580 model", "PAL clock default", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Voices", "Three SID oscillator voices.", { "Voice 1", "Voice 2", "Audible Voice 3", "OSC3/ENV3 utility planned" }),
        makeModule("tone", "Filter", "Register-backed SID filter mode and voice routing.", { "Filter mode", "Voice routing", "Cutoff", "Resonance" }),
        makeModule("envelope", "Envelope", "SID-style ADSR gate behavior.", { "Attack/decay nibbles", "Sustain nibble", "Release nibble", "ADSR quirks planned" }),
        makeModule("motion", "Motion", "Classic SID modulation gestures.", { "Voice detune", "PWM-ready width", "Osc interaction", "Template motion" }),
        makeModule("output", "Output", "Warm mono C64-style output groundwork.", { "Output gain", "Voice trims", "Drive planned", "Known differences" })
    };
}

std::array<ModuleDescriptor, 6> fmModules(std::string profile, std::string sourceTitle, std::string toneTitle)
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", profile, { "Chip variant", "Clock profile", "Hybrid default", "Core audit required" }),
        makeModule("sources", sourceTitle, "FM voice selection and routing.", { "Voice select", "Algorithm", "Feedback", "Voice level" }),
        makeModule("tone", toneTitle, "Operator-level tone shaping.", { "Operator ratios", "Operator levels", "Feedback", "Waveforms where native" }),
        makeModule("envelope", "Envelope", "Native operator envelope controls.", { "Attack", "Decay", "Sustain", "Release" }),
        makeModule("motion", "Motion", "FM modulation and performance helpers.", { "LFO", "Pitch mod", "Arp/glide", "Template morph" }),
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
            nesParameterSpecs(),
            verifiedPartial(
                {
                    "Pulse duty choices, pulse-2 duty override, sweep add/negate/mute paths, triangle linear counter behavior, noise mode/period controls, DMC direct level via event traces and APVTS/CC parameter mapping, frame-counter register paths, source gating, Chip Poly, presets, and MIDI CC metadata are covered by renderer tests."
                },
                {
                    "DMC sample playback is not complete.",
                    "Exact frame-sequencer cycle timing, hardware capture comparison, and cycle accuracy are not claimed."
                })
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
            dmgParameterSpecs(),
            verifiedPartial(
                {
                    "Pulse duty, sweep shift/overflow, wave RAM shape/level, envelope level/decay, noise width/clock edge cases, NR52 status bits, NR51 stereo routing, source gating, Chip Poly, presets, and MIDI CC metadata are covered by renderer tests."
                },
                {
                    "Exact DMG power-up behavior, all length-counter edge cases, hardware capture comparison, and cycle accuracy are not claimed.",
                    "CGB-specific differences remain planned."
                })
        },
        {
            ChipMode::sid,
            "SID / C64",
            "Three SID-style oscillator voices map to a partial clean-room register model with waveform, pulse-width, ADSR, and multimode filter groundwork.",
            {
                { "pulseWidth", "Pulse Width", "Voices", "Maps to the SID pulse-width registers." },
                { "detune", "Voice Detune", "Pitch", "Offsets the three SID voices for stacked patches." },
                { "cutoff", "Cutoff", "Filter", "Writes SID cutoff registers and drives the current partial multimode filter model." },
                { "sustain", "Sustain", "Envelope", "Maps to the SID sustain nibble." },
            },
            sidModules(),
            sidMacros(),
            true,
            true,
            sidParameterSpecs(),
            verifiedPartial(
                {
                    "Voice waveform bits, per-voice waveform overrides, pulse-width register mapping, source gating, Chip Poly, sync/ring control bits, ADSR nibble overrides, filter mode/routing/resonance, 6581/8580 profile selection, presets, and MIDI CC metadata are covered by renderer tests."
                },
                {
                    "OSC3/ENV3 voice-3 readback, silent Voice 3 utility/mod-source behavior, model-specific Voice 3 noise leakage, exact 6581/8580 analog filter behavior, ADSR bugs, oscillator sync/ring timing, waveform-combination quirks, external input, DAC nonlinearity, hardware capture comparison, and cycle accuracy are not claimed."
                })
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
            ym2149ParameterSpecs(),
            verifiedPartial(
                {
                    "Three tone channels, shared noise period, register-7 tone/noise mixer bits, per-channel mixer overrides, envelope shape/speed/reset behavior, volume curve, source gating, Chip Poly, presets, and MIDI CC metadata are covered by renderer tests."
                },
                {
                    "Chip-variant analog output differences, all envelope edge cases, hardware capture comparison, and cycle accuracy are not claimed."
                })
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
            sn76489ParameterSpecs(),
            verifiedPartial(
                {
                    "Latch/register write behavior, tone period 0/1 edge cases, volume data writes, noise modes including Tone-3-clocked noise, noise attenuation, source gating, Chip Poly, presets, and MIDI CC metadata are covered by renderer tests."
                },
                {
                    "Console-specific output coloration, hardware capture comparison, and cycle accuracy are not claimed."
                })
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
                            std::array<float, 4> sourceLevels,
                            float stereoSpread,
                            float envelopeDecay,
                            int waveShape,
                            int pulse2Duty,
                            int dmgWaveLevel,
                            int dmgStereoRoute,
                            int ymEnvelopeShape,
                            int ymChannelAMix,
                            int ymChannelBMix,
                            int ymChannelCMix,
                            int snNoiseMode,
                            int sidVoice2WaveShape,
                            int sidVoice3WaveShape,
                            int sidAttack,
                            int sidDecay,
                            int sidSustain,
                            int sidRelease,
                            int sidVoice2Attack,
                            int sidVoice2Decay,
                            int sidVoice2Sustain,
                            int sidVoice2Release,
                            int sidVoice3Attack,
                            int sidVoice3Decay,
                            int sidVoice3Sustain,
                            int sidVoice3Release,
                            int sidFilterRouting,
                            float sidVoice2PulseWidth,
                            float sidVoice3PulseWidth,
                            float nesDmcDirectLevel)
{
    const auto effectivePlayMode = supportsPlayMode(mode, playMode) ? playMode : PlayMode::stack;
    const auto maxYmEnvelopeShape = mode == ChipMode::sid ? 8 : 20;
    const auto maxWaveShape = mode == ChipMode::sid ? 8 : 4;

    return {
        macro,
        clampControl(control1),
        clampControl(control2),
        clampControl(control3),
        clampControl(control4),
        effectivePlayMode,
        sourceEnabled,
        {
            clampControl(sourceLevels[0]),
            clampControl(sourceLevels[1]),
            clampControl(sourceLevels[2]),
            clampControl(sourceLevels[3])
        },
        clampControl(stereoSpread),
        std::clamp(sidFilterRouting, 0, 8),
        clampControl(envelopeDecay),
        std::clamp(sidAttack, 0, 16),
        std::clamp(sidDecay, 0, 16),
        std::clamp(sidSustain, 0, 16),
        std::clamp(sidRelease, 0, 16),
        std::clamp(sidVoice2Attack, 0, 16),
        std::clamp(sidVoice2Decay, 0, 16),
        std::clamp(sidVoice2Sustain, 0, 16),
        std::clamp(sidVoice2Release, 0, 16),
        std::clamp(sidVoice3Attack, 0, 16),
        std::clamp(sidVoice3Decay, 0, 16),
        std::clamp(sidVoice3Sustain, 0, 16),
        std::clamp(sidVoice3Release, 0, 16),
        std::clamp(waveShape, 0, maxWaveShape),
        std::clamp(pulse2Duty, 0, 4),
        std::clamp(dmgWaveLevel, 0, 4),
        std::clamp(dmgStereoRoute, 0, 4),
        std::clamp(ymEnvelopeShape, 0, maxYmEnvelopeShape),
        std::clamp(ymChannelAMix, 0, 4),
        std::clamp(ymChannelBMix, 0, 4),
        std::clamp(ymChannelCMix, 0, 4),
        std::clamp(snNoiseMode, 0, 4),
        std::clamp(sidVoice2WaveShape, 0, 8),
        std::clamp(sidVoice3WaveShape, 0, 8),
        clampControl(sidVoice2PulseWidth),
        clampControl(sidVoice3PulseWidth),
        clampControl(nesDmcDirectLevel)
    };
}

uint8_t nesNoisePeriodForControl(float noisePeriodControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - clampControl(noisePeriodControl)) * 15.0f)), 0, 15));
}

uint8_t nesDmcDirectLevelForControl(float levelControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(levelControl) * 127.0f)), 0, 127));
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

uint8_t dmgWaveOutputLevelBitsForPatch(const PatchConfig& patch, float velocity, bool velocitySensitive)
{
    switch (std::clamp(patch.dmgWaveLevel, 0, 4))
    {
        case 1: return 0x00u;
        case 2: return 0x20u;
        case 3: return 0x40u;
        case 4: return 0x60u;
        case 0:
        default:
            break;
    }

    if (velocitySensitive)
    {
        const auto normalizedVelocity = clampControl(velocity);
        if (normalizedVelocity <= 0.0f)
            return 0x00u;
        if (normalizedVelocity >= 0.75f)
            return 0x20u;
        if (normalizedVelocity >= 0.35f)
            return 0x40u;
        return 0x60u;
    }

    return patch.macro == MacroKind::bass ? 0x20u : 0x40u;
}

uint8_t dmgStereoRouteRegisterForPatch(const PatchConfig& patch)
{
    switch (std::clamp(patch.dmgStereoRoute, 0, 4))
    {
        case 1: return 0xffu;
        case 2: return 0xf0u;
        case 3: return 0x0fu;
        case 4: return 0x5au;
        case 0:
        default:
            break;
    }

    switch (patch.macro)
    {
        case MacroKind::lead:
        case MacroKind::arp:
        case MacroKind::laser:
        case MacroKind::powerUp:
            return 0x5au;
        case MacroKind::manual:
        case MacroKind::coin:
        case MacroKind::bass:
        case MacroKind::drum:
        case MacroKind::hit:
        case MacroKind::jump:
        default:
            return 0xffu;
    }
}

uint16_t sidPulseWidthForControl(float pulseWidthControl)
{
    return static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(clampControl(pulseWidthControl) * 4095.0f)), 0, 0x0fff));
}

uint16_t sidPulseWidthForVoice(const PatchConfig& patch, size_t voice)
{
    switch (std::min(voice, size_t { 2u }))
    {
        case 1: return sidPulseWidthForControl(patch.sidVoice2PulseWidth);
        case 2: return sidPulseWidthForControl(patch.sidVoice3PulseWidth);
        case 0:
        default:
            return sidPulseWidthForControl(patch.control1);
    }
}

uint8_t sidWaveformControlForChoice(int choice)
{
    switch (std::clamp(choice, 0, 8))
    {
        case 1: return 0x10u;
        case 2: return 0x20u;
        case 3: return 0x40u;
        case 4: return 0x80u;
        case 5: return 0x30u;
        case 6: return 0x50u;
        case 7: return 0x60u;
        case 8: return 0x70u;
        case 0:
        default:
            return 0x40u;
    }
}

int sidResolvedWaveformChoiceForPatch(const PatchConfig& patch)
{
    auto choice = std::clamp(patch.waveShape, 0, 8);
    if (choice != 0)
        return choice;

    switch (patch.macro)
    {
        case MacroKind::drum:
        case MacroKind::hit:
            return 4;
        case MacroKind::arp:
        case MacroKind::laser:
        case MacroKind::powerUp:
            return 2;
        case MacroKind::jump:
            return 1;
        case MacroKind::manual:
        case MacroKind::coin:
        case MacroKind::bass:
        case MacroKind::lead:
        default:
            return 3;
    }
}

uint8_t sidWaveformControlForPatch(const PatchConfig& patch)
{
    return sidWaveformControlForChoice(sidResolvedWaveformChoiceForPatch(patch));
}

uint8_t sidWaveformControlForVoice(const PatchConfig& patch, size_t voice)
{
    auto choice = sidResolvedWaveformChoiceForPatch(patch);
    if (voice == 1)
    {
        const auto voiceChoice = std::clamp(patch.sidVoice2WaveShape, 0, 8);
        if (voiceChoice != 0)
            choice = voiceChoice;
    }
    else if (voice == 2)
    {
        const auto voiceChoice = std::clamp(patch.sidVoice3WaveShape, 0, 8);
        if (voiceChoice != 0)
            choice = voiceChoice;
    }

    return sidWaveformControlForChoice(choice);
}

uint8_t sidFilterModeBitsForPatch(const PatchConfig& patch)
{
    auto choice = std::clamp(patch.ymEnvelopeShape, 0, 8);
    if (choice == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::coin:
            case MacroKind::jump:
                choice = 3;
                break;
            case MacroKind::arp:
            case MacroKind::hit:
            case MacroKind::laser:
                choice = 2;
                break;
            case MacroKind::manual:
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::drum:
            case MacroKind::powerUp:
            default:
                choice = 1;
                break;
        }
    }

    switch (choice)
    {
        case 1: return 0x10u;
        case 2: return 0x20u;
        case 3: return 0x40u;
        case 4: return 0x00u;
        case 5: return 0x50u;
        case 6: return 0x30u;
        case 7: return 0x60u;
        case 8: return 0x70u;
        case 0:
        default:
            return 0x10u;
    }
}

uint8_t sidFilterRoutingBitsForPatch(const PatchConfig& patch)
{
    const auto choice = std::clamp(patch.sidFilterRouting, 0, 8);
    switch (choice)
    {
        case 1: return 0x07u;
        case 2: return 0x01u;
        case 3: return 0x02u;
        case 4: return 0x04u;
        case 5: return 0x03u;
        case 6: return 0x05u;
        case 7: return 0x06u;
        case 8: return 0x00u;
        case 0:
        default:
        {
            uint8_t bits = 0u;
            for (size_t voice = 0; voice < 3u; ++voice)
            {
                if (voice < patch.sourceEnabled.size() && patch.sourceEnabled[voice])
                    bits = static_cast<uint8_t>(bits | (1u << voice));
            }

            return bits == 0u ? 0x07u : static_cast<uint8_t>(bits & 0x07u);
        }
    }
}

uint8_t sidFilterResonanceForControl(float resonanceControl)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(resonanceControl) * 15.0f)), 0, 15));
}

uint8_t sidModulationBitsForPatch(const PatchConfig& patch, size_t voice)
{
    if (voice == 0)
        return 0x00u;

    auto choice = std::clamp(patch.snNoiseMode, 0, 4);
    if (choice == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::hit:
                choice = 3;
                break;
            case MacroKind::laser:
            case MacroKind::powerUp:
                choice = 2;
                break;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::arp:
            case MacroKind::drum:
            case MacroKind::jump:
            default:
                choice = 1;
                break;
        }
    }

    switch (choice)
    {
        case 2: return 0x02u;
        case 3: return 0x04u;
        case 4: return 0x06u;
        case 1:
        case 0:
        default:
            return 0x00u;
    }
}

int sidModelChoiceForPatch(const PatchConfig& patch)
{
    auto choice = std::clamp(patch.dmgStereoRoute, 0, 2);
    if (choice == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::lead:
            case MacroKind::arp:
            case MacroKind::powerUp:
                choice = 2;
                break;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::bass:
            case MacroKind::drum:
            case MacroKind::hit:
            case MacroKind::laser:
            case MacroKind::jump:
            default:
                choice = 1;
                break;
        }
    }

    return choice == 2 ? 2 : 1;
}

int sidModelNumberForPatch(const PatchConfig& patch)
{
    return sidModelChoiceForPatch(patch) == 2 ? 8580 : 6581;
}

int sidAttackChoiceForVoice(const PatchConfig& patch, size_t voice)
{
    if (voice == 1)
        return patch.sidVoice2Attack;
    if (voice == 2)
        return patch.sidVoice3Attack;

    return patch.sidAttack;
}

int sidDecayChoiceForVoice(const PatchConfig& patch, size_t voice)
{
    if (voice == 1)
        return patch.sidVoice2Decay;
    if (voice == 2)
        return patch.sidVoice3Decay;

    return patch.sidDecay;
}

int sidSustainChoiceForVoice(const PatchConfig& patch, size_t voice)
{
    if (voice == 1)
        return patch.sidVoice2Sustain;
    if (voice == 2)
        return patch.sidVoice3Sustain;

    return patch.sidSustain;
}

int sidReleaseChoiceForVoice(const PatchConfig& patch, size_t voice)
{
    if (voice == 1)
        return patch.sidVoice2Release;
    if (voice == 2)
        return patch.sidVoice3Release;

    return patch.sidRelease;
}

uint8_t sidAttackNibbleForVoice(const PatchConfig& patch, size_t voice)
{
    const auto speed = clampControl(patch.envelopeDecay);
    const auto choice = sidAttackChoiceForVoice(patch, voice);
    if (choice > 0)
        return static_cast<uint8_t>(std::clamp(choice - 1, 0, 15));

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - speed) * 6.0f)), 0, 15));
}

uint8_t sidDecayNibbleForVoice(const PatchConfig& patch, size_t voice)
{
    const auto speed = clampControl(patch.envelopeDecay);
    const auto choice = sidDecayChoiceForVoice(patch, voice);
    if (choice > 0)
        return static_cast<uint8_t>(std::clamp(choice - 1, 0, 15));

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - speed) * 9.0f)), 0, 15));
}

uint8_t sidSustainNibbleForVoice(const PatchConfig& patch, size_t voice)
{
    const auto choice = sidSustainChoiceForVoice(patch, voice);
    if (choice > 0)
        return static_cast<uint8_t>(std::clamp(choice - 1, 0, 15));

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 15.0f)), 0, 15));
}

uint8_t sidReleaseNibbleForVoice(const PatchConfig& patch, size_t voice)
{
    const auto choice = sidReleaseChoiceForVoice(patch, voice);
    if (choice > 0)
        return static_cast<uint8_t>(std::clamp(choice - 1, 0, 15));

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - clampControl(patch.envelopeDecay)) * 9.0f)), 0, 15));
}

uint8_t sidAttackNibbleForPatch(const PatchConfig& patch)
{
    return sidAttackNibbleForVoice(patch, 0);
}

uint8_t sidDecayNibbleForPatch(const PatchConfig& patch)
{
    return sidDecayNibbleForVoice(patch, 0);
}

uint8_t sidSustainNibbleForPatch(const PatchConfig& patch)
{
    return sidSustainNibbleForVoice(patch, 0);
}

uint8_t sidReleaseNibbleForPatch(const PatchConfig& patch)
{
    return sidReleaseNibbleForVoice(patch, 0);
}

uint8_t sidAttackDecayForPatch(const PatchConfig& patch)
{
    return sidAttackDecayForVoice(patch, 0);
}

uint8_t sidSustainReleaseForPatch(const PatchConfig& patch)
{
    return sidSustainReleaseForVoice(patch, 0);
}

uint8_t sidAttackDecayForVoice(const PatchConfig& patch, size_t voice)
{
    return static_cast<uint8_t>((sidAttackNibbleForVoice(patch, voice) << 4u) | sidDecayNibbleForVoice(patch, voice));
}

uint8_t sidSustainReleaseForVoice(const PatchConfig& patch, size_t voice)
{
    return static_cast<uint8_t>((sidSustainNibbleForVoice(patch, voice) << 4u) | sidReleaseNibbleForVoice(patch, voice));
}

double sidAttackSecondsForNibble(uint8_t nibble)
{
    static constexpr std::array<double, 16> times {
        0.002, 0.008, 0.016, 0.024, 0.038, 0.056, 0.068, 0.080,
        0.100, 0.250, 0.500, 0.800, 1.000, 3.000, 5.000, 8.000
    };
    return times[nibble & 0x0fu];
}

double sidDecayReleaseSecondsForNibble(uint8_t nibble)
{
    static constexpr std::array<double, 16> times {
        0.006, 0.024, 0.048, 0.072, 0.114, 0.168, 0.204, 0.240,
        0.300, 0.750, 1.500, 2.400, 3.000, 9.000, 15.000, 24.000
    };
    return times[nibble & 0x0fu];
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

int ym2149ChannelMixChoiceForPatch(const PatchConfig& patch, size_t channel)
{
    switch (channel)
    {
        case 0: return std::clamp(patch.ymChannelAMix, 0, 4);
        case 1: return std::clamp(patch.ymChannelBMix, 0, 4);
        case 2: return std::clamp(patch.ymChannelCMix, 0, 4);
        default: return 0;
    }
}

uint8_t ym2149MixerRegisterWithChannelOverrides(const PatchConfig& patch, uint8_t macroMixer)
{
    auto mixer = macroMixer;

    for (size_t channel = 0; channel < 3; ++channel)
    {
        const auto choice = ym2149ChannelMixChoiceForPatch(patch, channel);
        if (choice == 0)
            continue;

        const auto toneBit = static_cast<uint8_t>(1u << channel);
        const auto noiseBit = static_cast<uint8_t>(1u << (channel + 3u));
        const auto clearMask = static_cast<uint8_t>(0xffu ^ (toneBit | noiseBit));
        mixer = static_cast<uint8_t>(mixer & clearMask);

        switch (choice)
        {
            case 1: // Tone
                mixer = static_cast<uint8_t>(mixer | noiseBit);
                break;
            case 2: // Noise
                mixer = static_cast<uint8_t>(mixer | toneBit);
                break;
            case 4: // Off
                mixer = static_cast<uint8_t>(mixer | toneBit | noiseBit);
                break;
            case 3: // Both
            default:
                break;
        }
    }

    return mixer;
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
