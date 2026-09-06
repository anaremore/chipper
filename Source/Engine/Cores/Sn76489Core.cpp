#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
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
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
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
        const auto emuClock = static_cast<uint32_t>(std::max(1.0, std::round(clock)));
        const auto outputRate = static_cast<uint32_t>(std::max(1.0, std::round(sampleRate)));
        if (emu == nullptr)
            emu = SNG_new(emuClock, outputRate);
        else
        {
            SNG_set_clock(emu, emuClock);
            SNG_set_rate(emu, outputRate);
        }
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

}
std::unique_ptr<ChipCore> makeSn76489Core(AccuracyMode accuracy)
{
    return std::make_unique<Sn76489Core>(accuracy);
}
}
