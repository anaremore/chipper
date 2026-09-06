#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Ym2203Core final : public ChipCore
{
public:
    explicit Ym2203Core(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3993600.0;
        if (chip == nullptr)
            chip = std::make_unique<ymfm::ym2203>(host);
        chip->set_fidelity(ymfm::OPN_FIDELITY_MED);
        chip->reset();
        chipSampleRate = static_cast<double>(chip->sample_rate(static_cast<uint32_t>(std::round(clock))));
        sampleAccumulator = 0.0;
        regs.fill(0);
        currentFnum.fill(0);
        currentBlock.fill(0);
        currentAlgorithm.fill(0);
        currentFeedback.fill(0);
        currentAttackRate.fill(0x1fu);
        currentDecayRate.fill(0x08u);
        currentSustainRate.fill(0x00u);
        currentSustainRelease.fill(0x46u);
        currentSsgPeriod.fill(1);
        currentSsgVolume.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        ssgGateMask = 0;
        lastNativeFm = 0;
        lastSsgA = 0;
        lastSsgB = 0;
        lastSsgC = 0;
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

    void writeRegister(uint16_t address, uint8_t value) override
    {
        writeYmRegister(address & 0xffu, value);
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
        auto notes = std::array<int, 3> { heldNote, heldNote + 7, heldNote + 12 };
        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7 }; break;
            case MacroKind::coin:
            case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36 }; break;
            case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote }; break;
            case MacroKind::manual:
            default: break;
        }

        for (size_t channel = 0; channel < sourceChannelCount; ++channel)
            triggerChannel(channel, notes[channel % notes.size()], baseVelocity, channelEnabled(channel));
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
            {
                keyOffChannel(channel);
                channelNotes[channel] = -1;
                channelVelocity[channel] = 0.0f;
                channelStamp[channel] = 0;
            }
        }
    }

    StereoFrame renderSample() override
    {
        if (! chip)
            return {};

        if (! anyAudibleSourceEnabled())
        {
            lastNativeFm = 0;
            currentOutput = {};
            return currentOutput;
        }

        if (heldNote >= 0 && patch.macro == MacroKind::laser)
            applyLaserDrift();

        const auto ratio = chipSampleRate > 0.0 ? chipSampleRate / sampleRate : 1.0;
        sampleAccumulator += ratio;
        ymfm::ym2203::output_data output;
        auto generated = false;
        while (sampleAccumulator >= 1.0)
        {
            chip->generate(&output);
            lastNativeFm = output.data[0];
            if constexpr (ymfm::ym2203::OUTPUTS > 3)
            {
                lastSsgA = output.data[1];
                lastSsgB = output.data[2];
                lastSsgC = output.data[3];
            }
            sampleAccumulator -= 1.0;
            generated = true;
        }
        if (! generated && ratio >= 0.999)
        {
            chip->generate(&output);
            lastNativeFm = output.data[0];
            if constexpr (ymfm::ym2203::OUTPUTS > 3)
            {
                lastSsgA = output.data[1];
                lastSsgB = output.data[2];
                lastSsgC = output.data[3];
            }
        }

        constexpr auto scale = 1.0 / 32768.0;
        const auto fmSample = std::clamp(static_cast<double>(lastNativeFm) * scale, -1.0, 1.0);
        const auto ssgSample = (ssgOutputSample(0) + ssgOutputSample(1) + ssgOutputSample(2)) * 0.33;
        const auto sample = std::clamp(fmSample + ssgSample, -1.0, 1.0);
        currentOutput = { static_cast<float>(sample), static_cast<float>(sample) };
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

    ChipMode mode() const override { return ChipMode::ym2203; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "YM2203 / OPN"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPN FM register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YM2203/OPN synthesis core. Chipper currently maps musical controls and notes to the three FM channels plus the embedded three-channel SSG: operator, algorithm, feedback, f-number/block, FM key-on, SSG tone/noise period, mixer, amplitude, and SSG envelope registers are driven through the YM2203 address/data port. Timers, prescaler controls, CSM, golden emulator comparison, hardware comparison, and cycle accuracy are not complete.";
    }

    std::string debugStateJson() const override
    {
        const auto operatorCarrier = [this](size_t op)
        {
            return fmOperatorIsCarrierForAlgorithm(currentAlgorithm[0], op) ? 1 : 0;
        };

        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2203 / OPN\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPN FM register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"81aec25ccbb98f4873a255f7551ac4dadac59b4a\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":6,"
             << "\"exposedChannelCount\":6,"
             << "\"fmChannelCount\":3,"
             << "\"ssgChannelCount\":3,"
             << "\"ssgIntegrated\":1,"
             << "\"algorithm0\":" << static_cast<int>(currentAlgorithm[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"algorithmFeedbackRegister0\":" << static_cast<int>(regs[0xb0]) << ","
             << "\"operatorCarrier0\":" << operatorCarrier(0) << ","
             << "\"operatorCarrier1\":" << operatorCarrier(1) << ","
             << "\"operatorCarrier2\":" << operatorCarrier(2) << ","
             << "\"operatorCarrier3\":" << operatorCarrier(3) << ","
             << "\"fmOperatorLevel0\":" << patch.fmOperatorLevels[0] << ","
             << "\"fmOperatorLevel1\":" << patch.fmOperatorLevels[1] << ","
             << "\"fmOperatorLevel2\":" << patch.fmOperatorLevels[2] << ","
             << "\"fmOperatorLevel3\":" << patch.fmOperatorLevels[3] << ","
             << "\"fmOperatorMultiplier0\":" << patch.fmOperatorMultipliers[0] << ","
             << "\"fmOperatorMultiplier1\":" << patch.fmOperatorMultipliers[1] << ","
             << "\"fmOperatorMultiplier2\":" << patch.fmOperatorMultipliers[2] << ","
             << "\"fmOperatorMultiplier3\":" << patch.fmOperatorMultipliers[3] << ","
             << "\"operatorMultiple0\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 0)] & 0x0fu) << ","
             << "\"operatorMultiple1\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 1)] & 0x0fu) << ","
             << "\"operatorMultiple2\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 2)] & 0x0fu) << ","
             << "\"operatorMultiple3\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 3)] & 0x0fu) << ","
             << "\"operatorAttackRate0\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 0)]) << ","
             << "\"operatorAttackRate1\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 1)]) << ","
             << "\"operatorAttackRate2\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 2)]) << ","
             << "\"operatorAttackRate3\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 3)]) << ","
             << "\"operatorDecayRate0\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 0)]) << ","
             << "\"operatorDecayRate1\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 1)]) << ","
             << "\"operatorDecayRate2\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 2)]) << ","
             << "\"operatorDecayRate3\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 3)]) << ","
             << "\"operatorSustainRate0\":" << static_cast<int>(regs[opRegForChannel(0x70, 0, 0)]) << ","
             << "\"operatorSustainRate1\":" << static_cast<int>(regs[opRegForChannel(0x70, 0, 1)]) << ","
             << "\"operatorSustainRate2\":" << static_cast<int>(regs[opRegForChannel(0x70, 0, 2)]) << ","
             << "\"operatorSustainRate3\":" << static_cast<int>(regs[opRegForChannel(0x70, 0, 3)]) << ","
             << "\"operatorReleaseRate0\":" << static_cast<int>(regs[opRegForChannel(0x80, 0, 0)] & 0x0fu) << ","
             << "\"operatorReleaseRate1\":" << static_cast<int>(regs[opRegForChannel(0x80, 0, 1)] & 0x0fu) << ","
             << "\"operatorReleaseRate2\":" << static_cast<int>(regs[opRegForChannel(0x80, 0, 2)] & 0x0fu) << ","
             << "\"operatorReleaseRate3\":" << static_cast<int>(regs[opRegForChannel(0x80, 0, 3)] & 0x0fu) << ","
             << "\"operatorTotalLevel0\":" << static_cast<int>(regs[opRegForChannel(0x40, 0, 0)]) << ","
             << "\"operatorTotalLevel1\":" << static_cast<int>(regs[opRegForChannel(0x40, 0, 1)]) << ","
             << "\"operatorTotalLevel2\":" << static_cast<int>(regs[opRegForChannel(0x40, 0, 2)]) << ","
             << "\"operatorTotalLevel3\":" << static_cast<int>(regs[opRegForChannel(0x40, 0, 3)]) << ","
             << "\"envelopeShape\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"attackRate0\":" << static_cast<int>(currentAttackRate[0]) << ","
             << "\"decayRate0\":" << static_cast<int>(currentDecayRate[0]) << ","
             << "\"sustainRate0\":" << static_cast<int>(currentSustainRate[0]) << ","
             << "\"sustainRelease0\":" << static_cast<int>(currentSustainRelease[0]) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"ssgGateMask\":" << static_cast<int>(ssgGateMask) << ","
             << "\"ssgMixer\":" << static_cast<int>(regs[7]) << ","
             << "\"ssgNoisePeriod\":" << static_cast<int>(regs[6] & 0x1fu) << ","
             << "\"ssgEnvelopeChoice\":" << opnSsgEnvelopeChoiceForPatch(patch) << ","
             << "\"ssgEnvelopeEnabled\":" << (opnSsgEnvelopeEnabledForPatch(patch) ? 1 : 0) << ","
             << "\"ssgEnvelopePeriod\":" << static_cast<int>(regs[11] | (regs[12] << 8u)) << ","
             << "\"ssgEnvelopeShape\":" << static_cast<int>(regs[13] & 0x0fu) << ","
             << "\"ssgChannelMixChoiceA\":" << ym2149ChannelMixChoiceForPatch(patch, 0) << ","
             << "\"ssgChannelMixChoiceB\":" << ym2149ChannelMixChoiceForPatch(patch, 1) << ","
             << "\"ssgChannelMixChoiceC\":" << ym2149ChannelMixChoiceForPatch(patch, 2) << ","
             << "\"ssgToneEnabledA\":" << (((regs[7] & 0x01u) == 0) ? 1 : 0) << ","
             << "\"ssgToneEnabledB\":" << (((regs[7] & 0x02u) == 0) ? 1 : 0) << ","
             << "\"ssgToneEnabledC\":" << (((regs[7] & 0x04u) == 0) ? 1 : 0) << ","
             << "\"ssgNoiseEnabledA\":" << (((regs[7] & 0x08u) == 0) ? 1 : 0) << ","
             << "\"ssgNoiseEnabledB\":" << (((regs[7] & 0x10u) == 0) ? 1 : 0) << ","
             << "\"ssgNoiseEnabledC\":" << (((regs[7] & 0x20u) == 0) ? 1 : 0) << ","
             << "\"ssgTonePeriodA\":" << ssgPeriodRegister(0) << ","
             << "\"ssgTonePeriodB\":" << ssgPeriodRegister(1) << ","
             << "\"ssgTonePeriodC\":" << ssgPeriodRegister(2) << ","
             << "\"ssgVolumeA\":" << static_cast<int>(regs[8] & 0x1fu) << ","
             << "\"ssgVolumeB\":" << static_cast<int>(regs[9] & 0x1fu) << ","
             << "\"ssgVolumeC\":" << static_cast<int>(regs[10] & 0x1fu) << ","
             << "\"sourceEnabled0\":" << (channelEnabled(0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (channelEnabled(1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (channelEnabled(2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (channelEnabled(3) ? 1 : 0) << ","
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
             << "\"nativeFm\":" << lastNativeFm << ","
             << "\"ssgA\":" << lastSsgA << ","
             << "\"ssgB\":" << lastSsgB << ","
             << "\"ssgC\":" << lastSsgC << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    class Host final : public ymfm::ymfm_interface
    {
    };

    struct OPNPitch
    {
        uint16_t fnum = 0;
        uint8_t block = 0;
    };

    static uint16_t regForChannel(uint8_t base, size_t channel)
    {
        return static_cast<uint16_t>(base | static_cast<uint8_t>(channel % 3u));
    }

    static uint16_t opRegForChannel(uint8_t base, size_t channel, size_t op)
    {
        static constexpr std::array<uint8_t, 4> opOffsets { 0x00, 0x04, 0x08, 0x0c };
        return regForChannel(static_cast<uint8_t>(base + opOffsets[op]), channel);
    }

    int ssgPeriodRegister(size_t channel) const
    {
        if (channel >= ssgChannelCount)
            return 0;

        const auto reg = channel * 2u;
        return std::max(1, static_cast<int>(regs[reg] | ((regs[reg + 1u] & 0x0fu) << 8u)));
    }

    void writeYmRegister(uint16_t reg, uint8_t value)
    {
        regs[reg & 0xffu] = value;
        if (! chip)
            return;

        chip->write(0, static_cast<uint8_t>(reg & 0xffu));
        chip->write(1, value);
    }

    OPNPitch pitchForNote(int midiNote) const
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
        return ym2612AlgorithmForPatch(patch);
    }

    uint8_t feedbackForPatch() const
    {
        return fmFeedbackForPatch(patch);
    }

    uint8_t multiplierForPatch(size_t op) const
    {
        return fmOperatorMultipleForPatch(ChipMode::ym2203, patch, op);
    }

    uint8_t totalLevelForOperator(size_t op, float velocity) const
    {
        return fmOperatorTotalLevelForPatch(ChipMode::ym2203, patch, op, velocity);
    }

    bool channelEnabled(size_t channel) const
    {
        return channel < sourceChannelCount && sourceEnabled(patch, channel);
    }

    bool anyAudibleSourceEnabled() const
    {
        for (size_t channel = 0; channel < sourceChannelCount; ++channel)
        {
            if (channelEnabled(channel))
                return true;
        }
        return false;
    }

    void applyChannelPatch(size_t channel, float velocity)
    {
        if (channel >= fmChannelCount)
            return;

        const auto algorithm = algorithmForPatch();
        const auto feedback = feedbackForPatch();
        currentAlgorithm[channel] = algorithm;
        currentFeedback[channel] = feedback;

        for (size_t op = 0; op < 4; ++op)
        {
            const auto envelope = ym2612EnvelopeRegistersForPatch(patch, op);
            writeYmRegister(opRegForChannel(0x30, channel, op), multiplierForPatch(op));
            writeYmRegister(opRegForChannel(0x40, channel, op), totalLevelForOperator(op, velocity));
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
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        for (size_t channel = 0; channel < fmChannelCount; ++channel)
            applyChannelPatch(channel, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f);
        writeSsgSharedRegisters();
        updateSsgMixer();

        if (! preserveKeys)
            return;

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelGateActive(channel) && channelNotes[channel] >= 0)
                triggerChannel(channel, channelNotes[channel], channelVelocity[channel], channelEnabled(channel));
        }
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= fmChannelCount)
        {
            triggerSsgChannel(channel, midiNote, velocity, shouldEnable);
            return;
        }

        if (channel >= channelNotes.size() || ! chip)
            return;

        // control2 is the native OPN feedback field. It must not alter pitch:
        // doing so detunes only FM1-3 while the paired SSG lanes remain on the
        // requested notes, breaking the YM2203's combined FM/SSG instrument.
        const auto pitch = pitchForNote(midiNote);
        channelNotes[channel] = std::clamp(midiNote, 0, 127);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity) * sourceLevel(patch, channel));
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        applyChannelPatch(channel, channelVelocity[channel]);

        writeYmRegister(regForChannel(0xa4, channel), static_cast<uint8_t>(((pitch.block & 0x07u) << 3u) | ((pitch.fnum >> 8u) & 0x07u)));
        writeYmRegister(regForChannel(0xa0, channel), static_cast<uint8_t>(pitch.fnum & 0xffu));

        const auto code = static_cast<uint8_t>(channel & 0x03u);
        writeYmRegister(0x28, static_cast<uint8_t>(shouldEnable ? (0xf0u | code) : code));
        if (shouldEnable)
            keyOnMask |= static_cast<uint16_t>(1u << channel);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= fmChannelCount)
        {
            keyOffSsgChannel(channel);
            return;
        }

        if (channel >= channelNotes.size())
            return;

        writeYmRegister(0x28, static_cast<uint8_t>(channel & 0x03u));
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
            if (channelEnabled(channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelEnabled(channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelStamp.size(); ++channel)
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
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
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
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 7.0) * patch.control3 * 9.0));
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (! channelGateActive(channel))
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, channelEnabled(channel));
        }
    }

    bool channelGateActive(size_t channel) const
    {
        if (channel < fmChannelCount)
            return (keyOnMask & (1u << channel)) != 0;
        if (channel < sourceChannelCount)
            return (ssgGateMask & (1u << channel)) != 0;
        return false;
    }

    uint32_t ssgClockHz() const
    {
        if (chip)
            return chip->ssg_effective_clock(static_cast<uint32_t>(std::max(1.0, std::round(clock))));
        return static_cast<uint32_t>(std::max(1.0, std::round(clock / 4.0)));
    }

    uint16_t ssgPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto period = static_cast<double>(ssgClockHz()) / (16.0 * hz);
        return static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(period)), 1, 4095));
    }

    uint8_t ssgVolumeForVelocity(float velocity) const
    {
        const auto base = clamp01(velocity) * clamp01(patch.control4);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(base * 15.0f)), 0, 15));
    }

    void writeSsgTone(size_t ssgChannel, int midiNote)
    {
        if (ssgChannel >= ssgChannelCount)
            return;

        const auto period = ssgPeriodForNote(midiNote);
        currentSsgPeriod[ssgChannel] = period;
        const auto reg = static_cast<uint8_t>(ssgChannel * 2u);
        writeYmRegister(reg, static_cast<uint8_t>(period & 0xffu));
        writeYmRegister(static_cast<uint8_t>(reg + 1u), static_cast<uint8_t>((period >> 8u) & 0x0fu));
    }

    void writeSsgVolume(size_t ssgChannel, uint8_t volume)
    {
        if (ssgChannel >= ssgChannelCount)
            return;

        const auto clipped = static_cast<uint8_t>(std::min<uint8_t>(volume, 15u));
        currentSsgVolume[ssgChannel] = clipped;
        writeYmRegister(static_cast<uint8_t>(0x08u + ssgChannel),
                        static_cast<uint8_t>(opnSsgEnvelopeEnabledForPatch(patch) && clipped > 0 ? 0x10u : clipped));
    }

    void writeSsgSharedRegisters()
    {
        const auto envelopePeriod = ym2149EnvelopePeriodForControl(patch.envelopeDecay);
        writeYmRegister(0x06, opnSsgNoisePeriodForPatch(patch));
        writeYmRegister(0x0b, static_cast<uint8_t>(envelopePeriod & 0xffu));
        writeYmRegister(0x0c, static_cast<uint8_t>((envelopePeriod >> 8u) & 0xffu));
        writeYmRegister(0x0d, opnSsgEnvelopeShapeCodeForPatch(patch));
    }

    void updateSsgMixer()
    {
        auto mixer = static_cast<uint8_t>((regs[7] & 0xc0u) | (opnSsgMixerRegisterForPatch(patch) & 0x3fu));
        for (size_t ssg = 0; ssg < ssgChannelCount; ++ssg)
        {
            const auto source = ssgLaneOffset + ssg;
            const auto active = channelEnabled(source) && (ssgGateMask & (1u << source)) != 0;
            if (! active)
                mixer = static_cast<uint8_t>(mixer | (1u << ssg) | (1u << (ssg + 3u)));
        }
        writeYmRegister(0x07, mixer);
    }

    void triggerSsgChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel < ssgLaneOffset || channel >= sourceChannelCount)
            return;

        const auto ssg = channel - ssgLaneOffset;
        channelNotes[channel] = std::clamp(midiNote, 0, 127);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity));
        writeSsgSharedRegisters();
        writeSsgTone(ssg, midiNote);

        if (shouldEnable)
        {
            ssgGateMask |= static_cast<uint16_t>(1u << channel);
            writeSsgVolume(ssg, ssgVolumeForVelocity(velocity));
        }
        else
        {
            ssgGateMask &= static_cast<uint16_t>(~(1u << channel));
            writeSsgVolume(ssg, 0);
        }

        updateSsgMixer();
    }

    void keyOffSsgChannel(size_t channel)
    {
        if (channel < ssgLaneOffset || channel >= sourceChannelCount)
            return;

        ssgGateMask &= static_cast<uint16_t>(~(1u << channel));
        writeSsgVolume(channel - ssgLaneOffset, 0);
        updateSsgMixer();
    }

    double ssgOutputSample(size_t ssgChannel) const
    {
        if (ssgChannel >= ssgChannelCount)
            return 0.0;

        const auto source = ssgLaneOffset + ssgChannel;
        if (! channelEnabled(source))
            return 0.0;

        const auto raw = ssgChannel == 0 ? lastSsgA : (ssgChannel == 1 ? lastSsgB : lastSsgC);
        constexpr auto scale = 1.0 / 32768.0;
        return std::clamp(static_cast<double>(raw) * scale, -1.0, 1.0) * sourceLevel(patch, source);
    }

    AccuracyMode accuracy;
    static constexpr size_t fmChannelCount = 3;
    static constexpr size_t ssgChannelCount = 3;
    static constexpr size_t ssgLaneOffset = 3;
    static constexpr size_t sourceChannelCount = 6;
    double sampleRate = 48000.0;
    double clock = 3993600.0;
    double chipSampleRate = 333000.0;
    double sampleAccumulator = 0.0;
    Host host;
    std::unique_ptr<ymfm::ym2203> chip;
    PatchConfig patch;
    std::array<uint8_t, 0x100> regs {};
    std::array<uint16_t, 3> currentFnum {};
    std::array<uint8_t, 3> currentBlock {};
    std::array<uint8_t, 3> currentAlgorithm {};
    std::array<uint8_t, 3> currentFeedback {};
    std::array<uint8_t, 3> currentAttackRate {};
    std::array<uint8_t, 3> currentDecayRate {};
    std::array<uint8_t, 3> currentSustainRate {};
    std::array<uint8_t, 3> currentSustainRelease {};
    std::array<uint16_t, 3> currentSsgPeriod {};
    std::array<uint8_t, 3> currentSsgVolume {};
    std::array<int, 6> channelNotes {};
    std::array<float, 6> channelVelocity {};
    std::array<uint64_t, 6> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    uint16_t ssgGateMask = 0;
    double laserPhase = 0.0;
    int32_t lastNativeFm = 0;
    int32_t lastSsgA = 0;
    int32_t lastSsgB = 0;
    int32_t lastSsgC = 0;
    StereoFrame currentOutput {};
};

}
std::unique_ptr<ChipCore> makeYm2203Core(AccuracyMode accuracy)
{
    return std::make_unique<Ym2203Core>(accuracy);
}
}
