#include "Engine/ChipCore.h"
#include "Engine/ChipDescriptors.h"
#include "Engine/ControlRegistry.h"
#include "Presets.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{

enum class MetadataMode
{
    render,
    listDescriptors,
    describeChip
};

struct Options
{
    chipper::ChipMode chip = chipper::ChipMode::nes;
    chipper::AccuracyMode accuracy = chipper::AccuracyMode::hybrid;
    chipper::MacroKind macro = chipper::MacroKind::manual;
    chipper::PlayMode playMode = chipper::PlayMode::stack;
    MetadataMode metadataMode = MetadataMode::render;
    std::string presetId;
    float control1 = 0.5f;
    float control2 = 0.5f;
    float control3 = 0.5f;
    float control4 = 0.5f;
    std::array<bool, 4> controlProvided {};
    std::array<bool, 4> sourceEnabled { true, true, true, true };
    std::array<bool, 4> sourceProvided {};
    std::array<float, 4> sourceLevels { 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<bool, 4> sourceLevelProvided {};
    float stereoSpread = 0.0f;
    bool stereoSpreadProvided = false;
    float envelopeDecay = 0.0f;
    int sidAttack = 0;
    int sidDecay = 0;
    int sidSustain = 0;
    int sidRelease = 0;
    int sidVoice2Attack = 0;
    int sidVoice2Decay = 0;
    int sidVoice2Sustain = 0;
    int sidVoice2Release = 0;
    int sidVoice3Attack = 0;
    int sidVoice3Decay = 0;
    int sidVoice3Sustain = 0;
    int sidVoice3Release = 0;
    int waveShape = 0;
    int sidVoice2WaveShape = 0;
    int sidVoice3WaveShape = 0;
    int pulse2Duty = 0;
    int dmgWaveLevel = 0;
    int dmgStereoRoute = 0;
    int ymEnvelopeShape = 0;
    int ymChannelAMix = 0;
    int ymChannelBMix = 0;
    int ymChannelCMix = 0;
    int snNoiseMode = 0;
    int sidFilterRouting = 0;
    bool envelopeDecayProvided = false;
    bool sidAttackProvided = false;
    bool sidDecayProvided = false;
    bool sidSustainProvided = false;
    bool sidReleaseProvided = false;
    bool sidVoice2AttackProvided = false;
    bool sidVoice2DecayProvided = false;
    bool sidVoice2SustainProvided = false;
    bool sidVoice2ReleaseProvided = false;
    bool sidVoice3AttackProvided = false;
    bool sidVoice3DecayProvided = false;
    bool sidVoice3SustainProvided = false;
    bool sidVoice3ReleaseProvided = false;
    bool waveShapeProvided = false;
    bool sidVoice2WaveShapeProvided = false;
    bool sidVoice3WaveShapeProvided = false;
    bool pulse2DutyProvided = false;
    bool dmgWaveLevelProvided = false;
    bool dmgStereoRouteProvided = false;
    bool ymEnvelopeShapeProvided = false;
    bool ymChannelAMixProvided = false;
    bool ymChannelBMixProvided = false;
    bool ymChannelCMixProvided = false;
    bool snNoiseModeProvided = false;
    bool sidFilterRoutingProvided = false;
    float outputDb = 0.0f;
    double clock = 1789773.0;
    double sampleRate = 48000.0;
    double seconds = 1.0;
    int note = 69;
    std::filesystem::path eventFile;
    std::filesystem::path wavPath = "chipper-render.wav";
    std::filesystem::path debugPath = "chipper-render.json";
};

enum class EventType
{
    write,
    noteOff,
    noteOn
};

struct ScheduledEvent
{
    EventType type = EventType::write;
    uint64_t sample = 0;
    uint16_t address = 0;
    uint8_t value = 0;
    int note = 60;
    float velocity = 1.0f;
};

template <typename T>
bool parseNumber(const std::string& text, T& out)
{
    if constexpr (std::is_integral_v<T>)
    {
        int base = 10;
        std::string_view view(text);
        if (view.size() > 2 && view[0] == '0' && (view[1] == 'x' || view[1] == 'X'))
        {
            base = 16;
            view.remove_prefix(2);
        }

        T value {};
        const auto* first = view.data();
        const auto* last = view.data() + view.size();
        const auto result = std::from_chars(first, last, value, base);
        if (result.ec != std::errc() || result.ptr != last)
            return false;
        out = value;
        return true;
    }
    else
    {
        char* end = nullptr;
        const auto value = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0')
            return false;
        out = static_cast<T>(value);
        return true;
    }
}

std::string normalizedToken(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    text.erase(std::remove_if(text.begin(), text.end(), [](char c) { return c == '_' || c == '-' || c == ' '; }), text.end());
    return text;
}

bool parseWaveShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "ram" || key == "manual" || key == "trace" || key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "tri" || key == "triangle")
        out = 1;
    else if (key == "saw" || key == "ramp")
        out = 2;
    else if (key == "pulse" || key == "square")
        out = 3;
    else if (key == "steps" || key == "step" || key == "noise")
        out = 4;
    else
        return false;

    return true;
}

bool parseYmEnvelopeShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "fixed" || key == "off" || key == "volume" || key == "macro" || key == "default")
        out = 0;
    else if (key == "fall" || key == "decay" || key == "fallhold")
        out = 1;
    else if (key == "rise" || key == "attack" || key == "risehold")
        out = 2;
    else if (key == "saw" || key == "sawdown")
        out = 3;
    else if (key == "triangle" || key == "tri")
        out = 4;
    else
        return false;

    return true;
}

bool parseSidFilterMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 8);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "lp" || key == "lowpass")
        out = 1;
    else if (key == "bp" || key == "bandpass")
        out = 2;
    else if (key == "hp" || key == "highpass")
        out = 3;
    else if (key == "off" || key == "none" || key == "bypass" || key == "filteroff")
        out = 4;
    else if (key == "notch" || key == "br" || key == "bandreject" || key == "lphp" || key == "lp+hp")
        out = 5;
    else if (key == "lpbp" || key == "lp+bp" || key == "lowpassbandpass")
        out = 6;
    else if (key == "bphp" || key == "bp+hp" || key == "bandpasshighpass")
        out = 7;
    else if (key == "all" || key == "lpbphp" || key == "lp+bp+hp")
        out = 8;
    else
        return false;

    return true;
}

bool parseSidAdsrNibbleChoice(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        if (numeric <= 15u)
        {
            out = static_cast<int>(numeric) + 1;
            return true;
        }

        return false;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
    {
        out = 0;
        return true;
    }

    return false;
}

bool parseDmgWaveLevel(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        if (numeric == 100)
            out = 2;
        else if (numeric == 50)
            out = 3;
        else if (numeric == 25)
            out = 4;
        else
            out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "mute" || key == "off" || key == "0" || key == "0percent")
        out = 1;
    else if (key == "100" || key == "100percent" || key == "full")
        out = 2;
    else if (key == "50" || key == "50percent" || key == "half")
        out = 3;
    else if (key == "25" || key == "25percent" || key == "quarter")
        out = 4;
    else
        return false;

    return true;
}

bool parsePulse2Duty(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        if (numeric == 12)
            out = 1;
        else if (numeric == 25)
            out = 2;
        else if (numeric == 50)
            out = 3;
        else if (numeric == 75)
            out = 4;
        else
            out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "125" || key == "12.5" || key == "12_5" || key == "thin" || key == "eighth")
        out = 1;
    else if (key == "25" || key == "narrow" || key == "quarter")
        out = 2;
    else if (key == "50" || key == "square" || key == "half")
        out = 3;
    else if (key == "75" || key == "wide" || key == "threequarter" || key == "inverted")
        out = 4;
    else
        return false;

    return true;
}

bool parseDmgStereoRoute(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "both" || key == "center" || key == "mono" || key == "all")
        out = 1;
    else if (key == "left" || key == "l")
        out = 2;
    else if (key == "right" || key == "r")
        out = 3;
    else if (key == "split" || key == "wide" || key == "lr")
        out = 4;
    else
        return false;

    return true;
}

