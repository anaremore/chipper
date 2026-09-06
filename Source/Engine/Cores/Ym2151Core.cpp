#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Ym2151Core final : public ChipCore
{
public:
    explicit Ym2151Core(AccuracyMode requested) : accuracy(requested) { channelNotes.fill(-1); }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        if (chip == nullptr)
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
        currentLfoRate = 0;
        currentLfoAmDepth = 0;
        currentLfoPmDepth = 0;
        currentLfoWaveform = 0;
        currentLfoPmSensitivity = 0;
        currentLfoAmSensitivity = 0;
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
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "YM2151 arcade/X68000 FM"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPM register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YM2151/OPM synthesis core. Chipper currently maps musical controls and notes to OPM operator multiplier plus per-operator DT1/DT2, envelopes, algorithm, feedback, key-code/key-fraction, pan, $0F channel-8 noise, direct LFO waveform/PM/AM depth and sensitivity, and key-on registers for all eight melodic lanes. Independent per-channel patches, exact OPM noise timing/hardware comparison, timers, CSM behavior, deeper per-operator ADSR visualization, golden comparisons, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        const auto operatorCarrier = [this](size_t op)
        {
            return fmOperatorIsCarrierForAlgorithm(currentAlgorithm[0], op) ? 1 : 0;
        };

        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2151 arcade/X68000 FM\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPM register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"81aec25ccbb98f4873a255f7551ac4dadac59b4a\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":8,"
             << "\"exposedChannelCount\":8,"
             << "\"algorithm0\":" << static_cast<int>(currentAlgorithm[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
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
             << "\"opmOperatorDt1Choice0\":" << patch.opmOperatorDt1[0] << ","
             << "\"opmOperatorDt1Choice1\":" << patch.opmOperatorDt1[1] << ","
             << "\"opmOperatorDt1Choice2\":" << patch.opmOperatorDt1[2] << ","
             << "\"opmOperatorDt1Choice3\":" << patch.opmOperatorDt1[3] << ","
             << "\"opmOperatorDt2Choice0\":" << patch.opmOperatorDt2[0] << ","
             << "\"opmOperatorDt2Choice1\":" << patch.opmOperatorDt2[1] << ","
             << "\"opmOperatorDt2Choice2\":" << patch.opmOperatorDt2[2] << ","
             << "\"opmOperatorDt2Choice3\":" << patch.opmOperatorDt2[3] << ","
             << "\"operatorMultiple0\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 0))] & 0x0fu) << ","
             << "\"operatorMultiple1\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 1))] & 0x0fu) << ","
             << "\"operatorMultiple2\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 2))] & 0x0fu) << ","
             << "\"operatorMultiple3\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 3))] & 0x0fu) << ","
             << "\"operatorDt10\":" << static_cast<int>((regs[static_cast<uint8_t>(0x40 + opOffset(0, 0))] >> 4u) & 0x07u) << ","
             << "\"operatorDt11\":" << static_cast<int>((regs[static_cast<uint8_t>(0x40 + opOffset(0, 1))] >> 4u) & 0x07u) << ","
             << "\"operatorDt12\":" << static_cast<int>((regs[static_cast<uint8_t>(0x40 + opOffset(0, 2))] >> 4u) & 0x07u) << ","
             << "\"operatorDt13\":" << static_cast<int>((regs[static_cast<uint8_t>(0x40 + opOffset(0, 3))] >> 4u) & 0x07u) << ","
             << "\"operatorMultipleDt1Register0\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 0))]) << ","
             << "\"operatorMultipleDt1Register1\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 1))]) << ","
             << "\"operatorMultipleDt1Register2\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 2))]) << ","
             << "\"operatorMultipleDt1Register3\":" << static_cast<int>(regs[static_cast<uint8_t>(0x40 + opOffset(0, 3))]) << ","
             << "\"operatorAttackRate0\":" << static_cast<int>(regs[static_cast<uint8_t>(0x80 + opOffset(0, 0))]) << ","
             << "\"operatorAttackRate1\":" << static_cast<int>(regs[static_cast<uint8_t>(0x80 + opOffset(0, 1))]) << ","
             << "\"operatorAttackRate2\":" << static_cast<int>(regs[static_cast<uint8_t>(0x80 + opOffset(0, 2))]) << ","
             << "\"operatorAttackRate3\":" << static_cast<int>(regs[static_cast<uint8_t>(0x80 + opOffset(0, 3))]) << ","
             << "\"operatorDecayRate0\":" << static_cast<int>(regs[static_cast<uint8_t>(0xa0 + opOffset(0, 0))]) << ","
             << "\"operatorDecayRate1\":" << static_cast<int>(regs[static_cast<uint8_t>(0xa0 + opOffset(0, 1))]) << ","
             << "\"operatorDecayRate2\":" << static_cast<int>(regs[static_cast<uint8_t>(0xa0 + opOffset(0, 2))]) << ","
             << "\"operatorDecayRate3\":" << static_cast<int>(regs[static_cast<uint8_t>(0xa0 + opOffset(0, 3))]) << ","
             << "\"operatorSustainRate0\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 0))] & 0x1fu) << ","
             << "\"operatorSustainRate1\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 1))] & 0x1fu) << ","
             << "\"operatorSustainRate2\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 2))] & 0x1fu) << ","
             << "\"operatorSustainRate3\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 3))] & 0x1fu) << ","
             << "\"operatorDt20\":" << static_cast<int>((regs[static_cast<uint8_t>(0xc0 + opOffset(0, 0))] >> 6u) & 0x03u) << ","
             << "\"operatorDt21\":" << static_cast<int>((regs[static_cast<uint8_t>(0xc0 + opOffset(0, 1))] >> 6u) & 0x03u) << ","
             << "\"operatorDt22\":" << static_cast<int>((regs[static_cast<uint8_t>(0xc0 + opOffset(0, 2))] >> 6u) & 0x03u) << ","
             << "\"operatorDt23\":" << static_cast<int>((regs[static_cast<uint8_t>(0xc0 + opOffset(0, 3))] >> 6u) & 0x03u) << ","
             << "\"operatorDt2SustainRateRegister0\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 0))]) << ","
             << "\"operatorDt2SustainRateRegister1\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 1))]) << ","
             << "\"operatorDt2SustainRateRegister2\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 2))]) << ","
             << "\"operatorDt2SustainRateRegister3\":" << static_cast<int>(regs[static_cast<uint8_t>(0xc0 + opOffset(0, 3))]) << ","
             << "\"operatorReleaseRate0\":" << static_cast<int>(regs[static_cast<uint8_t>(0xe0 + opOffset(0, 0))] & 0x0fu) << ","
             << "\"operatorReleaseRate1\":" << static_cast<int>(regs[static_cast<uint8_t>(0xe0 + opOffset(0, 1))] & 0x0fu) << ","
             << "\"operatorReleaseRate2\":" << static_cast<int>(regs[static_cast<uint8_t>(0xe0 + opOffset(0, 2))] & 0x0fu) << ","
             << "\"operatorReleaseRate3\":" << static_cast<int>(regs[static_cast<uint8_t>(0xe0 + opOffset(0, 3))] & 0x0fu) << ","
             << "\"operatorTotalLevel0\":" << static_cast<int>(regs[static_cast<uint8_t>(0x60 + opOffset(0, 0))]) << ","
             << "\"operatorTotalLevel1\":" << static_cast<int>(regs[static_cast<uint8_t>(0x60 + opOffset(0, 1))]) << ","
             << "\"operatorTotalLevel2\":" << static_cast<int>(regs[static_cast<uint8_t>(0x60 + opOffset(0, 2))]) << ","
             << "\"operatorTotalLevel3\":" << static_cast<int>(regs[static_cast<uint8_t>(0x60 + opOffset(0, 3))]) << ","
             << "\"panBits0\":" << static_cast<int>(currentPanBits[0]) << ","
             << "\"panBits1\":" << static_cast<int>(currentPanBits[1]) << ","
             << "\"noiseRegister\":" << static_cast<int>(currentNoiseRegister) << ","
             << "\"noiseEnabled\":" << (((currentNoiseRegister & 0x80u) != 0u) ? 1 : 0) << ","
             << "\"noiseFrequency\":" << static_cast<int>(currentNoiseRegister & 0x1fu) << ","
             << "\"opmLfoWaveformChoice\":" << patch.opmLfoWaveform << ","
             << "\"opmLfoPmsChoice\":" << patch.opmLfoPms << ","
             << "\"opmLfoAmsChoice\":" << patch.opmLfoAms << ","
             << "\"lfoDepthControl\":" << patch.stereoSpread << ","
             << "\"lfoRate\":" << static_cast<int>(currentLfoRate) << ","
             << "\"lfoAmDepth\":" << static_cast<int>(currentLfoAmDepth) << ","
             << "\"lfoPmDepth\":" << static_cast<int>(currentLfoPmDepth) << ","
             << "\"lfoWaveform\":" << static_cast<int>(currentLfoWaveform) << ","
             << "\"lfoPmSensitivity\":" << static_cast<int>(currentLfoPmSensitivity) << ","
             << "\"lfoAmSensitivity\":" << static_cast<int>(currentLfoAmSensitivity) << ","
             << "\"lfoRegister18\":" << static_cast<int>(regs[0x18]) << ","
             << "\"lfoRegister19\":" << static_cast<int>(regs[0x19]) << ","
             << "\"lfoRegister1A\":" << static_cast<int>(regs[0x1a]) << ","
             << "\"lfoRegister1B\":" << static_cast<int>(regs[0x1b]) << ","
             << "\"lfoChannelRegister0\":" << static_cast<int>(regs[0x38]) << ","
             << "\"operatorAmEnable0\":" << (((regs[static_cast<uint8_t>(0xa0 + opOffset(0, 0))] & 0x80u) != 0u) ? 1 : 0) << ","
             << "\"operatorAmEnable1\":" << (((regs[static_cast<uint8_t>(0xa0 + opOffset(0, 1))] & 0x80u) != 0u) ? 1 : 0) << ","
             << "\"operatorAmEnable2\":" << (((regs[static_cast<uint8_t>(0xa0 + opOffset(0, 2))] & 0x80u) != 0u) ? 1 : 0) << ","
             << "\"operatorAmEnable3\":" << (((regs[static_cast<uint8_t>(0xa0 + opOffset(0, 3))] & 0x80u) != 0u) ? 1 : 0) << ","
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
        return fmOperatorMultipleForPatch(ChipMode::ym2151, patch, op);
    }

    uint8_t totalLevelForOperator(size_t op, float velocity, uint8_t algorithm) const
    {
        (void) algorithm;
        return fmOperatorTotalLevelForPatch(ChipMode::ym2151, patch, op, velocity);
    }

    void applyLfoPatch()
    {
        currentLfoRate = ym2151LfoRateForPatch(patch);
        currentLfoAmDepth = ym2151LfoAmDepthForPatch(patch);
        currentLfoPmDepth = ym2151LfoPmDepthForPatch(patch);
        currentLfoWaveform = ym2151LfoWaveformForPatch(patch);
        currentLfoPmSensitivity = ym2151LfoPmSensitivityForPatch(patch);
        currentLfoAmSensitivity = ym2151LfoAmSensitivityForPatch(patch);

        writeOpmRegister(0x18, currentLfoRate);
        writeOpmRegister(0x19, static_cast<uint8_t>(0x80u | currentLfoPmDepth));
        regs[0x1a] = currentLfoPmDepth;
        writeOpmRegister(0x19, currentLfoAmDepth);
        writeOpmRegister(0x1b, static_cast<uint8_t>(currentLfoWaveform & 0x03u));
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
            writeOpmRegister(static_cast<uint8_t>(0x40 + offs), ym2151OperatorMultipleDt1RegisterForPatch(patch, op));
            writeOpmRegister(static_cast<uint8_t>(0x60 + offs), totalLevelForOperator(op, velocity, algorithm));
            writeOpmRegister(static_cast<uint8_t>(0x80 + offs), envelope.attackRate);
            const auto decayRate = static_cast<uint8_t>(envelope.decayRate | (ym2151OperatorAmEnabledForPatch(patch, op) ? 0x80u : 0x00u));
            writeOpmRegister(static_cast<uint8_t>(0xa0 + offs), decayRate);
            writeOpmRegister(static_cast<uint8_t>(0xc0 + offs), ym2151OperatorDt2SustainRateRegisterForPatch(patch, op));
            writeOpmRegister(static_cast<uint8_t>(0xe0 + offs), envelope.sustainRelease);
        }

        writeOpmRegister(static_cast<uint8_t>(0x20 + channel), static_cast<uint8_t>(currentPanBits[channel] | (feedback << 3u) | algorithm));
        writeOpmRegister(static_cast<uint8_t>(0x38 + channel), ym2151LfoChannelRegisterForPatch(patch));
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        applyLfoPatch();
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
        const auto pitch = pitchForNote(midiNote);
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
    uint8_t currentLfoRate = 0;
    uint8_t currentLfoAmDepth = 0;
    uint8_t currentLfoPmDepth = 0;
    uint8_t currentLfoWaveform = 0;
    uint8_t currentLfoPmSensitivity = 0;
    uint8_t currentLfoAmSensitivity = 0;
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

}
std::unique_ptr<ChipCore> makeYm2151Core(AccuracyMode accuracy)
{
    return std::make_unique<Ym2151Core>(accuracy);
}
}
