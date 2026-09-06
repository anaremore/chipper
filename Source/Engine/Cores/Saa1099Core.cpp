#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Saa1099Core final : public ChipCore
{
public:
    explicit Saa1099Core(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 8000000.0;
        regs.fill(0);
        frequency.fill(0);
        octave.fill(0);
        tonePhase.fill(0.0);
        noisePhase.fill(0.0);
        noiseLfsr = { 0x1ffffu, 0x15555u };
        noiseOutput = { 1.0, -1.0 };
        envelopePhase.fill(0.0);
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        heldNote = -1;
        noteVelocity = 0.0f;
        noteStamp = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        const auto shouldClearPoly = newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled;
        patch = newPatch;

        if (shouldClearPoly)
            clearChipPolyState();
        else if (patch.playMode != PlayMode::chipPoly && heldNote >= 0)
            configureStackNote(heldNote, noteVelocity);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto index = static_cast<uint8_t>(address & 0x1fu);
        regs[index] = value;

        if (index < channelCount)
            return;

        if (index >= 0x08u && index <= 0x0du)
        {
            frequency[index - 0x08u] = value;
            return;
        }

        if (index >= 0x10u && index <= 0x12u)
        {
            const auto channel = static_cast<size_t>((index - 0x10u) * 2u);
            octave[channel] = value & 0x07u;
            if (channel + 1u < octave.size())
                octave[channel + 1u] = (value >> 4u) & 0x07u;
            return;
        }

        if (index == 0x16u)
        {
            noiseLfsr = { 0x1ffffu, 0x15555u };
            noisePhase.fill(0.0);
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        configureStackNote(std::clamp(midiNote, 0, 127), static_cast<float>(clamp01(velocity)));
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
            channelNotes.fill(-1);
            channelVelocity.fill(0.0f);
            clearAmplitudesAndMasks();
        }
    }

    StereoFrame renderSample() override
    {
        if ((regs[0x1c] & 0x01u) == 0)
            return {};

        advanceEnvelopeGenerators();
        advanceNoiseGenerator(0);
        advanceNoiseGenerator(1);

        double left = 0.0;
        double right = 0.0;
        const auto toneMask = regs[0x14] & sourceMask();
        const auto noiseMask = regs[0x15] & sourceMask();

        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            const auto bit = static_cast<uint8_t>(1u << channel);
            const auto toneEnabled = (toneMask & bit) != 0;
            const auto noiseEnabled = (noiseMask & bit) != 0;
            if (! toneEnabled && ! noiseEnabled)
                continue;

            const auto amp = regs[channel];
            const auto leftAmp = static_cast<double>(amp & 0x0fu) / 15.0;
            const auto rightAmp = static_cast<double>((amp >> 4u) & 0x0fu) / 15.0;
            if (leftAmp <= 0.0 && rightAmp <= 0.0)
                continue;

            auto sample = 0.0;
            auto parts = 0;
            if (toneEnabled)
            {
                sample += advanceTone(channel);
                ++parts;
            }
            if (noiseEnabled)
            {
                sample += noiseOutput[noiseIndexForChannel(channel)];
                ++parts;
            }

            if (parts > 1)
                sample /= static_cast<double>(parts);

            const auto env = envelopeMultiplierForChannel(channel);
            left += sample * leftAmp * env;
            right += sample * rightAmp * env;
        }

        constexpr double outputScale = 0.72 / static_cast<double>(channelCount);
        return { static_cast<float>(left * outputScale), static_cast<float>(right * outputScale) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(22);
        for (uint8_t channel = 0; channel < channelCount; ++channel)
            writes.push_back({ 0, channel, regs[channel] });
        for (uint8_t channel = 0; channel < channelCount; ++channel)
            writes.push_back({ 0, static_cast<uint16_t>(0x08u + channel), frequency[channel] });
        for (uint8_t index = 0x10; index <= 0x12; ++index)
            writes.push_back({ 0, index, regs[index] });
        writes.push_back({ 0, 0x14, regs[0x14] });
        writes.push_back({ 0, 0x15, regs[0x15] });
        writes.push_back({ 0, 0x16, regs[0x16] });
        writes.push_back({ 0, 0x18, regs[0x18] });
        writes.push_back({ 0, 0x19, regs[0x19] });
        writes.push_back({ 0, 0x1c, regs[0x1c] });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::saa1099; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "Philips SAA1099"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Six square-tone channels, two shared noise generators, frequency/octave writes, stereo amplitude nibbles, mixer masks, simplified envelope-shape timing, source gating, source levels, and Chip Poly allocation are modeled; exact envelope edge timing, amplitude DAC curve, external noise clocking, bus timing, and hardware or golden-emulator validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"Philips SAA1099\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":6,"
             << "\"mixerToneMask\":" << static_cast<int>(regs[0x14]) << ","
             << "\"mixerNoiseMask\":" << static_cast<int>(regs[0x15]) << ","
             << "\"noiseControl0\":" << static_cast<int>(regs[0x16] & 0x03u) << ","
             << "\"noiseControl1\":" << static_cast<int>((regs[0x16] >> 4u) & 0x03u) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"envelopeControl0\":" << static_cast<int>(regs[0x18]) << ","
             << "\"envelopeControl1\":" << static_cast<int>(regs[0x19]) << ","
             << "\"envelopeShapeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"activeChannels\":" << activeChannels() << ",";

        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            json << "\"frequencyRegister" << channel << "\":" << static_cast<int>(frequency[channel]) << ","
                 << "\"octave" << channel << "\":" << static_cast<int>(octave[channel]) << ","
                 << "\"frequencyHz" << channel << "\":" << frequencyHz(channel) << ","
                 << "\"amplitudeLeft" << channel << "\":" << static_cast<int>(regs[channel] & 0x0fu) << ","
                 << "\"amplitudeRight" << channel << "\":" << static_cast<int>((regs[channel] >> 4u) & 0x0fu) << ","
                 << "\"sourceEnabled" << channel << "\":" << (sourceEnabled(patch, channel) ? 1 : 0) << ","
                 << "\"sourceLevel" << channel << "\":" << sourceLevel(patch, channel) << ","
                 << "\"assignedNote" << channel << "\":" << channelNotes[channel] << ",";
        }

        json << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    struct EncodedPitch
    {
        uint8_t frequency = 0;
        uint8_t octave = 0;
    };

    static constexpr size_t channelCount = 6;
    static constexpr size_t noiseCount = 2;

    uint8_t sourceMask() const
    {
        uint8_t mask = 0;
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            if (sourceEnabled(patch, channel))
                mask |= static_cast<uint8_t>(1u << channel);
        }
        return mask;
    }

    uint8_t toneMaskForPatch() const
    {
        switch (patch.macro)
        {
            case MacroKind::drum: return 0x09u;
            case MacroKind::hit: return 0x15u;
            default: return 0x3fu;
        }
    }

    uint8_t noiseMaskForPatch() const
    {
        switch (std::clamp(patch.snNoiseMode, 0, 4))
        {
            case 1: return 0x00u;
            case 2: return 0x24u;
            case 3: return 0x2du;
            case 4: return 0x3fu;
            default: break;
        }

        switch (patch.macro)
        {
            case MacroKind::drum: return 0x3fu;
            case MacroKind::hit: return 0x2du;
            case MacroKind::laser: return 0x21u;
            case MacroKind::manual: return patch.control3 > 0.62f ? 0x09u : 0x00u;
            case MacroKind::lead: return patch.control3 > 0.86f ? 0x20u : 0x00u;
            default: return 0x00u;
        }
    }

    uint8_t noiseControlForPatch() const
    {
        auto mode = 0;
        switch (std::clamp(patch.snNoiseMode, 0, 4))
        {
            case 2: mode = 0; break;
            case 3: mode = 1; break;
            case 4: mode = 2; break;
            default:
                mode = std::clamp(static_cast<int>(std::round(clamp01(patch.control3) * 2.0)), 0, 2);
                break;
        }

        return static_cast<uint8_t>((mode & 0x03) | ((mode & 0x03) << 4u));
    }

    uint8_t envelopeShapeCodeForPatch(size_t generator) const
    {
        auto choice = std::clamp(patch.ymEnvelopeShape, 0, 4);
        if (choice == 0)
        {
            if (patch.macro == MacroKind::coin || patch.macro == MacroKind::jump)
                choice = 1;
            else if (patch.macro == MacroKind::powerUp)
                choice = 2;
            else if (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit)
                choice = 4;
        }

        if (choice == 0 && patch.envelopeDecay <= 0.01f)
            return 0x00u;

        switch (choice)
        {
            case 1: return 0x81u; // fall
            case 2: return 0x82u; // rise
            case 3: return 0x88u; // saw
            case 4: return static_cast<uint8_t>(generator == 0u ? 0x8eu : 0x8au); // triangle / alternate
            default: return 0x81u;
        }
    }

    void configureStackNote(int midiNote, float velocity)
    {
        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto spread = static_cast<int>(std::round(clamp01(patch.control1) * 19.0));
        std::array<int, channelCount> notes {
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
                notes = { heldNote - 24, heldNote - 12, heldNote - 5, heldNote, heldNote + 7, heldNote + 12 };
                break;
            case MacroKind::laser:
                notes = { heldNote + 24, heldNote + 12, heldNote, heldNote - 12, heldNote - 19, heldNote - 24 };
                break;
            case MacroKind::jump:
                notes = { heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24, heldNote + 31, heldNote + 36 };
                break;
            case MacroKind::powerUp:
                notes = { heldNote, heldNote + 5, heldNote + 7, heldNote + 12, heldNote + 17, heldNote + 24 };
                break;
            case MacroKind::lead:
            case MacroKind::hit:
            case MacroKind::manual:
            default:
                break;
        }

        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            channelNotes[channel] = sourceEnabled(patch, channel) ? std::clamp(notes[channel], 0, 127) : -1;
            channelVelocity[channel] = sourceEnabled(patch, channel) ? noteVelocity : 0.0f;
            writePitchToChannel(channel, notes[channel]);
            regs[channel] = amplitudeRegisterForChannel(channel, noteVelocity);
        }

        regs[0x14] = toneMaskForPatch() & sourceMask();
        regs[0x15] = noiseMaskForPatch() & sourceMask();
        regs[0x16] = noiseControlForPatch();
        regs[0x18] = envelopeShapeCodeForPatch(0);
        regs[0x19] = envelopeShapeCodeForPatch(1);
        regs[0x1c] = 0x01u;
        envelopePhase.fill(0.0);
    }

    void clearAmplitudesAndMasks()
    {
        for (size_t channel = 0; channel < channelCount; ++channel)
            regs[channel] = 0x00u;
        regs[0x14] = 0x00u;
        regs[0x15] = 0x00u;
    }

    uint8_t amplitudeRegisterForChannel(size_t channel, float velocity) const
    {
        if (! sourceEnabled(patch, channel))
            return 0x00u;

        static constexpr std::array<double, channelCount> panPositions { -1.0, -0.6, -0.2, 0.2, 0.6, 1.0 };
        const auto level = clamp01(patch.control4) * clamp01(velocity) * sourceLevel(patch, channel);
        const auto base = static_cast<int>(std::round(level * 15.0));
        const auto pan = std::clamp(panPositions[channel] * clamp01(patch.stereoSpread), -1.0, 1.0);
        const auto leftScale = pan > 0.0 ? 1.0 - (0.65 * pan) : 1.0;
        const auto rightScale = pan < 0.0 ? 1.0 + (0.65 * pan) : 1.0;
        const auto left = std::clamp(static_cast<int>(std::round(static_cast<double>(base) * leftScale)), 0, 15);
        const auto right = std::clamp(static_cast<int>(std::round(static_cast<double>(base) * rightScale)), 0, 15);
        return static_cast<uint8_t>((right << 4u) | left);
    }

    EncodedPitch encodePitch(double hz) const
    {
        EncodedPitch best {};
        auto bestError = std::numeric_limits<double>::max();
        for (uint8_t oct = 0; oct < 8; ++oct)
        {
            const auto base = ((2.0 * clock) / 512.0) * static_cast<double>(1u << oct);
            const auto raw = 511.0 - (base / std::max(1.0, hz));
            const auto reg = std::clamp(static_cast<int>(std::round(raw)), 0, 255);
            const auto rendered = base / std::max(1.0, 511.0 - static_cast<double>(reg));
            const auto error = std::abs(rendered - hz);
            if (error < bestError)
            {
                bestError = error;
                best.frequency = static_cast<uint8_t>(reg);
                best.octave = oct;
            }
        }

        return best;
    }

    void writePitchToChannel(size_t channel, int midiNote)
    {
        if (channel >= channelCount)
            return;

        const auto encoded = encodePitch(midiNoteToHz(std::clamp(midiNote, 0, 127)));
        frequency[channel] = encoded.frequency;
        octave[channel] = encoded.octave;
        regs[0x08u + channel] = encoded.frequency;
        syncOctaveRegisterForChannel(channel);
    }

    void syncOctaveRegisterForChannel(size_t channel)
    {
        const auto pair = channel / 2u;
        const auto left = octave[pair * 2u] & 0x07u;
        const auto right = (pair * 2u + 1u) < octave.size() ? ((octave[pair * 2u + 1u] & 0x07u) << 4u) : 0u;
        regs[0x10u + pair] = static_cast<uint8_t>(left | right);
    }

    double frequencyHz(size_t channel) const
    {
        const auto oct = std::min<size_t>(octave[channel], 7u);
        const auto base = ((2.0 * clock) / 512.0) * static_cast<double>(1u << oct);
        const auto denom = std::max(1.0, 511.0 - static_cast<double>(frequency[channel]));
        return base / denom;
    }

    double advanceTone(size_t channel)
    {
        const auto hz = std::min(frequencyHz(channel), sampleRate * 0.45);
        tonePhase[channel] = wrapPhase(tonePhase[channel] + hz / sampleRate);
        return tonePhase[channel] < 0.5 ? 1.0 : -1.0;
    }

    size_t noiseIndexForChannel(size_t channel) const
    {
        return channel < 3u ? 0u : 1u;
    }

    double noiseClockHz(size_t generator) const
    {
        const auto shift = generator == 0u ? 0u : 4u;
        const auto mode = (regs[0x16] >> shift) & 0x03u;
        switch (mode)
        {
            case 0: return clock / 1024.0;
            case 1: return clock / 2048.0;
            case 2: return clock / 4096.0;
            case 3:
            default:
            {
                const auto anchor = generator == 0u ? 0u : 3u;
                return std::min(sampleRate * 0.45, frequencyHz(anchor) * 2.0);
            }
        }
    }

    void advanceNoiseGenerator(size_t generator)
    {
        if (generator >= noiseCount)
            return;

        noisePhase[generator] += noiseClockHz(generator) / sampleRate;
        while (noisePhase[generator] >= 1.0)
        {
            noisePhase[generator] -= 1.0;
            const auto feedback = ((noiseLfsr[generator] & 1u) ^ ((noiseLfsr[generator] >> 5u) & 1u)) & 1u;
            noiseLfsr[generator] = ((noiseLfsr[generator] >> 1u) | (feedback << 16u)) & 0x1ffffu;
            if (noiseLfsr[generator] == 0)
                noiseLfsr[generator] = generator == 0u ? 0x1ffffu : 0x15555u;
            noiseOutput[generator] = (noiseLfsr[generator] & 1u) != 0 ? 1.0 : -1.0;
        }
    }

    void advanceEnvelopeGenerators()
    {
        const auto rate = 1.5 + (clamp01(patch.envelopeDecay) * 18.0);
        for (size_t generator = 0; generator < envelopePhase.size(); ++generator)
        {
            if ((regs[0x18u + generator] & 0x8fu) == 0u && patch.envelopeDecay <= 0.01f)
                continue;

            envelopePhase[generator] = wrapPhase(envelopePhase[generator] + rate / sampleRate);
        }
    }

    double envelopeMultiplierForChannel(size_t channel) const
    {
        const auto generator = noiseIndexForChannel(channel);
        const auto shape = regs[0x18u + generator] & 0x0fu;
        if (shape == 0u && patch.envelopeDecay <= 0.01f)
            return 1.0;

        const auto phase = envelopePhase[generator];
        switch (shape)
        {
            case 0x02: return std::clamp(phase, 0.05, 1.0);
            case 0x08: return std::clamp(1.0 - phase, 0.05, 1.0);
            case 0x0a:
            case 0x0e: return std::clamp(phase < 0.5 ? phase * 2.0 : 2.0 - (phase * 2.0), 0.05, 1.0);
            case 0x01:
            default: return std::clamp(1.0 - phase, 0.05, 1.0);
        }
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        auto oldestChannel = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            if (sourceEnabled(patch, channel) && channelStamp[channel] < oldestStamp)
            {
                oldestStamp = channelStamp[channel];
                oldestChannel = static_cast<int>(channel);
            }
        }

        return oldestChannel;
    }

    int activeChannels() const
    {
        int active = 0;
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            if (sourceEnabled(patch, channel) && channelNotes[channel] >= 0 && regs[channel] != 0)
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
        clearAmplitudesAndMasks();
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto selectedChannel = selectChipPolyChannel(midiNote);
        if (selectedChannel < 0)
            return;

        const auto channel = static_cast<size_t>(selectedChannel);
        channelNotes[channel] = std::clamp(midiNote, 0, 127);
        channelVelocity[channel] = static_cast<float>(clamp01(velocity));
        channelStamp[channel] = ++noteStamp;
        writePitchToChannel(channel, channelNotes[channel]);
        regs[channel] = amplitudeRegisterForChannel(channel, channelVelocity[channel]);
        regs[0x14] = (regs[0x14] | static_cast<uint8_t>(1u << channel)) & sourceMask();
        regs[0x15] &= static_cast<uint8_t>(~(1u << channel));
        regs[0x16] = noiseControlForPatch();
        regs[0x18] = envelopeShapeCodeForPatch(0);
        regs[0x19] = envelopeShapeCodeForPatch(1);
        regs[0x1c] = 0x01u;
        noteVelocity = activeChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t channel = 0; channel < channelCount; ++channel)
        {
            if (channelNotes[channel] != midiNote)
                continue;

            channelNotes[channel] = -1;
            channelVelocity[channel] = 0.0f;
            channelStamp[channel] = 0;
            regs[channel] = 0x00u;
            regs[0x14] &= static_cast<uint8_t>(~(1u << channel));
            regs[0x15] &= static_cast<uint8_t>(~(1u << channel));
        }

        noteVelocity = activeChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 8000000.0;
    std::array<uint8_t, 0x20> regs {};
    std::array<uint8_t, channelCount> frequency {};
    std::array<uint8_t, channelCount> octave {};
    std::array<double, channelCount> tonePhase {};
    std::array<double, noiseCount> noisePhase {};
    std::array<uint32_t, noiseCount> noiseLfsr { 0x1ffffu, 0x15555u };
    std::array<double, noiseCount> noiseOutput { 1.0, -1.0 };
    std::array<double, noiseCount> envelopePhase {};
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, channelCount> channelNotes {};
    std::array<float, channelCount> channelVelocity {};
    std::array<uint64_t, channelCount> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeSaa1099Core(AccuracyMode accuracy)
{
    return std::make_unique<Saa1099Core>(accuracy);
}
}
