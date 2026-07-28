#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace chipper::yamahaAdpcm
{
constexpr size_t regionCountA = 6u;
constexpr size_t opnaRomBytesA = 0x2000u;
constexpr size_t opnbMemoryBytesA = 0x100000u;
constexpr size_t opnbPageBytesA = 0x100u;

struct OpnaRegionDescriptor
{
    std::string_view name;
    size_t startByte = 0u;
    size_t endByteInclusive = 0u;
    double clockDivider = 432.0;

    constexpr size_t capacityBytes() const
    {
        return endByteInclusive >= startByte ? endByteInclusive - startByte + 1u : 0u;
    }
};

constexpr std::array<OpnaRegionDescriptor, regionCountA> opnaRegions {{
    { "Bass drum", 0x0000u, 0x01bfu, 432.0 },
    { "Snare", 0x01c0u, 0x043fu, 432.0 },
    { "Top cymbal", 0x0440u, 0x1b7fu, 432.0 },
    { "Hi-hat", 0x1b80u, 0x1cffu, 432.0 },
    { "Tom", 0x1d00u, 0x1f7fu, 864.0 },
    { "Rim", 0x1f80u, 0x1fffu, 864.0 }
}};

struct AdpcmARegionWindow
{
    uint32_t startByte = 0u;
    uint32_t endByteInclusive = 0u;
    bool populated = false;
};

struct PackedAdpcmABank
{
    std::vector<uint8_t> bytes;
    std::array<AdpcmARegionWindow, regionCountA> windows {};
    uint8_t activeMask = 0u;
};

std::array<uint8_t, opnaRomBytesA> makeGeneratedOpnaRom();

bool packOpnbRegions(const std::array<std::vector<uint8_t>, regionCountA>& regions,
                     PackedAdpcmABank& packed,
                     std::string& error,
                     size_t maximumBytes = opnbMemoryBytesA);
}
