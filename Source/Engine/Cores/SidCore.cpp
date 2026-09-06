#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class SidCore final : public ChipCore
{
public:
    explicit SidCore(AccuracyMode selectedAccuracy) : accuracy(selectedAccuracy) {}

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 985248.0;
        regs.fill(0);
        phase.fill(0.0);
        oscillatorWrapped.fill(false);
        noisePhase.fill(0.0);
        envelope.fill(0.0);
        envelopeState.fill(EnvelopeState::release);
        voiceVelocity.fill(0.0f);
        channelNotes.fill(-1);
        channelStamp.fill(0);
        noteStamp = 0;
        noiseLfsr = { 0x7ffff8u, 0x6d2b79u, 0x5a1f33u };
        heldNote = -1;
        noteVelocity = 0.0f;
        filterLowpass = 0.0;
        filterBandpass = 0.0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto index = registerIndex(address);
        if (index >= regs.size())
            return;

        const auto wasGate = isVoiceControl(index) && (regs[index] & 0x01u) != 0;
        regs[index] = value;

        if (! isVoiceControl(index))
            return;

        const auto voice = voiceForControlIndex(index);
        const auto gate = (value & 0x01u) != 0;
        if (gate && ! wasGate)
            startEnvelope(voice);
        else if (! gate && wasGate)
            releaseEnvelope(voice);

        if ((value & 0x08u) != 0)
        {
            phase[voice] = 0.0;
            noisePhase[voice] = 0.0;
            noiseLfsr[voice] = sidNoiseSeed(voice);
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

        auto note0 = heldNote;
        auto note1 = heldNote + std::max(1, static_cast<int>(std::round(patch.control2 * 7.0f)));
        auto note2 = heldNote + std::max(2, static_cast<int>(std::round(patch.control2 * 12.0f)));

        switch (patch.macro)
        {
            case MacroKind::coin:
                note0 = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                note1 = note0 + 12;
                note2 = note0 + 19;
                break;
            case MacroKind::bass:
                note0 = heldNote - 12;
                note1 = heldNote - 24;
                note2 = heldNote - 5;
                break;
            case MacroKind::arp:
                note1 = heldNote + 7;
                note2 = heldNote + 12;
                break;
            case MacroKind::drum:
                note0 = heldNote - 24;
                note1 = heldNote - 12;
                note2 = heldNote;
                break;
            case MacroKind::hit:
                note2 = heldNote - 12;
                break;
            case MacroKind::laser:
                note0 = heldNote + 12 + static_cast<int>(std::round(patch.control2 * 12.0f));
                note1 = heldNote - 12;
                note2 = heldNote + 7;
                break;
            case MacroKind::jump:
                note0 = heldNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                note1 = note0 + 7;
                note2 = note0 + 12;
                break;
            case MacroKind::powerUp:
                note1 = heldNote + 5;
                note2 = heldNote + 12;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        const auto enable = sourceEnableMask(patch) & 0x07u;
        writeFilterRegisters();

        const std::array<int, 3> notes { note0, note1, note2 };
        for (size_t voice = 0; voice < notes.size(); ++voice)
        {
            if ((enable & (1u << voice)) == 0)
            {
                clearGate(voice);
                voiceVelocity[voice] = 0.0f;
                continue;
            }

            voiceVelocity[voice] = noteVelocity;
            writeVoiceRegisters(voice, notes[voice], sidWaveformControlForVoice(patch, voice));
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
            for (size_t voice = 0; voice < voiceCount; ++voice)
                clearGate(voice);

            heldNote = -1;
            noteVelocity = 0.0f;
        }
    }

    StereoFrame renderSample() override
    {
        double mixed = 0.0;
        double filteredMixed = 0.0;
        double directMixed = 0.0;
        double activeVoices = 0.0;
        const auto filterRouting = regs[0x17] & 0x07u;
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            tickEnvelope(voice);

            if (! sourceEnabled(patch, voice) || voiceVelocity[voice] <= 0.0f)
                continue;

            const auto env = envelope[voice];
            if (env <= 0.0001 && ! gateOn(voice))
                continue;

            const auto sample = renderVoice(voice) * env * sourceLevel(patch, voice) * voiceVelocity[voice];
            mixed += sample;
            if ((filterRouting & (1u << voice)) != 0u)
                filteredMixed += sample;
            else
                directMixed += sample;
            activeVoices += 1.0;
        }

        if (activeVoices > 0.0)
        {
            mixed /= std::max(1.0, activeVoices);
            filteredMixed /= std::max(1.0, activeVoices);
            directMixed /= std::max(1.0, activeVoices);
        }

        const auto routedOutput = filterRouting == 0x07u ? applySidOutputFilter(mixed) : (directMixed + applySidOutputFilter(filteredMixed));
        const auto output = static_cast<float>(applySidOutputDrive(routedOutput) * sidMasterVolume() * 0.85);
        return { output, output };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(regs.size());
        for (size_t i = 0; i < regs.size(); ++i)
            writes.push_back({ 0, static_cast<uint16_t>(0xd400 + i), regs[i] });
        return writes;
    }

    ChipMode mode() const override { return ChipMode::sid; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "SID / C64"; }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        return "Three SID oscillator voices, 24-bit frequency-register pitch mapping, waveform control bits, 12-bit pulse width, control-register sync/ring bits with a musical approximation, gate-driven ADSR approximation with exact attack/decay/sustain/release nibble overrides, source trims, cutoff/resonance/filter-routing register writes, selectable partial 6581/8580 filter/output profiles, and a first-pass multimode filter approximation are modeled; OSC3/ENV3 voice-3 readback, silent Voice 3 utility/mod-source behavior, model-specific Voice 3 noise leakage, exact 6581/8580 analog filter behavior, exact ADSR bugs, exact oscillator sync/ring timing, waveform-combination quirks, external input, DAC nonlinearity, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SID / C64\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"sidModelChoice\":" << sidModelChoiceForPatch(patch) << ","
             << "\"sidModelNumber\":" << sidModelNumberForPatch(patch) << ","
             << "\"waveformChoice\":" << std::clamp(patch.waveShape, 0, 8) << ","
             << "\"waveformBits\":" << static_cast<int>(sidWaveformControlForPatch(patch)) << ","
             << "\"waveformChoice0\":" << std::clamp(patch.waveShape, 0, 8) << ","
             << "\"waveformChoice1\":" << std::clamp(patch.sidVoice2WaveShape, 0, 8) << ","
             << "\"waveformChoice2\":" << std::clamp(patch.sidVoice3WaveShape, 0, 8) << ","
             << "\"waveformBits0\":" << static_cast<int>(sidWaveformControlForVoice(patch, 0)) << ","
             << "\"waveformBits1\":" << static_cast<int>(sidWaveformControlForVoice(patch, 1)) << ","
             << "\"waveformBits2\":" << static_cast<int>(sidWaveformControlForVoice(patch, 2)) << ","
             << "\"modModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"modBits0\":" << static_cast<int>(sidModulationBitsForPatch(patch, 0)) << ","
             << "\"modBits1\":" << static_cast<int>(sidModulationBitsForPatch(patch, 1)) << ","
             << "\"modBits2\":" << static_cast<int>(sidModulationBitsForPatch(patch, 2)) << ","
             << "\"pulseWidthControl\":" << patch.control1 << ","
             << "\"pulseWidthRegister\":" << sidPulseWidthForControl(patch.control1) << ","
             << "\"pulseWidthControl1\":" << patch.sidVoice2PulseWidth << ","
             << "\"pulseWidthControl2\":" << patch.sidVoice3PulseWidth << ","
             << "\"pulseWidthRegister0\":" << sidPulseWidthForVoice(patch, 0) << ","
             << "\"pulseWidthRegister1\":" << sidPulseWidthForVoice(patch, 1) << ","
             << "\"pulseWidthRegister2\":" << sidPulseWidthForVoice(patch, 2) << ","
             << "\"filterCutoffRegister\":" << filterCutoffRegister() << ","
             << "\"filterResonanceControl\":" << patch.stereoSpread << ","
             << "\"filterResonanceNibble\":" << static_cast<int>(sidFilterResonanceForControl(patch.stereoSpread)) << ","
             << "\"filterRoutingChoice\":" << std::clamp(patch.sidFilterRouting, 0, 8) << ","
             << "\"filterRoutingBits\":" << static_cast<int>(sidFilterRoutingBitsForPatch(patch)) << ","
             << "\"filterRoutingRegister\":" << static_cast<int>(regs[0x17]) << ","
             << "\"filterModeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 8) << ","
             << "\"filterModeBits\":" << static_cast<int>(sidFilterModeBitsForPatch(patch)) << ","
             << "\"filterModeVolume\":" << static_cast<int>(regs[0x18]) << ","
             << "\"modelCutoffHz\":" << sidModelCutoffHz() << ","
             << "\"modelFilterDamping\":" << sidModelFilterDamping() << ","
             << "\"modelOutputDrive\":" << sidModelOutputDrive() << ","
             << "\"attackChoice\":" << std::clamp(patch.sidAttack, 0, 16) << ","
             << "\"decayChoice\":" << std::clamp(patch.sidDecay, 0, 16) << ","
             << "\"sustainChoice\":" << std::clamp(patch.sidSustain, 0, 16) << ","
             << "\"releaseChoice\":" << std::clamp(patch.sidRelease, 0, 16) << ","
             << "\"attackChoice0\":" << std::clamp(patch.sidAttack, 0, 16) << ","
             << "\"decayChoice0\":" << std::clamp(patch.sidDecay, 0, 16) << ","
             << "\"sustainChoice0\":" << std::clamp(patch.sidSustain, 0, 16) << ","
             << "\"releaseChoice0\":" << std::clamp(patch.sidRelease, 0, 16) << ","
             << "\"attackChoice1\":" << std::clamp(patch.sidVoice2Attack, 0, 16) << ","
             << "\"decayChoice1\":" << std::clamp(patch.sidVoice2Decay, 0, 16) << ","
             << "\"sustainChoice1\":" << std::clamp(patch.sidVoice2Sustain, 0, 16) << ","
             << "\"releaseChoice1\":" << std::clamp(patch.sidVoice2Release, 0, 16) << ","
             << "\"attackChoice2\":" << std::clamp(patch.sidVoice3Attack, 0, 16) << ","
             << "\"decayChoice2\":" << std::clamp(patch.sidVoice3Decay, 0, 16) << ","
             << "\"sustainChoice2\":" << std::clamp(patch.sidVoice3Sustain, 0, 16) << ","
             << "\"releaseChoice2\":" << std::clamp(patch.sidVoice3Release, 0, 16) << ","
             << "\"attackNibbleResolved\":" << static_cast<int>(sidAttackNibbleForPatch(patch)) << ","
             << "\"decayNibbleResolved\":" << static_cast<int>(sidDecayNibbleForPatch(patch)) << ","
             << "\"sustainNibbleResolved\":" << static_cast<int>(sidSustainNibbleForPatch(patch)) << ","
             << "\"releaseNibbleResolved\":" << static_cast<int>(sidReleaseNibbleForPatch(patch)) << ","
             << "\"attackDecayRegister\":" << static_cast<int>(sidAttackDecayForPatch(patch)) << ","
             << "\"sustainReleaseRegister\":" << static_cast<int>(sidSustainReleaseForPatch(patch)) << ","
             << "\"attackDecayRegister0\":" << static_cast<int>(sidAttackDecayForVoice(patch, 0)) << ","
             << "\"sustainReleaseRegister0\":" << static_cast<int>(sidSustainReleaseForVoice(patch, 0)) << ","
             << "\"attackDecayRegister1\":" << static_cast<int>(sidAttackDecayForVoice(patch, 1)) << ","
             << "\"sustainReleaseRegister1\":" << static_cast<int>(sidSustainReleaseForVoice(patch, 1)) << ","
             << "\"attackDecayRegister2\":" << static_cast<int>(sidAttackDecayForVoice(patch, 2)) << ","
             << "\"sustainReleaseRegister2\":" << static_cast<int>(sidSustainReleaseForVoice(patch, 2)) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 2) << ","
             << "\"frequency0\":" << frequencyRegister(0) << ","
             << "\"frequency1\":" << frequencyRegister(1) << ","
             << "\"frequency2\":" << frequencyRegister(2) << ","
             << "\"pulseWidth0\":" << pulseWidthRegister(0) << ","
             << "\"pulseWidth1\":" << pulseWidthRegister(1) << ","
             << "\"pulseWidth2\":" << pulseWidthRegister(2) << ","
             << "\"control0\":" << static_cast<int>(controlRegister(0)) << ","
             << "\"control1\":" << static_cast<int>(controlRegister(1)) << ","
             << "\"control2\":" << static_cast<int>(controlRegister(2)) << ","
             << "\"gate0\":" << (gateOn(0) ? 1 : 0) << ","
             << "\"gate1\":" << (gateOn(1) ? 1 : 0) << ","
             << "\"gate2\":" << (gateOn(2) ? 1 : 0) << ","
             << "\"attack0\":" << static_cast<int>(attackNibble(0)) << ","
             << "\"decay0\":" << static_cast<int>(decayNibble(0)) << ","
             << "\"sustain0\":" << static_cast<int>(sustainNibble(0)) << ","
             << "\"release0\":" << static_cast<int>(releaseNibble(0)) << ","
             << "\"envelope0\":" << envelope[0] << ","
             << "\"envelope1\":" << envelope[1] << ","
             << "\"envelope2\":" << envelope[2] << ","
             << "\"envelopeState0\":" << envelopeStateValue(envelopeState[0]) << ","
             << "\"envelopeState1\":" << envelopeStateValue(envelopeState[1]) << ","
             << "\"envelopeState2\":" << envelopeStateValue(envelopeState[2]) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNote0\":" << channelNotes[0] << ","
             << "\"assignedNote1\":" << channelNotes[1] << ","
             << "\"assignedNote2\":" << channelNotes[2] << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