bool parseYmChannelMix(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "tone" || key == "toneonly" || key == "square")
        out = 1;
    else if (key == "noise" || key == "noiseonly")
        out = 2;
    else if (key == "both" || key == "tonenoise" || key == "toneandnoise")
        out = 3;
    else if (key == "off" || key == "mute" || key == "silent")
        out = 4;
    else
        return false;

    return true;
}

bool parseSnNoiseMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "off")
        out = 0;
    else if (key == "long" || key == "longlfsr" || key == "wide" || key == "15bit" || key == "15bitlfsr" || key == "periodic" || key == "periodiclo" || key == "periodiclow" || key == "plo" || key == "p0")
        out = 1;
    else if (key == "short" || key == "shortloop" || key == "shortlfsr" || key == "narrow" || key == "7bit" || key == "7bitlfsr" || key == "periodichi" || key == "periodichigh" || key == "phi" || key == "p2")
        out = 2;
    else if (key == "whitelo" || key == "whitelow" || key == "wlo" || key == "white")
        out = 3;
    else if (key == "whitet3" || key == "tone3" || key == "wt3")
        out = 4;
    else
        return false;

    return true;
}

bool parseSidModMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "off" || key == "none" || key == "clear")
        out = 1;
    else if (key == "sync" || key == "hardsync")
        out = 2;
    else if (key == "ring" || key == "ringmod" || key == "ringmodulation")
        out = 3;
    else if (key == "both" || key == "syncring" || key == "ringsync" || key == "syncandring")
        out = 4;
    else
        return false;

    return true;
}

bool parseSidModel(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        if (numeric == 6581)
            out = 1;
        else if (numeric == 8580)
            out = 2;
        else
            out = std::clamp(static_cast<int>(numeric), 0, 2);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "6581" || key == "mos6581" || key == "warm" || key == "rough")
        out = 1;
    else if (key == "8580" || key == "mos8580" || key == "clean" || key == "bright")
        out = 2;
    else
        return false;

    return true;
}

bool parseSidFilterRouting(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 8);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "auto" || key == "default")
        out = 0;
    else if (key == "all" || key == "123" || key == "v123" || key == "voices123")
        out = 1;
    else if (key == "v1" || key == "voice1" || key == "1")
        out = 2;
    else if (key == "v2" || key == "voice2" || key == "2")
        out = 3;
    else if (key == "v3" || key == "voice3" || key == "3")
        out = 4;
    else if (key == "v1v2" || key == "v1+v2" || key == "12" || key == "voices12")
        out = 5;
    else if (key == "v1v3" || key == "v1+v3" || key == "13" || key == "voices13")
        out = 6;
    else if (key == "v2v3" || key == "v2+v3" || key == "23" || key == "voices23")
        out = 7;
    else if (key == "none" || key == "off" || key == "bypass" || key == "dry")
        out = 8;
    else
        return false;

    return true;
}

void printUsage()
{
    std::cerr
        << "Usage: chipper_render --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --out out.wav --debug out.json [--events events.txt]\n"
        << "       Metadata: chipper_render --list-descriptors --debug descriptors.json\n"
        << "                 chipper_render --describe-chip nes --debug nes-descriptor.json\n"
        << "       Optional: --preset nes-hero-pulse --macro coin --play-mode chip-poly --control1 0.2 --control2 0.8 --control3 0.1 --control4 0.5 --source1 1 --source2 0 --level1 1.0 --level2 0.5 --stereo-spread 0.75 --envelope-decay 0.7 --sid-adsr-speed 0.7 --sid-attack macro|0..15 --sid-decay macro|0..15 --sid-sustain macro|0..15 --sid-release macro|0..15 --sid-voice2-attack macro|0..15 --sid-voice2-decay macro|0..15 --sid-voice2-sustain macro|0..15 --sid-voice2-release macro|0..15 --sid-voice3-attack macro|0..15 --sid-voice3-decay macro|0..15 --sid-voice3-sustain macro|0..15 --sid-voice3-release macro|0..15 --wave-shape macro|tri|saw|pulse|steps|noise --sid-voice2-wave macro|tri|saw|pulse|noise --sid-voice3-wave macro|tri|saw|pulse|noise --nes-pulse2-duty macro|12.5|25|50|75 --dmg-wave-level 100|50|25|mute|macro --dmg-stereo-route both|left|right|split|macro --ym-envelope-shape fixed|fall|rise|saw|triangle --ym-channel-a-mix macro|tone|noise|both|off --ym-channel-b-mix macro|tone|noise|both|off --ym-channel-c-mix macro|tone|noise|both|off --sid-filter-mode macro|lp|bp|hp|off|notch|lp+bp|bp+hp|all --sid-filter-routing macro|all|v1|v2|v3|v1+v2|v1+v3|v2+v3|none --sid-mod-mode macro|off|sync|ring|both --sid-model macro|6581|8580 --sn-noise-mode white-t3|long|short|15-bit|7-bit --output-db -9\n"
        << "\nEvent file lines:\n"
        << "  write <sample> <address> <value>\n"
        << "  note_on <sample> <note> <velocity>\n"
        << "  note_off <sample> <note>\n"
        << "  note <sample> <note> <velocity> <lengthSamples>\n"
        << "Addresses and values may be decimal or 0x-prefixed hex.\n";
}

void applyPreset(Options& options, const chipper::PresetInfo& preset)
{
    options.presetId = preset.id;
    options.chip = preset.chip;
    options.accuracy = preset.accuracy;
    options.macro = preset.macro;
    options.playMode = preset.playMode;
    options.control1 = preset.controls[0];
    options.control2 = preset.controls[1];
    options.control3 = preset.controls[2];
    options.control4 = preset.controls[3];
    options.controlProvided = { true, true, true, true };
    options.sourceEnabled = preset.sourceEnabled;
    options.sourceProvided = { true, true, true, true };
    options.sourceLevels = { preset.source1Level, preset.source2Level, preset.source3Level, preset.source4Level };
    options.sourceLevelProvided = { true, true, true, true };
    options.stereoSpread = preset.stereoSpread;
    options.stereoSpreadProvided = true;
    options.envelopeDecay = preset.envelopeDecay;
    options.sidAttack = preset.sidAttack;
    options.sidDecay = preset.sidDecay;
    options.sidSustain = preset.sidSustain;
    options.sidRelease = preset.sidRelease;
    options.sidVoice2Attack = preset.sidVoice2Attack;
    options.sidVoice2Decay = preset.sidVoice2Decay;
    options.sidVoice2Sustain = preset.sidVoice2Sustain;
    options.sidVoice2Release = preset.sidVoice2Release;
    options.sidVoice3Attack = preset.sidVoice3Attack;
    options.sidVoice3Decay = preset.sidVoice3Decay;
    options.sidVoice3Sustain = preset.sidVoice3Sustain;
    options.sidVoice3Release = preset.sidVoice3Release;
    options.waveShape = preset.waveShape;
    options.sidVoice2WaveShape = preset.sidVoice2WaveShape;
    options.sidVoice3WaveShape = preset.sidVoice3WaveShape;
    options.pulse2Duty = 0;
    options.dmgWaveLevel = 0;
    options.dmgStereoRoute = preset.dmgStereoRoute;
    options.ymEnvelopeShape = preset.ymEnvelopeShape;
    options.ymChannelAMix = 0;
    options.ymChannelBMix = 0;
    options.ymChannelCMix = 0;
    options.snNoiseMode = preset.snNoiseMode;
    options.sidFilterRouting = preset.sidFilterRouting;
    options.envelopeDecayProvided = true;
    options.sidAttackProvided = true;
    options.sidDecayProvided = true;
    options.sidSustainProvided = true;
    options.sidReleaseProvided = true;
    options.sidVoice2AttackProvided = true;
    options.sidVoice2DecayProvided = true;
    options.sidVoice2SustainProvided = true;
    options.sidVoice2ReleaseProvided = true;
    options.sidVoice3AttackProvided = true;
    options.sidVoice3DecayProvided = true;
    options.sidVoice3SustainProvided = true;
    options.sidVoice3ReleaseProvided = true;
    options.waveShapeProvided = true;
    options.sidVoice2WaveShapeProvided = true;
    options.sidVoice3WaveShapeProvided = true;
    options.pulse2DutyProvided = true;
    options.dmgWaveLevelProvided = true;
    options.dmgStereoRouteProvided = true;
    options.ymEnvelopeShapeProvided = true;
    options.ymChannelAMixProvided = true;
    options.ymChannelBMixProvided = true;
    options.ymChannelCMixProvided = true;
    options.snNoiseModeProvided = true;
    options.sidFilterRoutingProvided = true;
    options.outputDb = preset.outputDb;
    options.clock = preset.clockHz;
}

