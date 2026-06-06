#include "Engine/ChipCore.h"
#include "Engine/ChipDescriptors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
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

enum class NesPulseDuty : uint8_t
{
    duty12_5 = 0,
    duty25 = 1,
    duty50 = 2,
    duty75 = 3
};

enum class DmgPulseDuty : uint8_t
{
    duty12_5 = 0,
    duty25 = 1,
    duty50 = 2,
    duty75 = 3
};

NesPulseDuty nesPulseDutyFromControl(float control)
{
    return static_cast<NesPulseDuty>(std::clamp(static_cast<int>(std::round(control * 3.0f)), 0, 3));
}

NesPulseDuty offsetNesPulseDuty(NesPulseDuty duty, int offset)
{
    return static_cast<NesPulseDuty>(std::clamp(static_cast<int>(duty) + offset, 0, 3));
}

uint8_t nesPulseDutyBits(NesPulseDuty duty)
{
    return static_cast<uint8_t>(duty);
}

DmgPulseDuty dmgPulseDutyFromControl(float control)
{
    return static_cast<DmgPulseDuty>(std::clamp(static_cast<int>(std::round(control * 3.0f)), 0, 3));
}

DmgPulseDuty offsetDmgPulseDuty(DmgPulseDuty duty, int offset)
{
    return static_cast<DmgPulseDuty>(std::clamp(static_cast<int>(duty) + offset, 0, 3));
}

uint8_t dmgPulseDutyBits(DmgPulseDuty duty)
{
    return static_cast<uint8_t>(duty);
}

bool sourceEnabled(const PatchConfig& patch, size_t index)
{
    return index < patch.sourceEnabled.size() && patch.sourceEnabled[index];
}

uint8_t sourceEnableMask(const PatchConfig& patch)
{
    uint8_t mask = 0;
    for (size_t i = 0; i < patch.sourceEnabled.size(); ++i)
    {
        if (patch.sourceEnabled[i])
            mask |= static_cast<uint8_t>(1u << i);
    }
    return mask;
}

bool envelopeDecayActive(const PatchConfig& patch)
{
    return patch.envelopeDecay > 0.01f;
}

