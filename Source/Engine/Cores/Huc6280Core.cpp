#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Huc6280Core final : public ChipCore
{
public:
    explicit Huc6280Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 3579545.0;
        selectedChannel = 0;
        frequency.fill(0x0fff);
        control.fill(0x00);
        balance.fill(0xff);
        noiseControl.fill(0x00);
        phase.fill(0.0);
        noisePhase.fill(0.0);
        noiseLfsr.fill(0x1ffffu);
        waveWriteIndex.fill(0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        lfoPhase = 0.0;

        for (size_t channel = 0; channel < waveRam.size(); ++channel)
            seedWave(channel);
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        const auto waveContentChanged = ! wavetablePatchContentMatches(newPatch, patch);
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
        for (size_t channel = 0; channel < noiseControl.size(); ++channel)
        {
            noiseControl[channel] = huc6280NoiseControlForPatch(patch, channel);
            if (waveContentChanged)
                seedWave(channel);
        }
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        switch (address & 0x0fu)
        {
            case 0x00:
                selectedChannel = static_cast<uint8_t>(value % 6u);
                break;
            case 0x02:
                frequency[selectedChannel] = static_cast<uint16_t>((frequency[selectedChannel] & 0x0f00u) | value);
                break;
            case 0x03:
                frequency[selectedChannel] = static_cast<uint16_t>(((value & 0x0fu) << 8u) | (frequency[selectedChannel] & 0x00ffu));
                break;
            case 0x04:
                control[selectedChannel] = value;
                break;
            case 0x05:
                balance[selectedChannel] = value;
                break;
            case 0x06:
                writeWaveSample(selectedChannel, value);
                break;
            case 0x07:
                noiseControl[selectedChannel] = value;
                noiseLfsr[selectedChannel] = 0x1ffffu;
                break;
            default:
                break;
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
        const auto spread = static_cast<int>(std::round(patch.control1 * 19.0f));
        std::array<int, 6> notes {
            heldNote,
            heldNote + std::max(1, spread / 3),
            heldNote + std::max(2, (spread * 2) / 3),
            heldNote + std::max(3, spread),
            heldNote - 12,
            heldNote + 12
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote - 17, heldNote + 7 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::laser:
                notes[0] = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                notes[1] = heldNote;
                notes[2] = heldNote - 12;
                notes[3] = heldNote - 19;
                break;
            case MacroKind::jump:
                notes[0] = heldNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                notes[1] = heldNote + 7;
                notes[2] = heldNote + 12;
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17, heldNote + 24, heldNote + 29 };
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            frequency[channel] = hucPeriodForNote(notes[channel]);
            control[channel] = channelActiveForPatch(channel) ? channelControlForPatch(channel) : 0x00u;
            balance[channel] = 0xffu;
            noiseControl[channel] = channelActiveForPatch(channel) ? huc6280NoiseControlForPatch(patch, channel) : 0x00u;
            seedWave(channel);
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
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 6> panPositions { -1.0, -0.6, -0.2, 0.2, 0.6, 1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            const auto sample = renderChannel(channel);
            if (sample == 0.0)
                continue;

            ++audibleCount;
            const auto gains = modernStereoGains(patch, panPositions[channel]);
            left += sample * gains.left;
            right += sample * gains.right;
        }

        const auto scale = static_cast<double>(noteVelocity) * 0.9 / static_cast<double>(std::max(1, audibleCount));
        return { static_cast<float>(left * scale), static_cast<float>(right * scale) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(6 * 5);
        for (uint8_t channel = 0; channel < 6; ++channel)
        {
            writes.push_back({ 0, 0x00, channel });
            writes.push_back({ 0, 0x02, static_cast<uint8_t>(frequency[channel] & 0xffu) });
            writes.push_back({ 0, 0x03, static_cast<uint8_t>((frequency[channel] >> 8u) & 0x0fu) });
            writes.push_back({ 0, 0x04, control[channel] });
            writes.push_back({ 0, 0x05, balance[channel] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::huc6280; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "PC Engine HuC6280"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Six channel-select, frequency, control, balance, waveform-RAM, and noise-control register paths are modeled with wavetable playback, simplified LFSR noise, and a partial channel 2 to channel 1 FM-LFO pairing; exact LFO register timing/scaling, exact noise taps, DDA details, stereo register behavior, timer edge timing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"PC Engine HuC6280\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"selectedChannel\":" << static_cast<int>(selectedChannel) << ","
             << "\"waveShapeChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 0)) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 1)) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 2)) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 3)) << ","
             << "\"waveShapeChoice4\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 4)) << ","
             << "\"waveShapeChoice5\":" << static_cast<int>(huc6280WaveShapeForChannel(patch, 5)) << ","
             << "\"frequency0\":" << frequency[0] << ","
             << "\"frequency1\":" << frequency[1] << ","
             << "\"frequency2\":" << frequency[2] << ","
             << "\"frequency3\":" << frequency[3] << ","
             << "\"frequency4\":" << frequency[4] << ","
             << "\"frequency5\":" << frequency[5] << ","
             << "\"control0\":" << static_cast<int>(control[0]) << ","
             << "\"control1\":" << static_cast<int>(control[1]) << ","
             << "\"control2\":" << static_cast<int>(control[2]) << ","
             << "\"control3\":" << static_cast<int>(control[3]) << ","
             << "\"control4\":" << static_cast<int>(control[4]) << ","
             << "\"control5\":" << static_cast<int>(control[5]) << ","
             << "\"noiseControl0\":" << static_cast<int>(noiseControl[0]) << ","
             << "\"noiseControl5\":" << static_cast<int>(noiseControl[5]) << ","
             << "\"lfoChoice\":" << std::clamp(patch.dmgStereoRoute, 0, 4) << ","
             << "\"lfoMode\":" << static_cast<int>(huc6280LfoModeForPatch(patch)) << ","
             << "\"lfoEnabled\":" << (lfoEnabled() ? 1 : 0) << ","
             << "\"lfoSourceChannel\":1,"
             << "\"lfoTargetChannel\":0,"
             << "\"lfoDepthCents\":" << static_cast<int>(std::round(lfoDepthSemitones() * 100.0)) << ","
             << "\"lfoRateHz\":" << lfoRateHz() << ","
             << "\"waveRam0\":" << static_cast<int>(waveRam[0][0]) << ","
             << "\"waveRam31\":" << static_cast<int>(waveRam[0][31]) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"sourceLevel5\":" << sourceLevel(patch, 5) << ","
             << "\"uiExposesSixChannels\":1,"
             << "\"internalChannelCount\":6,"
             << "\"chipPolyPlayableChannels\":6,"
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"assignedNote4\":" << channelNotes[4] << ","
             << "\"assignedNote5\":" << channelNotes[5] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    uint16_t hucPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (32.0 * hz)), 1.0, 4095.0));
    }

    uint8_t channelControlForPatch(size_t) const
    {
        const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control4 * 31.0f)), 1, 31));
        return static_cast<uint8_t>(0x80u | volume);
    }

    bool channelAudible(size_t channel) const
    {
        if (channel == 1u && lfoEnabled())
            return false;
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);
        return false;
    }

    bool chipPolyChannelEnabled(size_t channel) const
    {
        if (channel < patch.sourceEnabled.size())
            return sourceEnabled(patch, channel);
        return false;
    }

    bool channelActiveForPatch(size_t channel) const
    {
        return chipPolyChannelEnabled(channel);
    }

    void seedWave(size_t channel)
    {
        waveRam[channel] = wavetableLaneForPatch(ChipMode::huc6280, patch, channel);
    }

    void writeWaveSample(size_t channel, uint8_t value)
    {
        waveRam[channel][waveWriteIndex[channel]] = value & 0x1fu;
        waveWriteIndex[channel] = static_cast<uint8_t>((waveWriteIndex[channel] + 1u) & 31u);
    }

    double channelVolume(size_t channel) const
    {
        if ((control[channel] & 0x80u) == 0)
            return 0.0;
        return static_cast<double>(control[channel] & 0x1fu) / 31.0;
    }

    double renderChannel(size_t channel)
    {
        if (channel == 1u && lfoEnabled())
        {
            advanceLfoSource();
            return 0.0;
        }

        if (! channelAudible(channel))
            return 0.0;

        const auto volume = channelVolume(channel) * sourceLevel(patch, channel);
        if (volume <= 0.0)
            return 0.0;

        const auto period = std::max<uint16_t>(1, frequency[channel]);
        auto hz = clock / (32.0 * static_cast<double>(period));
        if (channel == 0u && lfoEnabled())
        {
            const auto shape = currentLfoSample();
            hz *= std::pow(2.0, (shape * lfoDepthSemitones()) / 12.0);
        }
        const auto useNoise = (noiseControl[channel] & 0x80u) != 0 || huc6280ChannelUsesNoiseForPatch(patch, channel);
        if (useNoise)
            return renderNoise(channel, hz) * volume;

        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);
        const auto index = static_cast<size_t>(std::floor(phase[channel] * 32.0)) & 31u;
        return ((static_cast<double>(waveRam[channel][index]) / 31.0) * 2.0 - 1.0) * volume;
    }

    double renderNoise(size_t channel, double baseHz)
    {
        const auto rateShift = std::clamp(static_cast<int>(noiseControl[channel] & 0x1fu), 0, 31);
        const auto hz = std::max(20.0, baseHz * (1.0 + static_cast<double>(31 - rateShift) / 8.0));
        noisePhase[channel] += hz / sampleRate;
        while (noisePhase[channel] >= 1.0)
        {
            noisePhase[channel] -= 1.0;
            const auto feedback = ((noiseLfsr[channel] & 1u) ^ ((noiseLfsr[channel] >> 5u) & 1u)) & 1u;
            noiseLfsr[channel] = ((noiseLfsr[channel] >> 1u) | (feedback << 16u)) & 0x1ffffu;
            if (noiseLfsr[channel] == 0)
                noiseLfsr[channel] = 0x1ffffu;
        }
        return (noiseLfsr[channel] & 1u) != 0 ? 1.0 : -1.0;
    }

    bool lfoEnabled() const
    {
        return huc6280LfoModeForPatch(patch) > 1u
            && sourceEnabled(patch, 0)
            && sourceEnabled(patch, 1);
    }

    double lfoDepthSemitones() const
    {
        return huc6280LfoDepthSemitonesForPatch(patch);
    }

    double lfoRateHz() const
    {
        return huc6280LfoRateHzForPatch(patch);
    }

    double currentLfoSample() const
    {
        const auto index = static_cast<size_t>(std::floor(lfoPhase * 32.0)) & 31u;
        return (static_cast<double>(waveRam[1][index]) / 31.0) * 2.0 - 1.0;
    }

    void advanceLfoSource()
    {
        lfoPhase = wrapPhase(lfoPhase + lfoRateHz() / sampleRate);
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (chipPolyChannelEnabled(channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (chipPolyChannelEnabled(channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelStamp.size(); ++channel)
        {
            if (chipPolyChannelEnabled(channel) && channelStamp[channel] < oldestStamp)
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
            if (chipPolyChannelEnabled(channel) && channelNotes[channel] >= 0)
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
        control.fill(0x00);
        noiseControl.fill(0x00);
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
        frequency[index] = hucPeriodForNote(channelNotes[index]);
        const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 31.0f)), 1, 31));
        control[index] = static_cast<uint8_t>(0x80u | volume);
        noiseControl[index] = huc6280NoiseControlForPatch(patch, index);
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
            control[channel] = 0x00;
            noiseControl[channel] = 0x00;
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3579545.0;
    uint8_t selectedChannel = 0;
    std::array<uint16_t, 6> frequency {};
    std::array<uint8_t, 6> control {};
    std::array<uint8_t, 6> balance {};
    std::array<uint8_t, 6> noiseControl {};
    std::array<std::array<uint8_t, 32>, 6> waveRam {};
    std::array<uint8_t, 6> waveWriteIndex {};
    std::array<double, 6> phase {};
    std::array<double, 6> noisePhase {};
    std::array<uint32_t, 6> noiseLfsr {};
    double lfoPhase = 0.0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 6> channelNotes {};
    std::array<float, 6> channelVelocity {};
    std::array<uint64_t, 6> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeHuc6280Core(AccuracyMode accuracy)
{
    return std::make_unique<Huc6280Core>(accuracy);
}
}