bool parseArgs(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto requireValue = [&](const char* name) -> const char*
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };
        auto parseToggle = [&](const char* name, bool& out) -> bool
        {
            const auto* value = requireValue(name);
            uint32_t parsed = 0;
            if (value == nullptr || ! parseNumber(std::string(value), parsed))
                return false;
            out = parsed != 0;
            return true;
        };

        if (arg == "--preset")
        {
            const auto* value = requireValue("--preset");
            if (value == nullptr)
                return false;
            const auto* preset = chipper::presetById(value);
            if (preset == nullptr)
            {
                std::cerr << "Unknown factory preset: " << value << "\n";
                return false;
            }
            applyPreset(options, *preset);
        }
        else if (arg == "--list-descriptors")
        {
            options.metadataMode = MetadataMode::listDescriptors;
        }
        else if (arg == "--describe")
        {
            options.metadataMode = MetadataMode::describeChip;
        }
        else if (arg == "--describe-chip")
        {
            const auto* value = requireValue("--describe-chip");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseChipMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown chip mode: " << value << "\n";
                return false;
            }
            options.chip = *parsed;
            options.metadataMode = MetadataMode::describeChip;
        }
        else if (arg == "--chip")
        {
            const auto* value = requireValue("--chip");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseChipMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown chip mode: " << value << "\n";
                return false;
            }
            options.chip = *parsed;
        }
        else if (arg == "--accuracy")
        {
            const auto* value = requireValue("--accuracy");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseAccuracyMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown accuracy mode: " << value << "\n";
                return false;
            }
            options.accuracy = *parsed;
        }
        else if (arg == "--macro")
        {
            const auto* value = requireValue("--macro");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseMacroKind(value);
            if (! parsed)
            {
                std::cerr << "Unknown macro: " << value << "\n";
                return false;
            }
            options.macro = *parsed;
        }
        else if (arg == "--play-mode")
        {
            const auto* value = requireValue("--play-mode");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parsePlayMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown play mode: " << value << "\n";
                return false;
            }
            options.playMode = *parsed;
        }
        else if (arg == "--control1")
        {
            const auto* value = requireValue("--control1");
            if (value == nullptr || ! parseNumber(std::string(value), options.control1))
                return false;
            options.controlProvided[0] = true;
        }
        else if (arg == "--control2")
        {
            const auto* value = requireValue("--control2");
            if (value == nullptr || ! parseNumber(std::string(value), options.control2))
                return false;
            options.controlProvided[1] = true;
        }
        else if (arg == "--control3")
        {
            const auto* value = requireValue("--control3");
            if (value == nullptr || ! parseNumber(std::string(value), options.control3))
                return false;
            options.controlProvided[2] = true;
        }
        else if (arg == "--control4")
        {
            const auto* value = requireValue("--control4");
            if (value == nullptr || ! parseNumber(std::string(value), options.control4))
                return false;
            options.controlProvided[3] = true;
        }
        else if (arg == "--source1")
        {
            if (! parseToggle("--source1", options.sourceEnabled[0]))
                return false;
            options.sourceProvided[0] = true;
        }
        else if (arg == "--source2")
        {
            if (! parseToggle("--source2", options.sourceEnabled[1]))
                return false;
            options.sourceProvided[1] = true;
        }
        else if (arg == "--source3")
        {
            if (! parseToggle("--source3", options.sourceEnabled[2]))
                return false;
            options.sourceProvided[2] = true;
        }
        else if (arg == "--source4")
        {
            if (! parseToggle("--source4", options.sourceEnabled[3]))
                return false;
            options.sourceProvided[3] = true;
        }
        else if (arg == "--level1")
        {
            const auto* value = requireValue("--level1");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[0]))
                return false;
            options.sourceLevelProvided[0] = true;
        }
        else if (arg == "--level2")
        {
            const auto* value = requireValue("--level2");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[1]))
                return false;
            options.sourceLevelProvided[1] = true;
        }
        else if (arg == "--level3")
        {
            const auto* value = requireValue("--level3");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[2]))
                return false;
            options.sourceLevelProvided[2] = true;
        }
        else if (arg == "--level4")
        {
            const auto* value = requireValue("--level4");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[3]))
                return false;
            options.sourceLevelProvided[3] = true;
        }
        else if (arg == "--envelope-decay" || arg == "--sid-adsr-speed")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseNumber(std::string(value), options.envelopeDecay))
                return false;
            options.envelopeDecayProvided = true;
        }
        else if (arg == "--stereo-spread")
        {
            const auto* value = requireValue("--stereo-spread");
            if (value == nullptr || ! parseNumber(std::string(value), options.stereoSpread))
                return false;
            options.stereoSpreadProvided = true;
        }
        else if (arg == "--sid-attack")
        {
            const auto* value = requireValue("--sid-attack");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidAttack))
                return false;
            options.sidAttackProvided = true;
        }
        else if (arg == "--sid-decay")
        {
            const auto* value = requireValue("--sid-decay");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidDecay))
                return false;
            options.sidDecayProvided = true;
        }
        else if (arg == "--sid-sustain")
        {
            const auto* value = requireValue("--sid-sustain");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidSustain))
                return false;
            options.sidSustainProvided = true;
        }
        else if (arg == "--sid-release")
        {
            const auto* value = requireValue("--sid-release");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidRelease))
                return false;
            options.sidReleaseProvided = true;
        }
        else if (arg == "--sid-voice2-attack")
        {
            const auto* value = requireValue("--sid-voice2-attack");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice2Attack))
                return false;
            options.sidVoice2AttackProvided = true;
        }
        else if (arg == "--sid-voice2-decay")
        {
            const auto* value = requireValue("--sid-voice2-decay");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice2Decay))
                return false;
            options.sidVoice2DecayProvided = true;
        }
        else if (arg == "--sid-voice2-sustain")
        {
            const auto* value = requireValue("--sid-voice2-sustain");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice2Sustain))
                return false;
            options.sidVoice2SustainProvided = true;
        }
        else if (arg == "--sid-voice2-release")
        {
            const auto* value = requireValue("--sid-voice2-release");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice2Release))
                return false;
            options.sidVoice2ReleaseProvided = true;
        }
        else if (arg == "--sid-voice3-attack")
        {
            const auto* value = requireValue("--sid-voice3-attack");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice3Attack))
                return false;
            options.sidVoice3AttackProvided = true;
        }
        else if (arg == "--sid-voice3-decay")
        {
            const auto* value = requireValue("--sid-voice3-decay");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice3Decay))
                return false;
            options.sidVoice3DecayProvided = true;
        }
        else if (arg == "--sid-voice3-sustain")
        {
            const auto* value = requireValue("--sid-voice3-sustain");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice3Sustain))
                return false;
            options.sidVoice3SustainProvided = true;
        }
        else if (arg == "--sid-voice3-release")
        {
            const auto* value = requireValue("--sid-voice3-release");
            if (value == nullptr || ! parseSidAdsrNibbleChoice(std::string(value), options.sidVoice3Release))
                return false;
            options.sidVoice3ReleaseProvided = true;
        }
        else if (arg == "--wave-shape")
        {
            const auto* value = requireValue("--wave-shape");
            if (value == nullptr || ! parseWaveShape(std::string(value), options.waveShape))
                return false;
            options.waveShapeProvided = true;
        }
        else if (arg == "--sid-voice2-wave")
        {
            const auto* value = requireValue("--sid-voice2-wave");
            if (value == nullptr || ! parseWaveShape(std::string(value), options.sidVoice2WaveShape))
                return false;
            options.sidVoice2WaveShapeProvided = true;
        }
        else if (arg == "--sid-voice3-wave")
        {
            const auto* value = requireValue("--sid-voice3-wave");
            if (value == nullptr || ! parseWaveShape(std::string(value), options.sidVoice3WaveShape))
                return false;
            options.sidVoice3WaveShapeProvided = true;
        }
        else if (arg == "--nes-pulse2-duty")
        {
            const auto* value = requireValue("--nes-pulse2-duty");
            if (value == nullptr || ! parsePulse2Duty(std::string(value), options.pulse2Duty))
                return false;
            options.pulse2DutyProvided = true;
        }
        else if (arg == "--dmg-wave-level")
        {
            const auto* value = requireValue("--dmg-wave-level");
            if (value == nullptr || ! parseDmgWaveLevel(std::string(value), options.dmgWaveLevel))
                return false;
            options.dmgWaveLevelProvided = true;
        }
        else if (arg == "--dmg-stereo-route")
        {
            const auto* value = requireValue("--dmg-stereo-route");
            if (value == nullptr || ! parseDmgStereoRoute(std::string(value), options.dmgStereoRoute))
                return false;
            options.dmgStereoRouteProvided = true;
        }
        else if (arg == "--sid-model")
        {
            const auto* value = requireValue("--sid-model");
            if (value == nullptr || ! parseSidModel(std::string(value), options.dmgStereoRoute))
                return false;
            options.dmgStereoRouteProvided = true;
        }
        else if (arg == "--ym-envelope-shape")
        {
            const auto* value = requireValue("--ym-envelope-shape");
            if (value == nullptr || ! parseYmEnvelopeShape(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
        }
        else if (arg == "--ym-channel-a-mix")
        {
            const auto* value = requireValue("--ym-channel-a-mix");
            if (value == nullptr || ! parseYmChannelMix(std::string(value), options.ymChannelAMix))
                return false;
            options.ymChannelAMixProvided = true;
        }
        else if (arg == "--ym-channel-b-mix")
        {
            const auto* value = requireValue("--ym-channel-b-mix");
            if (value == nullptr || ! parseYmChannelMix(std::string(value), options.ymChannelBMix))
                return false;
            options.ymChannelBMixProvided = true;
        }
        else if (arg == "--ym-channel-c-mix")
        {
            const auto* value = requireValue("--ym-channel-c-mix");
            if (value == nullptr || ! parseYmChannelMix(std::string(value), options.ymChannelCMix))
                return false;
            options.ymChannelCMixProvided = true;
        }
        else if (arg == "--sid-filter-mode")
        {
            const auto* value = requireValue("--sid-filter-mode");
            if (value == nullptr || ! parseSidFilterMode(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
        }
        else if (arg == "--sid-mod-mode")
        {
            const auto* value = requireValue("--sid-mod-mode");
            if (value == nullptr || ! parseSidModMode(std::string(value), options.snNoiseMode))
                return false;
            options.snNoiseModeProvided = true;
        }
        else if (arg == "--sid-filter-routing")
        {
            const auto* value = requireValue("--sid-filter-routing");
            if (value == nullptr || ! parseSidFilterRouting(std::string(value), options.sidFilterRouting))
                return false;
            options.sidFilterRoutingProvided = true;
        }
        else if (arg == "--sn-noise-mode")
        {
            const auto* value = requireValue("--sn-noise-mode");
            if (value == nullptr || ! parseSnNoiseMode(std::string(value), options.snNoiseMode))
                return false;
            options.snNoiseModeProvided = true;
        }
        else if (arg == "--output-db")
        {
            const auto* value = requireValue("--output-db");
            if (value == nullptr || ! parseNumber(std::string(value), options.outputDb))
                return false;
        }
        else if (arg == "--clock")
        {
            const auto* value = requireValue("--clock");
            if (value == nullptr || ! parseNumber(std::string(value), options.clock))
                return false;
        }
        else if (arg == "--rate")
        {
            const auto* value = requireValue("--rate");
            if (value == nullptr || ! parseNumber(std::string(value), options.sampleRate))
                return false;
        }
        else if (arg == "--seconds")
        {
            const auto* value = requireValue("--seconds");
            if (value == nullptr || ! parseNumber(std::string(value), options.seconds))
                return false;
        }
        else if (arg == "--note")
        {
            const auto* value = requireValue("--note");
            if (value == nullptr || ! parseNumber(std::string(value), options.note))
                return false;
        }
        else if (arg == "--events")
        {
            const auto* value = requireValue("--events");
            if (value == nullptr)
                return false;
            options.eventFile = value;
        }
        else if (arg == "--out")
        {
            const auto* value = requireValue("--out");
            if (value == nullptr)
                return false;
            options.wavPath = value;
        }
        else if (arg == "--debug")
        {
            const auto* value = requireValue("--debug");
            if (value == nullptr)
                return false;
            options.debugPath = value;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return false;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }

    return options.sampleRate > 0.0 && options.clock >= 0.0 && options.seconds > 0.0;
}

void applyMacroTemplateDefaults(Options& options)
{
    const auto& templ = chipper::macroTemplateFor(options.chip, options.macro);
    std::array<float*, 4> controls { &options.control1, &options.control2, &options.control3, &options.control4 };
    for (size_t i = 0; i < controls.size(); ++i)
    {
        if (! options.controlProvided[i])
            *controls[i] = templ.controls[i];
        if (! options.sourceProvided[i])
            options.sourceEnabled[i] = templ.sourceEnabled[i];
        if (! options.sourceLevelProvided[i])
            options.sourceLevels[i] = 1.0f;
    }

    if (! options.envelopeDecayProvided)
        options.envelopeDecay = templ.envelopeDecay;
    if (! options.sidAttackProvided)
        options.sidAttack = 0;
    if (! options.sidDecayProvided)
        options.sidDecay = 0;
    if (! options.sidSustainProvided)
        options.sidSustain = 0;
    if (! options.sidReleaseProvided)
        options.sidRelease = 0;
    if (! options.sidVoice2AttackProvided)
        options.sidVoice2Attack = 0;
    if (! options.sidVoice2DecayProvided)
        options.sidVoice2Decay = 0;
    if (! options.sidVoice2SustainProvided)
        options.sidVoice2Sustain = 0;
    if (! options.sidVoice2ReleaseProvided)
        options.sidVoice2Release = 0;
    if (! options.sidVoice3AttackProvided)
        options.sidVoice3Attack = 0;
    if (! options.sidVoice3DecayProvided)
        options.sidVoice3Decay = 0;
    if (! options.sidVoice3SustainProvided)
        options.sidVoice3Sustain = 0;
    if (! options.sidVoice3ReleaseProvided)
        options.sidVoice3Release = 0;
    if (! options.stereoSpreadProvided)
        options.stereoSpread = templ.stereoSpread;
    if (! options.waveShapeProvided)
        options.waveShape = templ.waveShape;
    if (! options.sidVoice2WaveShapeProvided)
        options.sidVoice2WaveShape = 0;
    if (! options.sidVoice3WaveShapeProvided)
        options.sidVoice3WaveShape = 0;
    if (! options.pulse2DutyProvided)
        options.pulse2Duty = 0;
    if (! options.dmgWaveLevelProvided)
        options.dmgWaveLevel = 0;
    if (! options.dmgStereoRouteProvided)
        options.dmgStereoRoute = templ.dmgStereoRoute;
    if (! options.ymEnvelopeShapeProvided)
        options.ymEnvelopeShape = templ.ymEnvelopeShape;
    if (! options.ymChannelAMixProvided)
        options.ymChannelAMix = 0;
    if (! options.ymChannelBMixProvided)
        options.ymChannelBMix = 0;
    if (! options.ymChannelCMixProvided)
        options.ymChannelCMix = 0;
    if (! options.snNoiseModeProvided)
        options.snNoiseMode = templ.snNoiseMode;
    if (! options.sidFilterRoutingProvided)
        options.sidFilterRouting = 0;
}

std::vector<ScheduledEvent> loadEvents(const std::filesystem::path& path)
{
    std::vector<ScheduledEvent> events;
    if (path.empty())
        return events;

    std::ifstream in(path);
    if (! in)
        throw std::runtime_error("Could not open event file: " + path.string());

    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line))
    {
        ++lineNumber;
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream parts(line);
        std::string kind;
        std::string sampleText;
        parts >> kind >> sampleText;

        ScheduledEvent event;
        if (! parseNumber(sampleText, event.sample))
            throw std::runtime_error("Bad event sample on line " + std::to_string(lineNumber));

        if (kind == "write")
        {
            std::string addressText;
            std::string valueText;
            uint32_t address = 0;
            uint32_t value = 0;
            parts >> addressText >> valueText;
            if (! parseNumber(addressText, address) || ! parseNumber(valueText, value))
                throw std::runtime_error("Bad write event value on line " + std::to_string(lineNumber));

            event.type = EventType::write;
            event.address = static_cast<uint16_t>(address & 0xffffu);
            event.value = static_cast<uint8_t>(value & 0xffu);
            events.push_back(event);
        }
        else if (kind == "note_on")
        {
            std::string noteText;
            std::string velocityText;
            parts >> noteText >> velocityText;
            if (! parseNumber(noteText, event.note) || ! parseNumber(velocityText, event.velocity))
                throw std::runtime_error("Bad note_on event value on line " + std::to_string(lineNumber));

            event.type = EventType::noteOn;
            events.push_back(event);
        }
        else if (kind == "note_off")
        {
            std::string noteText;
            parts >> noteText;
            if (! parseNumber(noteText, event.note))
                throw std::runtime_error("Bad note_off event value on line " + std::to_string(lineNumber));

            event.type = EventType::noteOff;
            events.push_back(event);
        }
        else if (kind == "note")
        {
            std::string noteText;
            std::string velocityText;
            std::string lengthText;
            uint64_t lengthSamples = 0;
            parts >> noteText >> velocityText >> lengthText;
            if (! parseNumber(noteText, event.note) || ! parseNumber(velocityText, event.velocity) || ! parseNumber(lengthText, lengthSamples))
                throw std::runtime_error("Bad note event value on line " + std::to_string(lineNumber));

            event.type = EventType::noteOn;
            events.push_back(event);

            ScheduledEvent off = event;
            off.type = EventType::noteOff;
            off.sample += lengthSamples;
            off.velocity = 0.0f;
            events.push_back(off);
        }
        else
        {
            throw std::runtime_error("Unsupported event kind on line " + std::to_string(lineNumber));
        }
    }

    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b)
    {
        if (a.sample != b.sample)
            return a.sample < b.sample;
        return static_cast<int>(a.type) < static_cast<int>(b.type);
    });
    return events;
}

