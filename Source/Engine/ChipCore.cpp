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

    void setPatch(const PatchConfig& newPatch) override { patch = newPatch; }
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
    PatchConfig patch;
};

class DmgApuCore final : public ChipCore
{
public:
    explicit DmgApuCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 4194304.0;
        regs.fill(0);
        phase.fill(0.0);
        enabled = { false, false, false, false };
        lfsr = 0x7fffu;
        heldNote = -1;
        noteVelocity = 0.0f;

        for (size_t i = 0; i < 16; ++i)
        {
            const auto high = static_cast<uint8_t>((i < 8 ? i : 15 - i) & 0x0f);
            const auto low = static_cast<uint8_t>((15 - high) & 0x0f);
            regs[0x20 + i] = static_cast<uint8_t>((high << 4u) | low);
        }
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto index = registerIndex(address);
        if (index < regs.size())
            regs[index] = value;

        switch (index)
        {
            case 0x04: if ((value & 0x80u) != 0) enabled[0] = true; break; // NR14
            case 0x09: if ((value & 0x80u) != 0) enabled[1] = true; break; // NR24
            case 0x0e: if ((value & 0x80u) != 0) enabled[2] = true; break; // NR34
            case 0x14: if ((value & 0x80u) != 0) enabled[3] = true; break; // NR44
            case 0x16:
                enabled[0] = (value & 0x01u) != 0;
                enabled[1] = (value & 0x02u) != 0;
                enabled[2] = (value & 0x04u) != 0;
                enabled[3] = (value & 0x08u) != 0;
                break;
            default:
                break;
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));

        const auto duty = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control1 * 3.0f)), 0, 3));
        const auto envelope = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control4 * 15.0f)), 1, 15));
        const auto noisePitch = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - patch.control3) * 7.0f)), 0, 7));
        auto ch1Note = midiNote;
        auto ch2Note = midiNote + 7;
        auto waveNote = midiNote - 12;
        auto ch1Vol = envelope;
        auto ch2Vol = static_cast<uint8_t>(std::max(0, envelope - 4));
        auto noiseVol = static_cast<uint8_t>(std::round(patch.control3 * 15.0f));
        auto enable = 0x83u;

        switch (patch.macro)
        {
            case MacroKind::coin:
                ch1Note = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                ch2Vol = 0;
                noiseVol = 0;
                enable = 0x81u;
                break;
            case MacroKind::bass:
                ch1Note = midiNote - 12;
                ch2Vol = 0;
                waveNote = midiNote - 24;
                noiseVol = 0;
                enable = 0x85u;
                break;
            case MacroKind::arp:
                ch2Note = midiNote + 7;
                waveNote = midiNote + 12;
                noiseVol = 0;
                enable = 0x87u;
                break;
            case MacroKind::drum:
                ch1Vol = 0;
                ch2Vol = 0;
                waveNote = midiNote - 24;
                noiseVol = static_cast<uint8_t>(std::max<int>(10, noiseVol));
                enable = 0x8cu;
                break;
            case MacroKind::hit:
                ch2Vol = 0;
                noiseVol = static_cast<uint8_t>(std::max<int>(8, noiseVol));
                enable = 0x89u;
                break;
            case MacroKind::laser:
                ch1Note = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                ch2Note = midiNote - 12;
                noiseVol = static_cast<uint8_t>(std::round(patch.control3 * 10.0f));
                enable = 0x8bu;
                break;
            case MacroKind::jump:
                ch1Note = midiNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                ch2Vol = 0;
                noiseVol = 0;
                enable = 0x81u;
                break;
            case MacroKind::powerUp:
                ch2Note = midiNote + 5;
                waveNote = midiNote + 12;
                noiseVol = 0;
                enable = 0x87u;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        writeRegister(0xff26, static_cast<uint8_t>(enable));
        writeRegister(0xff10, static_cast<uint8_t>(0x10u | std::clamp(static_cast<int>(std::round(patch.control2 * 7.0f)), 0, 7)));
        writePulseRegisters(0, duty, ch1Vol, ch1Note);
        writePulseRegisters(1, static_cast<uint8_t>(std::min<int>(3, duty + 1)), ch2Vol, ch2Note);
        writeWaveRegisters(waveNote, patch.macro == MacroKind::bass ? 0x20u : 0x40u);
        writeNoiseRegisters(noiseVol, noisePitch, patch.control3 > 0.55f);
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
        const auto wave = enabled[2] ? renderWave() : 0.0;
        const auto noise = enabled[3] ? renderNoise() : 0.0;
        const auto mixed = static_cast<float>(((p1 + p2 + wave + noise) / 4.0) * noteVelocity * 0.85);
        return { mixed, mixed };
    }

    ChipMode mode() const override { return ChipMode::dmg; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "Game Boy / DMG APU"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Pulse, wave RAM, noise, trigger bits, and core frequency formulas are modeled; envelope clocks, length counters, sweep edge cases, DAC quirks, stereo routing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Game Boy / DMG APU\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"pulse1FreqReg\":" << pulseFrequencyRegister(0) << ","
             << "\"pulse2FreqReg\":" << pulseFrequencyRegister(1) << ","
             << "\"waveFreqReg\":" << waveFrequencyRegister() << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    static size_t registerIndex(uint16_t address)
    {
        if (address >= 0xff10 && address <= 0xff3f)
            return static_cast<size_t>(address - 0xff10);
        if (address >= 0x10 && address <= 0x3f)
            return static_cast<size_t>(address - 0x10);
        return static_cast<size_t>(address & 0x3f);
    }

    uint16_t pulseFrequencyRegister(int channel) const
    {
        const auto loIndex = channel == 0 ? 0x03 : 0x08;
        const auto hiIndex = channel == 0 ? 0x04 : 0x09;
        return static_cast<uint16_t>(regs[loIndex] | ((regs[hiIndex] & 0x07u) << 8u));
    }

    uint16_t waveFrequencyRegister() const
    {
        return static_cast<uint16_t>(regs[0x0d] | ((regs[0x0e] & 0x07u) << 8u));
    }

    static uint16_t dmgFrequencyRegister(double baseClock, double divisor, int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(2048.0 - (baseClock / (divisor * hz))), 0.0, 2047.0));
    }

    void writePulseRegisters(int channel, uint8_t duty, uint8_t volume, int midiNote)
    {
        const auto base = channel == 0 ? 0xff11 : 0xff16;
        const auto freq = dmgFrequencyRegister(131072.0, 1.0, midiNote);
        writeRegister(static_cast<uint16_t>(base), static_cast<uint8_t>((duty << 6u) | 0x3fu));
        writeRegister(static_cast<uint16_t>(base + 1), static_cast<uint8_t>((std::min<uint8_t>(15, volume) << 4u) | 0x08u));
        writeRegister(static_cast<uint16_t>(base + 2), static_cast<uint8_t>(freq & 0xffu));
        writeRegister(static_cast<uint16_t>(base + 3), static_cast<uint8_t>(0x80u | ((freq >> 8u) & 0x07u)));
    }

    void writeWaveRegisters(int midiNote, unsigned outputLevelBits)
    {
        const auto freq = dmgFrequencyRegister(65536.0, 1.0, midiNote);
        writeRegister(0xff1a, 0x80);
        writeRegister(0xff1c, static_cast<uint8_t>(outputLevelBits & 0x60u));
        writeRegister(0xff1d, static_cast<uint8_t>(freq & 0xffu));
        writeRegister(0xff1e, static_cast<uint8_t>(0x80u | ((freq >> 8u) & 0x07u)));
    }

    void writeNoiseRegisters(uint8_t volume, uint8_t pitchCode, bool narrowMode)
    {
        writeRegister(0xff21, static_cast<uint8_t>((std::min<uint8_t>(15, volume) << 4u) | 0x08u));
        writeRegister(0xff22, static_cast<uint8_t>((pitchCode << 4u) | (narrowMode ? 0x08u : 0x00u) | 0x02u));
        writeRegister(0xff23, 0x80);
    }

    double envelopeVolume(size_t index) const
    {
        return static_cast<double>((regs[index] >> 4u) & 0x0fu) / 15.0;
    }

    double renderPulse(int channel)
    {
        static constexpr std::array<double, 4> dutyTable { 0.125, 0.25, 0.5, 0.75 };
        const auto freqReg = pulseFrequencyRegister(channel);
        const auto period = std::max(1, 2048 - static_cast<int>(freqReg));
        const auto hz = 131072.0 / static_cast<double>(period);
        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);

        const auto dutyIndex = (regs[channel == 0 ? 0x01 : 0x06] >> 6u) & 0x03u;
        const auto amp = phase[channel] < dutyTable[dutyIndex] ? 1.0 : -1.0;
        return amp * envelopeVolume(channel == 0 ? 0x02 : 0x07);
    }

    double renderWave()
    {
        const auto freqReg = waveFrequencyRegister();
        const auto period = std::max(1, 2048 - static_cast<int>(freqReg));
        const auto hz = 65536.0 / static_cast<double>(period);
        phase[2] = wrapPhase(phase[2] + hz / sampleRate);
        const auto sampleIndex = static_cast<size_t>(std::floor(phase[2] * 32.0)) & 0x1fu;
        const auto packed = regs[0x20 + (sampleIndex / 2)];
        const auto nibble = (sampleIndex & 1u) == 0 ? (packed >> 4u) : (packed & 0x0fu);
        const auto levelBits = (regs[0x0c] >> 5u) & 0x03u;
        const auto divider = levelBits == 0 ? 0.0 : static_cast<double>(1u << (levelBits - 1u));
        const auto normalized = (static_cast<double>(nibble) / 7.5) - 1.0;
        return divider == 0.0 ? 0.0 : normalized / divider;
    }

    double renderNoise()
    {
        static constexpr std::array<int, 8> divisors { 8, 16, 32, 48, 64, 80, 96, 112 };
        const auto nr43 = regs[0x12];
        const auto shift = (nr43 >> 4u) & 0x0fu;
        const auto divisor = divisors[nr43 & 0x07u];
        const auto hz = 524288.0 / static_cast<double>(divisor * (1u << std::min<unsigned>(shift, 14u)));
        phase[3] += hz / sampleRate;

        while (phase[3] >= 1.0)
        {
            phase[3] -= 1.0;
            const auto feedback = (lfsr & 1u) ^ ((lfsr >> 1u) & 1u);
            lfsr = static_cast<uint16_t>((lfsr >> 1u) | (feedback << 14u));
            if ((nr43 & 0x08u) != 0)
                lfsr = static_cast<uint16_t>((lfsr & ~(1u << 6u)) | (feedback << 6u));
            if (lfsr == 0)
                lfsr = 0x7fffu;
        }

        const auto amp = (lfsr & 1u) == 0 ? 1.0 : -1.0;
        return amp * envelopeVolume(0x11);
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 4194304.0;
    std::array<uint8_t, 0x30> regs {};
    std::array<double, 4> phase {};
    std::array<bool, 4> enabled {};
    uint16_t lfsr = 0x7fff;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    PatchConfig patch;
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

    void setPatch(const PatchConfig& newPatch) override
    {
        patch = newPatch;
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
        const auto duty = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control1 * 3.0f)), 0, 3));
        const auto noisePeriod = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - patch.control3) * 14.0f)), 0, 15));

        auto p1Note = midiNote;
        auto p2Note = midiNote;
        auto triNote = midiNote - 12;
        auto p1Vol = 15u;
        auto p2Vol = 8u;
        auto noiseVol = static_cast<unsigned>(std::round(patch.control3 * 12.0f));
        auto enable = 0x03u;
        auto noiseMode = 0x00u;

        switch (patch.macro)
        {
            case MacroKind::coin:
                p1Note = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                p2Vol = 0;
                noiseVol = 0;
                enable = 0x01u;
                break;
            case MacroKind::bass:
                p1Note = midiNote - 12;
                p2Note = midiNote - 24;
                triNote = midiNote - 24;
                p1Vol = 5u;
                p2Vol = 0u;
                noiseVol = 0;
                enable = 0x05u;
                break;
            case MacroKind::arp:
                p2Note = midiNote + 7 + static_cast<int>(std::round(patch.control2 * 5.0f));
                triNote = midiNote + 12;
                noiseVol = 0;
                enable = 0x07u;
                break;
            case MacroKind::drum:
                p1Note = midiNote - 24;
                p1Vol = 4u;
                p2Vol = 0u;
                triNote = midiNote - 36;
                noiseVol = static_cast<unsigned>(10u + std::round(patch.control3 * 5.0f));
                enable = 0x0du;
                noiseMode = patch.control3 > 0.55f ? 0x80u : 0x00u;
                break;
            case MacroKind::hit:
                p1Note = midiNote - 5;
                p2Vol = 0u;
                noiseVol = static_cast<unsigned>(8u + std::round(patch.control3 * 7.0f));
                enable = 0x09u;
                noiseMode = patch.control3 > 0.50f ? 0x80u : 0x00u;
                break;
            case MacroKind::laser:
                p1Note = midiNote + 7 + static_cast<int>(std::round(patch.control2 * 17.0f));
                p2Note = midiNote - 5;
                p2Vol = 5u;
                noiseVol = static_cast<unsigned>(std::round(patch.control3 * 8.0f));
                enable = 0x0bu;
                break;
            case MacroKind::jump:
                p1Note = midiNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                p2Vol = 0u;
                noiseVol = 0u;
                enable = 0x01u;
                break;
            case MacroKind::powerUp:
                p2Note = midiNote + 12;
                triNote = midiNote + 7;
                noiseVol = static_cast<unsigned>(std::round(patch.control3 * 5.0f));
                enable = 0x07u;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                enable = patch.control4 < 0.33f ? 0x04u : (patch.control4 > 0.66f ? 0x03u : 0x07u);
                break;
        }

        writePulseRegisters(0x4000, duty, p1Vol, p1Note);
        writePulseRegisters(0x4004, static_cast<uint8_t>(std::min<int>(3, duty + 1)), p2Vol, p2Note);
        writeTriangleRegisters(triNote);
        writeRegister(0x400c, static_cast<uint8_t>(std::min<unsigned>(15u, noiseVol)));
        writeRegister(0x400e, static_cast<uint8_t>(noiseMode | noisePeriod));
        writeRegister(0x4015, static_cast<uint8_t>(enable));
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
             << "\"macro\":\"" << toString(patch.macro) << "\","
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

    void writePulseRegisters(uint16_t baseAddress, uint8_t duty, unsigned volume, int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto pulseTimer = static_cast<int>(std::max(0.0, std::round(clock / (16.0 * hz) - 1.0)));
        writeRegister(baseAddress, static_cast<uint8_t>((duty << 6u) | (std::min<unsigned>(15u, volume) & 0x0fu)));
        writeRegister(static_cast<uint16_t>(baseAddress + 2), static_cast<uint8_t>(pulseTimer & 0xff));
        writeRegister(static_cast<uint16_t>(baseAddress + 3), static_cast<uint8_t>((pulseTimer >> 8) & 0x07));
    }

    void writeTriangleRegisters(int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto triTimer = static_cast<int>(std::max(0.0, std::round(clock / (32.0 * hz) - 1.0)));
        writeRegister(0x4008, 0x80);
        writeRegister(0x400a, static_cast<uint8_t>(triTimer & 0xff));
        writeRegister(0x400b, static_cast<uint8_t>((triTimer >> 8) & 0x07));
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
    PatchConfig patch;
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

    void setPatch(const PatchConfig& newPatch) override
    {
        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        regs[address & 0x0f] = value;
    }

    void noteOn(int midiNote, float velocity) override
    {
        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        const auto noisePitch = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0f - patch.control3) * 30.0f)) + 1, 1, 31));
        auto noteA = midiNote;
        auto noteB = midiNote + std::max(1, spread / 2);
        auto noteC = midiNote + std::max(2, spread);
        auto volA = 15u;
        auto volB = 10u;
        auto volC = 8u;
        auto mixer = 0x38u; // Tone enabled, noise disabled for A/B/C.

        switch (patch.macro)
        {
            case MacroKind::coin:
                noteA = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                noteB = noteA + 7;
                noteC = noteA + 12;
                volB = 4u;
                volC = 0u;
                break;
            case MacroKind::bass:
                noteA = midiNote - 12;
                noteB = midiNote - 5;
                noteC = midiNote;
                volB = 4u;
                volC = 0u;
                break;
            case MacroKind::arp:
                noteB = midiNote + 7;
                noteC = midiNote + 12;
                volB = 12u;
                volC = 10u;
                break;
            case MacroKind::drum:
                mixer = 0x07u; // Tone disabled, noise enabled.
                volA = 15u;
                volB = 10u;
                volC = 6u;
                break;
            case MacroKind::hit:
                mixer = 0x00u; // Tone and noise combined.
                volA = 15u;
                volB = 7u;
                volC = 5u;
                break;
            case MacroKind::laser:
                noteA = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                noteB = midiNote - 12;
                noteC = midiNote + 3;
                mixer = patch.control3 > 0.3f ? 0x00u : 0x38u;
                break;
            case MacroKind::jump:
                noteA = midiNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                volB = 0u;
                volC = 0u;
                break;
            case MacroKind::powerUp:
                noteA = midiNote;
                noteB = midiNote + 5;
                noteC = midiNote + 12;
                volB = 13u;
                volC = 11u;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                mixer = patch.control4 > 0.66f ? 0x00u : (patch.control4 < 0.33f ? 0x07u : 0x38u);
                break;
        }

        writeTone(0, noteA);
        writeTone(1, noteB);
        writeTone(2, noteC);
        writeRegister(6, noisePitch);
        writeRegister(7, static_cast<uint8_t>(mixer));
        writeRegister(8, static_cast<uint8_t>(std::min<unsigned>(15u, volA)));
        writeRegister(9, static_cast<uint8_t>(std::min<unsigned>(15u, volB)));
        writeRegister(10, static_cast<uint8_t>(std::min<unsigned>(15u, volC)));
        writeRegister(11, 0x80);
        writeRegister(12, 0x02);
        writeRegister(13, patch.macro == MacroKind::powerUp ? 0x0a : 0x09);
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
             << "\"macro\":\"" << toString(patch.macro) << "\","
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

    void writeTone(int channel, int midiNote)
    {
        const auto period = static_cast<int>(std::max(1.0, std::round(clock / (16.0 * midiNoteToHz(std::clamp(midiNote, 0, 127))))));
        writeRegister(static_cast<uint16_t>(channel * 2), static_cast<uint8_t>(period & 0xff));
        writeRegister(static_cast<uint16_t>(channel * 2 + 1), static_cast<uint8_t>((period >> 8) & 0x0f));
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
    PatchConfig patch;
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

    void setPatch(const PatchConfig& newPatch) override
    {
        patch = newPatch;
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
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        auto note0 = midiNote;
        auto note1 = midiNote + std::max(1, spread / 2);
        auto note2 = midiNote + std::max(2, spread);
        auto noiseAttenuation = static_cast<uint8_t>(std::clamp(static_cast<int>(15 - std::round(patch.control4 * 13.0f)), 0, 15));
        auto noise = 0x03u;

        switch (patch.macro)
        {
            case MacroKind::coin:
                note0 = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                note1 = note0 + 12;
                note2 = note0 + 19;
                attenuation = { 0x01, 0x08, 0x0f, 0x0f };
                break;
            case MacroKind::bass:
                note0 = midiNote - 12;
                note1 = midiNote - 24;
                note2 = midiNote - 5;
                attenuation = { 0x01, 0x08, 0x0f, 0x0f };
                break;
            case MacroKind::arp:
                note1 = midiNote + 7;
                note2 = midiNote + 12;
                attenuation = { 0x02, 0x05, 0x07, 0x0f };
                break;
            case MacroKind::drum:
                note0 = midiNote - 24;
                note1 = midiNote - 12;
                note2 = midiNote;
                noise = patch.control3 > 0.5f ? 0x04u : 0x00u;
                noiseAttenuation = 0x02u;
                attenuation = { 0x0f, 0x0f, 0x0f, noiseAttenuation };
                break;
            case MacroKind::hit:
                noise = 0x04u | static_cast<unsigned>(std::round(patch.control3 * 2.0f));
                attenuation = { 0x04, 0x0f, 0x0f, 0x03 };
                break;
            case MacroKind::laser:
                note0 = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                note1 = midiNote - 12;
                noise = 0x04u | static_cast<unsigned>(std::round(patch.control3 * 2.0f));
                attenuation = { 0x01, 0x09, 0x0f, noiseAttenuation };
                break;
            case MacroKind::jump:
                note0 = midiNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                attenuation = { 0x01, 0x0f, 0x0f, 0x0f };
                break;
            case MacroKind::powerUp:
                note1 = midiNote + 5;
                note2 = midiNote + 12;
                attenuation = { 0x01, 0x04, 0x07, 0x0f };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                attenuation = { 0x02, 0x07, 0x09, noiseAttenuation };
                noise = patch.control3 > 0.66f ? 0x04u : 0x03u;
                break;
        }

        tonePeriod[0] = notePeriod(note0);
        tonePeriod[1] = notePeriod(note1);
        tonePeriod[2] = notePeriod(note2);
        noiseControl = static_cast<uint8_t>(noise & 0x07u);
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
             << "\"macro\":\"" << toString(patch.macro) << "\","
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

    uint16_t notePeriod(int midiNote) const
    {
        return static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * midiNoteToHz(std::clamp(midiNote, 0, 127)))), 1.0, 1023.0));
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
    PatchConfig patch;
};

} // namespace

