#include "Engine/ChipCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace chipper
{
namespace
{
constexpr double twoPi = 6.28318530717958647692;

double clamp01(double value)
{
    return std::clamp(value, 0.0, 1.0);
}

double wrapPhase(double phase)
{
    phase -= std::floor(phase);
    return phase;
}

std::string lower(std::string_view text)
{
    std::string out(text.begin(), text.end());
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    out.erase(std::remove_if(out.begin(), out.end(), [](char c) { return c == '_' || c == '-' || c == '/' || c == ' '; }), out.end());
    return out;
}

std::string jsonEscape(std::string_view text)
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

class UnsupportedCore final : public ChipCore
{
public:
    UnsupportedCore(ChipMode selectedMode, AccuracyMode selectedAccuracy)
        : selected(selectedMode), accuracy(selectedAccuracy)
    {
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz;
    }

    void writeRegister(uint16_t, uint8_t) override {}
    void noteOn(int, float) override {}
    void noteOff(int) override {}
    StereoFrame renderSample() override { return {}; }

    ChipMode mode() const override { return selected; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return toString(selected); }
    std::string implementedAccuracy() const override { return "not implemented"; }
    std::string limitations() const override
    {
        return "No permissive emulation core or clean-room register model has been integrated for this mode yet; output is silent.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"" << jsonEscape(modeName()) << "\","
             << "\"implementedAccuracy\":\"not implemented\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    ChipMode selected;
    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 0.0;
};

class NesApuCore final : public ChipCore
{
public:
    explicit NesApuCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 1789773.0;
        regs.fill(0);
        phase.fill(0.0);
        timer.fill(0);
        enabled = { true, true, true, true };
        lfsr = 1;
        heldNote = -1;
        noteVelocity = 0.0f;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        if (address >= 0x4000 && address <= 0x4017)
            regs[static_cast<size_t>(address - 0x4000)] = value;

        if (address == 0x4015)
        {
            enabled[0] = (value & 0x01) != 0;
            enabled[1] = (value & 0x02) != 0;
            enabled[2] = (value & 0x04) != 0;
            enabled[3] = (value & 0x08) != 0;
        }

        updateTimers();
    }

    void noteOn(int midiNote, float velocity) override
    {
        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto hz = midiNoteToHz(midiNote);
        const auto pulseTimer = static_cast<int>(std::max(0.0, std::round(clock / (16.0 * hz) - 1.0)));
        const auto triTimer = static_cast<int>(std::max(0.0, std::round(clock / (32.0 * hz) - 1.0)));

        writeRegister(0x4000, static_cast<uint8_t>((2u << 6u) | 0x0f)); // 50% duty, constant volume.
        writeRegister(0x4002, static_cast<uint8_t>(pulseTimer & 0xff));
        writeRegister(0x4003, static_cast<uint8_t>((pulseTimer >> 8) & 0x07));
        writeRegister(0x4004, static_cast<uint8_t>((1u << 6u) | 0x0b)); // second pulse at 25%.
        writeRegister(0x4006, static_cast<uint8_t>(pulseTimer & 0xff));
        writeRegister(0x4007, static_cast<uint8_t>((pulseTimer >> 8) & 0x07));
        writeRegister(0x4008, 0x80);
        writeRegister(0x400a, static_cast<uint8_t>(triTimer & 0xff));
        writeRegister(0x400b, static_cast<uint8_t>((triTimer >> 8) & 0x07));
        writeRegister(0x400c, 0x08);
        writeRegister(0x400e, 0x04);
        writeRegister(0x4015, 0x0f);
    }

    void noteOff(int midiNote) override
    {
        if (midiNote == heldNote)
        {
            heldNote = -1;
            noteVelocity = 0.0f;
        }
    }

    StereoFrame renderSample() override
    {
        const auto p1 = enabled[0] ? renderPulse(0) : 0.0;
        const auto p2 = enabled[1] ? renderPulse(1) : 0.0;
        const auto tri = enabled[2] ? renderTriangle() : 0.0;
        const auto noi = enabled[3] ? renderNoise() : 0.0;

        const auto pulseSum = p1 + p2;
        const auto tndSum = tri / 8227.0 + noi / 12241.0;
        const auto pulseOut = pulseSum <= 0.0 ? 0.0 : 95.88 / ((8128.0 / pulseSum) + 100.0);
        const auto tndOut = tndSum <= 0.0 ? 0.0 : 159.79 / ((1.0 / tndSum) + 100.0);
        const auto mixed = static_cast<float>((pulseOut + tndOut) * 2.0 - 0.35);
        const auto scaled = mixed * noteVelocity;
        return { scaled, scaled };
    }

    ChipMode mode() const override { return ChipMode::nes; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "NES / RP2A03"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Pulse, triangle, noise, timers, duty, enable bits, and nonlinear mixer are approximated; DMC, frame sequencer, length counters, envelope/sweep edge cases, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"NES / RP2A03\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"pulseTimer1\":" << timer[0] << ","
             << "\"pulseTimer2\":" << timer[1] << ","
             << "\"triangleTimer\":" << timer[2] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    void updateTimers()
    {
        timer[0] = static_cast<uint16_t>(regs[0x02] | ((regs[0x03] & 0x07) << 8));
        timer[1] = static_cast<uint16_t>(regs[0x06] | ((regs[0x07] & 0x07) << 8));
        timer[2] = static_cast<uint16_t>(regs[0x0a] | ((regs[0x0b] & 0x07) << 8));
    }

    double dutyForPulse(int index) const
    {
        static constexpr std::array<double, 4> duty = { 0.125, 0.25, 0.5, 0.75 };
        const auto regIndex = index == 0 ? 0x00 : 0x04;
        return duty[(regs[regIndex] >> 6u) & 0x03u];
    }

    double constantVolume(int regIndex) const
    {
        return static_cast<double>(regs[regIndex] & 0x0f) / 15.0;
    }

    double renderPulse(int index)
    {
        const auto period = timer[index] + 1;
        const auto hz = clock / (16.0 * static_cast<double>(period));
        phase[index] = wrapPhase(phase[index] + hz / sampleRate);
        const auto value = phase[index] < dutyForPulse(index) ? 1.0 : 0.0;
        return value * 15.0 * constantVolume(index == 0 ? 0x00 : 0x04);
    }

    double renderTriangle()
    {
        const auto period = timer[2] + 1;
        const auto hz = clock / (32.0 * static_cast<double>(period));
        phase[2] = wrapPhase(phase[2] + hz / sampleRate);
        const auto tri = 1.0 - std::abs(phase[2] * 2.0 - 1.0);
        return tri * 15.0;
    }

    double renderNoise()
    {
        static constexpr std::array<int, 16> ntscPeriods = {
            4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
        };

        const auto noiseReg = regs[0x0e];
        const auto period = ntscPeriods[noiseReg & 0x0f];
        const auto hz = clock / static_cast<double>(period);
        phase[3] += hz / sampleRate;

        while (phase[3] >= 1.0)
        {
            phase[3] -= 1.0;
            const auto tap = (noiseReg & 0x80) != 0 ? 6u : 1u;
            const auto feedback = (lfsr & 1u) ^ ((lfsr >> tap) & 1u);
            lfsr = static_cast<uint16_t>((lfsr >> 1u) | (feedback << 14u));
            if (lfsr == 0)
                lfsr = 1;
        }

        const auto value = (~lfsr) & 1u;
        return static_cast<double>(value) * 15.0 * constantVolume(0x0c);
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 1789773.0;
    std::array<uint8_t, 0x18> regs {};
    std::array<double, 4> phase {};
    std::array<uint16_t, 3> timer {};
    std::array<bool, 4> enabled {};
    uint16_t lfsr = 1;
    int heldNote = -1;
    float noteVelocity = 0.0f;
};

class Ym2149Core final : public ChipCore
{
public:
    explicit Ym2149Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 2000000.0;
        regs.fill(0);
        tonePhase.fill(0.0);
        noisePhase = 0.0;
        envelopePhase = 0.0;
        lfsr = 0x1ffff;
        heldNote = -1;
        noteVelocity = 0.0f;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        regs[address & 0x0f] = value;
    }

    void noteOn(int midiNote, float velocity) override
    {
        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto period = static_cast<int>(std::max(1.0, std::round(clock / (16.0 * midiNoteToHz(midiNote)))));
        writeRegister(0, static_cast<uint8_t>(period & 0xff));
        writeRegister(1, static_cast<uint8_t>((period >> 8) & 0x0f));
        writeRegister(2, static_cast<uint8_t>((period * 5 / 4) & 0xff));
        writeRegister(3, static_cast<uint8_t>(((period * 5 / 4) >> 8) & 0x0f));
        writeRegister(4, static_cast<uint8_t>((period * 3 / 2) & 0xff));
        writeRegister(5, static_cast<uint8_t>(((period * 3 / 2) >> 8) & 0x0f));
        writeRegister(6, 0x08);
        writeRegister(7, 0x38); // Tone enabled, noise disabled for A/B/C.
        writeRegister(8, 0x0f);
        writeRegister(9, 0x0b);
        writeRegister(10, 0x09);
    }

    void noteOff(int midiNote) override
    {
        if (midiNote == heldNote)
        {
            heldNote = -1;
            noteVelocity = 0.0f;
        }
    }

    StereoFrame renderSample() override
    {
        updateNoise();
        updateEnvelope();

        double mix = 0.0;
        for (int ch = 0; ch < 3; ++ch)
            mix += renderChannel(ch);

        const auto sample = static_cast<float>((mix / 3.0) * noteVelocity * 0.7);
        return { sample, sample };
    }

    ChipMode mode() const override { return ChipMode::ym2149; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "YM2149 / AY-3-8910"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Tone/noise registers, mixer bits, volume registers, and basic envelope shapes are modeled; exact analog output curve and timing validation are still required.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2149 / AY-3-8910\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"periodA\":" << tonePeriod(0) << ","
             << "\"periodB\":" << tonePeriod(1) << ","
             << "\"periodC\":" << tonePeriod(2) << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    int tonePeriod(int channel) const
    {
        const auto lo = regs[static_cast<size_t>(channel * 2)];
        const auto hi = regs[static_cast<size_t>(channel * 2 + 1)] & 0x0f;
        return std::max(1, static_cast<int>(lo | (hi << 8)));
    }

    double channelVolume(int channel) const
    {
        const auto volReg = regs[static_cast<size_t>(8 + channel)];
        if ((volReg & 0x10) != 0)
            return envelopeVolume;
        return static_cast<double>(volReg & 0x0f) / 15.0;
    }

    void updateNoise()
    {
        const auto period = std::max(1, static_cast<int>(regs[6] & 0x1f));
        const auto hz = clock / (16.0 * static_cast<double>(period));
        noisePhase += hz / sampleRate;
        while (noisePhase >= 1.0)
        {
            noisePhase -= 1.0;
            const auto feedback = (lfsr & 1u) ^ ((lfsr >> 3u) & 1u);
            lfsr = ((lfsr >> 1u) | (feedback << 16u)) & 0x1ffffu;
            if (lfsr == 0)
                lfsr = 1;
        }
    }

    void updateEnvelope()
    {
        const auto period = std::max(1, static_cast<int>(regs[11] | (regs[12] << 8)));
        const auto hz = clock / (256.0 * static_cast<double>(period));
        envelopePhase = wrapPhase(envelopePhase + hz / sampleRate);
        const auto shape = regs[13] & 0x0f;
        const auto rising = (shape & 0x04) != 0;
        const auto hold = (shape & 0x01) != 0;
        auto value = rising ? envelopePhase : (1.0 - envelopePhase);
        if (hold && envelopePhase > 0.98)
            value = rising ? 1.0 : 0.0;
        envelopeVolume = clamp01(value);
    }

    double renderChannel(int channel)
    {
        const auto period = tonePeriod(channel);
        const auto hz = clock / (16.0 * static_cast<double>(period));
        tonePhase[static_cast<size_t>(channel)] = wrapPhase(tonePhase[static_cast<size_t>(channel)] + hz / sampleRate);

        const auto toneDisabled = (regs[7] & (1u << channel)) != 0;
        const auto noiseDisabled = (regs[7] & (1u << (channel + 3))) != 0;
        const auto tone = tonePhase[static_cast<size_t>(channel)] < 0.5 ? 1.0 : -1.0;
        const auto noise = (lfsr & 1u) != 0 ? 1.0 : -1.0;
        const auto gate = (toneDisabled ? 1.0 : tone) * (noiseDisabled ? 1.0 : noise);
        return gate * channelVolume(channel);
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 2000000.0;
    std::array<uint8_t, 16> regs {};
    std::array<double, 3> tonePhase {};
    double noisePhase = 0.0;
    double envelopePhase = 0.0;
    double envelopeVolume = 0.0;
    uint32_t lfsr = 0x1ffff;
    int heldNote = -1;
    float noteVelocity = 0.0f;
};

class Sn76489Core final : public ChipCore
{
public:
    explicit Sn76489Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        tonePeriod = { 0x3ff, 0x3ff, 0x3ff };
        attenuation = { 0x0f, 0x0f, 0x0f, 0x0f };
        phase.fill(0.0);
        latchedChannel = 0;
        latchedIsVolume = false;
        noiseControl = 0x03;
        lfsr = 0x8000;
        noisePhase = 0.0;
        heldNote = -1;
        noteVelocity = 0.0f;
    }

    void writeRegister(uint16_t, uint8_t value) override
    {
        if ((value & 0x80) != 0)
        {
            latchedChannel = (value >> 5u) & 0x03u;
            latchedIsVolume = (value & 0x10u) != 0;
            const auto data = value & 0x0fu;
            if (latchedIsVolume)
                attenuation[latchedChannel] = data;
            else if (latchedChannel == 3)
                noiseControl = data & 0x07u;
            else
                tonePeriod[latchedChannel] = static_cast<uint16_t>((tonePeriod[latchedChannel] & 0x3f0u) | data);
        }
        else if (latchedIsVolume)
        {
            attenuation[latchedChannel] = value & 0x0fu;
        }
        else if (latchedChannel < 3)
        {
            tonePeriod[latchedChannel] = static_cast<uint16_t>(((value & 0x3fu) << 4u) | (tonePeriod[latchedChannel] & 0x0fu));
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto period = static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * midiNoteToHz(midiNote))), 1.0, 1023.0));
        tonePeriod[0] = period;
        tonePeriod[1] = static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * midiNoteToHz(midiNote + 7))), 1.0, 1023.0));
        tonePeriod[2] = static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * midiNoteToHz(midiNote + 12))), 1.0, 1023.0));
        attenuation = { 0x02, 0x07, 0x09, 0x0f };
    }

    void noteOff(int midiNote) override
    {
        if (midiNote == heldNote)
        {
            heldNote = -1;
            noteVelocity = 0.0f;
        }
    }

    StereoFrame renderSample() override
    {
        double mix = 0.0;
        for (int ch = 0; ch < 3; ++ch)
            mix += renderTone(ch) * attenuationToLinear(attenuation[static_cast<size_t>(ch)]);

        mix += renderNoise() * attenuationToLinear(attenuation[3]);
        const auto sample = static_cast<float>((mix / 4.0) * noteVelocity * 0.8);
        return { sample, sample };
    }

    ChipMode mode() const override { return ChipMode::sn76489; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "SN76489 / Sega PSG"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Latch/data writes, tone periods, attenuation, and noise modes are modeled; exact variant behavior and hardware-level output validation are still required.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SN76489 / Sega PSG\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"period0\":" << tonePeriod[0] << ","
             << "\"period1\":" << tonePeriod[1] << ","
             << "\"period2\":" << tonePeriod[2] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    static double attenuationToLinear(uint8_t value)
    {
        if ((value & 0x0f) == 0x0f)
            return 0.0;
        return std::pow(10.0, -2.0 * static_cast<double>(value & 0x0f) / 20.0);
    }

    double renderTone(int channel)
    {
        const auto period = std::max<uint16_t>(1, tonePeriod[static_cast<size_t>(channel)]);
        const auto hz = clock / (32.0 * static_cast<double>(period));
        phase[static_cast<size_t>(channel)] = wrapPhase(phase[static_cast<size_t>(channel)] + hz / sampleRate);
        return phase[static_cast<size_t>(channel)] < 0.5 ? 1.0 : -1.0;
    }

    double renderNoise()
    {
        static constexpr std::array<int, 4> noiseDividers = { 512, 1024, 2048, 1 };
        const auto mode = noiseControl & 0x03u;
        const auto divider = mode == 3 ? std::max<int>(1, tonePeriod[2] * 32) : noiseDividers[mode];
        const auto hz = clock / static_cast<double>(divider);
        noisePhase += hz / sampleRate;
        while (noisePhase >= 1.0)
        {
            noisePhase -= 1.0;
            const auto white = (noiseControl & 0x04u) != 0;
            const auto feedback = white ? ((lfsr & 1u) ^ ((lfsr >> 3u) & 1u)) : (lfsr & 1u);
            lfsr = static_cast<uint16_t>((lfsr >> 1u) | (feedback << 15u));
            if (lfsr == 0)
                lfsr = 0x8000;
        }
        return (lfsr & 1u) != 0 ? 1.0 : -1.0;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    std::array<uint16_t, 3> tonePeriod {};
    std::array<uint8_t, 4> attenuation {};
    std::array<double, 3> phase {};
    uint8_t latchedChannel = 0;
    bool latchedIsVolume = false;
    uint8_t noiseControl = 0x03;
    uint16_t lfsr = 0x8000;
    double noisePhase = 0.0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
};

} // namespace

