#include "Engine/ChipCore.h"
#include "Engine/ChipDescriptors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <utility>

extern "C"
{
#include "emu2149.h"
#include "emu2212.h"
#include "emu2413.h"
#include "emu76489.h"
}

#include "ymfm_opn.h"
#include "ymfm_opl.h"
#include "ymfm_opm.h"

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

NesPulseDuty nesPulse2DutyForPatch(const PatchConfig& patch, NesPulseDuty pulse1Duty, bool stackedMacroOffset)
{
    const auto choice = std::clamp(patch.pulse2Duty, 0, 4);
    if (choice > 0)
        return static_cast<NesPulseDuty>(choice - 1);

    return stackedMacroOffset ? offsetNesPulseDuty(pulse1Duty, 1) : pulse1Duty;
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

DmgPulseDuty dmgPulse2DutyForPatch(const PatchConfig& patch, DmgPulseDuty pulse1Duty, bool stackedMacroOffset)
{
    const auto choice = std::clamp(patch.pulse2Duty, 0, 4);
    if (choice > 0)
        return static_cast<DmgPulseDuty>(choice - 1);

    return stackedMacroOffset ? offsetDmgPulseDuty(pulse1Duty, 1) : pulse1Duty;
}

uint8_t dmgPulseDutyBits(DmgPulseDuty duty)
{
    return static_cast<uint8_t>(duty);
}

bool sourceEnabled(const PatchConfig& patch, size_t index)
{
    return index < patch.sourceEnabled.size() && patch.sourceEnabled[index];
}

double sourceLevel(const PatchConfig& patch, size_t index)
{
    return index < patch.sourceLevels.size() ? clamp01(patch.sourceLevels[index]) : 1.0;
}

StereoFrame modernStereoGains(const PatchConfig& patch, double basePosition)
{
    const auto pan = std::clamp(basePosition * clamp01(patch.stereoSpread), -1.0, 1.0);
    const auto left = pan > 0.0 ? 1.0 - (0.75 * pan) : 1.0;
    const auto right = pan < 0.0 ? 1.0 + (0.75 * pan) : 1.0;
    return { static_cast<float>(left), static_cast<float>(right) };
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

uint8_t enabledSourceMaskOrRecipe(const PatchConfig& patch, uint8_t recipeMask)
{
    const auto requestedMask = sourceEnableMask(patch);
    return requestedMask != 0u ? requestedMask : recipeMask;
}

uint8_t ensureDmgRegisterVolume(uint8_t volume, uint8_t fallback)
{
    return volume > 0u ? volume : std::clamp<uint8_t>(fallback, static_cast<uint8_t>(1u), static_cast<uint8_t>(15u));
}

unsigned ensureNesRegisterVolume(unsigned volume, unsigned fallback)
{
    return volume > 0u ? volume : std::clamp<unsigned>(fallback, 1u, 15u);
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
        const auto previousPatch = patch;
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;

        if (newPatch.playMode == previousPatch.playMode
            && newPatch.sourceEnabled == previousPatch.sourceEnabled
            && (newPatch.control1 != previousPatch.control1 || newPatch.pulse2Duty != previousPatch.pulse2Duty))
        {
            refreshActivePulseDuties();
        }
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

        const auto enabledSources = enabledSourceMaskOrRecipe(patch, static_cast<uint8_t>(enable & 0x0fu)) & 0x0fu;
        if ((enabledSources & 0x01u) != 0u)
            ch1Vol = ensureDmgRegisterVolume(ch1Vol, envelope);
        if ((enabledSources & 0x02u) != 0u)
            ch2Vol = ensureDmgRegisterVolume(ch2Vol, static_cast<uint8_t>(std::max<int>(1, envelope - 4)));
        if ((enabledSources & 0x08u) != 0u)
            noiseVol = ensureDmgRegisterVolume(noiseVol, static_cast<uint8_t>(std::max<int>(8, std::round(patch.control3 * 15.0f))));
        enable = static_cast<unsigned>(0x80u | enabledSources);

        writeRegister(0xff26, 0x80);
        writeRegister(0xff24, 0x77);
        writeRegister(0xff25, dmgStereoRouteRegisterForPatch(patch));
        writeRegister(0xff10, dmgSweepRegisterForControl(patch.control2));
        if ((enable & 0x01u) != 0)
            writePulseRegisters(0, duty, ch1Vol, ch1Note);
        else
            silenceChannel(0);

        if ((enable & 0x02u) != 0)
            writePulseRegisters(1, dmgPulse2DutyForPatch(patch, duty, true), ch2Vol, ch2Note);
        else
            silenceChannel(1);

        if ((enable & 0x04u) != 0)
        {
            writePatchWaveRam();
            writeWaveRegisters(waveNote, dmgWaveOutputLevelBitsForPatch(patch, noteVelocity, false));
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

        const auto p1 = enabled[0] ? renderPulse(0) * sourceLevel(patch, 0) : 0.0;
        const auto p2 = enabled[1] ? renderPulse(1) * sourceLevel(patch, 1) : 0.0;
        const auto wave = enabled[2] ? renderWave() * sourceLevel(patch, 2) : 0.0;
        const auto noise = enabled[3] ? renderNoise() * sourceLevel(patch, 3) : 0.0;
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
             << "\"pulse2DutyChoice\":" << std::clamp(patch.pulse2Duty, 0, 4) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 3) << ","
             << "\"sweepRegister\":" << static_cast<int>(regs[0x00]) << ","
             << "\"sweepShadow\":" << sweepShadowPeriod << ","
             << "\"sweepTimer\":" << static_cast<int>(sweepTimer) << ","
             << "\"sweepEnabled\":" << (sweepEnabled ? 1 : 0) << ","
             << "\"sweepPace\":" << static_cast<int>(sweepPace()) << ","
             << "\"sweepShift\":" << static_cast<int>(sweepShift()) << ","
             << "\"sweepSubtract\":" << (sweepSubtract() ? 1 : 0) << ","
             << "\"nr50\":" << static_cast<int>(regs[0x14]) << ","
             << "\"nr51\":" << static_cast<int>(regs[0x15]) << ","
             << "\"dmgStereoRoute\":" << std::clamp(patch.dmgStereoRoute, 0, 4) << ","
             << "\"dmgStereoRouteRegister\":" << static_cast<int>(dmgStereoRouteRegisterForPatch(patch)) << ","
             << "\"leftVolume\":" << outputVolume(true) << ","
             << "\"rightVolume\":" << outputVolume(false) << ","
             << "\"noiseClockHz\":" << noiseClockHz() << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 2) << ","
             << "\"noiseRegister\":" << static_cast<int>(regs[0x12]) << ","
             << "\"noiseShift\":" << static_cast<int>(noiseClockShift()) << ","
             << "\"noiseDivisorCode\":" << static_cast<int>(noiseDivisorCode()) << ","
             << "\"noiseWidth7\":" << (noiseWidth7() ? 1 : 0) << ","
             << "\"waveShape\":" << patch.waveShape << ","
             << "\"dmgWaveLevel\":" << patch.dmgWaveLevel << ","
             << "\"nr32\":" << static_cast<int>(regs[0x0c]) << ","
             << "\"waveOutputLevelBits\":" << static_cast<int>(regs[0x0c] & 0x60u) << ","
             << "\"waveOutputLevelCode\":" << static_cast<int>((regs[0x0c] >> 5u) & 0x03u) << ","
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
        writeRegister(0xff25, dmgStereoRouteRegisterForPatch(patch));

        if (channel == 0)
        {
            const auto volume = scaledChipPolyEnvelopeVolume(index);
            writePulseRegisters(0, dmgPulseDutyFromControl(patch.control1), volume, channelNotes[index]);
        }
        else if (channel == 1)
        {
            const auto volume = scaledChipPolyEnvelopeVolume(index);
            const auto pulse1Duty = dmgPulseDutyFromControl(patch.control1);
            writePulseRegisters(1, dmgPulse2DutyForPatch(patch, pulse1Duty, false), volume, channelNotes[index]);
        }
        else
        {
            writePatchWaveRam();
            writeWaveRegisters(channelNotes[index], dmgWaveOutputLevelBitsForPatch(patch, channelVelocity[index], true));
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

    void writePulseDutyRegister(int channel, DmgPulseDuty duty)
    {
        const auto index = channel == 0 ? 0x01u : 0x06u;
        const auto address = channel == 0 ? 0xff11u : 0xff16u;
        const auto lengthBits = static_cast<uint8_t>(regs[index] & 0x3fu);
        writeRegister(static_cast<uint16_t>(address), static_cast<uint8_t>((dmgPulseDutyBits(duty) << 6u) | lengthBits));
    }

    void refreshActivePulseDuties()
    {
        const auto pulse1Duty = dmgPulseDutyFromControl(patch.control1);

        if (patch.playMode == PlayMode::chipPoly)
        {
            if (channelNotes[0] >= 0)
                writePulseDutyRegister(0, pulse1Duty);
            if (channelNotes[1] >= 0)
                writePulseDutyRegister(1, dmgPulse2DutyForPatch(patch, pulse1Duty, false));
            return;
        }

        if (heldNote < 0)
            return;

        if (enabled[0])
            writePulseDutyRegister(0, pulse1Duty);
        if (enabled[1])
            writePulseDutyRegister(1, dmgPulse2DutyForPatch(patch, pulse1Duty, true));
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

class SidCore final : public ChipCore
{
public:
    explicit SidCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 985248.0;
        regs.fill(0);
        phase.fill(0.0);
        oscillatorWrapped.fill(false);
        noisePhase.fill(0.0);
        envelope.fill(0.0);
        envelopeState.fill(EnvelopeState::release);
        voiceVelocity.fill(0.0f);
        channelNotes.fill(-1);
        channelStamp.fill(0);
        noteStamp = 0;
        noiseLfsr = { 0x7ffff8u, 0x6d2b79u, 0x5a1f33u };
        heldNote = -1;
        noteVelocity = 0.0f;
        filterLowpass = 0.0;
        filterBandpass = 0.0;
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
        if (index >= regs.size())
            return;

        const auto wasGate = isVoiceControl(index) && (regs[index] & 0x01u) != 0;
        regs[index] = value;

        if (! isVoiceControl(index))
            return;

        const auto voice = voiceForControlIndex(index);
        const auto gate = (value & 0x01u) != 0;
        if (gate && ! wasGate)
            startEnvelope(voice);
        else if (! gate && wasGate)
            releaseEnvelope(voice);

        if ((value & 0x08u) != 0)
        {
            phase[voice] = 0.0;
            noisePhase[voice] = 0.0;
            noiseLfsr[voice] = sidNoiseSeed(voice);
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));

        auto note0 = heldNote;
        auto note1 = heldNote + std::max(1, static_cast<int>(std::round(patch.control2 * 7.0f)));
        auto note2 = heldNote + std::max(2, static_cast<int>(std::round(patch.control2 * 12.0f)));

        switch (patch.macro)
        {
            case MacroKind::coin:
                note0 = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                note1 = note0 + 12;
                note2 = note0 + 19;
                break;
            case MacroKind::bass:
                note0 = heldNote - 12;
                note1 = heldNote - 24;
                note2 = heldNote - 5;
                break;
            case MacroKind::arp:
                note1 = heldNote + 7;
                note2 = heldNote + 12;
                break;
            case MacroKind::drum:
                note0 = heldNote - 24;
                note1 = heldNote - 12;
                note2 = heldNote;
                break;
            case MacroKind::hit:
                note2 = heldNote - 12;
                break;
            case MacroKind::laser:
                note0 = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                note1 = heldNote - 12;
                note2 = heldNote + 7;
                break;
            case MacroKind::jump:
                note0 = heldNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                note1 = note0 + 7;
                note2 = note0 + 12;
                break;
            case MacroKind::powerUp:
                note1 = heldNote + 5;
                note2 = heldNote + 12;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        const auto enable = sourceEnableMask(patch) & 0x07u;
        writeFilterRegisters();

        const std::array<int, 3> notes { note0, note1, note2 };
        for (size_t voice = 0; voice < notes.size(); ++voice)
        {
            if ((enable & (1u << voice)) == 0)
            {
                clearGate(voice);
                voiceVelocity[voice] = 0.0f;
                continue;
            }

            voiceVelocity[voice] = noteVelocity;
            writeVoiceRegisters(voice, notes[voice], sidWaveformControlForVoice(patch, voice));
        }
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
            for (size_t voice = 0; voice < voiceCount; ++voice)
                clearGate(voice);

            heldNote = -1;
            noteVelocity = 0.0f;
        }
    }

    StereoFrame renderSample() override
    {
        double mixed = 0.0;
        double filteredMixed = 0.0;
        double directMixed = 0.0;
        double activeVoices = 0.0;
        const auto filterRouting = regs[0x17] & 0x07u;
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            tickEnvelope(voice);

            if (! sourceEnabled(patch, voice) || voiceVelocity[voice] <= 0.0f)
                continue;

            const auto env = envelope[voice];
            if (env <= 0.0001 && ! gateOn(voice))
                continue;

            const auto sample = renderVoice(voice) * env * sourceLevel(patch, voice) * voiceVelocity[voice];
            mixed += sample;
            if ((filterRouting & (1u << voice)) != 0u)
                filteredMixed += sample;
            else
                directMixed += sample;
            activeVoices += 1.0;
        }

        if (activeVoices > 0.0)
        {
            mixed /= std::max(1.0, activeVoices);
            filteredMixed /= std::max(1.0, activeVoices);
            directMixed /= std::max(1.0, activeVoices);
        }

        const auto routedOutput = filterRouting == 0x07u ? applySidOutputFilter(mixed) : (directMixed + applySidOutputFilter(filteredMixed));
        const auto output = static_cast<float>(applySidOutputDrive(routedOutput) * sidMasterVolume() * 0.85);
        return { output, output };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(regs.size());
        for (size_t i = 0; i < regs.size(); ++i)
            writes.push_back({ 0, static_cast<uint16_t>(0xd400 + i), regs[i] });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::sid; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "SID / C64"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Three SID oscillator voices, 24-bit frequency-register pitch mapping, waveform control bits, 12-bit pulse width, control-register sync/ring bits with a musical approximation, gate-driven ADSR approximation with exact attack/decay/sustain/release nibble overrides, source trims, cutoff/resonance/filter-routing register writes, selectable partial 6581/8580 filter/output profiles, and a first-pass multimode filter approximation are modeled; OSC3/ENV3 voice-3 readback, silent Voice 3 utility/mod-source behavior, model-specific Voice 3 noise leakage, exact 6581/8580 analog filter behavior, exact ADSR bugs, exact oscillator sync/ring timing, waveform-combination quirks, external input, DAC nonlinearity, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SID / C64\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"sidModelChoice\":" << sidModelChoiceForPatch(patch) << ","
             << "\"sidModelNumber\":" << sidModelNumberForPatch(patch) << ","
             << "\"waveformChoice\":" << std::clamp(patch.waveShape, 0, 8) << ","
             << "\"waveformBits\":" << static_cast<int>(sidWaveformControlForPatch(patch)) << ","
             << "\"waveformChoice0\":" << std::clamp(patch.waveShape, 0, 8) << ","
             << "\"waveformChoice1\":" << std::clamp(patch.sidVoice2WaveShape, 0, 8) << ","
             << "\"waveformChoice2\":" << std::clamp(patch.sidVoice3WaveShape, 0, 8) << ","
             << "\"waveformBits0\":" << static_cast<int>(sidWaveformControlForVoice(patch, 0)) << ","
             << "\"waveformBits1\":" << static_cast<int>(sidWaveformControlForVoice(patch, 1)) << ","
             << "\"waveformBits2\":" << static_cast<int>(sidWaveformControlForVoice(patch, 2)) << ","
             << "\"modModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"modBits0\":" << static_cast<int>(sidModulationBitsForPatch(patch, 0)) << ","
             << "\"modBits1\":" << static_cast<int>(sidModulationBitsForPatch(patch, 1)) << ","
             << "\"modBits2\":" << static_cast<int>(sidModulationBitsForPatch(patch, 2)) << ","
             << "\"pulseWidthControl\":" << patch.control1 << ","
             << "\"pulseWidthRegister\":" << sidPulseWidthForControl(patch.control1) << ","
             << "\"pulseWidthControl1\":" << patch.sidVoice2PulseWidth << ","
             << "\"pulseWidthControl2\":" << patch.sidVoice3PulseWidth << ","
             << "\"pulseWidthRegister0\":" << sidPulseWidthForVoice(patch, 0) << ","
             << "\"pulseWidthRegister1\":" << sidPulseWidthForVoice(patch, 1) << ","
             << "\"pulseWidthRegister2\":" << sidPulseWidthForVoice(patch, 2) << ","
             << "\"filterCutoffRegister\":" << filterCutoffRegister() << ","
             << "\"filterResonanceControl\":" << patch.stereoSpread << ","
             << "\"filterResonanceNibble\":" << static_cast<int>(sidFilterResonanceForControl(patch.stereoSpread)) << ","
             << "\"filterRoutingChoice\":" << std::clamp(patch.sidFilterRouting, 0, 8) << ","
             << "\"filterRoutingBits\":" << static_cast<int>(sidFilterRoutingBitsForPatch(patch)) << ","
             << "\"filterRoutingRegister\":" << static_cast<int>(regs[0x17]) << ","
             << "\"filterModeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 8) << ","
             << "\"filterModeBits\":" << static_cast<int>(sidFilterModeBitsForPatch(patch)) << ","
             << "\"filterModeVolume\":" << static_cast<int>(regs[0x18]) << ","
             << "\"modelCutoffHz\":" << sidModelCutoffHz() << ","
             << "\"modelFilterDamping\":" << sidModelFilterDamping() << ","
             << "\"modelOutputDrive\":" << sidModelOutputDrive() << ","
             << "\"attackChoice\":" << std::clamp(patch.sidAttack, 0, 16) << ","
             << "\"decayChoice\":" << std::clamp(patch.sidDecay, 0, 16) << ","
             << "\"sustainChoice\":" << std::clamp(patch.sidSustain, 0, 16) << ","
             << "\"releaseChoice\":" << std::clamp(patch.sidRelease, 0, 16) << ","
             << "\"attackChoice0\":" << std::clamp(patch.sidAttack, 0, 16) << ","
             << "\"decayChoice0\":" << std::clamp(patch.sidDecay, 0, 16) << ","
             << "\"sustainChoice0\":" << std::clamp(patch.sidSustain, 0, 16) << ","
             << "\"releaseChoice0\":" << std::clamp(patch.sidRelease, 0, 16) << ","
             << "\"attackChoice1\":" << std::clamp(patch.sidVoice2Attack, 0, 16) << ","
             << "\"decayChoice1\":" << std::clamp(patch.sidVoice2Decay, 0, 16) << ","
             << "\"sustainChoice1\":" << std::clamp(patch.sidVoice2Sustain, 0, 16) << ","
             << "\"releaseChoice1\":" << std::clamp(patch.sidVoice2Release, 0, 16) << ","
             << "\"attackChoice2\":" << std::clamp(patch.sidVoice3Attack, 0, 16) << ","
             << "\"decayChoice2\":" << std::clamp(patch.sidVoice3Decay, 0, 16) << ","
             << "\"sustainChoice2\":" << std::clamp(patch.sidVoice3Sustain, 0, 16) << ","
             << "\"releaseChoice2\":" << std::clamp(patch.sidVoice3Release, 0, 16) << ","
             << "\"attackNibbleResolved\":" << static_cast<int>(sidAttackNibbleForPatch(patch)) << ","
             << "\"decayNibbleResolved\":" << static_cast<int>(sidDecayNibbleForPatch(patch)) << ","
             << "\"sustainNibbleResolved\":" << static_cast<int>(sidSustainNibbleForPatch(patch)) << ","
             << "\"releaseNibbleResolved\":" << static_cast<int>(sidReleaseNibbleForPatch(patch)) << ","
             << "\"attackDecayRegister\":" << static_cast<int>(sidAttackDecayForPatch(patch)) << ","
             << "\"sustainReleaseRegister\":" << static_cast<int>(sidSustainReleaseForPatch(patch)) << ","
             << "\"attackDecayRegister0\":" << static_cast<int>(sidAttackDecayForVoice(patch, 0)) << ","
             << "\"sustainReleaseRegister0\":" << static_cast<int>(sidSustainReleaseForVoice(patch, 0)) << ","
             << "\"attackDecayRegister1\":" << static_cast<int>(sidAttackDecayForVoice(patch, 1)) << ","
             << "\"sustainReleaseRegister1\":" << static_cast<int>(sidSustainReleaseForVoice(patch, 1)) << ","
             << "\"attackDecayRegister2\":" << static_cast<int>(sidAttackDecayForVoice(patch, 2)) << ","
             << "\"sustainReleaseRegister2\":" << static_cast<int>(sidSustainReleaseForVoice(patch, 2)) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 2) << ","
             << "\"frequency0\":" << frequencyRegister(0) << ","
             << "\"frequency1\":" << frequencyRegister(1) << ","
             << "\"frequency2\":" << frequencyRegister(2) << ","
             << "\"pulseWidth0\":" << pulseWidthRegister(0) << ","
             << "\"pulseWidth1\":" << pulseWidthRegister(1) << ","
             << "\"pulseWidth2\":" << pulseWidthRegister(2) << ","
             << "\"control0\":" << static_cast<int>(controlRegister(0)) << ","
             << "\"control1\":" << static_cast<int>(controlRegister(1)) << ","
             << "\"control2\":" << static_cast<int>(controlRegister(2)) << ","
             << "\"gate0\":" << (gateOn(0) ? 1 : 0) << ","
             << "\"gate1\":" << (gateOn(1) ? 1 : 0) << ","
             << "\"gate2\":" << (gateOn(2) ? 1 : 0) << ","
             << "\"attack0\":" << static_cast<int>(attackNibble(0)) << ","
             << "\"decay0\":" << static_cast<int>(decayNibble(0)) << ","
             << "\"sustain0\":" << static_cast<int>(sustainNibble(0)) << ","
             << "\"release0\":" << static_cast<int>(releaseNibble(0)) << ","
             << "\"envelope0\":" << envelope[0] << ","
             << "\"envelope1\":" << envelope[1] << ","
             << "\"envelope2\":" << envelope[2] << ","
             << "\"envelopeState0\":" << envelopeStateValue(envelopeState[0]) << ","
             << "\"envelopeState1\":" << envelopeStateValue(envelopeState[1]) << ","
             << "\"envelopeState2\":" << envelopeStateValue(envelopeState[2]) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    enum class EnvelopeState
    {
        attack,
        decay,
        sustain,
        release
    };

    static constexpr size_t voiceCount = 3;

    static size_t registerIndex(uint16_t address)
    {
        if (address >= 0xd400 && address <= 0xd418)
            return static_cast<size_t>(address - 0xd400);
        if (address <= 0x18)
            return static_cast<size_t>(address);
        return static_cast<size_t>(address & 0x1f);
    }

    static bool isVoiceControl(size_t index)
    {
        return index == 0x04 || index == 0x0b || index == 0x12;
    }

    static size_t voiceForControlIndex(size_t index)
    {
        return index < 0x07 ? 0u : (index < 0x0e ? 1u : 2u);
    }

    static size_t voiceBase(size_t voice)
    {
        return std::min(voice, voiceCount - 1u) * 7u;
    }

    static size_t previousVoice(size_t voice)
    {
        const auto safeVoice = std::min(voice, voiceCount - 1u);
        return safeVoice == 0 ? voiceCount - 1u : safeVoice - 1u;
    }

    static uint32_t sidNoiseSeed(size_t voice)
    {
        static constexpr std::array<uint32_t, voiceCount> seeds { 0x7ffff8u, 0x6d2b79u, 0x5a1f33u };
        return seeds[std::min(voice, voiceCount - 1u)];
    }

    static int envelopeStateValue(EnvelopeState state)
    {
        switch (state)
        {
            case EnvelopeState::attack: return 1;
            case EnvelopeState::decay: return 2;
            case EnvelopeState::sustain: return 3;
            case EnvelopeState::release: return 4;
        }
        return 0;
    }

    uint16_t frequencyRegister(size_t voice) const
    {
        const auto base = voiceBase(voice);
        return static_cast<uint16_t>(regs[base] | (regs[base + 1u] << 8u));
    }

    uint16_t pulseWidthRegister(size_t voice) const
    {
        const auto base = voiceBase(voice);
        return static_cast<uint16_t>(regs[base + 2u] | ((regs[base + 3u] & 0x0fu) << 8u));
    }

    uint8_t controlRegister(size_t voice) const
    {
        return regs[voiceBase(voice) + 4u];
    }

    uint8_t attackDecayRegister(size_t voice) const
    {
        return regs[voiceBase(voice) + 5u];
    }

    uint8_t sustainReleaseRegister(size_t voice) const
    {
        return regs[voiceBase(voice) + 6u];
    }

    bool gateOn(size_t voice) const
    {
        return (controlRegister(voice) & 0x01u) != 0;
    }

    uint8_t attackNibble(size_t voice) const
    {
        return static_cast<uint8_t>((attackDecayRegister(voice) >> 4u) & 0x0fu);
    }

    uint8_t decayNibble(size_t voice) const
    {
        return static_cast<uint8_t>(attackDecayRegister(voice) & 0x0fu);
    }

    uint8_t sustainNibble(size_t voice) const
    {
        return static_cast<uint8_t>((sustainReleaseRegister(voice) >> 4u) & 0x0fu);
    }

    uint8_t releaseNibble(size_t voice) const
    {
        return static_cast<uint8_t>(sustainReleaseRegister(voice) & 0x0fu);
    }

    uint16_t filterCutoffRegister() const
    {
        return static_cast<uint16_t>((regs[0x15] & 0x07u) | (regs[0x16] << 3u));
    }

    double sidMasterVolume() const
    {
        return static_cast<double>(regs[0x18] & 0x0fu) / 15.0;
    }

    uint16_t frequencyRegisterForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto value = std::round((hz * 16777216.0) / clock);
        return static_cast<uint16_t>(std::clamp(value, 0.0, 65535.0));
    }

    void writeVoiceRegisters(size_t voice, int midiNote, uint8_t waveformBits)
    {
        const auto base = voiceBase(voice);
        const auto freq = frequencyRegisterForNote(midiNote);
        const auto pulseWidth = sidPulseWidthForVoice(patch, voice);
        writeRegister(static_cast<uint16_t>(0xd400 + base), static_cast<uint8_t>(freq & 0xffu));
        writeRegister(static_cast<uint16_t>(0xd401 + base), static_cast<uint8_t>((freq >> 8u) & 0xffu));
        writeRegister(static_cast<uint16_t>(0xd402 + base), static_cast<uint8_t>(pulseWidth & 0xffu));
        writeRegister(static_cast<uint16_t>(0xd403 + base), static_cast<uint8_t>((pulseWidth >> 8u) & 0x0fu));
        writeRegister(static_cast<uint16_t>(0xd405 + base), sidAttackDecayForVoice(patch, voice));
        writeRegister(static_cast<uint16_t>(0xd406 + base), sidSustainReleaseForVoice(patch, voice));
        writeRegister(static_cast<uint16_t>(0xd404 + base), static_cast<uint8_t>(waveformBits | sidModulationBitsForPatch(patch, voice) | 0x01u));
    }

    void writeFilterRegisters()
    {
        const auto cutoff = static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(clamp01(patch.control3) * 2047.0)), 0, 2047));
        const auto resonance = sidFilterResonanceForControl(patch.stereoSpread);
        const auto modeBits = sidFilterModeBitsForPatch(patch);
        writeRegister(0xd415, static_cast<uint8_t>(cutoff & 0x07u));
        writeRegister(0xd416, static_cast<uint8_t>((cutoff >> 3u) & 0xffu));
        writeRegister(0xd417, static_cast<uint8_t>((resonance << 4u) | sidFilterRoutingBitsForPatch(patch)));
        writeRegister(0xd418, static_cast<uint8_t>(modeBits | 0x0fu));
    }

    void clearGate(size_t voice)
    {
        const auto base = voiceBase(voice);
        writeRegister(static_cast<uint16_t>(0xd404 + base), static_cast<uint8_t>(regs[base + 4u] & ~0x01u));
    }

    void startEnvelope(size_t voice)
    {
        envelopeState[voice] = EnvelopeState::attack;
        if (envelope[voice] <= 0.0)
            envelope[voice] = 0.0;
    }

    void releaseEnvelope(size_t voice)
    {
        envelopeState[voice] = EnvelopeState::release;
    }

    static double attackSeconds(uint8_t nibble)
    {
        return sidAttackSecondsForNibble(nibble);
    }

    static double decayReleaseSeconds(uint8_t nibble)
    {
        return sidDecayReleaseSecondsForNibble(nibble);
    }

    void tickEnvelope(size_t voice)
    {
        const auto sustain = static_cast<double>(sustainNibble(voice)) / 15.0;
        switch (envelopeState[voice])
        {
            case EnvelopeState::attack:
                envelope[voice] += 1.0 / std::max(1.0, attackSeconds(attackNibble(voice)) * sampleRate);
                if (envelope[voice] >= 1.0)
                {
                    envelope[voice] = 1.0;
                    envelopeState[voice] = EnvelopeState::decay;
                }
                break;
            case EnvelopeState::decay:
                if (envelope[voice] > sustain)
                {
                    envelope[voice] -= (1.0 - sustain) / std::max(1.0, decayReleaseSeconds(decayNibble(voice)) * sampleRate);
                    if (envelope[voice] <= sustain)
                    {
                        envelope[voice] = sustain;
                        envelopeState[voice] = EnvelopeState::sustain;
                    }
                }
                else
                {
                    envelopeState[voice] = EnvelopeState::sustain;
                }
                break;
            case EnvelopeState::sustain:
                envelope[voice] = sustain;
                break;
            case EnvelopeState::release:
                if (envelope[voice] > 0.0)
                {
                    envelope[voice] -= 1.0 / std::max(1.0, decayReleaseSeconds(releaseNibble(voice)) * sampleRate);
                    if (envelope[voice] < 0.0)
                        envelope[voice] = 0.0;
                }
                break;
        }
    }

    int activeChipPolyChannels() const
    {
        int active = 0;
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] >= 0)
                ++active;
        }
        return active;
    }

    void clearChipPolyState()
    {
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            channelNotes[voice] = -1;
            voiceVelocity[voice] = 0.0f;
            channelStamp[voice] = 0;
            clearGate(voice);
        }
        noteStamp = 0;
        noteVelocity = 0.0f;
    }

    int selectChipPolyVoice(int midiNote) const
    {
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] == midiNote)
                return static_cast<int>(voice);
        }

        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] < 0)
                return static_cast<int>(voice);
        }

        auto oldestVoice = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelStamp[voice] < oldestStamp)
            {
                oldestStamp = channelStamp[voice];
                oldestVoice = static_cast<int>(voice);
            }
        }

        return oldestVoice;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto selected = selectChipPolyVoice(midiNote);
        if (selected < 0)
            return;

        const auto voice = static_cast<size_t>(selected);
        channelNotes[voice] = std::clamp(midiNote, 0, 127);
        channelStamp[voice] = ++noteStamp;
        voiceVelocity[voice] = static_cast<float>(clamp01(velocity));
        writeFilterRegisters();
        writeVoiceRegisters(voice, channelNotes[voice], sidWaveformControlForVoice(patch, voice));
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (channelNotes[voice] != midiNote)
                continue;

            channelNotes[voice] = -1;
            channelStamp[voice] = 0;
            clearGate(voice);
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    double oscillatorFrequency(size_t voice) const
    {
        return (static_cast<double>(frequencyRegister(voice)) * clock) / 16777216.0;
    }

    double renderVoice(size_t voice)
    {
        if ((controlRegister(voice) & 0x08u) != 0)
            return 0.0;

        const auto hz = oscillatorFrequency(voice);
        const auto nextPhase = phase[voice] + (hz / sampleRate);
        phase[voice] = wrapPhase(nextPhase);
        oscillatorWrapped[voice] = nextPhase >= 1.0;

        if ((controlRegister(voice) & 0x02u) != 0 && oscillatorWrapped[previousVoice(voice)])
        {
            phase[voice] = 0.0;
            oscillatorWrapped[voice] = true;
        }

        const auto waveform = controlRegister(voice) & 0xf0u;
        if ((waveform & 0x80u) != 0)
            return renderNoise(voice, hz);

        double mixed = 0.0;
        auto activeWaveforms = 0;
        if ((waveform & 0x40u) != 0)
        {
            if (pulseWidthRegister(voice) != 0)
            {
                const auto width = std::clamp(static_cast<double>(pulseWidthRegister(voice)) / 4096.0, 1.0 / 4096.0, 4095.0 / 4096.0);
                mixed += phase[voice] < width ? 1.0 : -1.0;
                ++activeWaveforms;
            }
        }
        if ((waveform & 0x20u) != 0)
        {
            mixed += (phase[voice] * 2.0) - 1.0;
            ++activeWaveforms;
        }
        if ((waveform & 0x10u) != 0)
        {
            auto triangle = phase[voice] < 0.5 ? (-1.0 + (phase[voice] * 4.0)) : (3.0 - (phase[voice] * 4.0));
            if ((controlRegister(voice) & 0x04u) != 0 && phase[previousVoice(voice)] >= 0.5)
                triangle = -triangle;
            mixed += triangle;
            ++activeWaveforms;
        }

        if (activeWaveforms > 0)
            return mixed / static_cast<double>(activeWaveforms);

        return 0.0;
    }

    double renderNoise(size_t voice, double hz)
    {
        noisePhase[voice] += hz / sampleRate;
        while (noisePhase[voice] >= 1.0)
        {
            noisePhase[voice] -= 1.0;
            const auto feedback = ((noiseLfsr[voice] >> 22u) ^ (noiseLfsr[voice] >> 17u)) & 0x01u;
            noiseLfsr[voice] = ((noiseLfsr[voice] << 1u) | feedback) & 0x7fffffu;
            if (noiseLfsr[voice] == 0)
                noiseLfsr[voice] = sidNoiseSeed(voice);
        }

        const auto output =
            (((noiseLfsr[voice] >> 20u) & 0x01u) << 7u)
            | (((noiseLfsr[voice] >> 18u) & 0x01u) << 6u)
            | (((noiseLfsr[voice] >> 14u) & 0x01u) << 5u)
            | (((noiseLfsr[voice] >> 11u) & 0x01u) << 4u)
            | (((noiseLfsr[voice] >> 9u) & 0x01u) << 3u)
            | (((noiseLfsr[voice] >> 5u) & 0x01u) << 2u)
            | (((noiseLfsr[voice] >> 2u) & 0x01u) << 1u)
            | ((noiseLfsr[voice] >> 0u) & 0x01u);
        return (static_cast<double>(output) / 127.5) - 1.0;
    }

    double sidModelCutoffHz() const
    {
        const auto cutoffNorm = static_cast<double>(filterCutoffRegister()) / 2047.0;
        if (sidModelChoiceForPatch(patch) == 2)
            return 45.0 + (std::pow(cutoffNorm, 1.55) * 14000.0);

        return 120.0 + (std::pow(cutoffNorm, 2.35) * 9000.0);
    }

    double sidModelFilterDamping() const
    {
        const auto resonance = static_cast<double>(sidFilterResonanceForControl(patch.stereoSpread)) / 15.0;
        if (sidModelChoiceForPatch(patch) == 2)
            return std::clamp(1.10 - (resonance * 0.82), 0.12, 1.10);

        return std::clamp(1.35 - (resonance * 0.68), 0.25, 1.35);
    }

    double sidModelOutputDrive() const
    {
        return sidModelChoiceForPatch(patch) == 2 ? 0.96 : 1.32;
    }

    double applySidOutputDrive(double input) const
    {
        const auto drive = sidModelOutputDrive();
        if (drive <= 1.0)
            return input * drive;

        const auto normalizer = std::tanh(drive);
        return normalizer > 0.0 ? std::tanh(input * drive) / normalizer : input;
    }

    double applySidOutputFilter(double input)
    {
        const auto modeBits = regs[0x18] & 0x70u;
        if (modeBits == 0)
            return input;

        const auto cutoffHz = sidModelCutoffHz();
        const auto coefficient = std::clamp(2.0 * std::sin((0.5 * twoPi * cutoffHz) / std::max(1.0, sampleRate)), 0.001, 0.99);
        const auto damping = sidModelFilterDamping();

        filterLowpass += coefficient * filterBandpass;
        const auto highpass = input - filterLowpass - (damping * filterBandpass);
        filterBandpass += coefficient * highpass;

        auto output = 0.0;
        if ((modeBits & 0x10u) != 0)
            output += filterLowpass;
        if ((modeBits & 0x20u) != 0)
            output += filterBandpass;
        if ((modeBits & 0x40u) != 0)
            output += highpass;

        return std::clamp(output, -4.0, 4.0);
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 985248.0;
    std::array<uint8_t, 0x19> regs {};
    std::array<double, voiceCount> phase {};
    std::array<bool, voiceCount> oscillatorWrapped {};
    std::array<double, voiceCount> noisePhase {};
    std::array<double, voiceCount> envelope {};
    std::array<EnvelopeState, voiceCount> envelopeState {};
    std::array<uint32_t, voiceCount> noiseLfsr {};
    std::array<float, voiceCount> voiceVelocity {};
    std::array<int, voiceCount> channelNotes { -1, -1, -1 };
    std::array<uint64_t, voiceCount> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    double filterLowpass = 0.0;
    double filterBandpass = 0.0;
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
        dmcActive = false;
        dmcSampleCompleted = false;
        dmcByteIndex = 0;
        dmcBitIndex = 0;
        dmcPhase = 0.0;
        dmcBitsPlayed = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        const auto previousPatch = patch;
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
        if (patch.nesDmcLoop)
            regs[0x10] = static_cast<uint8_t>(regs[0x10] | 0x40u);
        else
            regs[0x10] = static_cast<uint8_t>(regs[0x10] & ~0x40u);
        regs[0x11] = nesDmcDirectLevelForControl(patch.nesDmcDirectLevel);

        if (newPatch.playMode == previousPatch.playMode
            && newPatch.sourceEnabled == previousPatch.sourceEnabled
            && (newPatch.control1 != previousPatch.control1 || newPatch.pulse2Duty != previousPatch.pulse2Duty))
        {
            refreshActivePulseDuties();
        }
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        dmcSample = std::move(data);
        dmcActive = false;
        dmcSampleCompleted = false;
        dmcByteIndex = 0;
        dmcBitIndex = 0;
        dmcPhase = 0.0;
        dmcBitsPlayed = 0;
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
            writeStatusRegister(value, true);
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
        if (! suppressDmcRestartOnNoteOn)
            startDmcSample();
        const auto duty = nesPulseDutyFromControl(patch.control1);
        const auto pulse2Duty = nesPulse2DutyForPatch(patch, duty, true);

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

        if (patch.nesDmcOnly)
        {
            enable = dmcSample.empty() ? 0u : 0x10u;
            noiseVol = 0u;
        }
        else
        {
            const auto requestedSources = enabledSourceMaskOrRecipe(patch, static_cast<uint8_t>(enable & 0x0fu)) & 0x0fu;
            enable = requestedSources;
            if ((enable & 0x01u) != 0u)
                p1Vol = ensureNesRegisterVolume(p1Vol, 8u);
            if ((enable & 0x02u) != 0u)
                p2Vol = ensureNesRegisterVolume(p2Vol, std::max(5u, p1Vol));
            if ((enable & 0x08u) != 0u)
                noiseVol = ensureNesRegisterVolume(noiseVol, static_cast<unsigned>(std::max<int>(8, std::round(patch.control3 * 12.0f))));
            if (! dmcSample.empty() && sourceEnabled(patch, 3))
                enable |= 0x10u;
        }

        writePulseRegisters(0x4000, duty, p1Vol, p1Note);
        writePulseRegisters(0x4004, pulse2Duty, p2Vol, p2Note);

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
        regs[0x10] = static_cast<uint8_t>((regs[0x10] & 0xf0u) | static_cast<uint8_t>(patch.nesDmcRateIndex & 0x0f));
        writeStatusRegister(static_cast<uint8_t>(enable), ! suppressDmcRestartOnNoteOn);
        updateTimers();
    }

    void replayHeldNote(int midiNote, float velocity) override
    {
        const auto previous = suppressDmcRestartOnNoteOn;
        suppressDmcRestartOnNoteOn = true;
        noteOn(midiNote, velocity);
        suppressDmcRestartOnNoteOn = previous;
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
        tickDmcSample();

        const auto p1 = channelActive(0) ? renderPulse(0) * sourceLevel(patch, 0) : 0.0;
        const auto p2 = channelActive(1) ? renderPulse(1) * sourceLevel(patch, 1) : 0.0;
        const auto tri = triangleActive() ? renderTriangle() * sourceLevel(patch, 2) : 0.0;
        const auto noi = channelActive(3) ? renderNoise() * sourceLevel(patch, 3) : 0.0;
        const auto dmc = static_cast<double>(dmcMixerLevel());

        const auto pulseSum = p1 + p2;
        const auto tndSum = tri / 8227.0 + noi / 12241.0 + dmc / 22638.0;
        const auto pulseOut = pulseSum <= 0.0 ? 0.0 : 95.88 / ((8128.0 / pulseSum) + 100.0);
        const auto tndOut = tndSum <= 0.0 ? 0.0 : 159.79 / ((1.0 / tndSum) + 100.0);
        const auto mixed = static_cast<float>(applyOutputFilters(pulseOut + tndOut) * 2.0);
        const auto outputGate = (dmc > 0.0 && noteVelocity <= 0.0f) ? 1.0f : noteVelocity;
        const auto scaled = mixed * outputGate;
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
        return "Pulse, triangle, noise, timers, duty including explicit pulse 2 duty override, enable bits, simple envelopes, length counters, triangle linear counter, DMC direct DAC level, external DPCM byte stepping, basic pulse sweep updates/muting, $4017 frame-counter mode/inhibit behavior, nonlinear mixer, the documented NES output filter chain, and pulse/triangle allocation for Chip Poly play mode are approximated; exact DMC DMA/address/IRQ timing, exact frame sequencer cycle timing, advanced sweep edge cases, and hardware validation are not complete.";
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
             << "\"pulse2DutyChoice\":" << std::clamp(patch.pulse2Duty, 0, 4) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 2) << ","
             << "\"noiseRegister\":" << static_cast<int>(regs[0x0e]) << ","
             << "\"noiseShortMode\":" << (((regs[0x0e] & 0x80u) != 0) ? 1 : 0) << ","
             << "\"noisePeriod\":" << static_cast<int>(regs[0x0e] & 0x0fu) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 3) << ","
             << "\"dmcDirectControl\":" << patch.nesDmcDirectLevel << ","
             << "\"nesDmcOnly\":" << (patch.nesDmcOnly ? 1 : 0) << ","
             << "\"dmcSampleLoaded\":" << (dmcSample.empty() ? 0 : 1) << ","
             << "\"dmcSampleBytes\":" << dmcSample.size() << ","
             << "\"dmcSampleActive\":" << (dmcActive ? 1 : 0) << ","
             << "\"dmcSampleCompleted\":" << (dmcSampleCompleted ? 1 : 0) << ","
             << "\"dmcSampleByteIndex\":" << dmcByteIndex << ","
             << "\"dmcSampleBitIndex\":" << static_cast<int>(dmcBitIndex) << ","
             << "\"dmcSampleBitsPlayed\":" << dmcBitsPlayed << ","
             << "\"dmcRateIndex\":" << static_cast<int>(dmcRateIndex()) << ","
             << "\"dmcRatePeriodCycles\":" << dmcRatePeriodCycles() << ","
             << "\"dmcLoopEnabled\":" << (dmcLoopEnabled() ? 1 : 0) << ","
             << "\"dmcControlRegister\":" << static_cast<int>(regs[0x10]) << ","
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
             << "\"dmcMixerLevel\":" << static_cast<int>(dmcMixerLevel()) << ","
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

    uint8_t dmcMixerLevel() const
    {
        // The DMC DAC holds its last output after a one-shot sample finishes.
        // Loop off stops byte stepping; it should not replace the held DAC with
        // the separate DMC Direct macro level.
        return dmcOutputLevel();
    }

    uint8_t dmcRateIndex() const
    {
        return static_cast<uint8_t>(regs[0x10] & 0x0fu);
    }

    int dmcRatePeriodCycles() const
    {
        static constexpr std::array<int, 16> ntscPeriods {
            428, 380, 340, 320, 286, 254, 226, 214,
            190, 160, 142, 128, 106, 85, 72, 54
        };
        return ntscPeriods[dmcRateIndex()];
    }

    bool dmcLoopEnabled() const
    {
        return (regs[0x10] & 0x40u) != 0;
    }

    void writeStatusRegister(uint8_t value, bool allowDmcStart)
    {
        regs[0x15] = value;
        enabled[0] = (value & 0x01) != 0;
        enabled[1] = (value & 0x02) != 0;
        enabled[2] = (value & 0x04) != 0;
        enabled[3] = (value & 0x08) != 0;
        if ((value & 0x10u) != 0)
        {
            if (allowDmcStart)
                startDmcSample();
        }
        else
        {
            stopDmcSample();
        }
        for (size_t i = 0; i < enabled.size(); ++i)
        {
            if (! enabled[i])
                lengthCounter[i] = 0;
        }
    }

    void startDmcSample()
    {
        if (dmcSample.empty() || dmcActive)
            return;

        dmcActive = true;
        dmcSampleCompleted = false;
        regs[0x15] = static_cast<uint8_t>(regs[0x15] | 0x10u);
        dmcByteIndex = 0;
        dmcBitIndex = 0;
        dmcPhase = 0.0;
        dmcBitsPlayed = 0;
    }

    void tickDmcSample()
    {
        if (! dmcActive || dmcSample.empty() || sampleRate <= 0.0)
            return;

        dmcPhase += clock / (static_cast<double>(dmcRatePeriodCycles()) * sampleRate);
        while (dmcPhase >= 1.0 && dmcActive)
        {
            dmcPhase -= 1.0;
            stepDmcBit();
        }
    }

    void stepDmcBit()
    {
        if (dmcByteIndex >= dmcSample.size())
        {
            if (dmcLoopEnabled())
            {
                dmcByteIndex = 0;
                dmcBitIndex = 0;
            }
            else
            {
                stopDmcSample(true);
                return;
            }
        }

        const auto byte = dmcSample[dmcByteIndex];
        const auto bit = (byte >> dmcBitIndex) & 0x01u;
        auto level = dmcOutputLevel();
        if (bit != 0)
        {
            if (level <= 125u)
                level = static_cast<uint8_t>(level + 2u);
        }
        else if (level >= 2u)
        {
            level = static_cast<uint8_t>(level - 2u);
        }
        regs[0x11] = level;
        ++dmcBitsPlayed;

        ++dmcBitIndex;
        if (dmcBitIndex >= 8u)
        {
            dmcBitIndex = 0;
            ++dmcByteIndex;
            if (dmcByteIndex >= dmcSample.size())
            {
                if (dmcLoopEnabled())
                {
                    dmcByteIndex = 0;
                }
                else
                {
                    stopDmcSample(true);
                }
            }
        }
    }

    void stopDmcSample(bool completed = false)
    {
        dmcActive = false;
        dmcSampleCompleted = completed;
        regs[0x15] = static_cast<uint8_t>(regs[0x15] & ~0x10u);
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

    void writePulseDutyRegister(uint16_t baseAddress, NesPulseDuty duty)
    {
        const auto regIndex = static_cast<size_t>(baseAddress - 0x4000);
        if (regIndex >= regs.size())
            return;

        const auto flagsAndVolume = static_cast<uint8_t>(regs[regIndex] & 0x3fu);
        writeRegister(baseAddress, static_cast<uint8_t>((nesPulseDutyBits(duty) << 6u) | flagsAndVolume));
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

        auto enable = static_cast<uint8_t>(sourceEnableMask(patch) & 0x0fu);
        if (! dmcSample.empty() && sourceEnabled(patch, 3))
            enable = static_cast<uint8_t>(enable | 0x10u);
        writeRegister(0x4015, enable);

        if (channel == 0)
        {
            const auto duty = nesPulseDutyFromControl(patch.control1);
            const auto volume = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
            writeRegister(0x4001, 0x08);
            writePulseRegisters(0x4000, duty, volume, channelNotes[index]);
        }
        else if (channel == 1)
        {
            const auto duty = nesPulse2DutyForPatch(patch, nesPulseDutyFromControl(patch.control1), false);
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

    void refreshActivePulseDuties()
    {
        const auto pulse1Duty = nesPulseDutyFromControl(patch.control1);

        if (patch.playMode == PlayMode::chipPoly)
        {
            if (channelNotes[0] >= 0)
                writePulseDutyRegister(0x4000, pulse1Duty);
            if (channelNotes[1] >= 0)
                writePulseDutyRegister(0x4004, nesPulse2DutyForPatch(patch, pulse1Duty, false));
            return;
        }

        if (heldNote < 0)
            return;

        if (enabled[0])
            writePulseDutyRegister(0x4000, pulse1Duty);
        if (enabled[1])
            writePulseDutyRegister(0x4004, nesPulse2DutyForPatch(patch, pulse1Duty, true));
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
    std::vector<uint8_t> dmcSample;
    bool dmcActive = false;
    bool dmcSampleCompleted = false;
    bool suppressDmcRestartOnNoteOn = false;
    size_t dmcByteIndex = 0;
    uint8_t dmcBitIndex = 0;
    double dmcPhase = 0.0;
    uint64_t dmcBitsPlayed = 0;
    PatchConfig patch;
};

class Ym2149Core final : public ChipCore
{
public:
    explicit Ym2149Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    ~Ym2149Core() override
    {
        if (emu != nullptr)
            PSG_delete(emu);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 2000000.0;
        resetEmu();
        regs.fill(0);
        envelopeStepPhase = 0.0;
        envelopeCounter = 0;
        envelopeDirection = 1;
        envelopeHolding = false;
        envelopeResetCount = 0;
        envelopeVolume = 0.0;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        syncEmuRegisters();
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
        syncEmuRegisters();
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<size_t>(address & 0x0f);
        regs[reg] = value;
        if (reg == 13)
            resetEnvelopeCounter();
        writeEmuRegister(static_cast<uint8_t>(reg), value);
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
        const auto sourceAwareMixer = applySourceMixerMask(ym2149MixerRegisterWithChannelOverrides(patch, static_cast<uint8_t>(mixer)));
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
        updateEnvelope();
        if (emu != nullptr)
            static_cast<void>(PSG_calc(emu));

        static constexpr std::array<double, 3> panPositions { -1.0, 0.0, 1.0 };
        double left = 0.0;
        double right = 0.0;
        for (int ch = 0; ch < 3; ++ch)
        {
            const auto channel = renderChannel(ch);
            const auto gains = modernStereoGains(patch, panPositions[static_cast<size_t>(ch)]);
            left += channel * gains.left;
            right += channel * gains.right;
        }

        const auto scale = static_cast<double>(noteVelocity) * 0.7 / 3.0;
        return { static_cast<float>(left * scale), static_cast<float>(right * scale) };
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
    std::string implementedAccuracy() const override { return "partial emu2149-backed register-level"; }
    std::string limitations() const override
    {
        return "MIT emu2149 is used for YM2149/AY tone/noise/envelope generation while Chipper maps musical controls to PSG register writes; per-channel source trims, modern stereo spread, macro templates, and Chip Poly allocation remain Chipper-side instrument conveniences. Exact analog output curve, AY/YM variant differences, external golden comparison, and hardware timing validation are still required.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2149 / AY-3-8910\","
             << "\"implementedAccuracy\":\"partial emu2149-backed register-level\","
             << "\"core\":\"emu2149\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"periodA\":" << tonePeriod(0) << ","
             << "\"periodB\":" << tonePeriod(1) << ","
             << "\"periodC\":" << tonePeriod(2) << ","
             << "\"mixer\":" << static_cast<int>(regs[7]) << ","
             << "\"channelMixChoiceA\":" << ym2149ChannelMixChoiceForPatch(patch, 0) << ","
             << "\"channelMixChoiceB\":" << ym2149ChannelMixChoiceForPatch(patch, 1) << ","
             << "\"channelMixChoiceC\":" << ym2149ChannelMixChoiceForPatch(patch, 2) << ","
             << "\"toneEnabledA\":" << (((regs[7] & 0x01u) == 0) ? 1 : 0) << ","
             << "\"toneEnabledB\":" << (((regs[7] & 0x02u) == 0) ? 1 : 0) << ","
             << "\"toneEnabledC\":" << (((regs[7] & 0x04u) == 0) ? 1 : 0) << ","
             << "\"noiseEnabledA\":" << (((regs[7] & 0x08u) == 0) ? 1 : 0) << ","
             << "\"noiseEnabledB\":" << (((regs[7] & 0x10u) == 0) ? 1 : 0) << ","
             << "\"noiseEnabledC\":" << (((regs[7] & 0x20u) == 0) ? 1 : 0) << ","
             << "\"noisePeriod\":" << static_cast<int>(regs[6] & 0x1f) << ","
             << "\"sourceEnabledA\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabledB\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabledC\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabledNoise\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevelA\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevelB\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevelC\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevelNoise\":" << sourceLevel(patch, 3) << ","
             << "\"stereoSpread\":" << clamp01(patch.stereoSpread) << ","
             << "\"stereoPanA\":" << -clamp01(patch.stereoSpread) << ","
             << "\"stereoPanB\":0,"
             << "\"stereoPanC\":" << clamp01(patch.stereoSpread) << ","
             << "\"volumeCurve\":\"ayYmLogApprox1_5dB\","
             << "\"volumeA\":" << static_cast<int>(regs[8]) << ","
             << "\"volumeB\":" << static_cast<int>(regs[9]) << ","
             << "\"volumeC\":" << static_cast<int>(regs[10]) << ","
             << "\"linearVolumeA\":" << channelVolume(0) << ","
             << "\"linearVolumeB\":" << channelVolume(1) << ","
             << "\"linearVolumeC\":" << channelVolume(2) << ","
             << "\"envelopeDecayControl\":" << patch.envelopeDecay << ","
             << "\"envelopePeriod\":" << envelopePeriodRegister() << ","
             << "\"envelopeShapeChoice\":" << patch.ymEnvelopeShape << ","
             << "\"envelopeShape\":" << static_cast<int>(regs[13] & 0x0f) << ","
             << "\"envelopeStepResolution\":" << envelopeStepResolution << ","
             << "\"envelopeStepRateHz\":" << envelopeStepRateHz() << ","
             << "\"envelopeCounter\":" << envelopeCounter << ","
             << "\"envelopeLinearLevel\":" << envelopeVolume << ","
             << "\"envelopeDirection\":" << envelopeDirection << ","
             << "\"envelopeHolding\":" << (envelopeHolding ? 1 : 0) << ","
             << "\"envelopeResetCount\":" << envelopeResetCount << ","
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

        return ym2149EnvelopeShapeCodeForChoice(patch.ymEnvelopeShape);
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

    void resetEmu()
    {
        if (emu != nullptr)
            PSG_delete(emu);

        emu = PSG_new(static_cast<uint32_t>(std::max(1.0, std::round(clock))),
                      static_cast<uint32_t>(std::max(1.0, std::round(sampleRate))));
        if (emu != nullptr)
        {
            PSG_reset(emu);
            PSG_setQuality(emu, 0);
            PSG_setVolumeMode(emu, 1);
        }
    }

    void writeEmuRegister(uint8_t reg, uint8_t value)
    {
        if (emu != nullptr)
            PSG_writeReg(emu, reg, value);
    }

    void syncEmuRegisters()
    {
        if (emu == nullptr)
            return;

        for (uint8_t reg = 0; reg < regs.size(); ++reg)
            writeEmuRegister(reg, regs[reg]);
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
        writeRegister(7, applySourceMixerMask(ym2149MixerRegisterWithChannelOverrides(patch, mixer)));
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
        return fixedVolumeToLinear(volReg);
    }

    static double logarithmicLevelToLinear(int level, int maxLevel)
    {
        if (level <= 0 || maxLevel <= 0)
            return 0.0;

        constexpr double dbPerStep = 1.5;
        const auto clipped = std::clamp(level, 0, maxLevel);
        const auto stepsBelowMax = maxLevel - clipped;
        return std::pow(10.0, (-dbPerStep * static_cast<double>(stepsBelowMax)) / 20.0);
    }

    static double fixedVolumeToLinear(uint8_t volume)
    {
        return logarithmicLevelToLinear(static_cast<int>(volume & 0x0f), 15);
    }

    static double envelopeCounterToLinear(int counter)
    {
        return logarithmicLevelToLinear(counter, envelopeStepResolution - 1);
    }

    double envelopeStepRateHz() const
    {
        const auto period = std::max(1, static_cast<int>(envelopePeriodRegister()));
        return clock / (8.0 * static_cast<double>(period));
    }

    void resetEnvelopeCounter()
    {
        const auto attack = (regs[13] & 0x04u) != 0;
        envelopeStepPhase = 0.0;
        envelopeDirection = attack ? 1 : -1;
        envelopeCounter = attack ? 0 : envelopeStepResolution - 1;
        envelopeHolding = false;
        envelopeVolume = envelopeCounterToLinear(envelopeCounter);
        ++envelopeResetCount;
    }

    void advanceEnvelopeCounter()
    {
        if (envelopeHolding)
            return;

        envelopeCounter += envelopeDirection;
        if (envelopeCounter >= 0 && envelopeCounter < envelopeStepResolution)
            return;

        const auto shape = regs[13] & 0x0f;
        const auto cont = (shape & 0x08u) != 0;
        const auto attack = (shape & 0x04u) != 0;
        const auto alternate = (shape & 0x02u) != 0;
        const auto hold = (shape & 0x01u) != 0;

        if (! cont)
        {
            envelopeCounter = 0;
            envelopeHolding = true;
            return;
        }

        if (hold)
        {
            envelopeCounter = alternate ? (attack ? 0 : envelopeStepResolution - 1)
                                        : (envelopeDirection > 0 ? envelopeStepResolution - 1 : 0);
            envelopeHolding = true;
            return;
        }

        if (alternate)
            envelopeDirection = -envelopeDirection;

        envelopeCounter = envelopeDirection > 0 ? 0 : envelopeStepResolution - 1;
    }

    void updateEnvelope()
    {
        envelopeStepPhase += envelopeStepRateHz() / sampleRate;
        while (envelopeStepPhase >= 1.0)
        {
            envelopeStepPhase -= 1.0;
            advanceEnvelopeCounter();
        }

        envelopeVolume = envelopeCounterToLinear(envelopeCounter);
    }

    double renderChannel(int channel)
    {
        if (emu == nullptr || channel < 0 || channel >= 3)
            return 0.0;

        const auto toneDisabled = (regs[7] & (1u << channel)) != 0;
        const auto noiseDisabled = (regs[7] & (1u << (channel + 3))) != 0;
        if (toneDisabled && noiseDisabled)
            return 0.0;

        const auto channelTrim = sourceLevel(patch, static_cast<size_t>(channel));
        const auto noiseTrim = sourceLevel(patch, 3);
        auto trim = channelTrim;
        if (toneDisabled && ! noiseDisabled)
            trim *= noiseTrim;
        else if (! toneDisabled && ! noiseDisabled)
            trim *= std::max(0.0, 0.5 + (noiseTrim * 0.5));

        const auto raw = static_cast<double>(emu->ch_out[static_cast<size_t>(channel)]) / 2048.0;
        const auto centered = raw - channelVolume(channel);
        return centered * trim;
    }

    AccuracyMode accuracy;
    static constexpr int envelopeStepResolution = 32;
    double sampleRate = 48000.0;
    double clock = 2000000.0;
    PSG* emu = nullptr;
    std::array<uint8_t, 16> regs {};
    double envelopeStepPhase = 0.0;
    double envelopeVolume = 0.0;
    int envelopeCounter = 0;
    int envelopeDirection = 1;
    int envelopeResetCount = 0;
    bool envelopeHolding = false;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 3> channelNotes { -1, -1, -1 };
    std::array<float, 3> channelVelocity {};
    std::array<uint64_t, 3> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class PokeyCore final : public ChipCore
{
public:
    explicit PokeyCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 1789790.0;
        audf.fill(0xff);
        audc.fill(0x00);
        phase.fill(0.0);
        highPassHold.fill(0.0);
        highPassClockLast.fill(1.0);
        audctl = 0x00;
        poly4 = 0x0fu;
        poly5 = 0x1fu;
        poly17 = 0x1ffffu;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled
            || pokeyAudctlForPatch(newPatch) != pokeyAudctlForPatch(patch))
            clearChipPolyState();

        patch = newPatch;
        audctl = pokeyAudctlForPatch(patch);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto index = static_cast<uint8_t>(address & 0x0fu);
        switch (index)
        {
            case 0x00: audf[0] = value; break;
            case 0x01: audc[0] = value; break;
            case 0x02: audf[1] = value; break;
            case 0x03: audc[1] = value; break;
            case 0x04: audf[2] = value; break;
            case 0x05: audc[2] = value; break;
            case 0x06: audf[3] = value; break;
            case 0x07: audc[3] = value; break;
            case 0x08: audctl = value; break;
            default: break;
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        std::array<int, 4> notes {
            heldNote,
            heldNote + std::max(1, spread / 3),
            heldNote + std::max(2, (spread * 2) / 3),
            heldNote + std::max(3, spread)
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote };
                break;
            case MacroKind::drum:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7 };
                break;
            case MacroKind::laser:
                notes = { heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote, heldNote - 12, heldNote - 19 };
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12 };
                break;
            case MacroKind::lead:
            case MacroKind::hit:
            case MacroKind::manual:
            default:
                break;
        }

        const auto control = pokeyAudcForPatch(patch);
        audctl = pokeyAudctlForPatch(patch);
        audc.fill(0x00u);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

            writePitchToChannel(channel, notes[channel]);
            audc[channel] = sourceEnabled(patch, channel) ? control : 0x00u;
        }
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
        static constexpr std::array<double, 4> panPositions { -1.0, -0.35, 0.35, 1.0 };
        std::array<double, 4> channelSamples {};
        double left = 0.0;
        double right = 0.0;

        for (size_t channel = 0; channel < 4; ++channel)
            channelSamples[channel] = renderChannel(channel) * sourceLevel(patch, channel);

        applyHighPassFilters(channelSamples);

        for (size_t channel = 0; channel < 4; ++channel)
        {
            const auto gains = modernStereoGains(patch, panPositions[channel]);
            left += channelSamples[channel] * gains.left;
            right += channelSamples[channel] * gains.right;
        }

        const auto scale = static_cast<double>(noteVelocity) * 0.85 / 4.0;
        return { static_cast<float>(left * scale), static_cast<float>(right * scale) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(9);
        for (uint8_t channel = 0; channel < 4; ++channel)
        {
            writes.push_back({ 0, static_cast<uint16_t>(channel * 2u), audf[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(channel * 2u + 1u), audc[channel] });
        }
        writes.push_back({ 0, 0x08, audctl });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::pokey; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "Atari POKEY"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Four AUDF/AUDC/AUDV-style channels, note-to-timer writes, source gating, chip-poly allocation, AUDCTL 16-bit channel-pair bits, simplified high-pass filter switches, and simplified pure/poly4/poly5/poly17 texture paths are modeled; 1.79 MHz direct clocks, exact polynomial taps, serial behavior, DAC curve, exact paired-channel carry timing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Atari POKEY\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"audf0\":" << static_cast<int>(audf[0]) << ","
             << "\"audf1\":" << static_cast<int>(audf[1]) << ","
             << "\"audf2\":" << static_cast<int>(audf[2]) << ","
             << "\"audf3\":" << static_cast<int>(audf[3]) << ","
             << "\"audc0\":" << static_cast<int>(audc[0]) << ","
             << "\"audc1\":" << static_cast<int>(audc[1]) << ","
             << "\"audc2\":" << static_cast<int>(audc[2]) << ","
             << "\"audc3\":" << static_cast<int>(audc[3]) << ","
             << "\"audctl\":" << static_cast<int>(audctl) << ","
             << "\"pair12\":" << (pair12Enabled() ? 1 : 0) << ","
             << "\"pair34\":" << (pair34Enabled() ? 1 : 0) << ","
             << "\"filterChoice\":" << static_cast<int>(pokeyFilterChoiceForPatch(patch)) << ","
             << "\"filterBits\":" << static_cast<int>(pokeyFilterBitsForPatch(patch)) << ","
             << "\"filterCh1ByCh3\":" << (highPass13Enabled() ? 1 : 0) << ","
             << "\"filterCh2ByCh4\":" << (highPass24Enabled() ? 1 : 0) << ","
             << "\"audfPair12\":" << pairedAudf(0) << ","
             << "\"audfPair34\":" << pairedAudf(2) << ","
             << "\"distortionChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"distortionCode\":" << static_cast<int>(pokeyAudcForPatch(patch) & 0xf0u) << ","
             << "\"volumeNibble\":" << static_cast<int>(pokeyAudcForPatch(patch) & 0x0fu) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"stereoSpread\":" << clamp01(patch.stereoSpread) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    static double volumeToLinear(uint8_t control)
    {
        return static_cast<double>(control & 0x0fu) / 15.0;
    }

    double channelClockHz(size_t channel) const
    {
        const auto period = static_cast<double>(channelIsPairedLowByte(channel) ? pairedAudf(channel) : audf[channel]) + 1.0;
        return clock / (56.0 * period);
    }

    double renderChannel(size_t channel)
    {
        if (channelIsPairedHighByte(channel))
            return 0.0;

        const auto drivesHighPass = (channel == 2u && highPass13Enabled()) || (channel == 3u && highPass24Enabled());
        if (! sourceEnabled(patch, channel) && ! drivesHighPass)
            return 0.0;

        const auto raw = advanceChannel(channel);

        if (! sourceEnabled(patch, channel))
            return 0.0;

        const auto volume = volumeToLinear(audc[channel]);
        if (volume <= 0.0)
            return 0.0;

        return raw * volume;
    }

    double advanceChannel(size_t channel)
    {
        const auto hz = channelClockHz(channel);
        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);
        if (phase[channel] < 0.5)
            return lastOutput[channel];

        phase[channel] -= 0.5;
        const auto distortion = audc[channel] & 0xf0u;
        if (distortion == 0x20u)
            clockPoly4(channel);
        else if (distortion == 0x40u)
            clockPoly5(channel);
        else if (distortion == 0x80u)
            clockPoly17(channel);
        else
            lastOutput[channel] = -lastOutput[channel];

        return lastOutput[channel];
    }

    bool pair12Enabled() const { return (audctl & 0x10u) != 0; }
    bool pair34Enabled() const { return (audctl & 0x08u) != 0; }
    bool highPass13Enabled() const { return (audctl & 0x04u) != 0; }
    bool highPass24Enabled() const { return (audctl & 0x02u) != 0; }

    void applyHighPassFilters(std::array<double, 4>& samples)
    {
        if (highPass13Enabled())
            samples[0] = applyHighPassPath(samples[0], 0, 2);
        else
            highPassHold[0] = 0.0;

        if (highPass24Enabled())
            samples[1] = applyHighPassPath(samples[1], 1, 3);
        else
            highPassHold[1] = 0.0;

        highPassClockLast[0] = lastOutput[2];
        highPassClockLast[1] = lastOutput[3];
    }

    double applyHighPassPath(double input, size_t outputIndex, size_t clockChannel)
    {
        const auto slot = outputIndex == 0u ? 0u : 1u;
        const auto output = input - highPassHold[slot];
        if (lastOutput[clockChannel] != highPassClockLast[slot])
            highPassHold[slot] = input;
        return output;
    }

    bool channelIsPairedHighByte(size_t channel) const
    {
        return (channel == 1u && pair12Enabled()) || (channel == 3u && pair34Enabled());
    }

    bool channelIsPairedLowByte(size_t channel) const
    {
        return (channel == 0u && pair12Enabled()) || (channel == 2u && pair34Enabled());
    }

    uint16_t pairedAudf(size_t lowChannel) const
    {
        if (lowChannel + 1u >= audf.size())
            return audf[lowChannel];

        return static_cast<uint16_t>(audf[lowChannel] | (static_cast<uint16_t>(audf[lowChannel + 1u]) << 8u));
    }

    uint16_t audf16ForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto divisor = std::round(clock / (56.0 * hz) - 1.0);
        return static_cast<uint16_t>(std::clamp(static_cast<int>(divisor), 0, 65535));
    }

    void writePitchToChannel(size_t channel, int midiNote)
    {
        if (channelIsPairedLowByte(channel))
        {
            const auto divisor = audf16ForNote(midiNote);
            audf[channel] = static_cast<uint8_t>(divisor & 0xffu);
            audf[channel + 1u] = static_cast<uint8_t>((divisor >> 8u) & 0xffu);
            return;
        }

        audf[channel] = pokeyAudfForNote(clock, midiNote);
    }

    void clockPoly4(size_t channel)
    {
        const auto feedback = ((poly4 & 1u) ^ ((poly4 >> 1u) & 1u)) & 1u;
        poly4 = static_cast<uint8_t>(((poly4 >> 1u) | (feedback << 3u)) & 0x0fu);
        if (poly4 == 0)
            poly4 = 0x0fu;
        lastOutput[channel] = (poly4 & 1u) != 0 ? 1.0 : -1.0;
    }

    void clockPoly5(size_t channel)
    {
        const auto feedback = ((poly5 & 1u) ^ ((poly5 >> 2u) & 1u)) & 1u;
        poly5 = static_cast<uint8_t>(((poly5 >> 1u) | (feedback << 4u)) & 0x1fu);
        if (poly5 == 0)
            poly5 = 0x1fu;
        lastOutput[channel] = (poly5 & 1u) != 0 ? 1.0 : -1.0;
    }

    void clockPoly17(size_t channel)
    {
        const auto feedback = ((poly17 & 1u) ^ ((poly17 >> 5u) & 1u)) & 1u;
        poly17 = ((poly17 >> 1u) | (feedback << 16u)) & 0x1ffffu;
        if (poly17 == 0)
            poly17 = 0x1ffffu;
        lastOutput[channel] = (poly17 & 1u) != 0 ? 1.0 : -1.0;
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

            if (sourceEnabled(patch, channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

            if (sourceEnabled(patch, channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelStamp.size(); ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

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
            if (channelIsPairedHighByte(channel))
                continue;

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
        audc.fill(0x00);
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        const auto index = static_cast<size_t>(channel);
        audctl = pokeyAudctlForPatch(patch);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;
        writePitchToChannel(index, channelNotes[index]);
        const auto baseControl = pokeyAudcForPatch(patch) & 0xf0u;
        const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
        audc[index] = static_cast<uint8_t>(baseControl | volume);
        if (channelIsPairedLowByte(index))
            audc[index + 1u] = 0x00u;
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
            audc[channel] = 0x00;
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 1789790.0;
    std::array<uint8_t, 4> audf {};
    std::array<uint8_t, 4> audc {};
    uint8_t audctl = 0x00;
    std::array<double, 4> phase {};
    std::array<double, 4> lastOutput { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, 2> highPassHold {};
    std::array<double, 2> highPassClockLast { 1.0, 1.0 };
    uint8_t poly4 = 0x0f;
    uint8_t poly5 = 0x1f;
    uint32_t poly17 = 0x1ffff;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 4> channelNotes {};
    std::array<float, 4> channelVelocity {};
    std::array<uint64_t, 4> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class Sn76489Core final : public ChipCore
{
public:
    explicit Sn76489Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    ~Sn76489Core() override
    {
        if (emu != nullptr)
            SNG_delete(emu);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        resetEmu();
        tonePeriod = { 0x3ff, 0x3ff, 0x3ff };
        attenuation = { 0x0f, 0x0f, 0x0f, 0x0f };
        latchedChannel = 0;
        latchedIsVolume = false;
        noiseControl = 0x03;
        lfsr = 0x8000;
        noiseRegisterResets = 0;
        volumeDataByteWrites = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        syncEmuRegisters();
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
        setNoiseControl(sn76489NoiseControlForPatch(patch), false);
        applySourceAttenuationMask();
        if (emu != nullptr)
            syncEmuRegisters();
    }

    void writeRegister(uint16_t, uint8_t value) override
    {
        if ((value & 0x80) != 0)
        {
            latchedChannel = (value >> 5u) & 0x03u;
            latchedIsVolume = (value & 0x10u) != 0;
            const auto data = value & 0x0fu;
            if (latchedIsVolume)
            {
                attenuation[latchedChannel] = data;
                writeEmuVolumeLatch(latchedChannel);
            }
            else if (latchedChannel == 3)
            {
                setNoiseControl(data, true);
            }
            else
            {
                tonePeriod[latchedChannel] = static_cast<uint16_t>((tonePeriod[latchedChannel] & 0x3f0u) | data);
                writeEmuTone(latchedChannel);
            }
        }
        else if (latchedIsVolume)
        {
            attenuation[latchedChannel] = value & 0x0fu;
            ++volumeDataByteWrites;
            writeEmuVolumeLatch(latchedChannel);
        }
        else if (latchedChannel < 3)
        {
            tonePeriod[latchedChannel] = static_cast<uint16_t>(((value & 0x3fu) << 4u) | (tonePeriod[latchedChannel] & 0x0fu));
            writeEmuTone(latchedChannel);
        }
        else
        {
            setNoiseControl(value, true);
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
        setNoiseControl(sn76489NoiseControlForPatch(patch), false);
        applySourceAttenuationMask();
        syncEmuRegisters();
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
        static constexpr std::array<double, 4> panPositions { -1.0, 0.0, 1.0, 0.0 };
        if (emu != nullptr)
            static_cast<void>(SNG_calc(emu));

        double left = 0.0;
        double right = 0.0;
        for (int ch = 0; ch < 3; ++ch)
        {
            const auto channel = renderEmuChannel(ch) * sourceLevel(patch, static_cast<size_t>(ch));
            const auto gains = modernStereoGains(patch, panPositions[static_cast<size_t>(ch)]);
            left += channel * gains.left;
            right += channel * gains.right;
        }

        const auto noise = renderEmuChannel(3) * sourceLevel(patch, 3);
        const auto noiseGains = modernStereoGains(patch, panPositions[3]);
        left += noise * noiseGains.left;
        right += noise * noiseGains.right;

        const auto scale = static_cast<double>(noteVelocity) * 1.6 / 4.0;
        return { static_cast<float>(left * scale), static_cast<float>(right * scale) };
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
    std::string implementedAccuracy() const override { return "partial emu76489-backed register-level"; }
    std::string limitations() const override
    {
        return "MIT emu76489 is used for SN76489 tone/noise generation while Chipper maps musical controls to PSG latch/register writes; source trims, modern stereo spread, and Chip Poly allocation remain Chipper-side conveniences. Exact chip variant behavior, external golden comparison, and hardware-level output validation are still required.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SN76489 / Sega PSG\","
             << "\"implementedAccuracy\":\"partial emu76489-backed register-level\","
             << "\"core\":\"emu76489\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"period0\":" << tonePeriod[0] << ","
             << "\"period1\":" << tonePeriod[1] << ","
             << "\"period2\":" << tonePeriod[2] << ","
             << "\"toneConstant0\":" << (toneConstant(0) ? 1 : 0) << ","
             << "\"toneConstant1\":" << (toneConstant(1) ? 1 : 0) << ","
             << "\"toneConstant2\":" << (toneConstant(2) ? 1 : 0) << ","
             << "\"noiseControl\":" << static_cast<int>(noiseControl) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"noiseWhite\":" << (((noiseControl & 0x04u) != 0) ? 1 : 0) << ","
             << "\"noiseRate\":" << static_cast<int>(noiseControl & 0x03u) << ","
             << "\"noiseTone3Clocked\":" << (((noiseControl & 0x03u) == 3) ? 1 : 0) << ","
             << "\"noiseDivider\":" << noiseClockDivider() << ","
             << "\"noiseLfsr\":" << lfsr << ","
             << "\"noiseRegisterResets\":" << noiseRegisterResets << ","
             << "\"volumeDataByteWrites\":" << volumeDataByteWrites << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabledNoise\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevelNoise\":" << sourceLevel(patch, 3) << ","
             << "\"stereoSpread\":" << clamp01(patch.stereoSpread) << ","
             << "\"stereoPan0\":" << -clamp01(patch.stereoSpread) << ","
             << "\"stereoPan1\":0,"
             << "\"stereoPan2\":" << clamp01(patch.stereoSpread) << ","
             << "\"stereoPanNoise\":0,"
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

    void resetEmu()
    {
        if (emu != nullptr)
            SNG_delete(emu);

        emu = SNG_new(static_cast<uint32_t>(std::max(1.0, std::round(clock))),
                      static_cast<uint32_t>(std::max(1.0, std::round(sampleRate))));
        if (emu != nullptr)
        {
            SNG_reset(emu);
            SNG_set_quality(emu, 0);
        }
    }

    void writeEmuByte(uint8_t value)
    {
        if (emu != nullptr)
            SNG_writeIO(emu, value);
    }

    void writeEmuTone(uint8_t channel)
    {
        if (channel >= 3)
            return;

        const auto period = tonePeriod[static_cast<size_t>(channel)] & 0x03ffu;
        writeEmuByte(static_cast<uint8_t>(0x80u | (channel << 5u) | (period & 0x0fu)));
        writeEmuByte(static_cast<uint8_t>((period >> 4u) & 0x3fu));
    }

    void writeEmuVolumeLatch(uint8_t channel)
    {
        if (channel >= 4)
            return;

        writeEmuByte(static_cast<uint8_t>(0x90u | (channel << 5u) | (attenuation[static_cast<size_t>(channel)] & 0x0fu)));
    }

    void writeEmuNoiseLatch()
    {
        writeEmuByte(static_cast<uint8_t>(0xe0u | (noiseControl & 0x07u)));
    }

    void syncEmuRegisters()
    {
        if (emu == nullptr)
            return;

        for (uint8_t channel = 0; channel < 3; ++channel)
            writeEmuTone(channel);
        writeEmuNoiseLatch();
        for (uint8_t channel = 0; channel < 4; ++channel)
            writeEmuVolumeLatch(channel);
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
        syncEmuRegisters();
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
        syncEmuRegisters();
    }

    double renderEmuChannel(int channel) const
    {
        if (emu == nullptr || channel < 0 || channel >= 4)
            return 0.0;

        return static_cast<double>(emu->ch_out[static_cast<size_t>(channel)]) / 8192.0;
    }

    bool toneConstant(int channel) const
    {
        return tonePeriod[static_cast<size_t>(channel)] <= 1;
    }

    void setNoiseControl(uint8_t value, bool fromRegisterWrite)
    {
        noiseControl = value & 0x07u;
        lfsr = 0x8000;
        if (fromRegisterWrite)
            ++noiseRegisterResets;
        writeEmuNoiseLatch();
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
    SNG* emu = nullptr;
    std::array<uint16_t, 3> tonePeriod {};
    std::array<uint8_t, 4> attenuation {};
    uint8_t latchedChannel = 0;
    bool latchedIsVolume = false;
    uint8_t noiseControl = 0x03;
    uint16_t lfsr = 0x8000;
    uint32_t noiseRegisterResets = 0;
    uint32_t volumeDataByteWrites = 0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 3> channelNotes { -1, -1, -1 };
    std::array<float, 3> channelVelocity {};
    std::array<uint64_t, 3> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class Huc6280Core final : public ChipCore
{
public:
    explicit Huc6280Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        selectedChannel = 0;
        frequency.fill(0x0fff);
        control.fill(0x00);
        balance.fill(0xff);
        noiseControl.fill(0x00);
        phase.fill(0.0);
        noisePhase.fill(0.0);
        noiseLfsr.fill(0x1ffffu);
        waveWriteIndex.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        lfoPhase = 0.0;

        for (size_t channel = 0; channel < waveRam.size(); ++channel)
            seedWave(channel);
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        switch (address & 0x0fu)
        {
            case 0x00:
                selectedChannel = static_cast<uint8_t>(value % 6u);
                break;
            case 0x02:
                frequency[selectedChannel] = static_cast<uint16_t>((frequency[selectedChannel] & 0x0f00u) | value);
                break;
            case 0x03:
                frequency[selectedChannel] = static_cast<uint16_t>(((value & 0x0fu) << 8u) | (frequency[selectedChannel] & 0x00ffu));
                break;
            case 0x04:
                control[selectedChannel] = value;
                break;
            case 0x05:
                balance[selectedChannel] = value;
                break;
            case 0x06:
                writeWaveSample(selectedChannel, value);
                break;
            case 0x07:
                noiseControl[selectedChannel] = value;
                noiseLfsr[selectedChannel] = 0x1ffffu;
                break;
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

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 19.0f));
        std::array<int, 6> notes {
            heldNote,
            heldNote + std::max(1, spread / 3),
            heldNote + std::max(2, (spread * 2) / 3),
            heldNote + std::max(3, spread),
            heldNote - 12,
            heldNote + 12
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote - 17, heldNote + 7 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::laser:
                notes[0] = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                notes[1] = heldNote;
                notes[2] = heldNote - 12;
                notes[3] = heldNote - 19;
                break;
            case MacroKind::jump:
                notes[0] = heldNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                notes[1] = heldNote + 7;
                notes[2] = heldNote + 12;
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            frequency[channel] = hucPeriodForNote(notes[channel]);
            control[channel] = channelActiveForPatch(channel) ? channelControlForPatch(channel) : 0x00u;
            balance[channel] = 0xffu;
            seedWave(channel);
        }
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
        static constexpr std::array<double, 6> panPositions { -1.0, -0.6, -0.2, 0.2, 0.6, 1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            const auto sample = renderChannel(channel);
            if (sample == 0.0)
                continue;

            ++audibleCount;
            const auto gains = modernStereoGains(patch, panPositions[channel]);
            left += sample * gains.left;
            right += sample * gains.right;
        }

        const auto scale = static_cast<double>(noteVelocity) * 0.9 / static_cast<double>(std::max(1, audibleCount));
        return { static_cast<float>(left * scale), static_cast<float>(right * scale) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(6 * 5);
        for (uint8_t channel = 0; channel < 6; ++channel)
        {
            writes.push_back({ 0, 0x00, channel });
            writes.push_back({ 0, 0x02, static_cast<uint8_t>(frequency[channel] & 0xffu) });
            writes.push_back({ 0, 0x03, static_cast<uint8_t>((frequency[channel] >> 8u) & 0x0fu) });
            writes.push_back({ 0, 0x04, control[channel] });
            writes.push_back({ 0, 0x05, balance[channel] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::huc6280; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "PC Engine HuC6280"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Six channel-select, frequency, control, balance, waveform-RAM, and noise-control register paths are modeled with wavetable playback, simplified LFSR noise, and a partial channel 2 to channel 1 FM-LFO pairing; exact LFO register timing/scaling, exact noise taps, DDA details, stereo register behavior, timer edge timing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"PC Engine HuC6280\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"selectedChannel\":" << static_cast<int>(selectedChannel) << ","
             << "\"waveShapeChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 0)) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 1)) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 2)) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 3)) << ","
             << "\"waveShapeChoice4\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 4)) << ","
             << "\"waveShapeChoice5\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 5)) << ","
             << "\"frequency0\":" << frequency[0] << ","
             << "\"frequency1\":" << frequency[1] << ","
             << "\"frequency2\":" << frequency[2] << ","
             << "\"frequency3\":" << frequency[3] << ","
             << "\"frequency4\":" << frequency[4] << ","
             << "\"frequency5\":" << frequency[5] << ","
             << "\"control0\":" << static_cast<int>(control[0]) << ","
             << "\"control1\":" << static_cast<int>(control[1]) << ","
             << "\"control2\":" << static_cast<int>(control[2]) << ","
             << "\"control3\":" << static_cast<int>(control[3]) << ","
             << "\"control4\":" << static_cast<int>(control[4]) << ","
             << "\"control5\":" << static_cast<int>(control[5]) << ","
             << "\"noiseControl0\":" << static_cast<int>(noiseControl[0]) << ","
             << "\"noiseControl5\":" << static_cast<int>(noiseControl[5]) << ","
             << "\"lfoChoice\":" << std::clamp(patch.dmgStereoRoute, 0, 4) << ","
             << "\"lfoMode\":" << static_cast<int>(huc6280LfoModeForPatch(patch)) << ","
             << "\"lfoEnabled\":" << (lfoEnabled() ? 1 : 0) << ","
             << "\"lfoSourceChannel\":1,"
             << "\"lfoTargetChannel\":0,"
             << "\"lfoDepthCents\":" << static_cast<int>(std::round(lfoDepthSemitones() * 100.0)) << ","
             << "\"lfoRateHz\":" << lfoRateHz() << ","
             << "\"waveRam0\":" << static_cast<int>(waveRam[0][0]) << ","
             << "\"waveRam31\":" << static_cast<int>(waveRam[0][31]) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"sourceLevel5\":" << sourceLevel(patch, 5) << ","
             << "\"uiExposesSixChannels\":1,"
             << "\"internalChannelCount\":6,"
             << "\"chipPolyPlayableChannels\":6,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t hucPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * hz)), 1.0, 4095.0));
    }

    uint8_t channelControlForPatch(size_t channel) const
    {
        const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control4 * 31.0f)), 1, 31));
        const auto useNoise = patch.macro == MacroKind::drum || patch.macro == MacroKind::hit || patch.waveShape == 4;
        if (useNoise && channel >= 4)
            return static_cast<uint8_t>(0x80u | volume);
        return static_cast<uint8_t>(0x80u | volume);
    }

    bool channelAudible(size_t channel) const
    {
        if (channel == 1u && lfoEnabled())
            return false;
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);
        return false;
    }

    bool chipPolyChannelEnabled(size_t channel) const
    {
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);
        return false;
    }

    bool channelActiveForPatch(size_t channel) const
    {
        if (! chipPolyChannelEnabled(channel))
            return false;

        if (channel < 4)
            return true;
        return patch.macro == MacroKind::arp
            || patch.macro == MacroKind::drum
            || patch.macro == MacroKind::hit
            || patch.macro == MacroKind::powerUp;
    }

    void seedWave(size_t channel)
    {
        const auto choice = static_cast<int>(huc6280WaveShapeForChannel(patch, channel));
        for (size_t i = 0; i < 32; ++i)
        {
            const auto phaseValue = static_cast<double>(i) / 32.0;
            auto sample = 0;
            switch (choice)
            {
                case 1: sample = static_cast<int>(std::round(31.0 * phaseValue)); break;
                case 2: sample = i < 16 ? static_cast<int>(std::round(31.0 * (static_cast<double>(i) / 15.0))) : static_cast<int>(std::round(31.0 * (1.0 - static_cast<double>(i - 16) / 15.0))); break;
                case 3: sample = (i < 16) ? 31 : 0; break;
                case 4: sample = ((i * 13 + static_cast<int>(channel) * 7) & 31); break;
                case 0:
                default: sample = static_cast<int>(std::round(15.5 + 15.5 * std::sin(twoPi * phaseValue))); break;
            }
            waveRam[channel][i] = static_cast<uint8_t>(std::clamp(sample, 0, 31));
        }
    }

    void writeWaveSample(size_t channel, uint8_t value)
    {
        waveRam[channel][waveWriteIndex[channel]] = value & 0x1fu;
        waveWriteIndex[channel] = static_cast<uint8_t>((waveWriteIndex[channel] + 1u) & 31u);
    }

    double channelVolume(size_t channel) const
    {
        if ((control[channel] & 0x80u) == 0)
            return 0.0;
        return static_cast<double>(control[channel] & 0x1fu) / 31.0;
    }

    double renderChannel(size_t channel)
    {
        if (channel == 1u && lfoEnabled())
        {
            advanceLfoSource();
            return 0.0;
        }

        if (! channelAudible(channel))
            return 0.0;

        const auto volume = channelVolume(channel) * sourceLevel(patch, channel);
        if (volume <= 0.0)
            return 0.0;

        const auto period = std::max<uint16_t>(1, frequency[channel]);
        auto hz = clock / (32.0 * static_cast<double>(period));
        if (channel == 0u && lfoEnabled())
        {
            const auto shape = currentLfoSample();
            hz *= std::pow(2.0, (shape * lfoDepthSemitones()) / 12.0);
        }
        const auto useNoise = (noiseControl[channel] & 0x80u) != 0 || huc6280ChannelUsesNoiseForPatch(patch, channel);
        if (useNoise)
            return renderNoise(channel, hz) * volume;

        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);
        const auto index = static_cast<size_t>(std::floor(phase[channel] * 32.0)) & 31u;
        return ((static_cast<double>(waveRam[channel][index]) / 31.0) * 2.0 - 1.0) * volume;
    }

    double renderNoise(size_t channel, double baseHz)
    {
        const auto rateShift = std::clamp(static_cast<int>(noiseControl[channel] & 0x1fu), 0, 31);
        const auto hz = std::max(20.0, baseHz * (1.0 + static_cast<double>(31 - rateShift) / 8.0));
        noisePhase[channel] += hz / sampleRate;
        while (noisePhase[channel] >= 1.0)
        {
            noisePhase[channel] -= 1.0;
            const auto feedback = ((noiseLfsr[channel] & 1u) ^ ((noiseLfsr[channel] >> 5u) & 1u)) & 1u;
            noiseLfsr[channel] = ((noiseLfsr[channel] >> 1u) | (feedback << 16u)) & 0x1ffffu;
            if (noiseLfsr[channel] == 0)
                noiseLfsr[channel] = 0x1ffffu;
        }
        return (noiseLfsr[channel] & 1u) != 0 ? 1.0 : -1.0;
    }

    bool lfoEnabled() const
    {
        return huc6280LfoModeForPatch(patch) > 1u
            && sourceEnabled(patch, 0)
            && sourceEnabled(patch, 1);
    }

    double lfoDepthSemitones() const
    {
        switch (huc6280LfoModeForPatch(patch))
        {
            case 2: return 0.25 + static_cast<double>(clamp01(patch.control3)) * 0.75;
            case 3: return 1.25 + static_cast<double>(clamp01(patch.control3)) * 2.75;
            case 4: return 0.75 + static_cast<double>(clamp01(patch.control3)) * 1.50;
            default: return 0.0;
        }
    }

    double lfoRateHz() const
    {
        const auto motion = static_cast<double>(clamp01(patch.control2));
        switch (huc6280LfoModeForPatch(patch))
        {
            case 2: return 2.0 + motion * 5.0;
            case 3: return 4.0 + motion * 9.0;
            case 4: return 9.0 + motion * 18.0;
            default: return 0.0;
        }
    }

    double currentLfoSample() const
    {
        const auto index = static_cast<size_t>(std::floor(lfoPhase * 32.0)) & 31u;
        return (static_cast<double>(waveRam[1][index]) / 31.0) * 2.0 - 1.0;
    }

    void advanceLfoSource()
    {
        lfoPhase = wrapPhase(lfoPhase + lfoRateHz() / sampleRate);
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (chipPolyChannelEnabled(channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (chipPolyChannelEnabled(channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelStamp.size(); ++channel)
        {
            if (chipPolyChannelEnabled(channel) && channelStamp[channel] < oldestStamp)
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
            if (chipPolyChannelEnabled(channel) && channelNotes[channel] >= 0)
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
        control.fill(0x00);
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
        frequency[index] = hucPeriodForNote(channelNotes[index]);
        const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 31.0f)), 1, 31));
        control[index] = static_cast<uint8_t>(0x80u | volume);
        seedWave(index);
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
            control[channel] = 0x00;
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    uint8_t selectedChannel = 0;
    std::array<uint16_t, 6> frequency {};
    std::array<uint8_t, 6> control {};
    std::array<uint8_t, 6> balance {};
    std::array<uint8_t, 6> noiseControl {};
    std::array<std::array<uint8_t, 32>, 6> waveRam {};
    std::array<uint8_t, 6> waveWriteIndex {};
    std::array<double, 6> phase {};
    std::array<double, 6> noisePhase {};
    std::array<uint32_t, 6> noiseLfsr {};
    double lfoPhase = 0.0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 6> channelNotes {};
    std::array<float, 6> channelVelocity {};
    std::array<uint64_t, 6> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class SccCore final : public ChipCore
{
public:
    explicit SccCore(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    ~SccCore() override
    {
        if (emu != nullptr)
            SCC_delete(emu);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        period.fill(0);
        volume.fill(0);
        keyOnMask = 0;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        recreateEmu();
        for (size_t channel = 0; channel < waveRam.size(); ++channel)
            seedWave(channel);
        syncAllRegistersToEmu();
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        patch = nextPatch;
        if (patch.playMode == PlayMode::chipPoly)
            clearChipPolyState();
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        if (reg < 0xa0u)
        {
            const auto channel = std::min<size_t>(reg / 32u, waveRam.size() - 1u);
            waveRam[channel][reg & 31u] = value;
            writeEmuRegister(reg, value);
            return;
        }

        if (reg >= 0xa0u && reg <= 0xa9u)
        {
            const auto channel = static_cast<size_t>((reg - 0xa0u) / 2u);
            if (channel >= period.size())
                return;

            if (((reg - 0xa0u) & 1u) == 0)
                period[channel] = static_cast<uint16_t>((period[channel] & 0x0f00u) | value);
            else
                period[channel] = static_cast<uint16_t>((period[channel] & 0x00ffu) | ((value & 0x0fu) << 8u));
            writeEmuRegister(static_cast<uint8_t>(0xc0u + (reg - 0xa0u)), value);
            return;
        }

        if (reg >= 0xaau && reg <= 0xaeu)
        {
            const auto channel = static_cast<size_t>(reg - 0xaau);
            if (channel < volume.size())
            {
                volume[channel] = value & 0x0fu;
                writeEmuRegister(static_cast<uint8_t>(0xd0u + channel), value);
            }
            return;
        }

        if (reg == 0xafu)
        {
            keyOnMask = value & 0x1fu;
            writeEmuRegister(0xe1u, keyOnMask);
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 16.0f));
        std::array<int, 5> notes {
            heldNote,
            heldNote + std::max(1, spread / 3),
            heldNote + std::max(2, (spread * 2) / 3),
            heldNote + std::max(3, spread),
            heldNote + 12
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote + 7 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12 };
                break;
            case MacroKind::laser:
                notes[0] = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                notes[1] = heldNote;
                notes[2] = heldNote - 12;
                notes[3] = heldNote - 19;
                notes[4] = heldNote - 24;
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        keyOnMask = 0;
        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            period[channel] = sccPeriodForNote(notes[channel]);
            volume[channel] = channelVolumeForPatch(channel);
            seedWave(channel);
            if (channelActiveForPatch(channel))
                keyOnMask |= static_cast<uint8_t>(1u << channel);
        }
        syncAllRegistersToEmu();
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
            keyOnMask = 0;
            writeEmuRegister(0xe1u, keyOnMask);
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 5> panPositions { -1.0, -0.5, 0.0, 0.5, 1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        if (emu != nullptr)
            SCC_calc(emu);

        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            const auto sample = renderChannelFromEmu(channel);
            if (std::abs(sample) <= 1.0e-9)
                continue;

            ++audibleCount;
            const auto spread = std::clamp(static_cast<double>(patch.stereoSpread), 0.0, 1.0);
            const auto pan = panPositions[channel] * spread;
            left += sample * (pan <= 0.0 ? 1.0 : 1.0 - pan);
            right += sample * (pan >= 0.0 ? 1.0 : 1.0 + pan);
        }

        if (audibleCount > 0)
        {
            left /= static_cast<double>(audibleCount);
            right /= static_cast<double>(audibleCount);
        }

        if (patch.envelopeDecay > 0.001f && noteVelocity > 0.0f)
        {
            const auto decay = 1.0 - static_cast<double>(patch.envelopeDecay) * 0.00045;
            noteVelocity = static_cast<float>(std::max(0.0, static_cast<double>(noteVelocity) * decay));
        }

        return { static_cast<float>(std::clamp(left * noteVelocity, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right * noteVelocity, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t channel = 0; channel < period.size(); ++channel)
        {
            writes.push_back({ 0, static_cast<uint16_t>(0xa0u + channel * 2u), static_cast<uint8_t>(period[channel] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(0xa1u + channel * 2u), static_cast<uint8_t>((period[channel] >> 8u) & 0x0fu) });
            writes.push_back({ 0, static_cast<uint16_t>(0xaau + channel), volume[channel] });
        }
        writes.push_back({ 0, 0xafu, keyOnMask });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::scc; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "Konami SCC"; }
    std::string implementedAccuracy() const override { return "partial emu2212-backed register-level"; }
    std::string limitations() const override
    {
        return "Five frequency, volume, key-on, and waveform-RAM register paths are driven through the MIT emu2212 SCC/SCC+ core; exact mapper/bank behavior, output curve, timing edge cases, golden emulator comparison, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Konami SCC\","
             << "\"implementedAccuracy\":\"partial emu2212-backed register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"core\":\"emu2212\","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 0)) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 1)) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 2)) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 3)) << ","
             << "\"waveShapeChoice4\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 4)) << ","
             << "\"period0\":" << period[0] << ","
             << "\"period1\":" << period[1] << ","
             << "\"period2\":" << period[2] << ","
             << "\"period3\":" << period[3] << ","
             << "\"period4\":" << period[4] << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"volume4\":" << static_cast<int>(volume[4]) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"waveRam0\":" << static_cast<int>(waveRam[0][0]) << ","
             << "\"waveRam31\":" << static_cast<int>(waveRam[0][31]) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"uiExposesFiveChannels\":1,"
             << "\"internalChannelCount\":5,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t sccPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * hz)), 1.0, 4095.0));
    }

    uint8_t channelVolumeForPatch(size_t channel) const
    {
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 15.0f)), 1, 15);
        const auto trim = sourceLevel(patch, channel);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(base) * trim)), 0, 15));
    }

    bool channelActiveForPatch(size_t channel) const
    {
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);

        return patch.macro == MacroKind::arp
            || patch.macro == MacroKind::powerUp
            || patch.macro == MacroKind::hit;
    }

    void seedWave(size_t channel)
    {
        const auto choice = std::clamp(patch.waveShape, 0, 4);
        const auto skew = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        for (size_t i = 0; i < 32; ++i)
        {
            const auto phaseValue = static_cast<double>(i) / 32.0;
            auto sample = 128;
            switch (choice)
            {
                case 1: sample = static_cast<int>(std::round(255.0 * phaseValue)); break;
                case 2: sample = i < 16 ? static_cast<int>(std::round(255.0 * (static_cast<double>(i) / 15.0))) : static_cast<int>(std::round(255.0 * (1.0 - static_cast<double>(i - 16) / 15.0))); break;
                case 3: sample = i < static_cast<size_t>(std::round(4.0 + skew * 24.0)) ? 255 : 0; break;
                case 4: sample = ((static_cast<int>(i) * 17 + static_cast<int>(channel) * 29) & 255); break;
                case 0:
                default: sample = static_cast<int>(std::round(128.0 + 127.0 * std::sin(twoPi * phaseValue))); break;
            }
            waveRam[channel][i] = static_cast<uint8_t>(std::clamp(sample, 0, 255));
        }
        syncWaveToEmu(channel);
    }

    double renderChannelFromEmu(size_t channel) const
    {
        if (emu == nullptr)
            return 0.0;

        if ((keyOnMask & (1u << channel)) == 0)
            return 0.0;

        const auto vol = static_cast<double>(volume[channel] & 0x0fu) / 15.0;
        if (vol <= 0.0)
            return 0.0;

        return std::clamp(static_cast<double>(emu->ch_out[channel]) / 4096.0, -1.0, 1.0);
    }

    void recreateEmu()
    {
        if (emu != nullptr)
            SCC_delete(emu);

        emu = SCC_new(static_cast<uint32_t>(std::max(1.0, std::round(clock))),
                      static_cast<uint32_t>(std::max(1.0, std::round(sampleRate))));
        if (emu == nullptr)
            return;

        SCC_set_type(emu, SCC_ENHANCED);
        SCC_set_quality(emu, 0);
        SCC_reset(emu);
        writeEmuRegister(0xe0u, 0x01u);
    }

    void writeEmuRegister(uint8_t address, uint8_t value) const
    {
        if (emu != nullptr)
            SCC_writeReg(emu, address, value);
    }

    void syncWaveToEmu(size_t channel) const
    {
        if (emu == nullptr || channel >= waveRam.size())
            return;

        const auto base = static_cast<uint8_t>(channel * 32u);
        for (uint8_t index = 0; index < 32u; ++index)
            writeEmuRegister(static_cast<uint8_t>(base + index), waveRam[channel][index]);
    }

    void syncAllRegistersToEmu() const
    {
        if (emu == nullptr)
            return;

        writeEmuRegister(0xe0u, 0x01u);
        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            syncWaveToEmu(channel);
            writeEmuRegister(static_cast<uint8_t>(0xc0u + channel * 2u), static_cast<uint8_t>(period[channel] & 0xffu));
            writeEmuRegister(static_cast<uint8_t>(0xc1u + channel * 2u), static_cast<uint8_t>((period[channel] >> 8u) & 0x0fu));
            writeEmuRegister(static_cast<uint8_t>(0xd0u + channel), volume[channel]);
        }
        writeEmuRegister(0xe1u, keyOnMask);
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
        keyOnMask = 0;
        writeEmuRegister(0xe1u, keyOnMask);
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
        period[index] = sccPeriodForNote(channelNotes[index]);
        volume[index] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 1, 15));
        keyOnMask |= static_cast<uint8_t>(1u << index);
        seedWave(index);
        syncAllRegistersToEmu();
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
            keyOnMask &= static_cast<uint8_t>(~(1u << channel));
        }
        writeEmuRegister(0xe1u, keyOnMask);
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    std::array<uint16_t, 5> period {};
    std::array<uint8_t, 5> volume {};
    uint8_t keyOnMask = 0;
    std::array<std::array<uint8_t, 32>, 5> waveRam {};
    SCC* emu = nullptr;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 5> channelNotes {};
    std::array<float, 5> channelVelocity {};
    std::array<uint64_t, 5> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class NamcoWsgCore final : public ChipCore
{
public:
    explicit NamcoWsgCore(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 96000.0;
        frequency.fill(0);
        volume.fill(0);
        enabledMask = 0;
        phase.fill(0.0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        for (size_t channel = 0; channel < waveRam.size(); ++channel)
            seedWave(channel);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        patch = nextPatch;
        if (patch.playMode == PlayMode::chipPoly)
            clearChipPolyState();
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        if (reg < 0x80u)
        {
            const auto channel = std::min<size_t>(reg / 32u, waveRam.size() - 1u);
            waveRam[channel][reg & 31u] = value & 0x0fu;
            return;
        }

        if (reg >= 0x80u && reg < 0xa0u)
        {
            const auto channel = static_cast<size_t>((reg - 0x80u) / 4u);
            if (channel >= frequency.size())
                return;

            switch ((reg - 0x80u) & 3u)
            {
                case 0: frequency[channel] = static_cast<uint16_t>((frequency[channel] & 0x0f00u) | value); break;
                case 1: frequency[channel] = static_cast<uint16_t>((frequency[channel] & 0x00ffu) | ((value & 0x0fu) << 8u)); break;
                case 2: volume[channel] = value & 0x0fu; break;
                case 3:
                    if ((value & 0x01u) != 0)
                        enabledMask |= static_cast<uint8_t>(1u << channel);
                    else
                        enabledMask &= static_cast<uint8_t>(~(1u << channel));
                    break;
            }
            return;
        }

        if (reg == 0xbfu)
            enabledMask = value;
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 19.0f));
        std::array<int, 8> notes {
            heldNote,
            heldNote + std::max(1, spread / 4),
            heldNote + std::max(2, spread / 2),
            heldNote + std::max(3, (spread * 3) / 4),
            heldNote + std::max(4, spread),
            heldNote - 12,
            heldNote + 12,
            heldNote + 19
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote - 17, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 17, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::laser:
                for (size_t channel = 0; channel < notes.size(); ++channel)
                    notes[channel] = heldNote + 24 - static_cast<int>(channel * 5) + static_cast<int>(std::round(patch.control2 * 12.0f));
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        enabledMask = 0;
        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            frequency[channel] = namcoPeriodForNote(notes[channel]);
            volume[channel] = channelVolumeForPatch(channel);
            seedWave(channel);
            if (channelActiveForPatch(channel))
                enabledMask |= static_cast<uint8_t>(1u << channel);
        }
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
            enabledMask = 0;
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 8> panPositions { -1.0, -0.72, -0.44, -0.16, 0.16, 0.44, 0.72, 1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            const auto sample = renderChannel(channel);
            if (std::abs(sample) <= 1.0e-9)
                continue;

            ++audibleCount;
            const auto spread = std::clamp(static_cast<double>(patch.stereoSpread), 0.0, 1.0);
            const auto pan = panPositions[channel] * spread;
            left += sample * (pan <= 0.0 ? 1.0 : 1.0 - pan);
            right += sample * (pan >= 0.0 ? 1.0 : 1.0 + pan);
        }

        if (audibleCount > 0)
        {
            left /= static_cast<double>(audibleCount);
            right /= static_cast<double>(audibleCount);
        }

        if (patch.envelopeDecay > 0.001f && noteVelocity > 0.0f)
        {
            const auto decay = 1.0 - static_cast<double>(patch.envelopeDecay) * 0.0005;
            noteVelocity = static_cast<float>(std::max(0.0, static_cast<double>(noteVelocity) * decay));
        }

        return { static_cast<float>(std::clamp(left * noteVelocity, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right * noteVelocity, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t channel = 0; channel < frequency.size(); ++channel)
        {
            const auto base = static_cast<uint16_t>(0x80u + channel * 4u);
            writes.push_back({ 0, base, static_cast<uint8_t>(frequency[channel] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>((frequency[channel] >> 8u) & 0x0fu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 2u), volume[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 3u), static_cast<uint8_t>((enabledMask >> channel) & 1u) });
        }
        writes.push_back({ 0, 0xbfu, enabledMask });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::namcoWsg; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "Namco arcade WSG"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Eight simplified wavetable lanes, 32-sample 4-bit wave RAM, frequency, volume, and enable register paths are modeled; exact Namco custom-chip variants, voice count differences, waveform addressing, DAC/output curve, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Namco arcade WSG\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 0)) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 1)) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 2)) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 3)) << ","
             << "\"waveShapeChoice4\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 4)) << ","
             << "\"waveShapeChoice5\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 5)) << ","
             << "\"waveShapeChoice6\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 6)) << ","
             << "\"waveShapeChoice7\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 7)) << ","
             << "\"period0\":" << frequency[0] << ","
             << "\"period1\":" << frequency[1] << ","
             << "\"period2\":" << frequency[2] << ","
             << "\"period3\":" << frequency[3] << ","
             << "\"period4\":" << frequency[4] << ","
             << "\"period7\":" << frequency[7] << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"volume7\":" << static_cast<int>(volume[7]) << ","
             << "\"enableMask\":" << static_cast<int>(enabledMask) << ","
             << "\"waveRam0\":" << static_cast<int>(waveRam[0][0]) << ","
             << "\"waveRam31\":" << static_cast<int>(waveRam[0][31]) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"exposedChannelCount\":8,"
             << "\"internalChannelCount\":8,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"assignedNote6\":" << channelNotes[6] << ","
             << "\"assignedNote7\":" << channelNotes[7] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t namcoPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * hz)), 1.0, 4095.0));
    }

    uint8_t channelVolumeForPatch(size_t channel) const
    {
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 15.0f)), 1, 15);
        const auto trim = sourceLevel(patch, channel);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(base) * trim)), 0, 15));
    }

    bool channelActiveForPatch(size_t channel) const
    {
        return sourceEnabled(patch, channel);
    }

    void seedWave(size_t channel)
    {
        const auto choice = std::clamp(patch.waveShape, 0, 4);
        const auto skew = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        for (size_t i = 0; i < 32; ++i)
        {
            const auto phaseValue = static_cast<double>(i) / 32.0;
            auto sample = 8;
            switch (choice)
            {
                case 1: sample = static_cast<int>(std::round(15.0 * phaseValue)); break;
                case 2: sample = i < 16 ? static_cast<int>(std::round(15.0 * (static_cast<double>(i) / 15.0))) : static_cast<int>(std::round(15.0 * (1.0 - static_cast<double>(i - 16) / 15.0))); break;
                case 3: sample = i < static_cast<size_t>(std::round(4.0 + skew * 24.0)) ? 15 : 0; break;
                case 4: sample = ((static_cast<int>(i) * 5 + static_cast<int>(channel) * 3) & 15); break;
                case 0:
                default: sample = static_cast<int>(std::round(7.5 + 7.5 * std::sin(twoPi * phaseValue))); break;
            }
            waveRam[channel][i] = static_cast<uint8_t>(std::clamp(sample, 0, 15));
        }
    }

    double renderChannel(size_t channel)
    {
        if ((enabledMask & (1u << channel)) == 0)
            return 0.0;

        const auto vol = static_cast<double>(volume[channel] & 0x0fu) / 15.0;
        if (vol <= 0.0)
            return 0.0;

        const auto hz = clock / (32.0 * static_cast<double>(std::max<uint16_t>(1, frequency[channel])));
        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);
        const auto index = static_cast<size_t>(std::floor(phase[channel] * 32.0)) & 31u;
        return ((static_cast<double>(waveRam[channel][index]) / 15.0) * 2.0 - 1.0) * vol;
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
        enabledMask = 0;
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
        frequency[index] = namcoPeriodForNote(channelNotes[index]);
        volume[index] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 1, 15));
        enabledMask |= static_cast<uint8_t>(1u << index);
        seedWave(index);
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
            enabledMask &= static_cast<uint8_t>(~(1u << channel));
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 96000.0;
    std::array<uint16_t, 8> frequency {};
    std::array<uint8_t, 8> volume {};
    uint8_t enabledMask = 0;
    std::array<std::array<uint8_t, 32>, 8> waveRam {};
    std::array<double, 8> phase {};
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 8> channelNotes {};
    std::array<float, 8> channelVelocity {};
    std::array<uint64_t, 8> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class Spc700Core final : public ChipCore
{
public:
    explicit Spc700Core(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 32000.0;
        pitch.fill(0x1000);
        volume.fill(0);
        adsr.fill(0);
        gain.fill(0);
        enabledMask = 0;
        keyOnMask = 0;
        position.fill(0.0);
        voiceAgeSamples.fill(0);
        lastVoiceSample.fill(0.0);
        envelope.fill(0.0);
        envelopeStage.fill(0);
        noiseAccumulator.fill(0.0);
        for (size_t voice = 0; voice < noiseLfsr.size(); ++voice)
            noiseLfsr[voice] = static_cast<uint16_t>(0x4000u | ((voice + 1u) * 0x0231u));
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        sampleTemplate = 0;
        playbackMode = 1;
        externalBrrSampleLoaded = false;
        externalPcmSampleLoaded = false;
        externalBrrBank.clear();
        externalBrrLoopStarts.clear();
        selectedExternalBrrSlot = -1;
        brrBlockCount = 0;
        brrEndFlagSeen = false;
        brrLoopFlagSeen = false;
        lastDecodedBrrLoopStart = 0;
        sampleLoopStarts.fill(0);
        echoMemoryLeft.fill(0.0);
        echoMemoryRight.fill(0.0);
        echoIndex = 0;
        for (size_t voice = 0; voice < sampleRam.size(); ++voice)
            seedSample(voice);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        const auto sampleSlotsChanged = nextPatch.spc700VoiceSampleSlots != patch.spc700VoiceSampleSlots;
        patch = nextPatch;
        const auto nextTemplate = resolvedSampleTemplate();
        if (! externalBrrSampleLoaded && nextTemplate != sampleTemplate)
        {
            sampleTemplate = nextTemplate;
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                seedSample(voice);
        }
        else if (externalBrrSampleLoaded && sampleSlotsChanged)
        {
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                applyCurrentSampleToVoice(voice);
        }
        playbackMode = spc700SamplePlaybackModeForPatch(patch);
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        std::vector<std::vector<uint8_t>> bank;
        if (! data.empty())
            bank.push_back(std::move(data));
        setExternalSampleBank(std::move(bank), 0);
    }

    void setExternalSampleBank(std::vector<std::vector<uint8_t>> bank, int selectedSlot) override
    {
        std::vector<ExternalSampleData> typedBank;
        typedBank.reserve(bank.size());
        for (auto& sampleBytes : bank)
            typedBank.push_back({ std::move(sampleBytes), ExternalSampleEncoding::spc700Brr });
        setExternalSampleBank(std::move(typedBank), selectedSlot);
    }

    void setExternalSampleBank(std::vector<ExternalSampleData> bank, int selectedSlot) override
    {
        externalBrrSampleLoaded = false;
        externalPcmSampleLoaded = false;
        externalBrrBank.clear();
        externalBrrLoopStarts.clear();
        selectedExternalBrrSlot = -1;
        brrBlockCount = 0;
        brrEndFlagSeen = false;
        brrLoopFlagSeen = false;

        externalBrrBank.reserve(bank.size());
        externalBrrLoopStarts.reserve(bank.size());
        for (const auto& sample : bank)
        {
            std::vector<double> decoded;
            auto decodedLoopStart = size_t { 0 };
            if (sample.encoding == ExternalSampleEncoding::signedPcm8)
            {
                decoded = decodePcm8Sample(sample.bytes);
                externalPcmSampleLoaded = externalPcmSampleLoaded || ! decoded.empty();
            }
            else
            {
                decoded = decodeBrrSample(sample.bytes);
                decodedLoopStart = lastDecodedBrrLoopStart;
                if (decoded.empty() && sample.encoding == ExternalSampleEncoding::rawBytes)
                {
                    decoded = decodePcm8Sample(sample.bytes);
                    decodedLoopStart = 0;
                    externalPcmSampleLoaded = externalPcmSampleLoaded || ! decoded.empty();
                }
            }

            if (! decoded.empty())
            {
                decodedLoopStart = decodedLoopStart < decoded.size() ? decodedLoopStart : 0;
                externalBrrBank.push_back(decoded);
                externalBrrLoopStarts.push_back(decodedLoopStart);
            }
        }

        if (externalBrrBank.empty())
            return;

        externalBrrSampleLoaded = true;
        setExternalSampleSlot(selectedSlot);
        sampleTemplate = 0;
        position.fill(0.0);
    }

    void setExternalSampleSlot(int selectedSlot) override
    {
        if (externalBrrBank.empty())
        {
            selectedExternalBrrSlot = -1;
            externalBrrSampleLoaded = false;
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                seedSample(voice);
            return;
        }

        if (selectedSlot < 0)
        {
            selectedExternalBrrSlot = -1;
            externalBrrSampleLoaded = true;
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                applyCurrentSampleToVoice(voice);
            return;
        }

        selectedExternalBrrSlot = std::clamp(selectedSlot, 0, static_cast<int>(externalBrrBank.size() - 1u));
        externalBrrSampleLoaded = true;
        for (size_t voice = 0; voice < sampleRam.size(); ++voice)
            applyCurrentSampleToVoice(voice);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        const auto voice = static_cast<size_t>((reg >> 4u) & 7u);
        const auto offset = reg & 0x0fu;

        switch (offset)
        {
            case 0: pitch[voice] = static_cast<uint16_t>((pitch[voice] & 0xff00u) | value); break;
            case 1: pitch[voice] = static_cast<uint16_t>((pitch[voice] & 0x00ffu) | ((value & 0x3fu) << 8u)); break;
            case 2: volume[voice] = value & 0x7fu; break;
            case 3: adsr[voice] = value; break;
            case 4: gain[voice] = value & 0x7fu; break;
            case 5:
                sampleTemplate = value % 5u;
                seedSample(voice);
                break;
            case 6:
                if ((value & 0x01u) != 0)
                    enabledMask |= static_cast<uint8_t>(1u << voice);
                else
                    enabledMask &= static_cast<uint8_t>(~(1u << voice));
                break;
            case 7:
                playbackMode = static_cast<uint8_t>(std::clamp(static_cast<int>(value), 1, 2));
                break;
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

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 19.0f));
        std::array<int, 8> notes {
            heldNote,
            heldNote + std::max(1, spread / 4),
            heldNote + std::max(2, spread / 2),
            heldNote + std::max(3, (spread * 3) / 4),
            heldNote - 12,
            heldNote + 12,
            heldNote + 19,
            heldNote + 24
        };

        switch (patch.macro)
        {
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote - 17, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 17, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67 };
                break;
            case MacroKind::laser:
                for (size_t voice = 0; voice < notes.size(); ++voice)
                    notes[voice] = heldNote + 24 - static_cast<int>(voice * 4) + static_cast<int>(std::round(patch.control2 * 12.0f));
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        enabledMask = 0;
        keyOnMask = 0;
        for (size_t voice = 0; voice < pitch.size(); ++voice)
            triggerVoice(voice, notes[voice], noteVelocity, voiceActiveForPatch(voice));
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
            for (size_t voice = 0; voice < envelopeStage.size(); ++voice)
                releaseVoice(voice);
        }
    }

    StereoFrame renderSample() override
    {
        double left = 0.0;
        double right = 0.0;
        double echoInputLeft = 0.0;
        double echoInputRight = 0.0;
        auto audibleCount = 0;
        auto echoCount = 0;

        for (size_t voice = 0; voice < pitch.size(); ++voice)
        {
            const auto sample = renderVoice(voice);
            if (std::abs(sample) <= 1.0e-9)
                continue;

            ++audibleCount;
            const auto leftGain = voiceLeftGain(voice);
            const auto rightGain = voiceRightGain(voice);
            left += sample * leftGain;
            right += sample * rightGain;

            const auto voiceEcho = voiceEchoSendLevel(voice);
            if (voiceEcho > 0.0)
            {
                ++echoCount;
                echoInputLeft += sample * leftGain * voiceEcho;
                echoInputRight += sample * rightGain * voiceEcho;
            }
        }

        if (audibleCount > 0)
        {
            left /= static_cast<double>(audibleCount);
            right /= static_cast<double>(audibleCount);
        }
        if (echoCount > 0)
        {
            echoInputLeft /= static_cast<double>(echoCount);
            echoInputRight /= static_cast<double>(echoCount);
        }

        const auto echoSend = spc700EchoSend();
        const auto echoFeedback = spc700EchoFeedback();
        const auto delaySamples = spc700EchoDelaySamples();
        const auto readIndex = (echoIndex + echoMemoryLeft.size() - delaySamples) % echoMemoryLeft.size();
        const auto filteredLeft = spc700FirFiltered(echoMemoryLeft, readIndex);
        const auto filteredRight = spc700FirFiltered(echoMemoryRight, readIndex);

        echoMemoryLeft[echoIndex] = std::clamp(echoInputLeft + (filteredLeft * echoFeedback), -1.0, 1.0);
        echoMemoryRight[echoIndex] = std::clamp(echoInputRight + (filteredRight * echoFeedback), -1.0, 1.0);
        echoIndex = (echoIndex + 1) % echoMemoryLeft.size();
        left += filteredLeft * echoSend;
        right += filteredRight * echoSend;

        return { static_cast<float>(std::clamp(left, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t voice = 0; voice < pitch.size(); ++voice)
        {
            const auto base = static_cast<uint16_t>(voice * 0x10u);
            writes.push_back({ 0, base, static_cast<uint8_t>(pitch[voice] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>((pitch[voice] >> 8u) & 0x3fu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 2u), volume[voice] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 3u), adsr[voice] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 4u), gain[voice] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 5u), sampleTemplate });
            writes.push_back({ 0, static_cast<uint16_t>(base + 6u), static_cast<uint8_t>((enabledMask >> voice) & 1u) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 7u), playbackMode });
            writes.push_back({ 0, static_cast<uint16_t>(base + 8u), voiceLeftVolumeRegister(voice) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 9u), voiceRightVolumeRegister(voice) });
        }
        writes.push_back({ 0, 0x0du, static_cast<uint8_t>(static_cast<int>(spc700EchoFeedbackRegister()) & 0xff) });
        writes.push_back({ 0, 0x2cu, static_cast<uint8_t>(static_cast<int>(spc700EchoLeftVolumeRegister()) & 0xff) });
        writes.push_back({ 0, 0x3cu, static_cast<uint8_t>(static_cast<int>(spc700EchoRightVolumeRegister()) & 0xff) });
        writes.push_back({ 0, 0x4du, echoEnabledMask() });
        writes.push_back({ 0, 0x7du, spc700EchoDelayRegister() });
        const auto fir = spc700FirCoefficients();
        for (size_t tap = 0; tap < fir.size(); ++tap)
        {
            const auto reg = static_cast<uint16_t>(0x0fu + (tap * 0x10u));
            const auto value = static_cast<uint8_t>(static_cast<int>(fir[tap]) & 0xff);
            writes.push_back({ 0, reg, value });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::spc700; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "SNES SPC700-style"; }
    std::string implementedAccuracy() const override { return "partial clean-room sample-voice model"; }
    std::string limitations() const override
    {
        return "Eight lo-fi sample voices, pitch, 7-bit per-voice left/right volume state, loop/one-shot playback state, playable ADSR/gain-style note shaping, generated sample templates, clean-room BRR block decoding for renderer-loaded samples, BRR loop-flag loop starts, Gaussian-style 4-tap sample interpolation, partial S-DSP-style per-voice noise source, musical pitch motion, partial PMON-style previous-voice pitch modulation, an echo-enable mask, echo volume/feedback/delay register state, signed 8-bit FIR coefficient state, and a musical stereo echo helper are modeled; exact S-DSP Gaussian table behavior, SPC700 CPU timing, S-DSP register edge cases, source-directory loop address behavior, exact noise timing, exact PMON timing/scaling, exact FIR echo memory behavior, sample directory addressing, exact envelope timing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SNES SPC700-style\","
             << "\"implementedAccuracy\":\"partial clean-room sample-voice model\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << static_cast<int>(sampleTemplate) << ","
             << "\"samplePlaybackMode\":" << static_cast<int>(playbackMode) << ","
             << "\"sampleLoopEnabled\":" << (playbackMode == 1u ? 1 : 0) << ","
             << "\"envelopeShapeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"envelopeShapeResolved\":" << static_cast<int>(spc700EnvelopeShapeForPatch(patch)) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"noiseModeResolved\":" << static_cast<int>(spc700NoiseModeForPatch(patch)) << ","
             << "\"noiseClock\":" << static_cast<int>(spc700NoiseClockForPatch(patch)) << ","
             << "\"noiseEnabledMask\":" << static_cast<int>(noiseEnabledMask()) << ","
             << "\"externalBrrSampleLoaded\":" << (externalBrrSampleLoaded ? 1 : 0) << ","
             << "\"externalPcmSampleLoaded\":" << (externalPcmSampleLoaded ? 1 : 0) << ","
             << "\"brrBlockCount\":" << brrBlockCount << ","
             << "\"brrEndFlagSeen\":" << (brrEndFlagSeen ? 1 : 0) << ","
             << "\"brrLoopFlagSeen\":" << (brrLoopFlagSeen ? 1 : 0) << ","
             << "\"externalBrrBankSize\":" << externalBrrBank.size() << ","
             << "\"selectedExternalBrrSlot\":" << selectedExternalBrrSlot << ","
             << "\"selectedExternalSampleLength\":" << selectedExternalSampleLength() << ","
             << "\"selectedExternalSampleLoopStart\":" << selectedExternalSampleLoopStart() << ","
             << "\"voiceSampleSlot0\":" << patch.spc700VoiceSampleSlots[0] << ","
             << "\"voiceSampleSlot1\":" << patch.spc700VoiceSampleSlots[1] << ","
             << "\"voiceSampleSlot2\":" << patch.spc700VoiceSampleSlots[2] << ","
             << "\"voiceSampleSlot3\":" << patch.spc700VoiceSampleSlots[3] << ","
             << "\"voiceSampleSlot4\":" << patch.spc700VoiceSampleSlots[4] << ","
             << "\"voiceSampleSlot5\":" << patch.spc700VoiceSampleSlots[5] << ","
             << "\"voiceSampleSlot6\":" << patch.spc700VoiceSampleSlots[6] << ","
             << "\"voiceSampleSlot7\":" << patch.spc700VoiceSampleSlots[7] << ","
             << "\"pitch0\":" << pitch[0] << ","
             << "\"pitch1\":" << pitch[1] << ","
             << "\"pitch2\":" << pitch[2] << ","
             << "\"pitch3\":" << pitch[3] << ","
             << "\"pitch4\":" << pitch[4] << ","
             << "\"pitch5\":" << pitch[5] << ","
             << "\"pitch6\":" << pitch[6] << ","
             << "\"pitch7\":" << pitch[7] << ","
             << "\"pitchMotionDepthCents\":" << spc700PitchMotionDepthCents() << ","
             << "\"pitchMotionDirection\":" << spc700PitchMotionDirection() << ","
             << "\"pitchModDepthCents\":" << spc700PitchModDepthCents() << ","
             << "\"pitchModEnabledMask\":" << static_cast<int>(spc700PitchModEnabledMask()) << ","
             << "\"pitchModSource0\":" << lastVoiceSample[0] << ","
             << "\"echoSend\":" << spc700EchoSend() << ","
             << "\"echoEnabledMask\":" << static_cast<int>(echoEnabledMask()) << ","
             << "\"echoInputLevel\":" << echoInputLevel() << ","
             << "\"echoFeedback\":" << spc700EchoFeedback() << ","
             << "\"echoDelayMs\":" << spc700EchoDelayMs() << ","
             << "\"echoDelaySamples\":" << spc700EchoDelaySamples() << ","
             << "\"echoLeftVolumeRegister\":" << static_cast<int>(spc700EchoLeftVolumeRegister()) << ","
             << "\"echoRightVolumeRegister\":" << static_cast<int>(spc700EchoRightVolumeRegister()) << ","
             << "\"echoFeedbackRegister\":" << static_cast<int>(spc700EchoFeedbackRegister()) << ","
             << "\"echoDelayRegister\":" << static_cast<int>(spc700EchoDelayRegister()) << ","
             << "\"firTap0\":" << static_cast<int>(spc700FirCoefficients()[0]) << ","
             << "\"firTap1\":" << static_cast<int>(spc700FirCoefficients()[1]) << ","
             << "\"firTap2\":" << static_cast<int>(spc700FirCoefficients()[2]) << ","
             << "\"firTap3\":" << static_cast<int>(spc700FirCoefficients()[3]) << ","
             << "\"firTap4\":" << static_cast<int>(spc700FirCoefficients()[4]) << ","
             << "\"firTap5\":" << static_cast<int>(spc700FirCoefficients()[5]) << ","
             << "\"firTap6\":" << static_cast<int>(spc700FirCoefficients()[6]) << ","
             << "\"firTap7\":" << static_cast<int>(spc700FirCoefficients()[7]) << ","
             << "\"firCoefficientSum\":" << spc700FirCoefficientSum() << ","
             << "\"pitch0Current\":" << currentPitchForVoice(0) << ","
             << "\"pitch1Current\":" << currentPitchForVoice(1) << ","
             << "\"pitch7Current\":" << currentPitchForVoice(7) << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"volume4\":" << static_cast<int>(volume[4]) << ","
             << "\"volume5\":" << static_cast<int>(volume[5]) << ","
             << "\"volume6\":" << static_cast<int>(volume[6]) << ","
             << "\"volume7\":" << static_cast<int>(volume[7]) << ","
             << "\"volumeLeft0\":" << static_cast<int>(voiceLeftVolumeRegister(0)) << ","
             << "\"volumeRight0\":" << static_cast<int>(voiceRightVolumeRegister(0)) << ","
             << "\"volumeLeft7\":" << static_cast<int>(voiceLeftVolumeRegister(7)) << ","
             << "\"volumeRight7\":" << static_cast<int>(voiceRightVolumeRegister(7)) << ","
             << "\"adsr0\":" << static_cast<int>(adsr[0]) << ","
             << "\"envelope0\":" << envelope[0] << ","
             << "\"envelopeStage0\":" << static_cast<int>(envelopeStage[0]) << ","
             << "\"sustainLevel\":" << spc700SustainLevel() << ","
             << "\"attackSeconds\":" << spc700AttackSeconds() << ","
             << "\"decaySeconds\":" << spc700DecaySeconds() << ","
             << "\"releaseSeconds\":" << spc700ReleaseSeconds() << ","
             << "\"gain0\":" << static_cast<int>(gain[0]) << ","
             << "\"enabledMask\":" << static_cast<int>(enabledMask) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"sourceLevel5\":" << sourceLevel(patch, 5) << ","
             << "\"sourceLevel6\":" << sourceLevel(patch, 6) << ","
             << "\"sourceLevel7\":" << sourceLevel(patch, 7) << ","
             << "\"sampleLength0\":" << sampleRam[0].size() << ","
             << "\"sampleLoopStart0\":" << sampleLoopStarts[0] << ","
             << "\"sampleLoopStartResolved0\":" << resolvedLoopStartForVoice(0) << ","
             << "\"sampleLoopEndResolved0\":" << resolvedLoopEndForVoice(0) << ","
             << "\"sampleLoopControlStart\":" << patch.spc700LoopStart << ","
             << "\"sampleLoopControlEnd\":" << patch.spc700LoopEnd << ","
             << "\"externalBrrLoopStartSelected\":" << (selectedExternalBrrSlot >= 0 ? loopStartForExternalSlot(static_cast<size_t>(selectedExternalBrrSlot)) : 0) << ","
             << "\"gaussianStyleInterpolation\":1,"
             << "\"interpolationTaps\":4,"
             << "\"uiExposesAllEightVoices\":1,"
             << "\"internalChannelCount\":8,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"assignedNote6\":" << channelNotes[6] << ","
             << "\"assignedNote7\":" << channelNotes[7] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t spcPitchForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round((hz / 32000.0) * 4096.0 * 16.0), 1.0, 16383.0));
    }

    uint8_t voiceVolumeForPatch(size_t voice, float velocity) const
    {
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 127.0f)), 1, 127);
        const auto trim = voice < patch.sourceLevels.size() ? sourceLevel(patch, voice) : 0.72f;
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(base) * trim * clamp01(velocity))), 0, 127));
    }

    bool voiceActiveForPatch(size_t voice) const
    {
        if (voice < patch.sourceEnabled.size())
            return sourceEnabled(patch, voice);

        return patch.macro == MacroKind::arp
            || patch.macro == MacroKind::hit
            || patch.macro == MacroKind::laser
            || patch.macro == MacroKind::powerUp;
    }

    uint8_t resolvedSampleTemplate() const
    {
        const auto choiceValue = std::clamp(patch.waveShape, 0, 4);
        if (choiceValue > 0)
            return static_cast<uint8_t>(choiceValue);

        switch (patch.macro)
        {
            case MacroKind::bass: return 2;
            case MacroKind::drum:
            case MacroKind::hit: return 4;
            case MacroKind::lead:
            case MacroKind::arp: return 1;
            default: return 3;
        }
    }

    void seedSample(size_t voice)
    {
        auto data = std::vector<double>(64, 0.0);
        const auto choice = sampleTemplate == 0 ? resolvedSampleTemplate() : sampleTemplate;
        for (size_t i = 0; i < data.size(); ++i)
        {
            const auto phaseValue = static_cast<double>(i) / static_cast<double>(data.size());
            double sample = 0.0;
            switch (choice)
            {
                case 1: sample = std::sin(twoPi * phaseValue) * 0.75 + (((i / 8u) & 1u) ? 0.12 : -0.12); break;
                case 2: sample = phaseValue < 0.5 ? (phaseValue * 4.0) - 1.0 : 3.0 - (phaseValue * 4.0); break;
                case 3: sample = phaseValue < 0.5 ? 0.78 : -0.78; break;
                case 4:
                {
                    const auto hash = (static_cast<int>(i) * 1664525u + static_cast<int>(voice) * 1013904223u) & 0xffffu;
                    const auto burst = std::exp(-phaseValue * 7.0);
                    sample = ((static_cast<double>(hash) / 32767.5) - 1.0) * burst;
                    break;
                }
                default: sample = std::sin(twoPi * phaseValue); break;
            }
            data[i] = std::clamp(std::round(sample * 127.0) / 127.0, -1.0, 1.0);
        }
        sampleRam[voice] = std::move(data);
        sampleLoopStarts[voice] = 0;
    }

    std::vector<double> decodeBrrSample(const std::vector<uint8_t>& data)
    {
        std::vector<double> decoded;
        lastDecodedBrrLoopStart = 0;
        if (data.size() < 9u || (data.size() % 9u) != 0u)
            return decoded;

        brrBlockCount = data.size() / 9u;
        decoded.reserve(brrBlockCount * 16u);
        auto previous1 = 0;
        auto previous2 = 0;
        auto decodedLoopFlagSeen = false;

        for (size_t block = 0; block < brrBlockCount; ++block)
        {
            const auto offset = block * 9u;
            const auto header = data[offset];
            const auto range = static_cast<int>((header >> 4u) & 0x0fu);
            const auto filter = static_cast<int>((header >> 2u) & 0x03u);
            brrEndFlagSeen = brrEndFlagSeen || ((header & 0x01u) != 0);
            if ((header & 0x02u) != 0)
            {
                if (! decodedLoopFlagSeen)
                {
                    lastDecodedBrrLoopStart = decoded.size();
                    decodedLoopFlagSeen = true;
                }
                brrLoopFlagSeen = true;
            }

            for (size_t packedIndex = 0; packedIndex < 8u; ++packedIndex)
            {
                const auto packed = data[offset + 1u + packedIndex];
                for (const auto nibble : { static_cast<uint8_t>(packed >> 4u), static_cast<uint8_t>(packed & 0x0fu) })
                {
                    auto sample = (nibble >= 8u ? static_cast<int>(nibble) - 16 : static_cast<int>(nibble));
                    if (range <= 12)
                        sample <<= range;
                    else
                        sample = sample < 0 ? -2048 : 0;

                    auto predicted = sample;
                    switch (filter)
                    {
                        case 1:
                            predicted += (previous1 * 15) / 16;
                            break;
                        case 2:
                            predicted += ((previous1 * 61) / 32) - ((previous2 * 15) / 16);
                            break;
                        case 3:
                            predicted += ((previous1 * 115) / 64) - ((previous2 * 13) / 16);
                            break;
                        case 0:
                        default:
                            break;
                    }

                    predicted = std::clamp(predicted, -32768, 32767);
                    previous2 = previous1;
                    previous1 = predicted;
                    decoded.push_back(static_cast<double>(predicted) / 32768.0);
                }
            }

            if ((header & 0x01u) != 0)
                break;
        }

        return decoded;
    }

    std::vector<double> decodePcm8Sample(const std::vector<uint8_t>& data) const
    {
        std::vector<double> decoded;
        decoded.reserve(data.size());
        for (const auto byte : data)
            decoded.push_back(std::clamp((static_cast<double>(byte) - 128.0) / 128.0, -1.0, 1.0));
        return decoded;
    }

    void triggerVoice(size_t voice, int midiNote, float velocity, bool shouldEnable)
    {
        playbackMode = spc700SamplePlaybackModeForPatch(patch);
        pitch[voice] = spcPitchForNote(midiNote + static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f)));
        volume[voice] = voiceVolumeForPatch(voice, velocity);
        adsr[voice] = spc700AdsrForPatch(patch);
        gain[voice] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control4 * 127.0f)), 0, 127));
        position[voice] = 0.0;
        voiceAgeSamples[voice] = 0;
        lastVoiceSample[voice] = 0.0;
        noiseAccumulator[voice] = 0.0;
        noiseLfsr[voice] = static_cast<uint16_t>(0x4000u | ((voice + 1u) * 0x0231u) | (static_cast<unsigned>(midiNote & 0x7f) << 3u));
        envelope[voice] = 0.0;
        envelopeStage[voice] = shouldEnable ? 1u : 0u;
        applyCurrentSampleToVoice(voice);
        if (shouldEnable && volume[voice] > 0)
        {
            enabledMask |= static_cast<uint8_t>(1u << voice);
            keyOnMask |= static_cast<uint8_t>(1u << voice);
        }
    }

    double renderVoice(size_t voice)
    {
        const auto useNoise = voiceUsesNoise(voice);
        if ((enabledMask & (1u << voice)) == 0 || (! useNoise && sampleRam[voice].empty()) || volume[voice] == 0)
        {
            lastVoiceSample[voice] = 0.0;
            return 0.0;
        }

        updateEnvelope(voice);
        if (envelope[voice] <= 0.0001 && envelopeStage[voice] == 0u)
        {
            lastVoiceSample[voice] = 0.0;
            return 0.0;
        }

        const auto rawSample = useNoise ? renderNoiseVoice(voice) : interpolatedSample(voice, position[voice]);
        lastVoiceSample[voice] = rawSample * envelope[voice];
        const auto sample = rawSample * (static_cast<double>(volume[voice]) / 127.0) * envelope[voice];
        const auto playbackHz = (currentPitchForVoice(voice) / (4096.0 * 16.0)) * 32000.0;
        if (! useNoise)
            position[voice] += (playbackHz * static_cast<double>(sampleRam[voice].size())) / sampleRate;
        ++voiceAgeSamples[voice];

        if (useNoise)
        {
            const auto transientSeconds = std::max(0.035, spc700DecaySeconds() * 2.5);
            if (playbackMode == 2u && static_cast<double>(voiceAgeSamples[voice]) > transientSeconds * sampleRate)
                releaseVoice(voice);
        }
        else if (position[voice] >= static_cast<double>(sampleRam[voice].size()))
        {
            if (playbackMode == 1u)
            {
                const auto loopStart = static_cast<double>(resolvedLoopStartForVoice(voice));
                const auto loopEnd = static_cast<double>(resolvedLoopEndForVoice(voice));
                const auto loopLength = std::max(1.0, loopEnd - loopStart);
                position[voice] = loopStart + std::fmod(std::max(0.0, position[voice] - loopStart), loopLength);
            }
            else
            {
                position[voice] = 0.0;
                envelope[voice] = 0.0;
                envelopeStage[voice] = 0u;
                lastVoiceSample[voice] = 0.0;
                enabledMask &= static_cast<uint8_t>(~(1u << voice));
                keyOnMask &= static_cast<uint8_t>(~(1u << voice));
                return 0.0;
            }
        }

        return sample;
    }

    bool voiceUsesNoise(size_t voice) const
    {
        const auto mode = spc700NoiseModeForPatch(patch);
        if (mode <= 1u)
            return false;

        const auto explicitNoise = std::clamp(patch.snNoiseMode, 0, 4) > 1;
        if (explicitNoise)
            return true;

        return patch.macro == MacroKind::drum
            || patch.macro == MacroKind::hit
            || patch.waveShape == 4
            || voice >= 6u;
    }

    uint8_t noiseEnabledMask() const
    {
        uint8_t mask = 0;
        for (size_t voice = 0; voice < noiseLfsr.size(); ++voice)
        {
            if (voiceUsesNoise(voice) && sourceEnabled(patch, voice))
                mask |= static_cast<uint8_t>(1u << voice);
        }
        return mask;
    }

    double spc700NoiseRateHz() const
    {
        const auto clockIndex = static_cast<double>(spc700NoiseClockForPatch(patch));
        if (clockIndex <= 0.0)
            return 0.0;

        return std::clamp(32000.0 / std::pow(2.0, (31.0 - clockIndex) / 4.0), 60.0, 32000.0);
    }

    double renderNoiseVoice(size_t voice)
    {
        const auto rate = spc700NoiseRateHz();
        if (rate <= 0.0 || sampleRate <= 0.0)
            return 0.0;

        noiseAccumulator[voice] += rate / sampleRate;
        while (noiseAccumulator[voice] >= 1.0)
        {
            const auto bit = static_cast<uint16_t>(((noiseLfsr[voice] >> 0u) ^ (noiseLfsr[voice] >> 1u)) & 1u);
            noiseLfsr[voice] = static_cast<uint16_t>((noiseLfsr[voice] >> 1u) | (bit << 14u));
            noiseAccumulator[voice] -= 1.0;
        }

        return (noiseLfsr[voice] & 1u) != 0 ? 0.72 : -0.72;
    }

    double spc700SustainLevel() const
    {
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.20;
            case 3: return 0.78;
            case 4: return 0.03;
            case 2:
            default:
                switch (patch.macro)
                {
                    case MacroKind::bass: return 0.72;
                    case MacroKind::arp: return 0.56;
                    case MacroKind::laser: return 0.34;
                    case MacroKind::powerUp: return 0.58;
                    case MacroKind::lead:
                    case MacroKind::manual:
                    default: return 0.64;
                }
        }
    }

    double spc700AttackSeconds() const
    {
        const auto speed = std::clamp(static_cast<double>(patch.envelopeDecay), 0.0, 1.0);
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.0015 + (1.0 - speed) * 0.006;
            case 3: return 0.025 + (1.0 - speed) * 0.155;
            case 4: return 0.0008 + (1.0 - speed) * 0.002;
            case 2:
            default: return 0.004 + (1.0 - speed) * 0.026;
        }
    }

    double spc700DecaySeconds() const
    {
        const auto speed = std::clamp(static_cast<double>(patch.envelopeDecay), 0.0, 1.0);
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.030 + (1.0 - speed) * 0.180;
            case 3: return 0.450 + (1.0 - speed) * 1.550;
            case 4: return 0.018 + (1.0 - speed) * 0.130;
            case 2:
            default: return 0.110 + (1.0 - speed) * 0.780;
        }
    }

    double spc700ReleaseSeconds() const
    {
        const auto speed = std::clamp(static_cast<double>(patch.envelopeDecay), 0.0, 1.0);
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.018 + (1.0 - speed) * 0.130;
            case 3: return 0.260 + (1.0 - speed) * 1.120;
            case 4: return 0.010 + (1.0 - speed) * 0.075;
            case 2:
            default: return 0.060 + (1.0 - speed) * 0.520;
        }
    }

    void updateEnvelope(size_t voice)
    {
        if (voice >= envelopeStage.size() || sampleRate <= 0.0)
            return;

        switch (envelopeStage[voice])
        {
            case 1:
            {
                envelope[voice] += 1.0 / std::max(1.0, spc700AttackSeconds() * sampleRate);
                if (envelope[voice] >= 1.0)
                {
                    envelope[voice] = 1.0;
                    envelopeStage[voice] = 2u;
                }
                break;
            }
            case 2:
            {
                const auto sustain = spc700SustainLevel();
                const auto coeff = std::exp(-1.0 / std::max(1.0, spc700DecaySeconds() * sampleRate));
                envelope[voice] = sustain + ((envelope[voice] - sustain) * coeff);
                if (std::abs(envelope[voice] - sustain) <= 0.001)
                {
                    envelope[voice] = sustain;
                    envelopeStage[voice] = 3u;
                }
                break;
            }
            case 3:
                envelope[voice] = spc700SustainLevel();
                break;
            case 4:
            {
                const auto coeff = std::exp(-1.0 / std::max(1.0, spc700ReleaseSeconds() * sampleRate));
                envelope[voice] *= coeff;
                if (envelope[voice] <= 0.0005)
                {
                    envelope[voice] = 0.0;
                    envelopeStage[voice] = 0u;
                    enabledMask &= static_cast<uint8_t>(~(1u << voice));
                    keyOnMask &= static_cast<uint8_t>(~(1u << voice));
                }
                break;
            }
            case 0:
            default:
                envelope[voice] = 0.0;
                break;
        }
    }

    void releaseVoice(size_t voice)
    {
        if (voice >= envelopeStage.size())
            return;

        keyOnMask &= static_cast<uint8_t>(~(1u << voice));
        if ((enabledMask & (1u << voice)) != 0 && envelope[voice] > 0.0005)
            envelopeStage[voice] = 4u;
        else
        {
            envelope[voice] = 0.0;
            envelopeStage[voice] = 0u;
            enabledMask &= static_cast<uint8_t>(~(1u << voice));
        }
    }

    void applyCurrentSampleToVoice(size_t voice)
    {
        if (externalBrrSampleLoaded)
        {
            const auto explicitSlotChoice = voice < patch.spc700VoiceSampleSlots.size()
                ? std::clamp(patch.spc700VoiceSampleSlots[voice], 0, 32)
                : 0;
            const auto explicitSlot = explicitSlotChoice > 0 ? explicitSlotChoice - 1 : -1;
            const auto resolvedSlot = explicitSlot >= 0 ? explicitSlot : selectedExternalBrrSlot;
            if (resolvedSlot >= 0 && static_cast<size_t>(resolvedSlot) < externalBrrBank.size())
            {
                sampleRam[voice] = externalBrrBank[static_cast<size_t>(resolvedSlot)];
                sampleLoopStarts[voice] = loopStartForExternalSlot(static_cast<size_t>(resolvedSlot));
            }
            else
            {
                sampleRam[voice].clear();
                sampleLoopStarts[voice] = 0;
            }
            return;
        }

        seedSample(voice);
    }

    size_t loopStartForExternalSlot(size_t slot) const
    {
        if (slot < externalBrrLoopStarts.size())
            return externalBrrLoopStarts[slot];

        return 0;
    }

    size_t selectedExternalSampleLength() const
    {
        if (selectedExternalBrrSlot < 0 || static_cast<size_t>(selectedExternalBrrSlot) >= externalBrrBank.size())
            return 0;

        return externalBrrBank[static_cast<size_t>(selectedExternalBrrSlot)].size();
    }

    size_t selectedExternalSampleLoopStart() const
    {
        if (selectedExternalBrrSlot < 0)
            return 0;

        return loopStartForExternalSlot(static_cast<size_t>(selectedExternalBrrSlot));
    }

    bool usesExplicitLoopPointControls() const
    {
        return patch.spc700LoopStart > 0.0005f || patch.spc700LoopEnd < 0.9995f;
    }

    size_t resolvedLoopStartForVoice(size_t voice) const
    {
        if (voice >= sampleRam.size() || sampleRam[voice].empty())
            return 0;

        const auto sampleLength = sampleRam[voice].size();
        if (! usesExplicitLoopPointControls())
            return std::min(sampleLoopStarts[voice], sampleLength - 1u);

        const auto start = static_cast<size_t>(std::round(std::clamp(patch.spc700LoopStart, 0.0f, 1.0f) * static_cast<float>(sampleLength - 1u)));
        return std::min(start, sampleLength - 1u);
    }

    size_t resolvedLoopEndForVoice(size_t voice) const
    {
        if (voice >= sampleRam.size() || sampleRam[voice].empty())
            return 0;

        const auto sampleLength = sampleRam[voice].size();
        if (! usesExplicitLoopPointControls())
            return sampleLength;

        auto end = static_cast<size_t>(std::round(std::clamp(patch.spc700LoopEnd, 0.0f, 1.0f) * static_cast<float>(sampleLength - 1u))) + 1u;
        const auto start = resolvedLoopStartForVoice(voice);
        if (end <= start + 1u)
            end = std::min(sampleLength, start + 2u);
        return std::clamp(end, size_t { 1u }, sampleLength);
    }

    double interpolatedSample(size_t voice, double samplePosition) const
    {
        const auto& data = sampleRam[voice];
        if (data.empty())
            return 0.0;

        const auto length = static_cast<int>(data.size());
        const auto base = static_cast<int>(std::floor(samplePosition));
        const auto frac = std::clamp(samplePosition - static_cast<double>(base), 0.0, 0.999999);
        static constexpr auto sigma = 0.58;

        auto weighted = 0.0;
        auto weightSum = 0.0;
        for (int tap = -1; tap <= 2; ++tap)
        {
            const auto wrapped = (base + tap + length) % length;
            const auto distance = static_cast<double>(tap) - frac;
            const auto weight = std::exp(-(distance * distance) / (2.0 * sigma * sigma));
            weighted += data[static_cast<size_t>(wrapped)] * weight;
            weightSum += weight;
        }

        return weightSum > 0.0 ? weighted / weightSum : data[static_cast<size_t>(base % length)];
    }

    double spc700PitchMotionDepthCents() const
    {
        const auto centered = static_cast<double>(patch.control2) - 0.5;
        const auto magnitude = std::max(0.0, std::abs(centered) - 0.08) / 0.42;
        const auto macroScale = patch.macro == MacroKind::laser || patch.macro == MacroKind::powerUp || patch.macro == MacroKind::jump
            ? 2400.0
            : 900.0;
        return std::clamp(magnitude, 0.0, 1.0) * macroScale;
    }

    double spc700PitchModDepthCents() const
    {
        if (spc700PitchMotionDirection() == 0)
            return 0.0;

        return std::clamp(spc700PitchMotionDepthCents() * 0.35, 0.0, 480.0);
    }

    uint8_t spc700PitchModEnabledMask() const
    {
        if (spc700PitchModDepthCents() <= 0.001)
            return 0;

        uint8_t mask = 0;
        for (size_t voice = 1; voice < pitch.size(); ++voice)
        {
            const auto voiceBit = static_cast<uint8_t>(1u << voice);
            const auto previousVoiceBit = static_cast<uint8_t>(1u << (voice - 1u));
            if ((enabledMask & voiceBit) != 0 && (enabledMask & previousVoiceBit) != 0)
                mask |= voiceBit;
        }
        return mask;
    }

    int spc700PitchMotionDirection() const
    {
        const auto centered = static_cast<double>(patch.control2) - 0.5;
        if (std::abs(centered) <= 0.08)
            return 0;
        return centered > 0.0 ? 1 : -1;
    }

    double currentPitchForVoice(size_t voice) const
    {
        const auto basePitch = static_cast<double>(pitch[voice]);
        auto currentPitch = basePitch;
        const auto direction = spc700PitchMotionDirection();
        const auto depthCents = spc700PitchMotionDepthCents();
        if (direction != 0 && depthCents > 0.001)
        {
            const auto seconds = sampleRate > 0.0 ? static_cast<double>(voiceAgeSamples[voice]) / sampleRate : 0.0;
            const auto attack = 1.0 - std::exp(-seconds * 7.0);
            const auto sfxTilt = (patch.macro == MacroKind::laser || patch.macro == MacroKind::hit) ? -1.0 : 1.0;
            const auto signedCents = depthCents * static_cast<double>(direction) * sfxTilt * attack;
            currentPitch = currentPitch * std::pow(2.0, signedCents / 1200.0);
        }

        if (voice > 0 && (spc700PitchModEnabledMask() & static_cast<uint8_t>(1u << voice)) != 0)
        {
            const auto modCents = std::clamp(lastVoiceSample[voice - 1u], -1.0, 1.0) * spc700PitchModDepthCents();
            currentPitch = currentPitch * std::pow(2.0, modCents / 1200.0);
        }

        return std::clamp(currentPitch, 1.0, 16383.0);
    }

    double voicePan(size_t voice) const
    {
        static constexpr std::array<double, 8> panPositions { -1.0, -0.72, -0.44, -0.16, 0.16, 0.44, 0.72, 1.0 };
        if (voice >= panPositions.size())
            return 0.0;

        return panPositions[voice] * std::clamp(static_cast<double>(patch.stereoSpread), 0.0, 1.0);
    }

    double voiceLeftGain(size_t voice) const
    {
        const auto pan = voicePan(voice);
        return pan <= 0.0 ? 1.0 : 1.0 - pan;
    }

    double voiceRightGain(size_t voice) const
    {
        const auto pan = voicePan(voice);
        return pan >= 0.0 ? 1.0 : 1.0 + pan;
    }

    uint8_t voiceLeftVolumeRegister(size_t voice) const
    {
        if (voice >= volume.size())
            return 0;

        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(volume[voice]) * voiceLeftGain(voice))), 0, 127));
    }

    uint8_t voiceRightVolumeRegister(size_t voice) const
    {
        if (voice >= volume.size())
            return 0;

        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(volume[voice]) * voiceRightGain(voice))), 0, 127));
    }

    double voiceEchoSendLevel(size_t voice) const
    {
        if (voice >= patch.sourceLevels.size() || ! sourceEnabled(patch, voice))
            return 0.0;

        return std::clamp(static_cast<double>(patch.sourceLevels[voice]), 0.0, 1.0);
    }

    uint8_t echoEnabledMask() const
    {
        if (spc700EchoSend() <= 0.0)
            return 0;

        uint8_t mask = 0;
        for (size_t voice = 0; voice < volume.size(); ++voice)
        {
            if (voiceEchoSendLevel(voice) > 0.001)
                mask |= static_cast<uint8_t>(1u << voice);
        }
        return mask;
    }

    double echoInputLevel() const
    {
        auto sum = 0.0;
        auto count = 0;
        for (size_t voice = 0; voice < volume.size(); ++voice)
        {
            const auto level = voiceEchoSendLevel(voice);
            if (level > 0.001)
            {
                sum += level;
                ++count;
            }
        }

        return count > 0 ? sum / static_cast<double>(count) : 0.0;
    }

    std::array<int8_t, 8> spc700FirCoefficients() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        if (color <= 0.01)
            return { 127, 0, 0, 0, 0, 0, 0, 0 };

        return {
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(112.0 - (48.0 * color))), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(8.0 + (20.0 * color))), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(4.0 + (12.0 * color))), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(8.0 * color)), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(4.0 * color)), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(-2.0 * color)), -128, 127)),
            0,
            0,
        };
    }

    int spc700FirCoefficientSum() const
    {
        const auto fir = spc700FirCoefficients();
        return std::accumulate(fir.begin(), fir.end(), 0, [] (int sum, int8_t coefficient)
        {
            return sum + static_cast<int>(coefficient);
        });
    }

    double spc700FirFiltered(const std::array<double, 48000>& memory, size_t readIndex) const
    {
        const auto fir = spc700FirCoefficients();
        auto filtered = 0.0;
        for (size_t tap = 0; tap < fir.size(); ++tap)
        {
            const auto index = (readIndex + memory.size() - tap) % memory.size();
            filtered += memory[index] * (static_cast<double>(fir[tap]) / 128.0);
        }
        return std::clamp(filtered, -1.0, 1.0);
    }

    double spc700EchoSend() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        if (color <= 0.01)
            return 0.0;
        return std::clamp(0.08 + (color * 0.42), 0.0, 0.50);
    }

    int8_t spc700EchoLeftVolumeRegister() const
    {
        return static_cast<int8_t>(std::clamp(static_cast<int>(std::round(spc700EchoSend() * 127.0)), -128, 127));
    }

    int8_t spc700EchoRightVolumeRegister() const
    {
        return spc700EchoLeftVolumeRegister();
    }

    double spc700EchoFeedback() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        return std::clamp(0.16 + (color * 0.42), 0.0, 0.58);
    }

    int8_t spc700EchoFeedbackRegister() const
    {
        return static_cast<int8_t>(std::clamp(static_cast<int>(std::round(spc700EchoFeedback() * 127.0)), -128, 127));
    }

    double spc700EchoDelayMs() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        return 32.0 + std::round(color * 13.0) * 16.0;
    }

    uint8_t spc700EchoDelayRegister() const
    {
        const auto edl = static_cast<int>(std::round((spc700EchoDelayMs() - 32.0) / 16.0));
        return static_cast<uint8_t>(std::clamp(edl, 0, 15));
    }

    size_t spc700EchoDelaySamples() const
    {
        const auto requested = static_cast<size_t>(std::max(1.0, std::round(spc700EchoDelayMs() * sampleRate / 1000.0)));
        return std::clamp<size_t>(requested, 1u, echoMemoryLeft.size() - 1u);
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] == midiNote)
                return static_cast<int>(voice);
        }
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] < 0)
                return static_cast<int>(voice);
        }

        auto oldestVoice = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t voice = 0; voice < channelStamp.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelStamp[voice] < oldestStamp)
            {
                oldestStamp = channelStamp[voice];
                oldestVoice = static_cast<int>(voice);
            }
        }
        return oldestVoice;
    }

    int activeChipPolyChannels() const
    {
        int active = 0;
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] >= 0)
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
        enabledMask = 0;
        keyOnMask = 0;
        lastVoiceSample.fill(0.0);
        envelope.fill(0.0);
        envelopeStage.fill(0);
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto voice = selectChipPolyChannel(midiNote);
        if (voice < 0)
            return;

        const auto index = static_cast<size_t>(voice);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;
        triggerVoice(index, channelNotes[index], channelVelocity[index], true);
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (channelNotes[voice] != midiNote)
                continue;

            channelNotes[voice] = -1;
            channelVelocity[voice] = 0.0f;
            channelStamp[voice] = 0;
            releaseVoice(voice);
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 32000.0;
    std::array<uint16_t, 8> pitch {};
    std::array<uint8_t, 8> volume {};
    std::array<uint8_t, 8> adsr {};
    std::array<uint8_t, 8> gain {};
    uint8_t enabledMask = 0;
    uint8_t keyOnMask = 0;
    uint8_t sampleTemplate = 0;
    uint8_t playbackMode = 1;
    bool externalBrrSampleLoaded = false;
    bool externalPcmSampleLoaded = false;
    int selectedExternalBrrSlot = -1;
    size_t brrBlockCount = 0;
    bool brrEndFlagSeen = false;
    bool brrLoopFlagSeen = false;
    size_t lastDecodedBrrLoopStart = 0;
    std::vector<std::vector<double>> externalBrrBank {};
    std::vector<size_t> externalBrrLoopStarts {};
    std::array<std::vector<double>, 8> sampleRam {};
    std::array<size_t, 8> sampleLoopStarts {};
    std::array<double, 8> position {};
    std::array<uint64_t, 8> voiceAgeSamples {};
    std::array<double, 8> lastVoiceSample {};
    std::array<double, 8> envelope {};
    std::array<uint8_t, 8> envelopeStage {};
    std::array<uint16_t, 8> noiseLfsr {};
    std::array<double, 8> noiseAccumulator {};
    std::array<double, 48000> echoMemoryLeft {};
    std::array<double, 48000> echoMemoryRight {};
    size_t echoIndex = 0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 8> channelNotes {};
    std::array<float, 8> channelVelocity {};
    std::array<uint64_t, 8> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

