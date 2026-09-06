#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Opl3Core final : public ChipCore
{
public:
    explicit Opl3Core(AccuracyMode requested) : accuracy(requested) { channelNotes.fill(-1); }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : ymf262NativeClockHz;
        if (chip == nullptr)
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
        writeOplRegister(0x104, 0x00);
        writeOplRegister(0x01, 0x20);
        applyPatchToAllChannels(false);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled
            || oplRhythmModeForPatch(nextPatch) != oplRhythmModeForPatch(patch)
            || opl18ChannelLayerForPatch(nextPatch) != opl18ChannelLayerForPatch(patch)
            || oplFourOperatorPairForPatch(nextPatch) != oplFourOperatorPairForPatch(patch))
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
            if (! channelPlayableForMode(channel))
                continue;
            triggerChannel(channel, notes[channel], baseVelocity, sourceEnabledForChannel(channel));
            if (layer18ModeActive())
            {
                const auto highChannel = channel + visibleChannelCount;
                triggerChannel(highChannel,
                               notes[channel] + secondBankIntervalForPatch(),
                               baseVelocity * 0.82f,
                               sourceEnabledForChannel(highChannel));
            }
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
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "OPL2/OPL3 / DOS FM"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPL3/YMF262 register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YMF262/OPL3 synthesis core for this OPL2/OPL3 mode pass. Chipper exposes nine OPL2-compatible two-operator lanes, native $BD rhythm-mode key bits, an explicit 18-channel OPL3 layer mode that maps each visible source card to a low/high-bank YMF262 channel pair, and shared editable $104 four-operator pairs for channels 1+4, 2+5, and 3+6. Per-lane operator patches, dedicated 18-card editing, LFO/tremolo/vibrato controls, rhythm-instrument fine tuning, golden comparisons, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        const auto operatorValue = [this](uint16_t base, size_t op)
        {
            const auto channel = op < 2u ? size_t { 0u } : size_t { 3u };
            return regs[operatorRegister(channel, base, (op % 2u) != 0u)];
        };

        const auto rhythmOperatorValue = [this](uint16_t base, size_t op)
        {
            const auto channel = size_t { 6u } + (op / 2u);
            return regs[operatorRegister(channel, base, (op % 2u) != 0u)];
        };

        std::ostringstream json;
        json << "{"
             << "\"mode\":\"OPL2/OPL3 / DOS FM\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPL3/YMF262 register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"oplCore\":\"YMF262\","
             << "\"opl3Core\":1,"
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"81aec25ccbb98f4873a255f7551ac4dadac59b4a\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":18,"
             << "\"exposedChannelCount\":9,"
             << "\"activeNativeChannelCount\":" << playableChannelCount() << ","
             << "\"opl3EighteenChannelLayer\":" << (layer18ModeActive() ? 1 : 0) << ","
             << "\"pairedHighBankSourceCards\":" << (layer18ModeActive() ? 9 : 0) << ","
             << "\"opl3FourOperatorPairMode\":" << (fourOperatorModeActive() ? 1 : 0) << ","
             << "\"opl3FourOperatorRegister\":" << static_cast<int>(regs[0x104]) << ","
             << "\"opl3FourOperatorPairCount\":" << fourOperatorPairCount() << ","
             << "\"opl3FourOperatorPlayableChannels\":" << modePlayableChannelCount() << ","
             << "\"opl3FourOperatorSecondaryMuted\":" << (fourOperatorModeActive() ? 1 : 0) << ","
             << "\"opl3FourOperatorAlgorithm\":" << static_cast<int>(oplFourOperatorAlgorithmForPatch(patch)) << ","
             << "\"opl3FourOperatorPrimaryConnection\":" << static_cast<int>(oplConnectionForOperatorStage(patch, 0)) << ","
             << "\"opl3FourOperatorSecondaryConnection\":" << static_cast<int>(oplConnectionForOperatorStage(patch, 1)) << ","
             << "\"opl3NewFlag\":" << static_cast<int>(regs[0x105] & 0x01u) << ","
             << "\"opl3NewModeRegister\":" << static_cast<int>(regs[0x105]) << ","
             << "\"waveform0\":" << static_cast<int>(currentWaveform[0]) << ","
             << "\"waveform9\":" << static_cast<int>(currentWaveform[9]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"feedback9\":" << static_cast<int>(currentFeedback[9]) << ","
             << "\"carrierControl0\":" << static_cast<int>(currentCarrierControl[0]) << ","
             << "\"carrierEgt0\":" << (((currentCarrierControl[0] & 0x20u) != 0u) ? 1 : 0) << ","
             << "\"carrierAttackDecay0\":" << static_cast<int>(currentCarrierAttackDecay[0]) << ","
             << "\"carrierSustainRelease0\":" << static_cast<int>(currentCarrierSustainRelease[0]) << ","
             << "\"operatorControl0\":" << static_cast<int>(operatorValue(0x20u, 0)) << ","
             << "\"operatorControl1\":" << static_cast<int>(operatorValue(0x20u, 1)) << ","
             << "\"operatorControl2\":" << static_cast<int>(operatorValue(0x20u, 2)) << ","
             << "\"operatorControl3\":" << static_cast<int>(operatorValue(0x20u, 3)) << ","
             << "\"operatorFlagChoice0\":" << std::clamp(patch.opmOperatorDt1[0], 0, 8) << ","
             << "\"operatorFlagChoice1\":" << std::clamp(patch.opmOperatorDt1[1], 0, 8) << ","
             << "\"operatorFlagChoice2\":" << std::clamp(patch.opmOperatorDt1[2], 0, 8) << ","
             << "\"operatorFlagChoice3\":" << std::clamp(patch.opmOperatorDt1[3], 0, 8) << ","
             << "\"operatorFlagBits0\":" << static_cast<int>(operatorValue(0x20u, 0) & 0xd0u) << ","
             << "\"operatorFlagBits1\":" << static_cast<int>(operatorValue(0x20u, 1) & 0xd0u) << ","
             << "\"operatorFlagBits2\":" << static_cast<int>(operatorValue(0x20u, 2) & 0xd0u) << ","
             << "\"operatorFlagBits3\":" << static_cast<int>(operatorValue(0x20u, 3) & 0xd0u) << ","
             << "\"operatorKslChoice0\":" << std::clamp(patch.opmOperatorDt2[0], 0, 4) << ","
             << "\"operatorKslChoice1\":" << std::clamp(patch.opmOperatorDt2[1], 0, 4) << ","
             << "\"operatorKslChoice2\":" << std::clamp(patch.opmOperatorDt2[2], 0, 4) << ","
             << "\"operatorKslChoice3\":" << std::clamp(patch.opmOperatorDt2[3], 0, 4) << ","
             << "\"operatorKslLevel0\":" << static_cast<int>(oplOperatorKeyScaleLevelForPatch(patch, 0)) << ","
             << "\"operatorKslLevel1\":" << static_cast<int>(oplOperatorKeyScaleLevelForPatch(patch, 1)) << ","
             << "\"operatorKslLevel2\":" << static_cast<int>(oplOperatorKeyScaleLevelForPatch(patch, 2)) << ","
             << "\"operatorKslLevel3\":" << static_cast<int>(oplOperatorKeyScaleLevelForPatch(patch, 3)) << ","
             << "\"operatorTotalLevelRegister0\":" << static_cast<int>(operatorValue(0x40u, 0)) << ","
             << "\"operatorTotalLevelRegister1\":" << static_cast<int>(operatorValue(0x40u, 1)) << ","
             << "\"operatorTotalLevelRegister2\":" << static_cast<int>(operatorValue(0x40u, 2)) << ","
             << "\"operatorTotalLevelRegister3\":" << static_cast<int>(operatorValue(0x40u, 3)) << ","
             << "\"rhythmOperatorControl0\":" << static_cast<int>(rhythmOperatorValue(0x20u, 0)) << ","
             << "\"rhythmOperatorControl1\":" << static_cast<int>(rhythmOperatorValue(0x20u, 1)) << ","
             << "\"rhythmOperatorControl2\":" << static_cast<int>(rhythmOperatorValue(0x20u, 2)) << ","
             << "\"rhythmOperatorControl3\":" << static_cast<int>(rhythmOperatorValue(0x20u, 3)) << ","
             << "\"rhythmOperatorControl4\":" << static_cast<int>(rhythmOperatorValue(0x20u, 4)) << ","
             << "\"rhythmOperatorControl5\":" << static_cast<int>(rhythmOperatorValue(0x20u, 5)) << ","
             << "\"rhythmOperatorFlagBits0\":" << static_cast<int>(rhythmOperatorValue(0x20u, 0) & 0xd0u) << ","
             << "\"rhythmOperatorFlagBits1\":" << static_cast<int>(rhythmOperatorValue(0x20u, 1) & 0xd0u) << ","
             << "\"rhythmOperatorTotalLevelRegister0\":" << static_cast<int>(rhythmOperatorValue(0x40u, 0)) << ","
             << "\"rhythmOperatorTotalLevelRegister1\":" << static_cast<int>(rhythmOperatorValue(0x40u, 1)) << ","
             << "\"rhythmOperatorTotalLevelRegister2\":" << static_cast<int>(rhythmOperatorValue(0x40u, 2)) << ","
             << "\"rhythmOperatorTotalLevelRegister3\":" << static_cast<int>(rhythmOperatorValue(0x40u, 3)) << ","
             << "\"rhythmOperatorTotalLevelRegister4\":" << static_cast<int>(rhythmOperatorValue(0x40u, 4)) << ","
             << "\"rhythmOperatorTotalLevelRegister5\":" << static_cast<int>(rhythmOperatorValue(0x40u, 5)) << ","
             << "\"rhythmOperatorKslBits0\":" << static_cast<int>(rhythmOperatorValue(0x40u, 0) & 0xc0u) << ","
             << "\"rhythmOperatorKslBits1\":" << static_cast<int>(rhythmOperatorValue(0x40u, 1) & 0xc0u) << ","
             << "\"connectionRegister0\":" << static_cast<int>(regs[0xc0]) << ","
             << "\"connectionRegister3\":" << static_cast<int>(regs[0xc3]) << ","
             << "\"connectionRegister6\":" << static_cast<int>(regs[0xc6]) << ","
             << "\"connectionRegister7\":" << static_cast<int>(regs[0xc7]) << ","
             << "\"connectionRegister8\":" << static_cast<int>(regs[0xc8]) << ","
             << "\"highBankConnectionRegister0\":" << static_cast<int>(regs[0x1c0]) << ","
             << "\"opl3StereoRouteChoice\":" << std::clamp(patch.dmgStereoRoute, 0, 4) << ","
             << "\"opl3OutputSelect0\":" << static_cast<int>(regs[0xc0] & 0xf0u) << ","
             << "\"opl3OutputSelect1\":" << static_cast<int>(regs[0xc1] & 0xf0u) << ","
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
             << "\"operatorMultiple0\":" << static_cast<int>(operatorValue(0x20u, 0) & 0x0fu) << ","
             << "\"operatorMultiple1\":" << static_cast<int>(operatorValue(0x20u, 1) & 0x0fu) << ","
             << "\"operatorMultiple2\":" << static_cast<int>(operatorValue(0x20u, 2) & 0x0fu) << ","
             << "\"operatorMultiple3\":" << static_cast<int>(operatorValue(0x20u, 3) & 0x0fu) << ","
             << "\"operatorTotalLevel0\":" << static_cast<int>(operatorValue(0x40u, 0) & 0x3fu) << ","
             << "\"operatorTotalLevel1\":" << static_cast<int>(operatorValue(0x40u, 1) & 0x3fu) << ","
             << "\"operatorTotalLevel2\":" << static_cast<int>(operatorValue(0x40u, 2) & 0x3fu) << ","
             << "\"operatorTotalLevel3\":" << static_cast<int>(operatorValue(0x40u, 3) & 0x3fu) << ","
             << "\"operatorAttackRate0\":" << static_cast<int>((operatorValue(0x60u, 0) >> 4u) & 0x0fu) << ","
             << "\"operatorAttackRate1\":" << static_cast<int>((operatorValue(0x60u, 1) >> 4u) & 0x0fu) << ","
             << "\"operatorAttackRate2\":" << static_cast<int>((operatorValue(0x60u, 2) >> 4u) & 0x0fu) << ","
             << "\"operatorAttackRate3\":" << static_cast<int>((operatorValue(0x60u, 3) >> 4u) & 0x0fu) << ","
             << "\"operatorDecayRate0\":" << static_cast<int>(operatorValue(0x60u, 0) & 0x0fu) << ","
             << "\"operatorDecayRate1\":" << static_cast<int>(operatorValue(0x60u, 1) & 0x0fu) << ","
             << "\"operatorDecayRate2\":" << static_cast<int>(operatorValue(0x60u, 2) & 0x0fu) << ","
             << "\"operatorDecayRate3\":" << static_cast<int>(operatorValue(0x60u, 3) & 0x0fu) << ","
             << "\"operatorSustainLevel0\":" << static_cast<int>((operatorValue(0x80u, 0) >> 4u) & 0x0fu) << ","
             << "\"operatorSustainLevel1\":" << static_cast<int>((operatorValue(0x80u, 1) >> 4u) & 0x0fu) << ","
             << "\"operatorSustainLevel2\":" << static_cast<int>((operatorValue(0x80u, 2) >> 4u) & 0x0fu) << ","
             << "\"operatorSustainLevel3\":" << static_cast<int>((operatorValue(0x80u, 3) >> 4u) & 0x0fu) << ","
             << "\"operatorReleaseRate0\":" << static_cast<int>(operatorValue(0x80u, 0) & 0x0fu) << ","
             << "\"operatorReleaseRate1\":" << static_cast<int>(operatorValue(0x80u, 1) & 0x0fu) << ","
             << "\"operatorReleaseRate2\":" << static_cast<int>(operatorValue(0x80u, 2) & 0x0fu) << ","
             << "\"operatorReleaseRate3\":" << static_cast<int>(operatorValue(0x80u, 3) & 0x0fu) << ","
             << "\"opl3HighBankOutputSelect0\":" << static_cast<int>(regs[0x1c0] & 0xf0u) << ","
             << "\"rhythmModeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"rhythmMode\":" << (rhythmModeActive() ? 1 : 0) << ","
             << "\"rhythmRegister\":" << static_cast<int>(regs[0xbd]) << ","
             << "\"rhythmKeyBits\":" << static_cast<int>(rhythmKeyBits) << ","
             << "\"rhythmBdLevel\":" << static_cast<int>(regs[0x53] & 0x3fu) << ","
             << "\"rhythmHatLevel\":" << static_cast<int>(regs[0x51] & 0x3fu) << ","
             << "\"rhythmSnareLevel\":" << static_cast<int>(regs[0x54] & 0x3fu) << ","
             << "\"rhythmTomLevel\":" << static_cast<int>(regs[0x52] & 0x3fu) << ","
             << "\"rhythmCymLevel\":" << static_cast<int>(regs[0x55] & 0x3fu) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"fnum9\":" << currentFnum[9] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"block9\":" << static_cast<int>(currentBlock[9]) << ","
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
             << "\"assignedNote9\":" << channelNotes[9] << ","
             << "\"assignedNote10\":" << channelNotes[10] << ","
             << "\"assignedNote11\":" << channelNotes[11] << ","
             << "\"assignedNote12\":" << channelNotes[12] << ","
             << "\"assignedNote13\":" << channelNotes[13] << ","
             << "\"assignedNote14\":" << channelNotes[14] << ","
             << "\"assignedNote15\":" << channelNotes[15] << ","
             << "\"assignedNote16\":" << channelNotes[16] << ","
             << "\"assignedNote17\":" << channelNotes[17] << ","
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

    static constexpr size_t visibleChannelCount = 9;
    static constexpr size_t nativeChannelCount = 18;

    static uint16_t bankBaseForChannel(size_t channel)
    {
        return channel >= visibleChannelCount ? 0x100u : 0x000u;
    }

    static uint8_t channelSlot(size_t channel)
    {
        return static_cast<uint8_t>(channel % visibleChannelCount);
    }

    static size_t visibleSourceIndexForChannel(size_t channel)
    {
        return channel % visibleChannelCount;
    }

    static uint16_t operatorRegister(size_t channel, uint16_t base, bool carrier)
    {
        return static_cast<uint16_t>(bankBaseForChannel(channel) + base + opOffset(channel, carrier));
    }

    static uint16_t channelRegister(size_t channel, uint16_t base)
    {
        return static_cast<uint16_t>(bankBaseForChannel(channel) + base + channelSlot(channel));
    }

    bool layer18ModeActive() const
    {
        return opl18ChannelLayerForPatch(patch);
    }

    bool fourOperatorModeActive() const
    {
        return oplFourOperatorPairForPatch(patch);
    }

    size_t playableChannelCount() const
    {
        return layer18ModeActive() ? nativeChannelCount : visibleChannelCount;
    }

    bool fourOperatorSecondaryChannel(size_t channel) const
    {
        if (! fourOperatorModeActive())
            return false;
        const auto slot = channelSlot(channel);
        return slot >= 3u && slot <= 5u;
    }

    bool channelPlayableForMode(size_t channel) const
    {
        return channel < playableChannelCount() && ! fourOperatorSecondaryChannel(channel);
    }

    size_t modePlayableChannelCount() const
    {
        size_t count = 0;
        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if (channelPlayableForMode(channel))
                ++count;
        }
        return count;
    }

    size_t fourOperatorPairCount() const
    {
        auto bits = oplFourOperatorEnableRegisterForPatch(patch);
        size_t count = 0;
        for (auto bit = 0u; bit < 6u; ++bit)
        {
            if ((bits & (1u << bit)) != 0u)
                ++count;
        }
        return count;
    }

    bool sourceEnabledForChannel(size_t channel) const
    {
        return sourceEnabled(patch, visibleSourceIndexForChannel(channel));
    }

    float sourceLevelForChannel(size_t channel) const
    {
        return sourceLevel(patch, visibleSourceIndexForChannel(channel));
    }

    int secondBankIntervalForPatch() const
    {
        switch (patch.macro)
        {
            case MacroKind::bass: return 12;
            case MacroKind::arp: return 7;
            case MacroKind::coin:
            case MacroKind::lead:
            case MacroKind::jump:
            case MacroKind::powerUp: return 12;
            case MacroKind::laser: return -12;
            case MacroKind::drum:
            case MacroKind::hit:
            case MacroKind::manual:
            default: break;
        }

        return 0;
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
        const auto wave = waveformForPatch();
        const auto feedback = feedbackForPatch();
        const auto levelVelocity = static_cast<float>(clamp01(velocity) * sourceLevelForChannel(channel));
        const auto slot = channelSlot(channel);
        const auto pairedSecondStage = fourOperatorModeActive() && slot >= 3u && slot <= 5u;
        const auto operatorBase = pairedSecondStage ? size_t { 2u } : size_t { 0u };

        currentWaveform[channel] = wave;
        currentFeedback[channel] = feedback;

        const auto melodicSustain = patch.macro != MacroKind::drum
            && patch.macro != MacroKind::hit
            && patch.macro != MacroKind::coin
            && patch.macro != MacroKind::jump;
        for (size_t physicalOperator = 0; physicalOperator < 2u; ++physicalOperator)
        {
            const auto op = operatorBase + physicalOperator;
            const auto carrier = physicalOperator != 0u;
            const auto envelope = oplOperatorEnvelopeRegistersForPatch(patch, op);
            const auto operatorControl = static_cast<uint8_t>(oplOperatorFlagBitsForPatch(patch, op)
                                                               | (melodicSustain ? 0x20u : 0x00u)
                                                               | oplOperatorMultipleForPatch(patch, op));
            const auto totalLevel = static_cast<uint8_t>(oplOperatorKeyScaleLevelBitsForPatch(patch, op)
                                                          | (oplOperatorTotalLevelForPatch(patch, op, levelVelocity) & 0x3fu));
            const auto attackDecay = static_cast<uint8_t>((envelope.attackRate << 4u) | envelope.decayRate);
            const auto sustainRelease = static_cast<uint8_t>((envelope.sustainLevel << 4u) | envelope.releaseRate);

            writeOplRegister(operatorRegister(channel, 0x20u, carrier), operatorControl);
            writeOplRegister(operatorRegister(channel, 0x40u, carrier), totalLevel);
            writeOplRegister(operatorRegister(channel, 0x60u, carrier), attackDecay);
            writeOplRegister(operatorRegister(channel, 0x80u, carrier), sustainRelease);
            writeOplRegister(operatorRegister(channel, 0xe0u, carrier), wave);

            if (carrier)
            {
                currentCarrierControl[channel] = operatorControl;
                currentCarrierAttackDecay[channel] = attackDecay;
                currentCarrierSustainRelease[channel] = sustainRelease;
            }
        }

        const auto connectionStage = pairedSecondStage ? size_t { 1u } : size_t { 0u };
        const auto connection = oplConnectionForOperatorStage(patch, connectionStage);
        writeOplRegister(channelRegister(channel, 0xc0u),
                         static_cast<uint8_t>(oplOutputSelectBitsForPatch(patch, channel) | (feedback << 1u) | connection));
    }

    void applyRhythmOperatorNativeFields()
    {
        for (size_t channel = 6u; channel < visibleChannelCount; ++channel)
        {
            for (size_t physicalOperator = 0; physicalOperator < 2u; ++physicalOperator)
            {
                const auto carrier = physicalOperator != 0u;
                const auto controlRegister = operatorRegister(channel, 0x20u, carrier);
                const auto totalLevelRegister = operatorRegister(channel, 0x40u, carrier);
                const auto operatorControl = static_cast<uint8_t>((regs[controlRegister] & 0x2fu)
                                                                   | oplOperatorFlagBitsForPatch(patch, physicalOperator));
                const auto totalLevel = static_cast<uint8_t>((regs[totalLevelRegister] & 0x3fu)
                                                              | oplOperatorKeyScaleLevelBitsForPatch(patch, physicalOperator));
                writeOplRegister(controlRegister, operatorControl);
                writeOplRegister(totalLevelRegister, totalLevel);
                if (carrier)
                    currentCarrierControl[channel] = operatorControl;
            }
        }
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        writeOplRegister(0x104, oplFourOperatorEnableRegisterForPatch(patch));
        if (rhythmModeActive())
            applyRhythmOperatorNativeFields();

        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if (rhythmModeActive() && channel >= 6u)
            {
                const auto outputRegister = channelRegister(channel, 0xc0u);
                const auto lowBits = static_cast<uint8_t>(regs[outputRegister] & 0x0fu);
                writeOplRegister(outputRegister,
                                 static_cast<uint8_t>(oplOutputSelectBitsForPatch(patch, channel) | lowBits));
                continue;
            }
            applyChannelPatch(channel, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f);
        }

        if (! preserveKeys)
            return;

        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if ((keyOnMask & (1u << channel)) != 0 && channelNotes[channel] >= 0)
            {
                if (rhythmModeActive() && channel >= 6u)
                    continue;
                if (! channelPlayableForMode(channel))
                    continue;
                triggerChannel(channel, channelNotes[channel], channelVelocity[channel], sourceEnabledForChannel(channel));
            }
        }
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= channelNotes.size() || ! chip)
            return;
        if (fourOperatorSecondaryChannel(channel))
        {
            keyOffChannel(channel);
            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            return;
        }
        const auto pitch = pitchForNote(midiNote);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity));
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        applyChannelPatch(channel, channelVelocity[channel]);
        if (fourOperatorModeActive() && channelSlot(channel) < 3u)
        {
            const auto pairedOperatorChannel = channel + 3u;
            if (pairedOperatorChannel < playableChannelCount())
                applyChannelPatch(pairedOperatorChannel, channelVelocity[channel]);
        }
        writeOplRegister(channelRegister(channel, 0xa0u), static_cast<uint8_t>(pitch.fnum & 0xffu));
        writeOplRegister(channelRegister(channel, 0xb0u), static_cast<uint8_t>((shouldEnable ? 0x20u : 0x00u) | ((pitch.block & 0x07u) << 2u) | ((pitch.fnum >> 8u) & 0x03u)));
        if (shouldEnable)
            keyOnMask |= uint32_t { 1 } << channel;
        else
            keyOnMask &= ~(uint32_t { 1 } << channel);
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= channelNotes.size())
            return;
        if (rhythmModeActive() && channel >= 6u && channel < visibleChannelCount)
        {
            writeRhythmRegister(0);
            keyOnMask &= ~(uint32_t { 1 } << channel);
            return;
        }
        const auto keyRegister = channelRegister(channel, 0xb0u);
        writeOplRegister(keyRegister, static_cast<uint8_t>(regs[keyRegister] & static_cast<uint8_t>(~0x20u)));
        keyOnMask &= ~(uint32_t { 1 } << channel);
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
        if (channel < 6u || channel >= visibleChannelCount)
            return;
        const auto pitch = pitchForNote(midiNote);
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        writeOplRegister(channelRegister(channel, 0xa0u), static_cast<uint8_t>(pitch.fnum & 0xffu));
        writeOplRegister(channelRegister(channel, 0xb0u),
                         static_cast<uint8_t>(((pitch.block & 0x07u) << 2u) | ((pitch.fnum >> 8u) & 0x03u)));
    }

    void triggerRhythm(int midiNote, float velocity)
    {
        if (! rhythmModeActive() || ! chip)
            return;

        writeRhythmRegister(0);
        for (size_t channel = 6; channel < visibleChannelCount; ++channel)
            applyChannelPatch(channel, velocity);

        writeRhythmPitch(6, midiNote - 24);
        writeRhythmPitch(7, midiNote + 7);
        writeRhythmPitch(8, midiNote);

        const auto bdLevel = rhythmTotalLevel(6, velocity);
        const auto hatSnareLevel = rhythmTotalLevel(7, velocity);
        const auto tomCymLevel = rhythmTotalLevel(8, velocity);
        writeOplRegister(0x53u, static_cast<uint8_t>((regs[0x53u] & 0xc0u) | bdLevel));
        writeOplRegister(0x51u, static_cast<uint8_t>((regs[0x51u] & 0xc0u) | hatSnareLevel));
        writeOplRegister(0x54u, static_cast<uint8_t>((regs[0x54u] & 0xc0u) | hatSnareLevel));
        writeOplRegister(0x52u, static_cast<uint8_t>((regs[0x52u] & 0xc0u) | tomCymLevel));
        writeOplRegister(0x55u, static_cast<uint8_t>((regs[0x55u] & 0xc0u) | tomCymLevel));

        uint8_t keyBits = 0;
        if (sourceEnabled(patch, 6))
            keyBits = static_cast<uint8_t>(keyBits | 0x10u); // Bass drum.
        if (sourceEnabled(patch, 7))
            keyBits = static_cast<uint8_t>(keyBits | 0x09u); // Hi-hat + snare.
        if (sourceEnabled(patch, 8))
            keyBits = static_cast<uint8_t>(keyBits | 0x06u); // Tom + cymbal.

        writeRhythmRegister(keyBits);

        if ((keyBits & 0x10u) != 0)
            keyOnMask |= uint32_t { 1 } << 6u;
        if ((keyBits & 0x09u) != 0)
            keyOnMask |= uint32_t { 1 } << 7u;
        if ((keyBits & 0x06u) != 0)
            keyOnMask |= uint32_t { 1 } << 8u;

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
        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if (channelPlayableForMode(channel) && sourceEnabledForChannel(channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }
        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if (channelPlayableForMode(channel) && sourceEnabledForChannel(channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }
        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if (channelPlayableForMode(channel) && sourceEnabledForChannel(channel) && channelStamp[channel] < oldestStamp)
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
        for (size_t channel = 0; channel < playableChannelCount(); ++channel)
        {
            if (channelPlayableForMode(channel) && sourceEnabledForChannel(channel) && channelNotes[channel] >= 0)
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
            if (! channelPlayableForMode(channel))
                continue;
            if ((keyOnMask & (1u << channel)) == 0)
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, sourceEnabledForChannel(channel));
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
    std::array<uint16_t, nativeChannelCount> currentFnum {};
    std::array<uint8_t, nativeChannelCount> currentBlock {};
    std::array<uint8_t, nativeChannelCount> currentWaveform {};
    std::array<uint8_t, nativeChannelCount> currentFeedback {};
    std::array<uint8_t, nativeChannelCount> currentCarrierControl {};
    std::array<uint8_t, nativeChannelCount> currentCarrierAttackDecay {};
    std::array<uint8_t, nativeChannelCount> currentCarrierSustainRelease {};
    std::array<int, nativeChannelCount> channelNotes {};
    std::array<float, nativeChannelCount> channelVelocity {};
    std::array<uint64_t, nativeChannelCount> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint32_t keyOnMask = 0;
    uint8_t rhythmKeyBits = 0;
    double laserPhase = 0.0;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    StereoFrame currentOutput {};
};

}
std::unique_ptr<ChipCore> makeOpl3Core(AccuracyMode accuracy)
{
    return std::make_unique<Opl3Core>(accuracy);
}
}