std::unique_ptr<ChipCore> createChipCore(ChipMode mode, AccuracyMode accuracy)
{
    switch (mode)
    {
        case ChipMode::nes: return std::make_unique<NesApuCore>(accuracy);
        case ChipMode::ym2149: return std::make_unique<Ym2149Core>(accuracy);
        case ChipMode::sn76489: return std::make_unique<Sn76489Core>(accuracy);
        default: return std::make_unique<UnsupportedCore>(mode, accuracy);
    }
}

std::optional<ChipMode> parseChipMode(std::string_view text)
{
    const auto key = lower(text);
    if (key == "nes" || key == "rp2a03") return ChipMode::nes;
    if (key == "dmg" || key == "gameboy" || key == "gameboydmg") return ChipMode::dmg;
    if (key == "sid" || key == "c64" || key == "commodore64") return ChipMode::sid;
    if (key == "ym2149" || key == "ay" || key == "ay38910" || key == "ay38910") return ChipMode::ym2149;
    if (key == "sn76489" || key == "segapsg" || key == "psg") return ChipMode::sn76489;
    if (key == "ym2612" || key == "opn2" || key == "genesis") return ChipMode::ym2612;
    if (key == "opl2" || key == "opl3" || key == "ym3812" || key == "ymf262") return ChipMode::opl3;
    if (key == "spc700" || key == "snes") return ChipMode::spc700;
    if (key == "pokey" || key == "atari") return ChipMode::pokey;
    if (key == "paula" || key == "amiga") return ChipMode::paula;
    if (key == "huc6280" || key == "pcengine" || key == "turbografx") return ChipMode::huc6280;
    if (key == "namco" || key == "namcowsg" || key == "wsg") return ChipMode::namcoWsg;
    if (key == "ym2151" || key == "opm") return ChipMode::ym2151;
    if (key == "ym2413" || key == "opll") return ChipMode::ym2413;
    if (key == "scc" || key == "konamiscc") return ChipMode::scc;
    if (key == "arcade") return ChipMode::arcade;
    if (key == "custom") return ChipMode::custom;
    return std::nullopt;
}

