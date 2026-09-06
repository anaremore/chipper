#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class Spc700Core final : public ChipCore
{
public:
    explicit Spc700Core(AccuracyMode requested)
        : accuracy(requested)
    {
        channelNotes.fill(-1);
        reset(sampleRate, clock);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate > 0.0 ? outputSampleRate : 48000.0;
        clock = chipClockHz > 0.0 ? chipClockHz : 32000.0;
        pitch.fill(0x1000);
        volume.fill(0);
        adsr.fill(0);
        gain.fill(0);
        enabledMask = 0;
        keyOnMask = 0;
        position.fill(0.0);
        voiceAgeSamples.fill(0);
        lastVoiceSample.fill(0.0);
        envelope.fill(0.0);
        envelopeStage.fill(0);
        noiseAccumulator.fill(0.0);
        for (size_t voice = 0; voice < noiseLfsr.size(); ++voice)
            noiseLfsr[voice] = static_cast<uint16_t>(0x4000u | ((voice + 1u) * 0x0231u));
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        noteStamp = 0;
        heldNote = -1;
        noteVelocity = 0.0f;
        sampleTemplate = 0;
        playbackMode = 1;
        const auto hasExternalBank = ! externalBrrBank.empty();
        if (! hasExternalBank)
        {
            externalBrrSampleLoaded = false;
            externalPcmSampleLoaded = false;
            selectedExternalBrrSlot = -1;
            brrBlockCount = 0;
            brrEndFlagSeen = false;
            brrLoopFlagSeen = false;
            lastDecodedBrrLoopStart = 0;
        }
        sampleLoopStarts.fill(0);
        echoMemoryLeft.fill(0.0);
        echoMemoryRight.fill(0.0);
        echoIndex = 0;
        for (size_t voice = 0; voice < sampleRam.size(); ++voice)
        {
            if (hasExternalBank)
                applyCurrentSampleToVoice(voice);
            else
                seedSample(voice);
        }
    }

    void setPatch(const PatchConfig& nextPatch) override
    {
        if (nextPatch.playMode != patch.playMode || nextPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        const auto sampleSlotsChanged = nextPatch.spc700VoiceSampleSlots != patch.spc700VoiceSampleSlots;
        patch = nextPatch;
        const auto nextTemplate = resolvedSampleTemplate();
        if (! externalBrrSampleLoaded && nextTemplate != sampleTemplate)
        {
            sampleTemplate = nextTemplate;
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                seedSample(voice);
        }
        else if (externalBrrSampleLoaded && sampleSlotsChanged)
        {
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                applyCurrentSampleToVoice(voice);
        }
        playbackMode = spc700SamplePlaybackModeForPatch(patch);
    }

    void setExternalSampleData(std::vector<uint8_t> data) override
    {
        std::vector<std::vector<uint8_t>> bank;
        if (! data.empty())
            bank.push_back(std::move(data));
        setExternalSampleBank(std::move(bank), 0);
    }

    void setExternalSampleBank(std::vector<std::vector<uint8_t>> bank, int selectedSlot) override
    {
        std::vector<ExternalSampleData> typedBank;
        typedBank.reserve(bank.size());
        for (auto& sampleBytes : bank)
            typedBank.push_back({ std::move(sampleBytes), ExternalSampleEncoding::spc700Brr });
        setExternalSampleBank(std::move(typedBank), selectedSlot);
    }

    void setExternalSampleBank(std::vector<ExternalSampleData> bank, int selectedSlot) override
    {
        externalBrrSampleLoaded = false;
        externalPcmSampleLoaded = false;
        externalBrrBank.clear();
        externalBrrLoopStarts.clear();
        selectedExternalBrrSlot = -1;
        brrBlockCount = 0;
        brrEndFlagSeen = false;
        brrLoopFlagSeen = false;

        externalBrrBank.reserve(bank.size());
        externalBrrLoopStarts.reserve(bank.size());
        for (const auto& sample : bank)
        {
            std::vector<double> decoded;
            auto decodedLoopStart = size_t { 0 };
            if (sample.encoding == ExternalSampleEncoding::signedPcm8)
            {
                decoded = decodePcm8Sample(sample.bytes);
                externalPcmSampleLoaded = externalPcmSampleLoaded || ! decoded.empty();
            }
            else
            {
                decoded = decodeBrrSample(sample.bytes);
                decodedLoopStart = lastDecodedBrrLoopStart;
                if (decoded.empty() && sample.encoding == ExternalSampleEncoding::rawBytes)
                {
                    decoded = decodePcm8Sample(sample.bytes);
                    decodedLoopStart = 0;
                    externalPcmSampleLoaded = externalPcmSampleLoaded || ! decoded.empty();
                }
            }

            if (! decoded.empty())
                decodedLoopStart = decodedLoopStart < decoded.size() ? decodedLoopStart : 0;
            externalBrrBank.push_back(std::move(decoded));
            externalBrrLoopStarts.push_back(decodedLoopStart);
        }

        if (externalBrrBank.empty())
            return;

        externalBrrSampleLoaded = true;
        setExternalSampleSlot(selectedSlot);
        sampleTemplate = 0;
        position.fill(0.0);
    }

    void setExternalSampleSlot(int selectedSlot) override
    {
        if (externalBrrBank.empty())
        {
            selectedExternalBrrSlot = -1;
            externalBrrSampleLoaded = false;
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                seedSample(voice);
            return;
        }

        if (selectedSlot < 0)
        {
            selectedExternalBrrSlot = -1;
            externalBrrSampleLoaded = true;
            for (size_t voice = 0; voice < sampleRam.size(); ++voice)
                applyCurrentSampleToVoice(voice);
            return;
        }

        selectedExternalBrrSlot = std::clamp(selectedSlot, 0, static_cast<int>(externalBrrBank.size() - 1u));
        externalBrrSampleLoaded = true;
        for (size_t voice = 0; voice < sampleRam.size(); ++voice)
            applyCurrentSampleToVoice(voice);
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        const auto reg = static_cast<uint8_t>(address & 0xffu);
        const auto voice = static_cast<size_t>((reg >> 4u) & 7u);
        const auto offset = reg & 0x0fu;

        switch (offset)
        {
            case 0: pitch[voice] = static_cast<uint16_t>((pitch[voice] & 0xff00u) | value); break;
            case 1: pitch[voice] = static_cast<uint16_t>((pitch[voice] & 0x00ffu) | ((value & 0x3fu) << 8u)); break;
            case 2: volume[voice] = value & 0x7fu; break;
            case 3: adsr[voice] = value; break;
            case 4: gain[voice] = value & 0x7fu; break;
            case 5:
                sampleTemplate = value % 5u;
                seedSample(voice);
                break;
            case 6:
                if ((value & 0x01u) != 0)
                    enabledMask |= static_cast<uint8_t>(1u << voice);
                else
                    enabledMask &= static_cast<uint8_t>(~(1u << voice));
                break;
            case 7:
                playbackMode = static_cast<uint8_t>(std::clamp(static_cast<int>(value), 1, 2));
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
        std::array<int, 8> notes {
            heldNote,
            heldNote + std::max(1, spread / 4),
            heldNote + std::max(2, spread / 2),
            heldNote + std::max(3, (spread * 3) / 4),
            heldNote - 12,
            heldNote + 12,
            heldNote + 19,
            heldNote + 24
        };

        switch (patch.macro)
        {
            case MacroKind::bass:
                notes = { heldNote - 24, heldNote - 12, heldNote, heldNote + 7, heldNote - 17, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::arp:
                notes = { heldNote, heldNote + 4, heldNote + 7, heldNote + 12, heldNote + 16, heldNote + 19, heldNote + 24, heldNote + 28 };
                break;
            case MacroKind::drum:
            case MacroKind::hit:
                notes = { heldNote - 24, heldNote - 17, heldNote - 12, heldNote, heldNote + 7, heldNote + 12, heldNote + 19, heldNote + 24 };
                break;
            case MacroKind::coin:
                notes = { heldNote + 24, heldNote + 31, heldNote + 36, heldNote + 43, heldNote + 48, heldNote + 55, heldNote + 60, heldNote + 67 };
                break;
            case MacroKind::laser:
                for (size_t voice = 0; voice < notes.size(); ++voice)
                    notes[voice] = heldNote + 24 - static_cast<int>(voice * 4) + static_cast<int>(std::round(patch.control2 * 12.0f));
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
        keyOnMask = 0;
        for (size_t voice = 0; voice < pitch.size(); ++voice)
            triggerVoice(voice, notes[voice], noteVelocity, voiceActiveForPatch(voice));
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
            for (size_t voice = 0; voice < envelopeStage.size(); ++voice)
                releaseVoice(voice);
        }
    }

    StereoFrame renderSample() override
    {
        double left = 0.0;
        double right = 0.0;
        double echoInputLeft = 0.0;
        double echoInputRight = 0.0;
        auto audibleCount = 0;
        auto echoCount = 0;

        for (size_t voice = 0; voice < pitch.size(); ++voice)
        {
            const auto sample = renderVoice(voice);
            if (std::abs(sample) <= 1.0e-9)
                continue;

            ++audibleCount;
            const auto leftGain = voiceLeftGain(voice);
            const auto rightGain = voiceRightGain(voice);
            left += sample * leftGain;
            right += sample * rightGain;

            const auto voiceEcho = voiceEchoSendLevel(voice);
            if (voiceEcho > 0.0)
            {
                ++echoCount;
                echoInputLeft += sample * leftGain * voiceEcho;
                echoInputRight += sample * rightGain * voiceEcho;
            }
        }

        if (audibleCount > 0)
        {
            left /= static_cast<double>(audibleCount);
            right /= static_cast<double>(audibleCount);
        }
        if (echoCount > 0)
        {
            echoInputLeft /= static_cast<double>(echoCount);
            echoInputRight /= static_cast<double>(echoCount);
        }

        const auto echoSend = spc700EchoSend();
        const auto echoFeedback = spc700EchoFeedback();
        const auto delaySamples = spc700EchoDelaySamples();
        const auto readIndex = (echoIndex + echoMemoryLeft.size() - delaySamples) % echoMemoryLeft.size();
        const auto filteredLeft = spc700FirFiltered(echoMemoryLeft, readIndex);
        const auto filteredRight = spc700FirFiltered(echoMemoryRight, readIndex);

        echoMemoryLeft[echoIndex] = std::clamp(echoInputLeft + (filteredLeft * echoFeedback), -1.0, 1.0);
        echoMemoryRight[echoIndex] = std::clamp(echoInputRight + (filteredRight * echoFeedback), -1.0, 1.0);
        echoIndex = (echoIndex + 1) % echoMemoryLeft.size();
        left += filteredLeft * echoSend;
        right += filteredRight * echoSend;

        return { static_cast<float>(std::clamp(left, -1.0, 1.0)),
                 static_cast<float>(std::clamp(right, -1.0, 1.0)) };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        for (uint8_t voice = 0; voice < pitch.size(); ++voice)
        {
            const auto base = static_cast<uint16_t>(voice * 0x10u);
            writes.push_back({ 0, base, static_cast<uint8_t>(pitch[voice] & 0xffu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>((pitch[voice] >> 8u) & 0x3fu) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 2u), volume[voice] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 3u), adsr[voice] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 4u), gain[voice] });
            writes.push_back({ 0, static_cast<uint16_t>(base + 5u), sampleTemplate });
            writes.push_back({ 0, static_cast<uint16_t>(base + 6u), static_cast<uint8_t>((enabledMask >> voice) & 1u) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 7u), playbackMode });
            writes.push_back({ 0, static_cast<uint16_t>(base + 8u), voiceLeftVolumeRegister(voice) });
            writes.push_back({ 0, static_cast<uint16_t>(base + 9u), voiceRightVolumeRegister(voice) });
        }
        writes.push_back({ 0, 0x0du, static_cast<uint8_t>(static_cast<int>(spc700EchoFeedbackRegister()) & 0xff) });
        writes.push_back({ 0, 0x2cu, static_cast<uint8_t>(static_cast<int>(spc700EchoLeftVolumeRegister()) & 0xff) });
        writes.push_back({ 0, 0x3cu, static_cast<uint8_t>(static_cast<int>(spc700EchoRightVolumeRegister()) & 0xff) });
        writes.push_back({ 0, 0x4du, echoEnabledMask() });
        writes.push_back({ 0, 0x7du, spc700EchoDelayRegister() });
        const auto fir = spc700FirCoefficients();
        for (size_t tap = 0; tap < fir.size(); ++tap)
        {
            const auto reg = static_cast<uint16_t>(0x0fu + (tap * 0x10u));
            const auto value = static_cast<uint8_t>(static_cast<int>(fir[tap]) & 0xff);
            writes.push_back({ 0, reg, value });
        }
        return writes;
    }

    ChipMode mode() const override { return ChipMode::spc700; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return "SNES SPC700-style"; }
    std::string implementedAccuracy() const override { return "partial clean-room sample-voice model"; }
    std::string limitations() const override
    {
        return "Eight lo-fi sample voices, pitch, 7-bit per-voice left/right volume state, loop/one-shot playback state, playable ADSR/gain-style note shaping, generated sample templates, clean-room BRR block decoding for renderer-loaded samples, BRR loop-flag loop starts, Gaussian-style 4-tap sample interpolation, partial S-DSP-style per-voice noise source, musical pitch motion, partial PMON-style previous-voice pitch modulation, an echo-enable mask, echo volume/feedback/delay register state, signed 8-bit FIR coefficient state, and a musical stereo echo helper are modeled; exact S-DSP Gaussian table behavior, SPC700 CPU timing, S-DSP register edge cases, source-directory loop address behavior, exact noise timing, exact PMON timing/scaling, exact FIR echo memory behavior, sample directory addressing, exact envelope timing, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"SNES SPC700-style\","
             << "\"implementedAccuracy\":\"partial clean-room sample-voice model\","
             << "\"clockHz\":" << clock << ","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"waveShapeChoice\":" << static_cast<int>(sampleTemplate) << ","
             << "\"samplePlaybackMode\":" << static_cast<int>(playbackMode) << ","
             << "\"sampleLoopEnabled\":" << (playbackMode == 1u ? 1 : 0) << ","
             << "\"envelopeShapeChoice\":" << std::clamp(patch.ymEnvelopeShape, 0, 4) << ","
             << "\"envelopeShapeResolved\":" << static_cast<int>(spc700EnvelopeShapeForPatch(patch)) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 4) << ","
             << "\"noiseModeResolved\":" << static_cast<int>(spc700NoiseModeForPatch(patch)) << ","
             << "\"noiseClock\":" << static_cast<int>(spc700NoiseClockForPatch(patch)) << ","
             << "\"noiseEnabledMask\":" << static_cast<int>(noiseEnabledMask()) << ","
             << "\"externalBrrSampleLoaded\":" << (externalBrrSampleLoaded ? 1 : 0) << ","
             << "\"externalPcmSampleLoaded\":" << (externalPcmSampleLoaded ? 1 : 0) << ","
             << "\"brrBlockCount\":" << brrBlockCount << ","
             << "\"brrEndFlagSeen\":" << (brrEndFlagSeen ? 1 : 0) << ","
             << "\"brrLoopFlagSeen\":" << (brrLoopFlagSeen ? 1 : 0) << ","
             << "\"externalBrrBankSize\":" << externalBrrBank.size() << ","
             << "\"selectedExternalBrrSlot\":" << selectedExternalBrrSlot << ","
             << "\"selectedExternalSampleLength\":" << selectedExternalSampleLength() << ","
             << "\"selectedExternalSampleLoopStart\":" << selectedExternalSampleLoopStart() << ","
             << "\"voiceSampleSlot0\":" << patch.spc700VoiceSampleSlots[0] << ","
             << "\"voiceSampleSlot1\":" << patch.spc700VoiceSampleSlots[1] << ","
             << "\"voiceSampleSlot2\":" << patch.spc700VoiceSampleSlots[2] << ","
             << "\"voiceSampleSlot3\":" << patch.spc700VoiceSampleSlots[3] << ","
             << "\"voiceSampleSlot4\":" << patch.spc700VoiceSampleSlots[4] << ","
             << "\"voiceSampleSlot5\":" << patch.spc700VoiceSampleSlots[5] << ","
             << "\"voiceSampleSlot6\":" << patch.spc700VoiceSampleSlots[6] << ","
             << "\"voiceSampleSlot7\":" << patch.spc700VoiceSampleSlots[7] << ","
             << "\"pitch0\":" << pitch[0] << ","
             << "\"pitch1\":" << pitch[1] << ","
             << "\"pitch2\":" << pitch[2] << ","
             << "\"pitch3\":" << pitch[3] << ","
             << "\"pitch4\":" << pitch[4] << ","
             << "\"pitch5\":" << pitch[5] << ","
             << "\"pitch6\":" << pitch[6] << ","
             << "\"pitch7\":" << pitch[7] << ","
             << "\"pitchMotionDepthCents\":" << spc700PitchMotionDepthCents() << ","
             << "\"pitchMotionDirection\":" << spc700PitchMotionDirection() << ","
             << "\"pitchModDepthCents\":" << spc700PitchModDepthCents() << ","
             << "\"pitchModEnabledMask\":" << static_cast<int>(spc700PitchModEnabledMask()) << ","
             << "\"pitchModSource0\":" << lastVoiceSample[0] << ","
             << "\"echoSend\":" << spc700EchoSend() << ","
             << "\"echoEnabledMask\":" << static_cast<int>(echoEnabledMask()) << ","
             << "\"echoInputLevel\":" << echoInputLevel() << ","
             << "\"echoFeedback\":" << spc700EchoFeedback() << ","
             << "\"echoDelayMs\":" << spc700EchoDelayMs() << ","
             << "\"echoDelaySamples\":" << spc700EchoDelaySamples() << ","
             << "\"echoLeftVolumeRegister\":" << static_cast<int>(spc700EchoLeftVolumeRegister()) << ","
             << "\"echoRightVolumeRegister\":" << static_cast<int>(spc700EchoRightVolumeRegister()) << ","
             << "\"echoFeedbackRegister\":" << static_cast<int>(spc700EchoFeedbackRegister()) << ","
             << "\"echoDelayRegister\":" << static_cast<int>(spc700EchoDelayRegister()) << ","
             << "\"firTap0\":" << static_cast<int>(spc700FirCoefficients()[0]) << ","
             << "\"firTap1\":" << static_cast<int>(spc700FirCoefficients()[1]) << ","
             << "\"firTap2\":" << static_cast<int>(spc700FirCoefficients()[2]) << ","
             << "\"firTap3\":" << static_cast<int>(spc700FirCoefficients()[3]) << ","
             << "\"firTap4\":" << static_cast<int>(spc700FirCoefficients()[4]) << ","
             << "\"firTap5\":" << static_cast<int>(spc700FirCoefficients()[5]) << ","
             << "\"firTap6\":" << static_cast<int>(spc700FirCoefficients()[6]) << ","
             << "\"firTap7\":" << static_cast<int>(spc700FirCoefficients()[7]) << ","
             << "\"firCoefficientSum\":" << spc700FirCoefficientSum() << ","
             << "\"pitch0Current\":" << currentPitchForVoice(0) << ","
             << "\"pitch1Current\":" << currentPitchForVoice(1) << ","
             << "\"pitch7Current\":" << currentPitchForVoice(7) << ","
             << "\"volume0\":" << static_cast<int>(volume[0]) << ","
             << "\"volume1\":" << static_cast<int>(volume[1]) << ","
             << "\"volume2\":" << static_cast<int>(volume[2]) << ","
             << "\"volume3\":" << static_cast<int>(volume[3]) << ","
             << "\"volume4\":" << static_cast<int>(volume[4]) << ","
             << "\"volume5\":" << static_cast<int>(volume[5]) << ","
             << "\"volume6\":" << static_cast<int>(volume[6]) << ","
             << "\"volume7\":" << static_cast<int>(volume[7]) << ","
             << "\"volumeLeft0\":" << static_cast<int>(voiceLeftVolumeRegister(0)) << ","
             << "\"volumeRight0\":" << static_cast<int>(voiceRightVolumeRegister(0)) << ","
             << "\"volumeLeft7\":" << static_cast<int>(voiceLeftVolumeRegister(7)) << ","
             << "\"volumeRight7\":" << static_cast<int>(voiceRightVolumeRegister(7)) << ","
             << "\"adsr0\":" << static_cast<int>(adsr[0]) << ","
             << "\"envelope0\":" << envelope[0] << ","
             << "\"envelopeStage0\":" << static_cast<int>(envelopeStage[0]) << ","
             << "\"sustainLevel\":" << spc700SustainLevel() << ","
             << "\"attackSeconds\":" << spc700AttackSeconds() << ","
             << "\"decaySeconds\":" << spc700DecaySeconds() << ","
             << "\"releaseSeconds\":" << spc700ReleaseSeconds() << ","
             << "\"gain0\":" << static_cast<int>(gain[0]) << ","
             << "\"enabledMask\":" << static_cast<int>(enabledMask) << ","
             << "\"keyOnMask\":" << static_cast<int>(keyOnMask) << ","
             << "\"sourceEnabled0\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
             << "\"sourceEnabled5\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
             << "\"sourceEnabled6\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
             << "\"sourceEnabled7\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
             << "\"sourceLevel0\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 3) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 4) << ","
             << "\"sourceLevel5\":" << sourceLevel(patch, 5) << ","
             << "\"sourceLevel6\":" << sourceLevel(patch, 6) << ","
             << "\"sourceLevel7\":" << sourceLevel(patch, 7) << ","
             << "\"sampleLength0\":" << sampleRam[0].size() << ","
             << "\"sampleLoopStart0\":" << sampleLoopStarts[0] << ","
             << "\"sampleLoopStartResolved0\":" << resolvedLoopStartForVoice(0) << ","
             << "\"sampleLoopEndResolved0\":" << resolvedLoopEndForVoice(0) << ","
             << "\"sampleLoopControlStart\":" << patch.spc700LoopStart << ","
             << "\"sampleLoopControlEnd\":" << patch.spc700LoopEnd << ","
             << "\"externalBrrLoopStartSelected\":" << (selectedExternalBrrSlot >= 0 ? loopStartForExternalSlot(static_cast<size_t>(selectedExternalBrrSlot)) : 0) << ","
             << "\"gaussianStyleInterpolation\":1,"
             << "\"interpolationTaps\":4,"
             << "\"uiExposesAllEightVoices\":1,"
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
    uint16_t spcPitchForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        return static_cast<uint16_t>(std::clamp(std::round((hz / 32000.0) * 4096.0 * 16.0), 1.0, 16383.0));
    }

    uint8_t voiceVolumeForPatch(size_t voice, float velocity) const
    {
        const auto base = std::clamp(static_cast<int>(std::round(patch.control4 * 127.0f)), 1, 127);
        const auto trim = voice < patch.sourceLevels.size() ? sourceLevel(patch, voice) : 0.72f;
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(base) * trim * clamp01(velocity))), 0, 127));
    }

    bool voiceActiveForPatch(size_t voice) const
    {
        if (voice < patch.sourceEnabled.size())
            return sourceEnabled(patch, voice);

        return patch.macro == MacroKind::arp
            || patch.macro == MacroKind::hit
            || patch.macro == MacroKind::laser
            || patch.macro == MacroKind::powerUp;
    }

    uint8_t resolvedSampleTemplate() const
    {
        const auto choiceValue = std::clamp(patch.waveShape, 0, 4);
        if (choiceValue > 0)
            return static_cast<uint8_t>(choiceValue);

        switch (patch.macro)
        {
            case MacroKind::bass: return 2;
            case MacroKind::drum:
            case MacroKind::hit: return 4;
            case MacroKind::lead:
            case MacroKind::arp: return 1;
            default: return 3;
        }
    }

    void seedSample(size_t voice)
    {
        auto& data = sampleRam[voice];
        data.assign(64u, 0.0);
        const auto choice = sampleTemplate == 0 ? resolvedSampleTemplate() : sampleTemplate;
        for (size_t i = 0; i < data.size(); ++i)
        {
            const auto phaseValue = static_cast<double>(i) / static_cast<double>(data.size());
            double sample = 0.0;
            switch (choice)
            {
                case 1: sample = std::sin(twoPi * phaseValue) * 0.75 + (((i / 8u) & 1u) ? 0.12 : -0.12); break;
                case 2: sample = phaseValue < 0.5 ? (phaseValue * 4.0) - 1.0 : 3.0 - (phaseValue * 4.0); break;
                case 3: sample = phaseValue < 0.5 ? 0.78 : -0.78; break;
                case 4:
                {
                    const auto hash = (static_cast<int>(i) * 1664525u + static_cast<int>(voice) * 1013904223u) & 0xffffu;
                    const auto burst = std::exp(-phaseValue * 7.0);
                    sample = ((static_cast<double>(hash) / 32767.5) - 1.0) * burst;
                    break;
                }
                default: sample = std::sin(twoPi * phaseValue); break;
            }
            data[i] = std::clamp(std::round(sample * 127.0) / 127.0, -1.0, 1.0);
        }
        sampleLoopStarts[voice] = 0;
    }

    std::vector<double> decodeBrrSample(const std::vector<uint8_t>& data)
    {
        std::vector<double> decoded;
        lastDecodedBrrLoopStart = 0;
        if (data.size() < 9u || (data.size() % 9u) != 0u)
            return decoded;

        brrBlockCount = data.size() / 9u;
        decoded.reserve(brrBlockCount * 16u);
        auto previous1 = 0;
        auto previous2 = 0;
        auto decodedLoopFlagSeen = false;

        for (size_t block = 0; block < brrBlockCount; ++block)
        {
            const auto offset = block * 9u;
            const auto header = data[offset];
            const auto range = static_cast<int>((header >> 4u) & 0x0fu);
            const auto filter = static_cast<int>((header >> 2u) & 0x03u);
            brrEndFlagSeen = brrEndFlagSeen || ((header & 0x01u) != 0);
            if ((header & 0x02u) != 0)
            {
                if (! decodedLoopFlagSeen)
                {
                    lastDecodedBrrLoopStart = decoded.size();
                    decodedLoopFlagSeen = true;
                }
                brrLoopFlagSeen = true;
            }

            for (size_t packedIndex = 0; packedIndex < 8u; ++packedIndex)
            {
                const auto packed = data[offset + 1u + packedIndex];
                for (const auto nibble : { static_cast<uint8_t>(packed >> 4u), static_cast<uint8_t>(packed & 0x0fu) })
                {
                    auto sample = (nibble >= 8u ? static_cast<int>(nibble) - 16 : static_cast<int>(nibble));
                    if (range <= 12)
                        sample <<= range;
                    else
                        sample = sample < 0 ? -2048 : 0;

                    auto predicted = sample;
                    switch (filter)
                    {
                        case 1:
                            predicted += (previous1 * 15) / 16;
                            break;
                        case 2:
                            predicted += ((previous1 * 61) / 32) - ((previous2 * 15) / 16);
                            break;
                        case 3:
                            predicted += ((previous1 * 115) / 64) - ((previous2 * 13) / 16);
                            break;
                        case 0:
                        default:
                            break;
                    }

                    predicted = std::clamp(predicted, -32768, 32767);
                    previous2 = previous1;
                    previous1 = predicted;
                    decoded.push_back(static_cast<double>(predicted) / 32768.0);
                }
            }

            if ((header & 0x01u) != 0)
                break;
        }

        return decoded;
    }

    std::vector<double> decodePcm8Sample(const std::vector<uint8_t>& data) const
    {
        std::vector<double> decoded;
        decoded.reserve(data.size());
        for (const auto byte : data)
            decoded.push_back(std::clamp((static_cast<double>(byte) - 128.0) / 128.0, -1.0, 1.0));
        return decoded;
    }

    void triggerVoice(size_t voice, int midiNote, float velocity, bool shouldEnable)
    {
        playbackMode = spc700SamplePlaybackModeForPatch(patch);
        pitch[voice] = spcPitchForNote(midiNote + static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f)));
        volume[voice] = voiceVolumeForPatch(voice, velocity);
        adsr[voice] = spc700AdsrForPatch(patch);
        gain[voice] = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control4 * 127.0f)), 0, 127));
        position[voice] = 0.0;
        voiceAgeSamples[voice] = 0;
        lastVoiceSample[voice] = 0.0;
        noiseAccumulator[voice] = 0.0;
        noiseLfsr[voice] = static_cast<uint16_t>(0x4000u | ((voice + 1u) * 0x0231u) | (static_cast<unsigned>(midiNote & 0x7f) << 3u));
        envelope[voice] = 0.0;
        envelopeStage[voice] = shouldEnable ? 1u : 0u;
        applyCurrentSampleToVoice(voice);
        if (shouldEnable && volume[voice] > 0)
        {
            enabledMask |= static_cast<uint8_t>(1u << voice);
            keyOnMask |= static_cast<uint8_t>(1u << voice);
        }
    }

    double renderVoice(size_t voice)
    {
        const auto useNoise = voiceUsesNoise(voice);
        if ((enabledMask & (1u << voice)) == 0 || (! useNoise && sampleRam[voice].empty()) || volume[voice] == 0)
        {
            lastVoiceSample[voice] = 0.0;
            return 0.0;
        }

        updateEnvelope(voice);
        if (envelope[voice] <= 0.0001 && envelopeStage[voice] == 0u)
        {
            lastVoiceSample[voice] = 0.0;
            return 0.0;
        }

        const auto rawSample = useNoise ? renderNoiseVoice(voice) : interpolatedSample(voice, position[voice]);
        lastVoiceSample[voice] = rawSample * envelope[voice];
        const auto sample = rawSample * (static_cast<double>(volume[voice]) / 127.0) * envelope[voice];
        const auto playbackHz = (currentPitchForVoice(voice) / (4096.0 * 16.0)) * 32000.0;
        if (! useNoise)
            position[voice] += (playbackHz * static_cast<double>(sampleRam[voice].size())) / sampleRate;
        ++voiceAgeSamples[voice];

        if (useNoise)
        {
            const auto transientSeconds = std::max(0.035, spc700DecaySeconds() * 2.5);
            if (playbackMode == 2u && static_cast<double>(voiceAgeSamples[voice]) > transientSeconds * sampleRate)
                releaseVoice(voice);
        }
        else if (position[voice] >= static_cast<double>(sampleRam[voice].size()))
        {
            if (playbackMode == 1u)
            {
                const auto loopStart = static_cast<double>(resolvedLoopStartForVoice(voice));
                const auto loopEnd = static_cast<double>(resolvedLoopEndForVoice(voice));
                const auto loopLength = std::max(1.0, loopEnd - loopStart);
                position[voice] = loopStart + std::fmod(std::max(0.0, position[voice] - loopStart), loopLength);
            }
            else
            {
                position[voice] = 0.0;
                envelope[voice] = 0.0;
                envelopeStage[voice] = 0u;
                lastVoiceSample[voice] = 0.0;
                enabledMask &= static_cast<uint8_t>(~(1u << voice));
                keyOnMask &= static_cast<uint8_t>(~(1u << voice));
                return 0.0;
            }
        }

        return sample;
    }

    bool voiceUsesNoise(size_t voice) const
    {
        const auto mode = spc700NoiseModeForPatch(patch);
        if (mode <= 1u)
            return false;

        const auto explicitNoise = std::clamp(patch.snNoiseMode, 0, 4) > 1;
        if (explicitNoise)
            return true;

        return patch.macro == MacroKind::drum
            || patch.macro == MacroKind::hit
            || patch.waveShape == 4
            || voice >= 6u;
    }

    uint8_t noiseEnabledMask() const
    {
        uint8_t mask = 0;
        for (size_t voice = 0; voice < noiseLfsr.size(); ++voice)
        {
            if (voiceUsesNoise(voice) && sourceEnabled(patch, voice))
                mask |= static_cast<uint8_t>(1u << voice);
        }
        return mask;
    }

    double spc700NoiseRateHz() const
    {
        const auto clockIndex = static_cast<double>(spc700NoiseClockForPatch(patch));
        if (clockIndex <= 0.0)
            return 0.0;

        return std::clamp(32000.0 / std::pow(2.0, (31.0 - clockIndex) / 4.0), 60.0, 32000.0);
    }

    double renderNoiseVoice(size_t voice)
    {
        const auto rate = spc700NoiseRateHz();
        if (rate <= 0.0 || sampleRate <= 0.0)
            return 0.0;

        noiseAccumulator[voice] += rate / sampleRate;
        while (noiseAccumulator[voice] >= 1.0)
        {
            const auto bit = static_cast<uint16_t>(((noiseLfsr[voice] >> 0u) ^ (noiseLfsr[voice] >> 1u)) & 1u);
            noiseLfsr[voice] = static_cast<uint16_t>((noiseLfsr[voice] >> 1u) | (bit << 14u));
            noiseAccumulator[voice] -= 1.0;
        }

        return (noiseLfsr[voice] & 1u) != 0 ? 0.72 : -0.72;
    }

    double spc700SustainLevel() const
    {
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.20;
            case 3: return 0.78;
            case 4: return 0.03;
            case 2:
            default:
                switch (patch.macro)
                {
                    case MacroKind::bass: return 0.72;
                    case MacroKind::arp: return 0.56;
                    case MacroKind::laser: return 0.34;
                    case MacroKind::powerUp: return 0.58;
                    case MacroKind::lead:
                    case MacroKind::manual:
                    default: return 0.64;
                }
        }
    }

    double spc700AttackSeconds() const
    {
        const auto speed = std::clamp(static_cast<double>(patch.envelopeDecay), 0.0, 1.0);
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.0015 + (1.0 - speed) * 0.006;
            case 3: return 0.025 + (1.0 - speed) * 0.155;
            case 4: return 0.0008 + (1.0 - speed) * 0.002;
            case 2:
            default: return 0.004 + (1.0 - speed) * 0.026;
        }
    }

    double spc700DecaySeconds() const
    {
        const auto speed = std::clamp(static_cast<double>(patch.envelopeDecay), 0.0, 1.0);
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.030 + (1.0 - speed) * 0.180;
            case 3: return 0.450 + (1.0 - speed) * 1.550;
            case 4: return 0.018 + (1.0 - speed) * 0.130;
            case 2:
            default: return 0.110 + (1.0 - speed) * 0.780;
        }
    }

    double spc700ReleaseSeconds() const
    {
        const auto speed = std::clamp(static_cast<double>(patch.envelopeDecay), 0.0, 1.0);
        switch (spc700EnvelopeShapeForPatch(patch))
        {
            case 1: return 0.018 + (1.0 - speed) * 0.130;
            case 3: return 0.260 + (1.0 - speed) * 1.120;
            case 4: return 0.010 + (1.0 - speed) * 0.075;
            case 2:
            default: return 0.060 + (1.0 - speed) * 0.520;
        }
    }

    void updateEnvelope(size_t voice)
    {
        if (voice >= envelopeStage.size() || sampleRate <= 0.0)
            return;

        switch (envelopeStage[voice])
        {
            case 1:
            {
                envelope[voice] += 1.0 / std::max(1.0, spc700AttackSeconds() * sampleRate);
                if (envelope[voice] >= 1.0)
                {
                    envelope[voice] = 1.0;
                    envelopeStage[voice] = 2u;
                }
                break;
            }
            case 2:
            {
                const auto sustain = spc700SustainLevel();
                const auto coeff = std::exp(-1.0 / std::max(1.0, spc700DecaySeconds() * sampleRate));
                envelope[voice] = sustain + ((envelope[voice] - sustain) * coeff);
                if (std::abs(envelope[voice] - sustain) <= 0.001)
                {
                    envelope[voice] = sustain;
                    envelopeStage[voice] = 3u;
                }
                break;
            }
            case 3:
                envelope[voice] = spc700SustainLevel();
                break;
            case 4:
            {
                const auto coeff = std::exp(-1.0 / std::max(1.0, spc700ReleaseSeconds() * sampleRate));
                envelope[voice] *= coeff;
                if (envelope[voice] <= 0.0005)
                {
                    envelope[voice] = 0.0;
                    envelopeStage[voice] = 0u;
                    enabledMask &= static_cast<uint8_t>(~(1u << voice));
                    keyOnMask &= static_cast<uint8_t>(~(1u << voice));
                }
                break;
            }
            case 0:
            default:
                envelope[voice] = 0.0;
                break;
        }
    }

    void releaseVoice(size_t voice)
    {
        if (voice >= envelopeStage.size())
            return;

        keyOnMask &= static_cast<uint8_t>(~(1u << voice));
        if ((enabledMask & (1u << voice)) != 0 && envelope[voice] > 0.0005)
            envelopeStage[voice] = 4u;
        else
        {
            envelope[voice] = 0.0;
            envelopeStage[voice] = 0u;
            enabledMask &= static_cast<uint8_t>(~(1u << voice));
        }
    }

    void applyCurrentSampleToVoice(size_t voice)
    {
        if (externalBrrSampleLoaded)
        {
            const auto explicitSlotChoice = voice < patch.spc700VoiceSampleSlots.size()
                ? std::clamp(patch.spc700VoiceSampleSlots[voice], 0, 32)
                : 0;
            const auto explicitSlot = explicitSlotChoice > 0 ? explicitSlotChoice - 1 : -1;
            const auto resolvedSlot = explicitSlot >= 0 ? explicitSlot : selectedExternalBrrSlot;
            if (resolvedSlot >= 0 && static_cast<size_t>(resolvedSlot) < externalBrrBank.size())
            {
                sampleRam[voice] = externalBrrBank[static_cast<size_t>(resolvedSlot)];
                sampleLoopStarts[voice] = loopStartForExternalSlot(static_cast<size_t>(resolvedSlot));
            }
            else
            {
                sampleRam[voice].clear();
                sampleLoopStarts[voice] = 0;
            }
            return;
        }

        seedSample(voice);
    }

    size_t loopStartForExternalSlot(size_t slot) const
    {
        if (slot < externalBrrLoopStarts.size())
            return externalBrrLoopStarts[slot];

        return 0;
    }

    size_t selectedExternalSampleLength() const
    {
        if (selectedExternalBrrSlot < 0 || static_cast<size_t>(selectedExternalBrrSlot) >= externalBrrBank.size())
            return 0;

        return externalBrrBank[static_cast<size_t>(selectedExternalBrrSlot)].size();
    }

    size_t selectedExternalSampleLoopStart() const
    {
        if (selectedExternalBrrSlot < 0)
            return 0;

        return loopStartForExternalSlot(static_cast<size_t>(selectedExternalBrrSlot));
    }

    bool usesExplicitLoopPointControls() const
    {
        return patch.spc700LoopStart > 0.0005f || patch.spc700LoopEnd < 0.9995f;
    }

    size_t resolvedLoopStartForVoice(size_t voice) const
    {
        if (voice >= sampleRam.size() || sampleRam[voice].empty())
            return 0;

        const auto sampleLength = sampleRam[voice].size();
        if (! usesExplicitLoopPointControls())
            return std::min(sampleLoopStarts[voice], sampleLength - 1u);

        const auto start = static_cast<size_t>(std::round(std::clamp(patch.spc700LoopStart, 0.0f, 1.0f) * static_cast<float>(sampleLength - 1u)));
        return std::min(start, sampleLength - 1u);
    }

    size_t resolvedLoopEndForVoice(size_t voice) const
    {
        if (voice >= sampleRam.size() || sampleRam[voice].empty())
            return 0;

        const auto sampleLength = sampleRam[voice].size();
        if (! usesExplicitLoopPointControls())
            return sampleLength;

        auto end = static_cast<size_t>(std::round(std::clamp(patch.spc700LoopEnd, 0.0f, 1.0f) * static_cast<float>(sampleLength - 1u))) + 1u;
        const auto start = resolvedLoopStartForVoice(voice);
        if (end <= start + 1u)
            end = std::min(sampleLength, start + 2u);
        return std::clamp(end, size_t { 1u }, sampleLength);
    }

    double interpolatedSample(size_t voice, double samplePosition) const
    {
        const auto& data = sampleRam[voice];
        if (data.empty())
            return 0.0;

        const auto length = static_cast<int>(data.size());
        const auto base = static_cast<int>(std::floor(samplePosition));
        const auto frac = std::clamp(samplePosition - static_cast<double>(base), 0.0, 0.999999);
        static constexpr auto sigma = 0.58;

        auto weighted = 0.0;
        auto weightSum = 0.0;
        for (int tap = -1; tap <= 2; ++tap)
        {
            const auto wrapped = (base + tap + length) % length;
            const auto distance = static_cast<double>(tap) - frac;
            const auto weight = std::exp(-(distance * distance) / (2.0 * sigma * sigma));
            weighted += data[static_cast<size_t>(wrapped)] * weight;
            weightSum += weight;
        }

        return weightSum > 0.0 ? weighted / weightSum : data[static_cast<size_t>(base % length)];
    }

    double spc700PitchMotionDepthCents() const
    {
        const auto centered = static_cast<double>(patch.control2) - 0.5;
        const auto magnitude = std::max(0.0, std::abs(centered) - 0.08) / 0.42;
        const auto macroScale = patch.macro == MacroKind::laser || patch.macro == MacroKind::powerUp || patch.macro == MacroKind::jump
            ? 2400.0
            : 900.0;
        return std::clamp(magnitude, 0.0, 1.0) * macroScale;
    }

    double spc700PitchModDepthCents() const
    {
        if (spc700PitchMotionDirection() == 0)
            return 0.0;

        return std::clamp(spc700PitchMotionDepthCents() * 0.35, 0.0, 480.0);
    }

    uint8_t spc700PitchModEnabledMask() const
    {
        if (spc700PitchModDepthCents() <= 0.001)
            return 0;

        uint8_t mask = 0;
        for (size_t voice = 1; voice < pitch.size(); ++voice)
        {
            const auto voiceBit = static_cast<uint8_t>(1u << voice);
            const auto previousVoiceBit = static_cast<uint8_t>(1u << (voice - 1u));
            if ((enabledMask & voiceBit) != 0 && (enabledMask & previousVoiceBit) != 0)
                mask |= voiceBit;
        }
        return mask;
    }

    int spc700PitchMotionDirection() const
    {
        const auto centered = static_cast<double>(patch.control2) - 0.5;
        if (std::abs(centered) <= 0.08)
            return 0;
        return centered > 0.0 ? 1 : -1;
    }

    double currentPitchForVoice(size_t voice) const
    {
        const auto basePitch = static_cast<double>(pitch[voice]);
        auto currentPitch = basePitch;
        const auto direction = spc700PitchMotionDirection();
        const auto depthCents = spc700PitchMotionDepthCents();
        if (direction != 0 && depthCents > 0.001)
        {
            const auto seconds = sampleRate > 0.0 ? static_cast<double>(voiceAgeSamples[voice]) / sampleRate : 0.0;
            const auto attack = 1.0 - std::exp(-seconds * 7.0);
            const auto sfxTilt = (patch.macro == MacroKind::laser || patch.macro == MacroKind::hit) ? -1.0 : 1.0;
            const auto signedCents = depthCents * static_cast<double>(direction) * sfxTilt * attack;
            currentPitch = currentPitch * std::pow(2.0, signedCents / 1200.0);
        }

        if (voice > 0 && (spc700PitchModEnabledMask() & static_cast<uint8_t>(1u << voice)) != 0)
        {
            const auto modCents = std::clamp(lastVoiceSample[voice - 1u], -1.0, 1.0) * spc700PitchModDepthCents();
            currentPitch = currentPitch * std::pow(2.0, modCents / 1200.0);
        }

        return std::clamp(currentPitch, 1.0, 16383.0);
    }

    double voicePan(size_t voice) const
    {
        static constexpr std::array<double, 8> panPositions { -1.0, -0.72, -0.44, -0.16, 0.16, 0.44, 0.72, 1.0 };
        if (voice >= panPositions.size())
            return 0.0;

        return panPositions[voice] * std::clamp(static_cast<double>(patch.stereoSpread), 0.0, 1.0);
    }

    double voiceLeftGain(size_t voice) const
    {
        const auto pan = voicePan(voice);
        return pan <= 0.0 ? 1.0 : 1.0 - pan;
    }

    double voiceRightGain(size_t voice) const
    {
        const auto pan = voicePan(voice);
        return pan >= 0.0 ? 1.0 : 1.0 + pan;
    }

    uint8_t voiceLeftVolumeRegister(size_t voice) const
    {
        if (voice >= volume.size())
            return 0;

        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(volume[voice]) * voiceLeftGain(voice))), 0, 127));
    }

    uint8_t voiceRightVolumeRegister(size_t voice) const
    {
        if (voice >= volume.size())
            return 0;

        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(static_cast<double>(volume[voice]) * voiceRightGain(voice))), 0, 127));
    }

    double voiceEchoSendLevel(size_t voice) const
    {
        if (voice >= patch.sourceLevels.size() || ! sourceEnabled(patch, voice))
            return 0.0;

        return std::clamp(static_cast<double>(patch.sourceLevels[voice]), 0.0, 1.0);
    }

    uint8_t echoEnabledMask() const
    {
        if (spc700EchoSend() <= 0.0)
            return 0;

        uint8_t mask = 0;
        for (size_t voice = 0; voice < volume.size(); ++voice)
        {
            if (voiceEchoSendLevel(voice) > 0.001)
                mask |= static_cast<uint8_t>(1u << voice);
        }
        return mask;
    }

    double echoInputLevel() const
    {
        auto sum = 0.0;
        auto count = 0;
        for (size_t voice = 0; voice < volume.size(); ++voice)
        {
            const auto level = voiceEchoSendLevel(voice);
            if (level > 0.001)
            {
                sum += level;
                ++count;
            }
        }

        return count > 0 ? sum / static_cast<double>(count) : 0.0;
    }

    std::array<int8_t, 8> spc700FirCoefficients() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        if (color <= 0.01)
            return { 127, 0, 0, 0, 0, 0, 0, 0 };

        return {
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(112.0 - (48.0 * color))), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(8.0 + (20.0 * color))), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(4.0 + (12.0 * color))), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(8.0 * color)), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(4.0 * color)), -128, 127)),
            static_cast<int8_t>(std::clamp(static_cast<int>(std::round(-2.0 * color)), -128, 127)),
            0,
            0,
        };
    }

    int spc700FirCoefficientSum() const
    {
        const auto fir = spc700FirCoefficients();
        return std::accumulate(fir.begin(), fir.end(), 0, [] (int sum, int8_t coefficient)
        {
            return sum + static_cast<int>(coefficient);
        });
    }

    double spc700FirFiltered(const std::array<double, 48000>& memory, size_t readIndex) const
    {
        const auto fir = spc700FirCoefficients();
        auto filtered = 0.0;
        for (size_t tap = 0; tap < fir.size(); ++tap)
        {
            const auto index = (readIndex + memory.size() - tap) % memory.size();
            filtered += memory[index] * (static_cast<double>(fir[tap]) / 128.0);
        }
        return std::clamp(filtered, -1.0, 1.0);
    }

    double spc700EchoSend() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        if (color <= 0.01)
            return 0.0;
        return std::clamp(0.08 + (color * 0.42), 0.0, 0.50);
    }

    int8_t spc700EchoLeftVolumeRegister() const
    {
        return static_cast<int8_t>(std::clamp(static_cast<int>(std::round(spc700EchoSend() * 127.0)), -128, 127));
    }

    int8_t spc700EchoRightVolumeRegister() const
    {
        return spc700EchoLeftVolumeRegister();
    }

    double spc700EchoFeedback() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        return std::clamp(0.16 + (color * 0.42), 0.0, 0.58);
    }

    int8_t spc700EchoFeedbackRegister() const
    {
        return static_cast<int8_t>(std::clamp(static_cast<int>(std::round(spc700EchoFeedback() * 127.0)), -128, 127));
    }

    double spc700EchoDelayMs() const
    {
        const auto color = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        return 32.0 + std::round(color * 13.0) * 16.0;
    }

    uint8_t spc700EchoDelayRegister() const
    {
        const auto edl = static_cast<int>(std::round((spc700EchoDelayMs() - 32.0) / 16.0));
        return static_cast<uint8_t>(std::clamp(edl, 0, 15));
    }

    size_t spc700EchoDelaySamples() const
    {
        const auto requested = static_cast<size_t>(std::max(1.0, std::round(spc700EchoDelayMs() * sampleRate / 1000.0)));
        return std::clamp<size_t>(requested, 1u, echoMemoryLeft.size() - 1u);
    }

    int selectChipPolyChannel(int midiNote) const
    {
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] == midiNote)
                return static_cast<int>(voice);
        }
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] < 0)
                return static_cast<int>(voice);
        }

        auto oldestVoice = -1;
        auto oldestStamp = std::numeric_limits<uint64_t>::max();
        for (size_t voice = 0; voice < channelStamp.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelStamp[voice] < oldestStamp)
            {
                oldestStamp = channelStamp[voice];
                oldestVoice = static_cast<int>(voice);
            }
        }
        return oldestVoice;
    }

    int activeChipPolyChannels() const
    {
        int active = 0;
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (sourceEnabled(patch, voice) && channelNotes[voice] >= 0)
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
        keyOnMask = 0;
        lastVoiceSample.fill(0.0);
        envelope.fill(0.0);
        envelopeStage.fill(0);
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        const auto voice = selectChipPolyChannel(midiNote);
        if (voice < 0)
            return;

        const auto index = static_cast<size_t>(voice);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;
        triggerVoice(index, channelNotes[index], channelVelocity[index], true);
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void noteOffChipPoly(int midiNote)
    {
        for (size_t voice = 0; voice < channelNotes.size(); ++voice)
        {
            if (channelNotes[voice] != midiNote)
                continue;

            channelNotes[voice] = -1;
            channelVelocity[voice] = 0.0f;
            channelStamp[voice] = 0;
            releaseVoice(voice);
        }
        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    AccuracyMode accuracy;
    double sampleRate = 48000.0;
    double clock = 32000.0;
    std::array<uint16_t, 8> pitch {};
    std::array<uint8_t, 8> volume {};
    std::array<uint8_t, 8> adsr {};
    std::array<uint8_t, 8> gain {};
    uint8_t enabledMask = 0;
    uint8_t keyOnMask = 0;
    uint8_t sampleTemplate = 0;
    uint8_t playbackMode = 1;
    bool externalBrrSampleLoaded = false;
    bool externalPcmSampleLoaded = false;
    int selectedExternalBrrSlot = -1;
    size_t brrBlockCount = 0;
    bool brrEndFlagSeen = false;
    bool brrLoopFlagSeen = false;
    size_t lastDecodedBrrLoopStart = 0;
    std::vector<std::vector<double>> externalBrrBank {};
    std::vector<size_t> externalBrrLoopStarts {};
    std::array<std::vector<double>, 8> sampleRam {};
    std::array<size_t, 8> sampleLoopStarts {};
    std::array<double, 8> position {};
    std::array<uint64_t, 8> voiceAgeSamples {};
    std::array<double, 8> lastVoiceSample {};
    std::array<double, 8> envelope {};
    std::array<uint8_t, 8> envelopeStage {};
    std::array<uint16_t, 8> noiseLfsr {};
    std::array<double, 8> noiseAccumulator {};
    std::array<double, 48000> echoMemoryLeft {};
    std::array<double, 48000> echoMemoryRight {};
    size_t echoIndex = 0;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 8> channelNotes {};
    std::array<float, 8> channelVelocity {};
    std::array<uint64_t, 8> channelStamp {};
    uint64_t noteStamp = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeSpc700Core(AccuracyMode accuracy)
{
    return std::make_unique<Spc700Core>(accuracy);
}
}
