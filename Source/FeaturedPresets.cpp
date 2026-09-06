#include "Presets.h"
#include <array>
#include <utility>

namespace chipper
{
std::string_view featuredPresetNote(std::string_view id) noexcept
{
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 16> sounds {{
        { "nes-hero-pulse", "Start with a monophonic melody; add the Major Motion template for an instant chip arpeggio." },
        { "nes-triangle-bass", "Play C2-C3 for a rounded low end beneath pulse leads." },
        { "nes-noise-snare", "Use short notes as backbeats; pair with triangle bass for a compact rhythm section." },
        { "dmg-wave-bass", "Try C2-C3, then draw a new table in Wave Lab to reshape the bass body." },
        { "dmg-pocket-arp", "Play a repeated single-note rhythm to hear the handheld pulse and wave stack." },
        { "sid-pwm-lead", "Hold notes while changing pulse width; the three voices supply the movement." },
        { "sid-filter-pluck", "Try short C3-C5 phrases and use the filter controls for brighter accents." },
        { "ym-noise-hat", "Open Motion Lab: alternate native noise periods 1 and 31 on Hold steps for two hat colors." },
        { "ym-envelope-bell", "Play sparse C4-C6 notes to leave room for the shared hardware envelope." },
        { "opn2-feedback-bass", "Start at C2-C3; reduce feedback to soften the FM edge." },
        { "opn2-electric-keys", "Try slow C3-C5 chords and compare carrier levels in the operator editor." },
        { "opl2-velvet-keys", "Play gently spaced C3-C5 chords for a softer two-operator FM texture." },
        { "spc700-echo-pad", "Hold C3-C5 notes and leave gaps so the echo remains clear." },
        { "spc700-muted-pluck", "Use short C3-C5 phrases for a rounded sample pluck." },
        { "paula-tracker-bass", "Use C2-C3 staccato lines to hear the looped 8-bit sample body." },
        { "paula-soft-pad", "Hold C3-C5 chords for sustained tracker texture; compare the output filter modes." },
    }};
    for (const auto& [key, note] : sounds)
        if (key == id) return note;
    return {};
}
} // namespace chipper
