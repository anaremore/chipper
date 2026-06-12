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

std::vector<MacroTemplate> plannedFmMacros(std::string familyName, bool rhythmFocused = false)
{
    const auto rhythmLabel = rhythmFocused ? "Rhythm Kit Plan" : "DAC Drum Plan";
    return {
        { MacroKind::manual, familyName + " Manual Plan", "Planned preset recipe for direct algorithm/operator editing once an audited FM core is integrated.", { 0.50f, 0.50f, 0.50f, 0.50f } },
        { MacroKind::coin, familyName + " UI Chime Plan", "Planned preset recipe for short FM game UI chimes.", { 0.20f, 0.70f, 0.35f, 0.30f } },
        { MacroKind::bass, familyName + " FM Bass Plan", "Planned preset recipe for feedback-heavy chip bass.", { 0.35f, 0.75f, 0.25f, 0.55f } },
        { MacroKind::lead, familyName + " Metallic Lead Plan", "Planned preset recipe for bright operator-ratio lead sounds.", { 0.65f, 0.60f, 0.45f, 0.70f } },
        { MacroKind::arp, familyName + " Algorithm Arp Plan", "Planned preset recipe for algorithm-aware fake chords and arps.", { 0.75f, 0.50f, 0.35f, 0.60f } },
        { MacroKind::drum, familyName + " " + rhythmLabel, "Planned preset recipe for native FM percussion behavior where the chip supports it.", { 0.25f, 0.80f, 0.75f, 0.45f } },
        { MacroKind::hit, familyName + " FM Impact Plan", "Planned preset recipe for short noisy/operator impact sounds.", { 0.45f, 0.85f, 0.65f, 0.40f } },
        { MacroKind::laser, familyName + " Pitch Sweep Plan", "Planned preset recipe for FM pitch-mod SFX.", { 0.30f, 0.95f, 0.50f, 0.75f } },
        { MacroKind::jump, familyName + " Rise Blip Plan", "Planned preset recipe for quick game-rise tones.", { 0.20f, 0.60f, 0.25f, 0.35f } },
        { MacroKind::powerUp, familyName + " Power Sweep Plan", "Planned preset recipe for longer algorithm and pitch sweeps.", { 0.70f, 0.90f, 0.40f, 0.80f } }
    };
}

std::vector<MacroTemplate> ym2612Macros()
{
    return {
        { MacroKind::manual, "OPN2 Manual", "Neutral YM2612 melodic-channel mapping using the ymfm OPN2 core.", { 0.50f, 0.35f, 0.45f, 0.70f }, { true, true, true, true }, 0.0f, 0 },
        { MacroKind::coin, "OPN2 Chime", "Short bright Genesis-style UI chime using additive FM.", { 1.00f, 0.18f, 0.70f, 0.76f }, { true, false, false, false }, 0.16f, 8 },
        { MacroKind::bass, "OPN2 Feedback Bass", "Dark feedback-heavy FM bass using a serial operator algorithm.", { 0.00f, 0.82f, 0.28f, 0.88f }, { true, true, false, false }, 0.08f, 1 },
        { MacroKind::lead, "OPN2 Metallic Lead", "Forward Genesis lead with parallel carrier bite.", { 0.58f, 0.44f, 0.62f, 0.82f }, { true, true, true, false }, 0.10f, 5 },
        { MacroKind::arp, "OPN2 Fake Chord Arp", "Six YM2612 melodic channels arranged for fake chords and arpeggios.", { 0.72f, 0.32f, 0.50f, 0.78f }, { true, true, true, true }, 0.08f, 6 },
        { MacroKind::drum, "OPN2 DAC Drum", "Short Genesis channel-6 DAC drum using generated low-rate sample bytes.", { 0.14f, 0.56f, 0.44f, 0.96f }, { false, false, true, true }, 0.24f, 1 },
        { MacroKind::hit, "OPN2 Damage Hit", "Aggressive stacked operator impact.", { 0.22f, 0.90f, 0.75f, 0.78f }, { true, false, true, true }, 0.55f, 2 },
        { MacroKind::laser, "OPN2 Pitch Laser", "Genesis FM pitch sweep SFX.", { 0.30f, 0.72f, 0.88f, 0.80f }, { true, true, false, true }, 0.28f, 3 },
        { MacroKind::jump, "OPN2 Jump Blip", "Quick upward FM game gesture.", { 1.00f, 0.22f, 0.66f, 0.76f }, { true, false, false, false }, 0.18f, 8 },
        { MacroKind::powerUp, "OPN2 Power Rise", "Longer bright FM rise over stacked channels.", { 0.86f, 0.36f, 0.58f, 0.84f }, { true, true, true, true }, 0.14f, 7 }
    };
}

std::vector<MacroTemplate> ym2151Macros()
{
    return {
        { MacroKind::manual, "OPM Manual", "Neutral YM2151 melodic-channel mapping using the ymfm OPM core.", { 0.50f, 0.35f, 0.48f, 0.72f }, { true, true, true, true }, 0.0f, 0 },
        { MacroKind::coin, "OPM Arcade Chime", "Short bright arcade FM UI chime.", { 0.84f, 0.16f, 0.70f, 0.78f }, { true, false, false, false }, 0.16f, 7 },
        { MacroKind::bass, "OPM Arcade Bass", "Feedback-heavy four-operator arcade/X68000 bass.", { 0.00f, 0.82f, 0.30f, 0.88f }, { true, true, false, false }, 0.08f, 1 },
        { MacroKind::lead, "OPM Metallic Lead", "Bright YM2151 lead using parallel-carrier algorithm bias.", { 0.58f, 0.48f, 0.66f, 0.84f }, { true, true, true, false }, 0.10f, 5 },
        { MacroKind::arp, "OPM Arcade Arp", "Four exposed OPM lanes arranged for fake chords and fast arps.", { 0.78f, 0.30f, 0.54f, 0.78f }, { true, true, true, true }, 0.08f, 8 },
        { MacroKind::drum, "OPM Noise Perc", "Arcade FM percussion using the native YM2151 channel-8 noise path.", { 0.25f, 0.88f, 0.86f, 0.74f }, { false, false, true, true }, 0.58f, 3, 4, 4 },
        { MacroKind::hit, "OPM Damage Hit", "Aggressive arcade FM impact.", { 0.22f, 0.92f, 0.82f, 0.78f }, { true, false, true, true }, 0.55f, 3 },
        { MacroKind::laser, "OPM Laser Sweep", "Pitch-swept arcade FM SFX.", { 0.34f, 0.74f, 0.92f, 0.80f }, { true, true, false, true }, 0.28f, 4 },
        { MacroKind::jump, "OPM Jump Blip", "Quick upward arcade FM game gesture.", { 0.82f, 0.20f, 0.68f, 0.76f }, { true, false, false, false }, 0.18f, 7 },
        { MacroKind::powerUp, "OPM Power Sweep", "Longer arcade FM rise over stacked lanes.", { 0.86f, 0.36f, 0.62f, 0.84f }, { true, true, true, true }, 0.14f, 6 }
    };
}

std::vector<MacroTemplate> oplMacros()
{
    return {
        { MacroKind::manual, "OPL2 Manual", "Neutral two-operator OPL-compatible melodic-channel mapping using the ymfm YMF262 core.", { 0.50f, 0.30f, 0.45f, 0.72f }, { true, true, true, true }, 0.0f, 0, 0 },
        { MacroKind::coin, "OPL2 UI Bell", "Short DOS FM bell for menu and pickup sounds.", { 0.85f, 0.20f, 0.55f, 0.78f }, { true, false, false, false }, 0.16f, 3, 1 },
        { MacroKind::bass, "OPL2 Bass", "Rounded two-operator FM bass with moderate feedback.", { 0.20f, 0.62f, 0.25f, 0.88f }, { true, true, false, false }, 0.08f, 1, 1 },
        { MacroKind::lead, "OPL2 Bright Lead", "Bright DOS FM lead using selectable OPL2 waveform registers.", { 0.70f, 0.42f, 0.72f, 0.82f }, { true, true, true, false }, 0.10f, 4, 1 },
        { MacroKind::arp, "OPL2 Organ Arp", "Nine OPL melodic lanes arranged for fake chords and arps.", { 0.78f, 0.28f, 0.48f, 0.78f }, { true, true, true, true }, 0.08f, 2, 1 },
        { MacroKind::drum, "OPL2 Rhythm Kit", "Native OPL2 rhythm-mode percussion using $BD key bits.", { 0.25f, 0.85f, 0.82f, 0.76f }, { false, false, true, true }, 0.58f, 4, 2 },
        { MacroKind::hit, "OPL2 Rhythm Impact", "Short native OPL2 rhythm impact with high feedback.", { 0.28f, 0.92f, 0.78f, 0.78f }, { true, false, true, true }, 0.55f, 4, 2 },
        { MacroKind::laser, "OPL2 Laser", "Pitch-swept DOS FM SFX.", { 0.32f, 0.72f, 0.90f, 0.80f }, { true, true, false, true }, 0.28f, 4, 1 },
        { MacroKind::jump, "OPL2 Jump Bell", "Quick upward FM game gesture.", { 0.84f, 0.22f, 0.58f, 0.76f }, { true, false, false, false }, 0.18f, 3, 1 },
        { MacroKind::powerUp, "OPL2 Power Rise", "Longer optimistic DOS FM rise.", { 0.82f, 0.34f, 0.55f, 0.84f }, { true, true, true, true }, 0.14f, 2, 1 }
    };
}

std::vector<MacroTemplate> ym2413Macros()
{
    return {
        { MacroKind::manual, "OPLL Manual", "Neutral YM2413 melodic-channel preset instrument mapping.", { 0.50f, 0.50f, 0.25f, 0.72f }, { true, true, true, true }, 0.0f, 0 },
        { MacroKind::coin, "OPLL UI Chime", "Short bright OPLL vibraphone-style game UI chime.", { 0.80f, 0.72f, 0.22f, 0.78f }, { true, false, false, false }, 0.18f, 12 },
        { MacroKind::bass, "OPLL Preset Bass", "Sustaining preset-FM bass using the YM2413 instrument table.", { 0.85f, 0.22f, 0.16f, 0.86f }, { true, true, false, false }, 0.08f, 13 },
        { MacroKind::lead, "OPLL Brass Lead", "Forward OPLL trumpet/lead tone with a supporting octave lane.", { 0.45f, 0.48f, 0.24f, 0.80f }, { true, true, true, false }, 0.10f, 7 },
        { MacroKind::arp, "OPLL Organ Arp", "Preset organ stack for fast fake-chord arps.", { 0.55f, 0.70f, 0.18f, 0.78f }, { true, true, true, true }, 0.08f, 8 },
        { MacroKind::drum, "OPLL Perc Hit", "Native YM2413 rhythm-mode percussion using register $0E drum key bits.", { 0.95f, 0.18f, 0.32f, 0.72f }, { false, false, true, true }, 0.55f, 0 },
        { MacroKind::hit, "OPLL Impact", "Short preset-FM impact using stacked melodic channels.", { 0.92f, 0.25f, 0.45f, 0.76f }, { true, false, true, true }, 0.50f, 0 },
        { MacroKind::laser, "OPLL Sweep Zap", "OPLL pitch-mod SFX with vibrato-like retrigger motion.", { 0.70f, 0.95f, 0.78f, 0.76f }, { true, true, false, false }, 0.24f, 0 },
        { MacroKind::jump, "OPLL Jump Bell", "Quick rising FM bell gesture.", { 0.80f, 0.70f, 0.22f, 0.76f }, { true, false, false, false }, 0.18f, 1 },
        { MacroKind::powerUp, "OPLL Power Organ", "Longer optimistic preset-FM rise.", { 0.55f, 0.88f, 0.30f, 0.82f }, { true, true, true, true }, 0.14f, 4 }
    };
}

std::vector<MacroTemplate> plannedSampleMacros(std::string familyName, std::string sampleTerm)
{
    return {
        { MacroKind::manual, familyName + " Manual Plan", "Planned preset recipe for direct " + sampleTerm + " channel editing once a validated core exists.", { 0.50f, 0.50f, 0.50f, 0.50f } },
        { MacroKind::coin, familyName + " Short Blip Plan", "Planned preset recipe for tiny pitched " + sampleTerm + " UI sounds.", { 0.25f, 0.60f, 0.25f, 0.40f } },
        { MacroKind::bass, familyName + " Low Sample Plan", "Planned preset recipe for low-rate bass playback.", { 0.35f, 0.40f, 0.20f, 0.55f } },
        { MacroKind::lead, familyName + " Lead Sample Plan", "Planned preset recipe for melodic " + sampleTerm + " playback.", { 0.65f, 0.50f, 0.30f, 0.65f } },
        { MacroKind::arp, familyName + " Tracker Arp Plan", "Planned preset recipe for retriggered sample arps.", { 0.80f, 0.65f, 0.35f, 0.55f } },
        { MacroKind::drum, familyName + " Drum Map Plan", "Planned preset recipe for mapped one-shot percussion.", { 0.35f, 0.25f, 0.85f, 0.45f } },
        { MacroKind::hit, familyName + " Impact Plan", "Planned preset recipe for gritty one-shot hits.", { 0.45f, 0.30f, 0.75f, 0.50f } },
        { MacroKind::laser, familyName + " Rate Sweep Plan", "Planned preset recipe for sample-rate pitch sweep SFX.", { 0.25f, 0.95f, 0.50f, 0.70f } },
        { MacroKind::jump, familyName + " Jump Sample Plan", "Planned preset recipe for short upward sample gestures.", { 0.25f, 0.70f, 0.20f, 0.45f } },
        { MacroKind::powerUp, familyName + " Tracker Rise Plan", "Planned preset recipe for longer retrigger/rate-rise patterns.", { 0.75f, 0.90f, 0.35f, 0.70f } }
    };
}

std::vector<MacroTemplate> pokeyMacros()
{
    return {
        { MacroKind::manual, "POKEY Manual", "Neutral four-channel Atari POKEY mapping.", { 0.50f, 0.50f, 0.50f, 0.50f }, { true, true, true, true }, 0.0f, 0, 0 },
        { MacroKind::coin, "POKEY Console Blip", "Sharp Atari UI blip with one bright channel.", { 0.18f, 0.72f, 0.18f, 0.70f }, { true, false, false, false }, 0.25f, 0, 1 },
        { MacroKind::bass, "POKEY Distortion Bass", "Low two-channel tone with restrained polynomial edge.", { 0.32f, 0.22f, 0.28f, 0.62f }, { true, true, false, false }, 0.08f, 0, 2 },
        { MacroKind::lead, "POKEY Buzzy Lead", "Forward Atari lead using audible distortion-code color.", { 0.68f, 0.42f, 0.40f, 0.72f }, { true, true, true, false }, 0.10f, 2, 0 },
        { MacroKind::arp, "POKEY Four-Channel Arp", "Four POKEY channels arranged for fake chords and fast patterns.", { 0.82f, 0.70f, 0.25f, 0.62f }, { true, true, true, true }, 0.08f, 0, 1 },
        { MacroKind::drum, "POKEY Poly Perc", "Polynomial-noise percussion using the channel control nibbles.", { 0.28f, 0.18f, 0.88f, 0.78f }, { false, false, true, true }, 0.80f, 3, 1 },
        { MacroKind::hit, "POKEY Impact", "Harsh short noise impact.", { 0.40f, 0.25f, 0.78f, 0.82f }, { true, false, true, true }, 0.70f, 3, 3 },
        { MacroKind::laser, "POKEY Pitch Drop", "Atari pitch-drop SFX gesture.", { 0.22f, 0.95f, 0.55f, 0.76f }, { true, true, false, true }, 0.35f, 2, 4 },
        { MacroKind::jump, "POKEY Jump", "Quick upward console-game tone.", { 0.22f, 0.70f, 0.16f, 0.62f }, { true, false, false, false }, 0.20f, 0, 1 },
        { MacroKind::powerUp, "POKEY Power Rise", "Longer multi-channel Atari rise.", { 0.72f, 0.90f, 0.30f, 0.78f }, { true, true, true, true }, 0.15f, 1, 4 }
    };
}

std::vector<MacroTemplate> spc700Macros()
{
    return {
        { MacroKind::manual, "SPC700 Manual", "Neutral eight-voice lo-fi sample mapping.", { 0.50f, 0.50f, 0.22f, 0.72f }, { true, true, true, true }, 0.0f, 3 },
        { MacroKind::coin, "SPC700 UI Chime", "Short bright sample chime for menu and UI sounds.", { 0.18f, 0.74f, 0.20f, 0.78f }, { true, false, false, false }, 0.20f, 1 },
        { MacroKind::bass, "SPC700 Soft Bass", "Rounded low sample bass with subtle echo color.", { 0.30f, 0.24f, 0.26f, 0.84f }, { true, true, false, false }, 0.08f, 2 },
        { MacroKind::lead, "SPC700 Bell Lead", "Bright lo-fi sample lead with SNES-style softness.", { 0.58f, 0.46f, 0.30f, 0.80f }, { true, true, true, false }, 0.10f, 1 },
        { MacroKind::arp, "SPC700 Voice Arp", "Stacked sample voices for fake chords and arps.", { 0.86f, 0.70f, 0.28f, 0.76f }, { true, true, true, true }, 0.08f, 1 },
        { MacroKind::drum, "SPC700 Drum Map", "Short lo-fi noise sample percussion.", { 0.30f, 0.20f, 0.10f, 0.82f }, { false, false, true, true }, 0.66f, 4 },
        { MacroKind::hit, "SPC700 Damage Hit", "Layered sample impact with echo color.", { 0.44f, 0.30f, 0.18f, 0.86f }, { true, false, true, true }, 0.62f, 4 },
        { MacroKind::laser, "SPC700 Pitch Sweep", "Sample-pitch sweep SFX gesture.", { 0.24f, 0.96f, 0.22f, 0.78f }, { true, true, false, true }, 0.34f, 1 },
        { MacroKind::jump, "SPC700 Jump", "Quick upward SNES-style sample blip.", { 0.22f, 0.72f, 0.18f, 0.74f }, { true, false, false, false }, 0.16f, 1 },
        { MacroKind::powerUp, "SPC700 Power Rise", "Longer multi-voice sample rise.", { 0.74f, 0.92f, 0.36f, 0.82f }, { true, true, true, true }, 0.14f, 3 }
    };
}

std::vector<MacroTemplate> paulaMacros()
{
    return {
        { MacroKind::manual, "Paula Manual", "Neutral four-channel Amiga tracker-sampler mapping.", { 0.50f, 0.50f, 0.45f, 0.72f }, { true, true, true, true }, 0.0f, 3, 0, 1 },
        { MacroKind::coin, "Paula Chip Blip", "Short pitched sample chirp for UI sounds.", { 0.18f, 0.74f, 0.22f, 0.78f }, { true, false, false, false }, 0.20f, 1, 0, 1 },
        { MacroKind::bass, "Paula Tracker Bass", "Low looping 8-bit sample bass with a second channel body.", { 0.30f, 0.26f, 0.60f, 0.84f }, { true, true, false, false }, 0.08f, 2, 0, 3 },
        { MacroKind::lead, "Paula Chip Lead", "Bright tracker lead from a compact ramp sample.", { 0.58f, 0.46f, 0.55f, 0.80f }, { true, true, true, false }, 0.10f, 1, 0, 1 },
        { MacroKind::arp, "Paula Tracker Arp", "Four Paula sample channels arranged for fake chords and arps.", { 0.86f, 0.70f, 0.58f, 0.76f }, { true, true, true, true }, 0.08f, 1, 0, 2 },
        { MacroKind::drum, "Paula Noise Tick", "Short one-shot noise burst for tracker percussion.", { 0.30f, 0.20f, 0.12f, 0.82f }, { false, false, true, true }, 0.66f, 4, 0, 1 },
        { MacroKind::hit, "Paula Damage Hit", "Crunchy four-channel one-shot impact.", { 0.44f, 0.30f, 0.14f, 0.86f }, { true, false, true, true }, 0.62f, 4, 0, 1 },
        { MacroKind::laser, "Paula Rate Sweep", "Sample-period sweep SFX gesture.", { 0.24f, 0.96f, 0.32f, 0.78f }, { true, true, false, true }, 0.34f, 1, 0, 1 },
        { MacroKind::jump, "Paula Jump", "Quick upward tracker-sample blip.", { 0.22f, 0.72f, 0.30f, 0.74f }, { true, false, false, false }, 0.16f, 1, 0, 1 },
        { MacroKind::powerUp, "Paula Power Rise", "Longer multi-channel sample rise.", { 0.74f, 0.92f, 0.62f, 0.82f }, { true, true, true, true }, 0.14f, 3, 0, 2 }
    };
}