class Ym2413Core final : public ChipCore
{
public:
    explicit Ym2413Core(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
    }

    ~Ym2413Core() override
    {
        if (opll != nullptr)
            OPLL_delete(opll);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;

        if (opll != nullptr)
            OPLL_delete(opll);

        opll = OPLL_new(static_cast<uint32_t>(std::round(clock)), static_cast<uint32_t>(std::round(sampleRate)));
        OPLL_reset(opll);
        OPLL_setChipType(opll, 0);
        OPLL_resetPatch(opll, OPLL_2413_TONE);

        regs.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        currentOutput = {};
        currentPatchNumber.fill(0);
        currentFnum.fill(0);
        currentBlock.fill(0);
        rhythmKeyBits = 0;

        for (size_t channel = 0; channel < 9; ++channel)
            writeOpllRegister(static_cast<uint8_t>(0x30u + channel), 0x0fu);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled
            || ym2413RhythmModeForPatch(nextPatch) != ym2413RhythmModeForPatch(patch))
            clearChipPolyState();
        patch = nextPatch;
        if (! rhythmModeActive())
            writeRhythmRegister(0);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        writeOpllRegister(static_cast<uint8_t>(address & 0x3fu), value);
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        const auto baseVelocity = static_cast<float>(clamp01(velocity));
        auto notes = std::array<int, 9> { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48 };
        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36 }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31, heldNote + 36 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31 }; break;
            case MacroKind::coin:
            case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67, heldNote + 72 }; break;
            case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote + 31, heldNote + 19, heldNote + 7, heldNote - 5, heldNote - 17 }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41, heldNote + 48 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36 }; break;
            case MacroKind::manual:
            default: break;
        }

        const auto rhythmOnlyTemplate = rhythmModeActive() && (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit);
        for (size_t channel = 0; channel < notes.size(); ++channel)
        {
            if (rhythmOnlyTemplate || (rhythmModeActive() && channel >= 6u))
                continue;
            triggerChannel(channel, notes[channel], baseVelocity, sourceEnabled(patch, channel));
        }

        if (rhythmModeActive())
            triggerRhythm(heldNote, baseVelocity);
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
            for (size_t channel = 0; channel < channelNotes.size(); ++channel)
                keyOffChannel(channel);
            writeRhythmRegister(0);
        }
    }

    StereoFrame renderSample() override
    {
        if (opll == nullptr)
            return {};

        if (heldNote >= 0 && patch.macro == MacroKind::laser)
            applyLaserDrift();

        int32_t stereo[2] {};
        OPLL_calcStereo(opll, stereo);
        const auto scale = 1.0 / 8192.0;
        const auto left = std::clamp(static_cast<double>(stereo[0]) * scale, -1.0, 1.0);
        const auto right = std::clamp(static_cast<double>(stereo[1]) * scale, -1.0, 1.0);
        currentOutput = { static_cast<float>(left), static_cast<float>(right) };
        return currentOutput;
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint16_t reg = 0; reg < regs.size(); ++reg)
        {
            if (regs[reg] != 0)
                writes.push_back({ 0, reg, regs[reg] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::ym2413; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "YM2413 / OPLL"; }
    std::string implementedAccuracy() const override { return "verified partial emu2413 register-level"; }
    std::string limitations() const override
    {
        return "MIT-licensed emu2413 provides the OPLL synthesis core for melodic and rhythm output; Chipper currently maps musical controls to preset-instrument, f-number/block, key-on, volume, and $0E rhythm registers. Deep rhythm-kit editing, custom user patches, VRC7/YMF281 variants, and hardware comparison are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2413 / OPLL\","
             << "\"implementedAccuracy\":\"verified partial emu2413 register-level\","
             << "\"vendoredCore\":\"emu2413\","
             << "\"vendoredCoreLicense\":\"MIT\","
             << "\"vendoredCoreCommit\":\"813cff619f5f01c47bd5c0588c1fd8435530b125\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":9,"
             << "\"exposedChannelCount\":9,"
             << "\"instrumentChoice\":" << std::clamp(patch.waveShape, 0, 15) << ","
             << "\"rhythmModeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 2) << ","
             << "\"rhythmMode\":" << (rhythmModeActive() ? 1 : 0) << ","
             << "\"rhythmRegister\":" << static_cast<int>(regs[0x0e]) << ","
             << "\"rhythmKeyBits\":" << static_cast<int>(rhythmKeyBits) << ","
             << "\"rhythmBdVolume\":" << static_cast<int>(regs[0x36] & 0x0fu) << ","
             << "\"rhythmHatVolume\":" << static_cast<int>((regs[0x37] >> 4u) & 0x0fu) << ","
             << "\"rhythmSnareVolume\":" << static_cast<int>(regs[0x37] & 0x0fu) << ","
             << "\"rhythmTomVolume\":" << static_cast<int>((regs[0x38] >> 4u) & 0x0fu) << ","
             << "\"rhythmCymVolume\":" << static_cast<int>(regs[0x38] & 0x0fu) << ","
             << "\"instrument0\":" << static_cast<int>(currentPatchNumber[0]) << ","
             << "\"instrument1\":" << static_cast<int>(currentPatchNumber[1]) << ","
             << "\"instrument2\":" << static_cast<int>(currentPatchNumber[2]) << ","
             << "\"instrument3\":" << static_cast<int>(currentPatchNumber[3]) << ","
             << "\"instrument8\":" << static_cast<int>(currentPatchNumber[8]) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"volume0\":" << static_cast<int>(regs[0x30] & 0x0fu) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"sourceEnabled8\":" << (sourceEnabled(patch, 8) ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"assignedNote6\":" << channelNotes[6] << ","
             << "\"assignedNote7\":" << channelNotes[7] << ","
             << "\"assignedNote8\":" << channelNotes[8] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    struct OpllPitch
    {
        uint16_t fnum = 0;
        uint8_t block = 0;
    };

    uint8_t instrumentForPatch() const
    {
        return ym2413InstrumentForPatch(patch);
    }

    bool rhythmModeActive() const
    {
        return ym2413RhythmModeForPatch(patch) == 2u;
    }

    OpllPitch pitchForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto base = (hz * 524288.0) / (clock / 72.0);
        auto block = 0;
        auto fnum = base;
        while (fnum > 511.0 && block < 7)
        {
            fnum *= 0.5;
            ++block;
        }
        while (fnum < 256.0 && block > 0)
        {
            fnum *= 2.0;
            --block;
        }

        return {
            static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(fnum)), 1, 511)),
            static_cast<uint8_t>(std::clamp(block, 0, 7))
        };
    }

    uint8_t volumeForChannel(size_t channel, float velocity) const
    {
        const auto trim = sourceLevel(patch, channel);
        const auto musicalLevel = clamp01(patch.control4) * clamp01(velocity) * trim;
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0 - musicalLevel) * 15.0)), 0, 15));
    }

    void writeOpllRegister(uint8_t reg, uint8_t value)
    {
        if (opll == nullptr)
            return;

        regs[reg & 0x3fu] = value;
        OPLL_writeReg(opll, reg & 0x3fu, value);
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= 9 || opll == nullptr)
            return;

        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f));
        const auto pitch = pitchForNote(midiNote + detune);
        const auto instrument = instrumentForPatch();
        const auto volume = volumeForChannel(channel, velocity);
        const auto channelReg = static_cast<uint8_t>(channel);

        currentPatchNumber[channel] = instrument;
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;

        writeOpllRegister(static_cast<uint8_t>(0x30u + channelReg), static_cast<uint8_t>((instrument << 4u) | volume));
        writeOpllRegister(static_cast<uint8_t>(0x10u + channelReg), static_cast<uint8_t>(pitch.fnum & 0xffu));
        const auto keyBit = shouldEnable && volume < 15 ? 0x10u : 0x00u;
        const auto reg20 = static_cast<uint8_t>(keyBit | ((pitch.block & 0x07u) << 1u) | ((pitch.fnum >> 8u) & 0x01u));
        writeOpllRegister(static_cast<uint8_t>(0x20u + channelReg), reg20);

        if (keyBit != 0)
            keyOnMask |= static_cast<uint16_t>(1u << channel);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void writeRhythmRegister(uint8_t keyBits)
    {
        rhythmKeyBits = static_cast<uint8_t>(keyBits & 0x1fu);
        writeOpllRegister(0x0eu, rhythmModeActive() ? static_cast<uint8_t>(0x20u | rhythmKeyBits) : 0x00u);
    }

    void writeRhythmPitch(size_t channel, int midiNote)
    {
        if (channel < 6u || channel >= 9u)
            return;

        const auto pitch = pitchForNote(midiNote);
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        writeOpllRegister(static_cast<uint8_t>(0x10u + channel), static_cast<uint8_t>(pitch.fnum & 0xffu));
        writeOpllRegister(static_cast<uint8_t>(0x20u + channel),
                          static_cast<uint8_t>(((pitch.block & 0x07u) << 1u) | ((pitch.fnum >> 8u) & 0x01u)));
    }

    void triggerRhythm(int midiNote, float velocity)
    {
        if (! rhythmModeActive())
            return;

        writeRhythmRegister(0);

        writeRhythmPitch(6, midiNote - 24);
        writeRhythmPitch(7, midiNote + 7);
        writeRhythmPitch(8, midiNote);

        currentPatchNumber[6] = 16;
        currentPatchNumber[7] = 17;
        currentPatchNumber[8] = 18;

        const auto bdVolume = volumeForChannel(6, velocity);
        const auto hhVolume = volumeForChannel(7, velocity);
        const auto sdVolume = volumeForChannel(7, velocity);
        const auto tomVolume = volumeForChannel(8, velocity);
        const auto cymVolume = volumeForChannel(8, velocity);
        writeOpllRegister(0x36u, bdVolume);
        writeOpllRegister(0x37u, static_cast<uint8_t>((hhVolume << 4u) | sdVolume));
        writeOpllRegister(0x38u, static_cast<uint8_t>((tomVolume << 4u) | cymVolume));

        uint8_t keyBits = 0;
        if (sourceEnabled(patch, 6))
            keyBits = static_cast<uint8_t>(keyBits | 0x10u); // BD
        if (sourceEnabled(patch, 7))
            keyBits = static_cast<uint8_t>(keyBits | 0x09u); // HH + SD
        if (sourceEnabled(patch, 8))
            keyBits = static_cast<uint8_t>(keyBits | 0x06u); // TOM + CYM

        writeRhythmRegister(keyBits);
        if ((keyBits & 0x10u) != 0)
            keyOnMask |= static_cast<uint16_t>(1u << 6u);
        if ((keyBits & 0x09u) != 0)
            keyOnMask |= static_cast<uint16_t>(1u << 7u);
        if ((keyBits & 0x06u) != 0)
            keyOnMask |= static_cast<uint16_t>(1u << 8u);
        channelNotes[6] = (keyBits & 0x10u) != 0 ? midiNote : -1;
        channelNotes[7] = (keyBits & 0x09u) != 0 ? midiNote : -1;
        channelNotes[8] = (keyBits & 0x06u) != 0 ? midiNote : -1;
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= 9)
            return;

        const auto reg = static_cast<uint8_t>(0x20u + channel);
        writeOpllRegister(reg, static_cast<uint8_t>(regs[reg] & static_cast<uint8_t>(~0x10u)));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void clearChipPolyState()
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
            keyOffChannel(channel);
        writeRhythmRegister(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
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
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
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
        auto active = 0;
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] >= 0)
                ++active;
        }
        return active;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        if (rhythmModeActive())
        {
            triggerRhythm(std::clamp(midiNote, 0, 127), static_cast<float>(clamp01(velocity)));
            return;
        }

        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        channelNotes[static_cast<size_t>(channel)] = std::clamp(midiNote, 0, 127);
        channelVelocity[static_cast<size_t>(channel)] = static_cast<float>(clamp01(velocity));
        channelStamp[static_cast<size_t>(channel)] = ++noteStamp;
        triggerChannel(static_cast<size_t>(channel), midiNote, velocity, true);
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
            keyOffChannel(channel);
        }

        if (rhythmModeActive())
            writeRhythmRegister(0);
    }

    void applyLaserDrift()
    {
        laserPhase += 1.0 / sampleRate;
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 7.0) * patch.control3 * 8.0));
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if ((keyOnMask & (1u << channel)) == 0)
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, sourceEnabled(patch, channel));
        }
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    OPLL* opll = nullptr;
    PatchConfig patch;
    std::array<uint8_t, 0x40> regs {};
    std::array<uint8_t, 9> currentPatchNumber {};
    std::array<uint16_t, 9> currentFnum {};
    std::array<uint8_t, 9> currentBlock {};
    std::array<int, 9> channelNotes {};
    std::array<float, 9> channelVelocity {};
    std::array<uint64_t, 9> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    uint8_t rhythmKeyBits = 0;
    double laserPhase = 0.0;
    StereoFrame currentOutput {};
};