private:
    enum class EnvelopeState
    {
        attack,
        decay,
        sustain,
        release
    };

    static constexpr size_t voiceCount = 3;

    static size_t registerIndex(uint16_t address)
    {
        if (address >= 0xd400 && address <= 0xd418)
            return static_cast<size_t>(address - 0xd400);
        if (address <= 0x18)
            return static_cast<size_t>(address);
        return static_cast<size_t>(address & 0x1f);
    }

    static bool isVoiceControl(size_t index)
    {
        return index == 0x04 || index == 0x0b || index == 0x12;
    }

    static size_t voiceForControlIndex(size_t index)
    {
        return index < 0x07 ? 0u : (index < 0x0e ? 1u : 2u);
    }

    static size_t voiceBase(size_t voice)
    {
        return std::min(voice, voiceCount - 1u) * 7u;
    }

    static size_t previousVoice(size_t voice)
    {
        const auto safeVoice = std::min(voice, voiceCount - 1u);
        return safeVoice == 0 ? voiceCount - 1u : safeVoice - 1u;
    }

    static uint32_t sidNoiseSeed(size_t voice)
    {
        static constexpr std::array<uint32_t, voiceCount> seeds { 0x7ffff8u, 0x6d2b79u, 0x5a1f33u };
        return seeds[std::min(voice, voiceCount - 1u)];
    }

    static int envelopeStateValue(EnvelopeState state)
    {
        switch (state)
        {
            case EnvelopeState::attack: return 1;
            case EnvelopeState::decay: return 2;
            case EnvelopeState::sustain: return 3;
            case EnvelopeState::release: return 4;
        }
        return 0;
    }

    uint16_t frequencyRegister(size_t voice) const
    {
        const auto base = voiceBase(voice);
        return static_cast<uint16_t>(regs[base] | (regs[base + 1u] << 8u));
    }

    uint16_t pulseWidthRegister(size_t voice) const
    {
        const auto base = voiceBase(voice);
        return static_cast<uint16_t>(regs[base + 2u] | ((regs[base + 3u] & 0x0fu) << 8u));
    }

    uint8_t controlRegister(size_t voice) const
    {
        return regs[voiceBase(voice) + 4u];
    }

    uint8_t attackDecayRegister(size_t voice) const
    {
        return regs[voiceBase(voice) + 5u];
    }

    uint8_t sustainReleaseRegister(size_t voice) const
    {
        return regs[voiceBase(voice) + 6u];
    }

    bool gateOn(size_t voice) const
    {
        return (controlRegister(voice) & 0x01u) != 0;
    }

    uint8_t attackNibble(size_t voice) const
    {
        return static_cast<uint8_t>((attackDecayRegister(voice) >> 4u) & 0x0fu);
    }

    uint8_t decayNibble(size_t voice) const
    {
        return static_cast<uint8_t>(attackDecayRegister(voice) & 0x0fu);
    }

    uint8_t sustainNibble(size_t voice) const
    {
        return static_cast<uint8_t>((sustainReleaseRegister(voice) >> 4u) & 0x0fu);
    }

    uint8_t releaseNibble(size_t voice) const
    {
        return static_cast<uint8_t>(sustainReleaseRegister(voice) & 0x0fu);
    }

    uint16_t filterCutoffRegister() const
    {
        return static_cast<uint16_t>((regs[0x15] & 0x07u) | (regs[0x16] << 3u));
    }

    double sidMasterVolume() const
    {
        return static_cast<double>(regs[0x18] & 0x0fu) / 15.0;
    }

    uint16_t frequencyRegisterForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto value = std::round((hz * 16777216.0) / clock);
        return static_cast<uint16_t>(std::clamp(value, 0.0, 65535.0));
    }

    void writeVoiceRegisters(size_t voice, int midiNote, uint8_t waveformBits)
    {
        const auto base = voiceBase(voice);
        const auto freq = frequencyRegisterForNote(midiNote);
        const auto pulseWidth = sidPulseWidthForVoice(patch, voice);
        writeRegister(static_cast<uint16_t>(0xd400 + base), static_cast<uint8_t>(freq & 0xffu));
        writeRegister(static_cast<uint16_t>(0xd401 + base), static_cast<uint8_t>((freq >> 8u) & 0xffu));
        writeRegister(static_cast<uint16_t>(0xd402 + base), static_cast<uint8_t>(pulseWidth & 0xffu));
        writeRegister(static_cast<uint16_t>(0xd403 + base), static_cast<uint8_t>((pulseWidth >> 8u) & 0x0fu));
        writeRegister(static_cast<uint16_t>(0xd405 + base), sidAttackDecayForVoice(patch, voice));
        writeRegister(static_cast<uint16_t>(0xd406 + base), sidSustainReleaseForVoice(patch, voice));
        writeRegister(static_cast<uint16_t>(0xd404 + base), static_cast<uint8_t>(waveformBits | sidModulationBitsForPatch(patch, voice) | 0x01u));
    }

    void writeFilterRegisters()
    {
        const auto cutoff = static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(clamp01(patch.control3) * 2047.0)), 0, 2047));
        const auto resonance = sidFilterResonanceForControl(patch.stereoSpread);
        const auto modeBits = sidFilterModeBitsForPatch(patch);
        writeRegister(0xd415, static_cast<uint8_t>(cutoff & 0x07u));
        writeRegister(0xd416, static_cast<uint8_t>((cutoff >> 3u) & 0xffu));
        writeRegister(0xd417, static_cast<uint8_t>((resonance << 4u) | sidFilterRoutingBitsForPatch(patch)));
        writeRegister(0xd418, static_cast<uint8_t>(modeBits | 0x0fu));
    }

    void clearGate(size_t voice)
    {
        const auto base = voiceBase(voice);
        writeRegister(static_cast<uint16_t>(0xd404 + base), static_cast<uint8_t>(regs[base + 4u] & ~0x01u));
    }

    void startEnvelope(size_t voice)
    {
        envelopeState[voice] = EnvelopeState::attack;
        if (envelope[voice] <= 0.0)
            envelope[voice] = 0.0;
    }

    void releaseEnvelope(size_t voice)
    {
        envelopeState[voice] = EnvelopeState::release;
    }

    static double attackSeconds(uint8_t nibble)
    {
        return sidAttackSecondsForNibble(nibble);
    }

    static double decayReleaseSeconds(uint8_t nibble)
    {
        return sidDecayReleaseSecondsForNibble(nibble);
    }

    void tickEnvelope(size_t voice)
    {
        const auto sustain = static_cast<double>(sustainNibble(voice)) / 15.0;
        switch (envelopeState[voice])
        {
            case EnvelopeState::attack:
                envelope[voice] += 1.0 / std::max(1.0, attackSeconds(attackNibble(voice)) * sampleRate);
                if (envelope[voice] >= 1.0)
                {
                    envelope[voice] = 1.0;
                    envelopeState[voice] = EnvelopeState::decay;
                }
                break;
            case EnvelopeState::decay:
                if (envelope[voice] > sustain)
                {
                    envelope[voice] -= (1.0 - sustain) / std::max(1.0, decayReleaseSeconds(decayNibble(voice)) * sampleRate);
                    if (envelope[voice] <= sustain)
                    {
                        envelope[voice] = sustain;
                        envelopeState[voice] = EnvelopeState::sustain;
                    }
                }
                else
                {
                    envelopeState[voice] = EnvelopeState::sustain;
                }
                break;
            case EnvelopeState::sustain:
                envelope[voice] = sustain;
                break;
            case EnvelopeState::release:
                if (envelope[voice] > 0.0)
                {
                    envelope[voice] -= 1.0 / std::max(1.0, decayReleaseSeconds(releaseNibble(voice)) * sampleRate);
                    if (envelope[voice] < 0.0)
                        envelope[voice] = 0.0;
                }
                break;
        }
    }

    int activeChipPolyChannels() const
    {
        int active = 0;
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] >= 0)
                ++active;
        }
        return active;
    }

    void clearChipPolyState()
    {
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            channelNotes[voice] = -1;
            voiceVelocity[voice] = 0.0f;
            channelStamp[voice] = 0;
            clearGate(voice);
        }
        noteStamp = 0;
        noteVelocity = 0.0f;
    }

    int selectChipPolyVoice(int midiNote) const
    {
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] == midiNote)
                return static_cast<int>(voice);
        }

        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] < 0)
                return static_cast<int>(voice);
        }

        auto oldestVoice = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (sourceEnabled(patch, voice) && channelStamp[voice] < oldestStamp)
            {
                oldestStamp = channelStamp[voice];
                oldestVoice = static_cast<int>(voice);
            }
        }

        return oldestVoice;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto selected = selectChipPolyVoice(midiNote);
        if (selected < 0)
            return;

        const auto voice = static_cast<size_t>(selected);
        channelNotes[voice] = std::clamp(midiNote, 0, 127);
        channelStamp[voice] = ++noteStamp;
        voiceVelocity[voice] = static_cast<float>(clamp01(velocity));
        writeFilterRegisters();
        writeVoiceRegisters(voice, channelNotes[voice], sidWaveformControlForVoice(patch, voice));
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t voice = 0; voice < voiceCount; ++voice)
        {
            if (channelNotes[voice] != midiNote)
                continue;

            channelNotes[voice] = -1;
            channelStamp[voice] = 0;
            clearGate(voice);
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    double oscillatorFrequency(size_t voice) const
    {
        return (static_cast<double>(frequencyRegister(voice)) * clock) / 16777216.0;
    }

    double renderVoice(size_t voice)
    {
        if ((controlRegister(voice) & 0x08u) != 0)
            return 0.0;

        const auto hz = oscillatorFrequency(voice);
        const auto nextPhase = phase[voice] + (hz / sampleRate);
        phase[voice] = wrapPhase(nextPhase);
        oscillatorWrapped[voice] = nextPhase >= 1.0;

        if ((controlRegister(voice) & 0x02u) != 0 && oscillatorWrapped[previousVoice(voice)])
        {
            phase[voice] = 0.0;
            oscillatorWrapped[voice] = true;
        }

        const auto waveform = controlRegister(voice) & 0xf0u;
        if ((waveform & 0x80u) != 0)
            return renderNoise(voice, hz);

        double mixed = 0.0;
        auto activeWaveforms = 0;
        if ((waveform & 0x40u) != 0)
        {
            if (pulseWidthRegister(voice) != 0)
            {
                const auto width = std::clamp(static_cast<double>(pulseWidthRegister(voice)) / 4096.0, 1.0 / 4096.0, 4095.0 / 4096.0);
                mixed += phase[voice] < width ? 1.0 : -1.0;
                ++activeWaveforms;
            }
        }
        if ((waveform & 0x20u) != 0)
        {
            mixed += (phase[voice] * 2.0) - 1.0;
            ++activeWaveforms;
        }
        if ((waveform & 0x10u) != 0)
        {
            auto triangle = phase[voice] < 0.5 ? (-1.0 + (phase[voice] * 4.0)) : (3.0 - (phase[voice] * 4.0));
            if ((controlRegister(voice) & 0x04u) != 0 && phase[previousVoice(voice)] >= 0.5)
                triangle = -triangle;
            mixed += triangle;
            ++activeWaveforms;
        }

        if (activeWaveforms > 0)
            return mixed / static_cast<double>(activeWaveforms);

        return 0.0;
    }

    double renderNoise(size_t voice, double hz)
    {
        noisePhase[voice] += hz / sampleRate;
        while (noisePhase[voice] >= 1.0)
        {
            noisePhase[voice] -= 1.0;
            const auto feedback = ((noiseLfsr[voice] >> 22u) ^ (noiseLfsr[voice] >> 17u)) & 0x01u;
            noiseLfsr[voice] = ((noiseLfsr[voice] << 1u) | feedback) & 0x7fffffu;
            if (noiseLfsr[voice] == 0)
                noiseLfsr[voice] = sidNoiseSeed(voice);
        }

        const auto output =
            (((noiseLfsr[voice] >> 20u) & 0x01u) << 7u)
            | (((noiseLfsr[voice] >> 18u) & 0x01u) << 6u)
            | (((noiseLfsr[voice] >> 14u) & 0x01u) << 5u)
            | (((noiseLfsr[voice] >> 11u) & 0x01u) << 4u)
            | (((noiseLfsr[voice] >> 9u) & 0x01u) << 3u)
            | (((noiseLfsr[voice] >> 5u) & 0x01u) << 2u)
            | (((noiseLfsr[voice] >> 2u) & 0x01u) << 1u)
            | ((noiseLfsr[voice] >> 0u) & 0x01u);
        return (static_cast<double>(output) / 127.5) - 1.0;
    }

    double sidModelCutoffHz() const
    {
        const auto cutoffNorm = static_cast<double>(filterCutoffRegister()) / 2047.0;
        if (sidModelChoiceForPatch(patch) == 2)
            return 45.0 + (std::pow(cutoffNorm, 1.55) * 14000.0);

        return 120.0 + (std::pow(cutoffNorm, 2.35) * 9000.0);
    }

    double sidModelFilterDamping() const
    {
        const auto resonance = static_cast<double>(sidFilterResonanceForControl(patch.stereoSpread)) / 15.0;
        if (sidModelChoiceForPatch(patch) == 2)
            return std::clamp(1.10 - (resonance * 0.82), 0.12, 1.10);

        return std::clamp(1.35 - (resonance * 0.68), 0.25, 1.35);
    }

    double sidModelOutputDrive() const
    {
        return sidModelChoiceForPatch(patch) == 2 ? 0.96 : 1.32;
    }

    double applySidOutputDrive(double input) const
    {
        const auto drive = sidModelOutputDrive();
        if (drive <= 1.0)
            return input * drive;

        const auto normalizer = std::tanh(drive);
        return normalizer > 0.0 ? std::tanh(input * drive) / normalizer : input;
    }

    double applySidOutputFilter(double input)
    {
        const auto modeBits = regs[0x18] & 0x70u;
        if (modeBits == 0)
            return input;

        const auto cutoffHz = sidModelCutoffHz();
        const auto coefficient = std::clamp(2.0 * std::sin((0.5 * twoPi * cutoffHz) / std::max(1.0, sampleRate)), 0.001, 0.99);
        const auto damping = sidModelFilterDamping();

        filterLowpass += coefficient * filterBandpass;
        const auto highpass = input - filterLowpass - (damping * filterBandpass);
        filterBandpass += coefficient * highpass;

        auto output = 0.0;
        if ((modeBits & 0x10u) != 0)
            output += filterLowpass;
        if ((modeBits & 0x20u) != 0)
            output += filterBandpass;
        if ((modeBits & 0x40u) != 0)
            output += highpass;

        return std::clamp(output, -4.0, 4.0);
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 985248.0;
    std::array<uint8_t, 0x19> regs {};
    std::array<double, voiceCount> phase {};
    std::array<bool, voiceCount> oscillatorWrapped {};
    std::array<double, voiceCount> noisePhase {};
    std::array<double, voiceCount> envelope {};
    std::array<EnvelopeState, voiceCount> envelopeState {};
    std::array<uint32_t, voiceCount> noiseLfsr {};
    std::array<float, voiceCount> voiceVelocity {};
    std::array<int, voiceCount> channelNotes { -1, -1, -1 };
    std::array<uint64_t, voiceCount> channelStamp {};
    uint64_t noteStamp = 0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    double filterLowpass = 0.0;
    double filterBandpass = 0.0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeSidCore(AccuracyMode accuracy)
{
    return std::make_unique<SidCore>(accuracy);
}
}
