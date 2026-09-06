#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class PaulaCore final : public ChipCore
{
public:
    explicit PaulaCore(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 3546895.0;
        period.fill(428);
        volume.fill(0);
        control.fill(0);
        position.fill(0.0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        sampleTemplate.fill(0);
        leftA500State = 0.0;
        rightA500State = 0.0;
        leftLedState = 0.0;
        rightLedState = 0.0;
        for (size_t channel = 0; channel < sampleRam.size(); ++channel)
            seedSample(channel);
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        const auto sampleSlotsChanged = nextPatch.spc700VoiceSampleSlots != patch.spc700VoiceSampleSlots;
        patch = nextPatch;
        for (size_t channel = 0; channel < sampleRam.size(); ++channel)
        {
            const auto nextTemplate = resolvedSampleTemplate(channel);
            if (nextTemplate != sampleTemplate[channel] || sampleSlotsChanged)
            {
                sampleTemplate[channel] = nextTemplate;
                seedSample(channel);
            }
        }
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        std::vector<std::vector<uint8_t>> bank;
        if (! data.empty())
            bank.push_back(std::move(data));
        const auto selectedSlot = bank.empty() ? -1 : 0;
        setExternalSampleBank(std::move(bank), selectedSlot);
    }

    void setExternalSampleBank(std::vector<std::vector<uint8_t>> bank, int selectedSlot) override
    {
        std::vector<ExternalSampleData> typedBank;
        typedBank.reserve(bank.size());
        for (auto& bytes : bank)
            typedBank.push_back({ std::move(bytes), ExternalSampleEncoding::signedPcm8 });

        setExternalSampleBank(std::move(typedBank), selectedSlot);
    }

    void setExternalSampleBank(std::vector<ExternalSampleData> bank, int selectedSlot) override
    {
        externalSampleBank.clear();
        externalSampleBank.reserve(bank.size());
        for (const auto& source : bank)
        {
            ExternalPaulaSample sample;
            sample.data.reserve(source.bytes.size());
            for (const auto byte : source.bytes)
                sample.data.push_back((static_cast<double>(byte) - 128.0) / 128.0);

            const auto sampleLength = sample.data.size();
            if (source.hasLoop && source.loopStart + 1u < source.loopEnd && source.loopEnd <= sampleLength)
            {
                sample.hasLoop = true;
                sample.loopStart = source.loopStart;
                sample.loopEnd = source.loopEnd;
            }
            else
            {
                sample.loopStart = 0u;
                sample.loopEnd = sampleLength;
            }

            externalSampleBank.push_back(std::move(sample));
        }

        setExternalSampleSlot(selectedSlot);
    }

    void setExternalSampleSlot(int selectedSlot) override
    {
        if (externalSampleBank.empty())
        {
            externalSampleSlot = -1;
            for (size_t channel = 0; channel < sampleRam.size(); ++channel)
                seedSample(channel);
            return;
        }

        externalSampleSlot = selectedSlot >= 0 && selectedSlot < static_cast<int>(externalSampleBank.size()) ? selectedSlot : -1;
        for (size_t channel = 0; channel < sampleRam.size(); ++channel)
            seedSample(channel);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        const auto channel = static_cast<size_t>((reg >> 4u) & 3u);
        const auto offset = reg & 0x0fu;

        switch (offset)
        {
            case 0: period[channel] = static_cast<uint16_t>((period[channel] & 0xff00u) | value); break;
            case 1: period[channel] = static_cast<uint16_t>((period[channel] & 0x00ffu) | (static_cast<uint16_t>(value) << 8u)); break;
            case 2: volume[channel] = static_cast<uint8_t>(std::min<int>(64, value & 0x7fu)); break;
            case 3: control[channel] = value & 0x03u; break;
            case 4:
                sampleTemplate[channel] = value % 5u;
                seedSample(channel);
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
        const auto spread = static_cast<int>(std::round(patch.control1 * 12.0f));
        auto notes = std::array<int, 4> { heldNote, heldNote + std::max(1, spread / 3), heldNote + std::max(2, (spread * 2) / 3), heldNote + std::max(3, spread) };

        switch (patch.macro)
        {
            case MacroKind::bass: notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7 }; break;
            case MacroKind::lead: notes = { heldNote, heldNote + 7, heldNote + 12, heldNote + 19 }; break;
            case MacroKind::arp: notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12 }; break;
            case MacroKind::drum:
            case MacroKind::hit: notes = { heldNote - 12, heldNote - 5, heldNote, heldNote + 7 }; break;
            case MacroKind::coin: notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43 }; break;
            case MacroKind::laser:
                notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12 };
                break;
            case MacroKind::jump: notes = { heldNote + static_cast<int>(std::round(patch.control2 * 12.0f)), heldNote + 7, heldNote + 12, heldNote + 19 }; break;
            case MacroKind::powerUp: notes = { heldNote, heldNote + 5, heldNote + 12, heldNote + 17 }; break;
            case MacroKind::manual:
            default: break;
        }

        for (size_t channel = 0; channel < period.size(); ++channel)
            triggerChannel(channel, notes[channel], noteVelocity, channelActiveForPatch(channel));
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
            for (auto& ctrl : control)
                ctrl &= static_cast<uint8_t>(~0x01u);
        }
    }

    StereoFrame renderSample() override
    {
        static constexpr std::array<double, 4> panPositions { -1.0, 1.0, 1.0, -1.0 };
        double left = 0.0;
        double right = 0.0;
        auto audibleCount = 0;

        for (size_t channel = 0; channel < period.size(); ++channel)
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
            const auto decay = 1.0 - static_cast<double>(patch.envelopeDecay) * 0.00035;
            noteVelocity = static_cast<float>(std::max(0.0, static_cast<double>(noteVelocity) * decay));
        }

        applyOutputFilter(left, right);

        return { static_cast<float>(std::clamp(left, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t channel = 0; channel < period.size(); ++channel)
        {
            const auto base = static_cast<uint16_t>(channel * 0x10u);
            writes.push_back({ 0, base, static_cast<uint8_t>(period[channel] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>((period[channel] >> 8u) & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 2u), volume[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 3u), control[channel] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 4u), sampleTemplate[channel] });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::paula; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "Amiga Paula"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Four Paula-inspired 8-bit sample channels, period, volume, enable/loop control, classic hard-panned channel layout, generated tracker sample templates, plugin-loaded WAV/AIFF/uncompressed IFF-8SVX/ProTracker MOD sample banks, renderer-loaded WAV/uncompressed IFF-8SVX samples, and imported WAV smpl, AIFF MARK/INST, uncompressed 8SVX, and MOD sample loop metadata are modeled; DMA pointer timing, exact PAL/NTSC video timing, compressed 8SVX, full MOD playback/effects, CIA timing, analog output path, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        const auto hasSelectedExternalSample = externalSampleSlot >= 0
            && externalSampleSlot < static_cast<int>(externalSampleBank.size());
        const auto selectedExternalSampleIndex = hasSelectedExternalSample
            ? static_cast<size_t>(externalSampleSlot)
            : size_t { 0 };
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Amiga Paula\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << static_cast<int>(sampleTemplate[0]) << ","
             << "\"waveShapeChoice0\":" << static_cast<int>(sampleTemplate[0]) << ","
             << "\"waveShapeChoice1\":" << static_cast<int>(sampleTemplate[1]) << ","
             << "\"waveShapeChoice2\":" << static_cast<int>(sampleTemplate[2]) << ","
             << "\"waveShapeChoice3\":" << static_cast<int>(sampleTemplate[3]) << ","
             << "\"period0\":" << period[0] << ","
             << "\"period1\":" << period[1] << ","
             << "\"period2\":" << period[2] << ","
             << "\"period3\":" << period[3] << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"control0\":" << static_cast<int>(control[0]) << ","
             << "\"control1\":" << static_cast<int>(control[1]) << ","
             << "\"control2\":" << static_cast<int>(control[2]) << ","
             << "\"control3\":" << static_cast<int>(control[3]) << ","
             << "\"outputFilterMode\":" << static_cast<int>(paulaOutputFilterModeForPatch(patch)) << ","
             << "\"nativeInterpolation\":\"nearest\","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sampleLength0\":" << sampleRam[0].size() << ","
             << "\"sampleLoopStart0\":" << sampleLoopStarts[0] << ","
             << "\"sampleLoopEnd0\":" << sampleLoopEnds[0] << ","
             << "\"externalSampleLoaded\":" << (externalSampleBank.empty() ? 0 : 1) << ","
             << "\"externalSampleBankCount\":" << externalSampleBank.size() << ","
             << "\"externalSampleSlot\":" << externalSampleSlot << ","
             << "\"externalSampleLoopStartSelected\":" << (hasSelectedExternalSample ? externalSampleBank[selectedExternalSampleIndex].loopStart : 0u) << ","
             << "\"externalSampleLoopEndSelected\":" << (hasSelectedExternalSample ? externalSampleBank[selectedExternalSampleIndex].loopEnd : 0u) << ","
             << "\"externalSampleLoopMetadataSelected\":" << (hasSelectedExternalSample && externalSampleBank[selectedExternalSampleIndex].hasLoop ? 1 : 0) << ","
             << "\"voiceSampleSlot0\":" << patch.spc700VoiceSampleSlots[0] << ","
             << "\"voiceSampleSlot1\":" << patch.spc700VoiceSampleSlots[1] << ","
             << "\"voiceSampleSlot2\":" << patch.spc700VoiceSampleSlots[2] << ","
             << "\"voiceSampleSlot3\":" << patch.spc700VoiceSampleSlots[3] << ","
             << "\"resolvedExternalSampleSlot0\":" << resolvedExternalSampleSlot[0] << ","
             << "\"resolvedExternalSampleSlot1\":" << resolvedExternalSampleSlot[1] << ","
             << "\"resolvedExternalSampleSlot2\":" << resolvedExternalSampleSlot[2] << ","
             << "\"resolvedExternalSampleSlot3\":" << resolvedExternalSampleSlot[3] << ","
             << "\"internalChannelCount\":4,"
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
    uint16_t paulaPeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round(clock / (2.0 * hz)), 113.0, 65535.0));
    }

    uint8_t channelVolumeForPatch(size_t channel, float velocity) const
    {
        const auto trim = sourceLevel(patch, channel);
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 64.0f)), 1, 64);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(base) * trim * clamp01(velocity))), 0, 64));
    }

    bool channelActiveForPatch(size_t channel) const
    {
        return sourceEnabled(patch, channel);
    }

    bool loopForPatch() const
    {
        return paulaLoopForPatch(patch);
    }

    uint8_t resolvedSampleTemplate(size_t channel) const
    {
        return wavetableWaveShapeForChannel(ChipMode::paula, patch, channel);
    }

    void seedSample(size_t channel)
    {
        if (channel < resolvedExternalSampleSlot.size())
            resolvedExternalSampleSlot[channel] = -1;

        if (! externalSampleBank.empty())
        {
            auto selectedSlot = externalSampleSlot;
            if (channel < patch.spc700VoiceSampleSlots.size() && patch.spc700VoiceSampleSlots[channel] > 0)
                selectedSlot = patch.spc700VoiceSampleSlots[channel] - 1;

            if (selectedSlot >= 0 && selectedSlot < static_cast<int>(externalSampleBank.size()))
            {
                const auto& sample = externalSampleBank[static_cast<size_t>(selectedSlot)];
                sampleRam[channel] = sample.data;
                sampleLoopStarts[channel] = sample.hasLoop ? sample.loopStart : 0u;
                sampleLoopEnds[channel] = sample.loopEnd > sampleLoopStarts[channel]
                    ? sample.loopEnd
                    : sampleRam[channel].size();
                if (channel < resolvedExternalSampleSlot.size())
                    resolvedExternalSampleSlot[channel] = selectedSlot;
            }
            else
            {
                sampleRam[channel].clear();
                sampleLoopStarts[channel] = 0u;
                sampleLoopEnds[channel] = 0u;
            }
            return;
        }

        auto& data = sampleRam[channel];
        data.assign(64u, 0.0);
        const auto choice = sampleTemplate[channel] == 0 ? resolvedSampleTemplate(channel) : sampleTemplate[channel];
        for (size_t i = 0; i < data.size(); ++i)
        {
            const auto phaseValue = static_cast<double>(i) / static_cast<double>(data.size());
            double sample = 0.0;
            switch (choice)
            {
                case 1: sample = (phaseValue * 2.0) - 1.0; break;
                case 2: sample = phaseValue < 0.5 ? (phaseValue * 4.0) - 1.0 : 3.0 - (phaseValue * 4.0); break;
                case 3: sample = std::sin(twoPi * phaseValue); break;
                case 4:
                {
                    const auto hash = (static_cast<int>(i) * 1103515245u + static_cast<int>(channel) * 12345u) & 0xffffu;
                    const auto burst = std::exp(-phaseValue * 6.0);
                    sample = ((static_cast<double>(hash) / 32767.5) - 1.0) * burst;
                    break;
                }
                default: sample = phaseValue < 0.5 ? 0.85 : -0.85; break;
            }
            data[i] = std::clamp(std::round(sample * 127.0) / 127.0, -1.0, 1.0);
        }
        sampleLoopStarts[channel] = 0u;
        sampleLoopEnds[channel] = sampleRam[channel].size();
    }

    void triggerChannel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        period[channel] = paulaPeriodForNote(midiNote + static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f)));
        volume[channel] = channelVolumeForPatch(channel, velocity);
        control[channel] = static_cast<uint8_t>((shouldEnable && volume[channel] > 0 ? 0x01u : 0x00u) | (loopForPatch() ? 0x02u : 0x00u));
        position[channel] = 0.0;
        seedSample(channel);
    }

    double renderChannel(size_t channel)
    {
        if ((control[channel] & 0x01u) == 0 || sampleRam[channel].empty() || volume[channel] == 0)
            return 0.0;

        const auto loopEnabled = (control[channel] & 0x02u) != 0;
        const auto sampleLength = sampleRam[channel].size();
        const auto loopStart = std::min(sampleLoopStarts[channel], sampleLength - 1u);
        const auto loopEnd = std::clamp(sampleLoopEnds[channel], loopStart + 1u, sampleLength);
        auto wrapPositionIfNeeded = [&]()
        {
            const auto end = static_cast<double>(loopEnabled ? loopEnd : sampleLength);
            if (position[channel] < end)
                return true;

            if (loopEnabled)
            {
                const auto start = static_cast<double>(loopStart);
                const auto length = std::max(1.0, static_cast<double>(loopEnd - loopStart));
                position[channel] = start + std::fmod(std::max(0.0, position[channel] - start), length);
                return true;
            }

            control[channel] &= static_cast<uint8_t>(~0x01u);
            return false;
        };

        if (! wrapPositionIfNeeded())
            return 0.0;

        const auto index = std::min(static_cast<size_t>(position[channel]), sampleLength - 1u);
        const auto sample = sampleRam[channel][index] * (static_cast<double>(volume[channel]) / 64.0);
        const auto playbackHz = clock / (2.0 * static_cast<double>(std::max<uint16_t>(113, period[channel])));
        position[channel] += (playbackHz * static_cast<double>(sampleLength)) / sampleRate;

        (void) wrapPositionIfNeeded();

        return sample;
    }

    void applyOutputFilter(double& left, double& right)
    {
        const auto mode = paulaOutputFilterModeForPatch(patch);
        if (mode == 1u)
            return;

        if (mode == 2u || mode == 4u)
        {
            left = onePoleLowPass(left, leftA500State, 26000.0);
            right = onePoleLowPass(right, rightA500State, 26000.0);
        }

        if (mode == 3u || mode == 4u)
        {
            left = onePoleLowPass(left, leftLedState, 7000.0);
            right = onePoleLowPass(right, rightLedState, 7000.0);
        }
    }

    double onePoleLowPass(double input, double& state, double cutoffHz) const
    {
        const auto safeRate = std::max(1000.0, sampleRate);
        const auto safeCutoff = std::clamp(cutoffHz, 20.0, safeRate * 0.45);
        const auto alpha = 1.0 - std::exp((-twoPi * safeCutoff) / safeRate);
        state += alpha * (input - state);
        return state;
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
        for (auto& ctrl : control)
            ctrl &= static_cast<uint8_t>(~0x01u);
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
        triggerChannel(index, channelNotes[index], channelVelocity[index], true);
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
            control[channel] &= static_cast<uint8_t>(~0x01u);
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3546895.0;
    std::array<uint16_t, 4> period {};
    std::array<uint8_t, 4> volume {};
    std::array<uint8_t, 4> control {};
    std::array<uint8_t, 4> sampleTemplate {};
    struct ExternalPaulaSample
    {
        std::vector<double> data;
        bool hasLoop = false;
        size_t loopStart = 0;
        size_t loopEnd = 0;
    };

    std::array<std::vector<double>, 4> sampleRam {};
    std::array<size_t, 4> sampleLoopStarts {};
    std::array<size_t, 4> sampleLoopEnds {};
    std::vector<ExternalPaulaSample> externalSampleBank;
    int externalSampleSlot = -1;
    std::array<int, 4> resolvedExternalSampleSlot { -1, -1, -1, -1 };
    std::array<double, 4> position {};
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 4> channelNotes {};
    std::array<float, 4> channelVelocity {};
    std::array<uint64_t, 4> channelStamp {};
    uint64_t noteStamp = 0;
    double leftA500State = 0.0;
    double rightA500State = 0.0;
    double leftLedState = 0.0;
    double rightLedState = 0.0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makePaulaCore(AccuracyMode accuracy)
{
    return std::make_unique<PaulaCore>(accuracy);
}
}
