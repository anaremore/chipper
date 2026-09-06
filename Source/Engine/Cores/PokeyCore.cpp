#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class PokeyCore final : public ChipCore
{
public:
    explicit PokeyCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 1789790.0;
        audf.fill(0xff);
        audc.fill(0x00);
        phase.fill(0.0);
        highPassHold.fill(0.0);
        highPassClockLast.fill(1.0);
        audctl = 0x00;
        poly4 = 0x0fu;
        poly5 = 0x1fu;
        poly17 = 0x1ffffu;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled
            || pokeyAudctlForPatch(newPatch) != pokeyAudctlForPatch(patch))
            clearChipPolyState();

        patch = newPatch;
        audctl = pokeyAudctlForPatch(patch);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto index = static_cast<uint8_t>(address & 0x0fu);
        switch (index)
        {
            case 0x00: audf[0] = value; break;
            case 0x01: audc[0] = value; break;
            case 0x02: audf[1] = value; break;
            case 0x03: audc[1] = value; break;
            case 0x04: audf[2] = value; break;
            case 0x05: audc[2] = value; break;
            case 0x06: audf[3] = value; break;
            case 0x07: audc[3] = value; break;
            case 0x08: audctl = value; break;
            default: break;
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
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        std::array<int, 4> notes {
            heldNote,
            heldNote + std::max(1, spread / 3),
            heldNote + std::max(2, (spread * 2) / 3),
            heldNote + std::max(3, spread)
        };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 };
                break;
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote };
                break;
            case MacroKind::drum:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7 };
                break;
            case MacroKind::laser:
                notes = { heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote, heldNote - 12, heldNote - 19 };
                break;
            case MacroKind::jump:
                notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12 };
                break;
            case MacroKind::lead:
            case MacroKind::hit:
            case MacroKind::manual:
            default:
                break;
        }

        const auto control = pokeyAudcForPatch(patch);
        audctl = pokeyAudctlForPatch(patch);
        audc.fill(0x00u);
        for (size_t channel = 0; channel < 4; ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

            writePitchToChannel(channel, notes[channel]);
            audc[channel] = sourceEnabled(patch, channel) ? control : 0x00u;
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
        static constexpr std::array<double, 4> panPositions { -1.0, -0.35, 0.35, 1.0 };
        std::array<double, 4> channelSamples {};
        double left = 0.0;
        double right = 0.0;

        for (size_t channel = 0; channel < 4; ++channel)
            channelSamples[channel] = renderChannel(channel) * sourceLevel(patch, channel);

        applyHighPassFilters(channelSamples);

        for (size_t channel = 0; channel < 4; ++channel)
        {
            const auto gains = modernStereoGains(patch, panPositions[channel]);
            left += channelSamples[channel] * gains.left;
            right += channelSamples[channel] * gains.right;
        }

        const auto scale = static_cast<double>(noteVelocity) * 0.85 / 4.0;
        return { static_cast<float>(left * scale), static_cast<float>(right * scale) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(9);
        for (uint8_t channel = 0; channel < 4; ++channel)
        {
            writes.push_back({ 0, static_cast<uint16_t>(channel * 2u), audf[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(channel * 2u + 1u), audc[channel] });
        }
        writes.push_back({ 0, 0x08, audctl });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::pokey; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "Atari POKEY"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Four AUDF/AUDC/AUDV-style channels, note-to-timer writes, source gating, chip-poly allocation, AUDCTL 16-bit channel-pair bits, simplified high-pass filter switches, and simplified pure/poly4/poly5/poly17 texture paths are modeled; 1.79 MHz direct clocks, exact polynomial taps, serial behavior, DAC curve, exact paired-channel carry timing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Atari POKEY\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"audf0\":" << static_cast<int>(audf[0]) << ","
             << "\"audf1\":" << static_cast<int>(audf[1]) << ","
             << "\"audf2\":" << static_cast<int>(audf[2]) << ","
             << "\"audf3\":" << static_cast<int>(audf[3]) << ","
             << "\"audc0\":" << static_cast<int>(audc[0]) << ","
             << "\"audc1\":" << static_cast<int>(audc[1]) << ","
             << "\"audc2\":" << static_cast<int>(audc[2]) << ","
             << "\"audc3\":" << static_cast<int>(audc[3]) << ","
             << "\"audctl\":" << static_cast<int>(audctl) << ","
             << "\"pair12\":" << (pair12Enabled() ? 1 : 0) << ","
             << "\"pair34\":" << (pair34Enabled() ? 1 : 0) << ","
             << "\"filterChoice\":" << static_cast<int>(pokeyFilterChoiceForPatch(patch)) << ","
             << "\"filterBits\":" << static_cast<int>(pokeyFilterBitsForPatch(patch)) << ","
             << "\"filterCh1ByCh3\":" << (highPass13Enabled() ? 1 : 0) << ","
             << "\"filterCh2ByCh4\":" << (highPass24Enabled() ? 1 : 0) << ","
             << "\"audfPair12\":" << pairedAudf(0) << ","
             << "\"audfPair34\":" << pairedAudf(2) << ","
             << "\"distortionChoice\":" << std::clamp(patch.waveShape, 0, 4) << ","
             << "\"distortionCode\":" << static_cast<int>(pokeyAudcForPatch(patch) & 0xf0u) << ","
             << "\"volumeNibble\":" << static_cast<int>(pokeyAudcForPatch(patch) & 0x0fu) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"stereoSpread\":" << clamp01(patch.stereoSpread) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"assignedNote3\":" << channelNotes[3] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    static double volumeToLinear(uint8_t control)
    {
        return static_cast<double>(control & 0x0fu) / 15.0;
    }

    double channelClockHz(size_t channel) const
    {
        const auto period = static_cast<double>(channelIsPairedLowByte(channel) ? pairedAudf(channel) : audf[channel]) + 1.0;
        return clock / (56.0 * period);
    }

    double renderChannel(size_t channel)
    {
        if (channelIsPairedHighByte(channel))
            return 0.0;

        const auto drivesHighPass = (channel == 2u && highPass13Enabled()) || (channel == 3u && highPass24Enabled());
        if (! sourceEnabled(patch, channel) && ! drivesHighPass)
            return 0.0;

        const auto raw = advanceChannel(channel);

        if (! sourceEnabled(patch, channel))
            return 0.0;

        const auto volume = volumeToLinear(audc[channel]);
        if (volume <= 0.0)
            return 0.0;

        return raw * volume;
    }

    double advanceChannel(size_t channel)
    {
        const auto hz = channelClockHz(channel);
        phase[channel] = wrapPhase(phase[channel] + hz / sampleRate);
        if (phase[channel] < 0.5)
            return lastOutput[channel];

        phase[channel] -= 0.5;
        const auto distortion = audc[channel] & 0xf0u;
        if (distortion == 0x20u)
            clockPoly4(channel);
        else if (distortion == 0x40u)
            clockPoly5(channel);
        else if (distortion == 0x80u)
            clockPoly17(channel);
        else
            lastOutput[channel] = -lastOutput[channel];

        return lastOutput[channel];
    }

    bool pair12Enabled() const { return (audctl & 0x10u) != 0; }
    bool pair34Enabled() const { return (audctl & 0x08u) != 0; }
    bool highPass13Enabled() const { return (audctl & 0x04u) != 0; }
    bool highPass24Enabled() const { return (audctl & 0x02u) != 0; }

    void applyHighPassFilters(std::array<double, 4>& samples)
    {
        if (highPass13Enabled())
            samples[0] = applyHighPassPath(samples[0], 0, 2);
        else
            highPassHold[0] = 0.0;

        if (highPass24Enabled())
            samples[1] = applyHighPassPath(samples[1], 1, 3);
        else
            highPassHold[1] = 0.0;

        highPassClockLast[0] = lastOutput[2];
        highPassClockLast[1] = lastOutput[3];
    }

    double applyHighPassPath(double input, size_t outputIndex, size_t clockChannel)
    {
        const auto slot = outputIndex == 0u ? 0u : 1u;
        const auto output = input - highPassHold[slot];
        if (lastOutput[clockChannel] != highPassClockLast[slot])
            highPassHold[slot] = input;
        return output;
    }

    bool channelIsPairedHighByte(size_t channel) const
    {
        return (channel == 1u && pair12Enabled()) || (channel == 3u && pair34Enabled());
    }

    bool channelIsPairedLowByte(size_t channel) const
    {
        return (channel == 0u && pair12Enabled()) || (channel == 2u && pair34Enabled());
    }

    uint16_t pairedAudf(size_t lowChannel) const
    {
        if (lowChannel + 1u >= audf.size())
            return audf[lowChannel];

        return static_cast<uint16_t>(audf[lowChannel] | (static_cast<uint16_t>(audf[lowChannel + 1u]) << 8u));
    }

    uint16_t audf16ForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto divisor = std::round(clock / (56.0 * hz) - 1.0);
        return static_cast<uint16_t>(std::clamp(static_cast<int>(divisor), 0, 65535));
    }

    void writePitchToChannel(size_t channel, int midiNote)
    {
        if (channelIsPairedLowByte(channel))
        {
            const auto divisor = audf16ForNote(midiNote);
            audf[channel] = static_cast<uint8_t>(divisor & 0xffu);
            audf[channel + 1u] = static_cast<uint8_t>((divisor >> 8u) & 0xffu);
            return;
        }

        audf[channel] = pokeyAudfForNote(clock, midiNote);
    }

    void clockPoly4(size_t channel)
    {
        const auto feedback = ((poly4 & 1u) ^ ((poly4 >> 1u) & 1u)) & 1u;
        poly4 = static_cast<uint8_t>(((poly4 >> 1u) | (feedback << 3u)) & 0x0fu);
        if (poly4 == 0)
            poly4 = 0x0fu;
        lastOutput[channel] = (poly4 & 1u) != 0 ? 1.0 : -1.0;
    }

    void clockPoly5(size_t channel)
    {
        const auto feedback = ((poly5 & 1u) ^ ((poly5 >> 2u) & 1u)) & 1u;
        poly5 = static_cast<uint8_t>(((poly5 >> 1u) | (feedback << 4u)) & 0x1fu);
        if (poly5 == 0)
            poly5 = 0x1fu;
        lastOutput[channel] = (poly5 & 1u) != 0 ? 1.0 : -1.0;
    }

    void clockPoly17(size_t channel)
    {
        const auto feedback = ((poly17 & 1u) ^ ((poly17 >> 5u) & 1u)) & 1u;
        poly17 = ((poly17 >> 1u) | (feedback << 16u)) & 0x1ffffu;
        if (poly17 == 0)
            poly17 = 0x1ffffu;
        lastOutput[channel] = (poly17 & 1u) != 0 ? 1.0 : -1.0;
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

            if (sourceEnabled(patch, channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelNotes.size(); ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

            if (sourceEnabled(patch, channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelStamp.size(); ++channel)
        {
            if (channelIsPairedHighByte(channel))
                continue;

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
            if (channelIsPairedHighByte(channel))
                continue;

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
        audc.fill(0x00);
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
            return;

        const auto index = static_cast<size_t>(channel);
        audctl = pokeyAudctlForPatch(patch);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;
        writePitchToChannel(index, channelNotes[index]);
        const auto baseControl = pokeyAudcForPatch(patch) & 0xf0u;
        const auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
        audc[index] = static_cast<uint8_t>(baseControl | volume);
        if (channelIsPairedLowByte(index))
            audc[index + 1u] = 0x00u;
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
            audc[channel] = 0x00;
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 1789790.0;
    std::array<uint8_t, 4> audf {};
    std::array<uint8_t, 4> audc {};
    uint8_t audctl = 0x00;
    std::array<double, 4> phase {};
    std::array<double, 4> lastOutput { 1.0, 1.0, 1.0, 1.0 };
    std::array<double, 2> highPassHold {};
    std::array<double, 2> highPassClockLast { 1.0, 1.0 };
    uint8_t poly4 = 0x0f;
    uint8_t poly5 = 0x1f;
    uint32_t poly17 = 0x1ffff;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 4> channelNotes {};
    std::array<float, 4> channelVelocity {};
    std::array<uint64_t, 4> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makePokeyCore(AccuracyMode accuracy)
{
    return std::make_unique<PokeyCore>(accuracy);
}
}