void writeU16(std::ofstream& out, uint16_t value)
{
    out.put(static_cast<char>(value & 0xffu));
    out.put(static_cast<char>((value >> 8u) & 0xffu));
}

void writeU32(std::ofstream& out, uint32_t value)
{
    writeU16(out, static_cast<uint16_t>(value & 0xffffu));
    writeU16(out, static_cast<uint16_t>((value >> 16u) & 0xffffu));
}

void writeWav(const std::filesystem::path& path, const std::vector<chipper::StereoFrame>& frames, double sampleRate)
{
    std::ofstream out(path, std::ios::binary);
    if (! out)
        throw std::runtime_error("Could not write WAV: " + path.string());

    const auto dataBytes = static_cast<uint32_t>(frames.size() * 2u * sizeof(int16_t));
    out.write("RIFF", 4);
    writeU32(out, 36u + dataBytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 2);
    writeU32(out, static_cast<uint32_t>(std::round(sampleRate)));
    writeU32(out, static_cast<uint32_t>(std::round(sampleRate)) * 2u * sizeof(int16_t));
    writeU16(out, 2u * sizeof(int16_t));
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, dataBytes);

    for (const auto& frame : frames)
    {
        const auto left = static_cast<int16_t>(std::round(std::clamp(frame.left, -1.0f, 1.0f) * 32767.0f));
        const auto right = static_cast<int16_t>(std::round(std::clamp(frame.right, -1.0f, 1.0f) * 32767.0f));
        writeU16(out, static_cast<uint16_t>(left));
        writeU16(out, static_cast<uint16_t>(right));
    }
}

