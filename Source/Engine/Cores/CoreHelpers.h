#pragma once
#include "Engine/ChipCore.h"
#include "Engine/ChipDescriptors.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <utility>

extern "C"
{
#include "emu2149.h"
#include "emu2212.h"
#include "emu2413.h"
#include "emu76489.h"
}

#include "ymfm_opn.h"
#include "ymfm_opl.h"
#include "ymfm_opm.h"

namespace chipper::core_detail
{
constexpr double twoPi = 6.28318530717958647692;
constexpr size_t opnaAdpcmARomSize = 0x2000;
constexpr size_t opnaAdpcmBMaxBytes = 0x40000;
constexpr size_t opnbAdpcmAMaxBytes = 0x100000;
constexpr size_t opnbAdpcmBMaxBytes = 0x1000000;

inline double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

inline double wrapPhase(double phase)
{
    phase -= std::floor(phase);
    return phase;
}

inline std::string lower(std::string_view text)
{
    std::string out(text.begin(), text.end());
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    out.erase(std::remove_if(out.begin(), out.end(), [](char c) { return c == '_' || c == '-' || c == '/' || c == ' '; }), out.end());
    return out;
}

inline std::string jsonEscape(std::string_view text)
{
    std::ostringstream out;
    for (const auto c : text)
    {
        if (c == '"' || c == '\\')
            out << '\\' << c;
        else if (c == '\n')
            out << "\\n";
        else
            out << c;
    }
    return out.str();
}

inline std::array<uint8_t, opnaAdpcmARomSize> makeGeneratedOpnaAdpcmARom()
{
    return yamahaAdpcm::makeGeneratedOpnaRom();
}

inline uint32_t checksumOpnaAdpcmARom(const std::array<uint8_t, opnaAdpcmARomSize>& rom)
{
    auto hash = 2166136261u;
    for (const auto byte : rom)
    {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

inline uint32_t checksumBytes(const std::vector<uint8_t>& bytes)
{
    auto hash = 2166136261u;
    for (const auto byte : bytes)
    {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

enum class NesPulseDuty : uint8_t
{
    duty12_5 = 0,
    duty25 = 1,
    duty50 = 2,
    duty75 = 3
};

enum class DmgPulseDuty : uint8_t
{
    duty12_5 = 0,
    duty25 = 1,
    duty50 = 2,
    duty75 = 3
};

inline NesPulseDuty nesPulseDutyFromControl(float control)
{
    return static_cast<NesPulseDuty>(std::clamp(static_cast<int>(std::round(control * 3.0f)), 0, 3));
}

inline NesPulseDuty offsetNesPulseDuty(NesPulseDuty duty, int offset)
{
    return static_cast<NesPulseDuty>(std::clamp(static_cast<int>(duty) + offset, 0, 3));
}

inline NesPulseDuty nesPulse2DutyForPatch(const PatchConfig& patch, NesPulseDuty pulse1Duty, bool stackedMacroOffset)
{
    const auto choice = std::clamp(patch.pulse2Duty, 0, 4);
    if (choice > 0)
        return static_cast<NesPulseDuty>(choice - 1);

    return stackedMacroOffset ? offsetNesPulseDuty(pulse1Duty, 1) : pulse1Duty;
}

inline uint8_t nesPulseDutyBits(NesPulseDuty duty)
{
    return static_cast<uint8_t>(duty);
}

inline DmgPulseDuty dmgPulseDutyFromControl(float control)
{
    return static_cast<DmgPulseDuty>(std::clamp(static_cast<int>(std::round(control * 3.0f)), 0, 3));
}

inline DmgPulseDuty offsetDmgPulseDuty(DmgPulseDuty duty, int offset)
{
    return static_cast<DmgPulseDuty>(std::clamp(static_cast<int>(duty) + offset, 0, 3));
}

inline DmgPulseDuty dmgPulse2DutyForPatch(const PatchConfig& patch, DmgPulseDuty pulse1Duty, bool stackedMacroOffset)
{
    const auto choice = std::clamp(patch.pulse2Duty, 0, 4);
    if (choice > 0)
        return static_cast<DmgPulseDuty>(choice - 1);

    return stackedMacroOffset ? offsetDmgPulseDuty(pulse1Duty, 1) : pulse1Duty;
}

inline uint8_t dmgPulseDutyBits(DmgPulseDuty duty)
{
    return static_cast<uint8_t>(duty);
}

inline bool sourceEnabled(const PatchConfig& patch, size_t index)
{
    return index < patch.sourceEnabled.size() && patch.sourceEnabled[index];
}

inline double sourceLevel(const PatchConfig& patch, size_t index)
{
    return index < patch.sourceLevels.size() ? clamp01(patch.sourceLevels[index]) : 1.0;
}

inline StereoFrame modernStereoGains(const PatchConfig& patch, double basePosition)
{
    const auto pan = std::clamp(basePosition * clamp01(patch.stereoSpread), -1.0, 1.0);
    const auto left = pan > 0.0 ? 1.0 - (0.75 * pan) : 1.0;
    const auto right = pan < 0.0 ? 1.0 + (0.75 * pan) : 1.0;
    return { static_cast<float>(left), static_cast<float>(right) };
}

inline uint8_t sourceEnableMask(const PatchConfig& patch)
{
    uint8_t mask = 0;
    for (size_t i = 0; i < patch.sourceEnabled.size(); ++i)
    {
        if (patch.sourceEnabled[i])
            mask |= static_cast<uint8_t>(1u << i);
    }
    return mask;
}

inline uint8_t enabledSourceMaskOrRecipe(const PatchConfig& patch, uint8_t recipeMask)
{
    const auto requestedMask = sourceEnableMask(patch);
    return requestedMask != 0u ? requestedMask : recipeMask;
}

inline uint8_t ensureDmgRegisterVolume(uint8_t volume, uint8_t fallback)
{
    return volume > 0u ? volume : std::clamp<uint8_t>(fallback, static_cast<uint8_t>(1u), static_cast<uint8_t>(15u));
}

inline unsigned ensureNesRegisterVolume(unsigned volume, unsigned fallback)
{
    return volume > 0u ? volume : std::clamp<unsigned>(fallback, 1u, 15u);
}

inline bool envelopeDecayActive(const PatchConfig& patch)
{
    return patch.envelopeDecay > 0.01f;
}

inline uint8_t decayPeriodFromControl(float control, uint8_t maxPeriod)
{
    const auto period = std::round(static_cast<float>(maxPeriod) - (std::clamp(control, 0.0f, 1.0f) * static_cast<float>(maxPeriod - 1u)));
    return static_cast<uint8_t>(std::clamp(static_cast<int>(period), 1, static_cast<int>(maxPeriod)));
}

}