std::vector<MacroTemplate> huc6280Macros()
{
    return {
        { MacroKind::manual, "HuC6280 Manual", "Neutral six-channel PC Engine wavetable mapping.", { 0.50f, 0.50f, 0.20f, 0.70f }, { true, true, true, true }, 0.0f, 0 },
        { MacroKind::coin, "HuC6280 Coin Ping", "Bright short wavetable UI ping.", { 0.20f, 0.72f, 0.10f, 0.78f }, { true, false, false, false }, 0.22f, 1 },
        { MacroKind::bass, "HuC6280 Wave Bass", "Low rounded PC Engine wavetable bass.", { 0.30f, 0.20f, 0.12f, 0.82f }, { true, true, false, false }, 0.08f, 2 },
        { MacroKind::lead, "HuC6280 Glass Lead", "Forward wavetable lead with a shaped 32-sample wave.", { 0.64f, 0.42f, 0.18f, 0.76f }, { true, true, true, false }, 0.10f, 1 },
        { MacroKind::arp, "HuC6280 Six-Wave Arp", "Stacked wavetable channels for fake chords and fast arps.", { 0.88f, 0.70f, 0.14f, 0.70f }, { true, true, true, true }, 0.08f, 0 },
        { MacroKind::drum, "HuC6280 Noise Tap", "Noise-enabled upper channels for PC Engine percussion flavor.", { 0.32f, 0.18f, 0.88f, 0.80f }, { false, false, true, true }, 0.72f, 4 },
        { MacroKind::hit, "HuC6280 Hit", "Short wavetable/noise impact.", { 0.42f, 0.25f, 0.76f, 0.84f }, { true, false, true, true }, 0.65f, 4 },
        { MacroKind::laser, "HuC6280 Sweep Zap", "Pitch sweep SFX across wavetable channels.", { 0.24f, 0.96f, 0.58f, 0.78f }, { true, true, false, true }, 0.35f, 3 },
        { MacroKind::jump, "HuC6280 Jump", "Quick rising PC Engine blip.", { 0.22f, 0.70f, 0.12f, 0.72f }, { true, false, false, false }, 0.18f, 1 },
        { MacroKind::powerUp, "HuC6280 Power Wave", "Longer optimistic wavetable rise.", { 0.72f, 0.90f, 0.22f, 0.80f }, { true, true, true, true }, 0.14f, 2 }
    };
}

std::vector<MacroTemplate> sccMacros()
{
    return {
        { MacroKind::manual, "SCC Manual", "Neutral five-channel Konami wavetable mapping.", { 0.50f, 0.50f, 0.30f, 0.72f }, { true, true, true, true }, 0.0f, 0 },
        { MacroKind::coin, "SCC Coin Ping", "Bright arcade wavetable UI ping.", { 0.18f, 0.76f, 0.22f, 0.78f }, { true, false, false, false }, 0.18f, 1 },
        { MacroKind::bass, "SCC Wave Bass", "Rounded SCC bass with a second-channel body.", { 0.26f, 0.24f, 0.28f, 0.84f }, { true, true, false, false }, 0.08f, 2 },
        { MacroKind::lead, "SCC Arcade Lead", "Forward Konami wavetable lead.", { 0.54f, 0.42f, 0.32f, 0.80f }, { true, true, true, false }, 0.10f, 1 },
        { MacroKind::arp, "SCC Five-Voice Arp", "Stacked wave channels for crunchy fake chords and arps.", { 0.88f, 0.72f, 0.34f, 0.76f }, { true, true, true, true }, 0.08f, 0 },
        { MacroKind::drum, "SCC Wave Tick", "Short stepped-wave percussion.", { 0.30f, 0.18f, 0.86f, 0.82f }, { false, false, true, true }, 0.66f, 4 },
        { MacroKind::hit, "SCC Damage Hit", "Dense five-channel wavetable impact.", { 0.44f, 0.32f, 0.78f, 0.86f }, { true, false, true, true }, 0.62f, 4 },
        { MacroKind::laser, "SCC Sweep Zap", "Arcade sweep over stepped wave RAM.", { 0.24f, 0.96f, 0.70f, 0.78f }, { true, true, false, true }, 0.34f, 3 },
        { MacroKind::jump, "SCC Jump", "Quick rising wavetable blip.", { 0.22f, 0.72f, 0.28f, 0.74f }, { true, false, false, false }, 0.16f, 1 },
        { MacroKind::powerUp, "SCC Power Wave", "Longer five-channel rise.", { 0.74f, 0.92f, 0.40f, 0.82f }, { true, true, true, true }, 0.14f, 2 }
    };
}

std::vector<MacroTemplate> namcoWsgMacros()
{
    return {
        { MacroKind::manual, "Namco WSG Manual", "Neutral eight-lane arcade wavetable mapping.", { 0.50f, 0.50f, 0.26f, 0.72f }, { true, true, true, true }, 0.0f, 0 },
        { MacroKind::coin, "Namco Coin Ping", "Bright arcade coin chirp from compact 4-bit waves.", { 0.16f, 0.78f, 0.20f, 0.78f }, { true, false, false, false }, 0.18f, 1 },
        { MacroKind::bass, "Namco Wave Bass", "Rounded arcade wavetable bass with a low octave layer.", { 0.28f, 0.22f, 0.24f, 0.84f }, { true, true, false, false }, 0.08f, 2 },
        { MacroKind::lead, "Namco Arcade Lead", "Forward WSG lead with a 4-bit ramp bite.", { 0.56f, 0.42f, 0.32f, 0.80f }, { true, true, true, false }, 0.10f, 1 },
        { MacroKind::arp, "Namco Tracker Arp", "Stacked arcade wavetable lanes for fast arps.", { 0.90f, 0.72f, 0.36f, 0.76f }, { true, true, true, true }, 0.08f, 0 },
        { MacroKind::drum, "Namco Wave Tick", "Short stepped-wave percussion.", { 0.30f, 0.20f, 0.86f, 0.82f }, { false, false, true, true }, 0.66f, 4 },
        { MacroKind::hit, "Namco Damage Hit", "Dense arcade impact using internal extra lanes.", { 0.46f, 0.34f, 0.80f, 0.86f }, { true, false, true, true }, 0.62f, 4 },
        { MacroKind::laser, "Namco Sweep Zap", "Stepped arcade laser sweep.", { 0.24f, 0.96f, 0.72f, 0.78f }, { true, true, false, true }, 0.34f, 3 },
        { MacroKind::jump, "Namco Jump", "Quick rising arcade blip.", { 0.22f, 0.72f, 0.26f, 0.74f }, { true, false, false, false }, 0.16f, 1 },
        { MacroKind::powerUp, "Namco Power Wave", "Longer multi-lane arcade rise.", { 0.74f, 0.92f, 0.42f, 0.82f }, { true, true, true, true }, 0.14f, 2 }
    };
}