std::optional<AccuracyMode> parseAccuracyMode(std::string_view text)
{
    const auto key = lower(text);
    if (key == "inspired") return AccuracyMode::inspired;
    if (key == "hybrid") return AccuracyMode::hybrid;
    if (key == "authentic") return AccuracyMode::authentic;
    return std::nullopt;
}

std::string toString(ChipMode mode)
{
    switch (mode)
    {
        case ChipMode::nes: return "NES / RP2A03";
        case ChipMode::dmg: return "Game Boy / DMG APU";
        case ChipMode::sid: return "SID / C64";
        case ChipMode::ym2149: return "YM2149 / AY";
        case ChipMode::sn76489: return "SN76489 / Sega PSG";
        case ChipMode::ym2612: return "YM2612 / Genesis FM";
        case ChipMode::opl3: return "OPL2/OPL3 / DOS FM";
        case ChipMode::spc700: return "SNES SPC700-style";
        case ChipMode::pokey: return "Atari POKEY";
        case ChipMode::paula: return "Amiga Paula";
        case ChipMode::huc6280: return "PC Engine HuC6280";
        case ChipMode::namcoWsg: return "Namco arcade WSG";
        case ChipMode::ym2151: return "YM2151 arcade/X68000 FM";
        case ChipMode::ym2413: return "YM2413 / OPLL";
        case ChipMode::scc: return "Konami SCC";
        case ChipMode::arcade: return "Arcade Hybrid";
        case ChipMode::custom: return "Custom";
    }
    return "Unknown";
}

std::string toString(AccuracyMode mode)
{
    switch (mode)
    {
        case AccuracyMode::inspired: return "Inspired";
        case AccuracyMode::hybrid: return "Hybrid";
        case AccuracyMode::authentic: return "Authentic";
    }
    return "Unknown";
}

double midiNoteToHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

} // namespace chipper