class Ym2612Core final : public ChipCore
{
public:
    explicit Ym2612Core(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 7670454.0;
        chip = std::make_unique<ymfm::ym2612>(host);
        chip->reset();
        chipSampleRate = static_cast<double>(chip->sample_rate(static_cast<uint32_t>(std::round(clock))));
        sampleAccumulator = 0.0;
        regs.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        currentFnum.fill(0);
        currentBlock.fill(0);
        currentAlgorithm.fill(0);
        currentFeedback.fill(0);
        currentPanBits.fill(0xc0u);
        currentAttackRate.fill(0x1fu);
        currentDecayRate.fill(0x08u);
        currentSustainRate.fill(0x00u);
        currentSustainRelease.fill(0x46u);
        dacSample.clear();
        dacPhase = 0.0;
        dacStep = 1.0;
        dacActive = false;
        dacEnabled = false;
        dacLastValue = 0x80u;
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        currentOutput = {};
        applyPatchToAllChannels(false);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode
            || nextPatch.sourceEnabled != patch.sourceEnabled
            || ym2612DacModeForPatch(nextPatch) != ym2612DacModeForPatch(patch))
            clearChipPolyState();
        patch = nextPatch;
        updateDacEnable(false);
        applyPatchToAllChannels(true);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        writeYmRegister(address & 0x1ffu, value);
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        const auto baseVelocity = static_cast<float>(clamp01(velocity));
        auto notes = std::array<int, 6> { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31 };
        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19 }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19 }; break;
            case MacroKind::coin:
            case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55 }; break;
            case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote - 19, heldNote - 24 }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19 }; break;
            case MacroKind::manual:
            default: break;
        }

        if (dacModeActive() && (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit))
        {
            triggerChannel(5, notes[5], baseVelocity, channelEnabled(5));
            return;
        }

        for (size_t channel = 0; channel < notes.size(); ++channel)
            triggerChannel(channel, notes[channel], baseVelocity, channelEnabled(channel));
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
            for (size_t channel = 0; channel < 6; ++channel)
                keyOffChannel(channel);
        }
    }

    StereoFrame renderSample() override
    {
        if (! chip)
            return {};

        if (! anyAudibleSourceEnabled() && ! dacActive)
        {
            lastNativeLeft = 0;
            lastNativeRight = 0;
            currentOutput = {};
            return currentOutput;
        }

        if (heldNote >= 0 && patch.macro == MacroKind::laser)
            applyLaserDrift();

        const auto ratio = chipSampleRate > 0.0 ? chipSampleRate / sampleRate : 1.0;
        sampleAccumulator += ratio;
        ymfm::ym2612::output_data output;
        auto generated = false;
        while (sampleAccumulator >= 1.0)
        {
            advanceDacPlayback();
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
            sampleAccumulator -= 1.0;
            generated = true;
        }
        if (! generated && ratio >= 0.999)
        {
            advanceDacPlayback();
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
        }

        constexpr auto scale = 1.0 / 32768.0;
        const auto left = std::clamp(static_cast<double>(lastNativeLeft) * scale, -1.0, 1.0);
        const auto right = std::clamp(static_cast<double>(lastNativeRight) * scale, -1.0, 1.0);
        currentOutput = { static_cast<float>(left), static_cast<float>(right) };
        return currentOutput;
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint16_t reg = 0; reg < regs.size(); ++reg)
        {
            if (regs[reg] != 0)
                writes.push_back({ 0, reg, regs[reg] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::ym2612; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "YM2612 / Genesis FM"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPN2 register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YM2612/OPN2 FM synthesis core. Chipper currently maps musical controls and notes to OPN2 operator, algorithm, feedback, f-number/block, $B4 left/right pan bits, key-on registers for all six melodic channels, and optional channel-6 DAC Drum playback through $2B/$2A, with all six source lanes exposed for play and mix control. User PCM import for the DAC, a dedicated operator-grid UI, LFO/AMS/PMS controls, SSG-EG edge cases, timer behavior, and hardware comparison are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2612 / Genesis FM\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPN2 register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"17decfae857b92ab55fbb30ade2287ace095a381\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":6,"
             << "\"uiExposesAllSixVoices\":1,"
             << "\"hiddenChannelsFollowAnySource\":0,"
             << "\"algorithm0\":" << static_cast<int>(currentAlgorithm[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"algorithmFeedbackRegister0\":" << static_cast<int>(regs[0xb0]) << ","
             << "\"panBits0\":" << static_cast<int>(currentPanBits[0]) << ","
             << "\"panBits1\":" << static_cast<int>(currentPanBits[1]) << ","
             << "\"dacMode\":" << static_cast<int>(ym2612DacModeForPatch(patch)) << ","
             << "\"dacEnabled\":" << (dacEnabled ? 1 : 0) << ","
             << "\"dacActive\":" << (dacActive ? 1 : 0) << ","
             << "\"dacRegister2A\":" << static_cast<int>(regs[0x2a]) << ","
             << "\"dacRegister2B\":" << static_cast<int>(regs[0x2b]) << ","
             << "\"dacSampleBytes\":" << dacSample.size() << ","
             << "\"envelopeShape\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"attackRate0\":" << static_cast<int>(currentAttackRate[0]) << ","
             << "\"decayRate0\":" << static_cast<int>(currentDecayRate[0]) << ","
             << "\"sustainRate0\":" << static_cast<int>(currentSustainRate[0]) << ","
             << "\"sustainRelease0\":" << static_cast<int>(currentSustainRelease[0]) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (channelEnabled(4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (channelEnabled(5) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"sourceLevel5\":" << sourceLevel(patch, 5) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"nativeLeft\":" << lastNativeLeft << ","
             << "\"nativeRight\":" << lastNativeRight << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    class Host final : public ymfm::ymfm_interface
    {
    };

    struct OPN2Pitch
    {
        uint16_t fnum = 0;
        uint8_t block = 0;
    };

    static uint8_t keyCodeForChannel(size_t channel)
    {
        return static_cast<uint8_t>(channel < 3 ? channel : channel + 1);
    }

    static uint16_t regForChannel(uint8_t base, size_t channel)
    {
        const auto port = channel >= 3 ? 0x100u : 0x000u;
        const auto local = static_cast<uint16_t>(channel % 3u);
        return static_cast<uint16_t>(port | base | local);
    }

    static uint16_t opRegForChannel(uint8_t base, size_t channel, size_t op)
    {
        static constexpr std::array<uint8_t, 4> opOffsets { 0x00, 0x04, 0x08, 0x0c };
        return static_cast<uint16_t>(regForChannel(static_cast<uint8_t>(base + opOffsets[op]), channel));
    }

    void writeYmRegister(uint16_t reg, uint8_t value)
    {
        regs[reg & 0x1ffu] = value;
        if (! chip)
            return;

        if ((reg & 0x100u) != 0)
        {
            chip->write(2, static_cast<uint8_t>(reg & 0xffu));
            chip->write(3, value);
        }
        else
        {
            chip->write(0, static_cast<uint8_t>(reg & 0xffu));
            chip->write(1, value);
        }
    }

    OPN2Pitch pitchForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto base = (hz * 144.0 * 1048576.0) / clock;
        auto block = 0;
        auto fnum = base;
        while (fnum > 2047.0 && block < 7)
        {
            fnum *= 0.5;
            ++block;
        }
        while (fnum < 512.0 && block > 0)
        {
            fnum *= 2.0;
            --block;
        }
        return {
            static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(fnum)), 1, 2047)),
            static_cast<uint8_t>(std::clamp(block, 0, 7))
        };
    }

    uint8_t algorithmForPatch() const
    {
        if (patch.waveShape > 0)
            return static_cast<uint8_t>(std::clamp(patch.waveShape - 1, 0, 7));

        switch (patch.macro)
        {
            case MacroKind::bass: return 0;
            case MacroKind::lead: return 4;
            case MacroKind::arp: return 5;
            case MacroKind::coin:
            case MacroKind::jump: return 7;
            case MacroKind::drum:
            case MacroKind::hit: return 1;
            case MacroKind::laser: return 2;
            case MacroKind::powerUp: return 6;
            case MacroKind::manual:
            default: break;
        }
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control1 * 7.0f)), 0, 7));
    }

    uint8_t feedbackForPatch() const
    {
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control2 * 7.0f)), 0, 7));
    }

    bool dacModeActive() const
    {
        return ym2612DacModeForPatch(patch) == 2u;
    }

    void updateDacEnable(bool shouldEnable)
    {
        const auto enable = shouldEnable && dacModeActive();
        if (enable == dacEnabled && regs[0x2b] == (enable ? 0x80u : 0x00u))
            return;

        dacEnabled = enable;
        writeYmRegister(0x2b, enable ? 0x80u : 0x00u);
        if (! enable)
            writeYmRegister(0x2a, 0x80u);
    }

    uint8_t multiplierForPatch(size_t op) const
    {
        const auto tone = std::clamp(static_cast<int>(std::round(patch.control3 * 14.0f)) + 1, 1, 15);
        static constexpr std::array<int, 4> offsets { 0, 1, 2, 4 };
        return static_cast<uint8_t>(std::clamp(tone + offsets[op], 1, 15));
    }

    uint8_t totalLevelForOperator(size_t op, float velocity, uint8_t algorithm) const
    {
        const auto level = clamp01(velocity) * clamp01(patch.control4);
        const auto carrierLevel = static_cast<int>(std::round((1.0 - level) * 28.0));
        const auto modulatorLevel = static_cast<int>(std::round(18.0 + (1.0 - clamp01(patch.control3)) * 40.0));
        return static_cast<uint8_t>(std::clamp(fmOperatorIsCarrierForAlgorithm(algorithm, op) ? carrierLevel : modulatorLevel, 0, 127));
    }

    bool channelEnabled(size_t channel) const
    {
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);

        return std::any_of(patch.sourceEnabled.begin(), patch.sourceEnabled.end(), [](bool enabled) { return enabled; });
    }

    bool anyAudibleSourceEnabled() const
    {
        for (size_t channel = 0; channel < 6u; ++channel)
        {
            if (channelEnabled(channel))
                return true;
        }
        return false;
    }

    void applyChannelPatch(size_t channel, float velocity)
    {
        if (channel >= 6)
            return;

        const auto algorithm = algorithmForPatch();
        const auto feedback = feedbackForPatch();
        currentAlgorithm[channel] = algorithm;
        currentFeedback[channel] = feedback;
        currentPanBits[channel] = ym2612PanBitsForPatch(patch, channel);

        for (size_t op = 0; op < 4; ++op)
        {
            const auto envelope = ym2612EnvelopeRegistersForPatch(patch, op);
            writeYmRegister(opRegForChannel(0x30, channel, op), multiplierForPatch(op));
            writeYmRegister(opRegForChannel(0x40, channel, op), totalLevelForOperator(op, velocity, algorithm));
            writeYmRegister(opRegForChannel(0x50, channel, op), envelope.attackRate);
            writeYmRegister(opRegForChannel(0x60, channel, op), envelope.decayRate);
            writeYmRegister(opRegForChannel(0x70, channel, op), envelope.sustainRate);
            writeYmRegister(opRegForChannel(0x80, channel, op), envelope.sustainRelease);
            writeYmRegister(opRegForChannel(0x90, channel, op), 0x00u);
            if (channel == 0 && op == 0)
            {
                currentAttackRate[0] = envelope.attackRate;
                currentDecayRate[0] = envelope.decayRate;
                currentSustainRate[0] = envelope.sustainRate;
                currentSustainRelease[0] = envelope.sustainRelease;
            }
        }

        writeYmRegister(regForChannel(0xb0, channel), static_cast<uint8_t>((feedback << 3u) | algorithm));
        writeYmRegister(regForChannel(0xb4, channel), currentPanBits[channel]);
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        for (size_t channel = 0; channel < 6; ++channel)
            applyChannelPatch(channel, channel < channelVelocity.size() && channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f);

        if (! preserveKeys)
            return;

        for (size_t channel = 0; channel < 6; ++channel)
        {
            if ((keyOnMask & (1u << channel)) != 0 && channelNotes[channel] >= 0)
                triggerChannel(channel, channelNotes[channel], channelVelocity[channel], channelEnabled(channel));
        }
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= 6 || ! chip)
            return;

        if (channel == 5 && dacModeActive())
        {
            triggerDacChannel(midiNote, velocity, shouldEnable);
            return;
        }

        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 8.0f));
        const auto pitch = pitchForNote(midiNote + detune);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity) * sourceLevel(patch, channel));
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        applyChannelPatch(channel, channelVelocity[channel]);

        writeYmRegister(regForChannel(0xa4, channel), static_cast<uint8_t>(((pitch.block & 0x07u) << 3u) | ((pitch.fnum >> 8u) & 0x07u)));
        writeYmRegister(regForChannel(0xa0, channel), static_cast<uint8_t>(pitch.fnum & 0xffu));

        const auto code = keyCodeForChannel(channel);
        writeYmRegister(0x28, static_cast<uint8_t>(shouldEnable ? (0xf0u | code) : code));
        if (shouldEnable)
            keyOnMask |= static_cast<uint16_t>(1u << channel);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= 6)
            return;
        if (channel == 5 && dacModeActive())
        {
            dacActive = false;
            dacSample.clear();
            updateDacEnable(false);
        }
        writeYmRegister(0x28, keyCodeForChannel(channel));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void triggerDacChannel(int midiNote, float velocity, bool shouldEnable)
    {
        channelVelocity[5] = static_cast<float>(clamp01(velocity) * sourceLevel(patch, 5));
        currentPanBits[5] = ym2612PanBitsForPatch(patch, 5);
        writeYmRegister(regForChannel(0xb4, 5), currentPanBits[5]);

        if (! shouldEnable || channelVelocity[5] <= 0.0f)
        {
            dacActive = false;
            dacSample.clear();
            updateDacEnable(false);
            keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
            return;
        }

        currentFnum[5] = 0;
        currentBlock[5] = 0;
        currentAlgorithm[5] = 0;
        currentFeedback[5] = 0;
        dacSample = makeDacDrumSample(midiNote, channelVelocity[5]);
        dacPhase = 0.0;
        const auto playbackHz = std::clamp(9000.0 * std::pow(2.0, (std::clamp(midiNote, 24, 96) - 60) / 24.0), 3500.0, 22000.0);
        dacStep = chipSampleRate > 0.0 ? playbackHz / chipSampleRate : 1.0;
        dacActive = ! dacSample.empty();
        dacLastValue = dacActive ? dacSample.front() : 0x80u;
        updateDacEnable(dacActive);
        writeYmRegister(0x2a, dacLastValue);
        if (dacActive)
            keyOnMask |= static_cast<uint16_t>(1u << 5u);
    }

    std::vector<uint8_t> makeDacDrumSample(int midiNote, float velocity) const
    {
        const auto length = (patch.macro == MacroKind::drum || midiNote < 48) ? 512u : 384u;
        std::vector<uint8_t> sample(length, 0x80u);
        uint32_t noise = 0x6d2b79f5u ^ static_cast<uint32_t>(std::clamp(midiNote, 0, 127) * 1103515245u);
        const auto velocityScale = std::clamp(static_cast<double>(velocity), 0.0, 1.0);
        const auto baseCycles = midiNote < 48 ? 9.0 : 20.0;
        for (size_t i = 0; i < sample.size(); ++i)
        {
            const auto t = static_cast<double>(i) / static_cast<double>(sample.size());
            const auto env = std::exp(-7.0 * t) * velocityScale;
            const auto sweep = baseCycles * (1.0 - 0.72 * t);
            auto value = std::sin(twoPi * sweep * t);

            noise = noise * 1664525u + 1013904223u;
            const auto noiseValue = (static_cast<int>((noise >> 24) & 0xffu) - 128) / 128.0;
            if (patch.macro == MacroKind::hit || midiNote >= 48)
                value = value * 0.45 + noiseValue * 0.55;
            else
                value = value * 0.82 + noiseValue * 0.18;

            const auto byteValue = static_cast<int>(std::round(128.0 + std::clamp(value * env, -1.0, 1.0) * 118.0));
            sample[i] = static_cast<uint8_t>(std::clamp(byteValue, 0, 255));
        }
        return sample;
    }

    void advanceDacPlayback()
    {
        if (! dacActive || ! dacEnabled || dacSample.empty())
            return;

        const auto index = static_cast<size_t>(dacPhase);
        if (index >= dacSample.size())
        {
            dacActive = false;
            dacSample.clear();
            keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
            writeYmRegister(0x2a, 0x80u);
            return;
        }

        const auto value = dacSample[index];
        if (value != dacLastValue)
        {
            dacLastValue = value;
            writeYmRegister(0x2a, value);
        }
        dacPhase += std::max(0.125, dacStep);
    }

    void clearChipPolyState()
    {
        for (size_t channel = 0; channel < 6; ++channel)
            keyOffChannel(channel);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < 6; ++channel)
        {
            if (channelEnabled(channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }
        for (size_t channel = 0; channel < 6; ++channel)
        {
            if (channelEnabled(channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < 6; ++channel)
        {
            if (channelEnabled(channel) && channelStamp[channel] < oldestStamp)
            {
                oldestStamp = channelStamp[channel];
                oldestChannel = static_cast<int>(channel);
            }
        }
        return oldestChannel;
    }

    int activeChipPolyChannels() const
    {
        auto active = 0;
        for (size_t channel = 0; channel < 6; ++channel)
        {
            if (channelEnabled(channel) && channelNotes[channel] >= 0)
                ++active;
        }
        return active;
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
        triggerChannel(index, midiNote, velocity, true);
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < 6; ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            keyOffChannel(channel);
        }
    }

    void applyLaserDrift()
    {
        laserPhase += 1.0 / sampleRate;
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 8.0) * patch.control3 * 10.0));
        for (size_t channel = 0; channel < 6; ++channel)
        {
            if ((keyOnMask & (1u << channel)) == 0)
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, channelEnabled(channel));
        }
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 7670454.0;
    double chipSampleRate = 53267.0;
    double sampleAccumulator = 0.0;
    Host host;
    std::unique_ptr<ymfm::ym2612> chip;
    PatchConfig patch;
    std::array<uint8_t, 0x200> regs {};
    std::array<uint16_t, 6> currentFnum {};
    std::array<uint8_t, 6> currentBlock {};
    std::array<uint8_t, 6> currentAlgorithm {};
    std::array<uint8_t, 6> currentFeedback {};
    std::array<uint8_t, 6> currentPanBits {};
    std::array<uint8_t, 6> currentAttackRate {};
    std::array<uint8_t, 6> currentDecayRate {};
    std::array<uint8_t, 6> currentSustainRate {};
    std::array<uint8_t, 6> currentSustainRelease {};
    std::array<int, 6> channelNotes {};
    std::array<float, 6> channelVelocity {};
    std::array<uint64_t, 6> channelStamp {};
    std::vector<uint8_t> dacSample;
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    double laserPhase = 0.0;
    double dacPhase = 0.0;
    double dacStep = 1.0;
    bool dacActive = false;
    bool dacEnabled = false;
    uint8_t dacLastValue = 0x80u;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    StereoFrame currentOutput {};
};