AccuracyDisclosure plannedDisclosure(std::string sourcePlan,
                                     std::string uiPlan,
                                     std::vector<std::string> blockers)
{
    AccuracyDisclosure disclosure;
    disclosure.summary = "Planned mode only: " + sourcePlan;
    disclosure.evidence = "No VST audio core is integrated yet. The editor exposes planned preset recipes so users can see the intended instrument shape without an accuracy claim.";
    disclosure.verifiedBehaviors = {
        "Descriptor, preset recipe, and automation metadata can be listed by the renderer.",
        uiPlan
    };
    disclosure.knownGaps = std::move(blockers);
    disclosure.knownGaps.push_back("Renderer audio, golden references, and hardware/emulator comparisons are not complete.");
    return disclosure;
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

std::vector<ParameterChoiceSpec> wavetableWaveChoices(std::string chipName, std::string memoryName, std::string pulseName = "Pulse", std::string stepsName = "Steps")
{
    return {
        choice("Follow", "Use the selected " + chipName + " preset waveform for this lane.", 0.0f, 0),
        choice("Ramp", "Rising " + memoryName + " wave RAM.", 0.25f, 1),
        choice("Tri", "Triangle-style " + memoryName + " wave RAM.", 0.5f, 2),
        choice(pulseName, pulseName + "-style wave RAM using the wave-shaping macro where available.", 0.75f, 3),
        choice(stepsName, stepsName + " wave RAM texture.", 1.0f, 4)
    };
}

ChipParameterSpec wavetableWaveSpec(ChipParameterRole role,
                                    std::string id,
                                    std::string label,
                                    std::string chipName,
                                    std::string memoryName,
                                    std::string helpSuffix = {},
                                    std::string pulseName = "Pulse",
                                    std::string stepsName = "Steps")
{
    auto help = "Selects this " + chipName + " channel's generated " + memoryName
        + " waveform RAM shape. Follow resolves from the selected preset recipe or the first channel's explicit shape.";
    if (! helpSuffix.empty())
        help += " " + helpSuffix;

    return segmentedSpec(role,
                         std::move(id),
                         std::move(label),
                         "Wave",
                         std::move(help),
                         wavetableWaveChoices(std::move(chipName), std::move(memoryName), std::move(pulseName), std::move(stepsName)),
                         ParameterKind::chipRegister);
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

std::vector<ParameterChoiceSpec> ym2413InstrumentChoices()
{
    static constexpr std::array<const char*, 16> labels {
        "Follow",
        "Violin",
        "Guitar",
        "Piano",
        "Flute",
        "Clarinet",
        "Oboe",
        "Trumpet",
        "Organ",
        "Horn",
        "Synth",
        "Harpsi",
        "Vibes",
        "Synth Bass",
        "Ac Bass",
        "E.Guitar"
    };

    std::vector<ParameterChoiceSpec> choices;
    choices.reserve(labels.size());
    choices.push_back(choice(labels[0], "Resolve the YM2413 preset instrument from the selected OPLL preset recipe.", 0.0f, 0));

    for (size_t instrument = 1; instrument < labels.size(); ++instrument)
    {
        choices.push_back(choice(labels[instrument],
                                 "Write YM2413 instrument " + std::to_string(instrument) + " into the high nibble of channel registers $30-$38.",
                                 static_cast<float>(instrument) / 15.0f,
                                 static_cast<int>(instrument)));
    }

    return choices;
}

std::vector<ParameterChoiceSpec> ym2413RhythmModeChoices()
{
    return {
        choice("Follow", "Use native OPLL rhythm mode for Drum/Hit presets and melodic mode otherwise.", 0.0f, 0),
        choice("Melodic", "Keep all nine YM2413 channels in melodic preset-instrument mode.", 0.5f, 1),
        choice("Rhythm", "Enable YM2413 rhythm mode: channels 7-9 become BD, HH, SD, TOM, and CYM.", 1.0f, 2)
    };
}

std::vector<ParameterChoiceSpec> oplRhythmModeChoices()
{
    return {
        choice("Follow", "Use native OPL rhythm mode for Drum/Hit presets and melodic mode otherwise.", 0.0f, 0),
        choice("Melodic", "Keep all nine OPL2 channels in melodic two-operator mode.", 0.5f, 1),
        choice("Rhythm", "Enable OPL2 rhythm mode: channels 7-9 become BD, HH, SD, TOM, and CYM.", 1.0f, 2)
    };
}

std::vector<ParameterChoiceSpec> ym2612AlgorithmChoices()
{
    return {
        choice("Follow", "Resolve the YM2612 algorithm from the selected OPN2 preset recipe.", 0.0f, 0),
        choice("Alg 0", "Use YM2612 algorithm 0: serial modulation for strong classic FM bass.", 1.0f / 8.0f, 1),
        choice("Alg 1", "Use YM2612 algorithm 1: serial pair with parallel modulation.", 2.0f / 8.0f, 2),
        choice("Alg 2", "Use YM2612 algorithm 2: feedback-oriented branching texture.", 3.0f / 8.0f, 3),
        choice("Alg 3", "Use YM2612 algorithm 3: mixed serial and parallel operators.", 4.0f / 8.0f, 4),
        choice("Alg 4", "Use YM2612 algorithm 4: two carrier stacks for bright leads.", 5.0f / 8.0f, 5),
        choice("Alg 5", "Use YM2612 algorithm 5: one modulator feeding three carriers.", 6.0f / 8.0f, 6),
        choice("Alg 6", "Use YM2612 algorithm 6: two modulators with two carriers.", 7.0f / 8.0f, 7),
        choice("Alg 7", "Use YM2612 algorithm 7: four parallel carriers for organ/chime tones.", 1.0f, 8)
    };
}

std::vector<ParameterChoiceSpec> ym2612PanChoices()
{
    return {
        choice("Follow", "Resolve pan from the selected OPN2 preset recipe; arps and power-up stacks alternate left/right.", 0.0f, 0),
        choice("Both", "Enable both YM2612 output bits for centered channel output.", 0.25f, 1),
        choice("Left", "Set the YM2612 left-output bit only.", 0.5f, 2),
        choice("Right", "Set the YM2612 right-output bit only.", 0.75f, 3),
        choice("Alt", "Alternate exposed YM2612 channels left and right.", 1.0f, 4)
    };
}

std::vector<ParameterChoiceSpec> ym2151PanChoices()
{
    return {
        choice("Follow", "Resolve pan from the selected OPM preset recipe; arps and power-up stacks alternate left/right.", 0.0f, 0),
        choice("Both", "Enable both YM2151 output bits in register $20+n for centered channel output.", 0.25f, 1),
        choice("Left", "Set the YM2151 left-output bit only in register $20+n.", 0.5f, 2),
        choice("Right", "Set the YM2151 right-output bit only in register $20+n.", 0.75f, 3),
        choice("Alt", "Alternate exposed YM2151 channels left and right.", 1.0f, 4)
    };
}

std::vector<ParameterChoiceSpec> ym2151NoiseChoices()
{
    return {
        choice("Follow", "Enable OPM noise for Drum/Hit presets and keep melodic presets in normal FM.", 0.0f, 0),
        choice("Off", "Clear YM2151 register $0F bit 7 so channel 8 remains normal four-operator FM.", 0.25f, 1),
        choice("Low", "Enable YM2151 channel-8 operator-4 noise with a slower $0F noise clock.", 0.5f, 2),
        choice("Mid", "Enable YM2151 channel-8 operator-4 noise with a mid $0F noise clock.", 0.75f, 3),
        choice("High", "Enable YM2151 channel-8 operator-4 noise with a fast $0F noise clock.", 1.0f, 4)
    };
}

std::vector<ParameterChoiceSpec> ym2612EnvelopeShapeChoices()
{
    return {
        choice("Follow", "Resolve the FM operator envelope registers from the selected preset recipe.", 0.0f, 0),
        choice("Pluck", "Fast attack with a short decay for bright Genesis plucks and keys.", 0.25f, 1),
        choice("Lead", "Fast attack with medium sustain for melodic FM leads.", 0.5f, 2),
        choice("Pad", "Slower attack and release for soft stacked FM tones.", 0.75f, 3),
        choice("Perc", "Fast transient with a short release for hits and percussive FM.", 1.0f, 4)
    };
}

std::vector<ParameterChoiceSpec> ym2612DacModeChoices()
{
    return {
        choice("Follow", "Use channel-6 DAC for Drum/Hit presets and melodic FM channel 6 otherwise.", 0.0f, 0),
        choice("FM Ch6", "Keep YM2612 channel 6 in normal four-operator FM mode.", 0.5f, 1),
        choice("DAC Drum", "Enable the YM2612 channel-6 DAC via $2B and stream an 8-bit drum waveform through $2A.", 1.0f, 2)
    };
}

std::vector<ChipParameterSpec> ym2612ParameterSpecs()
{
    return {
        sourceSpec(ChipParameterRole::source1Enabled, "ym2612.ch1.enabled", "FM Ch 1", "Enable YM2612 FM channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "ym2612.ch2.enabled", "FM Ch 2", "Enable YM2612 FM channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "ym2612.ch3.enabled", "FM Ch 3", "Enable YM2612 FM channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "ym2612.ch4.enabled", "FM Ch 4", "Enable YM2612 FM channel 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "ym2612.ch5.enabled", "FM Ch 5", "Enable YM2612 FM channel 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "ym2612.ch6.enabled", "FM Ch 6", "Enable YM2612 FM channel 6."),
        sourceLevelSpec(ChipParameterRole::source1Level, "ym2612.ch1.level", "FM Ch 1 Level", "Modern trim after YM2612 channel 1 output."),
        sourceLevelSpec(ChipParameterRole::source2Level, "ym2612.ch2.level", "FM Ch 2 Level", "Modern trim after YM2612 channel 2 output."),
        sourceLevelSpec(ChipParameterRole::source3Level, "ym2612.ch3.level", "FM Ch 3 Level", "Modern trim after YM2612 channel 3 output."),
        sourceLevelSpec(ChipParameterRole::source4Level, "ym2612.ch4.level", "FM Ch 4 Level", "Modern trim after YM2612 channel 4 output."),
        sourceLevelSpec(ChipParameterRole::source5Level, "ym2612.ch5.level", "FM Ch 5 Level", "Modern trim after YM2612 channel 5 output."),
        sourceLevelSpec(ChipParameterRole::source6Level, "ym2612.ch6.level", "FM Ch 6 Level", "Modern trim after YM2612 channel 6 output."),
        sliderSpec(ChipParameterRole::macroControl1,
                   "ym2612.algorithmBias",
                   "Algorithm Bias",
                   "FM",
                   "In Follow mode this chooses among the YM2612 algorithm register values; explicit Algorithm choices override it.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl2,
                   "ym2612.feedback",
                   "Feedback",
                   "FM",
                   "Maps to YM2612 feedback bits in the channel algorithm/feedback register.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl3,
                   "ym2612.operatorTone",
                   "Operator Tone",
                   "Operators",
                   "Scales operator multiplier and modulator total-level choices before writing OPN2 operator registers.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "ym2612.fmLevel",
                   "FM Level",
                   "Mixer",
                   "Controls audible carrier level through native OPN2 total-level registers.",
                   ParameterKind::chipRegister,
                   0.70f),
        { ChipParameterRole::waveShape,
          "ym2612.algorithm",
          "Algorithm",
          "FM",
          "Chooses the YM2612 four-operator algorithm. Follow lets the selected preset recipe choose.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          ym2612AlgorithmChoices(),
          0.0f,
          1.0f,
          0.0f },
        segmentedSpec(ChipParameterRole::dmgStereoRoute,
                      "ym2612.pan",
                      "Pan",
                      "Output",
                      "Writes the YM2612 channel output enable bits in register $B4: left, right, both, or alternating exposed channels.",
                      ym2612PanChoices(),
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "ym2612.envelopeShape",
                      "Envelope Shape",
                      "Envelope",
                      "Writes OPN2 operator attack, decay, sustain-rate, sustain-level, and release fields for the current musical envelope shape.",
                      ym2612EnvelopeShapeChoices(),
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "ym2612.dacMode",
                      "DAC Mode",
                      "Output",
                      "Controls the native YM2612 channel-6 DAC path. DAC Drum enables $2B and writes 8-bit samples to $2A; FM Ch6 keeps the sixth FM channel active.",
                      ym2612DacModeChoices(),
                      ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::stereoSpread,
                   "ym2612.stereoSpread",
                   "Stereo Spread",
                   "Output",
                   "Modern output width trim after native OPN2 pan bits; set Pan for register-accurate left/right routing.",
                   ParameterKind::continuous)
    };
}

std::vector<ParameterChoiceSpec> ym2151AlgorithmChoices()
{
    return {
        choice("Follow", "Resolve the YM2151 algorithm from the selected OPM preset recipe.", 0.0f, 0),
        choice("Alg 0", "Use YM2151 algorithm 0: serial four-operator modulation for classic FM bass.", 1.0f / 8.0f, 1),
        choice("Alg 1", "Use YM2151 algorithm 1: serial pair with parallel modulation.", 2.0f / 8.0f, 2),
        choice("Alg 2", "Use YM2151 algorithm 2: branching modulation texture.", 3.0f / 8.0f, 3),
        choice("Alg 3", "Use YM2151 algorithm 3: mixed serial and parallel operators.", 4.0f / 8.0f, 4),
        choice("Alg 4", "Use YM2151 algorithm 4: two carrier stacks for bright arcade leads.", 5.0f / 8.0f, 5),
        choice("Alg 5", "Use YM2151 algorithm 5: one modulator feeding three carriers.", 6.0f / 8.0f, 6),
        choice("Alg 6", "Use YM2151 algorithm 6: paired modulator/carrier routes.", 7.0f / 8.0f, 7),
        choice("Alg 7", "Use YM2151 algorithm 7: four parallel carriers for chimes and organ-like tones.", 1.0f, 8)
    };
}

std::vector<ChipParameterSpec> ym2151ParameterSpecs()
{
    return {
        sourceSpec(ChipParameterRole::source1Enabled, "ym2151.ch1.enabled", "OPM Ch 1", "Enable YM2151 melodic channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "ym2151.ch2.enabled", "OPM Ch 2", "Enable YM2151 melodic channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "ym2151.ch3.enabled", "OPM Ch 3", "Enable YM2151 melodic channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "ym2151.ch4.enabled", "OPM Ch 4", "Enable YM2151 melodic channel 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "ym2151.ch5.enabled", "OPM Ch 5", "Enable YM2151 melodic channel 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "ym2151.ch6.enabled", "OPM Ch 6", "Enable YM2151 melodic channel 6."),
        sourceSpec(ChipParameterRole::source7Enabled, "ym2151.ch7.enabled", "OPM Ch 7", "Enable YM2151 melodic channel 7."),
        sourceSpec(ChipParameterRole::source8Enabled, "ym2151.ch8.enabled", "OPM Ch 8", "Enable YM2151 melodic channel 8."),
        sourceLevelSpec(ChipParameterRole::source1Level, "ym2151.ch1.level", "OPM Ch 1 Level", "Modern trim before writing YM2151 channel 1 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source2Level, "ym2151.ch2.level", "OPM Ch 2 Level", "Modern trim before writing YM2151 channel 2 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source3Level, "ym2151.ch3.level", "OPM Ch 3 Level", "Modern trim before writing YM2151 channel 3 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source4Level, "ym2151.ch4.level", "OPM Ch 4 Level", "Modern trim before writing YM2151 channel 4 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source5Level, "ym2151.ch5.level", "OPM Ch 5 Level", "Modern trim before writing YM2151 channel 5 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source6Level, "ym2151.ch6.level", "OPM Ch 6 Level", "Modern trim before writing YM2151 channel 6 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source7Level, "ym2151.ch7.level", "OPM Ch 7 Level", "Modern trim before writing YM2151 channel 7 carrier levels."),
        sourceLevelSpec(ChipParameterRole::source8Level, "ym2151.ch8.level", "OPM Ch 8 Level", "Modern trim before writing YM2151 channel 8 carrier levels."),
        sliderSpec(ChipParameterRole::macroControl1,
                   "ym2151.algorithmBias",
                   "Algorithm Bias",
                   "FM",
                   "In Follow mode this chooses among the YM2151 algorithm register values; explicit Algorithm choices override it.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl2,
                   "ym2151.feedback",
                   "Feedback",
                   "FM",
                   "Maps to YM2151 feedback bits in the channel algorithm/feedback register.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl3,
                   "ym2151.operatorTone",
                   "Operator Tone",
                   "Operators",
                   "Scales operator multiplier and modulator total-level choices before writing OPM operator registers.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "ym2151.fmLevel",
                   "FM Level",
                   "Mixer",
                   "Controls audible carrier level through native OPM total-level registers.",
                   ParameterKind::chipRegister,
                   0.72f),
        { ChipParameterRole::waveShape,
          "ym2151.algorithm",
          "Algorithm",
          "FM",
          "Chooses the YM2151 four-operator algorithm. Follow lets the selected preset recipe choose.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          ym2151AlgorithmChoices(),
          0.0f,
          1.0f,
          0.0f },
        segmentedSpec(ChipParameterRole::dmgStereoRoute,
                      "ym2151.pan",
                      "Pan",
                      "Output",
                      "Writes the YM2151 channel left/right output bits in register $20+n: both, left, right, or alternating exposed lanes.",
                      ym2151PanChoices(),
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "ym2151.envelopeShape",
                      "Envelope Shape",
                      "Envelope",
                      "Writes OPM operator attack, decay, sustain-rate, sustain-level, and release fields for the current musical envelope shape.",
                      ym2612EnvelopeShapeChoices(),
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "ym2151.noise",
                      "OPM Noise",
                      "Motion",
                      "Writes YM2151 register $0F. Noise is the native channel-8 operator-4 noise source, useful for arcade percussion and SFX.",
                      ym2151NoiseChoices(),
                      ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::stereoSpread,
                   "ym2151.stereoSpread",
                   "Stereo Spread",
                   "Output",
                   "Modern output width trim after native OPM pan bits; set Pan for register-accurate left/right routing.",
                   ParameterKind::continuous)
    };
}

std::vector<ParameterChoiceSpec> oplWaveformChoices()
{
    return {
        choice("Follow", "Resolve the OPL2 waveform from the selected preset recipe.", 0.0f, 0),
        choice("Sine", "Write OPL2 waveform 0, the standard sine operator shape.", 0.25f, 1),
        choice("Half-Sine", "Write OPL2 waveform 1, the positive half-sine shape.", 0.5f, 2),
        choice("Abs-Sine", "Write OPL2 waveform 2, the absolute-sine shape.", 0.75f, 3),
        choice("Pseudo-Saw", "Write OPL2 waveform 3, the quarter-sine/pseudo-saw shape.", 1.0f, 4)
    };
}

std::vector<ChipParameterSpec> oplParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "opl.operatorBalance",
                   "Operator Balance",
                   "FM",
                   "Maps to the OPL connection bit, balancing modulator/carrier behavior for two-operator voices.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl2,
                   "opl.feedback",
                   "Feedback",
                   "FM",
                   "Maps to OPL feedback bits in the channel connection register.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl3,
                   "opl.operatorTone",
                   "Operator Tone",
                   "Operators",
                   "Scales OPL operator multiple, modulator total-level, and Follow waveform selection.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "opl.fmLevel",
                   "FM Level",
                   "Mixer",
                   "Controls carrier level through native OPL attenuation registers.",
                   ParameterKind::chipRegister,
                   0.72f),
        { ChipParameterRole::waveShape,
          "opl.waveform",
          "Waveform",
          "Operators",
          "Chooses the OPL2 operator waveform. Follow lets the selected preset recipe choose.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          oplWaveformChoices(),
          0.0f,
          1.0f,
          0.0f },
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "opl.rhythmMode",
                      "Rhythm Mode",
                      "Rhythm",
                      "Controls OPL2 register $BD rhythm mode. Follow uses Rhythm for Drum/Hit presets and Melodic otherwise.",
                      oplRhythmModeChoices(),
                      ParameterKind::chipRegister),
        sourceSpec(ChipParameterRole::source1Enabled, "opl.ch1.enabled", "OPL Ch 1", "Enable OPL2 melodic channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "opl.ch2.enabled", "OPL Ch 2", "Enable OPL2 melodic channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "opl.ch3.enabled", "OPL Ch 3", "Enable OPL2 melodic channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "opl.ch4.enabled", "OPL Ch 4", "Enable OPL2 melodic channel 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "opl.ch5.enabled", "OPL Ch 5", "Enable OPL2 melodic channel 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "opl.ch6.enabled", "OPL Ch 6", "Enable OPL2 melodic channel 6."),
        sourceSpec(ChipParameterRole::source7Enabled, "opl.ch7.enabled", "OPL Ch 7 / BD", "Enable OPL2 melodic channel 7, or Bass Drum in Rhythm Mode."),
        sourceSpec(ChipParameterRole::source8Enabled, "opl.ch8.enabled", "OPL Ch 8 / HH+SD", "Enable OPL2 melodic channel 8, or Hi-Hat plus Snare in Rhythm Mode."),
        sourceSpec(ChipParameterRole::source9Enabled, "opl.ch9.enabled", "OPL Ch 9 / TOM+CYM", "Enable OPL2 melodic channel 9, or Tom plus Cymbal in Rhythm Mode."),
        sourceLevelSpec(ChipParameterRole::source1Level, "opl.ch1.level", "Ch 1 Level", "Modern trim before writing OPL2 channel 1 carrier level."),
        sourceLevelSpec(ChipParameterRole::source2Level, "opl.ch2.level", "Ch 2 Level", "Modern trim before writing OPL2 channel 2 carrier level."),
        sourceLevelSpec(ChipParameterRole::source3Level, "opl.ch3.level", "Ch 3 Level", "Modern trim before writing OPL2 channel 3 carrier level."),
        sourceLevelSpec(ChipParameterRole::source4Level, "opl.ch4.level", "Ch 4 Level", "Modern trim before writing OPL2 channel 4 carrier level."),
        sourceLevelSpec(ChipParameterRole::source5Level, "opl.ch5.level", "Ch 5 Level", "Modern trim before writing OPL2 channel 5 carrier level."),
        sourceLevelSpec(ChipParameterRole::source6Level, "opl.ch6.level", "Ch 6 Level", "Modern trim before writing OPL2 channel 6 carrier level."),
        sourceLevelSpec(ChipParameterRole::source7Level, "opl.ch7.level", "Ch 7 / BD Level", "Modern trim before writing OPL2 channel 7 carrier level, or Bass Drum level in Rhythm Mode."),
        sourceLevelSpec(ChipParameterRole::source8Level, "opl.ch8.level", "Ch 8 / HH+SD Level", "Modern trim before writing OPL2 channel 8 carrier level, or Hi-Hat/Snare level in Rhythm Mode."),
        sourceLevelSpec(ChipParameterRole::source9Level, "opl.ch9.level", "Ch 9 / TOM+CYM Level", "Modern trim before writing OPL2 channel 9 carrier level, or Tom/Cymbal level in Rhythm Mode.")
    };
}

std::vector<ChipParameterSpec> ym2413ParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "ym2413.instrumentBias",
                   "Instrument Bias",
                   "Preset FM",
                   "In Follow mode this chooses across the YM2413 preset instrument numbers; explicit Instrument choices override it.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl2,
                   "ym2413.pitchSpread",
                   "Pitch Spread",
                   "Pitch",
                   "Offsets stacked melodic OPLL channels while preserving native f-number/block register writes."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "ym2413.motion",
                   "Motion",
                   "Motion",
                   "Scales musical pitch-motion presets. Native OPLL LFO/custom patch controls are planned separately.",
                   ParameterKind::macro),
        sliderSpec(ChipParameterRole::macroControl4,
                   "ym2413.channelVolume",
                   "Channel Volume",
                   "Mixer",
                   "Maps to the YM2413 channel volume nibble in registers $30-$38.",
                   ParameterKind::chipRegister,
                   0.72f),
        { ChipParameterRole::waveShape,
          "ym2413.instrument",
          "Instrument",
          "Preset FM",
          "Chooses a YM2413 ROM preset instrument. Follow lets the selected preset recipe choose the instrument.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          ym2413InstrumentChoices(),
          0.0f,
          1.0f,
          0.0f },
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "ym2413.rhythmMode",
                      "Rhythm Mode",
                      "OPLL",
                      "Controls native YM2413 register $0E rhythm mode. Follow uses Rhythm for Drum/Hit presets and Melodic otherwise.",
                      ym2413RhythmModeChoices(),
                      ParameterKind::chipRegister),
        sourceSpec(ChipParameterRole::source1Enabled, "ym2413.ch1.enabled", "OPLL Ch 1", "Enable exposed YM2413 melodic channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "ym2413.ch2.enabled", "OPLL Ch 2", "Enable exposed YM2413 melodic channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "ym2413.ch3.enabled", "OPLL Ch 3", "Enable exposed YM2413 melodic channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "ym2413.ch4.enabled", "OPLL Ch 4", "Enable YM2413 melodic channel 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "ym2413.ch5.enabled", "OPLL Ch 5", "Enable YM2413 melodic channel 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "ym2413.ch6.enabled", "OPLL Ch 6", "Enable YM2413 melodic channel 6."),
        sourceSpec(ChipParameterRole::source7Enabled, "ym2413.ch7.enabled", "OPLL Ch 7", "Enable YM2413 melodic channel 7."),
        sourceSpec(ChipParameterRole::source8Enabled, "ym2413.ch8.enabled", "OPLL Ch 8", "Enable YM2413 melodic channel 8."),
        sourceSpec(ChipParameterRole::source9Enabled, "ym2413.ch9.enabled", "OPLL Ch 9", "Enable YM2413 melodic channel 9."),
        sourceLevelSpec(ChipParameterRole::source1Level, "ym2413.ch1.level", "Ch 1 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source2Level, "ym2413.ch2.level", "Ch 2 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source3Level, "ym2413.ch3.level", "Ch 3 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source4Level, "ym2413.ch4.level", "Ch 4 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source5Level, "ym2413.ch5.level", "Ch 5 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source6Level, "ym2413.ch6.level", "Ch 6 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source7Level, "ym2413.ch7.level", "Ch 7 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source8Level, "ym2413.ch8.level", "Ch 8 Level", "Modern trim before writing the YM2413 channel volume nibble."),
        sourceLevelSpec(ChipParameterRole::source9Level, "ym2413.ch9.level", "Ch 9 Level", "Modern trim before writing the YM2413 channel volume nibble.")
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
                      "Overrides the RP2A03 pulse 2 duty register field. Follow uses the selected NES preset recipe.",
                      pulse2DutyChoices("Keep the current preset recipe: stacked Big Mono offsets Pulse 2 by one duty step; Chip Poly follows Pulse 1."),
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
        sourceSpec(ChipParameterRole::source4Enabled,
                   "nes.noise.enabled",
                   "Noise / DMC",
                   "Enable the RP2A03 noise source and the external DPCM sample lane when sample data is supplied."),
        sourceLevelSpec(ChipParameterRole::source1Level, "nes.pulse1.level", "Pulse 1 Level", "Modern trim for the first pulse source."),
        sourceLevelSpec(ChipParameterRole::source2Level, "nes.pulse2.level", "Pulse 2 Level", "Modern trim for the second pulse source."),
        sourceLevelSpec(ChipParameterRole::source3Level, "nes.triangle.level", "Triangle Level", "Modern trim for the triangle source."),
        sourceLevelSpec(ChipParameterRole::source4Level,
                        "nes.noise.level",
                        "Noise Level",
                        "Modern trim for the noise source. External DPCM follows the native DMC DAC path instead of this post trim."),
        sliderSpec(ChipParameterRole::nesDmcDirectLevel,
                   "nes.dmcDirectLevel",
                   "DMC Direct Level",
                   "DMC",
                   "Maps to the RP2A03 $4011 7-bit DMC direct DAC load. Renderer and VST playback can step external user-supplied .dmc bytes.",
                   ParameterKind::chipRegister),
        { ChipParameterRole::nesDmcSampleSlot,
          "nes.dmcSampleSlot",
          "DMC Sample Slot",
          "DMC",
          "Selects one checked .dmc sample from the current user-loaded DMC bank. MIDI CC changes switch the selected in-memory slot; WAV-to-DMC import is planned.",
          ParameterKind::steppedNumeric,
          ControlSurface::steppedSlider,
          {},
          0.0f,
          31.0f,
          0.0f },
        { ChipParameterRole::nesDmcRateIndex,
          "nes.dmcRateIndex",
          "DMC Rate",
          "DMC",
          "Maps to the RP2A03 $4010 DMC rate index. Match this to the DPCM encode rate to avoid aliasing or noisy, mis-clocked playback.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          {
              choice("0 4.18 kHz", "$4010 rate index 0, NTSC period 428 CPU cycles.", 0.0f / 15.0f, 0),
              choice("1 4.71 kHz", "$4010 rate index 1, NTSC period 380 CPU cycles.", 1.0f / 15.0f, 1),
              choice("2 5.26 kHz", "$4010 rate index 2, NTSC period 340 CPU cycles.", 2.0f / 15.0f, 2),
              choice("3 5.59 kHz", "$4010 rate index 3, NTSC period 320 CPU cycles.", 3.0f / 15.0f, 3),
              choice("4 6.26 kHz", "$4010 rate index 4, NTSC period 286 CPU cycles.", 4.0f / 15.0f, 4),
              choice("5 7.05 kHz", "$4010 rate index 5, NTSC period 254 CPU cycles.", 5.0f / 15.0f, 5),
              choice("6 7.92 kHz", "$4010 rate index 6, NTSC period 226 CPU cycles.", 6.0f / 15.0f, 6),
              choice("7 8.36 kHz", "$4010 rate index 7, NTSC period 214 CPU cycles.", 7.0f / 15.0f, 7),
              choice("8 9.42 kHz", "$4010 rate index 8, NTSC period 190 CPU cycles.", 8.0f / 15.0f, 8),
              choice("9 11.19 kHz", "$4010 rate index 9, NTSC period 160 CPU cycles.", 9.0f / 15.0f, 9),
              choice("10 12.60 kHz", "$4010 rate index 10, NTSC period 142 CPU cycles.", 10.0f / 15.0f, 10),
              choice("11 13.98 kHz", "$4010 rate index 11, NTSC period 128 CPU cycles.", 11.0f / 15.0f, 11),
              choice("12 16.88 kHz", "$4010 rate index 12, NTSC period 106 CPU cycles.", 12.0f / 15.0f, 12),
              choice("13 21.10 kHz", "$4010 rate index 13, NTSC period 85 CPU cycles.", 13.0f / 15.0f, 13),
              choice("14 24.86 kHz", "$4010 rate index 14, NTSC period 72 CPU cycles.", 14.0f / 15.0f, 14),
              choice("15 33.14 kHz", "$4010 rate index 15, NTSC period 54 CPU cycles.", 1.0f, 15)
          },
          0.0f,
          15.0f,
          15.0f },
        segmentedSpec(ChipParameterRole::nesDmcPlaybackMode,
                      "nes.dmcPlaybackMode",
                      "DMC Playback",
                      "DMC",
                      "Chooses whether the DMC lane plays the selected manual slot, maps the checked DMC bank across MIDI notes, or suppresses the other NES generators for a DMC-only sample map.",
                      {
                          choice("Manual", "Use the selected DMC Sample Slot for every note.", 0.0f, 0),
                          choice("Note Map", "Map checked DMC bank slots across MIDI notes from C1 upward; the NES DMC lane remains monophonic.", 0.5f, 1),
                          choice("Sample Map Only", "Map checked DMC bank slots across MIDI notes while muting pulse, triangle, and noise for a focused DMC sample-keyboard lane.", 1.0f, 2)
                      },
                      ParameterKind::chipRegister),
        { ChipParameterRole::nesDmcMapRoot,
          "nes.dmcMapRoot",
          "DMC Map Root",
          "DMC",
          "Selects the MIDI note where Note Map starts placing checked DMC bank slots. C1 preserves the default one-sample-per-key layout.",
          ParameterKind::steppedNumeric,
          ControlSurface::menu,
          {},
          0.0f,
          127.0f,
          36.0f },
        { ChipParameterRole::nesDmcLoop,
          "nes.dmcLoop",
          "DMC Loop",
          "DMC",
          "Maps to the RP2A03 $4010 loop bit. Off plays the selected DMC sample once; Loop repeats it from the first byte.",
          ParameterKind::chipRegister,
          ControlSurface::toggle,
          {},
          0.0f,
          1.0f,
          0.0f },
        envelopeSpec("nes.envelopeDecay", "Envelope Decay", "Maps musical decay to APU envelope period values."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "nes.noiseMode",
                      "Noise Mode",
                      "Noise",
                      "Maps to RP2A03 $400E bit 7: long LFSR noise or short-loop metallic noise. Follow resolves from the selected preset recipe.",
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
                      "Pulse 1 Duty",
                      "Pulse",
                      "Maps to the DMG NR11 duty register field for pulse channel 1.",
                      pulseDutyChoices("Thin 1/8 handheld pulse.", "Narrow 1/4 handheld pulse.", "Square 1/2 pulse.", "Wide 3/4 handheld pulse."),
                      ParameterKind::chipRegister,
                      2.0f / 3.0f),
        segmentedSpec(ChipParameterRole::pulse2Duty,
                      "dmg.pulse2Duty",
                      "Pulse 2 Duty",
                      "Pulse",
                      "Maps to the DMG NR21 duty register field for pulse channel 2. Follow keeps preset-friendly pairing with Pulse 1.",
                      pulse2DutyChoices("Keep the current preset recipe: mono stacks offset Pulse 2 by one duty step; Chip Poly follows Pulse 1."),
                      ParameterKind::chipRegister),
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
                      "Maps to NR43 bit 3: 15-bit LFSR noise or 7-bit metallic short noise. Follow resolves from the selected DMG preset recipe.",
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
                      "Maps to DMG NR51 output-routing bits for each hardware channel. Follow uses the selected DMG preset recipe.",
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
        sourceSpec(ChipParameterRole::source3Enabled, "sn76489.tone3.enabled", "Tone 3", "Enable SN76489 tone channel 3; this channel can also clock the native noise generator."),
        sourceSpec(ChipParameterRole::source4Enabled, "sn76489.noise.enabled", "Noise", "Enable the SN76489 noise channel driven by the noise-control register."),
        sourceLevelSpec(ChipParameterRole::source1Level, "sn76489.tone1.level", "Tone 1 Level", "Trims SN76489 tone channel 1 after attenuation."),
        sourceLevelSpec(ChipParameterRole::source2Level, "sn76489.tone2.level", "Tone 2 Level", "Trims SN76489 tone channel 2 after attenuation."),
        sourceLevelSpec(ChipParameterRole::source3Level, "sn76489.tone3.level", "Tone 3 Level", "Trims SN76489 tone channel 3 after attenuation."),
        sourceLevelSpec(ChipParameterRole::source4Level, "sn76489.noise.level", "Noise Level", "Trims the SN76489 noise channel after attenuation."),
        stereoSpreadSpec("sn76489.stereoSpread", "Modern stereo convenience that spreads tone channels across the stereo field; zero preserves mono PSG output."),
        { ChipParameterRole::snNoiseMode,
          "sn76489.noiseMode",
          "Noise Mode",
          "Noise",
          "Maps to the SN76489 noise-control register. Follow resolves from the current preset recipe.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          {
              choice("Follow", "Use the chip-specific macro to choose the noise register bits.", 0.0f, 0),
              choice("Periodic Low", "Periodic noise using divider N/512.", 0.25f, 1),
              choice("Periodic High", "Periodic noise using divider N/2048.", 0.5f, 2),
              choice("White Low", "White noise using divider N/512.", 0.75f, 3),
              choice("White Tone 3", "White noise clocked by tone channel 3.", 1.0f, 4)
          },
          0.0f,
          1.0f,
          0.0f }
    };
}

