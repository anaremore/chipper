#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
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
        if (chip == nullptr)
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
        currentLfoRegister = 0;
        currentLfoRate = 0;
        currentLfoAmSensitivity = 0;
        currentLfoPmSensitivity = 0;
        currentLfoChannelBits = 0;
        dacSample.clear();
        if (dacSample.capacity() < maxDacSampleBytes)
            dacSample.reserve(maxDacSampleBytes);
        dacPhase = 0.0;
        dacStep = 1.0;
        dacActive = false;
        dacEnabled = false;
        dacUsingExternalSample = false;
        dacLastTriggerUsedExternalSample = false;
        dacVelocityScale = 1.0;
        dacLastSourceIndex = std::numeric_limits<size_t>::max();
        dacTriggerSampleBytes = 0;
        dacTriggerSampleChecksum = 0;
        dacWriteCount = 0;
        dacWriteChecksum = 2166136261u;
        dacTriggerWriteCount = 0;
        dacTriggerWriteChecksum = 2166136261u;
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

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        ExternalPcmSampleData sample;
        sample.bytes = std::move(data);
        setExternalPcmSampleData(std::move(sample));
    }

    void setExternalPcmSampleData(ExternalPcmSampleData data) override
    {
        if (data.bytes.size() > maxDacSampleBytes)
            data.bytes.resize(maxDacSampleBytes);

        externalDacSample = std::move(data.bytes);
        externalDacSourceRateHz = std::isfinite(data.sourceRateHz) && data.sourceRateHz > 0.0
            ? data.sourceRateHz
            : 0.0;
        externalDacRootNote = std::clamp(data.rootNote, 0, 127);
        externalDacTrimStart = std::min(data.trimStart, externalDacSample.size());
        const auto requestedEnd = data.trimEnd == 0 ? externalDacSample.size() : data.trimEnd;
        externalDacTrimEnd = std::clamp(requestedEnd, externalDacTrimStart, externalDacSample.size());
        externalDacTailBehavior = data.tailBehavior;
        externalDacTrimChecksum = checksumDacRange(externalDacSample, externalDacTrimStart, externalDacTrimEnd);
        dacActive = false;
        dacUsingExternalSample = false;
        dacSample.clear();
        keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
        updateDacEnable(false);
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

        for (size_t channel = 0; channel < notes.size(); ++channel)
        {
            const auto channelNote = channel == 5u && dacModeActive() ? heldNote : notes[channel];
            triggerChannel(channel, channelNote, baseVelocity, channelEnabled(channel));
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
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "YM2612 / Genesis FM"; }
    std::string implementedAccuracy() const override { return "partial ymfm-backed OPN2 register-level"; }
    std::string limitations() const override
    {
        return "BSD-3-Clause ymfm provides the YM2612/OPN2 FM synthesis core. Chipper maps musical controls and notes to all six melodic channels and optional channel-6 DAC playback through $2B/$2A. Imported WAV/AIFF/raw unsigned PCM assets use bounded, sample-rate-aware, root-note and trim metadata with direct no-copy trigger playback plus selectable center/hold tail behavior; generated kick, snare, and chord-impact recipes remain deterministic. Exact host-bus cycle timing, YM2612 ladder nonlinearity, SSG-EG edge cases, timers, and hardware comparison are not complete.";
    }

    std::string debugStateJson() const override
    {
        const auto operatorCarrier = [this](size_t op)
        {
            return fmOperatorIsCarrierForAlgorithm(currentAlgorithm[0], op) ? 1 : 0;
        };

        std::ostringstream json;
        json << "{"
             << "\"mode\":\"YM2612 / Genesis FM\","
             << "\"implementedAccuracy\":\"partial ymfm-backed OPN2 register-level\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"81aec25ccbb98f4873a255f7551ac4dadac59b4a\","
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
             << "\"operatorToneControl\":" << patch.control3 << ","
             << "\"operatorMultiple0\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 0)] & 0x0fu) << ","
             << "\"operatorMultiple1\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 1)] & 0x0fu) << ","
             << "\"operatorMultiple2\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 2)] & 0x0fu) << ","
             << "\"operatorMultiple3\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 3)] & 0x0fu) << ","
             << "\"operatorDetune0\":" << static_cast<int>((regs[opRegForChannel(0x30, 0, 0)] >> 4u) & 0x07u) << ","
             << "\"operatorDetune1\":" << static_cast<int>((regs[opRegForChannel(0x30, 0, 1)] >> 4u) & 0x07u) << ","
             << "\"operatorDetune2\":" << static_cast<int>((regs[opRegForChannel(0x30, 0, 2)] >> 4u) & 0x07u) << ","
             << "\"operatorDetune3\":" << static_cast<int>((regs[opRegForChannel(0x30, 0, 3)] >> 4u) & 0x07u) << ","
             << "\"operatorMultipleDetuneRegister0\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 0)]) << ","
             << "\"operatorMultipleDetuneRegister1\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 1)]) << ","
             << "\"operatorMultipleDetuneRegister2\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 2)]) << ","
             << "\"operatorMultipleDetuneRegister3\":" << static_cast<int>(regs[opRegForChannel(0x30, 0, 3)]) << ","
             << "\"operatorAttackRate0\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 0)]) << ","
             << "\"operatorAttackRate1\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 1)]) << ","
             << "\"operatorAttackRate2\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 2)]) << ","
             << "\"operatorAttackRate3\":" << static_cast<int>(regs[opRegForChannel(0x50, 0, 3)]) << ","
             << "\"operatorDecayRate0\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 0)]) << ","
             << "\"operatorDecayRate1\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 1)]) << ","
             << "\"operatorDecayRate2\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 2)]) << ","
             << "\"operatorDecayRate3\":" << static_cast<int>(regs[opRegForChannel(0x60, 0, 3)]) << ","
             << "\"operatorAmEnable0\":" << static_cast<int>((regs[opRegForChannel(0x60, 0, 0)] >> 7u) & 0x01u) << ","
             << "\"operatorAmEnable1\":" << static_cast<int>((regs[opRegForChannel(0x60, 0, 1)] >> 7u) & 0x01u) << ","
             << "\"operatorAmEnable2\":" << static_cast<int>((regs[opRegForChannel(0x60, 0, 2)] >> 7u) & 0x01u) << ","
             << "\"operatorAmEnable3\":" << static_cast<int>((regs[opRegForChannel(0x60, 0, 3)] >> 7u) & 0x01u) << ","
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
             << "\"panBits1\":" << static_cast<int>(currentPanBits[1]) << ","
             << "\"lfoDepthControl\":" << patch.stereoSpread << ","
             << "\"lfoRegister22\":" << static_cast<int>(currentLfoRegister) << ","
             << "\"lfoEnabled\":" << ((currentLfoRegister & 0x08u) != 0u ? 1 : 0) << ","
             << "\"lfoRate\":" << static_cast<int>(currentLfoRate) << ","
             << "\"lfoAmSensitivity\":" << static_cast<int>(currentLfoAmSensitivity) << ","
             << "\"lfoPmSensitivity\":" << static_cast<int>(currentLfoPmSensitivity) << ","
             << "\"lfoChannelBits\":" << static_cast<int>(currentLfoChannelBits) << ","
             << "\"channelControlRegister0\":" << static_cast<int>(regs[0xb4]) << ","
             << "\"dacMode\":" << static_cast<int>(ym2612DacModeForPatch(patch)) << ","
             << "\"dacEnabled\":" << (dacEnabled ? 1 : 0) << ","
             << "\"dacActive\":" << (dacActive ? 1 : 0) << ","
             << "\"dacRegister2A\":" << static_cast<int>(regs[0x2a]) << ","
             << "\"dacRegister2B\":" << static_cast<int>(regs[0x2b]) << ","
             << "\"dacSampleBytes\":" << dacTriggerSampleBytes << ","
             << "\"dacExternalSampleLoaded\":" << (! externalDacSample.empty() ? 1 : 0) << ","
             << "\"dacExternalSampleBytes\":" << externalDacSample.size() << ","
             << "\"dacExternalSampleChecksum\":" << checksumBytes(externalDacSample) << ","
             << "\"dacExternalSourceRateHz\":" << externalDacSourceRateHz << ","
             << "\"dacResolvedSourceRateHz\":" << resolvedExternalDacSourceRate() << ","
             << "\"dacRootNote\":" << externalDacRootNote << ","
             << "\"dacTrimStart\":" << externalDacTrimStart << ","
             << "\"dacTrimEnd\":" << externalDacTrimEnd << ","
             << "\"dacTailBehavior\":\"" << (externalDacTailBehavior == PcmTailBehavior::hold ? "hold" : "center") << "\","
             << "\"dacCurrentSampleSourceUser\":" << (dacUsingExternalSample ? 1 : 0) << ","
             << "\"dacSampleSourceUser\":" << (dacLastTriggerUsedExternalSample ? 1 : 0) << ","
             << "\"dacPlaybackStep\":" << dacStep << ","
             << "\"dacEffectivePlaybackRateHz\":" << (dacStep * chipSampleRate) << ","
             << "\"dacPlaybackPositionBytes\":"
             << (dacLastSourceIndex == std::numeric_limits<size_t>::max()
                     ? 0u
                     : std::min(dacLastSourceIndex + 1u, dacTriggerSampleBytes)) << ","
             << "\"dacTriggerSampleBytes\":" << dacTriggerSampleBytes << ","
             << "\"dacTriggerSampleChecksum\":" << dacTriggerSampleChecksum << ","
             << "\"dacWriteCount\":" << dacWriteCount << ","
             << "\"dacWriteChecksum\":" << dacWriteChecksum << ","
             << "\"dacTriggerWriteCount\":" << dacTriggerWriteCount << ","
             << "\"dacTriggerWriteChecksum\":" << dacTriggerWriteChecksum << ","
             << "\"dacLastValue\":" << static_cast<int>(dacLastValue) << ","
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
        {
            dacLastValue = 0x80u;
            writeYmRegister(0x2a, dacLastValue);
        }
    }

    uint8_t multipleDetuneRegisterForPatch(size_t op) const
    {
        return ym2612OperatorMultipleDetuneRegisterForPatch(patch, op);
    }

    uint8_t totalLevelForOperator(size_t op, float velocity, uint8_t algorithm) const
    {
        (void) algorithm;
        return fmOperatorTotalLevelForPatch(ChipMode::ym2612, patch, op, velocity);
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

    void applyLfoPatch()
    {
        currentLfoRegister = ym2612LfoRegisterForPatch(patch);
        currentLfoRate = ym2612LfoRateForPatch(patch);
        currentLfoAmSensitivity = ym2612LfoAmSensitivityForPatch(patch);
        currentLfoPmSensitivity = ym2612LfoPmSensitivityForPatch(patch);
        currentLfoChannelBits = ym2612LfoChannelBitsForPatch(patch);
        writeYmRegister(0x22, currentLfoRegister);
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
            writeYmRegister(opRegForChannel(0x30, channel, op), multipleDetuneRegisterForPatch(op));
            writeYmRegister(opRegForChannel(0x40, channel, op), totalLevelForOperator(op, velocity, algorithm));
            writeYmRegister(opRegForChannel(0x50, channel, op), envelope.attackRate);
            const auto decayRate = static_cast<uint8_t>(envelope.decayRate | (ym2612OperatorAmEnabledForPatch(patch, op) ? 0x80u : 0x00u));
            writeYmRegister(opRegForChannel(0x60, channel, op), decayRate);
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
        writeYmRegister(regForChannel(0xb4, channel), ym2612ChannelControlForPatch(patch, channel));
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        applyLfoPatch();

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

        const auto pitch = pitchForNote(midiNote);
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
            dacUsingExternalSample = false;
            updateDacEnable(false);
        }
        writeYmRegister(0x28, keyCodeForChannel(channel));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    static uint32_t checksumDacRange(const std::vector<uint8_t>& bytes, size_t start, size_t end)
    {
        auto checksum = 2166136261u;
        const auto boundedStart = std::min(start, bytes.size());
        const auto boundedEnd = std::clamp(end, boundedStart, bytes.size());
        for (auto index = boundedStart; index < boundedEnd; ++index)
        {
            checksum ^= bytes[index];
            checksum *= 16777619u;
        }
        return checksum;
    }

    double resolvedExternalDacSourceRate() const
    {
        return externalDacSourceRateHz > 0.0 ? externalDacSourceRateHz : chipSampleRate;
    }

    size_t activeDacSampleSize() const
    {
        if (dacUsingExternalSample)
            return externalDacTrimEnd > externalDacTrimStart ? externalDacTrimEnd - externalDacTrimStart : 0u;
        return dacSample.size();
    }

    uint8_t activeDacSampleByte(size_t index) const
    {
        if (! dacUsingExternalSample)
            return index < dacSample.size() ? dacSample[index] : 0x80u;

        const auto sourceIndex = externalDacTrimStart + index;
        if (sourceIndex >= externalDacTrimEnd || sourceIndex >= externalDacSample.size())
            return 0x80u;

        const auto centered = (static_cast<double>(externalDacSample[sourceIndex]) - 128.0) * dacVelocityScale;
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(128.0 + centered)), 0, 255));
    }

    void writeDacByte(uint8_t value)
    {
        dacLastValue = value;
        writeYmRegister(0x2a, value);
        ++dacWriteCount;
        ++dacTriggerWriteCount;
        dacWriteChecksum ^= value;
        dacWriteChecksum *= 16777619u;
        dacTriggerWriteChecksum ^= value;
        dacTriggerWriteChecksum *= 16777619u;
    }

    void triggerDacChannel(int midiNote, float velocity, bool shouldEnable)
    {
        channelVelocity[5] = static_cast<float>(clamp01(velocity) * sourceLevel(patch, 5));
        currentPanBits[5] = ym2612PanBitsForPatch(patch, 5);
        writeYmRegister(regForChannel(0xb4, 5), ym2612ChannelControlForPatch(patch, 5));

        if (! shouldEnable || channelVelocity[5] <= 0.0f)
        {
            dacActive = false;
            dacSample.clear();
            dacUsingExternalSample = false;
            updateDacEnable(false);
            keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
            return;
        }

        currentFnum[5] = 0;
        currentBlock[5] = 0;
        currentAlgorithm[5] = 0;
        currentFeedback[5] = 0;
        dacUsingExternalSample = externalDacTrimEnd > externalDacTrimStart;
        dacLastTriggerUsedExternalSample = dacUsingExternalSample;
        dacVelocityScale = std::clamp(static_cast<double>(channelVelocity[5]), 0.0, 1.0);
        if (dacUsingExternalSample)
            dacSample.clear();
        else
            fillDacDrumSample(midiNote, channelVelocity[5]);

        dacPhase = 0.0;
        dacLastSourceIndex = std::numeric_limits<size_t>::max();
        dacTriggerWriteCount = 0;
        dacTriggerWriteChecksum = 2166136261u;
        if (dacUsingExternalSample)
        {
            const auto noteRatio = std::pow(2.0, (std::clamp(midiNote, 0, 127) - externalDacRootNote) / 12.0);
            const auto sourceRate = resolvedExternalDacSourceRate();
            dacStep = chipSampleRate > 0.0 ? (sourceRate / chipSampleRate) * noteRatio : noteRatio;
        }
        else
        {
            const auto playbackHz = std::clamp(9000.0 * std::pow(2.0, (std::clamp(midiNote, 24, 96) - 60) / 24.0), 3500.0, 22000.0);
            dacStep = chipSampleRate > 0.0 ? playbackHz / chipSampleRate : 1.0;
        }
        if (! std::isfinite(dacStep) || dacStep <= 0.0)
            dacStep = 1.0;

        dacTriggerSampleBytes = activeDacSampleSize();
        dacTriggerSampleChecksum = dacUsingExternalSample
            ? externalDacTrimChecksum
            : checksumDacRange(dacSample, 0, dacSample.size());
        dacActive = dacTriggerSampleBytes > 0;
        updateDacEnable(dacActive);
        if (dacActive)
            keyOnMask |= static_cast<uint16_t>(1u << 5u);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
    }

    void fillDacDrumSample(int midiNote, float velocity)
    {
        const auto recipe = std::clamp(static_cast<double>(patch.control1), 0.0, 1.0);
        const auto kickRecipe = recipe < 0.18;
        const auto snareRecipe = ! kickRecipe && recipe < 0.32;
        const auto length = kickRecipe ? 512u : (snareRecipe ? 384u : 448u);
        const auto baseCycles = kickRecipe ? 8.5 : (snareRecipe ? 22.0 : 14.0);
        const auto noiseMix = kickRecipe ? 0.14 : (snareRecipe ? 0.78 : 0.46);
        const auto decay = kickRecipe ? 7.4 : (snareRecipe ? 10.0 : 6.4);

        dacSample.resize(length, 0x80u);
        uint32_t noise = 0x6d2b79f5u
            ^ static_cast<uint32_t>(std::clamp(midiNote, 0, 127) * 1103515245u)
            ^ static_cast<uint32_t>(std::round(recipe * 65535.0));
        const auto velocityScale = std::clamp(static_cast<double>(velocity), 0.0, 1.0);
        for (size_t i = 0; i < dacSample.size(); ++i)
        {
            const auto t = static_cast<double>(i) / static_cast<double>(dacSample.size());
            const auto env = std::exp(-decay * t) * velocityScale;
            const auto sweep = baseCycles * (1.0 - (kickRecipe ? 0.78 : 0.48) * t);
            auto value = std::sin(twoPi * sweep * t);

            noise = noise * 1664525u + 1013904223u;
            const auto noiseValue = (static_cast<int>((noise >> 24) & 0xffu) - 128) / 128.0;
            value = value * (1.0 - noiseMix) + noiseValue * noiseMix;
            if (! kickRecipe)
                value += std::sin(twoPi * (snareRecipe ? 43.0 : 27.0) * t) * (snareRecipe ? 0.12 : 0.20);

            const auto byteValue = static_cast<int>(std::round(128.0 + std::clamp(value * env, -1.0, 1.0) * 118.0));
            dacSample[i] = static_cast<uint8_t>(std::clamp(byteValue, 0, 255));
        }
    }

    void advanceDacPlayback()
    {
        if (! dacActive || ! dacEnabled)
            return;

        const auto sampleBytes = activeDacSampleSize();
        if (sampleBytes == 0)
        {
            dacActive = false;
            dacUsingExternalSample = false;
            keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
            return;
        }

        const auto requestedIndex = static_cast<size_t>(std::max(0.0, std::floor(dacPhase)));
        const auto reachedEnd = requestedIndex >= sampleBytes;
        const auto targetIndex = std::min(requestedIndex, sampleBytes - 1u);
        auto nextIndex = dacLastSourceIndex == std::numeric_limits<size_t>::max()
            ? 0u
            : dacLastSourceIndex + 1u;
        while (nextIndex <= targetIndex)
        {
            writeDacByte(activeDacSampleByte(nextIndex));
            dacLastSourceIndex = nextIndex;
            ++nextIndex;
        }

        if (reachedEnd)
        {
            dacActive = false;
            dacSample.clear();
            dacUsingExternalSample = false;
            keyOnMask &= static_cast<uint16_t>(~(1u << 5u));
            if (! dacLastTriggerUsedExternalSample || externalDacTailBehavior == PcmTailBehavior::center)
                writeDacByte(0x80u);
            return;
        }

        dacPhase += dacStep;
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
    uint8_t currentLfoRegister = 0;
    uint8_t currentLfoRate = 0;
    uint8_t currentLfoAmSensitivity = 0;
    uint8_t currentLfoPmSensitivity = 0;
    uint8_t currentLfoChannelBits = 0;
    std::array<int, 6> channelNotes {};
    std::array<float, 6> channelVelocity {};
    std::array<uint64_t, 6> channelStamp {};
    static constexpr size_t maxDacSampleBytes = 0x40000u;
    std::vector<uint8_t> dacSample;
    std::vector<uint8_t> externalDacSample;
    double externalDacSourceRateHz = 0.0;
    int externalDacRootNote = 60;
    size_t externalDacTrimStart = 0;
    size_t externalDacTrimEnd = 0;
    uint32_t externalDacTrimChecksum = 2166136261u;
    PcmTailBehavior externalDacTailBehavior = PcmTailBehavior::center;
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    double laserPhase = 0.0;
    double dacPhase = 0.0;
    double dacStep = 1.0;
    double dacVelocityScale = 1.0;
    size_t dacLastSourceIndex = std::numeric_limits<size_t>::max();
    size_t dacTriggerSampleBytes = 0;
    uint32_t dacTriggerSampleChecksum = 0;
    uint64_t dacWriteCount = 0;
    uint32_t dacWriteChecksum = 2166136261u;
    uint64_t dacTriggerWriteCount = 0;
    uint32_t dacTriggerWriteChecksum = 2166136261u;
    bool dacActive = false;
    bool dacEnabled = false;
    bool dacUsingExternalSample = false;
    bool dacLastTriggerUsedExternalSample = false;
    uint8_t dacLastValue = 0x80u;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    StereoFrame currentOutput {};
};


}
std::unique_ptr<ChipCore> makeYm2612Core(AccuracyMode accuracy)
{
    return std::make_unique<Ym2612Core>(accuracy);
}
}