class Opl3Core final : public ChipCore
{
public:
    explicit Opl3Core(AccuracyMode requested) : accuracy(requested) { channelNotes.fill(-1); }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : ymf262NativeClockHz;
        chip = std::make_unique<ymfm::ymf262>(host);
        chip->reset();
        chipSampleRate = static_cast<double>(chip->sample_rate(static_cast<uint32_t>(std::round(clock))));
        sampleAccumulator = 0.0;
        regs.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        currentFnum.fill(0);
        currentBlock.fill(0);
        currentWaveform.fill(0);
        currentFeedback.fill(0);
        currentCarrierControl.fill(0);
        currentCarrierAttackDecay.fill(0);
        currentCarrierSustainRelease.fill(0);
        rhythmKeyBits = 0;
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        lastNativeLeft = 0;
        lastNativeRight = 0;
        currentOutput = {};
        writeOplRegister(0x105, 0x01);
        writeOplRegister(0x01, 0x20);
        applyPatchToAllChannels(false);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled
            || oplRhythmModeForPatch(nextPatch) != oplRhythmModeForPatch(patch))
            clearChipPolyState();
        patch = nextPatch;
        if (! rhythmModeActive())
            writeRhythmRegister(0);
        applyPatchToAllChannels(true);
    }

    void writeRegister(uint16_t address, uint8_t value) override { writeOplRegister(static_cast<uint16_t>(address & 0x1ffu), value); }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        const auto baseVelocity = static_cast<float>(clamp01(velocity));
        auto notes = std::array<int, 9> { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31, heldNote + 36, heldNote + 43 };
        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31 }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31, heldNote + 36 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31 }; break;
            case MacroKind::coin:
            case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67, heldNote + 72 }; break;
            case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote - 24, heldNote + 19, heldNote + 7, heldNote - 5, heldNote - 17 }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41, heldNote + 48 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 24, heldNote - 17, heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 }; break;
            case MacroKind::manual:
            default: break;
        }

        const auto rhythmOnlyTemplate = rhythmModeActive() && (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit);
        for (size_t channel = 0; channel < notes.size(); ++channel)
        {
            if (rhythmOnlyTemplate || (rhythmModeActive() && channel >= 6u))
                continue;
            triggerChannel(channel, notes[channel], baseVelocity, sourceEnabled(patch, channel));
        }

        if (rhythmModeActive())
            triggerRhythm(heldNote, baseVelocity);
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
            for (size_t channel = 0; channel < channelNotes.size(); ++channel)
                keyOffChannel(channel);
            writeRhythmRegister(0);
        }
    }

    StereoFrame renderSample() override
    {
        if (! chip)
        {
            currentOutput = {};
            return currentOutput;
        }

        if (heldNote >= 0 && patch.macro == MacroKind::laser)
            applyLaserDrift();

        const auto ratio = chipSampleRate > 0.0 ? chipSampleRate / sampleRate : 1.0;
        sampleAccumulator += ratio;
        ymfm::ymf262::output_data output;
        auto generated = false;
        while (sampleAccumulator >= 1.0)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0] + output.data[2];
            lastNativeRight = output.data[1] + output.data[3];
            sampleAccumulator -= 1.0;
            generated = true;
        }
        if (! generated && ratio >= 0.999)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0] + output.data[2];
            lastNativeRight = output.data[1] + output.data[3];
        }

        constexpr auto scale = 1.0 / 65536.0;
        const auto left = std::clamp(static_cast<double>(lastNativeLeft) * scale, -1.0, 1.0);
        const auto right = std::clamp(static_cast<double>(lastNativeRight) * scale, -1.0, 1.0);
        currentOutput = { static_cast<float>(left), static_cast<float>(right) };
        return currentOutput;
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint16_t reg = 0; reg < regs.size(); ++reg)
        {
            if (regs[reg] != 0)
                writes.push_back({ 0, reg, regs[reg] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::opl3; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "OPL2/OPL3 / DOS FM"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPL3/YMF262 register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YMF262/OPL3 synthesis core for this OPL2/OPL3 mode pass. Chipper currently exposes nine OPL2-compatible two-operator lanes and native $BD rhythm-mode key bits while the core runs with OPL3 new mode enabled and high-bank register writes preserved. Full 18-channel UI, four-operator pairs, deep per-operator ADSR UI, rhythm-instrument fine tuning, golden comparisons, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"OPL2/OPL3 / DOS FM\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPL3/YMF262 register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"oplCore\":\"YMF262\","
             << "\"opl3Core\":1,"
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"17decfae857b92ab55fbb30ade2287ace095a381\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":18,"
             << "\"exposedChannelCount\":9,"
             << "\"opl3NewFlag\":" << static_cast<int>(regs[0x105] & 0x01u) << ","
             << "\"opl3NewModeRegister\":" << static_cast<int>(regs[0x105]) << ","
             << "\"waveform0\":" << static_cast<int>(currentWaveform[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"carrierControl0\":" << static_cast<int>(currentCarrierControl[0]) << ","
             << "\"carrierEgt0\":" << (((currentCarrierControl[0] & 0x20u) != 0u) ? 1 : 0) << ","
             << "\"carrierAttackDecay0\":" << static_cast<int>(currentCarrierAttackDecay[0]) << ","
             << "\"carrierSustainRelease0\":" << static_cast<int>(currentCarrierSustainRelease[0]) << ","
             << "\"connectionRegister0\":" << static_cast<int>(regs[0xc0]) << ","
             << "\"opl3OutputSelect0\":" << static_cast<int>(regs[0xc0] & 0xf0u) << ","
             << "\"rhythmModeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 2) << ","
             << "\"rhythmMode\":" << (rhythmModeActive() ? 1 : 0) << ","
             << "\"rhythmRegister\":" << static_cast<int>(regs[0xbd]) << ","
             << "\"rhythmKeyBits\":" << static_cast<int>(rhythmKeyBits) << ","
             << "\"rhythmBdLevel\":" << static_cast<int>(regs[0x53] & 0x3fu) << ","
             << "\"rhythmHatLevel\":" << static_cast<int>(regs[0x51] & 0x3fu) << ","
             << "\"rhythmSnareLevel\":" << static_cast<int>(regs[0x54] & 0x3fu) << ","
             << "\"rhythmTomLevel\":" << static_cast<int>(regs[0x52] & 0x3fu) << ","
             << "\"rhythmCymLevel\":" << static_cast<int>(regs[0x55] & 0x3fu) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"sourceEnabled8\":" << (sourceEnabled(patch, 8) ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"assignedNote6\":" << channelNotes[6] << ","
             << "\"assignedNote7\":" << channelNotes[7] << ","
             << "\"assignedNote8\":" << channelNotes[8] << ","
             << "\"nativeLeft\":" << lastNativeLeft << ","
             << "\"nativeRight\":" << lastNativeRight << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    class Host final : public ymfm::ymfm_interface {};

    struct OplPitch
    {
        uint16_t fnum = 0;
        uint8_t block = 0;
    };

    static uint8_t opOffset(size_t channel, bool carrier)
    {
        static constexpr std::array<uint8_t, 9> base { 0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12 };
        return static_cast<uint8_t>(base[channel % base.size()] + (carrier ? 0x03 : 0x00));
    }

    void writeOplRegister(uint16_t reg, uint8_t value)
    {
        reg = static_cast<uint16_t>(reg & 0x1ffu);
        regs[reg] = value;
        if (! chip)
            return;
        const auto port = reg >= 0x100u ? 2u : 0u;
        chip->write(port, static_cast<uint8_t>(reg & 0xffu));
        chip->write(port + 1u, value);
    }

    OplPitch pitchForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto base = (hz * 72.0 * 1048576.0) / pitchClock();
        auto block = 0;
        auto fnum = base;
        while (fnum > 1023.0 && block < 7)
        {
            fnum *= 0.5;
            ++block;
        }
        while (fnum < 256.0 && block > 0)
        {
            fnum *= 2.0;
            --block;
        }
        return {
            static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(fnum)), 1, 1023)),
            static_cast<uint8_t>(std::clamp(block, 0, 7))
        };
    }

    uint8_t waveformForPatch() const
    {
        if (patch.waveShape > 0)
            return static_cast<uint8_t>(std::clamp(patch.waveShape - 1, 0, 3));

        switch (patch.macro)
        {
            case MacroKind::bass: return 0;
            case MacroKind::lead: return 3;
            case MacroKind::arp: return 1;
            case MacroKind::coin:
            case MacroKind::jump: return 2;
            case MacroKind::drum:
            case MacroKind::hit:
            case MacroKind::laser: return 3;
            case MacroKind::powerUp: return 1;
            case MacroKind::manual:
            default: break;
        }
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control3 * 3.0f)), 0, 3));
    }

    uint8_t feedbackForPatch() const
    {
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control2 * 7.0f)), 0, 7));
    }

    bool rhythmModeActive() const
    {
        return oplRhythmModeForPatch(patch) == 2u;
    }

    void applyChannelPatch(size_t channel, float velocity)
    {
        const auto mod = opOffset(channel, false);
        const auto car = opOffset(channel, true);
        const auto wave = waveformForPatch();
        const auto feedback = feedbackForPatch();
        const auto level = clamp01(velocity) * clamp01(patch.control4);
        const auto modLevel = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(12.0 + (1.0 - clamp01(patch.control3)) * 46.0)), 0, 63));
        const auto carLevel = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0 - level) * 24.0)), 0, 63));

        currentWaveform[channel] = wave;
        currentFeedback[channel] = feedback;

        const auto melodicSustain = patch.macro != MacroKind::drum
            && patch.macro != MacroKind::hit
            && patch.macro != MacroKind::coin
            && patch.macro != MacroKind::jump;
        const auto carrierControl = static_cast<uint8_t>(melodicSustain ? 0x21u : 0x01u);
        const auto attackDecay = static_cast<uint8_t>(0xf4u);
        const auto sustainRelease = static_cast<uint8_t>(melodicSustain ? 0x26u : 0xa6u);
        currentCarrierControl[channel] = carrierControl;
        currentCarrierAttackDecay[channel] = attackDecay;
        currentCarrierSustainRelease[channel] = sustainRelease;

        writeOplRegister(static_cast<uint8_t>(0x20 + mod), static_cast<uint8_t>(0x20 | std::clamp(static_cast<int>(std::round(patch.control3 * 14.0f)) + 1, 1, 15)));
        writeOplRegister(static_cast<uint8_t>(0x20 + car), carrierControl);
        writeOplRegister(static_cast<uint8_t>(0x40 + mod), modLevel);
        writeOplRegister(static_cast<uint8_t>(0x40 + car), carLevel);
        writeOplRegister(static_cast<uint8_t>(0x60 + mod), attackDecay);
        writeOplRegister(static_cast<uint8_t>(0x60 + car), attackDecay);
        writeOplRegister(static_cast<uint8_t>(0x80 + mod), sustainRelease);
        writeOplRegister(static_cast<uint8_t>(0x80 + car), sustainRelease);
        writeOplRegister(static_cast<uint8_t>(0xe0 + mod), wave);
        writeOplRegister(static_cast<uint8_t>(0xe0 + car), wave);
        writeOplRegister(static_cast<uint8_t>(0xc0 + channel), static_cast<uint8_t>(0xf0u | (feedback << 1u) | (patch.control1 > 0.55f ? 1u : 0u)));
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (rhythmModeActive() && channel >= 6u)
                continue;
            applyChannelPatch(channel, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f);
        }

        if (! preserveKeys)
            return;

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if ((keyOnMask & (1u << channel)) != 0 && channelNotes[channel] >= 0)
            {
                if (rhythmModeActive() && channel >= 6u)
                    continue;
                triggerChannel(channel, channelNotes[channel], channelVelocity[channel], sourceEnabled(patch, channel));
            }
        }
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= channelNotes.size() || ! chip)
            return;
        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 6.0f));
        const auto pitch = pitchForNote(midiNote + detune);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity));
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        applyChannelPatch(channel, channelVelocity[channel]);
        writeOplRegister(static_cast<uint8_t>(0xa0 + channel), static_cast<uint8_t>(pitch.fnum & 0xffu));
        writeOplRegister(static_cast<uint8_t>(0xb0 + channel), static_cast<uint8_t>((shouldEnable ? 0x20u : 0x00u) | ((pitch.block & 0x07u) << 2u) | ((pitch.fnum >> 8u) & 0x03u)));
        if (shouldEnable)
            keyOnMask |= static_cast<uint16_t>(1u << channel);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= channelNotes.size())
            return;
        if (rhythmModeActive() && channel >= 6u)
        {
            writeRhythmRegister(0);
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
            return;
        }
        writeOplRegister(static_cast<uint8_t>(0xb0 + channel), static_cast<uint8_t>(regs[0xb0 + channel] & static_cast<uint8_t>(~0x20u)));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    uint8_t rhythmTotalLevel(size_t sourceChannel, float velocity) const
    {
        const auto level = clamp01(patch.control4) * clamp01(velocity) * sourceLevel(patch, sourceChannel);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0 - level) * 42.0)), 0, 63));
    }

    void writeRhythmRegister(uint8_t keyBits)
    {
        rhythmKeyBits = static_cast<uint8_t>(keyBits & 0x1fu);
        writeOplRegister(0xbdu, rhythmModeActive() ? static_cast<uint8_t>(0x20u | rhythmKeyBits) : 0x00u);
    }

    void writeRhythmPitch(size_t channel, int midiNote)
    {
        if (channel < 6u || channel >= channelNotes.size())
            return;
        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 6.0f));
        const auto pitch = pitchForNote(midiNote + detune);
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        writeOplRegister(static_cast<uint8_t>(0xa0 + channel), static_cast<uint8_t>(pitch.fnum & 0xffu));
        writeOplRegister(static_cast<uint8_t>(0xb0 + channel),
                         static_cast<uint8_t>(((pitch.block & 0x07u) << 2u) | ((pitch.fnum >> 8u) & 0x03u)));
    }

    void triggerRhythm(int midiNote, float velocity)
    {
        if (! rhythmModeActive() || ! chip)
            return;

        writeRhythmRegister(0);
        for (size_t channel = 6; channel < channelNotes.size(); ++channel)
            applyChannelPatch(channel, velocity);

        writeRhythmPitch(6, midiNote - 24);
        writeRhythmPitch(7, midiNote + 7);
        writeRhythmPitch(8, midiNote);

        const auto bdLevel = rhythmTotalLevel(6, velocity);
        const auto hatSnareLevel = rhythmTotalLevel(7, velocity);
        const auto tomCymLevel = rhythmTotalLevel(8, velocity);
        writeOplRegister(0x53u, bdLevel);
        writeOplRegister(0x51u, hatSnareLevel);
        writeOplRegister(0x54u, hatSnareLevel);
        writeOplRegister(0x52u, tomCymLevel);
        writeOplRegister(0x55u, tomCymLevel);

        uint8_t keyBits = 0;
        if (sourceEnabled(patch, 6))
            keyBits = static_cast<uint8_t>(keyBits | 0x10u); // Bass drum.
        if (sourceEnabled(patch, 7))
            keyBits = static_cast<uint8_t>(keyBits | 0x09u); // Hi-hat + snare.
        if (sourceEnabled(patch, 8))
            keyBits = static_cast<uint8_t>(keyBits | 0x06u); // Tom + cymbal.

        writeRhythmRegister(keyBits);

        if ((keyBits & 0x10u) != 0)
            keyOnMask |= static_cast<uint16_t>(1u << 6u);
        if ((keyBits & 0x09u) != 0)
            keyOnMask |= static_cast<uint16_t>(1u << 7u);
        if ((keyBits & 0x06u) != 0)
            keyOnMask |= static_cast<uint16_t>(1u << 8u);

        channelNotes[6] = (keyBits & 0x10u) != 0 ? midiNote : -1;
        channelNotes[7] = (keyBits & 0x09u) != 0 ? midiNote : -1;
        channelNotes[8] = (keyBits & 0x06u) != 0 ? midiNote : -1;
    }

    void clearChipPolyState()
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
            keyOffChannel(channel);
        writeRhythmRegister(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
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
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
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
        auto active = 0;
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] >= 0)
                ++active;
        }
        return active;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        if (rhythmModeActive())
        {
            triggerRhythm(std::clamp(midiNote, 0, 127), static_cast<float>(clamp01(velocity)));
            return;
        }

        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;
        const auto index = static_cast<size_t>(channel);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;
        triggerChannel(index, midiNote, velocity, true);
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
            keyOffChannel(channel);
        }

        if (rhythmModeActive())
            writeRhythmRegister(0);
    }

    void applyLaserDrift()
    {
        laserPhase += 1.0 / sampleRate;
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 9.0) * patch.control3 * 8.0));
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (rhythmModeActive() && channel >= 6u)
                continue;
            if ((keyOnMask & (1u << channel)) == 0)
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, sourceEnabled(patch, channel));
        }
    }

    double pitchClock() const
    {
        return clock > 8000000.0 ? clock * 0.25 : clock;
    }

    static constexpr double ymf262NativeClockHz = 14318180.0;

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = ymf262NativeClockHz;
    double chipSampleRate = 49716.0;
    double sampleAccumulator = 0.0;
    Host host;
    std::unique_ptr<ymfm::ymf262> chip;
    PatchConfig patch;
    std::array<uint8_t, 0x200> regs {};
    std::array<uint16_t, 9> currentFnum {};
    std::array<uint8_t, 9> currentBlock {};
    std::array<uint8_t, 9> currentWaveform {};
    std::array<uint8_t, 9> currentFeedback {};
    std::array<uint8_t, 9> currentCarrierControl {};
    std::array<uint8_t, 9> currentCarrierAttackDecay {};
    std::array<uint8_t, 9> currentCarrierSustainRelease {};
    std::array<int, 9> channelNotes {};
    std::array<float, 9> channelVelocity {};
    std::array<uint64_t, 9> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    uint8_t rhythmKeyBits = 0;
    double laserPhase = 0.0;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    StereoFrame currentOutput {};
};

