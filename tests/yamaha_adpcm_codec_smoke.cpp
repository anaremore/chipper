#include "Engine/YamahaAdpcmBank.h"
#include "Engine/YamahaAdpcmCodec.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace
{
bool expect(bool condition, const std::string& message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

uint32_t checksum(std::span<const uint8_t> bytes)
{
    auto value = 2166136261u;
    for (const auto byte : bytes)
    {
        value ^= byte;
        value *= 16777619u;
    }
    return value;
}
}

int main()
{
    auto ok = true;
    constexpr std::array<uint8_t, 8> goldenBytes { 0x01u, 0x23u, 0x45u, 0x67u, 0x89u, 0xabu, 0xcdu, 0xefu };
    constexpr std::array<int16_t, 16> goldenA {
        2 * 16, 8 * 16, 18 * 16, 32 * 16, 50 * 16, 76 * 16, 126 * 16, 238 * 16,
        221 * 16, 173 * 16, 100 * 16, 7 * 16, -102 * 16, -264 * 16, -572 * 16, -1267 * 16
    };
    constexpr std::array<int16_t, 16> goldenB {
        15, 62, 141, 252, 394, 603, 996, 1903,
        1759, 1373, 800, 86, -730, -1930, -4190, -9406
    };

    const auto decodedA = chipper::yamahaAdpcm::decodeA(goldenBytes);
    const auto decodedB = chipper::yamahaAdpcm::decodeB(goldenBytes);
    ok &= expect(decodedA == std::vector<int16_t>(goldenA.begin(), goldenA.end()),
                 "ADPCM-A golden decode must match ymfm's signed 12-bit wrap state");
    ok &= expect(decodedB == std::vector<int16_t>(goldenB.begin(), goldenB.end()),
                 "ADPCM-B golden decode must match ymfm's signed 16-bit saturating state");
    ok &= expect(chipper::yamahaAdpcm::encodeA(goldenA) == std::vector<uint8_t>(goldenBytes.begin(), goldenBytes.end()),
                 "ADPCM-A golden states must encode high-nibble first");
    ok &= expect(chipper::yamahaAdpcm::encodeB(goldenB) == std::vector<uint8_t>(goldenBytes.begin(), goldenBytes.end()),
                 "ADPCM-B golden states must encode high-nibble first");

    constexpr std::array<int16_t, 3> oddPcm { 32767, -32768, 0 };
    const auto alignedA = chipper::yamahaAdpcm::encodeA(oddPcm, 4u);
    const auto alignedB = chipper::yamahaAdpcm::encodeB(oddPcm, 256u);
    ok &= expect(alignedA.size() == 4u, "ADPCM-A conversion must settle-pad to its requested byte boundary");
    ok &= expect(alignedB.size() == 256u, "ADPCM-B conversion must settle-pad to its requested byte boundary");
    ok &= expect(chipper::yamahaAdpcm::decodeA(alignedA, oddPcm.size()).size() == oddPcm.size(),
                 "ADPCM-A preview decode must trim alignment padding");
    ok &= expect(chipper::yamahaAdpcm::decodeB(alignedB, oddPcm.size()).size() == oddPcm.size(),
                 "ADPCM-B preview decode must trim alignment padding");

    const auto generatedOpna = chipper::yamahaAdpcm::makeGeneratedOpnaRom();
    auto coveredOpnaBytes = size_t { 0u };
    for (size_t region = 0; region < chipper::yamahaAdpcm::opnaRegions.size(); ++region)
    {
        const auto& descriptor = chipper::yamahaAdpcm::opnaRegions[region];
        ok &= expect(descriptor.startByte == coveredOpnaBytes,
                     "OPNA rhythm regions must be contiguous in the fixed 8 KiB ROM");
        coveredOpnaBytes += descriptor.capacityBytes();
    }
    ok &= expect(coveredOpnaBytes == chipper::yamahaAdpcm::opnaRomBytesA,
                 "The six fixed OPNA rhythm regions must cover exactly 8 KiB");
    ok &= expect(checksum(generatedOpna) == 1383326200u,
                 "Moving OPNA bank geometry must preserve the generated rhythm ROM bytes");

    std::array<std::vector<uint8_t>, chipper::yamahaAdpcm::regionCountA> regions;
    regions[0].assign(256u, 0x11u);
    regions[2].assign(512u, 0x33u);
    regions[5].assign(256u, 0x66u);
    chipper::yamahaAdpcm::PackedAdpcmABank packed;
    std::string packError;
    ok &= expect(chipper::yamahaAdpcm::packOpnbRegions(regions, packed, packError),
                 "Aligned OPNB regions must pack deterministically");
    ok &= expect(packed.bytes.size() == 1024u && packed.activeMask == 0x25u,
                 "OPNB packing must retain logical holes without wasting memory pages");
    ok &= expect(packed.windows[0].populated && packed.windows[0].startByte == 0u
                     && packed.windows[0].endByteInclusive == 255u
                     && ! packed.windows[1].populated
                     && packed.windows[2].startByte == 256u
                     && packed.windows[2].endByteInclusive == 767u
                     && packed.windows[5].startByte == 768u
                     && packed.windows[5].endByteInclusive == 1023u,
                 "OPNB explicit windows must be non-overlapping, inclusive, and 256-byte aligned");

    regions[1].assign(255u, 0x22u);
    const auto previousPacked = packed.bytes;
    ok &= expect(! chipper::yamahaAdpcm::packOpnbRegions(regions, packed, packError)
                     && packed.bytes == previousPacked,
                 "Invalid OPNB alignment must fail atomically");

    std::vector<int16_t> sine(4096u);
    for (size_t i = 0; i < sine.size(); ++i)
        sine[i] = static_cast<int16_t>(std::lround(std::sin(static_cast<double>(i) * 0.071) * 24000.0));
    const auto roundTripB = chipper::yamahaAdpcm::decodeB(chipper::yamahaAdpcm::encodeB(sine), sine.size());
    auto squaredError = 0.0;
    auto squaredSignal = 0.0;
    for (size_t i = 0; i < sine.size(); ++i)
    {
        const auto error = static_cast<double>(roundTripB[i]) - static_cast<double>(sine[i]);
        squaredError += error * error;
        squaredSignal += static_cast<double>(sine[i]) * static_cast<double>(sine[i]);
    }
    ok &= expect(squaredError / squaredSignal < 0.02,
                 "ADPCM-B sine reconstruction error must remain below the deterministic quality bound");

    if (! ok)
        return 1;
    std::cout << "Yamaha ADPCM codec smoke passed\n";
    return 0;
}
