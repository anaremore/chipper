#include "YamahaAdpcmCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace chipper::yamahaAdpcm
{
namespace
{
constexpr std::array<uint16_t, 49> stepsA {
    16, 17, 19, 21, 23, 25, 28,
    31, 34, 37, 41, 45, 50, 55,
    60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209,
    230, 253, 279, 307, 337, 371, 408,
    449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552
};
constexpr std::array<int8_t, 8> stepIndexDeltaA { -1, -1, -1, -1, 2, 5, 7, 9 };
constexpr std::array<uint8_t, 8> stepScaleB { 57, 57, 57, 57, 77, 102, 128, 153 };
constexpr int stepMinB = 127;
constexpr int stepMaxB = 24576;

int signed12(int value)
{
    value &= 0x0fff;
    return (value & 0x0800) != 0 ? value - 0x1000 : value;
}

int decodeNibbleA(uint8_t nibble, int& accumulator, int& stepIndex)
{
    auto delta = (2 * static_cast<int>(nibble & 0x07u) + 1) * static_cast<int>(stepsA[static_cast<size_t>(stepIndex)]) / 8;
    if ((nibble & 0x08u) != 0)
        delta = -delta;
    accumulator = (accumulator + delta) & 0x0fff;
    stepIndex = std::clamp(stepIndex + static_cast<int>(stepIndexDeltaA[nibble & 0x07u]), 0, 48);
    return signed12(accumulator);
}

int decodeNibbleB(uint8_t nibble, int& accumulator, int& step)
{
    auto delta = (2 * static_cast<int>(nibble & 0x07u) + 1) * step / 8;
    if ((nibble & 0x08u) != 0)
        delta = -delta;
    accumulator = std::clamp(accumulator + delta, -32768, 32767);
    step = std::clamp((step * static_cast<int>(stepScaleB[nibble & 0x07u])) / 64, stepMinB, stepMaxB);
    return accumulator;
}

size_t alignedByteCount(size_t sampleCount, size_t byteAlignment)
{
    const auto encodedBytes = (sampleCount + 1u) / 2u;
    const auto alignment = std::max<size_t>(1u, byteAlignment);
    if (encodedBytes == 0u)
        return 0u;
    return ((encodedBytes + alignment - 1u) / alignment) * alignment;
}

template <typename TargetFn, typename CandidateFn, typename CommitFn>
std::vector<uint8_t> encode(std::span<const int16_t> pcm,
                            size_t byteAlignment,
                            TargetFn targetForSample,
                            CandidateFn candidateForNibble,
                            CommitFn commitNibble)
{
    const auto outputBytes = alignedByteCount(pcm.size(), byteAlignment);
    if (outputBytes == 0u)
        return {};

    std::vector<uint8_t> encoded(outputBytes, 0u);
    const auto nibbleCount = outputBytes * 2u;
    for (size_t index = 0; index < nibbleCount; ++index)
    {
        const auto target = index < pcm.size() ? targetForSample(pcm[index]) : 0;
        auto bestNibble = uint8_t { 0u };
        auto bestError = std::numeric_limits<int>::max();
        for (auto nibble = uint8_t { 0u }; nibble < 16u; ++nibble)
        {
            const auto candidate = candidateForNibble(nibble);
            const auto error = std::abs(candidate - target);
            if (error < bestError)
            {
                bestError = error;
                bestNibble = nibble;
            }
        }

        commitNibble(bestNibble);
        auto& byte = encoded[index / 2u];
        if ((index & 1u) == 0u)
            byte = static_cast<uint8_t>(bestNibble << 4u);
        else
            byte = static_cast<uint8_t>(byte | bestNibble);
    }
    return encoded;
}

template <typename DecodeFn, typename ScaleFn>
std::vector<int16_t> decode(std::span<const uint8_t> bytes,
                            size_t requestedSampleCount,
                            DecodeFn decodeNibble,
                            ScaleFn scaleSample)
{
    const auto availableSamples = bytes.size() * 2u;
    const auto sampleCount = std::min(availableSamples, requestedSampleCount);
    std::vector<int16_t> pcm;
    pcm.reserve(sampleCount);
    for (size_t index = 0; index < sampleCount; ++index)
    {
        const auto byte = bytes[index / 2u];
        const auto nibble = static_cast<uint8_t>((index & 1u) == 0u ? byte >> 4u : byte & 0x0fu);
        pcm.push_back(scaleSample(decodeNibble(nibble)));
    }
    return pcm;
}
}

std::vector<uint8_t> encodeA(std::span<const int16_t> pcm, size_t byteAlignment)
{
    auto accumulator = 0;
    auto stepIndex = 0;
    return encode(
        pcm,
        byteAlignment,
        [](int16_t sample)
        {
            return std::clamp(static_cast<int>(std::lround(static_cast<double>(sample) / 16.0)), -2048, 2047);
        },
        [&accumulator, &stepIndex](uint8_t nibble)
        {
            auto candidateAccumulator = accumulator;
            auto candidateStepIndex = stepIndex;
            return decodeNibbleA(nibble, candidateAccumulator, candidateStepIndex);
        },
        [&accumulator, &stepIndex](uint8_t nibble)
        {
            decodeNibbleA(nibble, accumulator, stepIndex);
        });
}

std::vector<uint8_t> encodeB(std::span<const int16_t> pcm, size_t byteAlignment)
{
    auto accumulator = 0;
    auto step = stepMinB;
    return encode(
        pcm,
        byteAlignment,
        [](int16_t sample) { return static_cast<int>(sample); },
        [&accumulator, &step](uint8_t nibble)
        {
            auto candidateAccumulator = accumulator;
            auto candidateStep = step;
            return decodeNibbleB(nibble, candidateAccumulator, candidateStep);
        },
        [&accumulator, &step](uint8_t nibble)
        {
            decodeNibbleB(nibble, accumulator, step);
        });
}

std::vector<int16_t> decodeA(std::span<const uint8_t> bytes, size_t sampleCount)
{
    auto accumulator = 0;
    auto stepIndex = 0;
    return decode(
        bytes,
        sampleCount,
        [&accumulator, &stepIndex](uint8_t nibble)
        {
            return decodeNibbleA(nibble, accumulator, stepIndex);
        },
        [](int sample)
        {
            return static_cast<int16_t>(std::clamp(sample * 16, -32768, 32767));
        });
}

std::vector<int16_t> decodeB(std::span<const uint8_t> bytes, size_t sampleCount)
{
    auto accumulator = 0;
    auto step = stepMinB;
    return decode(
        bytes,
        sampleCount,
        [&accumulator, &step](uint8_t nibble)
        {
            return decodeNibbleB(nibble, accumulator, step);
        },
        [](int sample) { return static_cast<int16_t>(sample); });
}
}