std::vector<ChipParameterSpec> pokeyParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "pokey.channelSpread",
                   "Channel Spread",
                   "Channels",
                   "Sets interval spread across the four POKEY channels."),
        sliderSpec(ChipParameterRole::macroControl2,
                   "pokey.pitchMotion",
                   "Pitch",
                   "Pitch",
                   "Offsets preset pitch gestures and channel intervals; paired AUDCTL lanes keep higher pitch resolution."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "pokey.distortionBias",
                   "Distortion Bias",
                   "Distortion",
                   "Biases the selected POKEY AUDC distortion/noise character when Distortion Code is Follow.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "pokey.volume",
                   "Volume",
                   "Mixer",
                   "Maps to the 4-bit AUDV volume nibble for active POKEY channels.",
                   ParameterKind::chipRegister,
                   0.65f),
        sourceSpec(ChipParameterRole::source1Enabled, "pokey.channel1.enabled", "Channel 1", "Enable POKEY audio channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "pokey.channel2.enabled", "Channel 2", "Enable POKEY audio channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "pokey.channel3.enabled", "Channel 3", "Enable POKEY audio channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "pokey.channel4.enabled", "Channel 4", "Enable POKEY audio channel 4."),
        sourceLevelSpec(ChipParameterRole::source1Level, "pokey.channel1.level", "Channel 1 Level", "Modern trim after POKEY channel 1 AUDV volume."),
        sourceLevelSpec(ChipParameterRole::source2Level, "pokey.channel2.level", "Channel 2 Level", "Modern trim after POKEY channel 2 AUDV volume."),
        sourceLevelSpec(ChipParameterRole::source3Level, "pokey.channel3.level", "Channel 3 Level", "Modern trim after POKEY channel 3 AUDV volume."),
        sourceLevelSpec(ChipParameterRole::source4Level, "pokey.channel4.level", "Channel 4 Level", "Modern trim after POKEY channel 4 AUDV volume."),
        stereoSpreadSpec("pokey.stereoSpread", "Modern stereo convenience that spreads POKEY channels 1-4; zero preserves mono output."),
        segmentedSpec(ChipParameterRole::dmgStereoRoute,
                      "pokey.audctlPairing",
                      "AUDCTL Pairing",
                      "Channels",
                      "Writes the POKEY AUDCTL 16-bit channel-link bits: channel 1+2, channel 3+4, or both.",
                      {
                          choice("Follow", "Let the selected POKEY preset recipe choose whether channels are linked.", 0.0f, 0),
                          choice("Off", "Clear the modeled 16-bit channel-link bits.", 0.25f, 1),
                          choice("1+2", "Link channel 1 and 2 into one higher-resolution 16-bit pitch lane.", 0.5f, 2),
                          choice("3+4", "Link channel 3 and 4 into one higher-resolution 16-bit pitch lane.", 0.75f, 3),
                          choice("Both", "Link both POKEY channel pairs into two 16-bit pitch lanes.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "pokey.audctlFilter",
                      "AUDCTL Filter",
                      "Filter",
                      "Writes the modeled POKEY AUDCTL high-pass filter bits. Channel 3 can filter channel 1, and channel 4 can filter channel 2.",
                      {
                          choice("Follow", "Let the selected POKEY preset recipe choose the high-pass filter bits.", 0.0f, 0),
                          choice("Off", "Clear the modeled high-pass filter bits.", 0.25f, 1),
                          choice("1<-3", "Channel 3 clocks the high-pass latch for channel 1.", 0.5f, 2),
                          choice("2<-4", "Channel 4 clocks the high-pass latch for channel 2.", 0.75f, 3),
                          choice("Both", "Enable both modeled POKEY high-pass filter paths.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        envelopeSpec("pokey.decay", "Decay", "Applies a modern musical volume-gate helper on top of POKEY AUDV nibbles; native AUDV state remains visible in debug output."),
        segmentedSpec(ChipParameterRole::waveShape,
                      "pokey.distortionCode",
                      "Distortion Code",
                      "Distortion",
                      "Selects the modeled POKEY AUDC high-nibble behavior. Follow resolves from the selected preset recipe.",
                      {
                          choice("Follow", "Use the selected POKEY preset recipe and Distortion Bias control.", 0.0f, 0),
                          choice("Pure", "AUDC-style pure tone path with the 4-bit volume nibble.", 0.25f, 1),
                          choice("Poly4", "Short polynomial texture for buzzy tones.", 0.5f, 2),
                          choice("Poly5", "Medium polynomial texture for metallic Atari noise.", 0.75f, 3),
                          choice("Poly17", "Long polynomial noise texture for percussion and grit.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> spc700ParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "spc700.voiceSpread",
                   "Voice Spread",
                   "Voices",
                   "Sets pitch interval spread across SPC700-style sample voices."),
        sliderSpec(ChipParameterRole::macroControl2,
                   "spc700.pitchMotion",
                   "Pitch / PMON",
                   "Pitch",
                   "Offsets sample pitch gestures and stacked voice intervals. Stronger values also drive Chipper's partial PMON-style previous-voice pitch modulation; exact S-DSP PMON timing/scaling remains unverified."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "spc700.echoColor",
                   "Echo Color",
                   "Echo",
                   "Controls a musical stereo echo helper with delay, feedback, and signed 8-bit FIR-style taps. This is not a verified S-DSP echo model.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "spc700.voiceVolume",
                   "Voice Volume",
                   "Mixer",
                   "Maps to simplified 7-bit per-voice volume/gain state.",
                   ParameterKind::chipRegister,
                   0.72f),
        sourceSpec(ChipParameterRole::source1Enabled, "spc700.voice1.enabled", "Voice 1", "Enable SPC700-style sample voice 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "spc700.voice2.enabled", "Voice 2", "Enable SPC700-style sample voice 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "spc700.voice3.enabled", "Voice 3", "Enable SPC700-style sample voice 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "spc700.voice4.enabled", "Voice 4", "Enable SPC700-style sample voice 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "spc700.voice5.enabled", "Voice 5", "Enable SPC700-style sample voice 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "spc700.voice6.enabled", "Voice 6", "Enable SPC700-style sample voice 6."),
        sourceSpec(ChipParameterRole::source7Enabled, "spc700.voice7.enabled", "Voice 7", "Enable SPC700-style sample voice 7."),
        sourceSpec(ChipParameterRole::source8Enabled, "spc700.voice8.enabled", "Voice 8", "Enable SPC700-style sample voice 8."),
        sourceLevelSpec(ChipParameterRole::source1Level, "spc700.voice1.level", "Voice 1 Level", "Modern trim after SPC700-style voice 1 volume."),
        sourceLevelSpec(ChipParameterRole::source2Level, "spc700.voice2.level", "Voice 2 Level", "Modern trim after SPC700-style voice 2 volume."),
        sourceLevelSpec(ChipParameterRole::source3Level, "spc700.voice3.level", "Voice 3 Level", "Modern trim after SPC700-style voice 3 volume."),
        sourceLevelSpec(ChipParameterRole::source4Level, "spc700.voice4.level", "Voice 4 Level", "Modern trim after SPC700-style voice 4 volume."),
        sourceLevelSpec(ChipParameterRole::source5Level, "spc700.voice5.level", "Voice 5 Level", "Modern trim after SPC700-style voice 5 volume."),
        sourceLevelSpec(ChipParameterRole::source6Level, "spc700.voice6.level", "Voice 6 Level", "Modern trim after SPC700-style voice 6 volume."),
        sourceLevelSpec(ChipParameterRole::source7Level, "spc700.voice7.level", "Voice 7 Level", "Modern trim after SPC700-style voice 7 volume."),
        sourceLevelSpec(ChipParameterRole::source8Level, "spc700.voice8.level", "Voice 8 Level", "Modern trim after SPC700-style voice 8 volume."),
        stereoSpreadSpec("spc700.stereoSpread", "Modern stereo convenience that spreads exposed sample voices; zero preserves centered output."),
        envelopeSpec("spc700.adsrSpeed", "ADSR / Gain Speed", "Scales the playable clean-room S-DSP ADSR/gain-style envelope while exact hardware envelope timing remains unverified."),
        segmentedSpec(ChipParameterRole::ymEnvelopeShape,
                      "spc700.envelopeShape",
                      "Envelope Shape",
                      "Envelope",
                      "Selects a musical clean-room S-DSP-style ADSR/gain contour. Follow resolves from the active SPC700 preset recipe; exact hardware envelope timing remains unverified.",
                      {
                          choice("Follow", "Resolve the envelope contour from the active SPC700 preset recipe.", 0.0f, 0),
                          choice("Pluck", "Fast attack, short decay, and low sustain for chimes and keys.", 0.25f, 1),
                          choice("Lead", "Quick attack with playable sustain for melodic sample voices.", 0.5f, 2),
                          choice("Pad", "Slower attack, higher sustain, and longer release for soft SNES beds.", 0.75f, 3),
                          choice("Perc", "Immediate transient with near-zero sustain for drums and hits.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::nesDmcPlaybackMode,
                      "spc700.brrPlayback",
                      "Sample Playback",
                      "Sample",
                      "Chooses whether a loaded SPC700 sample bank plays the selected manual slot or maps bank slots across MIDI notes from the Sample Map Root. Drum Map uses the same bank mapping path but is intended for one-shot/percussion presets.",
                      {
                          choice("Manual", "Use the selected SPC700 sample slot for every note.", 0.0f, 0),
                          choice("Note Map", "Map loaded SPC700 folder slots across MIDI notes from the Sample Map Root.", 0.5f, 1),
                          choice("Drum Map", "Map loaded SPC700 folder slots for one-shot/percussion presets.", 1.0f, 2)
                      },
                      ParameterKind::chipRegister),
        { ChipParameterRole::dmgStereoRoute,
          "spc700.samplePlayback",
          "Loop Mode",
          "Sample",
          "Chooses whether generated or loaded SPC700-style samples loop like instruments or stop at the sample end like one-shot drums. Follow Preset resolves from the active preset recipe. This is separate from Sample Playback, which chooses Manual Slot versus note-mapped bank browsing.",
          ParameterKind::chipRegister,
          ControlSurface::menu,
          {
              choice("Follow Preset", "Loop melodic preset recipes and one-shot percussion/SFX preset recipes.", 0.0f, 0),
              choice("Loop While Held", "Loop the sample RAM continuously while the note is held.", 0.5f, 1),
              choice("One Shot", "Stop the voice when the generated sample reaches its end.", 1.0f, 2)
          },
          0.0f,
          1.0f,
          0.0f },
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "spc700.noiseSource",
                      "Noise Source",
                      "Noise",
                      "Controls the partial S-DSP-style NON/noise-clock path. Follow enables noise for drum, hit, and Noise sample presets; exact S-DSP noise timing remains unverified.",
                      {
                          choice("Follow", "Resolve the noise source from the selected SPC700 preset recipe.", 0.0f, 0),
                          choice("Off", "Keep voices on BRR/generated sample playback.", 0.25f, 1),
                          choice("Low", "Use a slower lo-fi noise clock for soft hats and breath.", 0.5f, 2),
                          choice("Mid", "Use a mid noise clock for snares and grit.", 0.75f, 3),
                          choice("High", "Use a fast noise clock for bright SNES-style hiss.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        { ChipParameterRole::nesDmcMapRoot,
          "spc700.brrMapRoot",
          "Sample Map Root",
          "Sample",
          "Selects the MIDI note where the loaded SPC700 folder bank starts mapping sample slots. C1 preserves the default one-sample-per-key layout.",
          ParameterKind::steppedNumeric,
          ControlSurface::menu,
          {},
          0.0f,
          127.0f,
          36.0f },
        sliderSpec(ChipParameterRole::spc700LoopStart,
                   "spc700.loopStart",
                   "Loop Start",
                   "Sample",
                   "Normalized explicit loop start for loaded or generated SPC700-style sample memory. At 0%, BRR loop flags remain the practical default unless Loop End is moved.",
                   ParameterKind::chipRegister,
                   0.0f),
        sliderSpec(ChipParameterRole::spc700LoopEnd,
                   "spc700.loopEnd",
                   "Loop End",
                   "Sample",
                   "Normalized explicit loop end for loaded or generated SPC700-style sample memory. Keep it at 100% to loop to the sample end.",
                   ParameterKind::chipRegister,
                   1.0f),
        segmentedSpec(ChipParameterRole::waveShape,
                      "spc700.sampleShape",
                      "Sample Shape",
                      "Sample",
                      "Selects the generated lo-fi sample shape. Follow resolves from the active SPC700 preset recipe.",
                      {
                          choice("Follow", "Use the active SPC700 preset recipe sample shape.", 0.0f, 0),
                          choice("Bell", "Rounded sample with stepped lo-fi edge.", 0.25f, 1),
                          choice("Tri", "Triangle-style looped sample for bass.", 0.5f, 2),
                          choice("Pulse", "Two-level looped sample.", 0.75f, 3),
                          choice("Noise", "Short decaying noise sample for drums.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> paulaParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "paula.channelSpread",
                   "Channel Spread",
                   "Channels",
                   "Sets pitch interval spread across the four Paula sample channels."),
        sliderSpec(ChipParameterRole::macroControl2,
                   "paula.periodMotion",
                   "Period Motion",
                   "Pitch",
                   "Offsets tracker sample periods for chip-like pitch gestures."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "paula.loopBias",
                   "Loop Bias",
                   "Sample",
                   "Controls whether the active preset recipe behaves more like looping instruments or short one-shots.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "paula.channelVolume",
                   "Channel Volume",
                   "Mixer",
                   "Maps to the 0-64 Paula channel volume range.",
                   ParameterKind::chipRegister,
                   0.72f),
        sourceSpec(ChipParameterRole::source1Enabled, "paula.channel1.enabled", "Channel 1 L", "Enable Paula sample channel 1, fixed to the left hardware output."),
        sourceSpec(ChipParameterRole::source2Enabled, "paula.channel2.enabled", "Channel 2 R", "Enable Paula sample channel 2, fixed to the right hardware output."),
        sourceSpec(ChipParameterRole::source3Enabled, "paula.channel3.enabled", "Channel 3 R", "Enable Paula sample channel 3, fixed to the right hardware output."),
        sourceSpec(ChipParameterRole::source4Enabled, "paula.channel4.enabled", "Channel 4 L", "Enable Paula sample channel 4, fixed to the left hardware output."),
        sourceLevelSpec(ChipParameterRole::source1Level, "paula.channel1.level", "Channel 1 L Level", "Modern trim after Paula channel 1 volume on the fixed left output."),
        sourceLevelSpec(ChipParameterRole::source2Level, "paula.channel2.level", "Channel 2 R Level", "Modern trim after Paula channel 2 volume on the fixed right output."),
        sourceLevelSpec(ChipParameterRole::source3Level, "paula.channel3.level", "Channel 3 R Level", "Modern trim after Paula channel 3 volume on the fixed right output."),
        sourceLevelSpec(ChipParameterRole::source4Level, "paula.channel4.level", "Channel 4 L Level", "Modern trim after Paula channel 4 volume on the fixed left output."),
        stereoSpreadSpec("paula.stereoSpread", "Modern stereo spread around the classic Paula L/R/R/L channel layout; zero preserves centered output."),
        envelopeSpec("paula.decay", "Decay", "Applies a modern musical gate over Paula's 6-bit channel volume path; native channel volume remains visible in debug output."),
        segmentedSpec(ChipParameterRole::snNoiseMode,
                      "paula.outputFilter",
                      "Output Filter",
                      "Output",
                      "Selects Paula-style output coloration. Raw preserves the bright no-interpolation DAC path; A500 and LED apply modeled low-pass stages.",
                      {
                          choice("Follow", "Use the selected Paula preset recipe's output filter choice.", 0.0f, 0),
                          choice("Raw", "Bright 8-bit Paula DAC path with no extra low-pass stage.", 0.25f, 1),
                          choice("A500", "Gentle fixed Amiga 500-style output softening.", 0.5f, 2),
                          choice("LED", "Iconic LED low-pass filter color for darker tracker playback.", 0.75f, 3),
                          choice("LED+A500", "Both output stages for the warmest Paula-style color.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::waveShape,
                      "paula.channel1.sampleShape",
                      "Ch 1 Sample",
                      "Sample",
                      "Selects the generated 8-bit sample shape for Paula channel 1. Follow resolves from the active Paula preset recipe.",
                      {
                          choice("Follow", "Use the active Paula preset recipe sample shape.", 0.0f, 0),
                          choice("Ramp", "Rough 8-bit ramp sample for buzzy tracker leads.", 0.25f, 1),
                          choice("Tri", "Triangle-style looped sample for bass.", 0.5f, 2),
                          choice("Sine", "Rounded looped sample.", 0.75f, 3),
                          choice("Noise", "Short decaying noise sample for percussion.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::sidVoice2WaveShape,
                      "paula.channel2.sampleShape",
                      "Ch 2 Sample",
                      "Sample",
                      "Selects the generated 8-bit sample shape for Paula channel 2. Follow resolves from the active Paula preset recipe.",
                      {
                          choice("Follow", "Use the active Paula preset recipe sample shape.", 0.0f, 0),
                          choice("Ramp", "Rough 8-bit ramp sample for buzzy tracker leads.", 0.25f, 1),
                          choice("Tri", "Triangle-style looped sample for bass.", 0.5f, 2),
                          choice("Sine", "Rounded looped sample.", 0.75f, 3),
                          choice("Noise", "Short decaying noise sample for percussion.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::sidVoice3WaveShape,
                      "paula.channel3.sampleShape",
                      "Ch 3 Sample",
                      "Sample",
                      "Selects the generated 8-bit sample shape for Paula channel 3. Follow resolves from the active Paula preset recipe.",
                      {
                          choice("Follow", "Use the active Paula preset recipe sample shape.", 0.0f, 0),
                          choice("Ramp", "Rough 8-bit ramp sample for buzzy tracker leads.", 0.25f, 1),
                          choice("Tri", "Triangle-style looped sample for bass.", 0.5f, 2),
                          choice("Sine", "Rounded looped sample.", 0.75f, 3),
                          choice("Noise", "Short decaying noise sample for percussion.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        segmentedSpec(ChipParameterRole::pulse2Duty,
                      "paula.channel4.sampleShape",
                      "Ch 4 Sample",
                      "Sample",
                      "Selects the generated 8-bit sample shape for Paula channel 4. Follow resolves from the active Paula preset recipe.",
                      {
                          choice("Follow", "Use the active Paula preset recipe sample shape.", 0.0f, 0),
                          choice("Ramp", "Rough 8-bit ramp sample for buzzy tracker leads.", 0.25f, 1),
                          choice("Tri", "Triangle-style looped sample for bass.", 0.5f, 2),
                          choice("Sine", "Rounded looped sample.", 0.75f, 3),
                          choice("Noise", "Short decaying noise sample for percussion.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister)
    };
}

std::vector<ChipParameterSpec> huc6280ParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "huc6280.channelSpread",
                   "Channel Spread",
                   "Channels",
                   "Sets interval spread across the PC Engine wavetable channels."),
        sliderSpec(ChipParameterRole::macroControl2,
                   "huc6280.pitchMotion",
                   "Pitch",
                   "Pitch",
                   "Offsets musical pitch gestures and channel intervals."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "huc6280.noiseBias",
                   "Noise Bias",
                   "Noise",
                   "Biases the simplified HuC6280 noise-control behavior for percussion presets.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "huc6280.channelVolume",
                   "Channel Volume",
                   "Mixer",
                   "Maps to the 5-bit channel volume portion of the HuC6280 channel control register.",
                   ParameterKind::chipRegister,
                   0.70f),
        sourceSpec(ChipParameterRole::source1Enabled, "huc6280.channel1.enabled", "Channel 1", "Enable HuC6280 wavetable channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "huc6280.channel2.enabled", "Channel 2", "Enable HuC6280 wavetable channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "huc6280.channel3.enabled", "Channel 3", "Enable HuC6280 wavetable channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "huc6280.channel4.enabled", "Channel 4", "Enable HuC6280 wavetable channel 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "huc6280.channel5.enabled", "Channel 5", "Enable HuC6280 wavetable channel 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "huc6280.channel6.enabled", "Channel 6", "Enable HuC6280 wavetable channel 6."),
        sourceLevelSpec(ChipParameterRole::source1Level, "huc6280.channel1.level", "Channel 1 Level", "Modern trim after HuC6280 channel 1 volume."),
        sourceLevelSpec(ChipParameterRole::source2Level, "huc6280.channel2.level", "Channel 2 Level", "Modern trim after HuC6280 channel 2 volume."),
        sourceLevelSpec(ChipParameterRole::source3Level, "huc6280.channel3.level", "Channel 3 Level", "Modern trim after HuC6280 channel 3 volume."),
        sourceLevelSpec(ChipParameterRole::source4Level, "huc6280.channel4.level", "Channel 4 Level", "Modern trim after HuC6280 channel 4 volume."),
        sourceLevelSpec(ChipParameterRole::source5Level, "huc6280.channel5.level", "Channel 5 Level", "Modern trim after HuC6280 channel 5 volume."),
        sourceLevelSpec(ChipParameterRole::source6Level, "huc6280.channel6.level", "Channel 6 Level", "Modern trim after HuC6280 channel 6 volume."),
        stereoSpreadSpec("huc6280.stereoSpread", "Modern stereo convenience that spreads audible HuC6280 channels; zero preserves centered output."),
        envelopeSpec("huc6280.decay", "Decay", "Applies a modern musical helper over HuC6280 channel volume; native 5-bit volume state remains visible in debug output."),
        segmentedSpec(ChipParameterRole::dmgStereoRoute,
                      "huc6280.lfoPair",
                      "Ch 1/2 LFO",
                      "Motion",
                      "Selects Chipper's partial HuC6280 channel-pair pitch modulation path. The source concept follows the PC Engine PSG channel 1/2 FM-LFO pairing, but exact native LFO timing is not yet verified.",
                      {
                          choice("Follow", "Use the selected HuC6280 preset recipe to decide whether the channel-pair LFO is active.", 0.0f, 0),
                          choice("Off", "Keep channels 1 and 2 as independent audible wavetable voices.", 0.25f, 1),
                          choice("Light", "Use channel 2 as a muted wavetable LFO for gentle channel 1 pitch movement.", 0.5f, 2),
                          choice("Deep", "Use channel 2 as a muted wavetable LFO for stronger PC Engine-style pitch movement.", 0.75f, 3),
                          choice("Fast", "Use channel 2 as a muted faster wavetable LFO for sharp arcade chirps.", 1.0f, 4)
                      },
                      ParameterKind::chipRegister),
        wavetableWaveSpec(ChipParameterRole::waveShape, "huc6280.channel1.waveShape", "Ch 1 Wave", "HuC6280", "32-sample 5-bit", {}, "Square", "Noise"),
        wavetableWaveSpec(ChipParameterRole::sidVoice2WaveShape, "huc6280.channel2.waveShape", "Ch 2 Wave", "HuC6280", "32-sample 5-bit", {}, "Square", "Noise"),
        wavetableWaveSpec(ChipParameterRole::sidVoice3WaveShape, "huc6280.channel3.waveShape", "Ch 3 Wave", "HuC6280", "32-sample 5-bit", {}, "Square", "Noise"),
        wavetableWaveSpec(ChipParameterRole::pulse2Duty, "huc6280.channel4.waveShape", "Ch 4 Wave", "HuC6280", "32-sample 5-bit", {}, "Square", "Noise"),
        wavetableWaveSpec(ChipParameterRole::dmgWaveLevel, "huc6280.channel5.waveShape", "Ch 5 Wave", "HuC6280", "32-sample 5-bit", {}, "Square", "Noise"),
        wavetableWaveSpec(ChipParameterRole::snNoiseMode, "huc6280.channel6.waveShape", "Ch 6 Wave", "HuC6280", "32-sample 5-bit", {}, "Square", "Noise")
    };
}

std::vector<ChipParameterSpec> sccParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "scc.channelSpread",
                   "Channel Spread",
                   "Channels",
                   "Sets interval spread across the Konami SCC wavetable channels."),
        sliderSpec(ChipParameterRole::macroControl2,
                   "scc.pitchMotion",
                   "Pitch",
                   "Pitch",
                   "Offsets musical pitch gestures and channel intervals."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "scc.waveSkew",
                   "Wave Skew",
                   "Wave",
                   "Shapes generated waveform RAM while preserving 32-byte SCC wave memory.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "scc.channelVolume",
                   "Channel Volume",
                   "Mixer",
                   "Maps to the 4-bit SCC channel volume registers.",
                   ParameterKind::chipRegister,
                   0.72f),
        sourceSpec(ChipParameterRole::source1Enabled, "scc.channel1.enabled", "Channel 1", "Enable SCC wavetable channel 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "scc.channel2.enabled", "Channel 2", "Enable SCC wavetable channel 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "scc.channel3.enabled", "Channel 3", "Enable SCC wavetable channel 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "scc.channel4.enabled", "Channel 4", "Enable SCC wavetable channel 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "scc.channel5.enabled", "Channel 5", "Enable SCC wavetable channel 5."),
        sourceLevelSpec(ChipParameterRole::source1Level, "scc.channel1.level", "Channel 1 Level", "Modern trim after SCC channel 1 volume."),
        sourceLevelSpec(ChipParameterRole::source2Level, "scc.channel2.level", "Channel 2 Level", "Modern trim after SCC channel 2 volume."),
        sourceLevelSpec(ChipParameterRole::source3Level, "scc.channel3.level", "Channel 3 Level", "Modern trim after SCC channel 3 volume."),
        sourceLevelSpec(ChipParameterRole::source4Level, "scc.channel4.level", "Channel 4 Level", "Modern trim after SCC channel 4 volume."),
        sourceLevelSpec(ChipParameterRole::source5Level, "scc.channel5.level", "Channel 5 Level", "Modern trim after SCC channel 5 volume."),
        stereoSpreadSpec("scc.stereoSpread", "Modern stereo convenience that spreads audible SCC channels; zero preserves centered output."),
        envelopeSpec("scc.decay", "Decay", "Applies a modern musical helper over SCC channel volume; native key and 4-bit volume registers remain visible in debug output."),
        wavetableWaveSpec(ChipParameterRole::waveShape, "scc.channel1.waveShape", "Ch 1 Wave", "SCC", "32-byte 8-bit"),
        wavetableWaveSpec(ChipParameterRole::sidVoice2WaveShape, "scc.channel2.waveShape", "Ch 2 Wave", "SCC", "32-byte 8-bit"),
        wavetableWaveSpec(ChipParameterRole::sidVoice3WaveShape, "scc.channel3.waveShape", "Ch 3 Wave", "SCC", "32-byte 8-bit"),
        wavetableWaveSpec(ChipParameterRole::pulse2Duty, "scc.channel4.waveShape", "Ch 4 Wave", "SCC", "32-byte 8-bit"),
        wavetableWaveSpec(ChipParameterRole::dmgWaveLevel, "scc.channel5.waveShape", "Ch 5 Wave", "SCC", "32-byte 8-bit")
    };
}

std::vector<ChipParameterSpec> namcoWsgParameterSpecs()
{
    return {
        sliderSpec(ChipParameterRole::macroControl1,
                   "namcoWsg.channelSpread",
                   "Channel Spread",
                   "Channels",
                   "Sets interval spread across the Namco WSG wavetable lanes."),
        sliderSpec(ChipParameterRole::macroControl2,
                   "namcoWsg.pitchMotion",
                   "Pitch",
                   "Pitch",
                   "Offsets arcade pitch gestures and lane intervals."),
        sliderSpec(ChipParameterRole::macroControl3,
                   "namcoWsg.waveSkew",
                   "Wave Skew",
                   "Wave",
                   "Shapes generated 4-bit waveform RAM.",
                   ParameterKind::chipRegister),
        sliderSpec(ChipParameterRole::macroControl4,
                   "namcoWsg.channelVolume",
                   "Channel Volume",
                   "Mixer",
                   "Maps to simplified 4-bit Namco WSG lane volume registers.",
                   ParameterKind::chipRegister,
                   0.72f),
        sourceSpec(ChipParameterRole::source1Enabled, "namcoWsg.channel1.enabled", "Lane 1", "Enable Namco WSG wavetable lane 1."),
        sourceSpec(ChipParameterRole::source2Enabled, "namcoWsg.channel2.enabled", "Lane 2", "Enable Namco WSG wavetable lane 2."),
        sourceSpec(ChipParameterRole::source3Enabled, "namcoWsg.channel3.enabled", "Lane 3", "Enable Namco WSG wavetable lane 3."),
        sourceSpec(ChipParameterRole::source4Enabled, "namcoWsg.channel4.enabled", "Lane 4", "Enable Namco WSG wavetable lane 4."),
        sourceSpec(ChipParameterRole::source5Enabled, "namcoWsg.channel5.enabled", "Lane 5", "Enable Namco WSG wavetable lane 5."),
        sourceSpec(ChipParameterRole::source6Enabled, "namcoWsg.channel6.enabled", "Lane 6", "Enable Namco WSG wavetable lane 6."),
        sourceSpec(ChipParameterRole::source7Enabled, "namcoWsg.channel7.enabled", "Lane 7", "Enable Namco WSG wavetable lane 7."),
        sourceSpec(ChipParameterRole::source8Enabled, "namcoWsg.channel8.enabled", "Lane 8", "Enable Namco WSG wavetable lane 8."),
        sourceLevelSpec(ChipParameterRole::source1Level, "namcoWsg.channel1.level", "Lane 1 Level", "Modern trim after Namco WSG lane 1 volume."),
        sourceLevelSpec(ChipParameterRole::source2Level, "namcoWsg.channel2.level", "Lane 2 Level", "Modern trim after Namco WSG lane 2 volume."),
        sourceLevelSpec(ChipParameterRole::source3Level, "namcoWsg.channel3.level", "Lane 3 Level", "Modern trim after Namco WSG lane 3 volume."),
        sourceLevelSpec(ChipParameterRole::source4Level, "namcoWsg.channel4.level", "Lane 4 Level", "Modern trim after Namco WSG lane 4 volume."),
        sourceLevelSpec(ChipParameterRole::source5Level, "namcoWsg.channel5.level", "Lane 5 Level", "Modern trim after Namco WSG lane 5 volume."),
        sourceLevelSpec(ChipParameterRole::source6Level, "namcoWsg.channel6.level", "Lane 6 Level", "Modern trim after Namco WSG lane 6 volume."),
        sourceLevelSpec(ChipParameterRole::source7Level, "namcoWsg.channel7.level", "Lane 7 Level", "Modern trim after Namco WSG lane 7 volume."),
        sourceLevelSpec(ChipParameterRole::source8Level, "namcoWsg.channel8.level", "Lane 8 Level", "Modern trim after Namco WSG lane 8 volume."),
        stereoSpreadSpec("namcoWsg.stereoSpread", "Modern stereo convenience that spreads audible WSG lanes; zero preserves centered output."),
        envelopeSpec("namcoWsg.decay", "Decay", "Applies a modern musical helper over Namco WSG lane volume; native 4-bit volume and enable state remain visible in debug output."),
        wavetableWaveSpec(ChipParameterRole::waveShape, "namcoWsg.lane1.waveShape", "Lane 1 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::sidVoice2WaveShape, "namcoWsg.lane2.waveShape", "Lane 2 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::sidVoice3WaveShape, "namcoWsg.lane3.waveShape", "Lane 3 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::pulse2Duty, "namcoWsg.lane4.waveShape", "Lane 4 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::dmgWaveLevel, "namcoWsg.lane5.waveShape", "Lane 5 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::snNoiseMode, "namcoWsg.lane6.waveShape", "Lane 6 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::ymEnvelopeShape, "namcoWsg.lane7.waveShape", "Lane 7 Wave", "Namco WSG", "32-sample 4-bit"),
        wavetableWaveSpec(ChipParameterRole::dmgStereoRoute, "namcoWsg.lane8.waveShape", "Lane 8 Wave", "Namco WSG", "32-sample 4-bit")
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
                      "Selects the partial SID chip-variant profile used by Chipper's filter curve and output drive model. Auto resolves from the selected SID preset recipe.",
                      {
                          choice("Auto", "Resolve 6581/8580 from the selected SID preset recipe.", 0.0f, 0),
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
                      "Maps Voice 1 to SID control-register waveform bits. Voice 2 and Voice 3 can follow the preset or choose their own waveform.",
                      sidWaveformChoices(),
                      ParameterKind::chipRegister),
        sidVoiceWaveSpec(ChipParameterRole::sidVoice2WaveShape,
                         "sid.voice2.waveform",
                         "Voice 2 Wave",
                         "Maps Voice 2 to its own SID control-register waveform bits. Follow uses Voice 1 or the selected SID preset recipe."),
        sidVoiceWaveSpec(ChipParameterRole::sidVoice3WaveShape,
                         "sid.voice3.waveform",
                         "Voice 3 Wave",
                         "Maps audible Voice 3 to its own SID control-register waveform bits. Follow uses Voice 1 or the selected SID preset recipe; OSC3 readback is not modeled yet."),
        { ChipParameterRole::ymEnvelopeShape,
          "sid.filterMode",
          "Filter Mode",
          "Filter",
          "Maps to SID $D418 filter-mode bits, including combined mode-bit outputs. Follow uses the selected SID preset recipe.",
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
                      "Maps to SID control-register sync/ring bits on voices 2 and 3. Follow uses the selected SID preset recipe.",
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
        makeModule("motion", "Motion", "Musical gestures will map to native registers.", { "Arp mapping planned", "Pitch motion planned", "Retrigger planned", "SFX presets planned" }),
        makeModule("output", "Output", "Output model waits for source validation.", { "Output level", "Variant coloration planned", "Golden references required", "Known differences documented" })
    };
}

std::array<ModuleDescriptor, 6> nesModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "RP2A03-inspired clean-room APU model.", { "2A03 family", "NTSC/PAL clock override", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Native channel layout exposed musically.", { "Pulse 1", "Pulse 2", "Triangle / Chip Poly", "Noise / DMC lane" }),
        makeModule("tone", "Shape / Mixer", "Pulse, triangle, noise, and nonlinear mixer behavior.", { "Pulse duty", "Pitch sweep macro", "Noise mode", "Nonlinear mixer" }),
        makeModule("envelope", "Envelope", "APU envelope and duration behavior.", { "Simple envelope", "Length counters", "Triangle linear planned", "Drum decay" }),
        makeModule("motion", "Motion", "Musical gestures write chip-like preset recipes.", { "Coin blip", "Jump rise", "Laser sweep", "Fast arps" }),
        makeModule("output", "Output", "Bright direct mono chip output.", { "Output gain", "Dry mono", "Bit/sample grit", "SFX presets" })
    };
}

std::array<ModuleDescriptor, 6> dmgModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "DMG APU clean-room register model.", { "DMG profile", "CGB quirks planned", "Hybrid default", "Authentic still partial" }),
        makeModule("sources", "Channels", "Four hardware sound generators.", { "Pulse 1 / sweep", "Pulse 2", "Wave RAM", "Chip Poly ready" }),
        makeModule("tone", "Wave / Noise", "Duty, wave RAM, and polynomial noise behavior.", { "Pulse duty", "Wave shape", "Wave level", "Noise clock" }),
        makeModule("envelope", "Envelope", "Hardware envelope, length, and sweep groundwork.", { "64 Hz envelope", "256 Hz length", "DAC gating", "128 Hz CH1 sweep" }),
        makeModule("motion", "Motion", "Portable-game gesture presets.", { "Arp stack", "Pitch rise/drop", "Retrigger", "Coin/noise SFX" }),
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
        makeModule("profile", "Profile", "SN76489 PSG model backed by MIT emu76489.", { "Sega PSG", "Clock override", "MIT emu76489 core", "Authentic still partial" }),
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
        makeModule("motion", "Motion", "Classic SID modulation gestures.", { "Voice detune", "PWM-ready width", "Osc interaction", "Preset motion" }),
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
        makeModule("motion", "Motion", "FM modulation and performance helpers.", { "LFO", "Pitch mod", "Arp/glide", "Preset morph" }),
        makeModule("output", "Output", "Chip output and stereo behavior.", { "Pan / stereo", "DAC grit", "Output gain", "Reference tests needed" })
    };
}

std::array<ModuleDescriptor, 6> ym2612Modules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "YM2612/OPN2 core is backed by audited BSD-licensed ymfm.", { "YM2612 model", "NTSC Genesis clock", "Hybrid default", "Verified partial" }),
        makeModule("sources", "FM Voices", "All six YM2612 melodic channels are exposed as playable source lanes.", { "FM Ch 1", "FM Ch 2", "FM Ch 3", "FM Ch 4-6" }),
        makeModule("tone", "Operators", "Musical controls write native OPN2 algorithm, feedback, multiplier, and total-level registers.", { "Algorithm", "Feedback", "Operator tone", "Carrier level" }),
        makeModule("envelope", "Envelope", "Preset and user-selected shapes write native OPN2 attack, decay, sustain-rate, sustain-level, and release registers.", { "Envelope shape", "Attack/decay bytes", "Sustain/release bytes", "Per-operator ADSR planned" }),
        makeModule("motion", "Motion", "Genesis-style preset recipes map to register-backed FM patches.", { "Chime", "Feedback bass", "Metal lead", "Pitch laser" }),
                makeModule("output", "Output", "ymfm stereo OPN2 output follows native channel pan bits plus output trim.", { "Stereo core", "Pan bits", "DAC drum", "Reference tests needed" })
    };
}

std::array<ModuleDescriptor, 6> oplModules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "OPL2-compatible surface is backed by audited BSD-licensed ymfm YMF262/OPL3 core.", { "YMF262 core", "14.32 MHz clock", "Hybrid default", "Verified partial" }),
        makeModule("sources", "FM Voices", "All nine OPL2 melodic channels are exposed as playable lanes; rhythm mode repurposes 7-9.", { "Ch 1-6 melodic", "Ch 7-9 melodic/rhythm", "BD HH SD TOM CYM", "Chip Poly" }),
        makeModule("tone", "Operators", "Musical controls write native OPL operator and channel registers.", { "Waveform", "Feedback", "Connection", "Operator tone" }),
        makeModule("envelope", "Envelope", "Melodic and rhythm presets write native OPL attack/decay and sustain/release bytes.", { "EG type sustain", "Attack/decay bytes", "Sustain/release bytes", "Per-operator ADSR planned" }),
        makeModule("motion", "Motion", "DOS FM preset recipes map to register-backed melodic and rhythm patches.", { "UI bell", "FM bass", "Rhythm hits", "Laser" }),
        makeModule("output", "Output", "ymfm OPL3 output is routed from the four native YMF262 output buses into plugin stereo.", { "OPL3 core", "$BD rhythm bits", "Output gain", "Reference tests needed" })
    };
}

std::array<ModuleDescriptor, 6> ym2151Modules()
{
    return std::array<ModuleDescriptor, 6> {
        makeModule("profile", "Profile", "YM2151/OPM core is backed by audited BSD-licensed ymfm.", { "YM2151 core", "Arcade clock", "Hybrid default", "Verified partial" }),
        makeModule("sources", "FM Voices", "All eight OPM melodic channels are exposed as playable lanes.", { "Ch 1-4", "Ch 5-8", "Chip Poly", "Per-lane trims" }),
        makeModule("tone", "Operators", "Musical controls write native OPM algorithm, feedback, multiplier, and total-level registers.", { "Algorithm", "Feedback", "Operator tone", "Carrier level" }),
        makeModule("envelope", "Envelope", "Preset and user-selected shapes write native OPM attack, decay, sustain-rate, sustain-level, and release registers.", { "Envelope shape", "Attack/decay bytes", "Sustain/release bytes", "Per-operator ADSR planned" }),
        makeModule("motion", "Motion", "Arcade FM preset recipes map to register-backed OPM patches.", { "Chime", "Arcade bass", "Metal lead", "Laser" }),
        makeModule("output", "Output", "ymfm stereo OPM output is rendered with left/right pan enabled per active lane.", { "Stereo core", "$0F noise", "Output gain", "Reference tests needed" })
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
        ChipMode::nes,
        "Unknown / Retired Chip Mode",
        "This stored chip mode is not product-facing. Select a named chip mode with an audited or planned register target.",
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
                    "Pulse duty choices, pulse-2 duty override, sweep add/negate/mute paths, triangle linear counter behavior, noise mode/period controls, DMC direct level via event traces and APVTS/CC parameter mapping, DMC rate and loop-bit controls, external DPCM byte stepping from a generated test fixture, frame-counter register paths, source gating, Chip Poly, presets, and MIDI CC metadata are covered by renderer tests."
                },
                {
                    "Exact DMC DMA/address/IRQ timing, frame-sequencer cycle timing, hardware capture comparison, and cycle accuracy are not claimed.",
                    "User-facing DMC file/folder loading and checkbox bank curation are implemented for user-supplied .dmc files; renderer tests use generated bytes and no bundled sample content."
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
                { "level", "Envelope Level", "Mixer", "Sets the initial envelope level used by preset recipes." },
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
            "Six exposed lanes write YM2612/OPN2 registers into the audited ymfm core for Genesis-style FM tones, with optional native channel-6 DAC drum playback.",
            {
                { "algorithm", "Algorithm", "FM", "Chooses or biases the native YM2612 algorithm register." },
                { "feedback", "Feedback", "FM", "Writes YM2612 feedback bits for the active FM voices." },
                { "operator", "Operator Tone", "Operators", "Scales operator multipliers and modulator levels." },
                { "level", "FM / DAC Level", "Output", "Controls carrier level and channel-6 DAC mode through native registers." },
            },
            ym2612Modules(),
            ym2612Macros(),
            true,
            true,
            ym2612ParameterSpecs(),
            verifiedPartial(
                {
                    "BSD-3-Clause ymfm is vendored and linked as the YM2612/OPN2 synthesis core.",
                    "Renderer notes and preset recipes write OPN2 algorithm, feedback, operator multiplier/total-level, f-number/block, left/right pan bits, and key-on registers across all six melodic channels.",
                    "Channel-6 DAC Drum mode enables $2B and streams generated 8-bit drum bytes through $2A via the ymfm core.",
                    "Descriptor, MIDI CC, renderer smoke, source gating, DAC Drum, and Chip Poly regression tests cover the first playable adapter, including six visible source lanes and six-channel note allocation."
                },
                {
                    "The six-lane UI is still a compact generic source-card layout rather than a dedicated operator grid.",
                    "User PCM import for OPN2 DAC playback, LFO/AMS/PMS, full per-operator ADSR UI, SSG-EG quirks, timers, and hardware capture comparison are not complete.",
                    "Cycle accuracy is not claimed."
                })
        },
        {
            ChipMode::opl3,
            "OPL2/OPL3 / DOS FM",
            "Nine exposed lanes write OPL-compatible registers into the audited ymfm YMF262/OPL3 core for DOS FM tones and native rhythm mode.",
            {
                { "balance", "Operator Balance", "FM", "Writes the OPL connection bit for two-operator voices." },
                { "feedback", "Feedback", "FM", "Writes OPL feedback bits." },
                { "waveform", "Waveform", "Operators", "Writes OPL2 operator waveform registers." },
                { "level", "FM Level", "Output", "Controls carrier level through native OPL attenuation registers." },
            },
            oplModules(),
            oplMacros(),
            true,
            true,
            oplParameterSpecs(),
            verifiedPartial(
                {
                    "BSD-3-Clause ymfm is vendored and linked as the YMF262/OPL3 synthesis core.",
                    "Renderer notes and preset recipes write OPL-compatible operator waveform, multiple, total-level, envelope, channel feedback/connection/output-select, f-number/block, key-on, OPL3 new-mode, and $BD rhythm registers.",
                    "Descriptor, MIDI CC, renderer smoke, YMF262 high-bank/new-mode state, source gating, Rhythm Mode, and Chip Poly regression tests cover the nine-lane adapter."
                },
                {
                    "The OPL2/OPL3 mode currently exposes nine OPL2-style lanes even though the internal core is YMF262; full 18-channel editing, four-operator pairs, and deep rhythm-kit editing are not implemented.",
                    "Deep per-operator ADSR UI, LFO/tremolo/vibrato controls, rhythm-instrument fine tuning, golden emulator comparison, and hardware capture comparison are not complete.",
                    "Cycle accuracy is not claimed."
                })
        },
        {
            ChipMode::spc700,
            "SNES SPC700-style",
            "Partial clean-room SNES-style lo-fi sample-voice model with eight internal voices.",
            {
                { "voices", "Sample Voices", "Sources", "All eight lo-fi sample voices are exposed in the current UI." },
                { "sample", "Sample Color", "Tone", "Generated lo-fi sample shapes, BRR loading, partial S-DSP-style noise, and PMON-style pitch modulation." },
                { "echo", "Echo Color", "Output", "Musical echo-color helper with signed FIR-style taps." },
                { "adsr", "ADSR/Gain", "Envelope", "Playable ADSR/gain-style note shaping with preset-aware sustain and release." },
            },
            {
                makeModule("profile", "Profile", "SPC700-style clean-room sample groundwork.", { "SNES family", "32 kHz DSP-rate default", "Hybrid default", "Authentic still partial" }),
                makeModule("sources", "Sample Voices", "All eight SPC700-style sample voices are exposed as playable lanes.", { "Voices 1-4", "Voices 5-8", "Sample lane", "Chip Poly" }),
                makeModule("sample", "Sample / Pitch", "Generated lo-fi sample shapes plus user BRR/WAV/AIFF file and folder loading.", { "Sample file", "Sample folder bank", "Note map", "Noise source" }),
                makeModule("envelope", "ADSR / Gain", "Playable gain and envelope shaping derived from the selected preset.", { "Attack", "Decay", "Sustain", "Release" }),
                makeModule("motion", "Motion", "SNES-style SFX gestures mapped to sample pitch and previous-voice modulation.", { "Voice arp", "Pitch sweep", "PMON-style link", "Damage hit" }),
                makeModule("output", "Output", "Soft sample output with echo-color helper.", { "Voice volume", "Stereo spread convenience", "Echo color", "Known differences" })
            },
            spc700Macros(),
            true,
            true,
            spc700ParameterSpecs(),
            verifiedPartial(
                {
                    "Eight generated lo-fi sample voices render with pitch, 7-bit per-voice left/right volume state, loop/one-shot playback mode, explicit playable envelope-shape choices, ADSR/gain-style state, key-on/enabled masks, clean-room BRR block decoding for renderer-loaded samples, BRR loop-flag loop-start handling, plugin-loaded user BRR sample banks, WAV/AIFF imported as 8-bit sample memory, note-to-sample-bank performance mapping, partial S-DSP-style per-voice noise source selection, clean-room Gaussian-style 4-tap interpolation, musical pitch-motion, partial PMON-style previous-voice pitch modulation, echo-enable mask state, echo volume/feedback/delay register state, signed 8-bit FIR-style echo coefficient state, and a musical stereo echo helper.",
                    "Source enables, source levels, sample-shape choices, envelope-shape choices, BRR block/end/loop debug metadata, BRR loop-start debug metadata, plugin sample slot selection/state recall, note-mapped sample bank selection, noise source mode/clock/mask metadata, pitch-motion and PMON-style pitch-mod metadata, per-voice left/right volume metadata, musical echo-helper, echo-enable, EVOL/EFB/EDL-style, and FIR-tap debug metadata, envelope-release debug metadata, interpolation metadata, chip-poly allocation across all eight exposed voices, presets, and debug JSON are covered by automated tests.",
                    "No third-party SPC700, S-DSP, BRR, SNES, or tracker-player source code is vendored in this clean-room partial model."
                },
                {
                    "Source directory/sample memory addressing, exact S-DSP source-directory loop address behavior, exact S-DSP noise clocking/NON edge behavior, exact PMON timing/scaling, exact S-DSP Gaussian interpolation table behavior, exact FIR echo memory/timing behavior, exact ADSR/gain envelope timing, and SPC700 CPU timing are not implemented.",
                    "Voice scheduling, loop points, echo buffer behavior, output filtering, and hardware validation still require trusted emulator and capture comparison.",
                    "This mode is labeled SPC700-style because the implementation is a musical sample-voice approximation, not a verified SNES audio subsystem emulator."
                })
        },
        {
            ChipMode::pokey,
            "Atari POKEY",
            "Partial clean-room Atari POKEY tone/noise model with four AUDC/AUDF/AUDV-style channels.",
            {
                { "channels", "Channel Mix", "Sources", "Four POKEY channel enables and level trims." },
                { "distortion", "Distortion", "Tone", "AUDC-style pure tone and polynomial noise choices." },
                { "poly", "Poly Noise", "Noise", "Poly4, Poly5, and Poly17-inspired texture paths." },
                { "volume", "AUDV Volume", "Output", "4-bit channel volume nibble plus modern output trim." },
                { "audctl", "AUDCTL", "Routing", "16-bit channel pairing and modeled high-pass filter bits." },
            },
            {
                makeModule("profile", "Profile", "POKEY clean-room groundwork.", { "Atari 8-bit family", "1.79 MHz default", "Hybrid default", "Authentic still partial" }),
                makeModule("sources", "Channels", "Four native POKEY audio channels.", { "Channel 1", "Channel 2", "Channel 3", "Channel 4" }),
                makeModule("tone", "Distortion / Noise", "AUDC-style tone and polynomial texture controls.", { "Pure tone", "Poly4", "Poly5", "Poly17" }),
                makeModule("envelope", "Volume", "AUDV volume nibble plus musical decay helper.", { "4-bit volume", "Per-channel trims", "Decay helper", "Register readout" }),
                makeModule("motion", "Motion", "Atari SFX gestures mapped to channel timers.", { "Console blip", "Pitch drop", "Four-channel arp", "Poly perc" }),
                makeModule("output", "Output", "Bright mono Atari-style output groundwork.", { "Output gain", "Stereo spread convenience", "AUDCTL pairing", "AUDCTL filter" })
            },
            pokeyMacros(),
            true,
            true,
            pokeyParameterSpecs(),
            verifiedPartial(
                {
                    "Four AUDF/AUDC/AUDV-style channels render audible tone or polynomial-noise textures.",
                    "Source enables, source levels, chip-poly allocation, distortion choices, volume nibble mapping, AUDCTL pairing/filter choices, and debug JSON are covered by automated renderer tests.",
                    "No third-party POKEY source code is vendored in this clean-room partial model."
                },
                {
                    "1.79 MHz direct clocks, serial/I/O behavior, SKCTL side effects, exact AUDCTL edge cases, exact high-pass latch timing/coupling, and exact paired-channel carry timing are not implemented.",
                    "Polynomial taps and timer edge behavior are musical approximations pending comparison against trusted emulators and hardware captures.",
                    "Output DAC, exact volume curve, and Atari model differences are not hardware validated."
                })
        },
        {
            ChipMode::paula,
            "Amiga Paula",
            "Partial clean-room Amiga Paula tracker-sampler model with four 8-bit sample channels.",
            {
                { "sample", "Sample Mix", "Sources", "Four 8-bit sample channels with source enables and trims." },
                { "period", "Period", "Tone", "Paula-style period mapping from notes to sample playback rate." },
                { "loop", "Loop", "Envelope", "Loop/one-shot control plus musical decay helper." },
                { "tracker", "Tracker Grit", "Output", "Hard-panned tracker channel layout with 8-bit sample shapes." },
            },
            {
                makeModule("profile", "Profile", "Paula clean-room tracker sampler groundwork.", { "Amiga family", "3.55 MHz PAL default", "Hybrid default", "Authentic still partial" }),
                makeModule("sources", "Channels", "Four native Paula sample channels in the fixed Amiga L/R/R/L hardware layout.", { "Ch 1 left", "Ch 2 right", "Ch 3 right", "Ch 4 left" }),
                makeModule("sample", "Sample / Period", "8-bit sample shapes plus user WAV/AIFF/8SVX folder-bank import.", { "WAV/AIFF/8SVX file", "Folder bank", "Note map", "Slot CC117" }),
                makeModule("envelope", "Loop / Decay", "Looped instrument, one-shot behavior, and LED/A500 output color.", { "Loop bias", "One-shot drums", "Decay helper", "Output filter" }),
                makeModule("motion", "Motion", "Tracker SFX gestures mapped to sample periods.", { "Tracker arp", "Rate sweep", "Jump blip", "Damage hit" }),
                makeModule("output", "Output", "Classic hard-panned Amiga channel layout groundwork.", { "0-64 volume", "Stereo spread convenience", "User samples", "Known differences" })
            },
            paulaMacros(),
            true,
            true,
            paulaParameterSpecs(),
            verifiedPartial(
                {
                    "Four sample channels render 8-bit generated sample shapes and plugin-loaded user WAV/AIFF/uncompressed IFF-8SVX sample banks with period, volume, enable, loop, and sample-shape register paths.",
                    "Source enables, source levels, chip-poly allocation, sample-shape choices, hard-panned output, plugin sample slot selection/state recall, note-mapped sample-bank selection, and debug JSON are covered by automated tests.",
                    "No third-party Paula, ProTracker, or module-player source code is vendored in this clean-room partial model."
                },
                {
                    "DMA pointer reload timing, exact sample memory addressing, interrupt behavior, CIA/video timing, and ProTracker effect commands are not implemented.",
                    "PAL/NTSC period tables, anti-aliasing/output filtering, and analog output behavior need comparison against trusted emulators and hardware captures.",
                    "Compressed 8SVX, MOD sample import, loop-point import, and exact tracker sample metadata are planned but not part of this partial core yet."
                })
        },
        {
            ChipMode::huc6280,
            "PC Engine HuC6280",
            "Partial clean-room PC Engine HuC6280 wavetable/noise model with six internal channels.",
            {
                { "wave", "Wave RAM", "Sources", "Six 32-sample 5-bit wavetable channels." },
                { "noise", "Noise", "Tone", "Simplified noise-control path for upper channels." },
                { "volume", "Channel Volume", "Mixer", "5-bit channel control volume plus source trims." },
                { "stereo", "Stereo", "Output", "Modern stereo spread helper." },
            },
            {
                makeModule("profile", "Profile", "HuC6280 clean-room groundwork.", { "PC Engine family", "3.58 MHz default", "Hybrid default", "Authentic still partial" }),
                makeModule("sources", "Wavetable Voices", "Six direct HuC6280 channels.", { "Ch 1-3 wave", "Ch 4 wave/noise", "Ch 5-6 wave/noise", "6-note Chip Poly" }),
                makeModule("tone", "Wave / Noise", "32-sample wave RAM plus simplified noise.", { "Sine-like", "Ramp", "Triangle", "Square / noise" }),
                makeModule("envelope", "Volume", "Channel control volume plus musical decay helper.", { "5-bit volume", "Per-channel trims", "Decay helper", "Register readout" }),
                makeModule("motion", "Motion", "PC Engine SFX gestures mapped to frequency registers.", { "Coin ping", "Sweep zap", "Six-wave arp", "Noise tap" }),
                makeModule("output", "Output", "Compact console wavetable output groundwork.", { "Output gain", "Stereo spread convenience", "LFO planned", "Known differences" })
            },
            huc6280Macros(),
            true,
            true,
            huc6280ParameterSpecs(),
            verifiedPartial(
                {
                    "Channel select, 12-bit frequency, channel control, balance, waveform-RAM write, and noise-control register paths are modeled.",
                    "Six internal channels render wavetable audio, Chip Poly allocates all six, and the source surface exposes all six channel enable/trim controls.",
                    "No third-party HuC6280 source code is vendored in this clean-room partial model."
                },
                {
                    "Native LFO behavior, exact DDA behavior, stereo register routing, timer edge timing, and exact noise taps are not implemented.",
                    "The six-lane HuC6280 UI is still a compact generic source-card layout rather than a dedicated wave-RAM editor with channel-pair LFO controls.",
                    "Output DAC, analog path, and hardware or trusted-emulator comparison are not complete."
                })
        },
        {
            ChipMode::namcoWsg,
            "Namco arcade WSG",
            "Partial clean-room Namco arcade WSG wavetable model with eight internal lanes.",
            {
                { "wave", "Wave RAM", "Wave", "32-sample 4-bit wavetable lanes." },
                { "vol", "Volume", "Mixer", "Simplified 4-bit lane volumes." },
                { "enable", "Lane Enables", "Channels", "Enable-mask register path." },
                { "arcade", "Arcade Stack", "Motion", "Internal multi-lane WSG preset recipes." },
            },
            {
                makeModule("profile", "Profile", "Namco WSG clean-room groundwork.", { "Arcade wavetable family", "96 kHz default", "Hybrid default", "Authentic still partial" }),
                makeModule("sources", "WSG Lanes", "All eight wavetable lanes are exposed as playable source lanes.", { "Lanes 1-4", "Lanes 5-8", "4-bit wave RAM", "Enable mask" }),
                makeModule("wave", "Wave / Mixer", "Wave RAM plus frequency and volume behavior.", { "Wave shape", "Wave skew", "4-bit volume", "Pitch periods" }),
                makeModule("motion", "Motion", "Arcade SFX gestures mapped to lane frequency registers.", { "Coin ping", "Sweep zap", "Tracker arp", "Wave tick" }),
                makeModule("output", "Output", "Centered arcade wavetable output with optional modern spread.", { "Output gain", "Stereo spread", "Verified partial", "Known gaps" })
            },
            namcoWsgMacros(),
            true,
            true,
            namcoWsgParameterSpecs(),
            verifiedPartial(
                {
                    "Eight simplified wavetable lanes, 32-sample 4-bit wave RAM shapes, frequency, volume, and enable register paths are modeled.",
                    "All eight WSG lanes are exposed through source enable/level controls and Chip Poly allocation.",
                    "No third-party Namco WSG source code is vendored in this clean-room partial model."
                },
                {
                    "Exact Namco custom-chip variants, voice count differences, waveform addressing, DAC/output curve, and timing edge cases are not implemented.",
                    "Hardware validation and comparisons against audited reference emulators remain required."
                })
        },
        {
            ChipMode::ym2151,
            "YM2151 arcade FM",
            "Eight melodic lanes write YM2151/OPM registers into the audited ymfm core for arcade/X68000 FM tones.",
            {
                { "algorithm", "Algorithm", "FM", "Chooses or biases the native YM2151 algorithm register." },
                { "feedback", "Feedback", "FM", "Writes YM2151 feedback bits for the active OPM voices." },
                { "operator", "Operator Tone", "Operators", "Scales operator multipliers and modulator levels." },
                { "level", "FM Level", "Output", "Controls carrier level through native OPM total-level registers." },
            },
            ym2151Modules(),
            ym2151Macros(),
            true,
            true,
            ym2151ParameterSpecs(),
            verifiedPartial(
                {
                    "BSD-3-Clause ymfm is vendored and linked as the YM2151/OPM synthesis core.",
                    "Renderer notes and preset recipes write OPM algorithm, feedback, operator multiplier/total-level/envelope seed, key-code/key-fraction, pan, $0F channel-8 noise, and key-on registers.",
                    "Descriptor, MIDI CC, renderer smoke, source gating, and Chip Poly regression tests cover all eight exposed melodic lanes."
                },
                {
                    "LFO PM/AM, exact OPM noise timing/hardware comparison, timers, CSM, deep per-operator ADSR UI, golden emulator comparison, and hardware capture comparison are not complete.",
                    "Cycle accuracy is not claimed."
                })
        },
        {
            ChipMode::ym2413,
            "YM2413 / OPLL",
            "MIT emu2413-backed partial YM2413/OPLL preset-FM mode with melodic channels and native rhythm slots.",
            {
                { "instrument", "Instrument", "Preset FM", "YM2413 ROM preset instrument selection for melodic channels." },
                { "pitch", "F-Number / Block", "Pitch", "MIDI notes map to native f-number and block register writes." },
                { "rhythm", "Rhythm Mode", "OPLL", "Register $0E switches channels 7-9 into BD/HH/SD/TOM/CYM rhythm slots." },
                { "volume", "Channel Volume", "Mixer", "Source trims map to channel volume nibbles." },
                { "core", "emu2413 Core", "Accuracy", "MIT-licensed OPLL synthesis core by Mitsutaka Okazaki." },
            },
            {
                makeModule("profile", "Profile", "YM2413/OPLL preset-FM groundwork backed by emu2413.", { "YM2413 family", "3.58 MHz default", "MIT emu2413 core", "Authentic still partial" }),
                makeModule("sources", "Preset FM Voices", "All nine OPLL melodic channels are exposed as playable lanes; rhythm mode repurposes 7-9.", { "Ch 1-6 melodic", "Ch 7-9 melodic/rhythm", "BD HH SD TOM CYM", "Chip Poly" }),
                makeModule("instrument", "ROM Instrument / Rhythm", "Preset instrument, rhythm-mode, f-number, block, and key-on register writes.", { "ROM preset instruments", "$0E rhythm mode", "F-number/block", "Key-on" }),
                makeModule("envelope", "OPLL Envelopes", "Native preset and rhythm patch envelopes come from the OPLL core.", { "ROM patch ADSR", "Volume nibbles", "$0E rhythm bits", "Custom patch planned" }),
                makeModule("motion", "Motion", "Musical OPLL preset recipes mapped to melodic-channel registers.", { "UI chime", "Organ arp", "Sweep zap", "Power organ" }),
                makeModule("output", "Output", "Mono/stereo render through emu2413 with modern output trim.", { "Channel volume", "Output gain", "Known differences", "No cycle claim" })
            },
            ym2413Macros(),
            true,
            true,
            ym2413ParameterSpecs(),
            verifiedPartial(
                {
                    "MIT-licensed emu2413 is vendored as the OPLL synthesis core and driven through YM2413 register writes.",
                    "Preset instrument, channel volume, f-number, block, key-on, and $0E rhythm-mode registers render audible melodic and rhythm output.",
                    "Descriptor metadata, MIDI CC mappings, renderer debug JSON, chip-poly allocation, presets, and smoke output are covered by automated tests."
                },
                {
                    "Deep rhythm-kit editing, custom user patch editing, VRC7/YMF281 patch-set selection, stereo/pan extensions, and golden reference comparison are not complete.",
                    "Hardware validation and trusted-emulator spectral/timing comparisons are not complete, so this mode is not labeled cycle-accurate."
                })
        },
        {
            ChipMode::scc,
            "Konami SCC",
            "Partial emu2212-backed Konami SCC/SCC+ wavetable model with five internal channels.",
            {
                { "wave", "Wave RAM", "Wave", "32-byte SCC wave memory shapes." },
                { "vol", "Volume", "Mixer", "4-bit channel volume registers." },
                { "key", "Key Mask", "Channels", "Key-on register path." },
                { "stack", "Wave Stack", "Motion", "Five-channel wavetable stack presets." },
            },
            {
                makeModule("profile", "Profile", "Konami SCC/SCC+ emu2212 groundwork.", { "Konami wavetable family", "3.58 MHz default", "MIT emu2212 core", "Authentic still partial" }),
                makeModule("sources", "Wavetable Voices", "All five SCC channels are exposed as playable wavetable lanes.", { "Channel 1", "Channel 2", "Channel 3", "Channel 4-5" }),
                makeModule("wave", "Wave / Mixer", "Wave RAM plus frequency and volume behavior.", { "Wave shape", "Wave skew", "4-bit volume", "Pitch periods" }),
                makeModule("motion", "Motion", "Arcade SFX gestures mapped to frequency registers.", { "Coin ping", "Sweep zap", "Five-voice arp", "Wave tick" }),
                makeModule("output", "Output", "Centered SCC output with optional modern spread.", { "Output gain", "Stereo spread", "Verified partial", "Known gaps" })
            },
            sccMacros(),
            true,
            true,
            sccParameterSpecs(),
            verifiedPartial(
                {
                    "Five frequency, volume, key-on, and waveform-RAM register paths are driven through vendored MIT emu2212.",
                    "The shared UI exposes all five generic source controls with host/MIDI-controllable enable and level trims, including channel 5 on CC64/66.",
                    "Descriptor metadata, MIDI CC mappings, renderer debug JSON, five-channel chip-poly allocation, presets, and smoke output are covered by automated tests."
                },
                {
                    "Exact SCC cartridge mapper/bank behavior, SCC vs SCC+ mode differences, channel D/E shared-wave quirks, exact DAC/output curve, timing edge cases, and hardware validation are not complete.",
                    "Golden emulator and hardware spectral/timing comparisons are not complete, so this mode is not labeled cycle-accurate."
                })
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

size_t visibleSourceCountForMode(ChipMode mode)
{
    if (mode == ChipMode::sid)
        return 3u;
    if (mode == ChipMode::spc700)
        return 8u;
    if (mode == ChipMode::ym2612)
        return 6u;
    if (mode == ChipMode::ym2151)
        return 8u;
    if (mode == ChipMode::ym2413)
        return 9u;
    if (mode == ChipMode::opl3)
        return 9u;
    if (mode == ChipMode::huc6280)
        return 6u;
    if (mode == ChipMode::namcoWsg)
        return 8u;
    if (mode == ChipMode::scc)
        return 5u;
    return 4u;
}

size_t nativeSourceCountForMode(ChipMode mode)
{
    switch (mode)
    {
        case ChipMode::sid: return 3u;
        case ChipMode::spc700: return 8u;
        case ChipMode::huc6280: return 6u;
        case ChipMode::namcoWsg: return 8u;
        case ChipMode::ym2612: return 6u;
        case ChipMode::ym2151: return 8u;
        case ChipMode::ym2413: return 9u;
        case ChipMode::opl3: return 9u;
        case ChipMode::scc: return 5u;
        default: return visibleSourceCountForMode(mode);
    }
}

bool hasInternalSourceLanes(ChipMode mode)
{
    return nativeSourceCountForMode(mode) > visibleSourceCountForMode(mode);
}

std::vector<ChipMode> chipModeOrder()
{
    std::vector<ChipMode> modes;
    const auto& items = descriptors();
    modes.reserve(items.size());
    for (const auto& item : items)
        modes.push_back(item.mode);
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
                            std::array<bool, 9> sourceEnabled,
                            std::array<float, 9> sourceLevels,
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
                            float nesDmcDirectLevel,
                            int nesDmcRateIndex,
                            bool nesDmcLoop,
                            bool nesDmcOnly,
                            float spc700LoopStart,
                            float spc700LoopEnd)
{
    const auto effectivePlayMode = supportsPlayMode(mode, playMode) ? playMode : PlayMode::stack;
    const auto maxYmEnvelopeShape = mode == ChipMode::sid ? 8 : ((mode == ChipMode::ym2612 || mode == ChipMode::ym2151) ? 4 : ((mode == ChipMode::ym2413 || mode == ChipMode::opl3) ? 2 : 20));
    const auto maxWaveShape = mode == ChipMode::ym2413
        ? 15
        : ((mode == ChipMode::sid || mode == ChipMode::ym2612 || mode == ChipMode::ym2151) ? 8 : 4);

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
            clampControl(sourceLevels[3]),
            clampControl(sourceLevels[4]),
            clampControl(sourceLevels[5]),
            clampControl(sourceLevels[6]),
            clampControl(sourceLevels[7]),
            clampControl(sourceLevels[8])
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
        clampControl(nesDmcDirectLevel),
        std::clamp(nesDmcRateIndex, 0, 15),
        nesDmcLoop,
        nesDmcOnly,
        clampControl(std::min(spc700LoopStart, spc700LoopEnd)),
        clampControl(std::max(spc700LoopStart, spc700LoopEnd))
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

uint8_t ym2149EnvelopeShapeCodeForChoice(int shapeChoice)
{
    shapeChoice = std::clamp(shapeChoice, 0, 20);
    if (shapeChoice >= 5)
        return static_cast<uint8_t>(shapeChoice - 5);

    switch (shapeChoice)
    {
        case 1: return 0x09u; // Fall, hold low.
        case 2: return 0x0du; // Rise, hold high.
        case 3: return 0x08u; // Repeating saw down.
        case 4: return 0x0eu; // Repeating triangle.
        case 0:
        default:
            return 0x09u;
    }
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

uint8_t spc700NoiseModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.snNoiseMode, 0, 4);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    if (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit || patch.waveShape == 4)
        return 4u;

    return 1u;
}

uint8_t spc700NoiseClockForPatch(const PatchConfig& patch)
{
    switch (spc700NoiseModeForPatch(patch))
    {
        case 2: return 10u;
        case 3: return 18u;
        case 4: return 26u;
        case 1:
        default:
            return 0u;
    }
}

uint8_t pokeyAudcForPatch(const PatchConfig& patch)
{
    auto choiceValue = std::clamp(patch.waveShape, 0, 4);
    if (choiceValue == 0)
    {
        if (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit)
            choiceValue = 4;
        else if (patch.macro == MacroKind::lead || patch.macro == MacroKind::laser)
            choiceValue = patch.control3 > 0.55f ? 3 : 2;
        else
            choiceValue = patch.control3 > 0.65f ? 2 : 1;
    }

    const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 15.0f)), 0, 15));
    uint8_t distortion = 0xa0u; // Pure tone.
    switch (choiceValue)
    {
        case 2: distortion = 0x20u; break; // Poly4-ish.
        case 3: distortion = 0x40u; break; // Poly5-ish.
        case 4: distortion = 0x80u; break; // Poly17-ish.
        case 1:
        default:
            distortion = 0xa0u;
            break;
    }

    return static_cast<uint8_t>(distortion | volume);
}

uint8_t pokeyAudctlForPatch(const PatchConfig& patch)
{
    auto choiceValue = std::clamp(patch.dmgStereoRoute, 0, 4);
    if (choiceValue == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::bass:
                choiceValue = 2;
                break;
            case MacroKind::powerUp:
                choiceValue = 4;
                break;
            case MacroKind::hit:
                choiceValue = patch.control2 > 0.65f ? 3 : 1;
                break;
            default:
                choiceValue = 1;
                break;
        }
    }

    switch (choiceValue)
    {
        case 2: return static_cast<uint8_t>(0x10u | pokeyFilterBitsForPatch(patch)); // CH1+CH2 form one 16-bit divider.
        case 3: return static_cast<uint8_t>(0x08u | pokeyFilterBitsForPatch(patch)); // CH3+CH4 form one 16-bit divider.
        case 4: return static_cast<uint8_t>(0x18u | pokeyFilterBitsForPatch(patch)); // Both 16-bit channel pairs enabled.
        case 1:
        default:
            return pokeyFilterBitsForPatch(patch);
    }
}

uint8_t pokeyFilterChoiceForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.ymEnvelopeShape, 0, 4);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    switch (patch.macro)
    {
        case MacroKind::bass:
            return 2;
        case MacroKind::hit:
            return 3;
        case MacroKind::laser:
        case MacroKind::powerUp:
            return 4;
        case MacroKind::manual:
        case MacroKind::coin:
        case MacroKind::lead:
        case MacroKind::arp:
        case MacroKind::drum:
        case MacroKind::jump:
        default:
            return 1;
    }
}

uint8_t pokeyFilterBitsForPatch(const PatchConfig& patch)
{
    switch (pokeyFilterChoiceForPatch(patch))
    {
        case 2: return 0x04u; // CH3 clocks high-pass filtering on CH1.
        case 3: return 0x02u; // CH4 clocks high-pass filtering on CH2.
        case 4: return 0x06u; // Both high-pass paths enabled.
        case 1:
        default:
            return 0x00u;
    }
}

uint8_t pokeyAudfForNote(double clockHz, int midiNote)
{
    const auto safeClock = clockHz > 0.0 ? clockHz : 1789790.0;
    const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
    const auto divisor = std::round(safeClock / (56.0 * hz) - 1.0);
    return static_cast<uint8_t>(std::clamp(static_cast<int>(divisor), 0, 255));
}

uint8_t ym2612AlgorithmForPatch(const PatchConfig& patch)
{
    if (patch.waveShape > 0)
        return static_cast<uint8_t>(std::clamp(patch.waveShape - 1, 0, 7));

    switch (patch.macro)
    {
        case MacroKind::bass: return 0;
        case MacroKind::lead: return 4;
        case MacroKind::arp: return 5;
        case MacroKind::coin:
        case MacroKind::jump: return 7;
        case MacroKind::drum:
        case MacroKind::hit: return 1;
        case MacroKind::laser: return 2;
        case MacroKind::powerUp: return 6;
        case MacroKind::manual:
        default: break;
    }

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control1) * 7.0f)), 0, 7));
}

