#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class SccCore final : public ChipCore
{
public:
    explicit SccCore(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    ~SccCore() override
    {
        if (emu != nullptr)
            SCC_delete(emu);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        period.fill(0);
        volume.fill(0);
        keyOnMask = 0;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        recreateEmu();
        for (size_t channel = 0; channel < waveRam.size(); ++channel)
            seedWave(channel);
        syncAllRegistersToEmu();
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        const auto waveContentChanged = ! wavetablePatchContentMatches(nextPatch, patch);
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();
        patch = nextPatch;

        if (waveContentChanged)
            for (size_t channel = 0; channel < waveRam.size(); ++channel)
                seedWave(channel);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        if (reg < 0xa0u)
        {
            const auto channel = std::min<size_t>(reg / 32u, waveRam.size() - 1u);
            waveRam[channel][reg & 31u] = value;
            writeEmuRegister(reg, value);
            return;
        }

        if (reg >= 0xa0u && reg <= 0xa9u)
        {
            const auto channel = static_cast<size_t>((reg - 0xa0u) / 2u);
            if (channel >= period.size())
                return;

            if (((reg - 0xa0u) & 1u) == 0)
                period[channel] = static_cast<uint16_t>((period[channel] & 0x0f00u) | value);
            else
                period[channel] = static_cast<uint16_t>((period[channel] & 0x00ffu) | ((value & 0x0fu) << 8u));
            writeEmuRegister(static_cast<uint8_t>(0xc0u + (reg - 0xa0u)), value);
            return;
        }

        if (reg >= 0xaau && reg <= 0xaeu)
        {
            const auto channel = static_cast<size_t>(reg - 0xaau);
            if (channel < volume.size())
            {
                volume[channel] = value & 0x0fu;
                writeEmuRegister(static_cast<uint8_t>(0xd0u + channel), value);
            }
            return;
        }

        if (reg == 0xafu)
        {
            keyOnMask = value & 0x1fu;
            writeEmuRegister(0xe1u, keyOnMask);
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(patch.control1 * 16.0f));
        std::array<int, 5> notes {
            heldNote,
            heldNote + std::max(1, spread / 3),
            heldNote + std::max(2, (spread * 2) / 3),
            heldNote + std::max(3, spread),
            heldNote + 12
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote + 7 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12 };
                break;
            case MacroKind::laser:
                notes[0] = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                notes[1] = heldNote;
                notes[2] = heldNote - 12;
                notes[3] = heldNote - 19;
                notes[4] = heldNote - 24;
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        keyOnMask = 0;
        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            period[channel] = sccPeriodForNote(notes[channel]);
            volume[channel] = channelVolumeForPatch(channel);
            seedWave(channel);
            if (channelActiveForPatch(channel))
                keyOnMask |= static_cast<uint8_t>(1u << channel);
        }
        syncAllRegistersToEmu();
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
            keyOnMask = 0;
            writeEmuRegister(0xe1u, keyOnMask);
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 5> panPositions { -1.0, -0.5, 0.0, 0.5, 1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        if (emu != nullptr)
            SCC_calc(emu);

        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            const auto sample = renderChannelFromEmu(channel);
            if (std::abs(sample) <= 1.0e-9)
                continue;

            ++audibleCount;
            const auto spread = std::clamp(static_cast<double>(patch.stereoSpread), 0.0, 1.0);
            const auto pan = panPositions[channel] * spread;
            left += sample * (pan <= 0.0 ? 1.0 : 1.0 - pan);
            right += sample * (pan >= 0.0 ? 1.0 : 1.0 + pan);
        }

        if (audibleCount > 0)
        {
            left /= static_cast<double>(audibleCount);
            right /= static_cast<double>(audibleCount);
        }

        if (patch.envelopeDecay > 0.001f && noteVelocity > 0.0f)
        {
            const auto decay = 1.0 - static_cast<double>(patch.envelopeDecay) * 0.00045;
            noteVelocity = static_cast<float>(std::max(0.0, static_cast<double>(noteVelocity) * decay));
        }

        return { static_cast<float>(std::clamp(left * noteVelocity, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right * noteVelocity, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t channel = 0; channel < period.size(); ++channel)
        {
            writes.push_back({ 0, static_cast<uint16_t>(0xa0u + channel * 2u), static_cast<uint8_t>(period[channel] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(0xa1u + channel * 2u), static_cast<uint8_t>((period[channel] >> 8u) & 0x0fu) });
            writes.push_back({ 0, static_cast<uint16_t>(0xaau + channel), volume[channel] });
        }
        writes.push_back({ 0, 0xafu, keyOnMask });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::scc; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "Konami SCC"; }
    std::string implementedAccuracy() const override { return "partial emu2212-backed register-level"; }
    std::string limitations() const override
    {
        return "Five independent frequency, volume, key-on, and waveform-RAM paths are driven through MIT emu2212 enhanced mode; SCC/SCC+ selection, original channel D/E wave sharing, exact mapper/bank behavior, output curve, timing edge cases, golden emulator comparison, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Konami SCC\","
             << "\"implementedAccuracy\":\"partial emu2212-backed register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"core\":\"emu2212\","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 0)) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 1)) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 2)) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 3)) << ","
             << "\"waveShapeChoice4\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::scc, patch, 4)) << ","
             << "\"period0\":" << period[0] << ","
             << "\"period1\":" << period[1] << ","
             << "\"period2\":" << period[2] << ","
             << "\"period3\":" << period[3] << ","
             << "\"period4\":" << period[4] << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"volume4\":" << static_cast<int>(volume[4]) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"waveRam0\":" << static_cast<int>(waveRam[0][0]) << ","
             << "\"waveRam31\":" << static_cast<int>(waveRam[0][31]) << ","
             << "\"waveRam1_0\":" << static_cast<int>(waveRam[1][0]) << ","
             << "\"waveRam1_31\":" << static_cast<int>(waveRam[1][31]) << ","
             << "\"waveRam2_0\":" << static_cast<int>(waveRam[2][0]) << ","
             << "\"waveRam2_31\":" << static_cast<int>(waveRam[2][31]) << ","
             << "\"waveRam3_0\":" << static_cast<int>(waveRam[3][0]) << ","
             << "\"waveRam3_31\":" << static_cast<int>(waveRam[3][31]) << ","
             << "\"waveRam4_0\":" << static_cast<int>(waveRam[4][0]) << ","
             << "\"waveRam4_31\":" << static_cast<int>(waveRam[4][31]) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"uiExposesFiveChannels\":1,"
             << "\"internalChannelCount\":5,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t sccPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * hz)), 1.0, 4095.0));
    }

    uint8_t channelVolumeForPatch(size_t channel) const
    {
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 15.0f)), 1, 15);
        const auto trim = sourceLevel(patch, channel);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(base) * trim)), 0, 15));
    }

    bool channelActiveForPatch(size_t channel) const
    {
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);

        return patch.macro == MacroKind::arp
            || patch.macro == MacroKind::powerUp
            || patch.macro == MacroKind::hit;
    }

    void seedWave(size_t channel)
    {
        waveRam[channel] = wavetableLaneForPatch(ChipMode::scc, patch, channel);
        syncWaveToEmu(channel);
    }

    double renderChannelFromEmu(size_t channel) const
    {
        if (emu == nullptr)
            return 0.0;

        if ((keyOnMask & (1u << channel)) == 0)
            return 0.0;

        const auto vol = static_cast<double>(volume[channel] & 0x0fu) / 15.0;
        if (vol <= 0.0)
            return 0.0;

        return std::clamp(static_cast<double>(emu->ch_out[channel]) / 4096.0, -1.0, 1.0);
    }

    void recreateEmu()
    {
        const auto emuClock = static_cast<uint32_t>(std::max(1.0, std::round(clock)));
        const auto outputRate = static_cast<uint32_t>(std::max(1.0, std::round(sampleRate)));
        if (emu == nullptr)
            emu = SCC_new(emuClock, outputRate);
        else
        {
            SCC_set_clock(emu, emuClock);
            SCC_set_rate(emu, outputRate);
        }
        if (emu == nullptr)
            return;

        SCC_set_type(emu, SCC_ENHANCED);
        SCC_set_quality(emu, 0);
        SCC_reset(emu);
        writeEmuRegister(0xe0u, 0x01u);
    }

    void writeEmuRegister(uint8_t address, uint8_t value) const
    {
        if (emu != nullptr)
            SCC_writeReg(emu, address, value);
    }

    void syncWaveToEmu(size_t channel) const
    {
        if (emu == nullptr || channel >= waveRam.size())
            return;

        const auto base = static_cast<uint8_t>(channel * 32u);
        for (uint8_t index = 0; index < 32u; ++index)
            writeEmuRegister(static_cast<uint8_t>(base + index), waveRam[channel][index]);
    }

    void syncAllRegistersToEmu() const
    {
        if (emu == nullptr)
            return;

        writeEmuRegister(0xe0u, 0x01u);
        for (size_t channel = 0; channel < period.size(); ++channel)
        {
            syncWaveToEmu(channel);
            writeEmuRegister(static_cast<uint8_t>(0xc0u + channel * 2u), static_cast<uint8_t>(period[channel] & 0xffu));
            writeEmuRegister(static_cast<uint8_t>(0xc1u + channel * 2u), static_cast<uint8_t>((period[channel] >> 8u) & 0x0fu));
            writeEmuRegister(static_cast<uint8_t>(0xd0u + channel), volume[channel]);
        }
        writeEmuRegister(0xe1u, keyOnMask);
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
        keyOnMask = 0;
        writeEmuRegister(0xe1u, keyOnMask);
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
        period[index] = sccPeriodForNote(channelNotes[index]);
        volume[index] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<float>(channelVolumeForPatch(index)) * channelVelocity[index])), 0, 15));
        keyOnMask |= static_cast<uint8_t>(1u << index);
        seedWave(index);
        syncAllRegistersToEmu();
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
            keyOnMask &= static_cast<uint8_t>(~(1u << channel));
        }
        writeEmuRegister(0xe1u, keyOnMask);
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    std::array<uint16_t, 5> period {};
    std::array<uint8_t, 5> volume {};
    uint8_t keyOnMask = 0;
    std::array<std::array<uint8_t, 32>, 5> waveRam {};
    SCC* emu = nullptr;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 5> channelNotes {};
    std::array<float, 5> channelVelocity {};
    std::array<uint64_t, 5> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeSccCore(AccuracyMode accuracy)
{
    return std::make_unique<SccCore>(accuracy);
}
}
