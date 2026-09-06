#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
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
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
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

}
std::unique_ptr<ChipCore> makeDmgApuCore(AccuracyMode accuracy)
{
    return std::make_unique<DmgApuCore>(accuracy);
}
}