uint8_t decayPeriodFromControl(float control, uint8_t maxPeriod)
{
    const auto period = std::round(static_cast<float>(maxPeriod) - (std::clamp(control, 0.0f, 1.0f) * static_cast<float>(maxPeriod - 1u)));
    return static_cast<uint8_t>(std::clamp(static_cast<int>(period), 1, static_cast<int>(maxPeriod)));
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
             << "\"playMode\":\"" << jsonEscape(toString(patch.playMode)) << "\","
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
        lengthClockPhase = 0.0;
        envelopeClockPhase = 0.0;
        sweepClockPhase = 0.0;
        sweepShadowPeriod = 0;
        sweepTimer = 0;
        sweepEnabled = false;
        lfsr = 0x7fffu;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        loadDefaultWaveRam();
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto index = registerIndex(address);
        if (index == 0x16)
        {
            if ((value & 0x80u) == 0)
            {
                powerOffApu();
            }
            else
            {
                regs[0x16] = 0x80;
            }
            return;
        }

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
            default:
                break;
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));

        const auto duty = dmgPulseDutyFromControl(patch.control1);
        const auto envelope = dmgInitialEnvelopeLevelForControl(patch.control4);
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

        enable = static_cast<unsigned>(0x80u | ((enable & 0x0fu) & sourceEnableMask(patch)));

        writeRegister(0xff26, 0x80);
        writeRegister(0xff24, 0x77);
        writeRegister(0xff25, 0xff);
        writeRegister(0xff10, dmgSweepRegisterForControl(patch.control2));
        if ((enable & 0x01u) != 0)
            writePulseRegisters(0, duty, ch1Vol, ch1Note);
        else
            silenceChannel(0);

        if ((enable & 0x02u) != 0)
            writePulseRegisters(1, offsetDmgPulseDuty(duty, 1), ch2Vol, ch2Note);
        else
            silenceChannel(1);

        if ((enable & 0x04u) != 0)
        {
            writePatchWaveRam();
            writeWaveRegisters(waveNote, patch.macro == MacroKind::bass ? 0x20u : 0x40u);
        }
        else
        {
            silenceChannel(2);
        }

        if ((enable & 0x08u) != 0)
            writeNoiseRegisters(noiseVol, dmgNoiseRegisterForPatch(patch));
        else
            silenceChannel(3);
    }

    void noteOff(int midiNote) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOffChipPoly(midiNote);
            return;
        }

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
        return "Pulse, wave RAM, noise, trigger bits, core frequency formulas, DAC gating, simple envelopes, length counters, basic CH1 sweep, NR50/NR51 stereo routing, NR52 power/read-only status-bit write behavior, NR43 noise clock behavior, and pitched-channel allocation for Chip Poly play mode are modeled; exact DIV-APU quirks, sweep obscure behavior, mixer analog details, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Game Boy / DMG APU\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"pulse1FreqReg\":" << pulseFrequencyRegister(0) << ","
             << "\"pulse2FreqReg\":" << pulseFrequencyRegister(1) << ","
             << "\"waveFreqReg\":" << waveFrequencyRegister() << ","
             << "\"pulseDuty1\":" << static_cast<int>(pulseDutyIndex(0)) << ","
             << "\"pulseDuty2\":" << static_cast<int>(pulseDutyIndex(1)) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sweepRegister\":" << static_cast<int>(regs[0x00]) << ","
             << "\"sweepShadow\":" << sweepShadowPeriod << ","
             << "\"sweepTimer\":" << static_cast<int>(sweepTimer) << ","
             << "\"sweepEnabled\":" << (sweepEnabled ? 1 : 0) << ","
             << "\"sweepPace\":" << static_cast<int>(sweepPace()) << ","
             << "\"sweepShift\":" << static_cast<int>(sweepShift()) << ","
             << "\"sweepSubtract\":" << (sweepSubtract() ? 1 : 0) << ","
             << "\"nr50\":" << static_cast<int>(regs[0x14]) << ","
             << "\"nr51\":" << static_cast<int>(regs[0x15]) << ","
             << "\"leftVolume\":" << outputVolume(true) << ","
             << "\"rightVolume\":" << outputVolume(false) << ","
             << "\"noiseClockHz\":" << noiseClockHz() << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 2) << ","
             << "\"noiseRegister\":" << static_cast<int>(regs[0x12]) << ","
             << "\"noiseShift\":" << static_cast<int>(noiseClockShift()) << ","
             << "\"noiseDivisorCode\":" << static_cast<int>(noiseDivisorCode()) << ","
             << "\"noiseWidth7\":" << (noiseWidth7() ? 1 : 0) << ","
             << "\"waveShape\":" << patch.waveShape << ","
             << "\"waveRam0\":" << static_cast<int>(regs[0x20]) << ","
             << "\"waveRam15\":" << static_cast<int>(regs[0x2f]) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNotePulse1\":" << channelNotes[0] << ","
             << "\"assignedNotePulse2\":" << channelNotes[1] << ","
             << "\"assignedNoteWave\":" << channelNotes[2] << ","
             << "\"envelope0\":" << static_cast<int>(envelopeLevel[0]) << ","
             << "\"envelope1\":" << static_cast<int>(envelopeLevel[1]) << ","
             << "\"envelopeNoise\":" << static_cast<int>(envelopeLevel[3]) << ","
             << "\"envelopeDecayControl\":" << patch.envelopeDecay << ","
             << "\"initialEnvelope0\":" << static_cast<int>(initialEnvelopeVolume(0)) << ","
             << "\"initialEnvelope1\":" << static_cast<int>(initialEnvelopeVolume(1)) << ","
             << "\"initialEnvelopeNoise\":" << static_cast<int>(initialEnvelopeVolume(3)) << ","
             << "\"envelopePeriod0\":" << static_cast<int>(envelopePeriod(0)) << ","
             << "\"envelopePeriod1\":" << static_cast<int>(envelopePeriod(1)) << ","
             << "\"envelopePeriodNoise\":" << static_cast<int>(envelopePeriod(3)) << ","
             << "\"envelopeDirection0\":" << (envelopeIncreasing(0) ? 1 : 0) << ","
             << "\"envelopeDirection1\":" << (envelopeIncreasing(1) ? 1 : 0) << ","
             << "\"envelopeDirectionNoise\":" << (envelopeIncreasing(3) ? 1 : 0) << ","
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

    uint8_t pulseDutyIndex(int channel) const
    {
        return static_cast<uint8_t>((regs[channel == 0 ? 0x01 : 0x06] >> 6u) & 0x03u);
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
        lengthClockPhase = 0.0;
        envelopeClockPhase = 0.0;
        sweepClockPhase = 0.0;
        sweepShadowPeriod = 0;
        sweepTimer = 0;
        sweepEnabled = false;
        lfsr = 0x7fff;
    }

    void silenceChannel(size_t channel)
    {
        if (channel >= enabled.size())
            return;

        enabled[channel] = false;
        lengthCounter[channel] = 0;
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

        enabled[channel] = dacEnabled(channel);

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

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (! sourceEnabled(patch, channel))
                continue;

            if (channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (! sourceEnabled(patch, channel))
                continue;

            if (channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        const auto oldest = std::min_element(channelStamp.begin(), channelStamp.end());
        return static_cast<int>(std::distance(channelStamp.begin(), oldest));
    }

    int activeChipPolyChannels() const
    {
        return static_cast<int>(std::count_if(channelNotes.begin(), channelNotes.end(), [](int note) { return note >= 0; }));
    }

    static uint8_t waveOutputLevelForVelocity(float velocity)
    {
        if (velocity <= 0.0f)
            return 0x00u;
        if (velocity >= 0.75f)
            return 0x20u;
        if (velocity >= 0.35f)
            return 0x40u;
        return 0x60u;
    }

    void clearChipPolyState()
    {
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        noteVelocity = 0.0f;
        for (auto& channel : enabled)
            channel = false;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        const auto index = static_cast<size_t>(channel);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;

        writeRegister(0xff26, 0x80);
        writeRegister(0xff24, 0x77);
        writeRegister(0xff25, 0xff);

        if (channel == 0)
        {
            const auto volume = scaledChipPolyEnvelopeVolume(index);
            writePulseRegisters(0, dmgPulseDutyFromControl(patch.control1), volume, channelNotes[index]);
        }
        else if (channel == 1)
        {
            const auto volume = scaledChipPolyEnvelopeVolume(index);
            writePulseRegisters(1, dmgPulseDutyFromControl(patch.control1), volume, channelNotes[index]);
        }
        else
        {
            writePatchWaveRam();
            writeWaveRegisters(channelNotes[index], waveOutputLevelForVelocity(channelVelocity[index]));
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            enabled[channel] = false;
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void writePulseRegisters(int channel, DmgPulseDuty duty, uint8_t volume, int midiNote)
    {
        const auto base = channel == 0 ? 0xff11 : 0xff16;
        const auto freq = dmgFrequencyRegister(131072.0, 1.0, midiNote);
        writeRegister(static_cast<uint16_t>(base), static_cast<uint8_t>((dmgPulseDutyBits(duty) << 6u) | 0x3fu));
        writeRegister(static_cast<uint16_t>(base + 1), dmgEnvelopeRegisterValue(volume));
        writeRegister(static_cast<uint16_t>(base + 2), static_cast<uint8_t>(freq & 0xffu));
        writeRegister(static_cast<uint16_t>(base + 3), static_cast<uint8_t>(0x80u | ((freq >> 8u) & 0x07u)));
    }

    uint8_t scaledChipPolyEnvelopeVolume(size_t index) const
    {
        const auto maxVolume = static_cast<int>(dmgInitialEnvelopeLevelForControl(patch.control4));
        if (channelVelocity[index] <= 0.0f)
            return 0;

        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(maxVolume) * channelVelocity[index])), 1, 15));
    }

    void writeWaveRegisters(int midiNote, unsigned outputLevelBits)
    {
        const auto freq = dmgFrequencyRegister(65536.0, 1.0, midiNote);
        writeRegister(0xff1a, 0x80);
        writeRegister(0xff1c, static_cast<uint8_t>(outputLevelBits & 0x60u));
        writeRegister(0xff1d, static_cast<uint8_t>(freq & 0xffu));
        writeRegister(0xff1e, static_cast<uint8_t>(0x80u | ((freq >> 8u) & 0x07u)));
    }

    void loadDefaultWaveRam()
    {
        for (size_t i = 0; i < 16; ++i)
        {
            const auto high = static_cast<uint8_t>((i < 8 ? i : 15 - i) & 0x0f);
            const auto low = static_cast<uint8_t>((15 - high) & 0x0f);
            regs[0x20 + i] = static_cast<uint8_t>((high << 4u) | low);
        }
    }

    void writePatchWaveRam()
    {
        if (patch.waveShape <= 0)
            return;

        for (size_t i = 0; i < 16; ++i)
            regs[0x20 + i] = packedWaveByte(patch.waveShape, i);
    }

    static uint8_t packedWaveByte(int shape, size_t byteIndex)
    {
        const auto high = waveSample(shape, byteIndex * 2u);
        const auto low = waveSample(shape, (byteIndex * 2u) + 1u);
        return static_cast<uint8_t>((high << 4u) | low);
    }

    static uint8_t waveSample(int shape, size_t sampleIndex)
    {
        const auto index = static_cast<int>(sampleIndex & 0x1fu);
        switch (std::clamp(shape, 0, 4))
        {
            case 1:
                return static_cast<uint8_t>(index < 16 ? index : 31 - index);
            case 2:
                return static_cast<uint8_t>(index / 2);
            case 3:
                return index < 16 ? uint8_t { 15 } : uint8_t { 0 };
            case 4:
                return static_cast<uint8_t>(((index / 4) % 4) * 5);
            case 0:
            default:
                return 0;
        }
    }

    void writeNoiseRegisters(uint8_t volume, uint8_t noiseRegister)
    {
        writeRegister(0xff21, dmgEnvelopeRegisterValue(volume));
        writeRegister(0xff22, noiseRegister);
        writeRegister(0xff23, 0x80);
    }

    uint8_t dmgEnvelopeRegisterValue(uint8_t volume) const
    {
        const auto level = static_cast<uint8_t>(std::min<uint8_t>(15, volume) << 4u);
        if (! envelopeDecayActive(patch))
            return static_cast<uint8_t>(level | 0x08u);

        return static_cast<uint8_t>(level | decayPeriodFromControl(patch.envelopeDecay, 7));
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

        const auto amp = phase[channel] < dutyTable[pulseDutyIndex(channel)] ? 1.0 : -1.0;
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
    double lengthClockPhase = 0.0;
    double envelopeClockPhase = 0.0;
    double sweepClockPhase = 0.0;
    uint16_t sweepShadowPeriod = 0;
    uint8_t sweepTimer = 0;
    bool sweepEnabled = false;
    uint16_t lfsr = 0x7fff;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 3> channelNotes { -1, -1, -1 };
    std::array<float, 3> channelVelocity {};
    std::array<uint64_t, 3> channelStamp {};
    uint64_t noteStamp = 0;
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
        frameSequencerPhase = 0.0;
        frameSequenceStep = 0;
        frameCounterFiveStep = false;
        frameIrqInhibit = false;
        frameIrqFlag = false;
        hp90Input = 0.0;
        hp90Output = 0.0;
        hp440Input = 0.0;
        hp440Output = 0.0;
        lp14000Output = 0.0;
        lfsr = 1;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        if (address >= 0x4000 && address <= 0x4017)
            regs[static_cast<size_t>(address - 0x4000)] = value;

        if (address == 0x4011)
            regs[0x11] = static_cast<uint8_t>(value & 0x7fu);

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
        else if (address == 0x4017)
        {
            writeFrameCounter(value);
        }

        updateTimers();
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto duty = nesPulseDutyFromControl(patch.control1);

        auto p1Note = midiNote;
        auto p2Note = midiNote;
        auto triNote = midiNote - 12;
        auto p1Vol = 15u;
        auto p2Vol = 8u;
        auto noiseVol = static_cast<unsigned>(std::round(patch.control3 * 12.0f));
        auto enable = 0x03u;

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
                break;
            case MacroKind::hit:
                p1Note = midiNote - 5;
                p2Vol = 0u;
                noiseVol = static_cast<unsigned>(8u + std::round(patch.control3 * 7.0f));
                enable = 0x09u;
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

        enable &= sourceEnableMask(patch);

        writePulseRegisters(0x4000, duty, p1Vol, p1Note);
        writePulseRegisters(0x4004, offsetNesPulseDuty(duty, 1), p2Vol, p2Note);

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
        writeRegister(0x400c, nesNoiseEnvelopeValue(noiseVol));
        writeRegister(0x400e, nesNoiseRegisterForPatch(patch));
        writeRegister(0x400f, 0x18);
        writeRegister(0x4015, static_cast<uint8_t>(enable));
    }

    void noteOff(int midiNote) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOffChipPoly(midiNote);
            return;
        }

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
        const auto dmc = static_cast<double>(dmcOutputLevel());

        const auto pulseSum = p1 + p2;
        const auto tndSum = tri / 8227.0 + noi / 12241.0 + dmc / 22638.0;
        const auto pulseOut = pulseSum <= 0.0 ? 0.0 : 95.88 / ((8128.0 / pulseSum) + 100.0);
        const auto tndOut = tndSum <= 0.0 ? 0.0 : 159.79 / ((1.0 / tndSum) + 100.0);
        const auto mixed = static_cast<float>(applyOutputFilters(pulseOut + tndOut) * 2.0);
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
        return "Pulse, triangle, noise, timers, duty, enable bits, simple envelopes, length counters, triangle linear counter, DMC direct DAC level, basic pulse sweep updates/muting, $4017 frame-counter mode/inhibit behavior, nonlinear mixer, the documented NES output filter chain, and pulse/triangle allocation for Chip Poly play mode are approximated; DMC sample playback, exact frame sequencer cycle timing, advanced sweep edge cases, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"NES / RP2A03\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"pulseTimer1\":" << timer[0] << ","
             << "\"pulseTimer2\":" << timer[1] << ","
             << "\"triangleTimer\":" << timer[2] << ","
             << "\"pulseDuty1\":" << static_cast<int>(pulseDutyIndex(0)) << ","
             << "\"pulseDuty2\":" << static_cast<int>(pulseDutyIndex(1)) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 2) << ","
             << "\"noiseRegister\":" << static_cast<int>(regs[0x0e]) << ","
             << "\"noiseShortMode\":" << (((regs[0x0e] & 0x80u) != 0) ? 1 : 0) << ","
             << "\"noisePeriod\":" << static_cast<int>(regs[0x0e] & 0x0fu) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"enabled0\":" << (enabled[0] ? 1 : 0) << ","
             << "\"enabled1\":" << (enabled[1] ? 1 : 0) << ","
             << "\"enabled2\":" << (enabled[2] ? 1 : 0) << ","
             << "\"enabled3\":" << (enabled[3] ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNotePulse1\":" << channelNotes[0] << ","
             << "\"assignedNotePulse2\":" << channelNotes[1] << ","
             << "\"assignedNoteTriangle\":" << channelNotes[2] << ","
             << "\"triangleActive\":" << (triangleActive() ? 1 : 0) << ","
             << "\"linearCounter\":" << static_cast<int>(linearCounter) << ","
             << "\"linearReloadValue\":" << static_cast<int>(linearReloadValue()) << ","
             << "\"linearReloadFlag\":" << (linearReloadFlag ? 1 : 0) << ","
             << "\"linearControlFlag\":" << (linearControlFlag() ? 1 : 0) << ","
             << "\"frameCounterFiveStep\":" << (frameCounterFiveStep ? 1 : 0) << ","
             << "\"frameIrqInhibit\":" << (frameIrqInhibit ? 1 : 0) << ","
             << "\"frameIrqFlag\":" << (frameIrqFlag ? 1 : 0) << ","
             << "\"frameSequenceStep\":" << static_cast<int>(frameSequenceStep) << ","
             << "\"dmcLevel\":" << static_cast<int>(dmcOutputLevel()) << ","
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
             << "\"envelopeDecayControl\":" << patch.envelopeDecay << ","
             << "\"envelopePeriod0\":" << static_cast<int>(envelopePeriodForRegister(0x00)) << ","
             << "\"envelopePeriod1\":" << static_cast<int>(envelopePeriodForRegister(0x04)) << ","
             << "\"envelopePeriodNoise\":" << static_cast<int>(envelopePeriodForRegister(0x0c)) << ","
             << "\"envelopeConstant0\":" << (constantEnvelopeFlag(0x00) ? 1 : 0) << ","
             << "\"envelopeConstant1\":" << (constantEnvelopeFlag(0x04) ? 1 : 0) << ","
             << "\"envelopeConstantNoise\":" << (constantEnvelopeFlag(0x0c) ? 1 : 0) << ","
             << "\"length0\":" << static_cast<int>(lengthCounter[0]) << ","
             << "\"lengthTriangle\":" << static_cast<int>(lengthCounter[2]) << ","
             << "\"lengthNoise\":" << static_cast<int>(lengthCounter[3]) << ","
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

    uint8_t dmcOutputLevel() const
    {
        return static_cast<uint8_t>(regs[0x11] & 0x7fu);
    }

    double highPass(double input, double cutoffHz, double& previousInput, double& previousOutput) const
    {
        if (sampleRate <= 0.0)
            return input;

        const auto rc = 1.0 / (twoPi * cutoffHz);
        const auto dt = 1.0 / sampleRate;
        const auto coefficient = rc / (rc + dt);
        const auto output = coefficient * (previousOutput + input - previousInput);
        previousInput = input;
        previousOutput = output;
        return output;
    }

    double lowPass(double input, double cutoffHz, double& previousOutput) const
    {
        if (sampleRate <= 0.0)
            return input;

        const auto rc = 1.0 / (twoPi * cutoffHz);
        const auto dt = 1.0 / sampleRate;
        const auto coefficient = dt / (rc + dt);
        previousOutput += coefficient * (input - previousOutput);
        return previousOutput;
    }

    double applyOutputFilters(double input)
    {
        auto output = highPass(input, 90.0, hp90Input, hp90Output);
        output = highPass(output, 440.0, hp440Input, hp440Output);
        return lowPass(output, 14000.0, lp14000Output);
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
        frameSequencerPhase += 240.0 / sampleRate;
        while (frameSequencerPhase >= 1.0)
        {
            frameSequencerPhase -= 1.0;
            tickFrameSequenceStep();
        }
    }

    void writeFrameCounter(uint8_t value)
    {
        frameCounterFiveStep = (value & 0x80u) != 0;
        frameIrqInhibit = (value & 0x40u) != 0;
        if (frameIrqInhibit)
            frameIrqFlag = false;

        frameSequencerPhase = 0.0;
        frameSequenceStep = 0;

        if (frameCounterFiveStep)
            clockQuarterAndHalfFrame();
    }

    void clockQuarterFrame()
    {
        tickEnvelopes();
        tickLinearCounter();
    }

    void clockHalfFrame()
    {
        tickSweeps();
        tickLengthCounters();
    }

    void clockQuarterAndHalfFrame()
    {
        clockQuarterFrame();
        clockHalfFrame();
    }

    void tickFrameSequenceStep()
    {
        if (frameCounterFiveStep)
        {
            switch (frameSequenceStep)
            {
                case 0:
                case 2:
                    clockQuarterFrame();
                    break;
                case 1:
                case 4:
                    clockQuarterAndHalfFrame();
                    break;
                default:
                    break;
            }
            frameSequenceStep = static_cast<uint8_t>((frameSequenceStep + 1u) % 5u);
            return;
        }

        clockQuarterFrame();
        if ((frameSequenceStep & 1u) != 0)
            clockHalfFrame();
        if (frameSequenceStep == 3 && ! frameIrqInhibit)
            frameIrqFlag = true;

        frameSequenceStep = static_cast<uint8_t>((frameSequenceStep + 1u) % 4u);
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

    void writePulseRegisters(uint16_t baseAddress, NesPulseDuty duty, unsigned volume, int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto pulseTimer = static_cast<int>(std::max(0.0, std::round(clock / (16.0 * hz) - 1.0)));
        const auto constantVolume = ! envelopeDecayActive(patch)
            && (patch.macro == MacroKind::manual
                || patch.macro == MacroKind::lead
                || patch.macro == MacroKind::arp
                || patch.macro == MacroKind::bass
                || patch.macro == MacroKind::powerUp);
        const auto flags = constantVolume ? 0x10u : 0x00u;
        const auto periodOrVolume = envelopeDecayActive(patch)
            ? decayPeriodFromControl(patch.envelopeDecay, 15)
            : static_cast<uint8_t>(std::min<unsigned>(15u, volume) & 0x0fu);
        writeRegister(baseAddress, static_cast<uint8_t>((nesPulseDutyBits(duty) << 6u) | flags | periodOrVolume));
        writeRegister(static_cast<uint16_t>(baseAddress + 2), static_cast<uint8_t>(pulseTimer & 0xff));
        writeRegister(static_cast<uint16_t>(baseAddress + 3), static_cast<uint8_t>((pulseTimer >> 8) & 0x07));
    }

    uint8_t nesNoiseEnvelopeValue(unsigned volume) const
    {
        if (envelopeDecayActive(patch))
            return decayPeriodFromControl(patch.envelopeDecay, 15);

        return static_cast<uint8_t>(std::min<unsigned>(15u, volume));
    }

    uint8_t envelopePeriodForRegister(int regIndex) const
    {
        return static_cast<uint8_t>(regs[static_cast<size_t>(regIndex)] & 0x0fu);
    }

    bool constantEnvelopeFlag(int regIndex) const
    {
        return (regs[static_cast<size_t>(regIndex)] & 0x10u) != 0;
    }

    void writeTriangleRegisters(int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto triTimer = static_cast<int>(std::max(0.0, std::round(clock / (32.0 * hz) - 1.0)));
        writeRegister(0x4008, 0xff);
        writeRegister(0x400a, static_cast<uint8_t>(triTimer & 0xff));
        writeRegister(0x400b, static_cast<uint8_t>((triTimer >> 8) & 0x07));
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (! sourceEnabled(patch, channel))
                continue;

            if (channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (! sourceEnabled(patch, channel))
                continue;

            if (channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        const auto oldest = std::min_element(channelStamp.begin(), channelStamp.end());
        return static_cast<int>(std::distance(channelStamp.begin(), oldest));
    }

    int activeChipPolyChannels() const
    {
        return static_cast<int>(std::count_if(channelNotes.begin(), channelNotes.end(), [](int note) { return note >= 0; }));
    }

    void clearChipPolyState()
    {
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        noteVelocity = 0.0f;
        for (auto& length : lengthCounter)
            length = 0;
        linearCounter = 0;
        linearReloadFlag = false;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        const auto index = static_cast<size_t>(channel);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;

        writeRegister(0x4015, 0x07);

        if (channel == 0)
        {
            const auto duty = nesPulseDutyFromControl(patch.control1);
            const auto volume = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
            writeRegister(0x4001, 0x08);
            writePulseRegisters(0x4000, duty, volume, channelNotes[index]);
        }
        else if (channel == 1)
        {
            const auto duty = nesPulseDutyFromControl(patch.control1);
            const auto volume = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
            writeRegister(0x4005, 0x08);
            writePulseRegisters(0x4004, duty, volume, channelNotes[index]);
        }
        else
        {
            writeTriangleRegisters(channelNotes[index]);
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            lengthCounter[channel] = 0;
            if (channel == 2)
            {
                linearCounter = 0;
                linearReloadFlag = false;
            }
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    uint8_t pulseDutyIndex(int index) const
    {
        const auto regIndex = index == 0 ? 0x00 : 0x04;
        return static_cast<uint8_t>((regs[regIndex] >> 6u) & 0x03u);
    }

    double dutyForPulse(int index) const
    {
        static constexpr std::array<double, 4> duty = { 0.125, 0.25, 0.5, 0.75 };
        return duty[pulseDutyIndex(index)];
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
    double frameSequencerPhase = 0.0;
    uint8_t frameSequenceStep = 0;
    bool frameCounterFiveStep = false;
    bool frameIrqInhibit = false;
    bool frameIrqFlag = false;
    double hp90Input = 0.0;
    double hp90Output = 0.0;
    double hp440Input = 0.0;
    double hp440Output = 0.0;
    double lp14000Output = 0.0;
    uint16_t lfsr = 1;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 3> channelNotes { -1, -1, -1 };
    std::array<float, 3> channelVelocity {};
    std::array<uint64_t, 3> channelStamp {};
    uint64_t noteStamp = 0;
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
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        regs[address & 0x0f] = value;
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        const auto noisePitch = ym2149NoisePeriodForControl(patch.control3);
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
                mixer = ym2149MixerRegisterForControl(patch.control4);
                break;
        }

        writeTone(0, noteA);
        writeTone(1, noteB);
        writeTone(2, noteC);
        const auto sourceAwareMixer = applySourceMixerMask(static_cast<uint8_t>(mixer));
        writeRegister(6, noisePitch);
        writeRegister(7, sourceAwareMixer);
        writeSourceAwareYmVolumeRegister(0, volA, sourceAwareMixer);
        writeSourceAwareYmVolumeRegister(1, volB, sourceAwareMixer);
        writeSourceAwareYmVolumeRegister(2, volC, sourceAwareMixer);
        writeYmEnvelopePeriod();
        writeRegister(13, ymEnvelopeShapeCode());
    }

    void noteOff(int midiNote) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOffChipPoly(midiNote);
            return;
        }

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
        return "Tone/noise registers, mixer bits, volume registers, basic envelope shapes and period registers, and YM channel allocation for Chip Poly play mode are modeled; exact analog output curve and timing validation are still required.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2149 / AY-3-8910\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"periodA\":" << tonePeriod(0) << ","
             << "\"periodB\":" << tonePeriod(1) << ","
             << "\"periodC\":" << tonePeriod(2) << ","
             << "\"mixer\":" << static_cast<int>(regs[7]) << ","
             << "\"noisePeriod\":" << static_cast<int>(regs[6] & 0x1f) << ","
             << "\"sourceEnabledA\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabledB\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabledC\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabledNoise\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"volumeA\":" << static_cast<int>(regs[8]) << ","
             << "\"volumeB\":" << static_cast<int>(regs[9]) << ","
             << "\"volumeC\":" << static_cast<int>(regs[10]) << ","
             << "\"envelopeDecayControl\":" << patch.envelopeDecay << ","
             << "\"envelopePeriod\":" << envelopePeriodRegister() << ","
             << "\"envelopeShapeChoice\":" << patch.ymEnvelopeShape << ","
             << "\"envelopeShape\":" << static_cast<int>(regs[13] & 0x0f) << ","
             << "\"envelopeEnabledA\":" << (envelopeEnabled(0) ? 1 : 0) << ","
             << "\"envelopeEnabledB\":" << (envelopeEnabled(1) ? 1 : 0) << ","
             << "\"envelopeEnabledC\":" << (envelopeEnabled(2) ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNoteA\":" << channelNotes[0] << ","
             << "\"assignedNoteB\":" << channelNotes[1] << ","
             << "\"assignedNoteC\":" << channelNotes[2] << ","
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

    uint16_t envelopePeriodRegister() const
    {
        return static_cast<uint16_t>(regs[11] | (regs[12] << 8u));
    }

    void writeYmEnvelopePeriod()
    {
        const auto period = ym2149EnvelopePeriodForControl(patch.envelopeDecay);
        writeRegister(11, static_cast<uint8_t>(period & 0xffu));
        writeRegister(12, static_cast<uint8_t>((period >> 8u) & 0xffu));
    }

    bool envelopeEnabled(int channel) const
    {
        return channel >= 0 && channel < 3 && (regs[static_cast<size_t>(8 + channel)] & 0x10u) != 0;
    }

    bool patchEnvelopeEnabled() const
    {
        return patch.ymEnvelopeShape > 0;
    }

    uint8_t ymEnvelopeShapeCode() const
    {
        if (! patchEnvelopeEnabled())
            return patch.macro == MacroKind::powerUp ? 0x0au : 0x09u;

        switch (std::clamp(patch.ymEnvelopeShape, 0, 4))
        {
            case 1: return 0x09u; // Fall, hold low.
            case 2: return 0x0du; // Rise, hold high.
            case 3: return 0x08u; // Repeating saw down.
            case 4: return 0x0eu; // Repeating triangle.
            case 0:
            default:
                return 0x09u;
        }
    }

    void writeYmVolumeRegister(uint16_t reg, unsigned volume)
    {
        const auto clipped = std::min<unsigned>(15u, volume);
        if (patchEnvelopeEnabled() && clipped > 0)
            writeRegister(reg, 0x10u);
        else
            writeRegister(reg, static_cast<uint8_t>(clipped));
    }

    uint8_t applySourceMixerMask(uint8_t mixer) const
    {
        for (int channel = 0; channel < 3; ++channel)
        {
            if (! sourceEnabled(patch, static_cast<size_t>(channel)))
                mixer = static_cast<uint8_t>(mixer | (1u << channel) | (1u << (channel + 3)));
        }

        if (! sourceEnabled(patch, 3))
            mixer = static_cast<uint8_t>(mixer | 0x38u);

        return mixer;
    }

    void writeSourceAwareYmVolumeRegister(int channel, unsigned volume, uint8_t mixer)
    {
        const auto toneDisabled = (mixer & (1u << channel)) != 0;
        const auto noiseDisabled = (mixer & (1u << (channel + 3))) != 0;
        if (! sourceEnabled(patch, static_cast<size_t>(channel)) || (toneDisabled && noiseDisabled))
        {
            writeRegister(static_cast<uint16_t>(8 + channel), 0);
            return;
        }

        writeYmVolumeRegister(static_cast<uint16_t>(8 + channel), volume);
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        const auto oldest = std::min_element(channelStamp.begin(), channelStamp.end());
        return static_cast<int>(std::distance(channelStamp.begin(), oldest));
    }

    void refreshChipPolyMixer()
    {
        uint8_t mixer = 0x38u; // Noise disabled for A/B/C; active channels keep tone enabled.
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelNotes[channel] < 0 || ! sourceEnabled(patch, channel))
                mixer = static_cast<uint8_t>(mixer | (1u << channel));
        }
        writeRegister(7, applySourceMixerMask(mixer));
    }

    int activeChipPolyChannels() const
    {
        int active = 0;
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] >= 0)
                ++active;
        }
        return active;
    }

    void clearChipPolyState()
    {
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        noteVelocity = 0.0f;
        for (uint16_t reg = 8; reg <= 10; ++reg)
            writeRegister(reg, 0);
        refreshChipPolyMixer();
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        const auto index = static_cast<size_t>(channel);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;

        writeTone(channel, channelNotes[index]);
        writeYmVolumeRegister(static_cast<uint16_t>(8 + channel),
                              static_cast<unsigned>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15)));
        writeRegister(6, ym2149NoisePeriodForControl(patch.control3));
        writeYmEnvelopePeriod();
        writeRegister(13, ymEnvelopeShapeCode());
        refreshChipPolyMixer();
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            writeRegister(static_cast<uint16_t>(8 + channel), 0);
        }

        refreshChipPolyMixer();
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
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
    std::array<int, 3> channelNotes { -1, -1, -1 };
    std::array<float, 3> channelVelocity {};
    std::array<uint64_t, 3> channelStamp {};
    uint64_t noteStamp = 0;
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
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

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
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        auto note0 = midiNote;
        auto note1 = midiNote + std::max(1, spread / 2);
        auto note2 = midiNote + std::max(2, spread);
        auto noiseAttenuation = sn76489NoiseAttenuationForControl(patch.control4);

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
                noiseAttenuation = 0x02u;
                attenuation = { 0x0f, 0x0f, 0x0f, noiseAttenuation };
                break;
            case MacroKind::hit:
                attenuation = { 0x04, 0x0f, 0x0f, 0x03 };
                break;
            case MacroKind::laser:
                note0 = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                note1 = midiNote - 12;
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
                break;
        }

        tonePeriod[0] = notePeriod(note0);
        tonePeriod[1] = notePeriod(note1);
        tonePeriod[2] = notePeriod(note2);
        noiseControl = sn76489NoiseControlForPatch(patch);
        applySourceAttenuationMask();
    }

    void noteOff(int midiNote) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOffChipPoly(midiNote);
            return;
        }

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
        return "Latch/data writes, tone periods, attenuation, noise modes, and tone-channel allocation for Chip Poly play mode are modeled; exact variant behavior and hardware-level output validation are still required.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SN76489 / Sega PSG\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"period0\":" << tonePeriod[0] << ","
             << "\"period1\":" << tonePeriod[1] << ","
             << "\"period2\":" << tonePeriod[2] << ","
             << "\"noiseControl\":" << static_cast<int>(noiseControl) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"noiseWhite\":" << (((noiseControl & 0x04u) != 0) ? 1 : 0) << ","
             << "\"noiseRate\":" << static_cast<int>(noiseControl & 0x03u) << ","
             << "\"noiseTone3Clocked\":" << (((noiseControl & 0x03u) == 3) ? 1 : 0) << ","
             << "\"noiseDivider\":" << noiseClockDivider() << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabledNoise\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"attenuation0\":" << static_cast<int>(attenuation[0]) << ","
             << "\"attenuation1\":" << static_cast<int>(attenuation[1]) << ","
             << "\"attenuation2\":" << static_cast<int>(attenuation[2]) << ","
             << "\"attenuationNoise\":" << static_cast<int>(attenuation[3]) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
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

    void applySourceAttenuationMask()
    {
        for (size_t channel = 0; channel < attenuation.size(); ++channel)
        {
            if (! sourceEnabled(patch, channel))
                attenuation[channel] = 0x0f;
        }
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelStamp.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelStamp[channel] < oldestStamp)
            {
                oldestStamp = channelStamp[channel];
                oldestChannel = static_cast<int>(channel);
            }
        }

        return oldestChannel;
    }

    int activeChipPolyChannels() const
    {
        int active = 0;
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] >= 0)
                ++active;
        }
        return active;
    }

    void clearChipPolyState()
    {
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        noteVelocity = 0.0f;
        attenuation = { 0x0f, 0x0f, 0x0f, 0x0f };
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        const auto index = static_cast<size_t>(channel);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;
        tonePeriod[index] = notePeriod(channelNotes[index]);
        attenuation[index] = static_cast<uint8_t>(std::clamp(static_cast<int>(15 - std::round(channelVelocity[index] * 15.0f)), 0, 15));
        attenuation[3] = 0x0f;
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            attenuation[channel] = 0x0f;
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
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
        const auto divider = noiseClockDivider();
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

    int noiseClockDivider() const
    {
        static constexpr std::array<int, 4> noiseDividers = { 512, 1024, 2048, 1 };
        const auto mode = noiseControl & 0x03u;
        return mode == 3 ? std::max<int>(1, static_cast<int>(tonePeriod[2]) * 32) : noiseDividers[mode];
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
    std::array<int, 3> channelNotes { -1, -1, -1 };
    std::array<float, 3> channelVelocity {};
    std::array<uint64_t, 3> channelStamp {};
    uint64_t noteStamp = 0;
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

std::optional<PlayMode> parsePlayMode(std::string_view text)
{
    const auto key = lower(text);
    if (key == "stack" || key == "bigmono" || key == "mono") return PlayMode::stack;
    if (key == "chippoly" || key == "allocate" || key == "allocation") return PlayMode::chipPoly;
    if (key == "manual" || key == "multi") return PlayMode::manual;
    if (key == "clone" || key == "hybridpoly") return PlayMode::clone;
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

std::string toString(PlayMode playMode)
{
    switch (playMode)
    {
        case PlayMode::stack: return "Big Mono";
        case PlayMode::chipPoly: return "Chip Poly";
        case PlayMode::manual: return "Manual";
        case PlayMode::clone: return "Clone";
    }
    return "Unknown";
}

double midiNoteToHz(int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

} // namespace chipper
