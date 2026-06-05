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
    scc,
    arcade,
    custom
};

enum class AccuracyMode
{
    inspired,
    hybrid,
    authentic
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
    uint64_t zeroCrossings = 0;
    uint64_t renderedSamples = 0;
};

class ChipCore
{
public:
    virtual ~ChipCore() = default;

    virtual void reset(double outputSampleRate, double chipClockHz) = 0;
    virtual void writeRegister(uint16_t address, uint8_t value) = 0;
    virtual void noteOn(int midiNote, float velocity) = 0;
    virtual void noteOff(int midiNote) = 0;
    virtual StereoFrame renderSample() = 0;

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
std::string toString(ChipMode mode);
std::string toString(AccuracyMode mode);
double midiNoteToHz(int midiNote);

} // namespace chipper
