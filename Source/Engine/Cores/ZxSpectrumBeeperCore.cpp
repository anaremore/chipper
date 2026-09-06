#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class ZxSpectrumBeeperCore final : public ChipCore
{
public:
    explicit ZxSpectrumBeeperCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 3500000.0;
        phase = 0.0;
        clickPhase = 0.0;
        lfsr = 0x5a17u;
        portFe = 0x00u;
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
        if ((address & 0xffu) != 0xfeu && (address & 0x01u) != 0u)
            return;

        portFe = value;
        gateActive = sourceEnabled(patch, 0) && ((portFe & 0x18u) != 0u);
        if (gateActive && noteVelocity <= 0.0f)
            noteVelocity = 1.0f;
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
            portFe = 0x00u;
        }
    }

    StereoFrame renderSample() override
    {
        if (! sourceEnabled(patch, 0) || ! gateActive || noteVelocity <= 0.0f)
            return {};

        const auto mode = beeperModeForPatch();
        const auto earEnabled = (portFe & 0x10u) != 0u && (mode == 1 || mode == 3 || mode == 4);
        const auto micEnabled = (portFe & 0x08u) != 0u && (mode == 2 || mode == 3 || mode == 4);
        if (! earEnabled && ! micEnabled)
            return {};

        auto sample = 0.0;
        auto parts = 0;

        if (earEnabled)
        {
            const auto hz = std::min(renderFrequencyHz(), sampleRate * 0.45);
            phase = wrapPhase(phase + hz / sampleRate);
            const auto duty = dutyCycleForPatch();
            sample += phase < duty ? 1.0 : -1.0;
            ++parts;
        }

        if (micEnabled)
        {
            const auto edgeRate = mode == 4
                ? 180.0 + (clamp01(patch.control2) * 2200.0)
                : 950.0 + (clamp01(patch.control3) * 11800.0);
            clickPhase += edgeRate / sampleRate;
            while (clickPhase >= 1.0)
            {
                clickPhase -= 1.0;
                advanceClickLfsr();
            }

            const auto click = (lfsr & 1u) != 0u ? 1.0 : -1.0;
            sample += click * (0.25 + (clamp01(patch.control3) * 0.75));
            ++parts;
        }

        if (parts > 1)
            sample /= static_cast<double>(parts);

        const auto env = gateEnvelope();
        const auto level = clamp01(patch.control4) * sourceLevel(patch, 0) * clamp01(noteVelocity);
        const auto color = 0.80 + (0.20 * clamp01(patch.control1));
        const auto out = static_cast<float>(sample * env * level * color * 0.64);
        ++sampleAge;
        return { out, out };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        return {
            { 0, 0x00fe, portFe }
        };
    }

    ChipMode mode() const override { return ChipMode::zxSpectrumBeeper; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "ZX Spectrum Beeper"; }
    std::string implementedAccuracy() const override { return "partial clean-room ULA port FE beeper"; }
    std::string limitations() const override
    {
        return "ULA port 0xFE EAR/MIC output bits, border-bit metadata, one-bit beeper gestures, source enable/level, and helper gate decay are modeled; CPU cycle-exact delay loops, contention timing, tape I/O analog behavior, AY-128 hardware, and hardware capture validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"ZX Spectrum Beeper\","
             << "\"implementedAccuracy\":\"partial clean-room ULA port FE beeper\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"internalChannelCount\":1,"
             << "\"beeperMode\":" << beeperModeForPatch() << ","
             << "\"ulaPortFe\":" << static_cast<int>(portFe) << ","
             << "\"earBit\":" << (((portFe & 0x10u) != 0u && gateActive) ? 1 : 0) << ","
             << "\"micBit\":" << (((portFe & 0x08u) != 0u && gateActive) ? 1 : 0) << ","
             << "\"borderColor\":" << static_cast<int>(portFe & 0x07u) << ","
             << "\"renderFrequencyHz\":" << renderFrequencyHz() << ","
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
    int beeperModeForPatch() const
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
            case MacroKind::arp:
            case MacroKind::jump:
            case MacroKind::powerUp:
                return 4;
            case MacroKind::manual:
            case MacroKind::coin:
            case MacroKind::bass:
            case MacroKind::lead:
            default:
                return 1;
        }
    }

    void configureNote(int midiNote, float velocity)
    {
        heldNote = std::clamp(midiNote, 0, 127);
        noteVelocity = static_cast<float>(clamp01(velocity));
        portFe = portValueForMode();
        gateActive = sourceEnabled(patch, 0) && noteVelocity > 0.0f;
        phase = 0.0;
        clickPhase = 0.0;
        sampleAge = 0;
    }

    uint8_t portValueForMode() const
    {
        const auto border = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clamp01(patch.control1) * 7.0)), 0, 7));
        const auto mode = beeperModeForPatch();
        auto value = border;
        if (mode == 1 || mode == 3 || mode == 4)
            value |= 0x10u;
        if (mode == 2 || mode == 3 || mode == 4)
            value |= 0x08u;
        return value;
    }

    double targetFrequencyHz() const
    {
        auto note = static_cast<double>(heldNote >= 0 ? heldNote : 69);
        switch (patch.macro)
        {
            case MacroKind::coin:
                note += 19.0 + (clamp01(patch.control2) * 12.0);
                break;
            case MacroKind::bass:
                note -= 24.0;
                break;
            case MacroKind::arp:
                note += std::round(clamp01(patch.control1) * 12.0);
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                note -= 9.0;
                break;
            case MacroKind::laser:
                note += 12.0 + (clamp01(patch.control2) * 10.0);
                break;
            case MacroKind::jump:
            case MacroKind::powerUp:
                note += clamp01(patch.control2) * 9.0;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }
        return midiNoteToHz(std::clamp(static_cast<int>(std::round(note)), 0, 127));
    }

    double renderFrequencyHz() const
    {
        auto hz = targetFrequencyHz();
        const auto ageSeconds = static_cast<double>(sampleAge) / std::max(1.0, sampleRate);
        switch (patch.macro)
        {
            case MacroKind::coin:
                hz *= 1.0 + std::max(0.0, 1.0 - (ageSeconds * 18.0)) * 0.45;
                break;
            case MacroKind::laser:
                hz *= 0.45 + std::max(0.0, 1.0 - (ageSeconds * (4.0 + clamp01(patch.control2) * 12.0))) * 1.85;
                break;
            case MacroKind::jump:
            case MacroKind::powerUp:
                hz *= 0.62 + std::min(1.0, ageSeconds * (5.0 + clamp01(patch.control2) * 14.0)) * 0.95;
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                hz *= 1.0 + std::max(0.0, 1.0 - (ageSeconds * 24.0)) * 0.65;
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
        const auto base = beeperModeForPatch() == 4 ? 0.18 : 0.5;
        const auto spread = beeperModeForPatch() == 4 ? 0.28 : 0.46;
        return std::clamp(base + ((clamp01(patch.control1) - 0.5) * spread), 0.08, 0.78);
    }

    double gateEnvelope() const
    {
        const auto ageSeconds = static_cast<double>(sampleAge) / std::max(1.0, sampleRate);
        auto decay = clamp01(patch.envelopeDecay);
        if (patch.macro == MacroKind::drum || patch.macro == MacroKind::hit || patch.macro == MacroKind::coin)
            decay = std::max(decay, 0.62);
        if (patch.macro == MacroKind::laser || patch.macro == MacroKind::jump || patch.macro == MacroKind::powerUp)
            decay = std::max(decay, 0.28);
        if (decay <= 0.01)
            return 1.0;
        return std::exp(-ageSeconds * (2.0 + (decay * 34.0)));
    }

    void advanceClickLfsr()
    {
        const auto feedback = ((lfsr & 1u) ^ ((lfsr >> 1u) & 1u) ^ ((lfsr >> 3u) & 1u) ^ ((lfsr >> 12u) & 1u)) & 1u;
        lfsr = (lfsr >> 1u) | (feedback << 15u);
        if (lfsr == 0u)
            lfsr = 0x5a17u;
    }

    int activeChannels() const
    {
        return sourceEnabled(patch, 0) && gateActive && heldNote >= 0 ? 1 : 0;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 3500000.0;
    double phase = 0.0;
    double clickPhase = 0.0;
    uint32_t lfsr = 0x5a17u;
    uint8_t portFe = 0x00u;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    bool gateActive = false;
    uint64_t sampleAge = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeZxSpectrumBeeperCore(AccuracyMode accuracy)
{
    return std::make_unique<ZxSpectrumBeeperCore>(accuracy);
}
}
