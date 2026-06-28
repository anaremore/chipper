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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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
    listPresets,
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
    bool chipWasProvided = false;
    std::string presetId;
    float control1 = 0.5f;
    float control2 = 0.5f;
    float control3 = 0.5f;
    float control4 = 0.5f;
    std::array<bool, 4> controlProvided {};
    std::array<float, 4> fmOperatorLevels { 0.5f, 0.5f, 0.5f, 0.5f };
    std::array<bool, 4> fmOperatorLevelProvided {};
    std::array<bool, 9> sourceEnabled { true, true, true, true, true, true, true, true, true };
    std::array<bool, 9> sourceProvided {};
    std::array<float, 9> sourceLevels { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::array<bool, 9> sourceLevelProvided {};
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
    float sidVoice2PulseWidth = 0.5f;
    float sidVoice3PulseWidth = 0.5f;
    float nesDmcDirectLevel = 0.0f;
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
    bool sidVoice2PulseWidthProvided = false;
    bool sidVoice3PulseWidthProvided = false;
    bool nesDmcDirectLevelProvided = false;
    bool nesDmcRateIndexProvided = false;
    bool nesDmcLoop = false;
    bool nesDmcLoopProvided = false;
    bool nesDmcOnly = false;
    bool nesDmcOnlyProvided = false;
    float outputDb = 0.0f;
    double clock = 1789773.0;
    double sampleRate = 48000.0;
    double seconds = 1.0;
    int note = 69;
    std::filesystem::path eventFile;
    std::filesystem::path nesDmcSamplePath;
    std::filesystem::path spc700BrrSamplePath;
    std::filesystem::path paulaSamplePath;
    std::string spc700BrrHex;
    std::vector<std::string> spc700BrrBankHex;
    int spc700SampleSlot = 0;
    std::array<int, 8> voiceSampleSlots { 0, 0, 0, 0, 0, 0, 0, 0 };
    int spc700MapRoot = -1;
    float spc700LoopStart = 0.0f;
    float spc700LoopEnd = 1.0f;
    int nesDmcRateIndex = 15;
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

bool paulaSampleSlotIndexForArg(const std::string& arg, size_t& index)
{
    constexpr const char* prefix = "--paula-sample-slot";
    if (arg.rfind(prefix, 0) != 0)
        return false;

    uint32_t lane = 0;
    if (! parseNumber(arg.substr(std::strlen(prefix)), lane) || lane < 1u || lane > 4u)
        return false;

    index = static_cast<size_t>(lane - 1u);
    return true;
}

bool spc700VoiceSampleSlotIndexForArg(const std::string& arg, size_t& index)
{
    constexpr const char* prefix = "--spc700-sample-slot";
    if (arg.rfind(prefix, 0) != 0)
        return false;

    uint32_t voice = 0;
    if (! parseNumber(arg.substr(std::strlen(prefix)), voice) || voice < 1u || voice > 8u)
        return false;

    index = static_cast<size_t>(voice - 1u);
    return true;
}

bool fmOperatorLevelIndexForArg(const std::string& arg, size_t& index)
{
    const std::array<const char*, 3> prefixes {
        "--fm-op",
        "--opn2-op",
        "--opm-op"
    };

    for (const auto* prefix : prefixes)
    {
        if (arg.rfind(prefix, 0) != 0)
            continue;

        const auto suffix = arg.substr(std::strlen(prefix));
        const auto marker = suffix.find("-level");
        if (marker == std::string::npos || marker == 0 || marker + std::strlen("-level") != suffix.size())
            return false;

        uint32_t op = 0;
        if (! parseNumber(suffix.substr(0, marker), op) || op < 1u || op > 4u)
            return false;

        index = static_cast<size_t>(op - 1u);
        return true;
    }

    return false;
}

bool parseWaveShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 15);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "ram" || key == "manual" || key == "trace" || key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "tri" || key == "triangle")
        out = 1;
    else if (key == "saw" || key == "ramp")
        out = 2;
    else if (key == "pulse" || key == "square")
        out = 3;
    else if (key == "steps" || key == "step" || key == "noise")
        out = 4;
    else if (key == "pure")
        out = 1;
    else if (key == "poly4" || key == "pokeypoly4")
        out = 2;
    else if (key == "poly5" || key == "pokeypoly5")
        out = 3;
    else if (key == "poly17" || key == "poly9" || key == "pokeypoly17")
        out = 4;
    else if (key == "tri+saw" || key == "triangle+saw" || key == "trisaw" || key == "trianglesaw")
        out = 5;
    else if (key == "tri+pulse" || key == "triangle+pulse" || key == "trisquare" || key == "tripulse" || key == "trianglepulse")
        out = 6;
    else if (key == "saw+pulse" || key == "sawsquare" || key == "sawpulse")
        out = 7;
    else if (key == "tri+saw+pulse" || key == "triangle+saw+pulse" || key == "trisawpulse" || key == "trianglesawpulse")
        out = 8;
    else if (key == "violin" || key == "opllviolin")
        out = 1;
    else if (key == "guitar" || key == "opllguitar")
        out = 2;
    else if (key == "piano" || key == "opllpiano")
        out = 3;
    else if (key == "flute" || key == "opllflute")
        out = 4;
    else if (key == "clarinet" || key == "opllclarinet")
        out = 5;
    else if (key == "oboe" || key == "oplloboe")
        out = 6;
    else if (key == "trumpet" || key == "oplltrumpet")
        out = 7;
    else if (key == "organ" || key == "opllorgan")
        out = 8;
    else if (key == "horn" || key == "opllhorn")
        out = 9;
    else if (key == "synth" || key == "synthesizer" || key == "opllsynth")
        out = 10;
    else if (key == "harpsi" || key == "harpsichord" || key == "opllharpsi")
        out = 11;
    else if (key == "vibes" || key == "vibraphone" || key == "bell" || key == "opllvibes")
        out = 12;
    else if (key == "synthbass" || key == "opllsynthbass")
        out = 13;
    else if (key == "acbass" || key == "acousticbass" || key == "bass" || key == "opllacbass")
        out = 14;
    else if (key == "eguitar" || key == "electricguitar" || key == "oplleguitar")
        out = 15;
    else
        return false;

    return true;
}

bool parseHucWaveShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "ram" || key == "manual" || key == "trace" || key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "ramp" || key == "saw")
        out = 1;
    else if (key == "tri" || key == "triangle")
        out = 2;
    else if (key == "pulse" || key == "square")
        out = 3;
    else if (key == "noise" || key == "steps" || key == "step")
        out = 4;
    else
        return false;

    return true;
}

bool parseWavetableWaveShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "ram" || key == "manual" || key == "trace" || key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "ramp" || key == "saw")
        out = 1;
    else if (key == "tri" || key == "triangle")
        out = 2;
    else if (key == "pulse" || key == "square")
        out = 3;
    else if (key == "steps" || key == "step" || key == "noise")
        out = 4;
    else
        return false;

    return true;
}

bool parsePaulaSampleShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "ram" || key == "manual" || key == "trace" || key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "ramp" || key == "saw")
        out = 1;
    else if (key == "tri" || key == "triangle")
        out = 2;
    else if (key == "sine" || key == "sin")
        out = 3;
    else if (key == "noise")
        out = 4;
    else
        return false;

    return true;
}

bool parseYmEnvelopeShape(const std::string& text, int& out)
{
    if (text.size() > 2u && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        uint32_t code = 0;
        if (! parseNumber(text, code))
            return false;
        out = 5 + std::clamp(static_cast<int>(code), 0, 15);
        return true;
    }

    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 20);
        return true;
    }

    const auto key = normalizedToken(text);
    auto parseExactCode = [&out](const std::string& suffix) {
        if (suffix.empty())
            return false;

        auto view = std::string_view(suffix);
        auto base = 10;
        if (view.size() > 2u && view[0] == '0' && view[1] == 'x')
        {
            base = 16;
            view.remove_prefix(2);
        }
        else if (std::any_of(view.begin(), view.end(), [](char c) {
                     return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                 }))
        {
            base = 16;
        }

        uint32_t code = 0;
        const auto* first = view.data();
        const auto* last = view.data() + view.size();
        const auto result = std::from_chars(first, last, code, base);
        if (result.ec != std::errc() || result.ptr != last)
            return false;

        out = 5 + std::clamp(static_cast<int>(code), 0, 15);
        return true;
    };

    if (key.rfind("code", 0) == 0 && parseExactCode(key.substr(4)))
        return true;
    if (key.rfind("shape", 0) == 0 && parseExactCode(key.substr(5)))
        return true;

    if (key == "fixed" || key == "off" || key == "volume" || key == "macro" || key == "follow" || key == "default")
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
    if (text.size() > 2u && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        uint32_t bits = 0;
        if (! parseNumber(text, bits))
            return false;

        switch (bits & 0x70u)
        {
            case 0x10u: out = 1; return true;
            case 0x20u: out = 2; return true;
            case 0x40u: out = 3; return true;
            case 0x00u: out = 4; return true;
            case 0x50u: out = 5; return true;
            case 0x30u: out = 6; return true;
            case 0x60u: out = 7; return true;
            case 0x70u: out = 8; return true;
            default: return false;
        }
    }

    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 8);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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

