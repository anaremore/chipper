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