class Ym2151Core final : public ChipCore
{
public:
    explicit Ym2151Core(AccuracyMode requested) : accuracy(requested) { channelNotes.fill(-1); }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        chip = std::make_unique<ymfm::ym2151>(host);
        chip->reset();
        chipSampleRate = static_cast<double>(chip->sample_rate(static_cast<uint32_t>(std::round(clock))));
        sampleAccumulator = 0.0;
        regs.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        currentKeyCode.fill(0);
        currentKeyFraction.fill(0);
        currentAlgorithm.fill(0);
        currentFeedback.fill(0);
        currentPanBits.fill(0xc0u);
        currentNoiseRegister = 0;
        currentAttackRate.fill(0);
        currentDecayRate.fill(0);
        currentSustainRate.fill(0);
        currentSustainRelease.fill(0);
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        lastNativeLeft = 0;
        lastNativeRight = 0;
        currentOutput = {};
        applyPatchToAllChannels(false);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();
        patch = nextPatch;
        applyPatchToAllChannels(true);
    }

    void writeRegister(uint16_t address, uint8_t value) override { writeOpmRegister(static_cast<uint8_t>(address & 0xffu), value); }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        const auto baseVelocity = static_cast<float>(clamp01(velocity));
        auto notes = std::array<int, 8> { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 };
        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31 }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28 }; break;
            case MacroKind::coin:
            case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67 }; break;
            case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote + 31, heldNote + 19, heldNote + 7, heldNote - 5 }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31 }; break;
            case MacroKind::manual:
            default: break;
        }

        for (size_t channel = 0; channel < notes.size(); ++channel)
            triggerChannel(channel, notes[channel], baseVelocity, sourceEnabled(patch, channel));
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
            for (size_t channel = 0; channel < channelNotes.size(); ++channel)
                keyOffChannel(channel);
        }
    }

    StereoFrame renderSample() override
    {
        if (! chip)
        {
            currentOutput = {};
            return currentOutput;
        }

        if (heldNote >= 0 && patch.macro == MacroKind::laser)
            applyLaserDrift();

        const auto ratio = chipSampleRate > 0.0 ? chipSampleRate / sampleRate : 1.0;
        sampleAccumulator += ratio;
        ymfm::ym2151::output_data output;
        auto generated = false;
        while (sampleAccumulator >= 1.0)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
            sampleAccumulator -= 1.0;
            generated = true;
        }
        if (! generated && ratio >= 0.999)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
        }

        constexpr auto scale = 1.0 / 32768.0;
        const auto left = std::clamp(static_cast<double>(lastNativeLeft) * scale, -1.0, 1.0);
        const auto right = std::clamp(static_cast<double>(lastNativeRight) * scale, -1.0, 1.0);
        currentOutput = { static_cast<float>(left), static_cast<float>(right) };
        return currentOutput;
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint16_t reg = 0; reg < regs.size(); ++reg)
        {
            if (regs[reg] != 0)
                writes.push_back({ 0, reg, regs[reg] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::ym2151; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "YM2151 arcade/X68000 FM"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPM register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YM2151/OPM synthesis core. Chipper currently maps musical controls and notes to OPM operator, algorithm, feedback, key-code/key-fraction, pan, $0F channel-8 noise, and key-on registers for all eight melodic lanes. LFO PM/AM controls, exact OPM noise timing/hardware comparison, timers, CSM behavior, deep per-operator ADSR UI, golden comparisons, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2151 arcade/X68000 FM\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPM register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"17decfae857b92ab55fbb30ade2287ace095a381\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":8,"
             << "\"exposedChannelCount\":8,"
             << "\"algorithm0\":" << static_cast<int>(currentAlgorithm[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"panBits0\":" << static_cast<int>(currentPanBits[0]) << ","
             << "\"panBits1\":" << static_cast<int>(currentPanBits[1]) << ","
             << "\"noiseRegister\":" << static_cast<int>(currentNoiseRegister) << ","
             << "\"noiseEnabled\":" << (((currentNoiseRegister & 0x80u) != 0u) ? 1 : 0) << ","
             << "\"noiseFrequency\":" << static_cast<int>(currentNoiseRegister & 0x1fu) << ","
             << "\"envelopeShape\":" << patch.ymEnvelopeShape << ","
             << "\"attackRate0\":" << static_cast<int>(currentAttackRate[0]) << ","
             << "\"decayRate0\":" << static_cast<int>(currentDecayRate[0]) << ","
             << "\"sustainRate0\":" << static_cast<int>(currentSustainRate[0]) << ","
             << "\"sustainRelease0\":" << static_cast<int>(currentSustainRelease[0]) << ","
             << "\"algorithmFeedbackRegister0\":" << static_cast<int>(regs[0x20]) << ","
             << "\"keyCode0\":" << static_cast<int>(currentKeyCode[0]) << ","
             << "\"keyFraction0\":" << static_cast<int>(currentKeyFraction[0]) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"assignedNote6\":" << channelNotes[6] << ","
             << "\"assignedNote7\":" << channelNotes[7] << ","
             << "\"nativeLeft\":" << lastNativeLeft << ","
             << "\"nativeRight\":" << lastNativeRight << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    class Host final : public ymfm::ymfm_interface {};

    struct OpmPitch
    {
        uint8_t keyCode = 0;
        uint8_t keyFraction = 0;
    };

    void writeOpmRegister(uint8_t reg, uint8_t value)
    {
        regs[reg] = value;
        if (! chip)
            return;
        chip->write(0, reg);
        chip->write(1, value);
    }

    static uint8_t opOffset(size_t channel, size_t op)
    {
        static constexpr std::array<uint8_t, 4> opOffsets { 0x00, 0x08, 0x10, 0x18 };
        return static_cast<uint8_t>((channel & 0x07u) + opOffsets[op % opOffsets.size()]);
    }

    OpmPitch pitchForNote(int midiNote) const
    {
        static constexpr std::array<uint8_t, 12> noteCodes { 0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14 };
        const auto clamped = std::clamp(midiNote, 0, 127);
        const auto octave = std::clamp((clamped / 12) - 1, 0, 7);
        const auto note = clamped % 12;
        return { static_cast<uint8_t>((octave << 4u) | noteCodes[static_cast<size_t>(note)]), 0 };
    }

    uint8_t algorithmForPatch() const
    {
        if (patch.waveShape > 0)
            return static_cast<uint8_t>(std::clamp(patch.waveShape - 1, 0, 7));

        switch (patch.macro)
        {
            case MacroKind::bass: return 0;
            case MacroKind::lead: return 4;
            case MacroKind::arp: return 7;
            case MacroKind::coin:
            case MacroKind::jump: return 6;
            case MacroKind::drum:
            case MacroKind::hit: return 2;
            case MacroKind::laser: return 3;
            case MacroKind::powerUp: return 5;
            case MacroKind::manual:
            default: break;
        }
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control1 * 7.0f)), 0, 7));
    }

    uint8_t feedbackForPatch() const
    {
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control2 * 7.0f)), 0, 7));
    }

    uint8_t multipleForOperator(size_t op) const
    {
        static constexpr std::array<int, 4> offsets { 0, 1, 3, 5 };
        const auto base = std::clamp(static_cast<int>(std::round(patch.control3 * 13.0f)) + 1, 1, 15);
        return static_cast<uint8_t>(std::clamp(base + offsets[op], 1, 15));
    }

    uint8_t totalLevelForOperator(size_t op, float velocity, uint8_t algorithm) const
    {
        const auto level = clamp01(velocity) * clamp01(patch.control4);
        const auto carrier = static_cast<int>(std::round((1.0 - level) * 24.0));
        const auto modulator = static_cast<int>(std::round(16.0 + (1.0 - clamp01(patch.control3)) * 52.0));
        return static_cast<uint8_t>(std::clamp(fmOperatorIsCarrierForAlgorithm(algorithm, op) ? carrier : modulator, 0, 127));
    }

    void applyChannelPatch(size_t channel, float velocity)
    {
        if (channel >= 8)
            return;

        const auto algorithm = algorithmForPatch();
        const auto feedback = feedbackForPatch();
        currentAlgorithm[channel] = algorithm;
        currentFeedback[channel] = feedback;
        currentPanBits[channel] = ym2151PanBitsForPatch(patch, channel);

        for (size_t op = 0; op < 4; ++op)
        {
            const auto offs = opOffset(channel, op);
            const auto envelope = ym2612EnvelopeRegistersForPatch(patch, op);
            if (op == 0)
            {
                currentAttackRate[channel] = envelope.attackRate;
                currentDecayRate[channel] = envelope.decayRate;
                currentSustainRate[channel] = envelope.sustainRate;
                currentSustainRelease[channel] = envelope.sustainRelease;
            }
            writeOpmRegister(static_cast<uint8_t>(0x40 + offs), multipleForOperator(op));
            writeOpmRegister(static_cast<uint8_t>(0x60 + offs), totalLevelForOperator(op, velocity, algorithm));
            writeOpmRegister(static_cast<uint8_t>(0x80 + offs), envelope.attackRate);
            writeOpmRegister(static_cast<uint8_t>(0xa0 + offs), envelope.decayRate);
            writeOpmRegister(static_cast<uint8_t>(0xc0 + offs), envelope.sustainRate);
            writeOpmRegister(static_cast<uint8_t>(0xe0 + offs), envelope.sustainRelease);
        }

        writeOpmRegister(static_cast<uint8_t>(0x20 + channel), static_cast<uint8_t>(currentPanBits[channel] | (feedback << 3u) | algorithm));
        writeOpmRegister(static_cast<uint8_t>(0x38 + channel), 0x00u);
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        currentNoiseRegister = ym2151NoiseRegisterForPatch(patch);
        writeOpmRegister(0x0f, currentNoiseRegister);

        for (size_t channel = 0; channel < 8; ++channel)
        {
            const auto velocity = channel < channelVelocity.size() && channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f;
            applyChannelPatch(channel, velocity * static_cast<float>(sourceLevel(patch, channel)));
        }

        if (! preserveKeys)
            return;

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if ((keyOnMask & (1u << channel)) != 0 && channelNotes[channel] >= 0)
                triggerChannel(channel, channelNotes[channel], channelVelocity[channel], sourceEnabled(patch, channel));
        }
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= channelNotes.size() || ! chip)
            return;
        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 8.0f));
        const auto pitch = pitchForNote(midiNote + detune);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity));
        currentKeyCode[channel] = pitch.keyCode;
        currentKeyFraction[channel] = pitch.keyFraction;
        applyChannelPatch(channel, channelVelocity[channel] * static_cast<float>(sourceLevel(patch, channel)));
        writeOpmRegister(static_cast<uint8_t>(0x28 + channel), pitch.keyCode);
        writeOpmRegister(static_cast<uint8_t>(0x30 + channel), pitch.keyFraction);
        writeOpmRegister(0x08, static_cast<uint8_t>((shouldEnable ? 0x78u : 0x00u) | (channel & 0x07u)));
        if (shouldEnable)
            keyOnMask |= static_cast<uint16_t>(1u << channel);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= channelNotes.size())
            return;
        writeOpmRegister(0x08, static_cast<uint8_t>(channel & 0x07u));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void clearChipPolyState()
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
            keyOffChannel(channel);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
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
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
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
        auto active = 0;
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] >= 0)
                ++active;
        }
        return active;
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
        triggerChannel(index, midiNote, velocity, true);
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
            keyOffChannel(channel);
        }
    }

    void applyLaserDrift()
    {
        laserPhase += 1.0 / sampleRate;
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 8.0) * patch.control3 * 9.0));
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if ((keyOnMask & (1u << channel)) == 0)
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, sourceEnabled(patch, channel));
        }
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    double chipSampleRate = 55930.0;
    double sampleAccumulator = 0.0;
    Host host;
    std::unique_ptr<ymfm::ym2151> chip;
    PatchConfig patch;
    std::array<uint8_t, 0x100> regs {};
    std::array<uint8_t, 8> currentKeyCode {};
    std::array<uint8_t, 8> currentKeyFraction {};
    std::array<uint8_t, 8> currentAlgorithm {};
    std::array<uint8_t, 8> currentFeedback {};
    std::array<uint8_t, 8> currentPanBits {};
    uint8_t currentNoiseRegister = 0;
    std::array<uint8_t, 8> currentAttackRate {};
    std::array<uint8_t, 8> currentDecayRate {};
    std::array<uint8_t, 8> currentSustainRate {};
    std::array<uint8_t, 8> currentSustainRelease {};
    std::array<int, 8> channelNotes {};
    std::array<float, 8> channelVelocity {};
    std::array<uint64_t, 8> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    double laserPhase = 0.0;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    StereoFrame currentOutput {};
};