bool parseHuc6280LfoMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "off" || key == "none" || key == "independent")
        out = 1;
    else if (key == "light" || key == "gentle" || key == "slow")
        out = 2;
    else if (key == "deep" || key == "strong" || key == "fm")
        out = 3;
    else if (key == "fast" || key == "chirp" || key == "arcade")
        out = 4;
    else
        return false;

    return true;
}

bool parseSpc700PlaybackMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 2);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "loop" || key == "looped" || key == "instrument" || key == "sustain")
        out = 1;
    else if (key == "oneshot" || key == "one" || key == "shot" || key == "drum" || key == "stop" || key == "end")
        out = 2;
    else
        return false;

    return true;
}

bool parsePokeyAudctlMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "off" || key == "none" || key == "8bit" || key == "independent")
        out = 1;
    else if (key == "12" || key == "1+2" || key == "ch12" || key == "ch1ch2" || key == "channel12")
        out = 2;
    else if (key == "34" || key == "3+4" || key == "ch34" || key == "ch3ch4" || key == "channel34")
        out = 3;
    else if (key == "both" || key == "all" || key == "linked" || key == "16bit")
        out = 4;
    else
        return false;

    return true;
}

bool parsePokeyFilterMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "off" || key == "none" || key == "bypass" || key == "filteroff")
        out = 1;
    else if (key == "13" || key == "1by3" || key == "ch1ch3" || key == "ch1bych3" || key == "ch1filteredbych3")
        out = 2;
    else if (key == "24" || key == "2by4" || key == "ch2ch4" || key == "ch2bych4" || key == "ch2filteredbych4")
        out = 3;
    else if (key == "both" || key == "all" || key == "1and2" || key == "13and24")
        out = 4;
    else
        return false;

    return true;
}

bool parsePaulaOutputFilterMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "raw" || key == "bright" || key == "nofilter" || key == "off")
        out = 1;
    else if (key == "a500" || key == "amiga500" || key == "fixed")
        out = 2;
    else if (key == "led" || key == "powerled" || key == "lowpass")
        out = 3;
    else if (key == "leda500" || key == "a500led" || key == "both" || key == "warm")
        out = 4;
    else
        return false;

    return true;
}

bool parseOpn2PanMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "both" || key == "center" || key == "centre" || key == "mono")
        out = 1;
    else if (key == "left" || key == "l")
        out = 2;
    else if (key == "right" || key == "r")
        out = 3;
    else if (key == "alt" || key == "alternate" || key == "alternating" || key == "split" || key == "wide")
        out = 4;
    else
        return false;

    return true;
}

bool parseOpn2EnvelopeShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "pluck" || key == "key" || key == "keys" || key == "short")
        out = 1;
    else if (key == "lead" || key == "sustain")
        out = 2;
    else if (key == "pad" || key == "slow" || key == "soft")
        out = 3;
    else if (key == "perc" || key == "percussion" || key == "drum" || key == "hit")
        out = 4;
    else
        return false;

    return true;
}

bool parseOpn2DacMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 2);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "fm" || key == "fmch6" || key == "ch6" || key == "melodic" || key == "off")
        out = 1;
    else if (key == "dac" || key == "dacdrum" || key == "drum" || key == "sample" || key == "pcm" || key == "on")
        out = 2;
    else
        return false;

    return true;
}

bool parseOpllRhythmMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 2);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "melodic" || key == "tone" || key == "tones" || key == "channels" || key == "off")
        out = 1;
    else if (key == "rhythm" || key == "drum" || key == "drums" || key == "perc" || key == "percussion" || key == "on")
        out = 2;
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "off")
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

bool parseSpc700NoiseMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "off" || key == "none" || key == "sample" || key == "brr")
        out = 1;
    else if (key == "low" || key == "slow" || key == "soft")
        out = 2;
    else if (key == "mid" || key == "medium" || key == "snare")
        out = 3;
    else if (key == "high" || key == "fast" || key == "hat" || key == "noise")
        out = 4;
    else
        return false;

    return true;
}

bool parseOpmNoiseMode(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "off" || key == "none" || key == "fm")
        out = 1;
    else if (key == "low" || key == "slow")
        out = 2;
    else if (key == "mid" || key == "medium")
        out = 3;
    else if (key == "high" || key == "fast" || key == "noise")
        out = 4;
    else
        return false;

    return true;
}

