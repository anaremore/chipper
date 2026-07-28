#include "YamahaAdpcmBank.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace chipper::yamahaAdpcm
{
namespace
{
constexpr double twoPi = 6.28318530717958647692;

int signedAccumulatorA(int accumulator)
{
    accumulator &= 0x0fff;
    return (accumulator & 0x0800) != 0 ? accumulator - 0x1000 : accumulator;
}

double generatedNoise(size_t index, uint32_t seed)
{
    auto value = static_cast<uint32_t>(index) ^ seed;
    value ^= value << 13u;
    value ^= value >> 17u;
    value ^= value << 5u;
    return (static_cast<double>(value & 0xffffu) / 32767.5) - 1.0;
}

double generatedTarget(size_t instrument, size_t nibble, size_t totalNibbles)
{
    const auto t = totalNibbles > 1u
        ? static_cast<double>(nibble) / static_cast<double>(totalNibbles - 1u)
        : 0.0;
    const auto tail = std::max(0.0, 1.0 - t);
    switch (instrument)
    {
        case 0:
            return std::sin(twoPi * (3.0 * t + 1.8 * t * t)) * std::pow(tail, 1.65) * 1800.0;
        case 1:
            return (generatedNoise(nibble, 0x6d2bu) * 0.82 + std::sin(twoPi * 28.0 * t) * 0.18)
                * std::pow(tail, 1.1) * 1500.0;
        case 2:
            return (generatedNoise(nibble * 3u, 0xa671u) * 0.72
                    + std::sin(twoPi * (92.0 * t + 9.0 * t * t)) * 0.28)
                * std::pow(tail, 0.55) * 1250.0;
        case 3:
            return (generatedNoise(nibble * 5u, 0x41c6u) * 0.9 + std::sin(twoPi * 110.0 * t) * 0.1)
                * std::pow(tail, 2.4) * 1400.0;
        case 4:
            return std::sin(twoPi * (7.0 * t + 2.8 * t * t)) * std::pow(tail, 1.45) * 1600.0;
        case 5:
            return (std::sin(twoPi * 36.0 * t) * 0.55 + generatedNoise(nibble, 0xd31fu) * 0.45)
                * std::pow(tail, 3.2) * 1700.0;
        default:
            return 0.0;
    }
}

uint8_t encodeGeneratedNibble(double target, int& accumulator, int& stepIndex)
{
    static constexpr std::array<uint16_t, 49> steps {
        16, 17, 19, 21, 23, 25, 28,
        31, 34, 37, 41, 45, 50, 55,
        60, 66, 73, 80, 88, 97, 107,
        118, 130, 143, 157, 173, 190, 209,
        230, 253, 279, 307, 337, 371, 408,
        449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552
    };
    static constexpr std::array<int8_t, 8> stepIncrement { -1, -1, -1, -1, 2, 5, 7, 9 };

    auto bestNibble = 0;
    auto bestAccumulator = accumulator;
    auto bestError = std::numeric_limits<double>::max();
    for (auto nibble = 0; nibble < 16; ++nibble)
    {
        auto delta = (2 * (nibble & 0x07) + 1) * static_cast<int>(steps[static_cast<size_t>(stepIndex)]) / 8;
        if ((nibble & 0x08) != 0)
            delta = -delta;
        const auto nextAccumulator = (accumulator + delta) & 0x0fff;
        const auto error = std::abs(static_cast<double>(signedAccumulatorA(nextAccumulator)) - target);
        if (error < bestError)
        {
            bestError = error;
            bestNibble = nibble;
            bestAccumulator = nextAccumulator;
        }
    }

    accumulator = bestAccumulator;
    stepIndex = std::clamp(stepIndex + static_cast<int>(stepIncrement[static_cast<size_t>(bestNibble & 0x07)]),
                           0,
                           48);
    return static_cast<uint8_t>(bestNibble);
}

void fillGeneratedRegion(std::array<uint8_t, opnaRomBytesA>& rom, size_t instrument)
{
    if (instrument >= opnaRegions.size())
        return;
    const auto& region = opnaRegions[instrument];
    auto accumulator = 0;
    auto stepIndex = 0;
    const auto totalNibbles = region.capacityBytes() * 2u;
    for (size_t nibble = 0; nibble < totalNibbles; ++nibble)
    {
        const auto encoded = encodeGeneratedNibble(generatedTarget(instrument, nibble, totalNibbles),
                                                   accumulator,
                                                   stepIndex);
        auto& byte = rom[region.startByte + nibble / 2u];
        if ((nibble & 1u) == 0u)
            byte = static_cast<uint8_t>(encoded << 4u);
        else
            byte = static_cast<uint8_t>(byte | encoded);
    }
}
}

std::array<uint8_t, opnaRomBytesA> makeGeneratedOpnaRom()
{
    std::array<uint8_t, opnaRomBytesA> rom {};
    for (size_t region = 0; region < opnaRegions.size(); ++region)
        fillGeneratedRegion(rom, region);
    return rom;
}

bool packOpnbRegions(const std::array<std::vector<uint8_t>, regionCountA>& regions,
                     PackedAdpcmABank& packed,
                     std::string& error,
                     size_t maximumBytes)
{
    PackedAdpcmABank candidate;
    for (size_t region = 0; region < regions.size(); ++region)
    {
        const auto& bytes = regions[region];
        if (bytes.empty())
            continue;
        if ((bytes.size() % opnbPageBytesA) != 0u)
        {
            error = "ADPCM-A region " + std::to_string(region + 1u)
                + " is not aligned to a 256-byte YM2610 page";
            return false;
        }
        if (candidate.bytes.size() > maximumBytes || bytes.size() > maximumBytes - candidate.bytes.size())
        {
            error = "The six ADPCM-A regions exceed the 1 MiB YM2610 memory window";
            return false;
        }

        auto& window = candidate.windows[region];
        window.startByte = static_cast<uint32_t>(candidate.bytes.size());
        candidate.bytes.insert(candidate.bytes.end(), bytes.begin(), bytes.end());
        window.endByteInclusive = static_cast<uint32_t>(candidate.bytes.size() - 1u);
        window.populated = true;
        candidate.activeMask = static_cast<uint8_t>(candidate.activeMask | (1u << region));
    }

    packed = std::move(candidate);
    error.clear();
    return true;
}
}