chipper::RenderStats calculateStats(const std::vector<chipper::StereoFrame>& frames)
{
    chipper::RenderStats stats;
    double energy = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    float previous = 0.0f;
    bool hasPrevious = false;

    for (const auto& frame : frames)
    {
        const auto mono = (frame.left + frame.right) * 0.5f;
        stats.peak = std::max(stats.peak, static_cast<double>(std::abs(mono)));
        stats.leftPeak = std::max(stats.leftPeak, static_cast<double>(std::abs(frame.left)));
        stats.rightPeak = std::max(stats.rightPeak, static_cast<double>(std::abs(frame.right)));
        energy += static_cast<double>(mono) * static_cast<double>(mono);
        leftEnergy += static_cast<double>(frame.left) * static_cast<double>(frame.left);
        rightEnergy += static_cast<double>(frame.right) * static_cast<double>(frame.right);

        if (hasPrevious && ((previous <= 0.0f && mono > 0.0f) || (previous >= 0.0f && mono < 0.0f)))
            ++stats.zeroCrossings;

        previous = mono;
        hasPrevious = true;
    }

    stats.renderedSamples = static_cast<uint64_t>(frames.size());
    stats.rms = frames.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(frames.size()));
    stats.leftRms = frames.empty() ? 0.0 : std::sqrt(leftEnergy / static_cast<double>(frames.size()));
    stats.rightRms = frames.empty() ? 0.0 : std::sqrt(rightEnergy / static_cast<double>(frames.size()));
    return stats;
}

float decibelsToGain(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

void applyOutputGain(std::vector<chipper::StereoFrame>& frames, float outputDb)
{
    const auto gain = decibelsToGain(outputDb);
    for (auto& frame : frames)
    {
        frame.left = std::clamp(frame.left * gain, -1.0f, 1.0f);
        frame.right = std::clamp(frame.right * gain, -1.0f, 1.0f);
    }
}

std::string jsonEscaped(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size() + 2u);
    for (const auto c : text)
    {
        switch (c)
        {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                escaped += c;
                break;
        }
    }

    return escaped;
}

std::ostream& writeJsonString(std::ostream& out, std::string_view text)
{
    out << "\"" << jsonEscaped(text) << "\"";
    return out;
}

const char* toJsonString(chipper::ParameterKind kind)
{
    switch (kind)
    {
        case chipper::ParameterKind::continuous: return "continuous";
        case chipper::ParameterKind::discreteChoice: return "discreteChoice";
        case chipper::ParameterKind::steppedNumeric: return "steppedNumeric";
        case chipper::ParameterKind::booleanToggle: return "booleanToggle";
        case chipper::ParameterKind::macro: return "macro";
        case chipper::ParameterKind::chipRegister: return "chipRegister";
        case chipper::ParameterKind::hiddenInternal: return "hiddenInternal";
    }

    return "continuous";
}