bool parseSpc700EnvelopeShape(const std::string& text, int& out)
{
    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 4);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
        out = 0;
    else if (key == "pluck" || key == "key" || key == "chime")
        out = 1;
    else if (key == "lead" || key == "sustain")
        out = 2;
    else if (key == "pad" || key == "soft" || key == "slow")
        out = 3;
    else if (key == "perc" || key == "percussion" || key == "drum" || key == "hit")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
    if (text.size() > 2u && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        uint32_t bits = 0;
        if (! parseNumber(text, bits) || bits > 0x07u)
            return false;

        switch (bits)
        {
            case 0x07u: out = 1; return true;
            case 0x01u: out = 2; return true;
            case 0x02u: out = 3; return true;
            case 0x04u: out = 4; return true;
            case 0x03u: out = 5; return true;
            case 0x05u: out = 6; return true;
            case 0x06u: out = 7; return true;
            case 0x00u: out = 8; return true;
            default: return false;
        }
    }

    uint32_t numeric = 0;
    if (parseNumber(text, numeric))
    {
        out = std::clamp(static_cast<int>(numeric), 0, 8);
        return true;
    }

    const auto key = normalizedToken(text);
    if (key == "macro" || key == "follow" || key == "auto" || key == "default")
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
        << "       Metadata: chipper_render --list-presets [--chip sid] --debug presets.json\n"
        << "                 chipper_render --list-descriptors --debug descriptors.json\n"
        << "                 chipper_render --describe-chip nes --debug nes-descriptor.json\n"
        << "       Optional: --preset nes-hero-pulse --macro coin --play-mode chip-poly --control1 0.2 --control2 0.8 --control3 0.1 --control4 0.5 --fm-op1-level 0..1 --fm-op2-level 0..1 --fm-op3-level 0..1 --fm-op4-level 0..1 --source1 1 --source2 0 --level1 1.0 --level2 0.5 --stereo-spread 0.75 --envelope-decay 0.7 --nes-dmc-direct-level 0..1 --nes-dmc-rate 0..15 --nes-dmc-loop 0|1 --nes-dmc-only 0|1 --nes-dmc-sample path.dmc --spc700-brr-sample path.brr --spc700-brr-hex 017f... --spc700-brr-bank-hex 017f... --spc700-sample-slot 0..31 --spc700-sample-slot1..8 0..32 --spc700-map-root 60 --spc700-loop-start 0..1 --spc700-loop-end 0..1 --paula-sample path.wav|path.8svx|raw --paula-shape1..4 follow|ramp|tri|sine|noise --paula-sample-slot1..4 0..32 --spc700-envelope follow|pluck|lead|pad|perc --spc700-noise follow|off|low|mid|high --sid-adsr-speed 0.7 --sid-attack follow|0..15 --sid-decay follow|0..15 --sid-sustain follow|0..15 --sid-release follow|0..15 --sid-voice2-attack follow|0..15 --sid-voice2-decay follow|0..15 --sid-voice2-sustain follow|0..15 --sid-voice2-release follow|0..15 --sid-voice3-attack follow|0..15 --sid-voice3-decay follow|0..15 --sid-voice3-sustain follow|0..15 --sid-voice3-release follow|0..15 --wave-shape follow|tri|saw|pulse|steps|noise --sid-voice2-wave follow|tri|saw|pulse|noise --sid-voice3-wave follow|tri|saw|pulse|noise --huc-wave1..6 follow|ramp|tri|square|noise --scc-wave1..5 follow|ramp|tri|pulse|steps --namco-wave1..8 follow|ramp|tri|pulse|steps --sid-voice2-pulse-width 0..1 --sid-voice3-pulse-width 0..1 --pulse2-duty follow|12.5|25|50|75 --dmg-wave-level follow|100|50|25|mute --dmg-stereo-route follow|both|left|right|split --huc-lfo follow|off|light|deep|fast --pokey-audctl follow|off|1+2|3+4|both --pokey-filter follow|off|1<-3|2<-4|both --paula-output-filter follow|raw|a500|led|both --spc700-playback follow|loop|one-shot --opn2-pan follow|both|left|right|alt --opm-pan follow|both|left|right|alt --opm-noise follow|off|low|mid|high --opn2-envelope follow|pluck|lead|pad|perc --opm-envelope follow|pluck|lead|pad|perc --fm-envelope follow|pluck|lead|pad|perc --opn2-dac follow|fm|dac --opl-rhythm follow|melodic|rhythm --opll-rhythm follow|melodic|rhythm --ym-envelope-shape fixed|fall|rise|saw|triangle|code0..code15|0x0..0xF --ym-channel-a-mix follow|tone|noise|both|off --ym-channel-b-mix follow|tone|noise|both|off --ym-channel-c-mix follow|tone|noise|both|off --sid-filter-mode follow|lp|bp|hp|off|notch|lp+bp|bp+hp|all|0x00|0x10|0x20|0x40|0x50|0x30|0x60|0x70 --sid-filter-routing follow|all|v1|v2|v3|v1+v2|v1+v3|v2+v3|none|0x00..0x07 --sid-mod-mode follow|off|sync|ring|both --sid-model follow|6581|8580 --sn-noise-mode follow|white-t3|long|short|15-bit|7-bit --output-db -9\n"
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
    options.fmOperatorLevels = chipper::fmOperatorLevelsForPreset(preset);
    options.controlProvided = { true, true, true, true };
    options.fmOperatorLevelProvided = { true, true, true, true };
    const auto anySourceEnabled = std::any_of(preset.sourceEnabled.begin(), preset.sourceEnabled.end(), [](bool enabled) { return enabled; });
    const auto nativeSourceCount = chipper::nativeSourceCountForMode(preset.chip);
    const auto useSource5 = anySourceEnabled && nativeSourceCount >= 5u;
    const auto useSource6 = anySourceEnabled && nativeSourceCount >= 6u;
    const auto useSource7 = anySourceEnabled && nativeSourceCount >= 7u;
    const auto useSource8 = anySourceEnabled && nativeSourceCount >= 8u;
    const auto useSource9 = anySourceEnabled && nativeSourceCount >= 9u;
    options.sourceEnabled = {
        preset.sourceEnabled[0],
        preset.sourceEnabled[1],
        preset.sourceEnabled[2],
        preset.sourceEnabled[3],
        useSource5,
        useSource6,
        useSource7,
        useSource8,
        useSource9
    };
    options.sourceProvided = { true, true, true, true, true, true, true, true, true };
    options.sourceLevels = { preset.source1Level, preset.source2Level, preset.source3Level, preset.source4Level, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    options.sourceLevelProvided = { true, true, true, true, true, true, true, true, true };
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
    options.dmgWaveLevel = preset.dmgWaveLevel;
    options.dmgStereoRoute = preset.dmgStereoRoute;
    options.ymEnvelopeShape = preset.ymEnvelopeShape;
    options.ymChannelAMix = 0;
    options.ymChannelBMix = 0;
    options.ymChannelCMix = 0;
    options.snNoiseMode = preset.snNoiseMode;
    options.sidFilterRouting = preset.sidFilterRouting;
    options.sidVoice2PulseWidth = preset.controls[0];
    options.sidVoice3PulseWidth = preset.controls[0];
    options.nesDmcDirectLevel = preset.nesDmcDirectLevel;
    options.nesDmcRateIndex = 15;
    options.nesDmcLoop = false;
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
    options.sidVoice2PulseWidthProvided = true;
    options.sidVoice3PulseWidthProvided = true;
    options.nesDmcDirectLevelProvided = true;
    options.nesDmcRateIndexProvided = true;
    options.nesDmcLoopProvided = true;
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
        auto parseWaveLane = [&](const char* name, int lane, bool huc) -> bool
        {
            const auto* value = requireValue(name);
            int parsed = 0;
            if (value == nullptr || ! (huc ? parseHucWaveShape(std::string(value), parsed) : parseWavetableWaveShape(std::string(value), parsed)))
                return false;

            switch (lane)
            {
                case 1:
                    options.waveShape = parsed;
                    options.waveShapeProvided = true;
                    return true;
                case 2:
                    options.sidVoice2WaveShape = parsed;
                    options.sidVoice2WaveShapeProvided = true;
                    return true;
                case 3:
                    options.sidVoice3WaveShape = parsed;
                    options.sidVoice3WaveShapeProvided = true;
                    return true;
                case 4:
                    options.pulse2Duty = parsed;
                    options.pulse2DutyProvided = true;
                    return true;
                case 5:
                    options.dmgWaveLevel = parsed;
                    options.dmgWaveLevelProvided = true;
                    return true;
                case 6:
                    options.snNoiseMode = parsed;
                    options.snNoiseModeProvided = true;
                    return true;
                case 7:
                    options.ymEnvelopeShape = parsed;
                    options.ymEnvelopeShapeProvided = true;
                    return true;
                case 8:
                    options.dmgStereoRoute = parsed;
                    options.dmgStereoRouteProvided = true;
                    return true;
                default:
                    return false;
            }
        };
        auto parsePaulaSampleLane = [&](const char* name, int lane) -> bool
        {
            const auto* value = requireValue(name);
            int parsed = 0;
            if (value == nullptr || ! parsePaulaSampleShape(std::string(value), parsed))
                return false;

            switch (lane)
            {
                case 1:
                    options.waveShape = parsed;
                    options.waveShapeProvided = true;
                    return true;
                case 2:
                    options.sidVoice2WaveShape = parsed;
                    options.sidVoice2WaveShapeProvided = true;
                    return true;
                case 3:
                    options.sidVoice3WaveShape = parsed;
                    options.sidVoice3WaveShapeProvided = true;
                    return true;
                case 4:
                    options.pulse2Duty = parsed;
                    options.pulse2DutyProvided = true;
                    return true;
                default:
                    return false;
            }
        };

        if (size_t paulaSlotIndex = 0; paulaSampleSlotIndexForArg(arg, paulaSlotIndex))
        {
            const auto* value = requireValue(arg.c_str());
            int parsed = 0;
            if (value == nullptr || ! parseNumber(std::string(value), parsed))
                return false;
            options.voiceSampleSlots[paulaSlotIndex] = std::clamp(parsed, 0, 32);
            continue;
        }

        if (size_t spc700SlotIndex = 0; spc700VoiceSampleSlotIndexForArg(arg, spc700SlotIndex))
        {
            const auto* value = requireValue(arg.c_str());
            int parsed = 0;
            if (value == nullptr || ! parseNumber(std::string(value), parsed))
                return false;
            options.voiceSampleSlots[spc700SlotIndex] = std::clamp(parsed, 0, 32);
            continue;
        }

        if (size_t fmOperatorIndex = 0; fmOperatorLevelIndexForArg(arg, fmOperatorIndex))
        {
            const auto* value = requireValue(arg.c_str());
            float parsed = 0.0f;
            if (value == nullptr || ! parseNumber(std::string(value), parsed))
                return false;
            options.fmOperatorLevels[fmOperatorIndex] = std::clamp(parsed, 0.0f, 1.0f);
            options.fmOperatorLevelProvided[fmOperatorIndex] = true;
            continue;
        }

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
        else if (arg == "--list-presets")
        {
            options.metadataMode = MetadataMode::listPresets;
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
            options.chipWasProvided = true;
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
            options.chipWasProvided = true;
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
        else if (arg == "--source5")
        {
            if (! parseToggle("--source5", options.sourceEnabled[4]))
                return false;
            options.sourceProvided[4] = true;
        }
        else if (arg == "--source6")
        {
            if (! parseToggle("--source6", options.sourceEnabled[5]))
                return false;
            options.sourceProvided[5] = true;
        }
        else if (arg == "--source7")
        {
            if (! parseToggle("--source7", options.sourceEnabled[6]))
                return false;
            options.sourceProvided[6] = true;
        }
        else if (arg == "--source8")
        {
            if (! parseToggle("--source8", options.sourceEnabled[7]))
                return false;
            options.sourceProvided[7] = true;
        }
        else if (arg == "--source9")
        {
            if (! parseToggle("--source9", options.sourceEnabled[8]))
                return false;
            options.sourceProvided[8] = true;
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
        else if (arg == "--level5")
        {
            const auto* value = requireValue("--level5");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[4]))
                return false;
            options.sourceLevelProvided[4] = true;
        }
        else if (arg == "--level6")
        {
            const auto* value = requireValue("--level6");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[5]))
                return false;
            options.sourceLevelProvided[5] = true;
        }
        else if (arg == "--level7")
        {
            const auto* value = requireValue("--level7");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[6]))
                return false;
            options.sourceLevelProvided[6] = true;
        }
        else if (arg == "--level8")
        {
            const auto* value = requireValue("--level8");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[7]))
                return false;
            options.sourceLevelProvided[7] = true;
        }
        else if (arg == "--level9")
        {
            const auto* value = requireValue("--level9");
            if (value == nullptr || ! parseNumber(std::string(value), options.sourceLevels[8]))
                return false;
            options.sourceLevelProvided[8] = true;
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
        else if (arg == "--huc-wave1")
        {
            if (! parseWaveLane("--huc-wave1", 1, true))
                return false;
        }
        else if (arg == "--huc-wave2")
        {
            if (! parseWaveLane("--huc-wave2", 2, true))
                return false;
        }
        else if (arg == "--huc-wave3")
        {
            if (! parseWaveLane("--huc-wave3", 3, true))
                return false;
        }
        else if (arg == "--huc-wave4")
        {
            if (! parseWaveLane("--huc-wave4", 4, true))
                return false;
        }
        else if (arg == "--huc-wave5")
        {
            if (! parseWaveLane("--huc-wave5", 5, true))
                return false;
        }
        else if (arg == "--huc-wave6")
        {
            if (! parseWaveLane("--huc-wave6", 6, true))
                return false;
        }
        else if (arg == "--scc-wave1")
        {
            if (! parseWaveLane("--scc-wave1", 1, false))
                return false;
        }
        else if (arg == "--scc-wave2")
        {
            if (! parseWaveLane("--scc-wave2", 2, false))
                return false;
        }
        else if (arg == "--scc-wave3")
        {
            if (! parseWaveLane("--scc-wave3", 3, false))
                return false;
        }
        else if (arg == "--scc-wave4")
        {
            if (! parseWaveLane("--scc-wave4", 4, false))
                return false;
        }
        else if (arg == "--scc-wave5")
        {
            if (! parseWaveLane("--scc-wave5", 5, false))
                return false;
        }
        else if (arg == "--namco-wave1" || arg == "--wsg-wave1")
        {
            if (! parseWaveLane(arg.c_str(), 1, false))
                return false;
        }
        else if (arg == "--namco-wave2" || arg == "--wsg-wave2")
        {
            if (! parseWaveLane(arg.c_str(), 2, false))
                return false;
        }
        else if (arg == "--namco-wave3" || arg == "--wsg-wave3")
        {
            if (! parseWaveLane(arg.c_str(), 3, false))
                return false;
        }
        else if (arg == "--namco-wave4" || arg == "--wsg-wave4")
        {
            if (! parseWaveLane(arg.c_str(), 4, false))
                return false;
        }
        else if (arg == "--namco-wave5" || arg == "--wsg-wave5")
        {
            if (! parseWaveLane(arg.c_str(), 5, false))
                return false;
        }
        else if (arg == "--namco-wave6" || arg == "--wsg-wave6")
        {
            if (! parseWaveLane(arg.c_str(), 6, false))
                return false;
        }
        else if (arg == "--namco-wave7" || arg == "--wsg-wave7")
        {
            if (! parseWaveLane(arg.c_str(), 7, false))
                return false;
        }
        else if (arg == "--namco-wave8" || arg == "--wsg-wave8")
        {
            if (! parseWaveLane(arg.c_str(), 8, false))
                return false;
        }
        else if (arg == "--paula-shape1" || arg == "--paula-sample1" || arg == "--paula-channel1-sample")
        {
            if (! parsePaulaSampleLane(arg.c_str(), 1))
                return false;
        }
        else if (arg == "--paula-shape2" || arg == "--paula-sample2" || arg == "--paula-channel2-sample")
        {
            if (! parsePaulaSampleLane(arg.c_str(), 2))
                return false;
        }
        else if (arg == "--paula-shape3" || arg == "--paula-sample3" || arg == "--paula-channel3-sample")
        {
            if (! parsePaulaSampleLane(arg.c_str(), 3))
                return false;
        }
        else if (arg == "--paula-shape4" || arg == "--paula-sample4" || arg == "--paula-channel4-sample")
        {
            if (! parsePaulaSampleLane(arg.c_str(), 4))
                return false;
        }
        else if (arg == "--sid-voice2-pulse-width" || arg == "--sid-v2-pw")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseNumber(std::string(value), options.sidVoice2PulseWidth))
                return false;
            options.sidVoice2PulseWidth = std::clamp(options.sidVoice2PulseWidth, 0.0f, 1.0f);
            options.sidVoice2PulseWidthProvided = true;
        }
        else if (arg == "--sid-voice3-pulse-width" || arg == "--sid-v3-pw")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseNumber(std::string(value), options.sidVoice3PulseWidth))
                return false;
            options.sidVoice3PulseWidth = std::clamp(options.sidVoice3PulseWidth, 0.0f, 1.0f);
            options.sidVoice3PulseWidthProvided = true;
        }
        else if (arg == "--pulse2-duty" || arg == "--nes-pulse2-duty" || arg == "--dmg-pulse2-duty")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parsePulse2Duty(std::string(value), options.pulse2Duty))
                return false;
            options.pulse2DutyProvided = true;
        }
        else if (arg == "--nes-dmc-direct-level")
        {
            const auto* value = requireValue("--nes-dmc-direct-level");
            if (value == nullptr || ! parseNumber(std::string(value), options.nesDmcDirectLevel))
                return false;
            options.nesDmcDirectLevel = std::clamp(options.nesDmcDirectLevel, 0.0f, 1.0f);
            options.nesDmcDirectLevelProvided = true;
        }
        else if (arg == "--nes-dmc-rate" || arg == "--nes-dmc-rate-index")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseNumber(std::string(value), options.nesDmcRateIndex))
                return false;
            options.nesDmcRateIndex = std::clamp(options.nesDmcRateIndex, 0, 15);
            options.nesDmcRateIndexProvided = true;
        }
        else if (arg == "--nes-dmc-loop")
        {
            if (! parseToggle("--nes-dmc-loop", options.nesDmcLoop))
                return false;
            options.nesDmcLoopProvided = true;
        }
        else if (arg == "--nes-dmc-only")
        {
            if (! parseToggle("--nes-dmc-only", options.nesDmcOnly))
                return false;
            options.nesDmcOnlyProvided = true;
        }
        else if (arg == "--nes-dmc-sample")
        {
            const auto* value = requireValue("--nes-dmc-sample");
            if (value == nullptr)
                return false;
            options.nesDmcSamplePath = value;
        }
        else if (arg == "--spc700-brr-sample")
        {
            const auto* value = requireValue("--spc700-brr-sample");
            if (value == nullptr)
                return false;
            options.spc700BrrSamplePath = value;
        }
        else if (arg == "--paula-sample")
        {
            const auto* value = requireValue("--paula-sample");
            if (value == nullptr)
                return false;
            options.paulaSamplePath = value;
        }
        else if (arg == "--spc700-brr-hex")
        {
            const auto* value = requireValue("--spc700-brr-hex");
            if (value == nullptr)
                return false;
            options.spc700BrrHex = value;
        }
        else if (arg == "--spc700-brr-bank-hex")
        {
            const auto* value = requireValue("--spc700-brr-bank-hex");
            if (value == nullptr)
                return false;
            options.spc700BrrBankHex.push_back(value);
        }
        else if (arg == "--spc700-sample-slot" || arg == "--spc700-brr-slot")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseNumber(std::string(value), options.spc700SampleSlot))
                return false;
            options.spc700SampleSlot = std::clamp(options.spc700SampleSlot, 0, 31);
        }
        else if (arg == "--spc700-map-root" || arg == "--spc700-note-map-root")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseNumber(std::string(value), options.spc700MapRoot))
                return false;
            options.spc700MapRoot = std::clamp(options.spc700MapRoot, 0, 127);
        }
        else if (arg == "--spc700-loop-start")
        {
            const auto* value = requireValue("--spc700-loop-start");
            if (value == nullptr || ! parseNumber(std::string(value), options.spc700LoopStart))
                return false;
            options.spc700LoopStart = std::clamp(options.spc700LoopStart, 0.0f, 1.0f);
        }
        else if (arg == "--spc700-loop-end")
        {
            const auto* value = requireValue("--spc700-loop-end");
            if (value == nullptr || ! parseNumber(std::string(value), options.spc700LoopEnd))
                return false;
            options.spc700LoopEnd = std::clamp(options.spc700LoopEnd, 0.0f, 1.0f);
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
        else if (arg == "--spc700-playback" || arg == "--spc700-playback-mode")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseSpc700PlaybackMode(std::string(value), options.dmgStereoRoute))
                return false;
            options.dmgStereoRouteProvided = true;
        }
        else if (arg == "--huc-lfo" || arg == "--huc6280-lfo")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseHuc6280LfoMode(std::string(value), options.dmgStereoRoute))
                return false;
            options.dmgStereoRouteProvided = true;
        }
        else if (arg == "--spc700-envelope" || arg == "--spc700-envelope-shape")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseSpc700EnvelopeShape(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
        }
        else if (arg == "--spc700-noise" || arg == "--spc700-noise-mode")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseSpc700NoiseMode(std::string(value), options.snNoiseMode))
                return false;
            options.snNoiseModeProvided = true;
        }
        else if (arg == "--pokey-audctl" || arg == "--pokey-pairing")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parsePokeyAudctlMode(std::string(value), options.dmgStereoRoute))
                return false;
            options.dmgStereoRouteProvided = true;
        }
        else if (arg == "--pokey-filter" || arg == "--pokey-audctl-filter")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parsePokeyFilterMode(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
        }
        else if (arg == "--paula-output-filter" || arg == "--paula-filter")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parsePaulaOutputFilterMode(std::string(value), options.snNoiseMode))
                return false;
            options.snNoiseModeProvided = true;
        }
        else if (arg == "--opn2-pan" || arg == "--ym2612-pan" || arg == "--opm-pan" || arg == "--ym2151-pan")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseOpn2PanMode(std::string(value), options.dmgStereoRoute))
                return false;
            options.dmgStereoRouteProvided = true;
        }
        else if (arg == "--opm-noise" || arg == "--ym2151-noise" || arg == "--opm-noise-mode")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseOpmNoiseMode(std::string(value), options.snNoiseMode))
                return false;
            options.snNoiseModeProvided = true;
        }
        else if (arg == "--opn2-envelope" || arg == "--ym2612-envelope" || arg == "--opm-envelope" || arg == "--ym2151-envelope" || arg == "--fm-envelope")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseOpn2EnvelopeShape(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
        }
        else if (arg == "--opn2-dac" || arg == "--ym2612-dac" || arg == "--opn2-dac-mode")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseOpn2DacMode(std::string(value), options.snNoiseMode))
                return false;
            options.snNoiseModeProvided = true;
        }
        else if (arg == "--opl-rhythm" || arg == "--opl-rhythm-mode")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseOpllRhythmMode(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
        }
        else if (arg == "--opll-rhythm" || arg == "--ym2413-rhythm" || arg == "--opll-rhythm-mode")
        {
            const auto* value = requireValue(arg.c_str());
            if (value == nullptr || ! parseOpllRhythmMode(std::string(value), options.ymEnvelopeShape))
                return false;
            options.ymEnvelopeShapeProvided = true;
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
        if (! options.fmOperatorLevelProvided[i])
            options.fmOperatorLevels[i] = 0.5f;
        if (! options.sourceProvided[i])
            options.sourceEnabled[i] = templ.sourceEnabled[i];
        if (! options.sourceLevelProvided[i])
            options.sourceLevels[i] = 1.0f;
    }
    const auto anyResolvedVisibleSourceEnabled = std::any_of(options.sourceEnabled.begin(),
                                                            options.sourceEnabled.begin() + 4,
                                                            [](bool enabled) { return enabled; });
    for (size_t i = 4; i < options.sourceEnabled.size(); ++i)
    {
        if (! options.sourceProvided[i])
            options.sourceEnabled[i] = anyResolvedVisibleSourceEnabled && chipper::nativeSourceCountForMode(options.chip) > i;
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
    if (! options.sidVoice2PulseWidthProvided)
        options.sidVoice2PulseWidth = options.control1;
    if (! options.sidVoice3PulseWidthProvided)
        options.sidVoice3PulseWidth = options.control1;
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
    if (! options.nesDmcDirectLevelProvided)
        options.nesDmcDirectLevel = templ.nesDmcDirectLevel;
    if (! options.nesDmcRateIndexProvided)
        options.nesDmcRateIndex = 15;
    if (! options.nesDmcLoopProvided)
        options.nesDmcLoop = false;
    if (! options.nesDmcOnlyProvided)
        options.nesDmcOnly = false;
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

std::vector<uint8_t> loadBinaryFile(const std::filesystem::path& path)
{
    if (path.empty())
        return {};

    std::ifstream in(path, std::ios::binary);
    if (! in)
        throw std::runtime_error("Could not open binary sample file: " + path.string());

    return {
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>()
    };
}

uint16_t readBigEndian16(const uint8_t* data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8u) | static_cast<uint16_t>(data[1]));
}

