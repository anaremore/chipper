#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class NamcoWsgCore final : public ChipCore
{
public:
    explicit NamcoWsgCore(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 96000.0;
        frequency.fill(0);
        volume.fill(0);
        enabledMask = 0;
        phase.fill(0.0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        for (size_t channel = 0; channel < waveRam.size(); ++channel)
            seedWave(channel);
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
        if (reg < 0x80u)
        {
            const auto channel = std::min<size_t>(reg / 32u, waveRam.size() - 1u);
            waveRam[channel][reg & 31u] = value & 0x0fu;
            return;
        }

        if (reg >= 0x80u && reg < 0xa0u)
        {
            const auto channel = static_cast<size_t>((reg - 0x80u) / 4u);
            if (channel >= frequency.size())
                return;

            switch ((reg - 0x80u) & 3u)
            {
                case 0: frequency[channel] = static_cast<uint16_t>((frequency[channel] & 0x0f00u) | value); break;
                case 1: frequency[channel] = static_cast<uint16_t>((frequency[channel] & 0x00ffu) | ((value & 0x0fu) << 8u)); break;
                case 2: volume[channel] = value & 0x0fu; break;
                case 3:
                    if ((value & 0x01u) != 0)
                        enabledMask |= static_cast<uint8_t>(1u << channel);
                    else
                        enabledMask &= static_cast<uint8_t>(~(1u << channel));
                    break;
            }
            return;
        }

        if (reg == 0xbfu)
            enabledMask = value;
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
        const auto spread = static_cast<int>(std::round(patch.control1 * 19.0f));
        std::array<int, 8> notes {
            heldNote,
            heldNote + std::max(1, spread / 4),
            heldNote + std::max(2, spread / 2),
            heldNote + std::max(3, (spread * 3) / 4),
            heldNote + std::max(4, spread),
            heldNote - 12,
            heldNote + 12,
            heldNote + 19
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote - 17, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 17, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::laser:
                for (size_t channel = 0; channel < notes.size(); ++channel)
                    notes[channel] = heldNote + 24 - static_cast<int>(channel * 5) + static_cast<int>(std::round(patch.control2 * 12.0f));
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29, heldNote + 36, heldNote + 41 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        enabledMask = 0;
        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            frequency[channel] = namcoPeriodForNote(notes[channel]);
            volume[channel] = channelVolumeForPatch(channel);
            seedWave(channel);
            if (channelActiveForPatch(channel))
                enabledMask |= static_cast<uint8_t>(1u << channel);
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
            noteVelocity = 0.0f;
            enabledMask = 0;
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 8> panPositions { -1.0, -0.72, -0.44, -0.16, 0.16, 0.44, 0.72, 1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            const auto sample = renderChannel(channel);
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
            const auto decay = 1.0 - static_cast<double>(patch.envelopeDecay) * 0.0005;
            noteVelocity = static_cast<float>(std::max(0.0, static_cast<double>(noteVelocity) * decay));
        }

        return { static_cast<float>(std::clamp(left * noteVelocity, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right * noteVelocity, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t channel = 0; channel < frequency.size(); ++channel)
        {
            const auto base = static_cast<uint16_t>(0x80u + channel * 4u);
            writes.push_back({ 0, base, static_cast<uint8_t>(frequency[channel] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>((frequency[channel] >> 8u) & 0x0fu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 2u), volume[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 3u), static_cast<uint8_t>((enabledMask >> channel) & 1u) });
        }
        writes.push_back({ 0, 0xbfu, enabledMask });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::namcoWsg; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "Namco arcade WSG"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Eight simplified wavetable lanes, 32-sample 4-bit wave RAM, frequency, volume, and enable register paths are modeled; exact Namco custom-chip variants, voice count differences, waveform addressing, DAC/output curve, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Namco arcade WSG\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 0)) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 1)) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 2)) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 3)) << ","
             << "\"waveShapeChoice4\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 4)) << ","
             << "\"waveShapeChoice5\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 5)) << ","
             << "\"waveShapeChoice6\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 6)) << ","
             << "\"waveShapeChoice7\":" << static_cast<int>(wavetableWaveShapeForChannel(ChipMode::namcoWsg, patch, 7)) << ","
             << "\"period0\":" << frequency[0] << ","
             << "\"period1\":" << frequency[1] << ","
             << "\"period2\":" << frequency[2] << ","
             << "\"period3\":" << frequency[3] << ","
             << "\"period4\":" << frequency[4] << ","
             << "\"period7\":" << frequency[7] << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"volume7\":" << static_cast<int>(volume[7]) << ","
             << "\"enableMask\":" << static_cast<int>(enabledMask) << ","
             << "\"waveRam0\":" << static_cast<int>(waveRam[0][0]) << ","
             << "\"waveRam31\":" << static_cast<int>(waveRam[0][31]) << ","
             << "\"waveRam1_0\":" << static_cast<int>(waveRam[1][0]) << ","
             << "\"waveRam1_31\":" << static_cast<int>(waveRam[1][31]) << ","
             << "\"waveRam2_0\":" << static_cast<int>(waveRam[2][0]) << ","
             << "\"waveRam2_31\":" << static_cast<int>(waveRam[2][31]) << ","
             << "\"waveRam3_0\":" << static_cast<int>(waveRam[3][0]) << ","
             << "\"waveRam3_31\":" << static_cast<int>(waveRam[3][31]) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"exposedChannelCount\":8,"
             << "\"internalChannelCount\":8,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"assignedNote6\":" << channelNotes[6] << ","
             << "\"assignedNote7\":" << channelNotes[7] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t namcoPeriodForNote(int midiNote) const
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
        return sourceEnabled(patch, channel);
    }

    void seedWave(size_t channel)
    {
        waveRam[channel] = wavetableLaneForPatch(ChipMode::namcoWsg, patch, channel);
    }

    double renderChannel(size_t channel)
    {
        if ((enabledMask & (1u << channel)) == 0)
            return 0.0;

        const auto vol = static_cast<double>(volume[channel] & 0x0fu) / 15.0;
        if (vol <= 0.0)
            return 0.0;

        const auto hz = clock / (32.0 * static_cast<double>(std::max<uint16_t>(1, frequency[channel])));
        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);
        const auto index = static_cast<size_t>(std::floor(phase[channel] * 32.0)) & 31u;
        return ((static_cast<double>(waveRam[channel][index]) / 15.0) * 2.0 - 1.0) * vol;
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
        enabledMask = 0;
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
        frequency[index] = namcoPeriodForNote(channelNotes[index]);
        volume[index] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 1, 15));
        enabledMask |= static_cast<uint8_t>(1u << index);
        seedWave(index);
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
            enabledMask &= static_cast<uint8_t>(~(1u << channel));
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 96000.0;
    std::array<uint16_t, 8> frequency {};
    std::array<uint8_t, 8> volume {};
    uint8_t enabledMask = 0;
    std::array<std::array<uint8_t, 32>, 8> waveRam {};
    std::array<double, 8> phase {};
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 8> channelNotes {};
    std::array<float, 8> channelVelocity {};
    std::array<uint64_t, 8> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeNamcoWsgCore(AccuracyMode accuracy)
{
    return std::make_unique<NamcoWsgCore>(accuracy);
}
}
