#include "Presets.h"

namespace chipper
{

const std::vector<PresetInfo>& presetCatalog()
{
    static const std::vector<PresetInfo> presets {
        { "NES Leads", "NES Hero Pulse", "Phase 1 core maps this to RP2A03 pulse registers." },
        { "NES Bass", "NES Triangle Bass", "Requires triangle linear-counter validation before Authentic label." },
        { "NES Drums", "NES Noise Snare", "Requires full noise/envelope regression coverage." },
        { "YM Arps", "YM Three-Voice Arp", "Maps to AY/YM tone periods and mixer bits." },
        { "SN76489 / Sega PSG", "Sega PSG Lead", "Maps to SN76489 tone and attenuation registers." },
        { "Genesis FM", "Genesis FM Bass", "Planned for ymfm-backed OPN2 integration after license audit." },
        { "DOS FM", "OPL Bell Lead", "Planned for ymfm-backed OPL integration after license audit." },
        { "SID Bass", "SID Dirty Bass", "Planned; not accurate until a permissive SID strategy exists." },
        { "Classic Game SFX", "Power-Up Rise", "Musical macro target; chip-specific register traces still needed." }
    };

    return presets;
}

} // namespace chipper