uint16_t readLittleEndian16(const uint8_t* data)
{
    return static_cast<uint16_t>(static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8u));
}

uint32_t readLittleEndian32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0])
        | (static_cast<uint32_t>(data[1]) << 8u)
        | (static_cast<uint32_t>(data[2]) << 16u)
        | (static_cast<uint32_t>(data[3]) << 24u);
}

uint32_t readBigEndian32(const uint8_t* data)
{
    return (static_cast<uint32_t>(data[0]) << 24u)
        | (static_cast<uint32_t>(data[1]) << 16u)
        | (static_cast<uint32_t>(data[2]) << 8u)
        | static_cast<uint32_t>(data[3]);
}

std::vector<uint8_t> signedPcm8ToPaulaBytes(const uint8_t* data, size_t size)
{
    std::vector<uint8_t> converted;
    converted.reserve(size);
    for (size_t i = 0; i < size; ++i)
        converted.push_back(static_cast<uint8_t>(static_cast<int>(static_cast<int8_t>(data[i])) + 128));
    return converted;
}

bool readWavSmplLoopMetadata(const std::vector<uint8_t>& data, size_t sampleCount, size_t& loopStart, size_t& loopEnd)
{
    loopStart = 0u;
    loopEnd = 0u;
    if (sampleCount <= 1u || data.size() < 12u || std::memcmp(data.data(), "RIFF", 4u) != 0 || std::memcmp(data.data() + 8u, "WAVE", 4u) != 0)
        return false;

    auto offset = static_cast<size_t>(12u);
    while (offset + 8u <= data.size())
    {
        const auto* chunkId = data.data() + offset;
        const auto chunkSize = static_cast<size_t>(readLittleEndian32(data.data() + offset + 4u));
        const auto chunkDataOffset = offset + 8u;
        if (chunkDataOffset + chunkSize > data.size())
            return false;

        if (std::memcmp(chunkId, "smpl", 4u) == 0)
        {
            if (chunkSize < 36u)
                return false;

            const auto loopCount = readLittleEndian32(data.data() + chunkDataOffset + 28u);
            auto loopOffset = chunkDataOffset + 36u;
            for (uint32_t i = 0; i < loopCount && loopOffset + 24u <= chunkDataOffset + chunkSize; ++i, loopOffset += 24u)
            {
                const auto type = readLittleEndian32(data.data() + loopOffset + 4u);
                const auto start = static_cast<size_t>(readLittleEndian32(data.data() + loopOffset + 8u));
                const auto inclusiveEnd = static_cast<size_t>(readLittleEndian32(data.data() + loopOffset + 12u));
                if (type == 0u && start < inclusiveEnd && start < sampleCount)
                {
                    loopStart = std::min(start, sampleCount - 1u);
                    loopEnd = std::min(sampleCount, inclusiveEnd + 1u);
                    return loopEnd > loopStart + 1u;
                }
            }

            return false;
        }

        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    return false;
}

struct WavPcmFormat
{
    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint16_t blockAlign = 0;
    uint16_t bitsPerSample = 0;
};

int32_t readSignedLittleEndian24(const uint8_t* data)
{
    auto value = static_cast<int32_t>(data[0])
        | (static_cast<int32_t>(data[1]) << 8)
        | (static_cast<int32_t>(data[2]) << 16);
    if ((value & 0x00800000) != 0)
        value |= static_cast<int32_t>(0xff000000);
    return value;
}

double readWavSampleAsNormalizedDouble(const uint8_t* frame, uint16_t bitsPerSample, uint16_t audioFormat)
{
    if (audioFormat == 1u)
    {
        if (bitsPerSample == 8u)
            return (static_cast<double>(frame[0]) - 128.0) / 128.0;
        if (bitsPerSample == 16u)
            return static_cast<double>(static_cast<int16_t>(readLittleEndian16(frame))) / 32768.0;
        if (bitsPerSample == 24u)
            return static_cast<double>(readSignedLittleEndian24(frame)) / 8388608.0;
        if (bitsPerSample == 32u)
            return static_cast<double>(static_cast<int32_t>(readLittleEndian32(frame))) / 2147483648.0;
    }
    else if (audioFormat == 3u && bitsPerSample == 32u)
    {
        float value = 0.0f;
        std::memcpy(&value, frame, sizeof(value));
        return static_cast<double>(value);
    }

    throw std::runtime_error("Paula WAV renderer import supports PCM 8/16/24/32-bit and float 32-bit files");
}

chipper::ExternalSampleData loadWavPaulaSampleFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
{
    if (data.size() < 12u || std::memcmp(data.data(), "RIFF", 4u) != 0 || std::memcmp(data.data() + 8u, "WAVE", 4u) != 0)
        throw std::runtime_error("Paula WAV file is not a RIFF/WAVE file: " + path.string());

    WavPcmFormat format;
    const uint8_t* pcmData = nullptr;
    size_t pcmDataSize = 0u;
    auto offset = static_cast<size_t>(12u);
    while (offset + 8u <= data.size())
    {
        const auto* chunkId = data.data() + offset;
        const auto chunkSize = static_cast<size_t>(readLittleEndian32(data.data() + offset + 4u));
        const auto chunkDataOffset = offset + 8u;
        if (chunkDataOffset + chunkSize > data.size())
            throw std::runtime_error("Paula WAV chunk extends past end of file: " + path.string());

        if (std::memcmp(chunkId, "fmt ", 4u) == 0)
        {
            if (chunkSize < 16u)
                throw std::runtime_error("Paula WAV fmt chunk is too short: " + path.string());

            format.audioFormat = readLittleEndian16(data.data() + chunkDataOffset);
            format.channels = readLittleEndian16(data.data() + chunkDataOffset + 2u);
            format.blockAlign = readLittleEndian16(data.data() + chunkDataOffset + 12u);
            format.bitsPerSample = readLittleEndian16(data.data() + chunkDataOffset + 14u);
        }
        else if (std::memcmp(chunkId, "data", 4u) == 0)
        {
            pcmData = data.data() + chunkDataOffset;
            pcmDataSize = chunkSize;
        }

        offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
    }

    if (format.channels == 0u || format.blockAlign == 0u || format.bitsPerSample == 0u)
        throw std::runtime_error("Paula WAV file is missing usable fmt metadata: " + path.string());

    if (pcmData == nullptr || pcmDataSize == 0u)
        throw std::runtime_error("Paula WAV file has no sample data: " + path.string());

    const auto bytesPerSample = static_cast<size_t>((format.bitsPerSample + 7u) / 8u);
    const auto minimumBlockAlign = bytesPerSample * static_cast<size_t>(format.channels);
    if (bytesPerSample == 0u || format.blockAlign < minimumBlockAlign)
        throw std::runtime_error("Paula WAV block alignment is invalid: " + path.string());

    constexpr size_t maxImportedSamples = 262144u;
    const auto frameCount = std::min(pcmDataSize / format.blockAlign, maxImportedSamples);
    chipper::ExternalSampleData sample;
    sample.encoding = chipper::ExternalSampleEncoding::signedPcm8;
    sample.bytes.resize(frameCount);

    for (size_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
    {
        const auto* frame = pcmData + frameIndex * format.blockAlign;
        auto mixed = 0.0;
        for (uint16_t channel = 0; channel < format.channels; ++channel)
            mixed += readWavSampleAsNormalizedDouble(frame + static_cast<size_t>(channel) * bytesPerSample,
                                                     format.bitsPerSample,
                                                     format.audioFormat);

        mixed /= static_cast<double>(format.channels);
        const auto quantized = std::clamp(static_cast<int>(std::round(std::clamp(mixed, -1.0, 1.0) * 127.0)), -128, 127);
        sample.bytes[frameIndex] = static_cast<uint8_t>(quantized + 128);
    }

    size_t loopStart = 0u;
    size_t loopEnd = 0u;
    if (readWavSmplLoopMetadata(data, sample.bytes.size(), loopStart, loopEnd))
    {
        sample.hasLoop = true;
        sample.loopStart = loopStart;
        sample.loopEnd = loopEnd;
    }

    return sample;
}

chipper::ExternalSampleData loadPaulaSampleFile(const std::filesystem::path& path)
{
    auto data = loadBinaryFile(path);
    chipper::ExternalSampleData sample;
    sample.encoding = chipper::ExternalSampleEncoding::signedPcm8;
    if (data.empty())
        return sample;

    if (data.size() >= 12u && std::memcmp(data.data(), "RIFF", 4u) == 0 && std::memcmp(data.data() + 8u, "WAVE", 4u) == 0)
        return loadWavPaulaSampleFile(path, data);

    if (data.size() >= 12u && std::memcmp(data.data(), "FORM", 4u) == 0 && std::memcmp(data.data() + 8u, "8SVX", 4u) == 0)
    {
        const auto formSize = static_cast<size_t>(readBigEndian32(data.data() + 4u));
        const auto formEnd = std::min(data.size(), static_cast<size_t>(8u + formSize));
        auto offset = static_cast<size_t>(12u);
        bool sawVhdr = false;
        bool isCompressed = false;
        uint32_t oneShotSamples = 0;
        uint32_t repeatSamples = 0;
        const uint8_t* body = nullptr;
        size_t bodySize = 0u;

        while (offset + 8u <= formEnd)
        {
            const auto* chunkId = data.data() + offset;
            const auto chunkSize = static_cast<size_t>(readBigEndian32(data.data() + offset + 4u));
            const auto chunkDataOffset = offset + 8u;
            if (chunkDataOffset + chunkSize > data.size())
                throw std::runtime_error("Paula 8SVX chunk extends past end of file: " + path.string());

            if (std::memcmp(chunkId, "VHDR", 4u) == 0)
            {
                if (chunkSize < 16u)
                    throw std::runtime_error("Paula 8SVX VHDR chunk is too short: " + path.string());

                sawVhdr = true;
                oneShotSamples = readBigEndian32(data.data() + chunkDataOffset);
                repeatSamples = readBigEndian32(data.data() + chunkDataOffset + 4u);
                const auto sampleRate = readBigEndian16(data.data() + chunkDataOffset + 12u);
                const auto compression = data[chunkDataOffset + 15u];
                if (sampleRate == 0u)
                    throw std::runtime_error("Paula 8SVX sample rate is invalid: " + path.string());

                isCompressed = compression != 0u;
            }
            else if (std::memcmp(chunkId, "BODY", 4u) == 0)
            {
                body = data.data() + chunkDataOffset;
                bodySize = chunkSize;
            }

            offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
        }

        if (! sawVhdr)
            throw std::runtime_error("Paula 8SVX file is missing VHDR metadata: " + path.string());

        if (isCompressed)
            throw std::runtime_error("Paula 8SVX renderer import supports uncompressed BODY data only: " + path.string());

        if (body == nullptr || bodySize == 0u)
            throw std::runtime_error("Paula 8SVX file has no BODY sample data: " + path.string());

        sample.bytes = signedPcm8ToPaulaBytes(body, bodySize);
        const auto sampleCount = sample.bytes.size();
        if (repeatSamples > 1u && oneShotSamples < sampleCount)
        {
            sample.hasLoop = true;
            sample.loopStart = static_cast<size_t>(std::min<uint32_t>(oneShotSamples, static_cast<uint32_t>(sampleCount - 1u)));
            sample.loopEnd = std::min(sampleCount, static_cast<size_t>(oneShotSamples) + static_cast<size_t>(repeatSamples));
            if (sample.loopEnd <= sample.loopStart + 1u)
                sample.hasLoop = false;
        }
        return sample;
    }

    sample.bytes = signedPcm8ToPaulaBytes(data.data(), data.size());
    return sample;
}

std::vector<uint8_t> parseHexBytes(std::string text)
{
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c) != 0 || c == ',' || c == '_' || c == '-';
    }), text.end());

    if (text.size() % 2u != 0)
        throw std::runtime_error("Hex byte string must contain an even number of digits");

    std::vector<uint8_t> bytes;
    bytes.reserve(text.size() / 2u);
    for (size_t i = 0; i < text.size(); i += 2u)
    {
        uint32_t value = 0;
        const auto chunk = text.substr(i, 2u);
        if (! parseNumber("0x" + chunk, value) || value > 0xffu)
            throw std::runtime_error("Bad hex byte near: " + chunk);
        bytes.push_back(static_cast<uint8_t>(value));
    }

    return bytes;
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
    double tailEnergy = 0.0;
    const auto tailStart = frames.empty() ? 0u : (frames.size() * 3u) / 4u;
    float previous = 0.0f;
    bool hasPrevious = false;

    for (size_t index = 0; index < frames.size(); ++index)
    {
        const auto& frame = frames[index];
        const auto mono = (frame.left + frame.right) * 0.5f;
        stats.peak = std::max(stats.peak, static_cast<double>(std::abs(mono)));
        stats.leftPeak = std::max(stats.leftPeak, static_cast<double>(std::abs(frame.left)));
        stats.rightPeak = std::max(stats.rightPeak, static_cast<double>(std::abs(frame.right)));
        energy += static_cast<double>(mono) * static_cast<double>(mono);
        leftEnergy += static_cast<double>(frame.left) * static_cast<double>(frame.left);
        rightEnergy += static_cast<double>(frame.right) * static_cast<double>(frame.right);
        if (index >= tailStart)
            tailEnergy += static_cast<double>(mono) * static_cast<double>(mono);

        if (hasPrevious && ((previous <= 0.0f && mono > 0.0f) || (previous >= 0.0f && mono < 0.0f)))
            ++stats.zeroCrossings;

        previous = mono;
        hasPrevious = true;
    }

    stats.renderedSamples = static_cast<uint64_t>(frames.size());
    stats.rms = frames.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(frames.size()));
    stats.leftRms = frames.empty() ? 0.0 : std::sqrt(leftEnergy / static_cast<double>(frames.size()));
    stats.rightRms = frames.empty() ? 0.0 : std::sqrt(rightEnergy / static_cast<double>(frames.size()));
    const auto tailSamples = frames.size() > tailStart ? frames.size() - tailStart : 0u;
    stats.tailRms = tailSamples == 0u ? 0.0 : std::sqrt(tailEnergy / static_cast<double>(tailSamples));
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