class PaulaCore final : public ChipCore
{
public:
    explicit PaulaCore(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3546895.0;
        period.fill(428);
        volume.fill(0);
        control.fill(0);
        position.fill(0.0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        sampleTemplate.fill(0);
        leftA500State = 0.0;
        rightA500State = 0.0;
        leftLedState = 0.0;
        rightLedState = 0.0;
        for (size_t channel = 0; channel < sampleRam.size(); ++channel)
            seedSample(channel);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        const auto sampleSlotsChanged = nextPatch.spc700VoiceSampleSlots != patch.spc700VoiceSampleSlots;
        patch = nextPatch;
        for (size_t channel = 0; channel < sampleRam.size(); ++channel)
        {
            const auto nextTemplate = resolvedSampleTemplate(channel);
            if (nextTemplate != sampleTemplate[channel] || sampleSlotsChanged)
            {
                sampleTemplate[channel] = nextTemplate;
                seedSample(channel);
            }
        }
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        std::vector<std::vector<uint8_t>> bank;
        if (! data.empty())
            bank.push_back(std::move(data));
        const auto selectedSlot = bank.empty() ? -1 : 0;
        setExternalSampleBank(std::move(bank), selectedSlot);
    }

    void setExternalSampleBank(std::vector<std::vector<uint8_t>> bank, int selectedSlot) override
    {
        externalSampleBank.clear();
        externalSampleBank.reserve(bank.size());
        for (const auto& bytes : bank)
        {
            std::vector<double> decoded;
            decoded.reserve(bytes.size());
            for (const auto byte : bytes)
                decoded.push_back((static_cast<double>(byte) - 128.0) / 128.0);

            if (! decoded.empty())
                externalSampleBank.push_back(std::move(decoded));
        }

        setExternalSampleSlot(selectedSlot);
    }

    void setExternalSampleSlot(int selectedSlot) override
    {
        if (externalSampleBank.empty())
        {
            externalSampleSlot = -1;
            for (size_t channel = 0; channel < sampleRam.size(); ++channel)
                seedSample(channel);
            return;
        }

        externalSampleSlot = selectedSlot >= 0 && selectedSlot < static_cast<int>(externalSampleBank.size()) ? selectedSlot : -1;
        for (size_t channel = 0; channel < sampleRam.size(); ++channel)
            seedSample(channel);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        const auto channel = static_cast<size_t>((reg >> 4u) & 3u);
        const auto offset = reg & 0x0fu;

        switch (offset)
        {
            case 0: period[channel] = static_cast<uint16_t>((period[channel] & 0xff00u) | value); break;
            case 1: period[channel] = static_cast<uint16_t>((period[channel] & 0x00ffu) | (static_cast<uint16_t>(value) << 8u)); break;
            case 2: volume[channel] = static_cast<uint8_t>(std::min<int>(64, value & 0x7fu)); break;
            case 3: control[channel] = value & 0x03u; break;
            case 4:
                sampleTemplate[channel] = value % 5u;
                seedSample(channel);
                break;
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

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        auto notes = std::array<int, 4> { heldNote, heldNote + std::max(1, spread / 3), heldNote + std::max(2, (spread * 2) / 3), heldNote + std::max(3, spread) };

        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7 }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 19 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7 }; break;
            case MacroKind::coin: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 }; break;
            case MacroKind::laser:
                notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12 };
                break;
            case MacroKind::jump: notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19 }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17 }; break;
            case MacroKind::manual:
            default: break;
        }

        for (size_t channel = 0; channel < period.size(); ++channel)
            triggerChannel(channel, notes[channel], noteVelocity, channelActiveForPatch(channel));
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
            for (auto& ctrl : control)
                ctrl &= static_cast<uint8_t>(~0x01u);
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 4> panPositions { -1.0, 1.0, 1.0, -1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            const auto sample = renderChannel(channel);
            if (std::abs(sample) <= 1.0e-9)
                continue;

            ++audibleCount;
            const auto spread = std::clamp(static_cast<double>(patch.stereoSpread), 0.0, 1.0);
            const auto pan = panPositions[channel] * spread;
            left += sample * (pan <= 0.0 ? 1.0 : 1.0 - pan);
            right += sample * (pan >= 0.0 ? 1.0 : 1.0 + pan);
        }

        if (audibleCount > 0)
        {
            left /= static_cast<double>(audibleCount);
            right /= static_cast<double>(audibleCount);
        }

        if (patch.envelopeDecay > 0.001f && noteVelocity > 0.0f)
        {
            const auto decay = 1.0 - static_cast<double>(patch.envelopeDecay) * 0.00035;
            noteVelocity = static_cast<float>(std::max(0.0, static_cast<double>(noteVelocity) * decay));
        }

        applyOutputFilter(left, right);

        return { static_cast<float>(std::clamp(left, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t channel = 0; channel < period.size(); ++channel)
        {
            const auto base = static_cast<uint16_t>(channel * 0x10u);
            writes.push_back({ 0, base, static_cast<uint8_t>(period[channel] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>((period[channel] >> 8u) & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 2u), volume[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 3u), control[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 4u), sampleTemplate[channel] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::paula; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    std::string modeName() const override { return "Amiga Paula"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Four Paula-inspired 8-bit sample channels, period, volume, enable/loop control, classic hard-panned channel layout, generated tracker sample templates, and plugin/renderer-loaded WAV/AIFF/uncompressed IFF-8SVX sample banks are modeled; DMA pointer timing, exact PAL/NTSC video timing, compressed 8SVX, MOD import, loop-point metadata, mod playback effects, CIA timing, analog output path, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Amiga Paula\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << static_cast<int>(sampleTemplate[0]) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(sampleTemplate[0]) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(sampleTemplate[1]) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(sampleTemplate[2]) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(sampleTemplate[3]) << ","
             << "\"period0\":" << period[0] << ","
             << "\"period1\":" << period[1] << ","
             << "\"period2\":" << period[2] << ","
             << "\"period3\":" << period[3] << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"control0\":" << static_cast<int>(control[0]) << ","
             << "\"control1\":" << static_cast<int>(control[1]) << ","
             << "\"control2\":" << static_cast<int>(control[2]) << ","
             << "\"control3\":" << static_cast<int>(control[3]) << ","
             << "\"outputFilterMode\":" << static_cast<int>(paulaOutputFilterModeForPatch(patch)) << ","
             << "\"nativeInterpolation\":\"nearest\"," 
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sampleLength0\":" << sampleRam[0].size() << ","
             << "\"externalSampleLoaded\":" << (externalSampleBank.empty() ? 0 : 1) << ","
             << "\"externalSampleBankCount\":" << externalSampleBank.size() << ","
             << "\"externalSampleSlot\":" << externalSampleSlot << ","
             << "\"voiceSampleSlot0\":" << patch.spc700VoiceSampleSlots[0] << ","
             << "\"voiceSampleSlot1\":" << patch.spc700VoiceSampleSlots[1] << ","
             << "\"voiceSampleSlot2\":" << patch.spc700VoiceSampleSlots[2] << ","
             << "\"voiceSampleSlot3\":" << patch.spc700VoiceSampleSlots[3] << ","
             << "\"resolvedExternalSampleSlot0\":" << resolvedExternalSampleSlot[0] << ","
             << "\"resolvedExternalSampleSlot1\":" << resolvedExternalSampleSlot[1] << ","
             << "\"resolvedExternalSampleSlot2\":" << resolvedExternalSampleSlot[2] << ","
             << "\"resolvedExternalSampleSlot3\":" << resolvedExternalSampleSlot[3] << ","
             << "\"internalChannelCount\":4,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t paulaPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (2.0 * hz)), 113.0, 65535.0));
    }

    uint8_t channelVolumeForPatch(size_t channel, float velocity) const
    {
        const auto trim = sourceLevel(patch, channel);
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 64.0f)), 1, 64);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(base) * trim * clamp01(velocity))), 0, 64));
    }

    bool channelActiveForPatch(size_t channel) const
    {
        return sourceEnabled(patch, channel);
    }

    bool loopForPatch() const
    {
        return paulaLoopForPatch(patch);
    }

    uint8_t resolvedSampleTemplate(size_t channel) const
    {
        return wavetableWaveShapeForChannel(ChipMode::paula, patch, channel);
    }

    void seedSample(size_t channel)
    {
        if (channel < resolvedExternalSampleSlot.size())
            resolvedExternalSampleSlot[channel] = -1;

        if (! externalSampleBank.empty())
        {
            auto selectedSlot = externalSampleSlot;
            if (channel < patch.spc700VoiceSampleSlots.size() && patch.spc700VoiceSampleSlots[channel] > 0)
                selectedSlot = patch.spc700VoiceSampleSlots[channel] - 1;

            if (selectedSlot >= 0 && selectedSlot < static_cast<int>(externalSampleBank.size()))
            {
                sampleRam[channel] = externalSampleBank[static_cast<size_t>(selectedSlot)];
                if (channel < resolvedExternalSampleSlot.size())
                    resolvedExternalSampleSlot[channel] = selectedSlot;
            }
            else
                sampleRam[channel].clear();
            return;
        }

        auto data = std::vector<double>(64, 0.0);
        const auto choice = sampleTemplate[channel] == 0 ? resolvedSampleTemplate(channel) : sampleTemplate[channel];
        for (size_t i = 0; i < data.size(); ++i)
        {
            const auto phaseValue = static_cast<double>(i) / static_cast<double>(data.size());
            double sample = 0.0;
            switch (choice)
            {
                case 1: sample = (phaseValue * 2.0) - 1.0; break;
                case 2: sample = phaseValue < 0.5 ? (phaseValue * 4.0) - 1.0 : 3.0 - (phaseValue * 4.0); break;
                case 3: sample = std::sin(twoPi * phaseValue); break;
                case 4:
                {
                    const auto hash = (static_cast<int>(i) * 1103515245u + static_cast<int>(channel) * 12345u) & 0xffffu;
                    const auto burst = std::exp(-phaseValue * 6.0);
                    sample = ((static_cast<double>(hash) / 32767.5) - 1.0) * burst;
                    break;
                }
                default: sample = phaseValue < 0.5 ? 0.85 : -0.85; break;
            }
            data[i] = std::clamp(std::round(sample * 127.0) / 127.0, -1.0, 1.0);
        }
        sampleRam[channel] = std::move(data);
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        period[channel] = paulaPeriodForNote(midiNote + static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f)));
        volume[channel] = channelVolumeForPatch(channel, velocity);
        control[channel] = static_cast<uint8_t>((shouldEnable && volume[channel] > 0 ? 0x01u : 0x00u) | (loopForPatch() ? 0x02u : 0x00u));
        position[channel] = 0.0;
        seedSample(channel);
    }

    double renderChannel(size_t channel)
    {
        if ((control[channel] & 0x01u) == 0 || sampleRam[channel].empty() || volume[channel] == 0)
            return 0.0;

        const auto index = static_cast<size_t>(position[channel]) % sampleRam[channel].size();
        const auto sample = sampleRam[channel][index] * (static_cast<double>(volume[channel]) / 64.0);
        const auto playbackHz = clock / (2.0 * static_cast<double>(std::max<uint16_t>(113, period[channel])));
        position[channel] += (playbackHz * static_cast<double>(sampleRam[channel].size())) / sampleRate;

        if (position[channel] >= static_cast<double>(sampleRam[channel].size()))
        {
            if ((control[channel] & 0x02u) != 0)
                position[channel] = std::fmod(position[channel], static_cast<double>(sampleRam[channel].size()));
            else
                control[channel] &= static_cast<uint8_t>(~0x01u);
        }

        return sample;
    }

    void applyOutputFilter(double& left, double& right)
    {
        const auto mode = paulaOutputFilterModeForPatch(patch);
        if (mode == 1u)
            return;

        if (mode == 2u || mode == 4u)
        {
            left = onePoleLowPass(left, leftA500State, 26000.0);
            right = onePoleLowPass(right, rightA500State, 26000.0);
        }

        if (mode == 3u || mode == 4u)
        {
            left = onePoleLowPass(left, leftLedState, 7000.0);
            right = onePoleLowPass(right, rightLedState, 7000.0);
        }
    }

    double onePoleLowPass(double input, double& state, double cutoffHz) const
    {
        const auto safeRate = std::max(1000.0, sampleRate);
        const auto safeCutoff = std::clamp(cutoffHz, 20.0, safeRate * 0.45);
        const auto alpha = 1.0 - std::exp((-twoPi * safeCutoff) / safeRate);
        state += alpha * (input - state);
        return state;
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
        for (auto& ctrl : control)
            ctrl &= static_cast<uint8_t>(~0x01u);
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
        triggerChannel(index, channelNotes[index], channelVelocity[index], true);
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
            control[channel] &= static_cast<uint8_t>(~0x01u);
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3546895.0;
    std::array<uint16_t, 4> period {};
    std::array<uint8_t, 4> volume {};
    std::array<uint8_t, 4> control {};
    std::array<uint8_t, 4> sampleTemplate {};
    std::array<std::vector<double>, 4> sampleRam {};
    std::vector<std::vector<double>> externalSampleBank;
    int externalSampleSlot = -1;
    std::array<int, 4> resolvedExternalSampleSlot { -1, -1, -1, -1 };
    std::array<double, 4> position {};
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 4> channelNotes {};
    std::array<float, 4> channelVelocity {};
    std::array<uint64_t, 4> channelStamp {};
    uint64_t noteStamp = 0;
    double leftA500State = 0.0;
    double rightA500State = 0.0;
    double leftLedState = 0.0;
    double rightLedState = 0.0;
    PatchConfig patch;
};

} // namespace