uint8_t ym2612DacModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.snNoiseMode, 0, 2);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    return (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit) ? 2u : 1u;
}

uint8_t ym2612PanBitsForPatch(const PatchConfig& patch, size_t channel)
{
    auto choiceValue = std::clamp(patch.dmgStereoRoute, 0, 4);
    if (choiceValue == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::arp:
            case MacroKind::powerUp:
                choiceValue = 4;
                break;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::drum:
            case MacroKind::hit:
            case MacroKind::laser:
            case MacroKind::jump:
            default:
                choiceValue = 1;
                break;
        }
    }

    switch (choiceValue)
    {
        case 2: return 0x80u;
        case 3: return 0x40u;
        case 4: return (channel % 2u) == 0u ? 0x80u : 0x40u;
        case 1:
        default:
            return 0xc0u;
    }
}

uint8_t ym2151PanBitsForPatch(const PatchConfig& patch, size_t channel)
{
    return ym2612PanBitsForPatch(patch, channel);
}

uint8_t ym2151NoiseRegisterForPatch(const PatchConfig& patch)
{
    auto choice = std::clamp(patch.snNoiseMode, 0, 4);
    if (choice == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::drum:
            case MacroKind::hit:
                return static_cast<uint8_t>(0x80u | std::clamp(static_cast<int>(std::round(clampControl(patch.control3) * 31.0f)), 0, 31));
            case MacroKind::laser:
                choice = 3;
                break;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::arp:
            case MacroKind::jump:
            case MacroKind::powerUp:
            default:
                choice = 1;
                break;
        }
    }

    switch (choice)
    {
        case 2: return 0x84u;
        case 3: return 0x90u;
        case 4: return 0x9fu;
        case 1:
        case 0:
        default:
            return 0x00u;
    }
}

