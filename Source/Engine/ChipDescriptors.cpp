#include "Engine/ChipDescriptors.h"

#include <algorithm>
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
        makeModule("sources", "Channels", "Native channel layout exposed musically.", { "Pulse 1", "Pulse 2", "Triangle", "Noise / DMC planned" }),
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
        makeModule("sources", "Channels", "Four hardware sound generators.", { "Pulse 1 / sweep planned", "Pulse 2", "Wave RAM", "Noise" }),
        makeModule("tone", "Wave / Noise", "Duty, wave RAM, and polynomial noise behavior.", { "Pulse duty", "Wave level", "Noise clock", "Narrow noise" }),
        makeModule("envelope", "Envelope", "Hardware envelope and length groundwork.", { "64 Hz envelope", "256 Hz length", "DAC gating", "Sweep pending" }),
        makeModule("motion", "Motion", "Portable-game gesture templates.", { "Arp stack", "Pitch rise/drop", "Retrigger", "Coin/noise SFX" }),
        makeModule("output", "Output", "Compact handheld output character.", { "Output gain", "Speaker color planned", "Width helper planned", "Crunch helper" })
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
        makeModule("sources", "Channels", "Three tone channels and one noise channel.", { "Tone 1", "Tone 2", "Tone 3", "Noise" }),
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
            nesModules(),
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
            dmgModules(),
            commonMacros(),
            true
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
            sn76489Modules(),
            commonMacros(),
            true
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
