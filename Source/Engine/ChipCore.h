#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace chipper
{

enum class ChipMode
{
    nes,
    dmg,
    sid,
    ym2149,
    sn76489,
    ym2612,
    opl3,
    spc700,
    pokey,
    paula,
    huc6280,
    namcoWsg,
    ym2151,
    ym2413,
    scc
};

enum class AccuracyMode
{
    inspired,
    hybrid,
    authentic
};

enum class MacroKind
{
    manual,
    coin,
    bass,
    lead,
    arp,
    drum,
    hit,
    laser,
    jump,
    powerUp
};

enum class PlayMode
{
    stack,
    chipPoly,
    manual,
    clone
};

struct PatchConfig
{
    MacroKind macro = MacroKind::manual;
    float control1 = 0.5f;
    float control2 = 0.5f;
    float control3 = 0.5f;
    float control4 = 0.5f;
    PlayMode playMode = PlayMode::stack;
    std::array<bool, 8> sourceEnabled { true, true, true, true, true, true, true, true };
    std::array<float, 8> sourceLevels { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float stereoSpread = 0.0f;
    int sidFilterRouting = 0;
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
    int pulse2Duty = 0;
    int dmgWaveLevel = 0;
    int dmgStereoRoute = 0;
    int ymEnvelopeShape = 0;
    int ymChannelAMix = 0;
    int ymChannelBMix = 0;
    int ymChannelCMix = 0;
    // Generic chip noise-mode selector. SN76489 currently uses all 5 choices;
    // NES clamps to Macro/Long/Short via its descriptor.
    int snNoiseMode = 0;
    int sidVoice2WaveShape = 0;
    int sidVoice3WaveShape = 0;
    float sidVoice2PulseWidth = 0.5f;
    float sidVoice3PulseWidth = 0.5f;
    float nesDmcDirectLevel = 0.0f;
    int nesDmcRateIndex = 15;
    bool nesDmcLoop = false;
    bool nesDmcOnly = false;
};

struct StereoFrame
{
    float left = 0.0f;
    float right = 0.0f;
};

struct RegisterWrite
{
    uint64_t sample = 0;
    uint16_t address = 0;
    uint8_t value = 0;
};

struct NoteEvent
{
    uint64_t sample = 0;
    int note = 60;
    float velocity = 1.0f;
    uint64_t lengthSamples = 0;
};

struct RenderStats
{
    double peak = 0.0;
    double rms = 0.0;
    double leftPeak = 0.0;
    double rightPeak = 0.0;
    double leftRms = 0.0;
    double rightRms = 0.0;
    uint64_t zeroCrossings = 0;
    uint64_t renderedSamples = 0;
};

class ChipCore
{
public:
    virtual ~ChipCore() = default;

    virtual void reset(double outputSampleRate, double chipClockHz) = 0;
    virtual void setPatch(const PatchConfig& patch) = 0;
    virtual void setExternalSampleData(std::vector<uint8_t> data) { (void) data; }
    virtual void writeRegister(uint16_t address, uint8_t value) = 0;
    virtual void noteOn(int midiNote, float velocity) = 0;
    virtual void noteOff(int midiNote) = 0;
    virtual StereoFrame renderSample() = 0;
    virtual std::vector<RegisterWrite> exportRegisterState() const = 0;

    virtual ChipMode mode() const = 0;
    virtual AccuracyMode requestedAccuracy() const = 0;
    virtual std::string modeName() const = 0;
    virtual std::string implementedAccuracy() const = 0;
    virtual std::string limitations() const = 0;
    virtual std::string debugStateJson() const = 0;
};

std::unique_ptr<ChipCore> createChipCore(ChipMode mode, AccuracyMode accuracy);

std::optional<ChipMode> parseChipMode(std::string_view text);
std::optional<AccuracyMode> parseAccuracyMode(std::string_view text);
std::optional<MacroKind> parseMacroKind(std::string_view text);
std::optional<PlayMode> parsePlayMode(std::string_view text);
std::string toString(ChipMode mode);
std::string toString(AccuracyMode mode);
std::string toString(MacroKind macro);
std::string toString(PlayMode playMode);
double midiNoteToHz(int midiNote);

} // namespace chipper