FmEnvelopeRegisters ym2612EnvelopeRegistersForPatch(const PatchConfig& patch, size_t op)
{
    auto choiceValue = std::clamp(patch.ymEnvelopeShape, 0, 4);
    if (choiceValue == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::coin:
            case MacroKind::jump:
                choiceValue = 1;
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                choiceValue = 4;
                break;
            case MacroKind::powerUp:
                choiceValue = 3;
                break;
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::arp:
            case MacroKind::laser:
            case MacroKind::manual:
            default:
                choiceValue = 2;
                break;
        }
    }

    FmEnvelopeRegisters envelope;
    switch (choiceValue)
    {
        case 1: // Pluck
            envelope = { 0x1fu, 0x12u, 0x06u, 0x84u };
            break;
        case 3: // Pad
            envelope = { 0x14u, 0x06u, 0x00u, 0x45u };
            break;
        case 4: // Perc
            envelope = { 0x1fu, 0x1au, 0x0eu, 0x2fu };
            break;
        case 2: // Lead
        default:
            envelope = { 0x1fu, 0x06u, 0x00u, 0x24u };
            break;
    }

    if (op < 3u)
    {
        envelope.sustainRelease = static_cast<uint8_t>(std::min(0xff, static_cast<int>(envelope.sustainRelease) + 0x10));
        envelope.decayRate = static_cast<uint8_t>(std::max(0, static_cast<int>(envelope.decayRate) - 2));
    }

    return envelope;
}

