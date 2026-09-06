#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
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

        const auto opllClock = static_cast<uint32_t>(std::round(clock));
        const auto outputRate = static_cast<uint32_t>(std::round(sampleRate));
        if (opll == nullptr)
            opll = OPLL_new(opllClock, outputRate);
        else
            OPLL_setClockRate(opll, opllClock, outputRate);
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
        applyCustomPatchIfNeeded();
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
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "YM2413 / OPLL"; }
    std::string implementedAccuracy() const override { return "verified partial emu2413 register-level"; }
    std::string limitations() const override
    {
        return "MIT-licensed emu2413 provides the OPLL synthesis core for melodic and rhythm output; Chipper maps musical controls to preset-instrument, user-patch slot 0, f-number/block, key-on, channel volume, source-card rhythm trims for BD/HH+SD/TOM+CYM, and $0E rhythm registers. Split HH/SD and TOM/CYM fine tuning, VRC7/YMF281 variants, and hardware comparison are not complete.";
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
             << "\"customPatchEnabled\":" << (opllCustomPatchEnabledForPatch(patch) ? 1 : 0) << ","
             << "\"customPatchByte0\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[0]) << ","
             << "\"customPatchByte1\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[1]) << ","
             << "\"customPatchByte2\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[2]) << ","
             << "\"customPatchByte3\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[3]) << ","
             << "\"customPatchByte4\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[4]) << ","
             << "\"customPatchByte5\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[5]) << ","
             << "\"customPatchByte6\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[6]) << ","
             << "\"customPatchByte7\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[7]) << ","
             << "\"customModulatorMultiple\":" << static_cast<int>(opllOperatorMultipleForPatch(patch, 0)) << ","
             << "\"customCarrierMultiple\":" << static_cast<int>(opllOperatorMultipleForPatch(patch, 1)) << ","
             << "\"customModulatorTotalLevel\":" << static_cast<int>(opllModulatorTotalLevelForPatch(patch)) << ","
             << "\"customFeedback\":" << static_cast<int>(opllFeedbackForPatch(patch)) << ","
             << "\"rhythmModeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 2) << ","
             << "\"rhythmMode\":" << (rhythmModeActive() ? 1 : 0) << ","
             << "\"rhythmRegister\":" << static_cast<int>(regs[0x0e]) << ","
             << "\"rhythmKeyBits\":" << static_cast<int>(rhythmKeyBits) << ","
             << "\"rhythmSourceKeyBits\":" << static_cast<int>(ym2413RhythmKeyBitsForPatch(patch)) << ","
             << "\"rhythmSlotCount\":5,"
             << "\"rhythmMixerSlots\":3,"
             << "\"rhythmBdSource\":6,"
             << "\"rhythmHatSnareSource\":7,"
             << "\"rhythmTomCymSource\":8,"
             << "\"rhythmBdKeyBit\":16,"
             << "\"rhythmHatKeyBit\":1,"
             << "\"rhythmSnareKeyBit\":8,"
             << "\"rhythmTomKeyBit\":4,"
             << "\"rhythmCymKeyBit\":2,"
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
        const auto carrierTrim = opllCustomPatchEnabledForPatch(patch)
            ? std::clamp(0.5f + static_cast<float>(clamp01(patch.fmOperatorLevels[1])), 0.25f, 1.5f)
            : 1.0f;
        const auto musicalLevel = std::clamp(static_cast<float>(clamp01(patch.control4) * clamp01(velocity) * trim * carrierTrim), 0.0f, 1.0f);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0 - musicalLevel) * 15.0)), 0, 15));
    }

    void writeOpllRegister(uint8_t reg, uint8_t value)
    {
        if (opll == nullptr)
            return;

        regs[reg & 0x3fu] = value;
        OPLL_writeReg(opll, reg & 0x3fu, value);
    }

    void applyCustomPatchIfNeeded()
    {
        if (! opllCustomPatchEnabledForPatch(patch))
            return;

        const auto bytes = opllCustomPatchBytesForPatch(patch);
        for (size_t reg = 0; reg < bytes.size(); ++reg)
            writeOpllRegister(static_cast<uint8_t>(reg), bytes[reg]);
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (channel >= 9 || opll == nullptr)
            return;

        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f));
        const auto pitch = pitchForNote(midiNote + detune);
        applyCustomPatchIfNeeded();
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

        const auto bdVolume = ym2413RhythmBassDrumVolumeForPatch(patch, velocity);
        const auto hhVolume = ym2413RhythmHatVolumeForPatch(patch, velocity);
        const auto sdVolume = ym2413RhythmSnareVolumeForPatch(patch, velocity);
        const auto tomVolume = ym2413RhythmTomVolumeForPatch(patch, velocity);
        const auto cymVolume = ym2413RhythmCymVolumeForPatch(patch, velocity);
        writeOpllRegister(0x36u, bdVolume);
        writeOpllRegister(0x37u, static_cast<uint8_t>((hhVolume << 4u) | sdVolume));
        writeOpllRegister(0x38u, static_cast<uint8_t>((tomVolume << 4u) | cymVolume));

        const auto keyBits = ym2413RhythmKeyBitsForPatch(patch);

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

}
std::unique_ptr<ChipCore> makeYm2413Core(AccuracyMode accuracy)
{
    return std::make_unique<Ym2413Core>(accuracy);
}
}
