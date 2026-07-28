#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace chipper::yamahaAdpcm
{
std::vector<uint8_t> encodeA(std::span<const int16_t> pcm, size_t byteAlignment = 1u);
std::vector<uint8_t> encodeB(std::span<const int16_t> pcm, size_t byteAlignment = 1u);

std::vector<int16_t> decodeA(std::span<const uint8_t> bytes,
                             size_t sampleCount = static_cast<size_t>(-1));
std::vector<int16_t> decodeB(std::span<const uint8_t> bytes,
                             size_t sampleCount = static_cast<size_t>(-1));
}