uint8_t ym2151AlgorithmForPatch(const PatchConfig& patch)
{
    if (patch.waveShape > 0)
        return static_cast<uint8_t>(std::clamp(patch.waveShape - 1, 0, 7));

    switch (patch.macro)
    {
        case MacroKind::bass: return 0;
        case MacroKind::lead: return 4;
        case MacroKind::arp: return 7;
        case MacroKind::coin:
        case MacroKind::jump: return 6;
        case MacroKind::drum:
        case MacroKind::hit: return 2;
        case MacroKind::laser: return 3;
        case MacroKind::powerUp: return 5;
        case MacroKind::manual:
        default: break;
    }

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control1) * 7.0f)), 0, 7));
}

uint8_t fmFeedbackForPatch(const PatchConfig& patch)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control2) * 7.0f)), 0, 7));
}

uint8_t fmOperatorMultipleForPatch(ChipMode mode, const PatchConfig& patch, size_t op)
{
    if (mode == ChipMode::ym2151)
    {
        static constexpr std::array<int, 4> offsets { 0, 1, 3, 5 };
        const auto base = std::clamp(static_cast<int>(std::round(clampControl(patch.control3) * 13.0f)) + 1, 1, 15);
        return static_cast<uint8_t>(std::clamp(base + offsets[std::min(op, size_t { 3u })], 1, 15));
    }

    static constexpr std::array<int, 4> offsets { 0, 1, 2, 4 };
    const auto tone = std::clamp(static_cast<int>(std::round(clampControl(patch.control3) * 14.0f)) + 1, 1, 15);
    return static_cast<uint8_t>(std::clamp(tone + offsets[std::min(op, size_t { 3u })], 1, 15));
}