void writeJsonStringArray(std::ostream& out, const std::vector<std::string>& values, const std::string& indent)
{
    out << "[\n";
    for (size_t i = 0; i < values.size(); ++i)
    {
        out << indent;
        writeJsonString(out, values[i]);
        out << (i + 1u == values.size() ? "\n" : ",\n");
    }
    out << indent.substr(0, indent.size() >= 2u ? indent.size() - 2u : 0u) << "]";
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
        case chipper::ControlSurface::steppedSlider: return "steppedSlider";
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
        case chipper::ChipParameterRole::source5Enabled: return "source5Enabled";
        case chipper::ChipParameterRole::source6Enabled: return "source6Enabled";
        case chipper::ChipParameterRole::source7Enabled: return "source7Enabled";
        case chipper::ChipParameterRole::source8Enabled: return "source8Enabled";
        case chipper::ChipParameterRole::source9Enabled: return "source9Enabled";
        case chipper::ChipParameterRole::source1Level: return "source1Level";
        case chipper::ChipParameterRole::source2Level: return "source2Level";
        case chipper::ChipParameterRole::source3Level: return "source3Level";
        case chipper::ChipParameterRole::source4Level: return "source4Level";
        case chipper::ChipParameterRole::source5Level: return "source5Level";
        case chipper::ChipParameterRole::source6Level: return "source6Level";
        case chipper::ChipParameterRole::source7Level: return "source7Level";
        case chipper::ChipParameterRole::source8Level: return "source8Level";
        case chipper::ChipParameterRole::source9Level: return "source9Level";
        case chipper::ChipParameterRole::stereoSpread: return "stereoSpread";
        case chipper::ChipParameterRole::sidFilterRouting: return "sidFilterRouting";
        case chipper::ChipParameterRole::sidVoice2PulseWidth: return "sidVoice2PulseWidth";
        case chipper::ChipParameterRole::sidVoice3PulseWidth: return "sidVoice3PulseWidth";
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
        case chipper::ChipParameterRole::nesDmcDirectLevel: return "nesDmcDirectLevel";
        case chipper::ChipParameterRole::nesDmcSampleSlot: return "nesDmcSampleSlot";
        case chipper::ChipParameterRole::nesDmcRateIndex: return "nesDmcRateIndex";
        case chipper::ChipParameterRole::nesDmcPlaybackMode: return "nesDmcPlaybackMode";
        case chipper::ChipParameterRole::nesDmcMapRoot: return "nesDmcMapRoot";
        case chipper::ChipParameterRole::nesDmcLoop: return "nesDmcLoop";
        case chipper::ChipParameterRole::spc700LoopStart: return "spc700LoopStart";
        case chipper::ChipParameterRole::spc700LoopEnd: return "spc700LoopEnd";
        case chipper::ChipParameterRole::spc700Voice1SampleSlot: return "spc700Voice1SampleSlot";
        case chipper::ChipParameterRole::spc700Voice2SampleSlot: return "spc700Voice2SampleSlot";
        case chipper::ChipParameterRole::spc700Voice3SampleSlot: return "spc700Voice3SampleSlot";
        case chipper::ChipParameterRole::spc700Voice4SampleSlot: return "spc700Voice4SampleSlot";
        case chipper::ChipParameterRole::spc700Voice5SampleSlot: return "spc700Voice5SampleSlot";
        case chipper::ChipParameterRole::spc700Voice6SampleSlot: return "spc700Voice6SampleSlot";
        case chipper::ChipParameterRole::spc700Voice7SampleSlot: return "spc700Voice7SampleSlot";
        case chipper::ChipParameterRole::spc700Voice8SampleSlot: return "spc700Voice8SampleSlot";
        case chipper::ChipParameterRole::fmOperator1Level: return "fmOperator1Level";
        case chipper::ChipParameterRole::fmOperator2Level: return "fmOperator2Level";
        case chipper::ChipParameterRole::fmOperator3Level: return "fmOperator3Level";
        case chipper::ChipParameterRole::fmOperator4Level: return "fmOperator4Level";
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
    }

    return "nes";
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
        << "    \"presetCount\": " << presets.size() << ",\n"
        << "    \"sourceLanes\": {\n"
        << "      \"visible\": " << chipper::visibleSourceCountForMode(mode) << ",\n"
        << "      \"native\": " << chipper::nativeSourceCountForMode(mode) << ",\n"
        << "      \"hasInternal\": " << (chipper::hasInternalSourceLanes(mode) ? "true" : "false") << "\n"
        << "    },\n"
        << "    \"implemented\": " << (descriptor.implemented ? "true" : "false") << ",\n"
        << "    \"supportsChipPoly\": " << (descriptor.supportsChipPoly ? "true" : "false") << ",\n"
        << "    \"verification\": {\n"
        << "      \"badge\": ";
    writeJsonString(out, descriptor.verification.badge);
    out << ",\n"
        << "      \"summary\": ";
    writeJsonString(out, descriptor.verification.summary);
    out << ",\n"
        << "      \"evidence\": ";
    writeJsonString(out, descriptor.verification.evidence);
    out << ",\n"
        << "      \"hardwareValidated\": " << (descriptor.verification.hardwareValidated ? "true" : "false") << ",\n"
        << "      \"cycleAccurate\": " << (descriptor.verification.cycleAccurate ? "true" : "false") << ",\n"
        << "      \"verifiedBehaviors\": ";
    writeJsonStringArray(out, descriptor.verification.verifiedBehaviors, "        ");
    out << ",\n"
        << "      \"knownGaps\": ";
    writeJsonStringArray(out, descriptor.verification.knownGaps, "        ");
    out << "\n"
        << "    },\n"
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
            << macro.stereoSpread << ", \"nesDmcDirectLevel\": "
            << macro.nesDmcDirectLevel << " }"
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
    struct ChipPresetCount
    {
        chipper::ChipMode mode = chipper::ChipMode::nes;
        size_t count = 0u;
    };

    struct NamedPresetCount
    {
        std::string name;
        size_t count = 0u;
    };

    const auto bumpNamedCount = [](std::vector<NamedPresetCount>& counts, std::string name)
    {
        const auto iter = std::find_if(counts.begin(), counts.end(), [&name](const auto& entry)
        {
            return entry.name == name;
        });
        if (iter != counts.end())
        {
            ++iter->count;
            return;
        }

        counts.push_back({ std::move(name), 1u });
    };

    std::vector<ChipPresetCount> chipCounts;
    std::vector<NamedPresetCount> roleCounts;
    std::vector<NamedPresetCount> engineCounts;
    const auto& qualityTargets = chipper::presetQualityTargets();
    chipCounts.reserve(modes.size());
    size_t totalPresetCount = 0u;
    for (const auto mode : modes)
    {
        size_t count = 0u;
        for (const auto& preset : presets)
        {
            if (preset.chip == mode)
                ++count;
        }

        if (count > 0u)
        {
            chipCounts.push_back({ mode, count });
            totalPresetCount += count;
        }
    }

    for (const auto& preset : presets)
    {
        if (! includesMode(modes, preset.chip))
            continue;

        bumpNamedCount(roleCounts, chipper::presetRoleFor(preset));
        bumpNamedCount(engineCounts, chipper::presetEngineFor(preset));
    }

    out << "  \"summary\": {\n"
        << "    \"totalPresetCount\": " << totalPresetCount << ",\n"
        << "    \"chipCounts\": [\n";
    for (size_t i = 0; i < chipCounts.size(); ++i)
    {
        const auto& entry = chipCounts[i];
        out << "      { \"chipKey\": ";
        writeJsonString(out, chipModeKey(entry.mode));
        out << ", \"chip\": ";
        writeJsonString(out, chipper::toString(entry.mode));
        out << ", \"presetCount\": " << entry.count << " }"
            << (i + 1u == chipCounts.size() ? "\n" : ",\n");
    }
    out << "    ],\n"
        << "    \"roleCounts\": [\n";
    for (size_t i = 0; i < roleCounts.size(); ++i)
    {
        const auto& entry = roleCounts[i];
        out << "      { \"role\": ";
        writeJsonString(out, entry.name);
        out << ", \"presetCount\": " << entry.count << " }"
            << (i + 1u == roleCounts.size() ? "\n" : ",\n");
    }
    out << "    ],\n"
        << "    \"engineCounts\": [\n";
    for (size_t i = 0; i < engineCounts.size(); ++i)
    {
        const auto& entry = engineCounts[i];
        out << "      { \"engine\": ";
        writeJsonString(out, entry.name);
        out << ", \"presetCount\": " << entry.count << " }"
            << (i + 1u == engineCounts.size() ? "\n" : ",\n");
    }
    out << "    ],\n"
        << "    \"qualityTargets\": [\n";
    bool wroteTarget = false;
    for (const auto& target : qualityTargets)
    {
        if (! includesMode(modes, target.chip))
            continue;

        if (wroteTarget)
            out << ",\n";

        std::vector<const chipper::PresetInfo*> targetPresets;
        for (const auto& preset : presets)
        {
            if (preset.chip == target.chip)
                targetPresets.push_back(&preset);
        }

        const auto presetCount = static_cast<int>(targetPresets.size());
        const auto missingPresetCount = std::max(0, target.minimumPresetCount - presetCount);

        std::vector<NamedPresetCount> targetRoleCounts;
        std::vector<std::string> missingRoles;
        for (const auto& role : target.requiredRoles)
        {
            const auto roleCount = static_cast<size_t>(std::count_if(targetPresets.begin(), targetPresets.end(), [&role](const auto* preset)
            {
                return preset != nullptr && chipper::presetRoleFor(*preset) == role;
            }));
            targetRoleCounts.push_back({ role, roleCount });
            if (roleCount == 0u)
                missingRoles.push_back(role);
        }

        std::vector<NamedPresetCount> targetReferenceTagCounts;
        std::vector<std::string> missingReferenceTags;
        for (const auto& tag : target.referenceTags)
        {
            size_t tagCount = 0u;
            for (const auto* preset : targetPresets)
            {
                if (preset == nullptr)
                    continue;

                const auto tags = chipper::presetTagsFor(*preset);
                if (std::find(tags.begin(), tags.end(), tag) != tags.end())
                    ++tagCount;
            }

            targetReferenceTagCounts.push_back({ tag, tagCount });
            if (tagCount == 0u)
                missingReferenceTags.push_back(tag);
        }

        out << "      {\n"
            << "        \"chipKey\": ";
        writeJsonString(out, chipModeKey(target.chip));
        out << ",\n"
            << "        \"chip\": ";
        writeJsonString(out, chipper::toString(target.chip));
        out << ",\n"
            << "        \"source\": ";
        writeJsonString(out, "Furnace-informed clean-room coverage target");
        out << ",\n"
            << "        \"minimumPresetCount\": " << target.minimumPresetCount << ",\n"
            << "        \"coveredPresetCount\": " << presetCount << ",\n"
            << "        \"missingPresetCount\": " << missingPresetCount << ",\n"
            << "        \"requiredRoles\": ";
        writeJsonStringArray(out, target.requiredRoles, "          ");
        out << ",\n"
            << "        \"referenceTags\": ";
        writeJsonStringArray(out, target.referenceTags, "          ");
        out << ",\n"
            << "        \"note\": ";
        writeJsonString(out, target.note);
        out << ",\n"
            << "        \"targetRoleCount\": " << target.requiredRoles.size() << ",\n"
            << "        \"coveredRoleCount\": " << (target.requiredRoles.size() - missingRoles.size()) << ",\n"
            << "        \"roleCounts\": [\n";
        for (size_t i = 0; i < targetRoleCounts.size(); ++i)
        {
            const auto& entry = targetRoleCounts[i];
            out << "          { \"role\": ";
            writeJsonString(out, entry.name);
            out << ", \"presetCount\": " << entry.count << " }"
                << (i + 1u == targetRoleCounts.size() ? "\n" : ",\n");
        }
        out << "        ],\n"
            << "        \"missingRoles\": ";
        writeJsonStringArray(out, missingRoles, "          ");
        out << ",\n"
            << "        \"targetReferenceTagCount\": " << target.referenceTags.size() << ",\n"
            << "        \"coveredReferenceTagCount\": " << (target.referenceTags.size() - missingReferenceTags.size()) << ",\n"
            << "        \"referenceTagCounts\": [\n";
        for (size_t i = 0; i < targetReferenceTagCounts.size(); ++i)
        {
            const auto& entry = targetReferenceTagCounts[i];
            out << "          { \"tag\": ";
            writeJsonString(out, entry.name);
            out << ", \"presetCount\": " << entry.count << " }"
                << (i + 1u == targetReferenceTagCounts.size() ? "\n" : ",\n");
        }
        out << "        ],\n"
            << "        \"missingReferenceTags\": ";
        writeJsonStringArray(out, missingReferenceTags, "          ");
        out << "\n"
            << "      }";
        wroteTarget = true;
    }
    out << "\n"
        << "    ]\n"
        << "  },\n"
        << "  \"presets\": [\n";
    bool wrotePreset = false;
    for (const auto& preset : presets)
    {
        if (! includesMode(modes, preset.chip))
            continue;

        if (wrotePreset)
            out << ",\n";

        const auto fmOperatorLevels = chipper::fmOperatorLevelsForPreset(preset);

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
            << "      \"role\": ";
        writeJsonString(out, chipper::presetRoleFor(preset));
        out << ",\n"
            << "      \"engine\": ";
        writeJsonString(out, chipper::presetEngineFor(preset));
        out << ",\n"
            << "      \"tags\": ";
        writeJsonStringArray(out, chipper::presetTagsFor(preset), "        ");
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
            << "      \"fmOperatorLevels\": [" << fmOperatorLevels[0] << ", "
            << fmOperatorLevels[1] << ", "
            << fmOperatorLevels[2] << ", "
            << fmOperatorLevels[3] << "],\n"
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
            << "      \"dmgWaveLevel\": " << preset.dmgWaveLevel << ",\n"
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
            << "      \"nesDmcDirectLevel\": " << preset.nesDmcDirectLevel << ",\n"
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

void writePresetListJson(const std::filesystem::path& path, const std::vector<chipper::ChipMode>& modes)
{
    std::ofstream out(path);
    if (! out)
        throw std::runtime_error("Could not write preset JSON: " + path.string());

    out << "{\n"
        << "  \"schema\": \"chipper.presets.v1\",\n";
    writePresetCatalogJson(out, modes);
    out << "}\n";
}

void writeDebugJson(const std::filesystem::path& path,
                    const Options& options,
                    const chipper::PatchConfig& patch,
                    const chipper::ChipCore& core,
                    const chipper::RenderStats& stats,
                    std::string_view renderEndCoreStateJson,
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
        << "  \"verificationBadge\": ";
    writeJsonString(out, chipper::descriptorFor(options.chip).verification.badge);
    out << ",\n"
        << "  \"cycleAccurate\": " << (chipper::descriptorFor(options.chip).verification.cycleAccurate ? "true" : "false") << ",\n"
        << "  \"clockHz\": " << options.clock << ",\n"
        << "  \"sampleRate\": " << options.sampleRate << ",\n"
        << "  \"seconds\": " << options.seconds << ",\n"
        << "  \"note\": " << options.note << ",\n"
        << "  \"outputDb\": " << options.outputDb << ",\n"
        << "  \"outputGain\": " << decibelsToGain(options.outputDb) << ",\n"
        << "  \"stereoSpread\": " << patch.stereoSpread << ",\n"
        << "  \"sidFilterRouting\": " << patch.sidFilterRouting << ",\n"
        << "  \"fmOperatorLevels\": [" << patch.fmOperatorLevels[0] << ", "
        << patch.fmOperatorLevels[1] << ", "
        << patch.fmOperatorLevels[2] << ", "
        << patch.fmOperatorLevels[3] << "],\n"
        << "  \"sidVoice2PulseWidth\": " << patch.sidVoice2PulseWidth << ",\n"
        << "  \"sidVoice3PulseWidth\": " << patch.sidVoice3PulseWidth << ",\n"
        << "  \"pulse2Duty\": " << patch.pulse2Duty << ",\n"
        << "  \"nesDmcDirectLevel\": " << patch.nesDmcDirectLevel << ",\n"
        << "  \"nesDmcRateIndex\": " << patch.nesDmcRateIndex << ",\n"
        << "  \"nesDmcLoop\": " << (patch.nesDmcLoop ? "true" : "false") << ",\n"
        << "  \"nesDmcOnly\": " << (patch.nesDmcOnly ? "true" : "false") << ",\n"
        << "  \"nesDmcSamplePath\": ";
    writeJsonString(out, options.nesDmcSamplePath.string());
    out << ",\n"
        << "  \"spc700SampleSlot\": " << options.spc700SampleSlot << ",\n"
        << "  \"spc700MapRoot\": " << options.spc700MapRoot << ",\n"
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
        << "  \"tailRms\": " << stats.tailRms << ",\n"
        << "  \"zeroCrossings\": " << stats.zeroCrossings << ",\n"
        << "  \"descriptor\": ";
    writeDescriptorJson(out, options.chip);
    out << ",\n"
        << "  \"renderEndCoreState\": " << renderEndCoreStateJson << ",\n"
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
                : options.metadataMode == MetadataMode::listPresets && ! options.chipWasProvided
                    ? chipper::chipModeOrder()
                    : std::vector<chipper::ChipMode> { options.chip };
            if (options.metadataMode == MetadataMode::listPresets)
            {
                writePresetListJson(options.debugPath, modes);
                std::cout << "Wrote preset metadata to " << options.debugPath.string() << "\n";
            }
            else
            {
                writeDescriptorListJson(options.debugPath, modes);
                std::cout << "Wrote descriptor metadata to " << options.debugPath.string() << "\n";
            }
            return 0;
        }

        applyMacroTemplateDefaults(options);
        auto core = chipper::createChipCore(options.chip, options.accuracy);
        core->reset(options.sampleRate, options.clock);
        if (! options.nesDmcSamplePath.empty())
            core->setExternalSampleData(loadBinaryFile(options.nesDmcSamplePath));
        if (! options.paulaSamplePath.empty())
        {
            std::vector<chipper::ExternalSampleData> bank;
            bank.push_back(loadPaulaSampleFile(options.paulaSamplePath));
            core->setExternalSampleBank(std::move(bank), 0);
        }
        if (! options.spc700BrrBankHex.empty())
        {
            std::vector<std::vector<uint8_t>> bank;
            bank.reserve(options.spc700BrrBankHex.size());
            for (const auto& hex : options.spc700BrrBankHex)
                bank.push_back(parseHexBytes(hex));
            auto selectedSlot = options.spc700SampleSlot;
            if (options.spc700MapRoot >= 0)
            {
                const auto mappedSlot = options.note - options.spc700MapRoot;
                selectedSlot = mappedSlot >= 0 && mappedSlot < static_cast<int>(bank.size()) ? mappedSlot : -1;
            }
            core->setExternalSampleBank(std::move(bank), selectedSlot);
        }
        else if (! options.spc700BrrSamplePath.empty())
            core->setExternalSampleData(loadBinaryFile(options.spc700BrrSamplePath));
        else if (! options.spc700BrrHex.empty())
            core->setExternalSampleData(parseHexBytes(options.spc700BrrHex));
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
                                                    options.sidFilterRouting,
                                                    options.sidVoice2PulseWidth,
                                                    options.sidVoice3PulseWidth,
                                                    options.nesDmcDirectLevel,
                                                    options.nesDmcRateIndex,
                                                    options.nesDmcLoop,
                                                    options.nesDmcOnly,
                                                    options.spc700LoopStart,
                                                    options.spc700LoopEnd,
                                                    options.voiceSampleSlots,
                                                    options.fmOperatorLevels);
        core->setPatch(patch);
        const auto events = loadEvents(options.eventFile);
        const auto registerWriteCount = static_cast<size_t>(std::count_if(events.begin(), events.end(), [](const auto& event) { return event.type == EventType::write; }));
        const auto noteEventCount = events.size() - registerWriteCount;

        const auto sampleCount = static_cast<uint64_t>(std::ceil(options.seconds * options.sampleRate));
        std::vector<chipper::StereoFrame> frames;
        frames.reserve(static_cast<size_t>(sampleCount));

        const auto hasEnabledSource = std::any_of(options.sourceEnabled.begin(),
                                                  options.sourceEnabled.end(),
                                                  [](bool enabled) { return enabled; });
        const auto hasAudibleDmcDirect = options.chip == chipper::ChipMode::nes && options.nesDmcDirectLevel > 0.0f;
        if (noteEventCount == 0 && (hasEnabledSource || hasAudibleDmcDirect))
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

        const auto renderEndCoreStateJson = core->debugStateJson();
        core->noteOff(options.note);
        applyOutputGain(frames, options.outputDb);
        const auto stats = calculateStats(frames);
        writeWav(options.wavPath, frames, options.sampleRate);
        writeDebugJson(options.debugPath, options, patch, *core, stats, renderEndCoreStateJson, registerWriteCount, noteEventCount);

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