const char* toJsonString(chipper::ControlSurface surface)
{
    switch (surface)
    {
        case chipper::ControlSurface::slider: return "slider";
        case chipper::ControlSurface::segmentedChoice: return "segmentedChoice";
        case chipper::ControlSurface::sourceCards: return "sourceCards";
        case chipper::ControlSurface::toggle: return "toggle";
        case chipper::ControlSurface::menu: return "menu";
        case chipper::ControlSurface::hidden: return "hidden";
    }

    return "slider";
}

const char* toJsonString(chipper::ChipParameterRole role)
{
    switch (role)
    {
        case chipper::ChipParameterRole::macroControl1: return "macroControl1";
        case chipper::ChipParameterRole::macroControl2: return "macroControl2";
        case chipper::ChipParameterRole::macroControl3: return "macroControl3";
        case chipper::ChipParameterRole::macroControl4: return "macroControl4";
        case chipper::ChipParameterRole::source1Enabled: return "source1Enabled";
        case chipper::ChipParameterRole::source2Enabled: return "source2Enabled";
        case chipper::ChipParameterRole::source3Enabled: return "source3Enabled";
        case chipper::ChipParameterRole::source4Enabled: return "source4Enabled";
        case chipper::ChipParameterRole::source1Level: return "source1Level";
        case chipper::ChipParameterRole::source2Level: return "source2Level";
        case chipper::ChipParameterRole::source3Level: return "source3Level";
        case chipper::ChipParameterRole::source4Level: return "source4Level";
        case chipper::ChipParameterRole::stereoSpread: return "stereoSpread";
        case chipper::ChipParameterRole::sidFilterRouting: return "sidFilterRouting";
        case chipper::ChipParameterRole::envelopeDecay: return "envelopeDecay";
        case chipper::ChipParameterRole::sidAttack: return "sidAttack";
        case chipper::ChipParameterRole::sidDecay: return "sidDecay";
        case chipper::ChipParameterRole::sidSustain: return "sidSustain";
        case chipper::ChipParameterRole::sidRelease: return "sidRelease";
        case chipper::ChipParameterRole::sidVoice2Attack: return "sidVoice2Attack";
        case chipper::ChipParameterRole::sidVoice2Decay: return "sidVoice2Decay";
        case chipper::ChipParameterRole::sidVoice2Sustain: return "sidVoice2Sustain";
        case chipper::ChipParameterRole::sidVoice2Release: return "sidVoice2Release";
        case chipper::ChipParameterRole::sidVoice3Attack: return "sidVoice3Attack";
        case chipper::ChipParameterRole::sidVoice3Decay: return "sidVoice3Decay";
        case chipper::ChipParameterRole::sidVoice3Sustain: return "sidVoice3Sustain";
        case chipper::ChipParameterRole::sidVoice3Release: return "sidVoice3Release";
        case chipper::ChipParameterRole::waveShape: return "waveShape";
        case chipper::ChipParameterRole::sidVoice2WaveShape: return "sidVoice2WaveShape";
        case chipper::ChipParameterRole::sidVoice3WaveShape: return "sidVoice3WaveShape";
        case chipper::ChipParameterRole::pulse2Duty: return "pulse2Duty";
        case chipper::ChipParameterRole::dmgWaveLevel: return "dmgWaveLevel";
        case chipper::ChipParameterRole::dmgStereoRoute: return "dmgStereoRoute";
        case chipper::ChipParameterRole::ymEnvelopeShape: return "ymEnvelopeShape";
        case chipper::ChipParameterRole::ymChannelAMix: return "ymChannelAMix";
        case chipper::ChipParameterRole::ymChannelBMix: return "ymChannelBMix";
        case chipper::ChipParameterRole::ymChannelCMix: return "ymChannelCMix";
        case chipper::ChipParameterRole::snNoiseMode: return "snNoiseMode";
        case chipper::ChipParameterRole::clockHz: return "clockHz";
        case chipper::ChipParameterRole::outputDb: return "outputDb";
    }

    return "macroControl1";
}

const char* chipModeKey(chipper::ChipMode mode)
{
    switch (mode)
    {
        case chipper::ChipMode::nes: return "nes";
        case chipper::ChipMode::dmg: return "dmg";
        case chipper::ChipMode::sid: return "sid";
        case chipper::ChipMode::ym2149: return "ym2149";
        case chipper::ChipMode::sn76489: return "sn76489";
        case chipper::ChipMode::ym2612: return "ym2612";
        case chipper::ChipMode::opl3: return "opl3";
        case chipper::ChipMode::spc700: return "spc700";
        case chipper::ChipMode::pokey: return "pokey";
        case chipper::ChipMode::paula: return "paula";
        case chipper::ChipMode::huc6280: return "huc6280";
        case chipper::ChipMode::namcoWsg: return "namcoWsg";
        case chipper::ChipMode::ym2151: return "ym2151";
        case chipper::ChipMode::ym2413: return "ym2413";
        case chipper::ChipMode::scc: return "scc";
        case chipper::ChipMode::arcade: return "arcade";
        case chipper::ChipMode::custom: return "custom";
    }

    return "custom";
}

void writeDescriptorJson(std::ostream& out, chipper::ChipMode mode)
{
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto presets = chipper::presetsForChip(mode);

    out << "{\n"
        << "    \"displayName\": ";
    writeJsonString(out, descriptor.displayName);
    out << ",\n"
        << "    \"summary\": ";
    writeJsonString(out, descriptor.summary);
    out << ",\n"
        << "    \"implemented\": " << (descriptor.implemented ? "true" : "false") << ",\n"
        << "    \"supportsChipPoly\": " << (descriptor.supportsChipPoly ? "true" : "false") << ",\n"
        << "    \"macros\": [\n";

    for (size_t i = 0; i < descriptor.macros.size(); ++i)
    {
        const auto& macro = descriptor.macros[i];
        out << "      { \"kind\": ";
        writeJsonString(out, chipper::toString(macro.macro));
        out << ", \"label\": ";
        writeJsonString(out, macro.label);
        out << ", \"help\": ";
        writeJsonString(out, macro.help);
        out << ", \"controls\": ["
            << macro.controls[0] << ", "
            << macro.controls[1] << ", "
            << macro.controls[2] << ", "
            << macro.controls[3] << "], \"sourceEnabled\": ["
            << (macro.sourceEnabled[0] ? "true" : "false") << ", "
            << (macro.sourceEnabled[1] ? "true" : "false") << ", "
            << (macro.sourceEnabled[2] ? "true" : "false") << ", "
            << (macro.sourceEnabled[3] ? "true" : "false") << "], \"envelopeDecay\": "
            << macro.envelopeDecay << ", \"waveShape\": "
            << macro.waveShape << ", \"ymEnvelopeShape\": "
            << macro.ymEnvelopeShape << ", \"snNoiseMode\": "
            << macro.snNoiseMode << ", \"dmgStereoRoute\": "
            << macro.dmgStereoRoute << ", \"stereoSpread\": "
            << macro.stereoSpread << " }"
            << (i + 1u == descriptor.macros.size() ? "\n" : ",\n");
    }

    out << "    ],\n"
        << "    \"parameters\": [\n";

    for (size_t i = 0; i < descriptor.parameters.size(); ++i)
    {
        const auto& parameter = descriptor.parameters[i];
        const auto* parameterId = chipper::parameterIdForChipParameterRole(parameter.role);
        const auto midiCc = parameterId == nullptr ? -1 : chipper::midiControllerForParameterId(parameterId);
        out << "      {\n"
            << "        \"role\": ";
        writeJsonString(out, toJsonString(parameter.role));
        out << ",\n"
            << "        \"parameterId\": ";
        if (parameterId != nullptr)
            writeJsonString(out, parameterId);
        else
            out << "null";
        out << ",\n"
            << "        \"midiCc\": " << midiCc << ",\n"
            << "        \"id\": ";
        writeJsonString(out, parameter.id);
        out << ",\n"
            << "        \"label\": ";
        writeJsonString(out, parameter.label);
        out << ",\n"
            << "        \"group\": ";
        writeJsonString(out, parameter.group);
        out << ",\n"
            << "        \"help\": ";
        writeJsonString(out, parameter.help);
        out << ",\n"
            << "        \"kind\": ";
        writeJsonString(out, toJsonString(parameter.kind));
        out << ",\n"
            << "        \"surface\": ";
        writeJsonString(out, toJsonString(parameter.surface));
        out << ",\n"
            << "        \"minValue\": " << parameter.minValue << ",\n"
            << "        \"maxValue\": " << parameter.maxValue << ",\n"
            << "        \"defaultValue\": " << parameter.defaultValue << ",\n"
            << "        \"automatable\": " << (parameter.automatable ? "true" : "false") << ",\n"
            << "        \"choices\": [\n";

        for (size_t choiceIndex = 0; choiceIndex < parameter.choices.size(); ++choiceIndex)
        {
            const auto& choice = parameter.choices[choiceIndex];
            out << "          { \"label\": ";
            writeJsonString(out, choice.label);
            out << ", \"help\": ";
            writeJsonString(out, choice.help);
            out << ", \"normalizedValue\": " << choice.normalizedValue
                << ", \"choiceValue\": " << choice.choiceValue << " }"
                << (choiceIndex + 1u == parameter.choices.size() ? "\n" : ",\n");
        }

        out << "        ]\n"
            << "      }" << (i + 1u == descriptor.parameters.size() ? "\n" : ",\n");
    }

    out << "    ],\n"
        << "    \"presets\": [\n";

    for (size_t i = 0; i < presets.size(); ++i)
    {
        const auto& preset = *presets[i];
        out << "      { \"id\": ";
        writeJsonString(out, preset.id);
        out << ", \"category\": ";
        writeJsonString(out, preset.category);
        out << ", \"name\": ";
        writeJsonString(out, preset.name);
        out << " }" << (i + 1u == presets.size() ? "\n" : ",\n");
    }

    out << "    ]\n"
        << "  }";
}

