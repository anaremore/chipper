#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class PcSpeakerCore final : public ChipCore
{
public:
    explicit PcSpeakerCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 1193182.0;
        phase = 0.0;
        clickPhase = 0.0;
        lfsr = 0xace1u;
        pitDivisor = 1;
        controlWord = 0xb6u;
        port61 = 0x00u;
        pendingLowByte = 0;
        waitingForHighByte = false;
        heldNote = -1;
        noteVelocity = 0.0f;
        gateActive = false;
        sampleAge = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        patch = newPatch;
        if (heldNote >= 0 && sourceEnabled(patch, 0))
            configureNote(heldNote, noteVelocity);
        else if (! sourceEnabled(patch, 0))
            gateActive = false;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        switch (address & 0xffu)
        {
            case 0x43:
                controlWord = value;
                waitingForHighByte = false;
                break;

            case 0x42:
                if (! waitingForHighByte)
                {
                    pendingLowByte = value;
                    waitingForHighByte = true;
                }
                else
                {
                    const auto raw = static_cast<uint16_t>(pendingLowByte | (static_cast<uint16_t>(value) << 8u));
                    pitDivisor = raw == 0u ? 65536u : static_cast<uint32_t>(raw);
                    waitingForHighByte = false;
                }
                break;

            case 0x61:
                port61 = value;
                gateActive = sourceEnabled(patch, 0) && (port61 & 0x02u) != 0u;
                break;

            default:
                break;
        }
    }

    void noteOn(int midiNote, float velocity) override
    {
        configureNote(std::clamp(midiNote, 0, 127), static_cast<float>(clamp01(velocity)));
    }

    void noteOff(int midiNote) override
    {
        if (midiNote == heldNote)
        {
            heldNote = -1;
            noteVelocity = 0.0f;
            gateActive = false;
            port61 = 0x00u;
        }
    }

    StereoFrame renderSample() override
    {
        if (! sourceEnabled(patch, 0) || ! gateActive || noteVelocity <= 0.0f)
            return {};

        const auto mode = speakerModeForPatch();
        const auto gateBit = (port61 & 0x01u) != 0u;
        const auto dataBit = (port61 & 0x02u) != 0u;
        const auto toneEnabled = gateBit && (mode == 1 || mode == 3 || mode == 4);
        const auto clickEnabled = dataBit && (mode == 2 || mode == 3 || mode == 4);
        if (! toneEnabled && ! clickEnabled)
            return {};

        auto sample = 0.0;
        auto parts = 0;

        if (toneEnabled)
        {
            const auto hz = std::min(renderFrequencyHz(), sampleRate * 0.45);
            phase = wrapPhase(phase + hz / sampleRate);
            sample += phase < dutyCycleForPatch() ? 1.0 : -1.0;
            ++parts;
        }

        if (clickEnabled)
        {
            const auto edgeRate = mode == 4
                ? 70.0 + (clamp01(patch.control2) * 520.0)
                : 700.0 + (clamp01(patch.control3) * 9200.0);
            clickPhase += edgeRate / sampleRate;
            while (clickPhase >= 1.0)
            {
                clickPhase -= 1.0;
                advanceClickLfsr();
            }

            const auto click = (lfsr & 1u) != 0u ? 1.0 : -1.0;
            sample += click * (0.35 + (clamp01(patch.control3) * 0.65));
            ++parts;
        }

        if (parts > 1)
            sample /= static_cast<double>(parts);

        const auto env = gateEnvelope();
        const auto level = clamp01(patch.control4) * sourceLevel(patch, 0) * clamp01(noteVelocity);
        const auto color = 0.82 + (0.18 * clamp01(patch.control1));
        const auto out = static_cast<float>(sample * env * level * color * 0.68);
        ++sampleAge;
        return { out, out };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        const auto divisor = static_cast<uint16_t>(std::min<uint32_t>(pitDivisor, 65535u));
        return {
            { 0, 0x43, controlWord },
            { 0, 0x42, static_cast<uint8_t>(divisor & 0xffu) },
            { 0, 0x42, static_cast<uint8_t>((divisor >> 8u) & 0xffu) },
            { 0, 0x61, port61 }
        };
    }

    ChipMode mode() const override { return ChipMode::pcSpeaker; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "PC Speaker"; }
    std::string implementedAccuracy() const override { return "partial clean-room PIT beeper"; }
    std::string limitations() const override
    {
        return "PIT channel 2 divisor, port 0x61 gate/data state, one-bit tone output, click/grit SFX, source enable/level, and gesture presets are modeled; exact 8253/8254 edge timing, motherboard analog filtering, DMA tricks, and hardware capture validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"PC Speaker\","
             << "\"implementedAccuracy\":\"partial clean-room PIT beeper\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":1,"
             << "\"speakerMode\":" << speakerModeForPatch() << ","
             << "\"pitDivisor\":" << pitDivisor << ","
             << "\"pitFrequencyHz\":" << pitFrequencyHz() << ","
             << "\"renderFrequencyHz\":" << renderFrequencyHz() << ","
             << "\"controlWord\":" << static_cast<int>(controlWord) << ","
             << "\"port61\":" << static_cast<int>(port61) << ","
             << "\"speakerGate\":" << (((port61 & 0x01u) != 0u && gateActive) ? 1 : 0) << ","
             << "\"speakerData\":" << (((port61 & 0x02u) != 0u && gateActive) ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChannels() << ","
             << "\"dutyCycle\":" << dutyCycleForPatch() << ","
             << "\"clickGrit\":" << clamp01(patch.control3) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"assignedNote0\":" << heldNote << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    int speakerModeForPatch() const
    {
        const auto choice = std::clamp(patch.waveShape, 0, 4);
        if (choice > 0)
            return choice;

        switch (patch.macro)
        {
            case MacroKind::drum:
            case MacroKind::hit:
                return 2;
            case MacroKind::laser:
                return 3;
            case MacroKind::jump:
            case MacroKind::powerUp:
                return 4;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::arp:
            default:
                return 1;
        }
    }

    void configureNote(int midiNote, float velocity)
    {
        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        const auto hz = targetFrequencyHz();
        pitDivisor = divisorForFrequency(hz);
        controlWord = 0xb6u;
        port61 = 0x03u;
        gateActive = sourceEnabled(patch, 0) && noteVelocity > 0.0f;
        phase = 0.0;
        clickPhase = 0.0;
        sampleAge = 0;
    }

    uint32_t divisorForFrequency(double hz) const
    {
        const auto divisor = std::round(clock / std::max(1.0, hz));
        return static_cast<uint32_t>(std::clamp(divisor, 1.0, 65536.0));
    }

    double targetFrequencyHz() const
    {
        auto note = static_cast<double>(heldNote >= 0 ? heldNote : 69);
        switch (patch.macro)
        {
            case MacroKind::coin:
                note += 24.0 + (clamp01(patch.control2) * 12.0);
                break;
            case MacroKind::bass:
                note -= 24.0;
                break;
            case MacroKind::arp:
                note += std::round(clamp01(patch.control1) * 12.0);
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                note -= 12.0;
                break;
            case MacroKind::laser:
                note += 12.0 + (clamp01(patch.control2) * 12.0);
                break;
            case MacroKind::jump:
            case MacroKind::powerUp:
                note += clamp01(patch.control2) * 7.0;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }
        return midiNoteToHz(std::clamp(static_cast<int>(std::round(note)), 0, 127));
    }

    double pitFrequencyHz() const
    {
        return clock / static_cast<double>(std::max<uint32_t>(1u, pitDivisor));
    }

    double renderFrequencyHz() const
    {
        auto hz = pitFrequencyHz();
        const auto ageSeconds = static_cast<double>(sampleAge) / std::max(1.0, sampleRate);
        switch (patch.macro)
        {
            case MacroKind::coin:
                hz *= 1.0 + std::max(0.0, 1.0 - (ageSeconds * 14.0)) * 0.35;
                break;
            case MacroKind::laser:
                hz *= 0.55 + std::max(0.0, 1.0 - (ageSeconds * (3.5 + clamp01(patch.control2) * 9.0))) * 1.6;
                break;
            case MacroKind::jump:
            case MacroKind::powerUp:
                hz *= 0.7 + std::min(1.0, ageSeconds * (4.0 + clamp01(patch.control2) * 10.0)) * 0.8;
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                hz *= 1.0 + std::max(0.0, 1.0 - (ageSeconds * 22.0)) * 0.5;
                break;
            case MacroKind::manual:
            case MacroKind::bass:
            case MacroKind::lead:
            case MacroKind::arp:
            default:
                break;
        }
        return std::max(1.0, hz);
    }

    double dutyCycleForPatch() const
    {
        return std::clamp(0.5 + ((clamp01(patch.control1) - 0.5) * 0.5), 0.25, 0.75);
    }

    double gateEnvelope() const
    {
        const auto ageSeconds = static_cast<double>(sampleAge) / std::max(1.0, sampleRate);
        auto decay = clamp01(patch.envelopeDecay);
        if (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit || patch.macro == MacroKind::coin)
            decay = std::max(decay, 0.6);
        if (patch.macro == MacroKind::laser || patch.macro == MacroKind::jump || patch.macro == MacroKind::powerUp)
            decay = std::max(decay, 0.25);
        if (decay <= 0.01)
            return 1.0;
        return std::exp(-ageSeconds * (2.0 + (decay * 32.0)));
    }

    void advanceClickLfsr()
    {
        const auto feedback = ((lfsr & 1u) ^ ((lfsr >> 2u) & 1u) ^ ((lfsr >> 3u) & 1u) ^ ((lfsr >> 5u) & 1u)) & 1u;
        lfsr = (lfsr >> 1u) | (feedback << 15u);
        if (lfsr == 0u)
            lfsr = 0xace1u;
    }

    int activeChannels() const
    {
        return sourceEnabled(patch, 0) && gateActive && heldNote >= 0 ? 1 : 0;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 1193182.0;
    double phase = 0.0;
    double clickPhase = 0.0;
    uint32_t lfsr = 0xace1u;
    uint32_t pitDivisor = 1;
    uint8_t controlWord = 0xb6u;
    uint8_t port61 = 0x00u;
    uint8_t pendingLowByte = 0;
    bool waitingForHighByte = false;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    bool gateActive = false;
    uint64_t sampleAge = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makePcSpeakerCore(AccuracyMode accuracy)
{
    return std::make_unique<PcSpeakerCore>(accuracy);
}
}
