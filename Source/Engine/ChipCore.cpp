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
    std::vector<RegisterWrite> exportRegisterState() const override { return {}; }

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
        envelopeLevel.fill(0);
        envelopeDivider.fill(0);
        lengthCounter.fill(0);
        channelMask = 0x0f;
        lengthClockPhase = 0.0;
        envelopeClockPhase = 0.0;
        sweepClockPhase = 0.0;
        sweepShadowPeriod = 0;
        sweepTimer = 0;
        sweepEnabled = false;
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
            case 0x01: lengthCounter[0] = lengthFromRegister(0, value); break; // NR11
            case 0x02: if (! dacEnabled(0)) enabled[0] = false; break; // NR12
            case 0x04: if ((value & 0x80u) != 0) triggerChannel(0); break; // NR14
            case 0x06: lengthCounter[1] = lengthFromRegister(1, value); break; // NR21
            case 0x07: if (! dacEnabled(1)) enabled[1] = false; break; // NR22
            case 0x09: if ((value & 0x80u) != 0) triggerChannel(1); break; // NR24
            case 0x0a: if (! dacEnabled(2)) enabled[2] = false; break; // NR30
            case 0x0b: lengthCounter[2] = lengthFromRegister(2, value); break; // NR31
            case 0x0e: if ((value & 0x80u) != 0) triggerChannel(2); break; // NR34
            case 0x10: lengthCounter[3] = lengthFromRegister(3, value); break; // NR41
            case 0x11: if (! dacEnabled(3)) enabled[3] = false; break; // NR42
            case 0x13: if ((value & 0x80u) != 0) triggerChannel(3); break; // NR44
            case 0x16:
                if ((value & 0x80u) == 0)
                {
                    powerOffApu();
                }
                else if ((value & 0x0fu) != 0)
                {
                    channelMask = static_cast<uint8_t>(value & 0x0fu);
                    for (size_t channel = 0; channel < enabled.size(); ++channel)
                    {
                        if ((channelMask & (1u << channel)) == 0)
                            enabled[channel] = false;
                    }
                }
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
        writeRegister(0xff24, 0x77);
        writeRegister(0xff25, 0xff);
        writeRegister(0xff10, static_cast<uint8_t>(0x18u | std::clamp(static_cast<int>(std::round(patch.control2 * 7.0f)), 0, 7)));
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
        tickFrameSequencer();

        const auto p1 = enabled[0] ? renderPulse(0) : 0.0;
        const auto p2 = enabled[1] ? renderPulse(1) : 0.0;
        const auto wave = enabled[2] ? renderWave() : 0.0;
        const auto noise = enabled[3] ? renderNoise() : 0.0;
        const std::array<double, 4> sources { p1, p2, wave, noise };

        double left = 0.0;
        double right = 0.0;
        for (size_t channel = 0; channel < sources.size(); ++channel)
        {
            if (routedToLeft(channel))
                left += sources[channel];
            if (routedToRight(channel))
                right += sources[channel];
        }

        const auto leftOut = static_cast<float>((left / 4.0) * outputVolume(true) * noteVelocity * 0.85);
        const auto rightOut = static_cast<float>((right / 4.0) * outputVolume(false) * noteVelocity * 0.85);
        return { leftOut, rightOut };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(regs.size());
        for (size_t i = 0; i < regs.size(); ++i)
            writes.push_back({ 0, static_cast<uint16_t>(0xff10 + i), regs[i] });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::dmg; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "Game Boy / DMG APU"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Pulse, wave RAM, noise, trigger bits, core frequency formulas, DAC gating, simple envelopes, length counters, basic CH1 sweep, NR50/NR51 stereo routing, and NR43 noise clock behavior are modeled; exact DIV-APU quirks, sweep obscure behavior, mixer analog details, and hardware validation are not complete.";
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
             << "\"sweepShadow\":" << sweepShadowPeriod << ","
             << "\"sweepTimer\":" << static_cast<int>(sweepTimer) << ","
             << "\"sweepEnabled\":" << (sweepEnabled ? 1 : 0) << ","
             << "\"nr50\":" << static_cast<int>(regs[0x14]) << ","
             << "\"nr51\":" << static_cast<int>(regs[0x15]) << ","
             << "\"leftVolume\":" << outputVolume(true) << ","
             << "\"rightVolume\":" << outputVolume(false) << ","
             << "\"noiseClockHz\":" << noiseClockHz() << ","
             << "\"noiseShift\":" << static_cast<int>(noiseClockShift()) << ","
             << "\"noiseDivisorCode\":" << static_cast<int>(noiseDivisorCode()) << ","
             << "\"noiseWidth7\":" << (noiseWidth7() ? 1 : 0) << ","
             << "\"waveRam0\":" << static_cast<int>(regs[0x20]) << ","
             << "\"envelope0\":" << static_cast<int>(envelopeLevel[0]) << ","
             << "\"envelope1\":" << static_cast<int>(envelopeLevel[1]) << ","
             << "\"envelopeNoise\":" << static_cast<int>(envelopeLevel[3]) << ","
             << "\"length0\":" << lengthCounter[0] << ","
             << "\"length1\":" << lengthCounter[1] << ","
             << "\"lengthWave\":" << lengthCounter[2] << ","
             << "\"lengthNoise\":" << lengthCounter[3] << ","
             << "\"enabled0\":" << (enabled[0] ? 1 : 0) << ","
             << "\"enabled1\":" << (enabled[1] ? 1 : 0) << ","
             << "\"enabledWave\":" << (enabled[2] ? 1 : 0) << ","
             << "\"enabledNoise\":" << (enabled[3] ? 1 : 0) << ","
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

    double outputVolume(bool left) const
    {
        const auto volume = left ? ((regs[0x14] >> 4u) & 0x07u) : (regs[0x14] & 0x07u);
        return static_cast<double>(volume + 1u) / 8.0;
    }

    bool routedToLeft(size_t channel) const
    {
        return channel < 4 && (regs[0x15] & (1u << (channel + 4u))) != 0;
    }

    bool routedToRight(size_t channel) const
    {
        return channel < 4 && (regs[0x15] & (1u << channel)) != 0;
    }

    uint8_t noiseClockShift() const
    {
        return static_cast<uint8_t>((regs[0x12] >> 4u) & 0x0fu);
    }

    uint8_t noiseDivisorCode() const
    {
        return static_cast<uint8_t>(regs[0x12] & 0x07u);
    }

    bool noiseWidth7() const
    {
        return (regs[0x12] & 0x08u) != 0;
    }

    double noiseDivisor() const
    {
        const auto code = noiseDivisorCode();
        return code == 0 ? 0.5 : static_cast<double>(code);
    }

    double noiseClockHz() const
    {
        const auto shift = noiseClockShift();
        if (shift >= 14)
            return 0.0;

        return 262144.0 / (noiseDivisor() * static_cast<double>(1u << shift));
    }

    static uint16_t lengthMax(size_t channel)
    {
        return channel == 2 ? 256u : 64u;
    }

    static uint16_t lengthFromRegister(size_t channel, uint8_t value)
    {
        const auto mask = channel == 2 ? 0xffu : 0x3fu;
        return static_cast<uint16_t>(lengthMax(channel) - static_cast<uint16_t>(value & mask));
    }

    size_t envelopeRegisterForChannel(size_t channel) const
    {
        if (channel == 0)
            return 0x02;
        if (channel == 1)
            return 0x07;
        return 0x11;
    }

    size_t controlRegisterForChannel(size_t channel) const
    {
        if (channel == 0)
            return 0x04;
        if (channel == 1)
            return 0x09;
        if (channel == 2)
            return 0x0e;
        return 0x13;
    }

    bool lengthEnabled(size_t channel) const
    {
        return (regs[controlRegisterForChannel(channel)] & 0x40u) != 0;
    }

    bool dacEnabled(size_t channel) const
    {
        if (channel == 2)
            return (regs[0x0a] & 0x80u) != 0;

        return (regs[envelopeRegisterForChannel(channel)] & 0xf8u) != 0;
    }

    uint8_t initialEnvelopeVolume(size_t channel) const
    {
        return static_cast<uint8_t>((regs[envelopeRegisterForChannel(channel)] >> 4u) & 0x0fu);
    }

    uint8_t envelopePeriod(size_t channel) const
    {
        return static_cast<uint8_t>(regs[envelopeRegisterForChannel(channel)] & 0x07u);
    }

    bool envelopeIncreasing(size_t channel) const
    {
        return (regs[envelopeRegisterForChannel(channel)] & 0x08u) != 0;
    }

    void powerOffApu()
    {
        std::array<uint8_t, 16> waveRam {};
        std::copy(regs.begin() + 0x20, regs.begin() + 0x30, waveRam.begin());

        regs.fill(0);
        std::copy(waveRam.begin(), waveRam.end(), regs.begin() + 0x20);
        phase.fill(0.0);
        enabled = { false, false, false, false };
        envelopeLevel.fill(0);
        envelopeDivider.fill(0);
        lengthCounter.fill(0);
        channelMask = 0x0f;
        lengthClockPhase = 0.0;
        envelopeClockPhase = 0.0;
        sweepClockPhase = 0.0;
        sweepShadowPeriod = 0;
        sweepTimer = 0;
        sweepEnabled = false;
        lfsr = 0x7fff;
    }

    void triggerChannel(size_t channel)
    {
        if (channel >= enabled.size())
            return;

        if (lengthCounter[channel] == 0)
            lengthCounter[channel] = lengthMax(channel);

        if (channel == 0 || channel == 1 || channel == 3)
        {
            envelopeLevel[channel] = initialEnvelopeVolume(channel);
            envelopeDivider[channel] = envelopePeriod(channel);
        }

        enabled[channel] = dacEnabled(channel) && ((channelMask & (1u << channel)) != 0);

        if (channel == 3)
            lfsr = 0x7fff;
        else if (channel == 0)
            triggerSweep();
    }

    void tickFrameSequencer()
    {
        lengthClockPhase += 256.0 / sampleRate;
        while (lengthClockPhase >= 1.0)
        {
            lengthClockPhase -= 1.0;
            tickLengthCounters();
        }

        envelopeClockPhase += 64.0 / sampleRate;
        while (envelopeClockPhase >= 1.0)
        {
            envelopeClockPhase -= 1.0;
            tickEnvelopes();
        }

        sweepClockPhase += 128.0 / sampleRate;
        while (sweepClockPhase >= 1.0)
        {
            sweepClockPhase -= 1.0;
            tickSweep();
        }
    }

    void tickLengthCounters()
    {
        for (size_t channel = 0; channel < lengthCounter.size(); ++channel)
        {
            if (! lengthEnabled(channel) || lengthCounter[channel] == 0)
                continue;

            --lengthCounter[channel];
            if (lengthCounter[channel] == 0)
                enabled[channel] = false;
        }
    }

    void tickEnvelopes()
    {
        for (const auto channel : { size_t(0), size_t(1), size_t(3) })
        {
            const auto period = envelopePeriod(channel);
            if (period == 0)
                continue;

            if (envelopeDivider[channel] > 1)
            {
                --envelopeDivider[channel];
                continue;
            }

            envelopeDivider[channel] = period;
            if (envelopeIncreasing(channel))
            {
                if (envelopeLevel[channel] < 15)
                    ++envelopeLevel[channel];
            }
            else if (envelopeLevel[channel] > 0)
            {
                --envelopeLevel[channel];
            }
        }
    }

    uint8_t sweepPace() const
    {
        return static_cast<uint8_t>((regs[0x00] >> 4u) & 0x07u);
    }

    uint8_t sweepReloadValue() const
    {
        const auto pace = sweepPace();
        return pace == 0 ? 8u : pace;
    }

    bool sweepSubtract() const
    {
        return (regs[0x00] & 0x08u) != 0;
    }

    uint8_t sweepShift() const
    {
        return static_cast<uint8_t>(regs[0x00] & 0x07u);
    }

    int calculateSweepTarget() const
    {
        const auto shift = sweepShift();
        const auto delta = shift == 0 ? 0 : (sweepShadowPeriod >> shift);
        return sweepSubtract()
            ? static_cast<int>(sweepShadowPeriod) - static_cast<int>(delta)
            : static_cast<int>(sweepShadowPeriod) + static_cast<int>(delta);
    }

    void writeSweepPeriod(uint16_t period)
    {
        period = static_cast<uint16_t>(period & 0x07ffu);
        regs[0x03] = static_cast<uint8_t>(period & 0xffu);
        regs[0x04] = static_cast<uint8_t>((regs[0x04] & 0xf8u) | ((period >> 8u) & 0x07u));
    }

    bool sweepOverflowCheck()
    {
        if (calculateSweepTarget() > 2047)
        {
            enabled[0] = false;
            return true;
        }

        return false;
    }

    void triggerSweep()
    {
        sweepShadowPeriod = pulseFrequencyRegister(0);
        sweepTimer = sweepReloadValue();
        sweepEnabled = sweepPace() != 0 || sweepShift() != 0;

        if (sweepShift() != 0)
            sweepOverflowCheck();
    }

    void tickSweep()
    {
        if (sweepTimer > 0)
            --sweepTimer;

        if (sweepTimer != 0)
            return;

        sweepTimer = sweepReloadValue();
        if (! sweepEnabled || sweepPace() == 0)
            return;

        const auto target = calculateSweepTarget();
        if (target > 2047)
        {
            enabled[0] = false;
            return;
        }

        if (sweepShift() != 0)
        {
            sweepShadowPeriod = static_cast<uint16_t>(target);
            writeSweepPeriod(sweepShadowPeriod);
            sweepOverflowCheck();
        }
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
        if (index == 0x02)
            return static_cast<double>(envelopeLevel[0]) / 15.0;
        if (index == 0x07)
            return static_cast<double>(envelopeLevel[1]) / 15.0;
        return static_cast<double>(envelopeLevel[3]) / 15.0;
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
        const auto hz = noiseClockHz();
        phase[3] += hz / sampleRate;

        while (phase[3] >= 1.0)
        {
            phase[3] -= 1.0;
            const auto feedback = (lfsr & 1u) ^ ((lfsr >> 1u) & 1u);
            lfsr = static_cast<uint16_t>((lfsr >> 1u) | (feedback << 14u));
            if (noiseWidth7())
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
    std::array<uint8_t, 4> envelopeLevel {};
    std::array<uint8_t, 4> envelopeDivider {};
    std::array<uint16_t, 4> lengthCounter {};
    uint8_t channelMask = 0x0f;
    double lengthClockPhase = 0.0;
    double envelopeClockPhase = 0.0;
    double sweepClockPhase = 0.0;
    uint16_t sweepShadowPeriod = 0;
    uint8_t sweepTimer = 0;
    bool sweepEnabled = false;
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
        envelopeVolume.fill(0);
        envelopeDivider.fill(0);
        envelopeStart.fill(false);
        lengthCounter.fill(0);
        sweepDivider.fill(0);
        sweepReload.fill(false);
        linearCounter = 0;
        linearReloadFlag = false;
        quarterFramePhase = 0.0;
        halfFramePhase = 0.0;
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

        if (address == 0x4003)
            triggerChannel(0, value);
        else if (address == 0x4007)
            triggerChannel(1, value);
        else if (address == 0x400b)
        {
            triggerChannel(2, value);
            linearReloadFlag = true;
        }
        else if (address == 0x400f)
            triggerChannel(3, value);

        if (address == 0x4001)
            sweepReload[0] = true;
        else if (address == 0x4005)
            sweepReload[1] = true;

        if (address == 0x4015)
        {
            enabled[0] = (value & 0x01) != 0;
            enabled[1] = (value & 0x02) != 0;
            enabled[2] = (value & 0x04) != 0;
            enabled[3] = (value & 0x08) != 0;
            for (size_t i = 0; i < enabled.size(); ++i)
            {
                if (! enabled[i])
                    lengthCounter[i] = 0;
            }
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

        const auto sweepShift = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(1.0f + patch.control2 * 2.0f)), 1, 3));
        const auto sweepPeriod = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control2 * 3.0f)), 0, 3) << 4u);
        auto pulse1Sweep = uint8_t { 0x08u };
        auto pulse2Sweep = uint8_t { 0x08u };
        if (patch.macro == MacroKind::coin || patch.macro == MacroKind::jump || patch.macro == MacroKind::powerUp)
            pulse1Sweep = static_cast<uint8_t>(0x80u | sweepPeriod | 0x08u | sweepShift);
        else if (patch.macro == MacroKind::laser || patch.macro == MacroKind::hit)
            pulse1Sweep = static_cast<uint8_t>(0x80u | sweepPeriod | sweepShift);

        writeRegister(0x4001, pulse1Sweep);
        writeRegister(0x4005, pulse2Sweep);
        writeTriangleRegisters(triNote);
        writeRegister(0x400c, static_cast<uint8_t>(std::min<unsigned>(15u, noiseVol)));
        writeRegister(0x400e, static_cast<uint8_t>(noiseMode | noisePeriod));
        writeRegister(0x400f, 0x18);
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
        tickFrameUnits();

        const auto p1 = channelActive(0) ? renderPulse(0) : 0.0;
        const auto p2 = channelActive(1) ? renderPulse(1) : 0.0;
        const auto tri = triangleActive() ? renderTriangle() : 0.0;
        const auto noi = channelActive(3) ? renderNoise() : 0.0;

        const auto pulseSum = p1 + p2;
        const auto tndSum = tri / 8227.0 + noi / 12241.0;
        const auto pulseOut = pulseSum <= 0.0 ? 0.0 : 95.88 / ((8128.0 / pulseSum) + 100.0);
        const auto tndOut = tndSum <= 0.0 ? 0.0 : 159.79 / ((1.0 / tndSum) + 100.0);
        const auto mixed = static_cast<float>((pulseOut + tndOut) * 2.0 - 0.35);
        const auto scaled = mixed * noteVelocity;
        return { scaled, scaled };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(regs.size());
        for (size_t i = 0; i < regs.size(); ++i)
            writes.push_back({ 0, static_cast<uint16_t>(0x4000 + i), regs[i] });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::nes; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "NES / RP2A03"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Pulse, triangle, noise, timers, duty, enable bits, simple envelopes, length counters, triangle linear counter, basic pulse sweep updates/muting, and nonlinear mixer are approximated; DMC, exact frame sequencer timing, advanced sweep edge cases, and hardware validation are not complete.";
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
             << "\"triangleActive\":" << (triangleActive() ? 1 : 0) << ","
             << "\"linearCounter\":" << static_cast<int>(linearCounter) << ","
             << "\"linearReloadValue\":" << static_cast<int>(linearReloadValue()) << ","
             << "\"linearReloadFlag\":" << (linearReloadFlag ? 1 : 0) << ","
             << "\"linearControlFlag\":" << (linearControlFlag() ? 1 : 0) << ","
             << "\"sweepTarget0\":" << sweepTargetPeriod(0) << ","
             << "\"sweepTarget1\":" << sweepTargetPeriod(1) << ","
             << "\"sweepMuted0\":" << (pulseSweepMuted(0) ? 1 : 0) << ","
             << "\"sweepMuted1\":" << (pulseSweepMuted(1) ? 1 : 0) << ","
             << "\"sweepEnabled0\":" << (sweepUnitEnabled(0) ? 1 : 0) << ","
             << "\"sweepEnabled1\":" << (sweepUnitEnabled(1) ? 1 : 0) << ","
             << "\"sweepDivider0\":" << static_cast<int>(sweepDivider[0]) << ","
             << "\"sweepDivider1\":" << static_cast<int>(sweepDivider[1]) << ","
             << "\"envelope0\":" << static_cast<int>(envelopeVolume[0]) << ","
             << "\"envelope1\":" << static_cast<int>(envelopeVolume[1]) << ","
             << "\"envelopeNoise\":" << static_cast<int>(envelopeVolume[3]) << ","
             << "\"length0\":" << static_cast<int>(lengthCounter[0]) << ","
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

    static uint8_t lengthFromIndex(uint8_t value)
    {
        static constexpr std::array<uint8_t, 32> table {
            10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
            12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
        };
        return table[(value >> 3u) & 0x1fu];
    }

    void triggerChannel(size_t channel, uint8_t value)
    {
        if (channel < lengthCounter.size())
        {
            lengthCounter[channel] = lengthFromIndex(value);
            envelopeStart[channel] = true;
        }
    }

    bool channelActive(size_t channel) const
    {
        return channel < enabled.size() && enabled[channel] && lengthCounter[channel] > 0;
    }

    bool triangleActive() const
    {
        return channelActive(2) && linearCounter > 0;
    }

    int envelopeRegisterForChannel(size_t channel) const
    {
        if (channel == 0)
            return 0x00;
        if (channel == 1)
            return 0x04;
        return 0x0c;
    }

    void tickFrameUnits()
    {
        quarterFramePhase += 240.0 / sampleRate;
        while (quarterFramePhase >= 1.0)
        {
            quarterFramePhase -= 1.0;
            tickEnvelopes();
            tickLinearCounter();
        }

        halfFramePhase += 120.0 / sampleRate;
        while (halfFramePhase >= 1.0)
        {
            halfFramePhase -= 1.0;
            tickSweeps();
            tickLengthCounters();
        }
    }

    void tickEnvelopes()
    {
        for (const auto channel : { size_t(0), size_t(1), size_t(3) })
        {
            const auto reg = regs[static_cast<size_t>(envelopeRegisterForChannel(channel))];
            const auto period = static_cast<uint8_t>(reg & 0x0fu);
            const auto loop = (reg & 0x20u) != 0;

            if (envelopeStart[channel])
            {
                envelopeStart[channel] = false;
                envelopeVolume[channel] = 15;
                envelopeDivider[channel] = period;
                continue;
            }

            if (envelopeDivider[channel] > 0)
            {
                --envelopeDivider[channel];
                continue;
            }

            envelopeDivider[channel] = period;
            if (envelopeVolume[channel] > 0)
                --envelopeVolume[channel];
            else if (loop)
                envelopeVolume[channel] = 15;
        }
    }

    uint8_t linearReloadValue() const
    {
        return static_cast<uint8_t>(regs[0x08] & 0x7fu);
    }

    bool linearControlFlag() const
    {
        return (regs[0x08] & 0x80u) != 0;
    }

    void tickLinearCounter()
    {
        if (linearReloadFlag)
        {
            linearCounter = linearReloadValue();
        }
        else if (linearCounter > 0)
        {
            --linearCounter;
        }

        if (! linearControlFlag())
            linearReloadFlag = false;
    }

    void tickLengthCounters()
    {
        for (size_t channel = 0; channel < lengthCounter.size(); ++channel)
        {
            const auto haltReg = channel == 0 ? regs[0x00] : (channel == 1 ? regs[0x04] : (channel == 2 ? regs[0x08] : regs[0x0c]));
            const auto halted = (haltReg & 0x20u) != 0;
            if (! halted && lengthCounter[channel] > 0)
                --lengthCounter[channel];
        }
    }

    uint8_t sweepRegisterForChannel(size_t channel) const
    {
        return regs[channel == 0 ? 0x01 : 0x05];
    }

    uint8_t sweepPeriod(size_t channel) const
    {
        return static_cast<uint8_t>((sweepRegisterForChannel(channel) >> 4u) & 0x07u);
    }

    uint8_t sweepShift(size_t channel) const
    {
        return static_cast<uint8_t>(sweepRegisterForChannel(channel) & 0x07u);
    }

    bool sweepNegate(size_t channel) const
    {
        return (sweepRegisterForChannel(channel) & 0x08u) != 0;
    }

    bool sweepUnitEnabled(size_t channel) const
    {
        return (sweepRegisterForChannel(channel) & 0x80u) != 0 && sweepShift(channel) != 0;
    }

    int sweepTargetPeriod(size_t channel) const
    {
        if (channel >= 2)
            return 0;

        const auto current = static_cast<int>(timer[channel]);
        const auto change = current >> sweepShift(channel);
        const auto target = sweepNegate(channel)
            ? current - change - (channel == 0 ? 1 : 0)
            : current + change;

        return std::max(0, target);
    }

    bool pulseSweepMuted(size_t channel) const
    {
        return channel < 2 && (timer[channel] < 8 || sweepTargetPeriod(channel) > 0x7ff);
    }

    void setPulseTimer(size_t channel, uint16_t period)
    {
        if (channel >= 2)
            return;

        period = static_cast<uint16_t>(period & 0x07ffu);
        const auto loIndex = channel == 0 ? size_t(0x02) : size_t(0x06);
        const auto hiIndex = channel == 0 ? size_t(0x03) : size_t(0x07);
        regs[loIndex] = static_cast<uint8_t>(period & 0xffu);
        regs[hiIndex] = static_cast<uint8_t>((regs[hiIndex] & 0xf8u) | ((period >> 8u) & 0x07u));
        timer[channel] = period;
    }

    void tickSweeps()
    {
        for (size_t channel = 0; channel < 2; ++channel)
        {
            const auto dividerWasZero = sweepDivider[channel] == 0;
            if (dividerWasZero && sweepUnitEnabled(channel) && ! pulseSweepMuted(channel))
                setPulseTimer(channel, static_cast<uint16_t>(sweepTargetPeriod(channel)));

            if (dividerWasZero || sweepReload[channel])
            {
                sweepDivider[channel] = sweepPeriod(channel);
                sweepReload[channel] = false;
            }
            else
            {
                --sweepDivider[channel];
            }
        }
    }

    void writePulseRegisters(uint16_t baseAddress, uint8_t duty, unsigned volume, int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto pulseTimer = static_cast<int>(std::max(0.0, std::round(clock / (16.0 * hz) - 1.0)));
        const auto constantVolume = patch.macro == MacroKind::manual
            || patch.macro == MacroKind::lead
            || patch.macro == MacroKind::arp
            || patch.macro == MacroKind::bass
            || patch.macro == MacroKind::powerUp;
        const auto flags = constantVolume ? 0x10u : 0x00u;
        writeRegister(baseAddress, static_cast<uint8_t>((duty << 6u) | flags | (std::min<unsigned>(15u, volume) & 0x0fu)));
        writeRegister(static_cast<uint16_t>(baseAddress + 2), static_cast<uint8_t>(pulseTimer & 0xff));
        writeRegister(static_cast<uint16_t>(baseAddress + 3), static_cast<uint8_t>((pulseTimer >> 8) & 0x07));
    }

    void writeTriangleRegisters(int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto triTimer = static_cast<int>(std::max(0.0, std::round(clock / (32.0 * hz) - 1.0)));
        writeRegister(0x4008, 0xff);
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
        const auto reg = regs[static_cast<size_t>(regIndex)];
        if ((reg & 0x10u) != 0)
            return static_cast<double>(reg & 0x0f) / 15.0;

        const auto channel = regIndex == 0x00 ? size_t(0) : (regIndex == 0x04 ? size_t(1) : size_t(3));
        return static_cast<double>(envelopeVolume[channel]) / 15.0;
    }

    double renderPulse(int index)
    {
        if (pulseSweepMuted(static_cast<size_t>(index)))
            return 0.0;

        const auto period = timer[index] + 1;
        const auto hz = clock / (16.0 * static_cast<double>(period));
        phase[index] = wrapPhase(phase[index] + hz / sampleRate);
        const auto value = phase[index] < dutyForPulse(index) ? 1.0 : 0.0;
        return value * 15.0 * constantVolume(index == 0 ? 0x00 : 0x04);
    }

    double renderTriangle()
    {
        static constexpr std::array<double, 32> sequence {
            15, 14, 13, 12, 11, 10, 9, 8,
            7, 6, 5, 4, 3, 2, 1, 0,
            0, 1, 2, 3, 4, 5, 6, 7,
            8, 9, 10, 11, 12, 13, 14, 15
        };

        const auto period = timer[2] + 1;
        const auto hz = clock / (32.0 * static_cast<double>(period));
        phase[2] = wrapPhase(phase[2] + hz / sampleRate);
        const auto step = std::clamp(static_cast<size_t>(phase[2] * sequence.size()), size_t(0), sequence.size() - 1);
        return sequence[step];
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
    std::array<uint8_t, 4> envelopeVolume {};
    std::array<uint8_t, 4> envelopeDivider {};
    std::array<bool, 4> envelopeStart {};
    std::array<uint8_t, 4> lengthCounter {};
    std::array<uint8_t, 2> sweepDivider {};
    std::array<bool, 2> sweepReload {};
    uint8_t linearCounter = 0;
    bool linearReloadFlag = false;
    double quarterFramePhase = 0.0;
    double halfFramePhase = 0.0;
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

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(regs.size());
        for (size_t i = 0; i < regs.size(); ++i)
            writes.push_back({ 0, static_cast<uint16_t>(i), regs[i] });
        return writes;
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

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(10);

        const auto addTone = [&writes](uint8_t channel, uint16_t period)
        {
            writes.push_back({ 0, 0, static_cast<uint8_t>(0x80u | (channel << 5u) | (period & 0x0fu)) });
            writes.push_back({ 0, 0, static_cast<uint8_t>((period >> 4u) & 0x3fu) });
        };

        addTone(0, tonePeriod[0]);
        addTone(1, tonePeriod[1]);
        addTone(2, tonePeriod[2]);

        for (uint8_t channel = 0; channel < 4; ++channel)
            writes.push_back({ 0, 0, static_cast<uint8_t>(0x90u | (channel << 5u) | (attenuation[channel] & 0x0fu)) });

        writes.push_back({ 0, 0, static_cast<uint8_t>(0xe0u | (noiseControl & 0x07u)) });
        return writes;
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