bool fmOperatorIsCarrierForAlgorithm(uint8_t algorithm, size_t op)
{
    static constexpr std::array<std::array<bool, 4>, 8> carriers {{
        {{ false, false, false, true  }},
        {{ false, false, false, true  }},
        {{ false, false, false, true  }},
        {{ false, false, false, true  }},
        {{ false, true,  false, true  }},
        {{ false, true,  true,  true  }},
        {{ false, true,  true,  true  }},
        {{ true,  true,  true,  true  }}
    }};

    return carriers[algorithm & 0x07u][std::min(op, size_t { 3u })];
}

uint8_t fmOperatorTotalLevelForPatch(ChipMode mode, const PatchConfig& patch, size_t op, float velocity)
{
    const auto level = clampControl(velocity) * clampControl(patch.control4);
    if (mode == ChipMode::ym2151)
    {
        const auto carrier = static_cast<int>(std::round((1.0f - level) * 24.0f));
        const auto modulator = static_cast<int>(std::round(16.0f + (1.0f - clampControl(patch.control3)) * 52.0f));
        return static_cast<uint8_t>(std::clamp(fmOperatorIsCarrierForAlgorithm(ym2151AlgorithmForPatch(patch), op) ? carrier : modulator, 0, 127));
    }

    const auto carrier = static_cast<int>(std::round((1.0f - level) * 28.0f));
    const auto modulator = static_cast<int>(std::round(18.0f + (1.0f - clampControl(patch.control3)) * 40.0f));
    return static_cast<uint8_t>(std::clamp(fmOperatorIsCarrierForAlgorithm(ym2612AlgorithmForPatch(patch), op) ? carrier : modulator, 0, 127));
}

uint8_t oplWaveformForPatch(const PatchConfig& patch)
{
    if (patch.waveShape > 0)
        return static_cast<uint8_t>(std::clamp(patch.waveShape - 1, 0, 3));

    switch (patch.macro)
    {
        case MacroKind::bass: return 0;
        case MacroKind::lead: return 3;
        case MacroKind::arp: return 1;
        case MacroKind::coin:
        case MacroKind::jump: return 2;
        case MacroKind::drum:
        case MacroKind::hit:
        case MacroKind::laser: return 3;
        case MacroKind::powerUp: return 1;
        case MacroKind::manual:
        default: break;
    }

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control3) * 3.0f)), 0, 3));
}

uint8_t oplConnectionForPatch(const PatchConfig& patch)
{
    return patch.control1 > 0.55f ? 1u : 0u;
}

uint8_t oplModulatorMultipleForPatch(const PatchConfig& patch)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control3) * 14.0f)) + 1, 1, 15));
}

uint8_t oplModulatorTotalLevelForPatch(const PatchConfig& patch)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(12.0f + (1.0f - clampControl(patch.control3)) * 46.0f)), 0, 63));
}

uint8_t oplCarrierTotalLevelForPatch(const PatchConfig& patch, float velocity)
{
    const auto level = clampControl(velocity) * clampControl(patch.control4);
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - level) * 24.0f)), 0, 63));
}

uint8_t oplRhythmModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.ymEnvelopeShape, 0, 2);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    return (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit) ? 2u : 1u;
}

uint8_t ym2413InstrumentForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.waveShape, 0, 15);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    switch (patch.macro)
    {
        case MacroKind::coin:
        case MacroKind::jump: return 12;
        case MacroKind::bass: return 13;
        case MacroKind::lead: return 7;
        case MacroKind::arp:
        case MacroKind::powerUp: return 8;
        case MacroKind::drum:
        case MacroKind::hit: return 15;
        case MacroKind::laser: return 10;
        case MacroKind::manual:
        default: break;
    }

    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control1) * 15.0f)), 1, 15));
}

uint8_t ym2413RhythmModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.ymEnvelopeShape, 0, 2);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    return (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit) ? 2u : 1u;
}

uint8_t ym2413VolumeNibbleForPatch(const PatchConfig& patch, size_t channel, float velocity)
{
    const auto trim = channel < patch.sourceLevels.size() ? clampControl(patch.sourceLevels[channel]) : 1.0f;
    const auto musicalLevel = clampControl(patch.control4) * clampControl(velocity) * trim;
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - musicalLevel) * 15.0f)), 0, 15));
}

uint8_t huc6280ControlForPatch(const PatchConfig& patch, size_t)
{
    const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 31.0f)), 1, 31));
    return static_cast<uint8_t>(0x80u | volume);
}

uint8_t wavetableWaveShapeForChannel(ChipMode mode, const PatchConfig& patch, size_t channel)
{
    const std::array<int, 8> choices {
        patch.waveShape,
        patch.sidVoice2WaveShape,
        patch.sidVoice3WaveShape,
        patch.pulse2Duty,
        patch.dmgWaveLevel,
        patch.snNoiseMode,
        patch.ymEnvelopeShape,
        patch.dmgStereoRoute
    };

    const auto channelCount = mode == ChipMode::namcoWsg ? size_t { 8u }
        : (mode == ChipMode::huc6280 ? size_t { 6u }
        : (mode == ChipMode::scc ? size_t { 5u }
        : (mode == ChipMode::paula ? size_t { 4u } : size_t { 1u })));
    auto baseChoice = std::clamp(patch.waveShape, 0, 4);
    if (mode == ChipMode::paula && baseChoice == 0)
    {
        switch (patch.macro)
        {
            case MacroKind::bass: baseChoice = 2; break;
            case MacroKind::drum:
            case MacroKind::hit: baseChoice = 4; break;
            case MacroKind::lead:
            case MacroKind::arp: baseChoice = 1; break;
            default: baseChoice = 3; break;
        }
    }
    auto choice = choices[std::min(channel, channelCount - 1u)];
    choice = std::clamp(choice, 0, 4);
    return static_cast<uint8_t>(choice == 0 ? baseChoice : choice);
}

uint8_t huc6280WaveShapeForChannel(const PatchConfig& patch, size_t channel)
{
    return wavetableWaveShapeForChannel(ChipMode::huc6280, patch, channel);
}

bool huc6280ChannelUsesNoiseForPatch(const PatchConfig& patch, size_t channel)
{
    const auto usesNoiseTemplate = patch.macro == MacroKind::drum || patch.macro == MacroKind::hit || huc6280WaveShapeForChannel(patch, channel) == 4u;
    return usesNoiseTemplate && channel >= 4u;
}

uint8_t huc6280LfoModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.dmgStereoRoute, 0, 4);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    switch (patch.macro)
    {
        case MacroKind::lead:
        case MacroKind::arp:
            return 2;
        case MacroKind::laser:
        case MacroKind::powerUp:
            return 3;
        case MacroKind::coin:
        case MacroKind::jump:
            return 4;
        case MacroKind::manual:
        case MacroKind::bass:
        case MacroKind::drum:
        case MacroKind::hit:
        default:
            break;
    }

    return 1;
}

uint8_t sccVolumeForPatch(const PatchConfig& patch, size_t channel)
{
    const auto base = std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 15.0f)), 1, 15);
    const auto trim = channel < 4u ? clampControl(patch.sourceLevels[channel]) : 0.85f;
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(base) * trim)), 0, 15));
}

bool sccChannelKeyOnForPatch(const PatchConfig& patch, size_t channel)
{
    if (channel < 4u)
        return patch.sourceEnabled[channel];

    return patch.macro == MacroKind::arp
        || patch.macro == MacroKind::powerUp
        || patch.macro == MacroKind::hit;
}

uint8_t namcoWsgVolumeForPatch(const PatchConfig& patch, size_t channel)
{
    const auto base = std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 15.0f)), 1, 15);
    const auto trim = channel < 4u ? clampControl(patch.sourceLevels[channel]) : 0.80f;
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(base) * trim)), 0, 15));
}

bool namcoWsgChannelEnabledForPatch(const PatchConfig& patch, size_t channel)
{
    if (channel < 4u)
        return patch.sourceEnabled[channel];

    return patch.macro == MacroKind::arp
        || patch.macro == MacroKind::hit
        || patch.macro == MacroKind::laser
        || patch.macro == MacroKind::powerUp;
}

uint8_t wavetableRamSampleForPatch(ChipMode mode, const PatchConfig& patch, size_t channel, size_t sampleIndex)
{
    constexpr auto twoPiLocal = 6.283185307179586476925286766559;
    const auto choice = mode == ChipMode::huc6280 || mode == ChipMode::namcoWsg || mode == ChipMode::scc
                            ? static_cast<int>(wavetableWaveShapeForChannel(mode, patch, channel))
                            : std::clamp(patch.waveShape, 0, 4);
    const auto i = sampleIndex & 31u;
    const auto phaseValue = static_cast<double>(i) / 32.0;
    const auto skew = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);

    if (mode == ChipMode::huc6280)
    {
        auto sample = 0;
        switch (choice)
        {
            case 1: sample = static_cast<int>(std::round(31.0 * phaseValue)); break;
            case 2: sample = i < 16u ? static_cast<int>(std::round(31.0 * (static_cast<double>(i) / 15.0))) : static_cast<int>(std::round(31.0 * (1.0 - static_cast<double>(i - 16u) / 15.0))); break;
            case 3: sample = i < 16u ? 31 : 0; break;
            case 4: sample = ((static_cast<int>(i) * 13 + static_cast<int>(channel) * 7) & 31); break;
            case 0:
            default: sample = static_cast<int>(std::round(15.5 + 15.5 * std::sin(twoPiLocal * phaseValue))); break;
        }
        return static_cast<uint8_t>(std::clamp(sample, 0, 31));
    }

    if (mode == ChipMode::namcoWsg)
    {
        auto sample = 8;
        switch (choice)
        {
            case 1: sample = static_cast<int>(std::round(15.0 * phaseValue)); break;
            case 2: sample = i < 16u ? static_cast<int>(std::round(15.0 * (static_cast<double>(i) / 15.0))) : static_cast<int>(std::round(15.0 * (1.0 - static_cast<double>(i - 16u) / 15.0))); break;
            case 3: sample = i < static_cast<size_t>(std::round(4.0 + skew * 24.0)) ? 15 : 0; break;
            case 4: sample = ((static_cast<int>(i) * 5 + static_cast<int>(channel) * 3) & 15); break;
            case 0:
            default: sample = static_cast<int>(std::round(7.5 + 7.5 * std::sin(twoPiLocal * phaseValue))); break;
        }
        return static_cast<uint8_t>(std::clamp(sample, 0, 15));
    }

    if (mode == ChipMode::scc)
    {
        auto sample = 128;
        switch (choice)
        {
            case 1: sample = static_cast<int>(std::round(255.0 * phaseValue)); break;
            case 2: sample = i < 16u ? static_cast<int>(std::round(255.0 * (static_cast<double>(i) / 15.0))) : static_cast<int>(std::round(255.0 * (1.0 - static_cast<double>(i - 16u) / 15.0))); break;
            case 3: sample = i < static_cast<size_t>(std::round(4.0 + skew * 24.0)) ? 255 : 0; break;
            case 4: sample = ((static_cast<int>(i) * 17 + static_cast<int>(channel) * 29) & 255); break;
            case 0:
            default: sample = static_cast<int>(std::round(128.0 + 127.0 * std::sin(twoPiLocal * phaseValue))); break;
        }
        return static_cast<uint8_t>(std::clamp(sample, 0, 255));
    }

    return 0;
}

uint8_t sampleTemplateForPatch(ChipMode mode, const PatchConfig& patch)
{
    const auto choiceValue = std::clamp(patch.waveShape, 0, 4);
    if (choiceValue > 0)
        return static_cast<uint8_t>(choiceValue);

    if (mode == ChipMode::spc700 || mode == ChipMode::paula)
    {
        switch (patch.macro)
        {
            case MacroKind::bass: return 2;
            case MacroKind::drum:
            case MacroKind::hit: return 4;
            case MacroKind::lead:
            case MacroKind::arp: return 1;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::laser:
            case MacroKind::jump:
            case MacroKind::powerUp:
            default: return 3;
        }
    }

    return 0;
}

int8_t generatedSampleValueForPatch(ChipMode mode, const PatchConfig& patch, size_t channel, size_t sampleIndex)
{
    constexpr auto twoPiLocal = 6.283185307179586476925286766559;
    const auto choice = mode == ChipMode::paula
        ? wavetableWaveShapeForChannel(mode, patch, channel)
        : sampleTemplateForPatch(mode, patch);
    const auto i = sampleIndex & 63u;
    const auto phaseValue = static_cast<double>(i) / 64.0;
    auto sample = 0.0;

    if (mode == ChipMode::spc700)
    {
        switch (choice)
        {
            case 1: sample = std::sin(twoPiLocal * phaseValue) * 0.75 + (((i / 8u) & 1u) ? 0.12 : -0.12); break;
            case 2: sample = phaseValue < 0.5 ? (phaseValue * 4.0) - 1.0 : 3.0 - (phaseValue * 4.0); break;
            case 3: sample = phaseValue < 0.5 ? 0.78 : -0.78; break;
            case 4:
            {
                const auto hash = (static_cast<int>(i) * 1664525u + static_cast<int>(channel) * 1013904223u) & 0xffffu;
                const auto burst = std::exp(-phaseValue * 7.0);
                sample = ((static_cast<double>(hash) / 32767.5) - 1.0) * burst;
                break;
            }
            default: sample = std::sin(twoPiLocal * phaseValue); break;
        }
    }
    else if (mode == ChipMode::paula)
    {
        switch (choice)
        {
            case 1: sample = (phaseValue * 2.0) - 1.0; break;
            case 2: sample = phaseValue < 0.5 ? (phaseValue * 4.0) - 1.0 : 3.0 - (phaseValue * 4.0); break;
            case 3: sample = std::sin(twoPiLocal * phaseValue); break;
            case 4:
            {
                const auto hash = (static_cast<int>(i) * 1103515245u + static_cast<int>(channel) * 12345u) & 0xffffu;
                const auto burst = std::exp(-phaseValue * 6.0);
                sample = ((static_cast<double>(hash) / 32767.5) - 1.0) * burst;
                break;
            }
            default: sample = phaseValue < 0.5 ? 0.85 : -0.85; break;
        }
    }

    return static_cast<int8_t>(std::clamp(static_cast<int>(std::round(std::clamp(sample, -1.0, 1.0) * 127.0)), -127, 127));
}

uint8_t spc700SamplePlaybackModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.dmgStereoRoute, 0, 2);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    switch (patch.macro)
    {
        case MacroKind::coin:
        case MacroKind::drum:
        case MacroKind::hit:
        case MacroKind::jump:
            return 2;

        case MacroKind::bass:
        case MacroKind::lead:
        case MacroKind::arp:
        case MacroKind::laser:
        case MacroKind::powerUp:
        case MacroKind::manual:
        default:
            return 1;
    }
}

bool spc700SampleLoopsForPatch(const PatchConfig& patch)
{
    return spc700SamplePlaybackModeForPatch(patch) == 1u;
}

uint8_t spc700VoiceVolumeForPatch(const PatchConfig& patch, size_t voice, float velocity)
{
    const auto base = std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 127.0f)), 1, 127);
    const auto trim = voice < patch.sourceLevels.size() ? clampControl(patch.sourceLevels[voice]) : 1.0f;
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(base) * trim * clampControl(velocity))), 0, 127));
}

uint8_t spc700EnvelopeShapeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.ymEnvelopeShape, 0, 4);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    switch (patch.macro)
    {
        case MacroKind::coin:
        case MacroKind::jump:
            return 1u;
        case MacroKind::powerUp:
            return 3u;
        case MacroKind::drum:
        case MacroKind::hit:
            return 4u;
        case MacroKind::bass:
        case MacroKind::lead:
        case MacroKind::arp:
        case MacroKind::laser:
        case MacroKind::manual:
        default:
            return 2u;
    }
}

uint8_t spc700AdsrForPatch(const PatchConfig& patch)
{
    const auto speed = std::clamp(static_cast<int>(std::round(clampControl(patch.envelopeDecay) * 15.0f)), 0, 15);
    auto attack = 12;
    auto decay = 3;

    switch (spc700EnvelopeShapeForPatch(patch))
    {
        case 1: // Pluck.
            attack = 15;
            decay = 6;
            break;
        case 3: // Pad.
            attack = std::clamp(5 + speed / 3, 5, 10);
            decay = 2;
            break;
        case 4: // Perc.
            attack = 15;
            decay = 7;
            break;
        case 2: // Lead.
        default:
            attack = std::clamp(11 + speed / 4, 11, 15);
            decay = 3;
            break;
    }

    return static_cast<uint8_t>(0x80u | ((decay & 0x07) << 4u) | (attack & 0x0f));
}

uint8_t spc700GainForPatch(const PatchConfig& patch)
{
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 127.0f)), 0, 127));
}

bool spc700VoiceEnabledForPatch(const PatchConfig& patch, size_t voice)
{
    if (voice < patch.sourceEnabled.size())
        return patch.sourceEnabled[voice];

    return patch.macro == MacroKind::arp
        || patch.macro == MacroKind::hit
        || patch.macro == MacroKind::laser
        || patch.macro == MacroKind::powerUp;
}

uint8_t paulaChannelVolumeForPatch(const PatchConfig& patch, size_t channel, float velocity)
{
    const auto trim = channel < patch.sourceLevels.size() ? clampControl(patch.sourceLevels[channel]) : 1.0f;
    const auto base = std::clamp(static_cast<int>(std::round(clampControl(patch.control4) * 64.0f)), 1, 64);
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(base) * trim * clampControl(velocity))), 0, 64));
}

bool paulaLoopForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.dmgStereoRoute, 0, 2);
    if (explicitChoice > 0)
        return explicitChoice == 1;

    return patch.macro != MacroKind::drum && patch.macro != MacroKind::hit && patch.control3 >= 0.25f;
}

uint8_t paulaControlForPatch(const PatchConfig& patch, size_t channel, float velocity)
{
    const auto shouldEnable = channel < patch.sourceEnabled.size() ? patch.sourceEnabled[channel] : false;
    const auto volume = paulaChannelVolumeForPatch(patch, channel, velocity);
    return static_cast<uint8_t>((shouldEnable && volume > 0 ? 0x01u : 0x00u) | (paulaLoopForPatch(patch) ? 0x02u : 0x00u));
}

uint8_t paulaOutputFilterModeForPatch(const PatchConfig& patch)
{
    const auto explicitChoice = std::clamp(patch.snNoiseMode, 0, 4);
    if (explicitChoice > 0)
        return static_cast<uint8_t>(explicitChoice);

    switch (patch.macro)
    {
        case MacroKind::bass:
            return 3;
        case MacroKind::arp:
        case MacroKind::powerUp:
            return 2;
        case MacroKind::manual:
        case MacroKind::coin:
        case MacroKind::lead:
        case MacroKind::drum:
        case MacroKind::hit:
        case MacroKind::laser:
        case MacroKind::jump:
        default:
            return 1;
    }
}

} // namespace chipper
