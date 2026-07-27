#include "MotionData.h"

#include "ChipCore.h"

#include <algorithm>
#include <cmath>

namespace chipper
{

namespace
{
bool validRate(MotionRate rate) noexcept
{
    switch (rate)
    {
        case MotionRate::eighth:
        case MotionRate::sixteenth:
        case MotionRate::thirtySecond:
        case MotionRate::sixtyFourth:
            return true;
    }
    return false;
}

bool validGate(MotionGate gate) noexcept
{
    switch (gate)
    {
        case MotionGate::hold:
        case MotionGate::retrigger:
        case MotionGate::cut:
            return true;
    }
    return false;
}
}

MotionPattern sanitizeMotionPattern(const MotionPattern& pattern) noexcept
{
    auto sanitized = pattern;
    if (! validRate(sanitized.rate))
        sanitized.rate = MotionRate::sixteenth;
    sanitized.length = static_cast<uint8_t>(std::clamp(static_cast<int>(sanitized.length),
                                                       1,
                                                       static_cast<int>(motionStepCount)));
    for (auto& step : sanitized.steps)
    {
        step.pitch = static_cast<int8_t>(std::clamp(static_cast<int>(step.pitch),
                                                   motionMinimumPitch,
                                                   motionMaximumPitch));
        step.level = static_cast<uint8_t>(std::min(step.level, motionMaximumLevel));
        if (! validGate(step.gate))
            step.gate = MotionGate::hold;
    }
    return sanitized;
}

MotionPattern motionPatternTemplate(MotionTemplate type) noexcept
{
    MotionPattern pattern;
    if (type == MotionTemplate::init)
        return pattern;

    pattern.enabled = true;
    switch (type)
    {
        case MotionTemplate::majorArp:
        {
            constexpr std::array<int8_t, motionStepCount> pitches { 0, 4, 7, 12, 7, 4, 0, 12 };
            for (size_t step = 0; step < pattern.steps.size(); ++step)
            {
                pattern.steps[step].pitch = pitches[step];
                pattern.steps[step].gate = MotionGate::retrigger;
            }
            break;
        }
        case MotionTemplate::minorArp:
        {
            constexpr std::array<int8_t, motionStepCount> pitches { 0, 3, 7, 12, 7, 3, 0, 12 };
            for (size_t step = 0; step < pattern.steps.size(); ++step)
            {
                pattern.steps[step].pitch = pitches[step];
                pattern.steps[step].gate = MotionGate::retrigger;
            }
            break;
        }
        case MotionTemplate::rise:
            for (size_t step = 0; step < pattern.steps.size(); ++step)
                pattern.steps[step].pitch = static_cast<int8_t>(step * 2u);
            break;
        case MotionTemplate::fall:
            for (size_t step = 0; step < pattern.steps.size(); ++step)
                pattern.steps[step].pitch = static_cast<int8_t>(12 - static_cast<int>(step * 2u));
            break;
        case MotionTemplate::pulse:
            for (size_t step = 0; step < pattern.steps.size(); ++step)
            {
                pattern.steps[step].level = step % 2u == 0u ? motionMaximumLevel : 6u;
                pattern.steps[step].gate = step == 3u || step == 7u ? MotionGate::cut : MotionGate::retrigger;
            }
            break;
        case MotionTemplate::init:
            break;
    }
    return sanitizeMotionPattern(pattern);
}

double motionStepSamples(MotionRate rate, double bpm, double sampleRate) noexcept
{
    const auto safeRate = validRate(rate) ? rate : MotionRate::sixteenth;
    const auto safeBpm = std::clamp(std::isfinite(bpm) ? bpm : 120.0, 20.0, 400.0);
    const auto safeSampleRate = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0,
                                           1000.0,
                                           768000.0);
    const auto denominator = static_cast<double>(static_cast<uint8_t>(safeRate));
    return std::max(1.0, safeSampleRate * 60.0 / safeBpm * 4.0 / denominator);
}

std::string_view motionRateName(MotionRate rate) noexcept
{
    switch (rate)
    {
        case MotionRate::eighth: return "1/8";
        case MotionRate::sixteenth: return "1/16";
        case MotionRate::thirtySecond: return "1/32";
        case MotionRate::sixtyFourth: return "1/64";
    }
    return "1/16";
}

std::string_view motionGateName(MotionGate gate) noexcept
{
    switch (gate)
    {
        case MotionGate::hold: return "Hold";
        case MotionGate::retrigger: return "Trig";
        case MotionGate::cut: return "Cut";
    }
    return "Hold";
}

std::string_view motionDestinationForMode(ChipMode mode) noexcept
{
    switch (mode)
    {
        case ChipMode::nes:
        case ChipMode::nesVrc6:
        case ChipMode::nesFds:
        case ChipMode::nesSunsoft5b:
        case ChipMode::nesMmc5:
        case ChipMode::nesVrc7:
            return "timer / F-number + key; post-chip level";
        case ChipMode::dmg:
            return "NR13/NR14 period + trigger; post-chip level";
        case ChipMode::sid:
            return "frequency + gate; post-chip level";
        case ChipMode::ym2149:
        case ChipMode::sn76489:
        case ChipMode::saa1099:
        case ChipMode::pokey:
            return "tone period + gate; post-chip level";
        case ChipMode::ym2612:
        case ChipMode::opl3:
        case ChipMode::ym2151:
        case ChipMode::ym2413:
        case ChipMode::ym2203:
        case ChipMode::ym2608:
        case ChipMode::ym2610:
        case ChipMode::ym2610b:
            return "F-number/block + key-on; post-chip level";
        case ChipMode::spc700:
        case ChipMode::paula:
            return "sample pitch/period + key; post-chip level";
        case ChipMode::huc6280:
        case ChipMode::namcoWsg:
        case ChipMode::scc:
            return "frequency + key-on; post-chip level";
        case ChipMode::pcSpeaker:
            return "PIT divisor + speaker gate; post-chip level";
        case ChipMode::zxSpectrumBeeper:
            return "ULA delay pitch + EAR/MIC gate; post-chip level";
    }
    return "pitch + gate; post-chip level";
}

} // namespace chipper