std::unique_ptr<ChipCore> createChipCore(ChipMode mode, AccuracyMode accuracy)
{
    switch (mode)
    {
        case ChipMode::nes: return std::make_unique<NesApuCore>(accuracy);
        case ChipMode::dmg: return std::make_unique<DmgApuCore>(accuracy);
        case ChipMode::sid: return std::make_unique<SidCore>(accuracy);
        case ChipMode::ym2149: return std::make_unique<Ym2149Core>(accuracy);
        case ChipMode::sn76489: return std::make_unique<Sn76489Core>(accuracy);
        case ChipMode::ym2612: return std::make_unique<Ym2612Core>(accuracy);
        case ChipMode::opl3: return std::make_unique<Opl3Core>(accuracy);
        case ChipMode::ym2151: return std::make_unique<Ym2151Core>(accuracy);
        case ChipMode::spc700: return std::make_unique<Spc700Core>(accuracy);
        case ChipMode::pokey: return std::make_unique<PokeyCore>(accuracy);
        case ChipMode::paula: return std::make_unique<PaulaCore>(accuracy);
        case ChipMode::huc6280: return std::make_unique<Huc6280Core>(accuracy);
        case ChipMode::namcoWsg: return std::make_unique<NamcoWsgCore>(accuracy);
        case ChipMode::ym2413: return std::make_unique<Ym2413Core>(accuracy);
        case ChipMode::scc: return std::make_unique<SccCore>(accuracy);
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
