#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Ym2608Core final : public ChipCore
{
public:
    explicit Ym2608Core(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 7987200.0;
        if (chip == nullptr)
            chip = std::make_unique<ymfm::ym2608>(host);
        chip->set_fidelity(ymfm::OPN_FIDELITY_MED);
        chip->reset();
        host.resetAdpcmCounters();
        chipSampleRate = static_cast<double>(chip->sample_rate(static_cast<uint32_t>(std::round(clock))));
        sampleAccumulator = 0.0;
        regs.fill(0);
        currentFnum.fill(0);
        currentBlock.fill(0);
        currentAlgorithm.fill(0);
        currentFeedback.fill(0);
        currentPanBits.fill(0xc0u);
        currentAttackRate.fill(0x1fu);
        currentDecayRate.fill(0x08u);
        currentSustainRate.fill(0x00u);
        currentSustainRelease.fill(0x46u);
        currentSsgPeriod.fill(1);
        currentSsgVolume.fill(0);
        currentOpnaRhythmLevels.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        ssgGateMask = 0;
        opnaRhythmKeyBits = 0;
        opnaRhythmTotalLevel = 0x3fu;
        opnaAdpcmBControlRegister = 0;
        opnaAdpcmBPanRegister = 0;
        opnaAdpcmBStartRegister = 0;
        opnaAdpcmBEndRegister = 0;
        opnaAdpcmBLimitRegister = 0;
        opnaAdpcmBDeltaNRegister = 0;
        opnaAdpcmBLevelRegister = 0;
        lastNativeLeft = 0;
        lastNativeRight = 0;
        lastSsg = 0;
        currentOutput = {};
        applyPatchToAllChannels(false);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode
            || nextPatch.sourceEnabled != patch.sourceEnabled
            || opnaRhythmEnabledForMacro(nextPatch.macro) != opnaRhythmEnabledForMacro(patch.macro))
            clearChipPolyState();

        patch = nextPatch;
        if (! opnaRhythmEnabledForPatch())
            keyOffOpnaRhythm();
        applyPatchToAllChannels(true);
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        host.setAdpcmARom(std::move(data));
        host.resetAdpcmCounters();
    }

    void setExternalAdpcmBData(std::vector<uint8_t> data) override
    {
        host.setAdpcmBMemory(std::move(data));
        host.resetAdpcmCounters();
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
        auto notes = std::array<int, 9> { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote, heldNote + 12, heldNote + 19 };
        switch (patch.macro)
        {
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote - 12, heldNote, heldNote + 7 };
                break;
            case MacroKind::lead:
                notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31 };
                break;
            case MacroKind::coin:
            case MacroKind::jump:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 36, heldNote + 43, heldNote + 48 };
                break;
            case MacroKind::laser:
                notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote - 19, heldNote - 24, heldNote + 19, heldNote + 7, heldNote - 5 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41, heldNote + 48 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36 };
                break;
            case MacroKind::manual:
            default:
                break;
        }

        for (size_t channel = 0; channel < sourceChannelCount; ++channel)
            triggerChannel(channel, notes[channel], baseVelocity, channelEnabled(channel));

        if (opnaRhythmEnabledForPatch())
            triggerOpnaRhythm(heldNote, baseVelocity);
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
            keyOffOpnaRhythm();
        }
    }

    StereoFrame renderSample() override
    {
        if (! chip)
            return {};

        if (! anyAudibleSourceEnabled())
        {
            lastNativeLeft = 0;
            lastNativeRight = 0;
            lastSsg = 0;
            currentOutput = {};
            return currentOutput;
        }

        if (heldNote >= 0 && patch.macro == MacroKind::laser)
            applyLaserDrift();

        const auto ratio = chipSampleRate > 0.0 ? chipSampleRate / sampleRate : 1.0;
        sampleAccumulator += ratio;
        ymfm::ym2608::output_data output;
        auto generated = false;
        while (sampleAccumulator >= 1.0)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
            if constexpr (ymfm::ym2608::OUTPUTS > 2)
                lastSsg = output.data[2];
            sampleAccumulator -= 1.0;
            generated = true;
        }
        if (! generated && ratio >= 0.999)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
            if constexpr (ymfm::ym2608::OUTPUTS > 2)
                lastSsg = output.data[2];
        }

        constexpr auto scale = 1.0 / 32768.0;
        const auto ssgSample = ssgOutputSample() * 0.45;
        const auto left = std::clamp(static_cast<double>(lastNativeLeft) * scale + ssgSample, -1.0, 1.0);
        const auto right = std::clamp(static_cast<double>(lastNativeRight) * scale + ssgSample, -1.0, 1.0);
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

    ChipMode mode() const override { return ChipMode::ym2608; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "YM2608 / OPNA"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPNA FM+SSG register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YM2608/OPNA synthesis core. Chipper currently maps musical controls and notes to six OPNA FM channels plus the embedded three-channel SSG tone/noise/envelope generator: operator, algorithm, feedback, f-number/block, FM key-on, FM pan, SSG tone/noise period, mixer, amplitude, and envelope registers are driven through the YM2608 low/high address-data ports. Drum and Hit macros also write native OPNA ADPCM-A rhythm key, total-level, pan, and instrument-level registers, using original Chipper-generated in-memory percussion bytes by default with an optional user-owned ADPCM-A rhythm ROM override. A first-pass ADPCM-B byte-memory path can layer user-owned encoded ADPCM-B samples onto Drum and Hit macros. A pinned YM2608-LLE stereo fixture independently gates covered OPNA ADPCM-A BD/Tom decoding, native rates, addressing, routing, gain, duration, and a failing nibble mutation. Broader ADPCM-A traces, ADPCM-B reference proof, timers, prescaler controls, analog/hardware comparison, and cycle accuracy are not complete.";
    }

    std::string debugStateJson() const override
    {
        const auto operatorCarrier = [this](size_t op)
        {
            return fmOperatorIsCarrierForAlgorithm(currentAlgorithm[0], op) ? 1 : 0;
        };

        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2608 / OPNA\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPNA FM+SSG register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"81aec25ccbb98f4873a255f7551ac4dadac59b4a\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":9,"
             << "\"exposedChannelCount\":9,"
             << "\"fmChannelCount\":6,"
             << "\"ssgChannelCount\":3,"
             << "\"ssgIntegrated\":1,"
             << "\"rhythmImplemented\":1,"
             << "\"adpcmAImplemented\":1,"
             << "\"adpcmBImplemented\":1,"
             << "\"opnaRhythmOverlay\":" << (opnaRhythmEnabledForPatch() ? 1 : 0) << ","
             << "\"opnaRhythmRegister\":" << static_cast<int>(regs[0x10]) << ","
             << "\"opnaRhythmKeyBits\":" << static_cast<int>(opnaRhythmKeyBits) << ","
             << "\"opnaRhythmTotalLevel\":" << static_cast<int>(opnaRhythmTotalLevel) << ","
             << "\"opnaRhythmBassLevel\":" << static_cast<int>(currentOpnaRhythmLevels[0]) << ","
             << "\"opnaRhythmSnareLevel\":" << static_cast<int>(currentOpnaRhythmLevels[1]) << ","
             << "\"opnaRhythmTopCymLevel\":" << static_cast<int>(currentOpnaRhythmLevels[2]) << ","
             << "\"opnaRhythmHatLevel\":" << static_cast<int>(currentOpnaRhythmLevels[3]) << ","
             << "\"opnaRhythmTomLevel\":" << static_cast<int>(currentOpnaRhythmLevels[4]) << ","
             << "\"opnaRhythmRimLevel\":" << static_cast<int>(currentOpnaRhythmLevels[5]) << ","
             << "\"opnaAdpcmARomSource\":" << (host.usingUserAdpcmARom() ? 1 : 0) << ","
             << "\"opnaAdpcmAGeneratedRom\":" << (host.usingUserAdpcmARom() ? 0 : 1) << ","
             << "\"opnaAdpcmAUserRomLoaded\":" << (host.usingUserAdpcmARom() ? 1 : 0) << ","
             << "\"opnaAdpcmARomBytes\":" << opnaAdpcmARomSize << ","
             << "\"opnaAdpcmARomProvidedBytes\":" << host.adpcmAProvidedBytes() << ","
             << "\"opnaAdpcmARomCopiedBytes\":" << host.adpcmACopiedBytes() << ","
             << "\"opnaAdpcmARomChecksum\":" << host.adpcmARomChecksum() << ","
             << "\"opnaAdpcmAReadCount\":" << host.adpcmAReads() << ","
             << "\"opnaAdpcmALastReadAddress\":" << host.lastAdpcmAReadAddress() << ","
             << "\"opnaAdpcmBReadCount\":" << host.adpcmBReads() << ","
             << "\"opnaAdpcmBLoaded\":" << (host.adpcmBLoaded() ? 1 : 0) << ","
             << "\"opnaAdpcmBProvidedBytes\":" << host.adpcmBProvidedBytes() << ","
             << "\"opnaAdpcmBCopiedBytes\":" << host.adpcmBCopiedBytes() << ","
             << "\"opnaAdpcmBChecksum\":" << host.adpcmBChecksum() << ","
             << "\"opnaAdpcmBLastReadAddress\":" << host.lastAdpcmBReadAddress() << ","
             << "\"opnaAdpcmBControlRegister\":" << static_cast<int>(opnaAdpcmBControlRegister) << ","
             << "\"opnaAdpcmBPanRegister\":" << static_cast<int>(opnaAdpcmBPanRegister) << ","
             << "\"opnaAdpcmBStartRegister\":" << opnaAdpcmBStartRegister << ","
             << "\"opnaAdpcmBEndRegister\":" << opnaAdpcmBEndRegister << ","
             << "\"opnaAdpcmBLimitRegister\":" << opnaAdpcmBLimitRegister << ","
             << "\"opnaAdpcmBDeltaNRegister\":" << opnaAdpcmBDeltaNRegister << ","
             << "\"opnaAdpcmBLevelRegister\":" << static_cast<int>(opnaAdpcmBLevelRegister) << ","
             << "\"opnaAdpcmBMaxBytes\":" << opnaAdpcmBMaxBytes << ","
             << "\"algorithm0\":" << static_cast<int>(currentAlgorithm[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"algorithmFeedbackRegister0\":" << static_cast<int>(regs[0xb0]) << ","
             << "\"algorithmFeedbackRegister3\":" << static_cast<int>(regs[0x1b0]) << ","
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
             << "\"fmOperatorAttackRate0\":" << patch.fmOperatorAttackRates[0] << ","
             << "\"fmOperatorAttackRate1\":" << patch.fmOperatorAttackRates[1] << ","
             << "\"fmOperatorAttackRate2\":" << patch.fmOperatorAttackRates[2] << ","
             << "\"fmOperatorAttackRate3\":" << patch.fmOperatorAttackRates[3] << ","
             << "\"fmOperatorDecayRate0\":" << patch.fmOperatorDecayRates[0] << ","
             << "\"fmOperatorDecayRate1\":" << patch.fmOperatorDecayRates[1] << ","
             << "\"fmOperatorDecayRate2\":" << patch.fmOperatorDecayRates[2] << ","
             << "\"fmOperatorDecayRate3\":" << patch.fmOperatorDecayRates[3] << ","
             << "\"fmOperatorSustainRate0\":" << patch.fmOperatorSustainRates[0] << ","
             << "\"fmOperatorSustainRate1\":" << patch.fmOperatorSustainRates[1] << ","
             << "\"fmOperatorSustainRate2\":" << patch.fmOperatorSustainRates[2] << ","
             << "\"fmOperatorSustainRate3\":" << patch.fmOperatorSustainRates[3] << ","
             << "\"fmOperatorReleaseRate0\":" << patch.fmOperatorReleaseRates[0] << ","
             << "\"fmOperatorReleaseRate1\":" << patch.fmOperatorReleaseRates[1] << ","
             << "\"fmOperatorReleaseRate2\":" << patch.fmOperatorReleaseRates[2] << ","
             << "\"fmOperatorReleaseRate3\":" << patch.fmOperatorReleaseRates[3] << ","
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
             << "\"panBits0\":" << static_cast<int>(currentPanBits[0]) << ","
             << "\"panBits3\":" << static_cast<int>(currentPanBits[3]) << ","
             << "\"envelopeShape\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"attackRate0\":" << static_cast<int>(currentAttackRate[0]) << ","
             << "\"decayRate0\":" << static_cast<int>(currentDecayRate[0]) << ","
             << "\"sustainRate0\":" << static_cast<int>(currentSustainRate[0]) << ","
             << "\"sustainRelease0\":" << static_cast<int>(currentSustainRelease[0]) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"fnum3\":" << currentFnum[3] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"block3\":" << static_cast<int>(currentBlock[3]) << ","
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
             << "\"sourceEnabled6\":" << (channelEnabled(6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (channelEnabled(7) ? 1 : 0) << ","
             << "\"sourceEnabled8\":" << (channelEnabled(8) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"sourceLevel5\":" << sourceLevel(patch, 5) << ","
             << "\"sourceLevel6\":" << sourceLevel(patch, 6) << ","
             << "\"sourceLevel7\":" << sourceLevel(patch, 7) << ","
             << "\"sourceLevel8\":" << sourceLevel(patch, 8) << ","
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
             << "\"ssgMono\":" << lastSsg << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    class Host final : public ymfm::ymfm_interface
    {
    public:
        Host()
            : adpcmARom(makeGeneratedOpnaAdpcmARom())
        {
        }

        void setAdpcmARom(std::vector<uint8_t> data)
        {
            if (data.empty())
            {
                adpcmARom = makeGeneratedOpnaAdpcmARom();
                userAdpcmARom = false;
                providedAdpcmABytes = 0;
                copiedAdpcmABytes = 0;
                return;
            }

            adpcmARom.fill(0);
            providedAdpcmABytes = data.size();
            copiedAdpcmABytes = std::min(data.size(), adpcmARom.size());
            std::copy_n(data.begin(), copiedAdpcmABytes, adpcmARom.begin());
            userAdpcmARom = true;
        }

        void setAdpcmBMemory(std::vector<uint8_t> data)
        {
            providedAdpcmBBytes = data.size();
            copiedAdpcmBBytes = std::min(data.size(), opnaAdpcmBMaxBytes);
            adpcmBMemory.clear();
            adpcmBMemory.reserve(copiedAdpcmBBytes);
            if (copiedAdpcmBBytes > 0)
                adpcmBMemory.insert(adpcmBMemory.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(copiedAdpcmBBytes));
            if (adpcmBMemory.empty())
            {
                providedAdpcmBBytes = 0;
                copiedAdpcmBBytes = 0;
            }
        }

        uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
        {
            if (type == ymfm::ACCESS_ADPCM_A)
            {
                ++adpcmAReadCount;
                lastAdpcmAAddress = address & static_cast<uint32_t>(adpcmARom.size() - 1u);
                return adpcmARom[static_cast<size_t>(lastAdpcmAAddress)];
            }

            if (type == ymfm::ACCESS_ADPCM_B)
            {
                ++adpcmBReadCount;
                lastAdpcmBAddress = address;
                if (! adpcmBMemory.empty())
                    return adpcmBMemory[static_cast<size_t>(address % static_cast<uint32_t>(adpcmBMemory.size()))];
            }
            return 0;
        }

        void resetAdpcmCounters()
        {
            adpcmAReadCount = 0;
            adpcmBReadCount = 0;
            lastAdpcmAAddress = 0;
            lastAdpcmBAddress = 0;
        }

        uint64_t adpcmAReads() const { return adpcmAReadCount; }
        uint64_t adpcmBReads() const { return adpcmBReadCount; }
        uint32_t lastAdpcmAReadAddress() const { return lastAdpcmAAddress; }
        uint32_t lastAdpcmBReadAddress() const { return lastAdpcmBAddress; }
        bool usingUserAdpcmARom() const { return userAdpcmARom; }
        size_t adpcmAProvidedBytes() const { return providedAdpcmABytes; }
        size_t adpcmACopiedBytes() const { return copiedAdpcmABytes; }
        bool adpcmBLoaded() const { return ! adpcmBMemory.empty(); }
        size_t adpcmBProvidedBytes() const { return providedAdpcmBBytes; }
        size_t adpcmBCopiedBytes() const { return copiedAdpcmBBytes; }
        uint32_t adpcmBChecksum() const { return adpcmBMemory.empty() ? 0u : checksumBytes(adpcmBMemory); }

        uint32_t adpcmARomChecksum() const { return checksumOpnaAdpcmARom(adpcmARom); }

    private:
        std::vector<uint8_t> adpcmBMemory;
        size_t providedAdpcmBBytes = 0;
        size_t copiedAdpcmBBytes = 0;
        std::array<uint8_t, opnaAdpcmARomSize> adpcmARom {};
        bool userAdpcmARom = false;
        size_t providedAdpcmABytes = 0;
        size_t copiedAdpcmABytes = 0;
        uint64_t adpcmAReadCount = 0;
        uint64_t adpcmBReadCount = 0;
        uint32_t lastAdpcmAAddress = 0;
        uint32_t lastAdpcmBAddress = 0;
    };

    struct OPNAPitch
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

    int ssgPeriodRegister(size_t channel) const
    {
        if (channel >= ssgChannelCount)
            return 0;

        const auto reg = channel * 2u;
        return std::max(1, static_cast<int>(regs[reg] | ((regs[reg + 1u] & 0x0fu) << 8u)));
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

    OPNAPitch pitchForNote(int midiNote) const
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
        return fmOperatorMultipleForPatch(ChipMode::ym2608, patch, op);
    }

    uint8_t totalLevelForOperator(size_t op, float velocity) const
    {
        return fmOperatorTotalLevelForPatch(ChipMode::ym2608, patch, op, velocity);
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

    static bool opnaRhythmEnabledForMacro(MacroKind macro)
    {
        return macro == MacroKind::drum || macro == MacroKind::hit;
    }

    bool opnaRhythmEnabledForPatch() const
    {
        return opnaRhythmEnabledForMacro(patch.macro);
    }

    uint8_t opnaRhythmInstrumentLevel(size_t rhythmChannel, float velocity) const
    {
        const auto source = std::min(rhythmChannel, fmChannelCount - static_cast<size_t>(1));
        const auto level = clamp01(velocity) * clamp01(patch.control4) * sourceLevel(patch, source);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(level * 31.0)), 0, 31));
    }

    uint8_t opnaRhythmKeyBitsForPatch(float velocity) const
    {
        if (! opnaRhythmEnabledForPatch() || ! anyAudibleSourceEnabled())
            return 0;

        uint8_t keyBits = 0;
        for (size_t rhythm = 0; rhythm < currentOpnaRhythmLevels.size(); ++rhythm)
        {
            if (channelEnabled(rhythm) && opnaRhythmInstrumentLevel(rhythm, velocity) > 0)
                keyBits = static_cast<uint8_t>(keyBits | (1u << rhythm));
        }
        return keyBits;
    }

    uint8_t opnaAdpcmBLevelForPatch(float velocity) const
    {
        auto rhythmTrim = 0.0;
        for (size_t rhythm = 0; rhythm < currentOpnaRhythmLevels.size(); ++rhythm)
        {
            if (channelEnabled(rhythm))
                rhythmTrim = std::max(rhythmTrim, sourceLevel(patch, rhythm));
        }
        const auto level = clamp01(velocity) * clamp01(patch.control4) * rhythmTrim;
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(level * 255.0)), 0, 255));
    }

    uint16_t opnaAdpcmBEndForLoadedSample() const
    {
        const auto copiedBytes = host.adpcmBCopiedBytes();
        if (copiedBytes <= 1u)
            return 0;
        const auto end = (copiedBytes - 1u) >> 2u;
        return static_cast<uint16_t>(std::min<size_t>(end, 0xffffu));
    }

    uint16_t opnaAdpcmBDeltaNForNote(int midiNote) const
    {
        const auto semitones = static_cast<double>(std::clamp(midiNote, 0, 127) - 60) / 12.0;
        const auto macroBoost = patch.macro == MacroKind::hit ? 1.18 : 1.0;
        const auto delta = std::round(0x1800 * std::pow(2.0, semitones) * macroBoost);
        return static_cast<uint16_t>(std::clamp(static_cast<int>(delta), 0x0100, 0xffff));
    }

    void keyOffOpnaAdpcmB()
    {
        const auto wasExecuting = (regs[0x100] & 0x80u) != 0u || (opnaAdpcmBControlRegister & 0x80u) != 0u;
        opnaAdpcmBControlRegister = host.adpcmBLoaded() ? 0x20u : 0x00u;
        if (chip && wasExecuting)
            writeYmRegister(0x100u, opnaAdpcmBControlRegister);
    }

    void triggerOpnaAdpcmB(int midiNote, float velocity, uint8_t keyBits)
    {
        if (! host.adpcmBLoaded() || keyBits == 0)
        {
            keyOffOpnaAdpcmB();
            return;
        }

        const auto level = opnaAdpcmBLevelForPatch(velocity);
        if (level == 0)
        {
            keyOffOpnaAdpcmB();
            return;
        }

        const auto end = opnaAdpcmBEndForLoadedSample();
        const auto delta = opnaAdpcmBDeltaNForNote(midiNote);
        opnaAdpcmBControlRegister = 0xa0u;
        opnaAdpcmBPanRegister = 0xc0u;
        opnaAdpcmBStartRegister = 0;
        opnaAdpcmBEndRegister = end;
        opnaAdpcmBLimitRegister = end;
        opnaAdpcmBDeltaNRegister = delta;
        opnaAdpcmBLevelRegister = level;

        writeYmRegister(0x100u, 0x01u);
        writeYmRegister(0x100u, 0x20u);
        writeYmRegister(0x101u, opnaAdpcmBPanRegister);
        writeYmRegister(0x102u, 0x00u);
        writeYmRegister(0x103u, 0x00u);
        writeYmRegister(0x104u, static_cast<uint8_t>(end & 0xffu));
        writeYmRegister(0x105u, static_cast<uint8_t>((end >> 8u) & 0xffu));
        writeYmRegister(0x109u, static_cast<uint8_t>(delta & 0xffu));
        writeYmRegister(0x10au, static_cast<uint8_t>((delta >> 8u) & 0xffu));
        writeYmRegister(0x10bu, level);
        writeYmRegister(0x10cu, static_cast<uint8_t>(end & 0xffu));
        writeYmRegister(0x10du, static_cast<uint8_t>((end >> 8u) & 0xffu));
        writeYmRegister(0x100u, opnaAdpcmBControlRegister);
    }

    void keyOffOpnaRhythm()
    {
        const auto shouldWriteDump = opnaRhythmKeyBits != 0 || (regs[0x10] & 0x3fu) != 0;
        opnaRhythmKeyBits = 0;
        keyOffOpnaAdpcmB();
        currentOpnaRhythmLevels.fill(0);
        if (chip && shouldWriteDump)
            writeYmRegister(0x10u, 0xbfu);
    }

    void triggerOpnaRhythm(int midiNote, float velocity)
    {
        if (! chip || ! opnaRhythmEnabledForPatch())
            return;

        const auto keyBits = opnaRhythmKeyBitsForPatch(velocity);
        keyOffOpnaRhythm();
        opnaRhythmTotalLevel = 0x3fu;
        writeYmRegister(0x11u, opnaRhythmTotalLevel);

        for (size_t rhythm = 0; rhythm < currentOpnaRhythmLevels.size(); ++rhythm)
        {
            const auto level = opnaRhythmInstrumentLevel(rhythm, velocity);
            currentOpnaRhythmLevels[rhythm] = level;
            const auto pan = ym2612PanBitsForPatch(patch, rhythm);
            writeYmRegister(static_cast<uint16_t>(0x18u + rhythm), static_cast<uint8_t>(pan | level));
        }

        opnaRhythmKeyBits = keyBits;
        triggerOpnaAdpcmB(midiNote, velocity, keyBits);
        if (keyBits != 0)
            writeYmRegister(0x10u, keyBits);
    }

    void applyChannelPatch(size_t channel, float velocity)
    {
        if (channel >= fmChannelCount)
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
        writeYmRegister(regForChannel(0xb4, channel), currentPanBits[channel]);
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

        // control2 is the native OPNA feedback field. It must not alter pitch:
        // feedback changes the operator loop written to $B0, while FNUM/block
        // continue to follow the requested MIDI note.
        const auto pitch = pitchForNote(midiNote);
        channelNotes[channel] = std::clamp(midiNote, 0, 127);
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
        if (channel >= fmChannelCount)
        {
            keyOffSsgChannel(channel);
            return;
        }

        if (channel >= channelNotes.size())
            return;

        writeYmRegister(0x28, keyCodeForChannel(channel));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void clearChipPolyState()
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
            keyOffChannel(channel);
        keyOffOpnaRhythm();
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
        if (opnaRhythmEnabledForPatch())
            triggerOpnaRhythm(midiNote, velocity);
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
        if (opnaRhythmEnabledForPatch())
            keyOffOpnaRhythm();
    }

    void applyLaserDrift()
    {
        laserPhase += 1.0 / sampleRate;
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 8.0) * patch.control3 * 10.0));
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

    uint8_t ssgVolumeForChannel(size_t channel, float velocity) const
    {
        const auto base = clamp01(velocity) * clamp01(patch.control4) * sourceLevel(patch, channel);
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
            writeSsgVolume(ssg, ssgVolumeForChannel(channel, velocity));
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

    double ssgOutputSample() const
    {
        if ((ssgGateMask & 0x01c0u) == 0)
            return 0.0;

        constexpr auto scale = 1.0 / 32768.0;
        return std::clamp(static_cast<double>(lastSsg) * scale, -1.0, 1.0);
    }

    AccuracyMode accuracy;
    static constexpr size_t fmChannelCount = 6;
    static constexpr size_t ssgChannelCount = 3;
    static constexpr size_t ssgLaneOffset = 6;
    static constexpr size_t sourceChannelCount = 9;
    double sampleRate = 48000.0;
    double clock = 7987200.0;
    double chipSampleRate = 332800.0;
    double sampleAccumulator = 0.0;
    Host host;
    std::unique_ptr<ymfm::ym2608> chip;
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
    std::array<uint16_t, 3> currentSsgPeriod {};
    std::array<uint8_t, 3> currentSsgVolume {};
    std::array<uint8_t, 6> currentOpnaRhythmLevels {};
    std::array<int, 9> channelNotes {};
    std::array<float, 9> channelVelocity {};
    std::array<uint64_t, 9> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    uint16_t ssgGateMask = 0;
    uint8_t opnaRhythmKeyBits = 0;
    uint8_t opnaRhythmTotalLevel = 0x3f;
    uint8_t opnaAdpcmBControlRegister = 0;
    uint8_t opnaAdpcmBPanRegister = 0;
    uint16_t opnaAdpcmBStartRegister = 0;
    uint16_t opnaAdpcmBEndRegister = 0;
    uint16_t opnaAdpcmBLimitRegister = 0;
    uint16_t opnaAdpcmBDeltaNRegister = 0;
    uint8_t opnaAdpcmBLevelRegister = 0;
    double laserPhase = 0.0;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    int32_t lastSsg = 0;
    StereoFrame currentOutput {};
};

}
std::unique_ptr<ChipCore> makeYm2608Core(AccuracyMode accuracy)
{
    return std::make_unique<Ym2608Core>(accuracy);
}
}
