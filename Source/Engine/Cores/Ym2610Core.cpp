#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Ym2610Core final : public ChipCore
{
public:
    explicit Ym2610Core(AccuracyMode requested, bool ym2610bMode = false)
        : accuracy(requested)
        , opnb2(ym2610bMode)
    {
        channelNotes.fill(-1);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 8000000.0;
        if (chip == nullptr)
            chip = std::make_unique<ymfm::ym2610>(host, opnb2 ? 0x3fu : 0x36u);
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
        currentSsgPeriod.fill(1);
        currentSsgVolume.fill(0);
        currentOpnbAdpcmALevels.fill(0);
        currentOpnbAdpcmAStartRegisters.fill(0);
        currentOpnbAdpcmAEndRegisters.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        keyOnMask = 0;
        opnbAdpcmAKeyBits = 0;
        opnbAdpcmATotalLevel = 0x3fu;
        opnbAdpcmBControlRegister = 0;
        opnbAdpcmBPanRegister = 0;
        opnbAdpcmBStartRegister = 0;
        opnbAdpcmBEndRegister = 0;
        opnbAdpcmBDeltaNRegister = 0;
        opnbAdpcmBLevelRegister = 0;
        ssgGateMask = 0;
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
            || opnbAdpcmEnabledForMacro(nextPatch.macro) != opnbAdpcmEnabledForMacro(patch.macro))
            clearChipPolyState();

        patch = nextPatch;
        if (! opnbAdpcmEnabledForPatch())
            keyOffOpnbAdpcm();
        applyPatchToAllChannels(true);
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        usesExplicitAdpcmARegions = false;
        explicitAdpcmARegions = {};
        host.setAdpcmAMemory(std::move(data));
        host.resetAdpcmCounters();
    }

    void setExternalAdpcmAData(
        std::vector<uint8_t> data,
        std::array<yamahaAdpcm::AdpcmARegionWindow, yamahaAdpcm::regionCountA> regions) override
    {
        explicitAdpcmARegions = regions;
        usesExplicitAdpcmARegions = true;
        host.setAdpcmAMemory(std::move(data));
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
        auto notes = std::array<int, maxSourceChannelCount> { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote, heldNote + 12, heldNote + 19 };
        if (opnb2)
        {
            switch (patch.macro)
            {
                case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote - 12, heldNote, heldNote + 7 }; break;
                case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 12, heldNote + 19, heldNote + 24 }; break;
                case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28, heldNote + 31 }; break;
                case MacroKind::coin:
                case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 31, heldNote + 36, heldNote + 43 }; break;
                case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote - 19, heldNote - 24, heldNote + 19, heldNote + 7, heldNote - 5 }; break;
                case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41, heldNote + 48 }; break;
                case MacroKind::drum:
                case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36 }; break;
                case MacroKind::manual:
                default: break;
            }
        }
        else
        {
            switch (patch.macro)
            {
                case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote - 12, heldNote, heldNote + 7, heldNote, heldNote }; break;
                case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 12, heldNote + 19, heldNote + 24, heldNote, heldNote }; break;
                case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote, heldNote }; break;
                case MacroKind::coin:
                case MacroKind::jump: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 31, heldNote + 36, heldNote + 43, heldNote, heldNote }; break;
                case MacroKind::laser: notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote + 19, heldNote + 7, heldNote - 5, heldNote, heldNote }; break;
                case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote, heldNote }; break;
                case MacroKind::drum:
                case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote, heldNote }; break;
                case MacroKind::manual:
                default: break;
            }
        }

        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
            triggerChannel(channel, notes[channel], baseVelocity, channelEnabled(channel));

        if (opnbAdpcmEnabledForPatch())
            triggerOpnbAdpcm(heldNote, baseVelocity);
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
            for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
            {
                keyOffChannel(channel);
                channelNotes[channel] = -1;
                channelVelocity[channel] = 0.0f;
                channelStamp[channel] = 0;
            }
            keyOffOpnbAdpcm();
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
        ymfm::ym2610::output_data output;
        auto generated = false;
        while (sampleAccumulator >= 1.0)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
            if constexpr (ymfm::ym2610::OUTPUTS > 2)
                lastSsg = output.data[2];
            sampleAccumulator -= 1.0;
            generated = true;
        }
        if (! generated && ratio >= 0.999)
        {
            chip->generate(&output);
            lastNativeLeft = output.data[0];
            lastNativeRight = output.data[1];
            if constexpr (ymfm::ym2610::OUTPUTS > 2)
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

    ChipMode mode() const override { return opnb2 ? ChipMode::ym2610b : ChipMode::ym2610; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return opnb2 ? "YM2610B / OPNB2" : "YM2610 / OPNB"; }
    std::string implementedAccuracy() const override { return opnb2 ? "partial ymfm-backed OPNB2 FM+SSG register-level" : "partial ymfm-backed OPNB FM+SSG register-level"; }
    std::string limitations() const override
    {
        if (opnb2)
            return "BSD-3-Clause ymfm provides the YM2610B/OPNB2-compatible synthesis path through the YM2610 core's six-FM channel mask. Chipper maps musical controls and notes to all six OPNB2 FM channels plus the embedded three-channel SSG tone/noise/envelope generator: operator, algorithm, feedback, f-number/block, FM key-on, FM pan, SSG tone/noise period, mixer, amplitude, and envelope registers are driven through the YM2610 low/high address-data ports. Drum and Hit macros can also write native OPNB ADPCM-A key/start/end/level registers and ADPCM-B start/end/delta/level registers from user-owned encoded byte memory. A pinned YM2608-LLE YM2610 stereo fixture independently gates covered ADPCM-A page addressing, decoding, routing, gain, duration, and a failing page-misaddress mutation in the YM2610 configuration. Broader ADPCM traces, timers, prescaler controls, analog/hardware comparison, and cycle accuracy are not complete.";

        return "BSD-3-Clause ymfm provides the YM2610/OPNB synthesis core. Chipper currently maps musical controls and notes to the four exposed OPNB FM channels plus the embedded three-channel SSG tone/noise/envelope generator: operator, algorithm, feedback, f-number/block, FM key-on, FM pan, SSG tone/noise period, mixer, amplitude, and envelope registers are driven through the YM2610 low/high address-data ports. Drum and Hit macros can also write native OPNB ADPCM-A key/start/end/level registers and ADPCM-B start/end/delta/level registers from user-owned encoded byte memory. A pinned YM2608-LLE YM2610 stereo fixture independently gates covered ADPCM-A page addressing, decoding, routing, gain, duration, and a failing page-misaddress mutation in the YM2610 configuration. Broader ADPCM traces, timers, prescaler controls, analog/hardware comparison, and cycle accuracy are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"" << modeName() << "\","
             << "\"implementedAccuracy\":\"" << implementedAccuracy() << "\","
             << "\"vendoredCore\":\"ymfm\","
             << "\"vendoredCoreLicense\":\"BSD-3-Clause\","
             << "\"vendoredCoreCommit\":\"81aec25ccbb98f4873a255f7551ac4dadac59b4a\","
             << "\"clockHz\":" << clock << ","
             << "\"sampleRate\":" << sampleRate << ","
             << "\"chipSampleRate\":" << chipSampleRate << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":" << sourceChannelCount() << ","
             << "\"exposedChannelCount\":" << sourceChannelCount() << ","
             << "\"fmChannelCount\":" << fmChannelCount() << ","
             << "\"ssgChannelCount\":3,"
             << "\"ssgIntegrated\":1,"
             << "\"opnb2Implemented\":" << (opnb2 ? 1 : 0) << ","
             << "\"adpcmAImplemented\":1,"
             << "\"adpcmBImplemented\":1,"
             << "\"opnbAdpcmAOverlay\":" << (opnbAdpcmEnabledForPatch() ? 1 : 0) << ","
             << "\"opnbAdpcmAKeyBits\":" << static_cast<int>(opnbAdpcmAKeyBits) << ","
             << "\"opnbAdpcmATotalLevel\":" << static_cast<int>(opnbAdpcmATotalLevel) << ","
             << "\"opnbAdpcmALoaded\":" << (host.adpcmALoaded() ? 1 : 0) << ","
             << "\"opnbAdpcmAProvidedBytes\":" << host.adpcmAProvidedBytes() << ","
             << "\"opnbAdpcmACopiedBytes\":" << host.adpcmACopiedBytes() << ","
             << "\"opnbAdpcmAChecksum\":" << host.adpcmAChecksum() << ","
             << "\"opnbAdpcmAReadCount\":" << host.adpcmAReads() << ","
             << "\"opnbAdpcmALastReadAddress\":" << host.lastAdpcmAReadAddress() << ","
             << "\"opnbAdpcmARegionMode\":" << (usesExplicitAdpcmARegions ? 1 : 0) << ","
             << "\"opnbAdpcmAActiveMask\":" << static_cast<int>(opnbAdpcmAConfiguredMask()) << ","
             << "\"opnbAdpcmAStartRegister0\":" << currentOpnbAdpcmAStartRegisters[0] << ","
             << "\"opnbAdpcmAEndRegister0\":" << currentOpnbAdpcmAEndRegisters[0] << ","
             << "\"opnbAdpcmAStartRegister1\":" << currentOpnbAdpcmAStartRegisters[1] << ","
             << "\"opnbAdpcmAEndRegister1\":" << currentOpnbAdpcmAEndRegisters[1] << ","
             << "\"opnbAdpcmAStartRegister2\":" << currentOpnbAdpcmAStartRegisters[2] << ","
             << "\"opnbAdpcmAEndRegister2\":" << currentOpnbAdpcmAEndRegisters[2] << ","
             << "\"opnbAdpcmAStartRegister3\":" << currentOpnbAdpcmAStartRegisters[3] << ","
             << "\"opnbAdpcmAEndRegister3\":" << currentOpnbAdpcmAEndRegisters[3] << ","
             << "\"opnbAdpcmAStartRegister4\":" << currentOpnbAdpcmAStartRegisters[4] << ","
             << "\"opnbAdpcmAEndRegister4\":" << currentOpnbAdpcmAEndRegisters[4] << ","
             << "\"opnbAdpcmAStartRegister5\":" << currentOpnbAdpcmAStartRegisters[5] << ","
             << "\"opnbAdpcmAEndRegister5\":" << currentOpnbAdpcmAEndRegisters[5] << ","
             << "\"opnbAdpcmAMaxBytes\":" << opnbAdpcmAMaxBytes << ","
             << "\"opnbAdpcmBReadCount\":" << host.adpcmBReads() << ","
             << "\"opnbAdpcmBLoaded\":" << (host.adpcmBLoaded() ? 1 : 0) << ","
             << "\"opnbAdpcmBProvidedBytes\":" << host.adpcmBProvidedBytes() << ","
             << "\"opnbAdpcmBCopiedBytes\":" << host.adpcmBCopiedBytes() << ","
             << "\"opnbAdpcmBChecksum\":" << host.adpcmBChecksum() << ","
             << "\"opnbAdpcmBLastReadAddress\":" << host.lastAdpcmBReadAddress() << ","
             << "\"opnbAdpcmBControlRegister\":" << static_cast<int>(opnbAdpcmBControlRegister) << ","
             << "\"opnbAdpcmBPanRegister\":" << static_cast<int>(opnbAdpcmBPanRegister) << ","
             << "\"opnbAdpcmBStartRegister\":" << opnbAdpcmBStartRegister << ","
             << "\"opnbAdpcmBEndRegister\":" << opnbAdpcmBEndRegister << ","
             << "\"opnbAdpcmBDeltaNRegister\":" << opnbAdpcmBDeltaNRegister << ","
             << "\"opnbAdpcmBLevelRegister\":" << static_cast<int>(opnbAdpcmBLevelRegister) << ","
             << "\"opnbAdpcmBMaxBytes\":" << opnbAdpcmBMaxBytes << ","
             << "\"algorithm0\":" << static_cast<int>(currentAlgorithm[0]) << ","
             << "\"feedback0\":" << static_cast<int>(currentFeedback[0]) << ","
             << "\"algorithmFeedbackRegister0\":" << static_cast<int>(regs[regForNativeChannel(0xb0, nativeFmChannel(0))]) << ","
             << "\"algorithmFeedbackRegister2\":" << static_cast<int>(regs[regForNativeChannel(0xb0, nativeFmChannel(2))]) << ","
             << "\"algorithmFeedbackRegister5\":" << static_cast<int>(fmChannelCount() > 5 ? regs[regForNativeChannel(0xb0, nativeFmChannel(5))] : 0) << ","
             << "\"operatorTotalLevel0\":" << static_cast<int>(regs[opRegForNativeChannel(0x40, nativeFmChannel(0), 0)]) << ","
             << "\"operatorTotalLevel3\":" << static_cast<int>(regs[opRegForNativeChannel(0x40, nativeFmChannel(0), 3)]) << ","
             << "\"panBits0\":" << static_cast<int>(currentPanBits[0]) << ","
             << "\"panBits2\":" << static_cast<int>(currentPanBits[2]) << ","
             << "\"fnum0\":" << currentFnum[0] << ","
             << "\"fnum2\":" << currentFnum[2] << ","
             << "\"block0\":" << static_cast<int>(currentBlock[0]) << ","
             << "\"block2\":" << static_cast<int>(currentBlock[2]) << ","
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
        void setAdpcmAMemory(std::vector<uint8_t> data)
        {
            setMemory(std::move(data), opnbAdpcmAMaxBytes, adpcmAMemory, providedAdpcmABytes, copiedAdpcmABytes);
        }

        void setAdpcmBMemory(std::vector<uint8_t> data)
        {
            setMemory(std::move(data), opnbAdpcmBMaxBytes, adpcmBMemory, providedAdpcmBBytes, copiedAdpcmBBytes);
        }

        uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
        {
            if (type == ymfm::ACCESS_ADPCM_A)
            {
                ++adpcmAReadCount;
                lastAdpcmAAddress = address;
                if (! adpcmAMemory.empty())
                    return adpcmAMemory[static_cast<size_t>(address % static_cast<uint32_t>(adpcmAMemory.size()))];
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

        bool adpcmALoaded() const { return ! adpcmAMemory.empty(); }
        bool adpcmBLoaded() const { return ! adpcmBMemory.empty(); }
        size_t adpcmAProvidedBytes() const { return providedAdpcmABytes; }
        size_t adpcmACopiedBytes() const { return copiedAdpcmABytes; }
        size_t adpcmBProvidedBytes() const { return providedAdpcmBBytes; }
        size_t adpcmBCopiedBytes() const { return copiedAdpcmBBytes; }
        uint32_t adpcmAChecksum() const { return adpcmAMemory.empty() ? 0u : checksumBytes(adpcmAMemory); }
        uint32_t adpcmBChecksum() const { return adpcmBMemory.empty() ? 0u : checksumBytes(adpcmBMemory); }
        uint64_t adpcmAReads() const { return adpcmAReadCount; }
        uint64_t adpcmBReads() const { return adpcmBReadCount; }
        uint32_t lastAdpcmAReadAddress() const { return lastAdpcmAAddress; }
        uint32_t lastAdpcmBReadAddress() const { return lastAdpcmBAddress; }

    private:
        static void setMemory(std::vector<uint8_t> data, size_t maxBytes, std::vector<uint8_t>& memory, size_t& providedBytes, size_t& copiedBytes)
        {
            providedBytes = data.size();
            copiedBytes = std::min(data.size(), maxBytes);
            memory.clear();
            memory.reserve(copiedBytes);
            if (copiedBytes > 0)
                memory.insert(memory.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(copiedBytes));
            if (memory.empty())
            {
                providedBytes = 0;
                copiedBytes = 0;
            }
        }

        std::vector<uint8_t> adpcmAMemory;
        std::vector<uint8_t> adpcmBMemory;
        size_t providedAdpcmABytes = 0;
        size_t copiedAdpcmABytes = 0;
        size_t providedAdpcmBBytes = 0;
        size_t copiedAdpcmBBytes = 0;
        uint64_t adpcmAReadCount = 0;
        uint64_t adpcmBReadCount = 0;
        uint32_t lastAdpcmAAddress = 0;
        uint32_t lastAdpcmBAddress = 0;
    };

    struct OPNBPitch
    {
        uint16_t fnum = 0;
        uint8_t block = 0;
    };

    static uint8_t keyCodeForNativeChannel(size_t nativeChannel)
    {
        return static_cast<uint8_t>(nativeChannel < 3 ? nativeChannel : nativeChannel + 1);
    }

    static uint16_t regForNativeChannel(uint8_t base, size_t nativeChannel)
    {
        const auto port = nativeChannel >= 3 ? 0x100u : 0x000u;
        const auto local = static_cast<uint16_t>(nativeChannel % 3u);
        return static_cast<uint16_t>(port | base | local);
    }

    static uint16_t opRegForNativeChannel(uint8_t base, size_t nativeChannel, size_t op)
    {
        static constexpr std::array<uint8_t, 4> opOffsets { 0x00, 0x04, 0x08, 0x0c };
        return static_cast<uint16_t>(regForNativeChannel(static_cast<uint8_t>(base + opOffsets[op]), nativeChannel));
    }

    size_t fmChannelCount() const
    {
        return opnb2 ? maxFmChannelCount : opnbFmChannelCount;
    }

    size_t ssgLaneOffset() const
    {
        return fmChannelCount();
    }

    size_t sourceChannelCount() const
    {
        return fmChannelCount() + ssgChannelCount;
    }

    size_t nativeFmChannel(size_t channel) const
    {
        return opnb2 ? std::min(channel, maxFmChannelCount - static_cast<size_t>(1)) : opnbNativeFmChannels[std::min(channel, opnbNativeFmChannels.size() - static_cast<size_t>(1))];
    }

    uint16_t ssgLaneMask() const
    {
        uint16_t mask = 0;
        for (size_t ssg = 0; ssg < ssgChannelCount; ++ssg)
            mask = static_cast<uint16_t>(mask | (1u << (ssgLaneOffset() + ssg)));
        return mask;
    }

    int ssgPeriodRegister(size_t channel) const
    {
        if (channel >= ssgChannelCount)
            return 0;

        const auto reg = channel * 2u;
        return std::max(1, static_cast<int>(regs[reg] | ((regs[reg + 1u] & 0x0fu) << 8u)));
    }
    static bool opnbAdpcmEnabledForMacro(MacroKind macro)
    {
        return macro == MacroKind::drum || macro == MacroKind::hit;
    }

    bool opnbAdpcmEnabledForPatch() const
    {
        return opnbAdpcmEnabledForMacro(patch.macro);
    }

    uint8_t opnbAdpcmAChannelLevel(size_t channel, float velocity) const
    {
        const auto source = std::min(channel, sourceChannelCount() - static_cast<size_t>(1));
        const auto level = clamp01(velocity) * clamp01(patch.control4) * sourceLevel(patch, source);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(level * 31.0)), 0, 31));
    }

    uint8_t opnbAdpcmAConfiguredMask() const
    {
        if (! usesExplicitAdpcmARegions)
            return host.adpcmALoaded() ? 0x3fu : 0u;

        auto mask = uint8_t { 0u };
        for (size_t channel = 0; channel < explicitAdpcmARegions.size(); ++channel)
            if (explicitAdpcmARegions[channel].populated)
                mask = static_cast<uint8_t>(mask | (1u << channel));
        return mask;
    }

    uint8_t opnbAdpcmAKeyBitsForPatch(float velocity) const
    {
        if (! opnbAdpcmEnabledForPatch() || ! host.adpcmALoaded() || ! anyAudibleSourceEnabled())
            return 0;

        const auto configuredMask = opnbAdpcmAConfiguredMask();
        uint8_t keyBits = 0;
        for (size_t channel = 0; channel < currentOpnbAdpcmALevels.size(); ++channel)
        {
            const auto source = std::min(channel, sourceChannelCount() - static_cast<size_t>(1));
            if ((configuredMask & (1u << channel)) != 0u
                && channelEnabled(source)
                && opnbAdpcmAChannelLevel(channel, velocity) > 0)
                keyBits = static_cast<uint8_t>(keyBits | (1u << channel));
        }
        return keyBits;
    }

    static uint16_t opnbAdpcmEndRegisterForBytes(size_t copiedBytes)
    {
        if (copiedBytes <= 1u)
            return 0;
        const auto end = (copiedBytes - 1u) >> 8u;
        return static_cast<uint16_t>(std::min<size_t>(end, 0xffffu));
    }

    std::pair<uint16_t, uint16_t> opnbAdpcmAWindowForChannel(size_t channel) const
    {
        if (usesExplicitAdpcmARegions && channel < explicitAdpcmARegions.size())
        {
            const auto& region = explicitAdpcmARegions[channel];
            if (! region.populated)
                return { 0, 0 };
            return { static_cast<uint16_t>(region.startByte >> 8u),
                     static_cast<uint16_t>(region.endByteInclusive >> 8u) };
        }

        const auto copiedBytes = host.adpcmACopiedBytes();
        if (copiedBytes <= 1u)
            return { 0, 0 };

        const auto segmentBytes = std::max<size_t>(0x100u, copiedBytes / currentOpnbAdpcmALevels.size());
        const auto startByte = std::min(copiedBytes - 1u, segmentBytes * channel);
        const auto endByte = std::min(copiedBytes - 1u, startByte + segmentBytes - 1u);
        return {
            static_cast<uint16_t>(std::min<size_t>(startByte >> 8u, 0xffffu)),
            static_cast<uint16_t>(std::min<size_t>(endByte >> 8u, 0xffffu))
        };
    }

    uint8_t opnbAdpcmBLevelForPatch(float velocity) const
    {
        auto trim = 0.0;
        for (size_t source = 0; source < sourceChannelCount(); ++source)
        {
            if (channelEnabled(source))
                trim = std::max(trim, sourceLevel(patch, source));
        }
        const auto level = clamp01(velocity) * clamp01(patch.control4) * trim;
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(level * 255.0)), 0, 255));
    }

    uint16_t opnbAdpcmBDeltaNForNote(int midiNote) const
    {
        const auto semitones = static_cast<double>(std::clamp(midiNote, 0, 127) - 60) / 12.0;
        const auto macroBoost = patch.macro == MacroKind::hit ? 1.18 : 1.0;
        const auto delta = std::round(0x1800 * std::pow(2.0, semitones) * macroBoost);
        return static_cast<uint16_t>(std::clamp(static_cast<int>(delta), 0x0100, 0xffff));
    }

    void keyOffOpnbAdpcmA()
    {
        const auto shouldWriteDump = opnbAdpcmAKeyBits != 0 || (regs[0x100] & 0x3fu) != 0;
        const auto dumpMask = static_cast<uint8_t>(0x80u | (opnbAdpcmAKeyBits != 0 ? opnbAdpcmAKeyBits : 0x3fu));
        opnbAdpcmAKeyBits = 0;
        currentOpnbAdpcmALevels.fill(0);
        if (chip && shouldWriteDump)
            writeYmRegister(0x100u, dumpMask);
    }

    void triggerOpnbAdpcmA(float velocity)
    {
        const auto keyBits = opnbAdpcmAKeyBitsForPatch(velocity);
        keyOffOpnbAdpcmA();
        if (keyBits == 0)
            return;

        opnbAdpcmATotalLevel = 0x3fu;
        writeYmRegister(0x101u, opnbAdpcmATotalLevel);
        for (size_t channel = 0; channel < currentOpnbAdpcmALevels.size(); ++channel)
        {
            const auto level = opnbAdpcmAChannelLevel(channel, velocity);
            const auto pan = ym2612PanBitsForPatch(patch, channel);
            const auto [start, end] = opnbAdpcmAWindowForChannel(channel);
            currentOpnbAdpcmALevels[channel] = level;
            currentOpnbAdpcmAStartRegisters[channel] = start;
            currentOpnbAdpcmAEndRegisters[channel] = end;
            writeYmRegister(static_cast<uint16_t>(0x108u + channel), static_cast<uint8_t>(pan | level));
            writeYmRegister(static_cast<uint16_t>(0x110u + channel), static_cast<uint8_t>(start & 0xffu));
            writeYmRegister(static_cast<uint16_t>(0x118u + channel), static_cast<uint8_t>((start >> 8u) & 0xffu));
            writeYmRegister(static_cast<uint16_t>(0x120u + channel), static_cast<uint8_t>(end & 0xffu));
            writeYmRegister(static_cast<uint16_t>(0x128u + channel), static_cast<uint8_t>((end >> 8u) & 0xffu));
        }

        opnbAdpcmAKeyBits = keyBits;
        writeYmRegister(0x100u, keyBits);
    }

    void keyOffOpnbAdpcmB()
    {
        const auto wasExecuting = (regs[0x10] & 0x80u) != 0u || (opnbAdpcmBControlRegister & 0x80u) != 0u;
        opnbAdpcmBControlRegister = host.adpcmBLoaded() ? 0x20u : 0x00u;
        if (chip && wasExecuting)
            writeYmRegister(0x10u, opnbAdpcmBControlRegister);
    }

    void triggerOpnbAdpcmB(int midiNote, float velocity)
    {
        if (! host.adpcmBLoaded() || ! opnbAdpcmEnabledForPatch() || ! anyAudibleSourceEnabled())
        {
            keyOffOpnbAdpcmB();
            return;
        }

        const auto level = opnbAdpcmBLevelForPatch(velocity);
        if (level == 0)
        {
            keyOffOpnbAdpcmB();
            return;
        }

        const auto end = opnbAdpcmEndRegisterForBytes(host.adpcmBCopiedBytes());
        const auto delta = opnbAdpcmBDeltaNForNote(midiNote);
        opnbAdpcmBControlRegister = 0xa0u;
        opnbAdpcmBPanRegister = 0xc0u;
        opnbAdpcmBStartRegister = 0;
        opnbAdpcmBEndRegister = end;
        opnbAdpcmBDeltaNRegister = delta;
        opnbAdpcmBLevelRegister = level;

        writeYmRegister(0x10u, 0x01u);
        writeYmRegister(0x10u, 0x20u);
        writeYmRegister(0x11u, opnbAdpcmBPanRegister);
        writeYmRegister(0x12u, 0x00u);
        writeYmRegister(0x13u, 0x00u);
        writeYmRegister(0x14u, static_cast<uint8_t>(end & 0xffu));
        writeYmRegister(0x15u, static_cast<uint8_t>((end >> 8u) & 0xffu));
        writeYmRegister(0x19u, static_cast<uint8_t>(delta & 0xffu));
        writeYmRegister(0x1au, static_cast<uint8_t>((delta >> 8u) & 0xffu));
        writeYmRegister(0x1bu, level);
        writeYmRegister(0x10u, opnbAdpcmBControlRegister);
    }

    void keyOffOpnbAdpcm()
    {
        keyOffOpnbAdpcmA();
        keyOffOpnbAdpcmB();
    }

    void triggerOpnbAdpcm(int midiNote, float velocity)
    {
        if (! chip || ! opnbAdpcmEnabledForPatch())
            return;

        triggerOpnbAdpcmA(velocity);
        triggerOpnbAdpcmB(midiNote, velocity);
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

    OPNBPitch pitchForNote(int midiNote) const
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

    bool channelEnabled(size_t channel) const
    {
        return channel < sourceChannelCount() && sourceEnabled(patch, channel);
    }

    bool anyAudibleSourceEnabled() const
    {
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
        {
            if (channelEnabled(channel))
                return true;
        }
        return false;
    }

    void applyChannelPatch(size_t channel, float velocity)
    {
        if (channel >= fmChannelCount())
            return;

        const auto nativeChannel = nativeFmChannel(channel);
        const auto algorithm = algorithmForPatch();
        const auto feedback = feedbackForPatch();
        currentAlgorithm[channel] = algorithm;
        currentFeedback[channel] = feedback;
        currentPanBits[channel] = ym2612PanBitsForPatch(patch, channel);

        for (size_t op = 0; op < 4; ++op)
        {
            const auto envelope = ym2612EnvelopeRegistersForPatch(patch, op);
            writeYmRegister(opRegForNativeChannel(0x30, nativeChannel, op), fmOperatorMultipleForPatch(ChipMode::ym2610, patch, op));
            writeYmRegister(opRegForNativeChannel(0x40, nativeChannel, op), fmOperatorTotalLevelForPatch(ChipMode::ym2610, patch, op, velocity));
            writeYmRegister(opRegForNativeChannel(0x50, nativeChannel, op), envelope.attackRate);
            writeYmRegister(opRegForNativeChannel(0x60, nativeChannel, op), envelope.decayRate);
            writeYmRegister(opRegForNativeChannel(0x70, nativeChannel, op), envelope.sustainRate);
            writeYmRegister(opRegForNativeChannel(0x80, nativeChannel, op), envelope.sustainRelease);
            writeYmRegister(opRegForNativeChannel(0x90, nativeChannel, op), 0x00u);
        }

        writeYmRegister(regForNativeChannel(0xb0, nativeChannel), static_cast<uint8_t>((feedback << 3u) | algorithm));
        writeYmRegister(regForNativeChannel(0xb4, nativeChannel), currentPanBits[channel]);
    }

    void applyPatchToAllChannels(bool preserveKeys)
    {
        for (size_t channel = 0; channel < fmChannelCount(); ++channel)
            applyChannelPatch(channel, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f);
        writeSsgSharedRegisters();
        updateSsgMixer();

        if (! preserveKeys)
            return;

        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
        {
            if (channelGateActive(channel) && channelNotes[channel] >= 0)
                triggerChannel(channel, channelNotes[channel], channelVelocity[channel], channelEnabled(channel));
        }
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= fmChannelCount())
        {
            triggerSsgChannel(channel, midiNote, velocity, shouldEnable);
            return;
        }

        if (channel >= channelNotes.size() || ! chip)
            return;

        const auto pitch = pitchForNote(midiNote);
        const auto nativeChannel = nativeFmChannel(channel);
        channelNotes[channel] = std::clamp(midiNote, 0, 127);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity) * sourceLevel(patch, channel));
        currentFnum[channel] = pitch.fnum;
        currentBlock[channel] = pitch.block;
        applyChannelPatch(channel, channelVelocity[channel]);

        writeYmRegister(regForNativeChannel(0xa4, nativeChannel), static_cast<uint8_t>(((pitch.block & 0x07u) << 3u) | ((pitch.fnum >> 8u) & 0x07u)));
        writeYmRegister(regForNativeChannel(0xa0, nativeChannel), static_cast<uint8_t>(pitch.fnum & 0xffu));

        const auto code = keyCodeForNativeChannel(nativeChannel);
        writeYmRegister(0x28, static_cast<uint8_t>(shouldEnable ? (0xf0u | code) : code));
        if (shouldEnable)
            keyOnMask |= static_cast<uint16_t>(1u << channel);
        else
            keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void keyOffChannel(size_t channel)
    {
        if (channel >= fmChannelCount())
        {
            keyOffSsgChannel(channel);
            return;
        }

        if (channel >= channelNotes.size())
            return;

        writeYmRegister(0x28, keyCodeForNativeChannel(nativeFmChannel(channel)));
        keyOnMask &= static_cast<uint16_t>(~(1u << channel));
    }

    void clearChipPolyState()
    {
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
            keyOffChannel(channel);
        keyOffOpnbAdpcm();
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
        {
            if (channelEnabled(channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
        {
            if (channelEnabled(channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
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
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
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
        if (opnbAdpcmEnabledForPatch())
            triggerOpnbAdpcm(midiNote, velocity);
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            keyOffChannel(channel);
        }
        if (opnbAdpcmEnabledForPatch())
            keyOffOpnbAdpcm();
    }

    void applyLaserDrift()
    {
        laserPhase += 1.0 / sampleRate;
        const auto bend = static_cast<int>(std::round(std::sin(twoPi * laserPhase * 8.0) * patch.control3 * 10.0));
        for (size_t channel = 0; channel < sourceChannelCount(); ++channel)
        {
            if (! channelGateActive(channel))
                continue;
            const auto note = (patch.playMode == PlayMode::chipPoly && channelNotes[channel] >= 0) ? channelNotes[channel] : heldNote;
            triggerChannel(channel, note + bend, channelVelocity[channel] > 0.0f ? channelVelocity[channel] : 1.0f, channelEnabled(channel));
        }
    }

    bool channelGateActive(size_t channel) const
    {
        if (channel < fmChannelCount())
            return (keyOnMask & (1u << channel)) != 0;
        if (channel < sourceChannelCount())
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
            const auto source = ssgLaneOffset() + ssg;
            const auto active = channelEnabled(source) && (ssgGateMask & (1u << source)) != 0;
            if (! active)
                mixer = static_cast<uint8_t>(mixer | (1u << ssg) | (1u << (ssg + 3u)));
        }
        writeYmRegister(0x07, mixer);
    }

    void triggerSsgChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel < ssgLaneOffset() || channel >= sourceChannelCount())
            return;

        const auto ssg = channel - ssgLaneOffset();
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
        if (channel < ssgLaneOffset() || channel >= sourceChannelCount())
            return;

        ssgGateMask &= static_cast<uint16_t>(~(1u << channel));
        writeSsgVolume(channel - ssgLaneOffset(), 0);
        updateSsgMixer();
    }

    double ssgOutputSample() const
    {
        if ((ssgGateMask & ssgLaneMask()) == 0)
            return 0.0;

        constexpr auto scale = 1.0 / 32768.0;
        return std::clamp(static_cast<double>(lastSsg) * scale, -1.0, 1.0);
    }

    AccuracyMode accuracy;
    bool opnb2 = false;
    static constexpr size_t opnbFmChannelCount = 4;
    static constexpr size_t maxFmChannelCount = 6;
    static constexpr size_t ssgChannelCount = 3;
    static constexpr size_t maxSourceChannelCount = maxFmChannelCount + ssgChannelCount;
    static constexpr std::array<size_t, 4> opnbNativeFmChannels { 1u, 2u, 4u, 5u };
    double sampleRate = 48000.0;
    double clock = 8000000.0;
    double chipSampleRate = 55555.0;
    double sampleAccumulator = 0.0;
    Host host;
    std::unique_ptr<ymfm::ym2610> chip;
    PatchConfig patch;
    std::array<uint8_t, 0x200> regs {};
    std::array<uint16_t, maxFmChannelCount> currentFnum {};
    std::array<uint8_t, maxFmChannelCount> currentBlock {};
    std::array<uint8_t, maxFmChannelCount> currentAlgorithm {};
    std::array<uint8_t, maxFmChannelCount> currentFeedback {};
    std::array<uint8_t, maxFmChannelCount> currentPanBits {};
    std::array<uint16_t, 3> currentSsgPeriod {};
    std::array<uint8_t, 3> currentSsgVolume {};
    std::array<int, maxSourceChannelCount> channelNotes {};
    std::array<uint8_t, 6> currentOpnbAdpcmALevels {};
    std::array<uint16_t, 6> currentOpnbAdpcmAStartRegisters {};
    std::array<uint16_t, 6> currentOpnbAdpcmAEndRegisters {};
    std::array<yamahaAdpcm::AdpcmARegionWindow, yamahaAdpcm::regionCountA> explicitAdpcmARegions {};
    bool usesExplicitAdpcmARegions = false;
    uint8_t opnbAdpcmAKeyBits = 0;
    uint8_t opnbAdpcmATotalLevel = 0x3f;
    uint8_t opnbAdpcmBControlRegister = 0;
    uint8_t opnbAdpcmBPanRegister = 0;
    uint16_t opnbAdpcmBStartRegister = 0;
    uint16_t opnbAdpcmBEndRegister = 0;
    uint16_t opnbAdpcmBDeltaNRegister = 0;
    uint8_t opnbAdpcmBLevelRegister = 0;
    std::array<float, maxSourceChannelCount> channelVelocity {};
    std::array<uint64_t, maxSourceChannelCount> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    uint16_t keyOnMask = 0;
    uint16_t ssgGateMask = 0;
    double laserPhase = 0.0;
    int32_t lastNativeLeft = 0;
    int32_t lastNativeRight = 0;
    int32_t lastSsg = 0;
    StereoFrame currentOutput {};
};

}
std::unique_ptr<ChipCore> makeYm2610Core(AccuracyMode accuracy, bool sixFmChannels)
{
    return std::make_unique<Ym2610Core>(accuracy, sixFmChannels);
}
}