std::unique_ptr<ChipCore> createChipCore(ChipMode mode, AccuracyMode accuracy)
{
    switch (mode)
    {
        case ChipMode::nes: return std::make_unique<NesApuCore>(accuracy);
        case ChipMode::dmg: return std::make_unique<DmgApuCore>(accuracy);
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

std::optional<MacroKind> parseMacroKind(std::string_view text)
{
    const auto key = lower(text);
    if (key == "manual") return MacroKind::manual;
    if (key == "coin") return MacroKind::coin;
    if (key == "bass") return MacroKind::bass;
    if (key == "lead") return MacroKind::lead;
    if (key == "arp") return MacroKind::arp;
    if (key == "drum") return MacroKind::drum;
    if (key == "hit") return MacroKind::hit;
    if (key == "laser") return MacroKind::laser;
    if (key == "jump") return MacroKind::jump;
    if (key == "powerup") return MacroKind::powerUp;
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

std::string toString(MacroKind macro)
{
    switch (macro)
    {
        case MacroKind::manual: return "Manual";
        case MacroKind::coin: return "Coin";
        case MacroKind::bass: return "Bass";
        case MacroKind::lead: return "Lead";
        case MacroKind::arp: return "Arp";
        case MacroKind::drum: return "Drum";
        case MacroKind::hit: return "Hit";
        case MacroKind::laser: return "Laser";
        case MacroKind::jump: return "Jump";
        case MacroKind::powerUp: return "Power-Up";
    }
    return "Unknown";
}

double midiNoteToHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

} // namespace chipper