bool includesMode(const std::vector<chipper::ChipMode>& modes, chipper::ChipMode mode)
{
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

void writePresetCatalogJson(std::ostream& out, const std::vector<chipper::ChipMode>& modes)
{
    const auto& presets = chipper::presetCatalog();
    out << "  \"presets\": [\n";
    bool wrotePreset = false;
    for (const auto& preset : presets)
    {
        if (! includesMode(modes, preset.chip))
            continue;

        if (wrotePreset)
            out << ",\n";

        out << "    {\n"
            << "      \"id\": ";
        writeJsonString(out, preset.id);
        out << ",\n"
            << "      \"category\": ";
        writeJsonString(out, preset.category);
        out << ",\n"
            << "      \"name\": ";
        writeJsonString(out, preset.name);
        out << ",\n"
            << "      \"note\": ";
        writeJsonString(out, preset.note);
        out << ",\n"
            << "      \"chipKey\": ";
        writeJsonString(out, chipModeKey(preset.chip));
        out << ",\n"
            << "      \"chip\": ";
        writeJsonString(out, chipper::toString(preset.chip));
        out << ",\n"
            << "      \"accuracy\": ";
        writeJsonString(out, chipper::toString(preset.accuracy));
        out << ",\n"
            << "      \"macro\": ";
        writeJsonString(out, chipper::toString(preset.macro));
        out << ",\n"
            << "      \"playMode\": ";
        writeJsonString(out, chipper::toString(preset.playMode));
        out << ",\n"
            << "      \"controls\": [" << preset.controls[0] << ", "
            << preset.controls[1] << ", "
            << preset.controls[2] << ", "
            << preset.controls[3] << "],\n"
            << "      \"sourceEnabled\": ["
            << (preset.sourceEnabled[0] ? "true" : "false") << ", "
            << (preset.sourceEnabled[1] ? "true" : "false") << ", "
            << (preset.sourceEnabled[2] ? "true" : "false") << ", "
            << (preset.sourceEnabled[3] ? "true" : "false") << "],\n"
            << "      \"sourceLevels\": [" << preset.source1Level << ", "
            << preset.source2Level << ", "
            << preset.source3Level << ", "
            << preset.source4Level << "],\n"
            << "      \"envelopeDecay\": " << preset.envelopeDecay << ",\n"
            << "      \"waveShape\": " << preset.waveShape << ",\n"
            << "      \"sidVoice2WaveShape\": " << preset.sidVoice2WaveShape << ",\n"
            << "      \"sidVoice3WaveShape\": " << preset.sidVoice3WaveShape << ",\n"
            << "      \"sidFilterRouting\": " << preset.sidFilterRouting << ",\n"
            << "      \"sidAdsr\": ["
            << preset.sidAttack << ", "
            << preset.sidDecay << ", "
            << preset.sidSustain << ", "
            << preset.sidRelease << ", "
            << preset.sidVoice2Attack << ", "
            << preset.sidVoice2Decay << ", "
            << preset.sidVoice2Sustain << ", "
            << preset.sidVoice2Release << ", "
            << preset.sidVoice3Attack << ", "
            << preset.sidVoice3Decay << ", "
            << preset.sidVoice3Sustain << ", "
            << preset.sidVoice3Release << "],\n"
            << "      \"dmgStereoRoute\": " << preset.dmgStereoRoute << ",\n"
            << "      \"ymEnvelopeShape\": " << preset.ymEnvelopeShape << ",\n"
            << "      \"snNoiseMode\": " << preset.snNoiseMode << ",\n"
            << "      \"outputDb\": " << preset.outputDb << ",\n"
            << "      \"clockHz\": " << preset.clockHz << ",\n"
            << "      \"stereoSpread\": " << preset.stereoSpread << "\n"
            << "    }";
        wrotePreset = true;
    }
    if (wrotePreset)
        out << "\n";
    out << "  ]\n";
}

void writeMidiCcMappingJson(std::ostream& out)
{
    const auto& mappings = chipper::midiCcMappings();
    out << "  \"midiCcMappings\": [\n";
    for (size_t i = 0; i < mappings.size(); ++i)
    {
        const auto& mapping = mappings[i];
        out << "    {\n"
            << "      \"cc\": " << mapping.controller << ",\n"
            << "      \"parameterId\": ";
        writeJsonString(out, mapping.parameterId != nullptr ? mapping.parameterId : "");
        out << ",\n"
            << "      \"label\": ";
        writeJsonString(out, mapping.label != nullptr ? mapping.label : "");
        out << "\n"
            << "    }" << (i + 1u == mappings.size() ? "\n" : ",\n");
    }
    out << "  ]";
}

void writeDescriptorListJson(const std::filesystem::path& path, const std::vector<chipper::ChipMode>& modes)
{
    std::ofstream out(path);
    if (! out)
        throw std::runtime_error("Could not write descriptor JSON: " + path.string());

    out << "{\n"
        << "  \"schema\": \"chipper.descriptors.v1\",\n"
        << "  \"descriptors\": [\n";

    for (size_t i = 0; i < modes.size(); ++i)
    {
        const auto mode = modes[i];
        out << "    {\n"
            << "      \"chipKey\": ";
        writeJsonString(out, chipModeKey(mode));
        out << ",\n"
            << "      \"chip\": ";
        writeJsonString(out, chipper::toString(mode));
        out << ",\n"
            << "      \"descriptor\": ";
        writeDescriptorJson(out, mode);
        out << "\n"
            << "    }" << (i + 1u == modes.size() ? "\n" : ",\n");
    }

    out << "  ],\n";
    writePresetCatalogJson(out, modes);
    out << ",\n";
    writeMidiCcMappingJson(out);
    out << "}\n";
}

void writeDebugJson(const std::filesystem::path& path,
                    const Options& options,
                    const chipper::PatchConfig& patch,
                    const chipper::ChipCore& core,
                    const chipper::RenderStats& stats,
                    size_t registerWriteCount,
                    size_t noteEventCount)
{
    std::ofstream out(path);
    if (! out)
        throw std::runtime_error("Could not write debug JSON: " + path.string());

    out << "{\n"
        << "  \"chip\": \"" << chipper::toString(options.chip) << "\",\n"
        << "  \"preset\": \"" << options.presetId << "\",\n"
        << "  \"requestedAccuracy\": \"" << chipper::toString(options.accuracy) << "\",\n"
        << "  \"macro\": \"" << chipper::toString(patch.macro) << "\",\n"
        << "  \"playMode\": \"" << chipper::toString(patch.playMode) << "\",\n"
        << "  \"requestedPlayMode\": \"" << chipper::toString(options.playMode) << "\",\n"
        << "  \"implementedAccuracy\": \"" << core.implementedAccuracy() << "\",\n"
        << "  \"clockHz\": " << options.clock << ",\n"
        << "  \"sampleRate\": " << options.sampleRate << ",\n"
        << "  \"seconds\": " << options.seconds << ",\n"
        << "  \"note\": " << options.note << ",\n"
        << "  \"outputDb\": " << options.outputDb << ",\n"
        << "  \"outputGain\": " << decibelsToGain(options.outputDb) << ",\n"
        << "  \"stereoSpread\": " << patch.stereoSpread << ",\n"
        << "  \"sidFilterRouting\": " << patch.sidFilterRouting << ",\n"
        << "  \"pulse2Duty\": " << patch.pulse2Duty << ",\n"
        << "  \"dmgStereoRoute\": " << patch.dmgStereoRoute << ",\n"
        << "  \"ymChannelAMix\": " << patch.ymChannelAMix << ",\n"
        << "  \"ymChannelBMix\": " << patch.ymChannelBMix << ",\n"
        << "  \"ymChannelCMix\": " << patch.ymChannelCMix << ",\n"
        << "  \"sidVoice2WaveShape\": " << patch.sidVoice2WaveShape << ",\n"
        << "  \"sidVoice3WaveShape\": " << patch.sidVoice3WaveShape << ",\n"
        << "  \"sidAttack\": " << patch.sidAttack << ",\n"
        << "  \"sidDecay\": " << patch.sidDecay << ",\n"
        << "  \"sidSustain\": " << patch.sidSustain << ",\n"
        << "  \"sidRelease\": " << patch.sidRelease << ",\n"
        << "  \"sidVoice2Attack\": " << patch.sidVoice2Attack << ",\n"
        << "  \"sidVoice2Decay\": " << patch.sidVoice2Decay << ",\n"
        << "  \"sidVoice2Sustain\": " << patch.sidVoice2Sustain << ",\n"
        << "  \"sidVoice2Release\": " << patch.sidVoice2Release << ",\n"
        << "  \"sidVoice3Attack\": " << patch.sidVoice3Attack << ",\n"
        << "  \"sidVoice3Decay\": " << patch.sidVoice3Decay << ",\n"
        << "  \"sidVoice3Sustain\": " << patch.sidVoice3Sustain << ",\n"
        << "  \"sidVoice3Release\": " << patch.sidVoice3Release << ",\n"
        << "  \"registerWriteCount\": " << registerWriteCount << ",\n"
        << "  \"noteEventCount\": " << noteEventCount << ",\n"
        << "  \"renderedSamples\": " << stats.renderedSamples << ",\n"
        << "  \"peak\": " << stats.peak << ",\n"
        << "  \"rms\": " << stats.rms << ",\n"
        << "  \"leftPeak\": " << stats.leftPeak << ",\n"
        << "  \"rightPeak\": " << stats.rightPeak << ",\n"
        << "  \"leftRms\": " << stats.leftRms << ",\n"
        << "  \"rightRms\": " << stats.rightRms << ",\n"
        << "  \"zeroCrossings\": " << stats.zeroCrossings << ",\n"
        << "  \"descriptor\": ";
    writeDescriptorJson(out, options.chip);
    out << ",\n"
        << "  \"coreState\": " << core.debugStateJson() << "\n"
        << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        Options options;
        if (! parseArgs(argc, argv, options))
        {
            printUsage();
            return 2;
        }

        if (options.metadataMode != MetadataMode::render)
        {
            const auto modes = options.metadataMode == MetadataMode::listDescriptors
                ? chipper::chipModeOrder()
                : std::vector<chipper::ChipMode> { options.chip };
            writeDescriptorListJson(options.debugPath, modes);
            std::cout << "Wrote descriptor metadata to " << options.debugPath.string() << "\n";
            return 0;
        }

        applyMacroTemplateDefaults(options);
        auto core = chipper::createChipCore(options.chip, options.accuracy);
        core->reset(options.sampleRate, options.clock);
        const auto patch = chipper::makePatchConfig(options.chip,
                                                    options.macro,
                                                    options.control1,
                                                    options.control2,
                                                    options.control3,
                                                    options.control4,
                                                    options.playMode,
                                                    options.sourceEnabled,
                                                    options.sourceLevels,
                                                    options.stereoSpread,
                                                    options.envelopeDecay,
                                                    options.waveShape,
                                                    options.pulse2Duty,
                                                    options.dmgWaveLevel,
                                                    options.dmgStereoRoute,
                                                    options.ymEnvelopeShape,
                                                    options.ymChannelAMix,
                                                    options.ymChannelBMix,
                                                    options.ymChannelCMix,
                                                    options.snNoiseMode,
                                                    options.sidVoice2WaveShape,
                                                    options.sidVoice3WaveShape,
                                                    options.sidAttack,
                                                    options.sidDecay,
                                                    options.sidSustain,
                                                    options.sidRelease,
                                                    options.sidVoice2Attack,
                                                    options.sidVoice2Decay,
                                                    options.sidVoice2Sustain,
                                                    options.sidVoice2Release,
                                                    options.sidVoice3Attack,
                                                    options.sidVoice3Decay,
                                                    options.sidVoice3Sustain,
                                                    options.sidVoice3Release,
                                                    options.sidFilterRouting);
        core->setPatch(patch);
        const auto events = loadEvents(options.eventFile);
        const auto registerWriteCount = static_cast<size_t>(std::count_if(events.begin(), events.end(), [](const auto& event) { return event.type == EventType::write; }));
        const auto noteEventCount = events.size() - registerWriteCount;

        const auto sampleCount = static_cast<uint64_t>(std::ceil(options.seconds * options.sampleRate));
        std::vector<chipper::StereoFrame> frames;
        frames.reserve(static_cast<size_t>(sampleCount));

        if (noteEventCount == 0)
            core->noteOn(options.note, 1.0f);

        size_t eventIndex = 0;
        for (uint64_t sample = 0; sample < sampleCount; ++sample)
        {
            while (eventIndex < events.size() && events[eventIndex].sample == sample)
            {
                const auto& event = events[eventIndex];
                if (event.type == EventType::write)
                    core->writeRegister(event.address, event.value);
                else if (event.type == EventType::noteOn)
                    core->noteOn(event.note, event.velocity);
                else if (event.type == EventType::noteOff)
                    core->noteOff(event.note);

                ++eventIndex;
            }

            frames.push_back(core->renderSample());
        }

        core->noteOff(options.note);
        applyOutputGain(frames, options.outputDb);
        const auto stats = calculateStats(frames);
        writeWav(options.wavPath, frames, options.sampleRate);
        writeDebugJson(options.debugPath, options, patch, *core, stats, registerWriteCount, noteEventCount);

        std::cout << "Rendered " << frames.size() << " samples to " << options.wavPath.string()
                  << " and " << options.debugPath.string() << "\n";
        std::cout << core->modeName() << " -> " << core->implementedAccuracy() << "\n";
        if (! core->limitations().empty())
            std::cout << "Limitations: " << core->limitations() << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "chipper_render failed: " << e.what() << "\n";
        return 1;
    }
}
