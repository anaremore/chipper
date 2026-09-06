#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
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
        motionNoisePeriod = 0;
        baseNoisePeriod = 0;
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
        if (reg == 6)
        {
            baseNoisePeriod = value;
            if (motionNoisePeriod != 0)
                value = static_cast<uint8_t>(motionNoisePeriod - 1u);
        }
        regs[reg] = value;
        if (reg == 13)
            resetEnvelopeCounter();
        writeEmuRegister(static_cast<uint8_t>(reg), value);
    }

    void setYmNoiseMotion(uint8_t choice) noexcept override
    {
        motionNoisePeriod = std::min<uint8_t>(choice, 32u);
        regs[6] = motionNoisePeriod == 0 ? baseNoisePeriod : static_cast<uint8_t>(motionNoisePeriod - 1u);
        writeEmuRegister(6, regs[6]);
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
            writes.push_back({ 0, static_cast<uint16_t>(i), i == 6u ? baseNoisePeriod : regs[i] });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::ym2149; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
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
    uint8_t motionNoisePeriod = 0;
    uint8_t baseNoisePeriod = 0;
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
        const auto emuClock = static_cast<uint32_t>(std::max(1.0, std::round(clock)));
        const auto outputRate = static_cast<uint32_t>(std::max(1.0, std::round(sampleRate)));
        if (emu == nullptr)
            emu = PSG_new(emuClock, outputRate);
        else
        {
            PSG_setClock(emu, emuClock);
            PSG_setRate(emu, outputRate);
        }
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

}
std::unique_ptr<ChipCore> makeYm2149Core(AccuracyMode accuracy)
{
    return std::make_unique<Ym2149Core>(accuracy);
}
}
