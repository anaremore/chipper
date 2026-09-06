#include "CoreHelpers.h"
#include "CoreFactories.h"
namespace chipper::core_detail
{
namespace
{
class NesApuCore final : public ChipCore
{
public:
    explicit NesApuCore(AccuracyMode selectedAccuracy, ChipMode selectedMode = ChipMode::nes)
        : accuracy(selectedAccuracy), selectedMode(selectedMode) {}

    ~NesApuCore() override
    {
        if (vrc7Opll != nullptr)
            OPLL_delete(vrc7Opll);
    }

    void reset(double outputSampleRate, double chipClockHz) override
    {
        sampleRate = outputSampleRate;
        clock = chipClockHz > 0.0 ? chipClockHz : 1789773.0;
        regs.fill(0);
        phase.fill(0.0);
        timer.fill(0);
        enabled = { true, true, true, true };
        envelopeVolume.fill(0);
        envelopeDivider.fill(0);
        envelopeStart.fill(false);
        lengthCounter.fill(0);
        sweepDivider.fill(0);
        sweepReload.fill(false);
        linearCounter = 0;
        linearReloadFlag = false;
        frameSequencerPhase = 0.0;
        frameSequenceStep = 0;
        frameCounterFiveStep = false;
        frameIrqInhibit = false;
        frameIrqFlag = false;
        hp90Input = 0.0;
        hp90Output = 0.0;
        hp440Input = 0.0;
        hp440Output = 0.0;
        lp14000Output = 0.0;
        lfsr = 1;
        heldNote = -1;
        noteVelocity = 0.0f;
        channelNotes.fill(-1);
        channelVelocity.fill(0.0f);
        channelStamp.fill(0);
        vrc6Phase.fill(0.0);
        vrc6Timer.fill(0);
        vrc6Enabled.fill(false);
        vrc6PulseVolume.fill(0);
        vrc6PulseDuty.fill(0);
        vrc6SawRate = 0;
        vrc6SawAccumulator = 0;
        vrc6SawStep = 0;
        vrc6SawStepPhase = 0.0;
        fdsWaveRam.fill(32);
        fdsModTable.fill(0);
        fdsPhase = 0.0;
        fdsModPhase = 0.0;
        fdsFrequency = 0;
        fdsModFrequency = 0;
        fdsVolume = 0;
        fdsMasterVolume = 1;
        fdsModDepth = 0;
        fdsModAccumulator = 0;
        fdsModIndex = 0;
        fdsEnabled = false;
        if (hasFds())
            refreshFdsWaveRam();
        sunsoft5bPhase.fill(0.0);
        sunsoft5bTonePeriod.fill(1);
        sunsoft5bVolume.fill(0);
        sunsoft5bEnabled.fill(false);
        sunsoft5bMixer = 0x38;
        mmc5Phase.fill(0.0);
        mmc5PulseTimer.fill(1);
        mmc5PulseVolume.fill(0);
        mmc5PulseDuty.fill(0);
        mmc5PulseEnabled.fill(false);
        mmc5PcmLevel = 64;
        mmc5PcmEnvelope = 0.0;
        mmc5PcmEnabled = false;
        vrc7Regs.fill(0);
        vrc7CurrentPatch.fill(0);
        vrc7CurrentFnum.fill(0);
        vrc7CurrentBlock.fill(0);
        vrc7KeyOnMask = 0;
        if (hasVrc7())
        {
            const auto vrc7Clock = static_cast<uint32_t>(std::round(vrc7ClockHz()));
            const auto outputRate = static_cast<uint32_t>(std::round(sampleRate));
            if (vrc7Opll == nullptr)
                vrc7Opll = OPLL_new(vrc7Clock, outputRate);
            else
                OPLL_setClockRate(vrc7Opll, vrc7Clock, outputRate);
            OPLL_reset(vrc7Opll);
            OPLL_setChipType(vrc7Opll, 1);
            OPLL_resetPatch(vrc7Opll, OPLL_VRC7_TONE);
            OPLL_setMask(vrc7Opll, OPLL_MASK_CH(6) | OPLL_MASK_CH(7) | OPLL_MASK_CH(8) | OPLL_MASK_RHYTHM);
            for (size_t channel = 0; channel < 6; ++channel)
                writeVrc7Register(static_cast<uint8_t>(0x30u + channel), 0x0fu);
        }
        noteStamp = 0;
        dmcActive = false;
        dmcSampleCompleted = false;
        dmcByteIndex = 0;
        dmcBitIndex = 0;
        dmcPhase = 0.0;
        dmcBitsPlayed = 0;
    }

    void setPatch(const PatchConfig& newPatch) override
    {
        const auto previousPatch = patch;
        if (newPatch.playMode != patch.playMode || newPatch.sourceEnabled != patch.sourceEnabled)
            clearChipPolyState();

        patch = newPatch;
        if (patch.nesDmcLoop)
            regs[0x10] = static_cast<uint8_t>(regs[0x10] | 0x40u);
        else
            regs[0x10] = static_cast<uint8_t>(regs[0x10] & ~0x40u);
        regs[0x11] = nesDmcDirectLevelForControl(patch.nesDmcDirectLevel);

        if (newPatch.playMode == previousPatch.playMode
            && newPatch.sourceEnabled == previousPatch.sourceEnabled
            && (newPatch.control1 != previousPatch.control1 || newPatch.pulse2Duty != previousPatch.pulse2Duty))
        {
            refreshActivePulseDuties();
        }

        if (hasFds())
            refreshFdsWaveRam();
        if (hasVrc7())
            applyVrc7CustomPatchIfNeeded();
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
        dmcSampleBank = std::move(bank);
        setExternalSampleSlot(selectedSlot);
    }

    void setExternalSampleSlot(int selectedSlot) override
    {
        dmcSampleSlot = dmcSampleBank.empty()
            ? -1
            : std::clamp(selectedSlot, -1, static_cast<int>(dmcSampleBank.size() - 1u));
        dmcActive = false;
        dmcSampleCompleted = false;
        dmcByteIndex = 0;
        dmcBitIndex = 0;
        dmcPhase = 0.0;
        dmcBitsPlayed = 0;
    }

    void writeRegister(uint16_t address, uint8_t value) override
    {
        if (address >= 0x4000 && address <= 0x4017)
            regs[static_cast<size_t>(address - 0x4000)] = value;

        if (address == 0x4011)
            regs[0x11] = static_cast<uint8_t>(value & 0x7fu);

        if (address == 0x4003)
            triggerChannel(0, value);
        else if (address == 0x4007)
            triggerChannel(1, value);
        else if (address == 0x400b)
        {
            triggerChannel(2, value);
            linearReloadFlag = true;
        }
        else if (address == 0x400f)
            triggerChannel(3, value);

        if (address == 0x4001)
            sweepReload[0] = true;
        else if (address == 0x4005)
            sweepReload[1] = true;

        if (address == 0x4015)
            writeStatusRegister(value, true);
        else if (address == 0x4017)
        {
            writeFrameCounter(value);
        }

        updateTimers();
    }

    void noteOn(int midiNote, float velocity) override
    {
        if (patch.playMode == PlayMode::chipPoly)
        {
            noteOnChipPoly(midiNote, velocity);
            return;
        }

        heldNote = midiNote;
        noteVelocity = static_cast<float>(clamp01(velocity));
        if (! hasVrc7() && ! suppressDmcRestartOnNoteOn)
            startDmcSample();
        const auto duty = nesPulseDutyFromControl(patch.control1);
        const auto pulse2Duty = nesPulse2DutyForPatch(patch, duty, true);

        auto p1Note = midiNote;
        auto p2Note = midiNote;
        auto triNote = midiNote - 12;
        auto p1Vol = 15u;
        auto p2Vol = 8u;
        auto noiseVol = static_cast<unsigned>(std::round(patch.control3 * 12.0f));
        auto enable = 0x03u;

        switch (patch.macro)
        {
            case MacroKind::coin:
                p1Note = midiNote + 12 + static_cast<int>(std::round(patch.control2 * 7.0f));
                p2Vol = 0;
                noiseVol = 0;
                enable = 0x01u;
                break;
            case MacroKind::bass:
                p1Note = midiNote - 12;
                p2Note = midiNote - 24;
                triNote = midiNote - 24;
                p1Vol = 5u;
                p2Vol = 0u;
                noiseVol = 0;
                enable = 0x05u;
                break;
            case MacroKind::arp:
                p2Note = midiNote + 7 + static_cast<int>(std::round(patch.control2 * 5.0f));
                triNote = midiNote + 12;
                noiseVol = 0;
                enable = 0x07u;
                break;
            case MacroKind::drum:
                p1Note = midiNote - 24;
                p1Vol = 4u;
                p2Vol = 0u;
                triNote = midiNote - 36;
                noiseVol = static_cast<unsigned>(10u + std::round(patch.control3 * 5.0f));
                enable = 0x0du;
                break;
            case MacroKind::hit:
                p1Note = midiNote - 5;
                p2Vol = 0u;
                noiseVol = static_cast<unsigned>(8u + std::round(patch.control3 * 7.0f));
                enable = 0x09u;
                break;
            case MacroKind::laser:
                p1Note = midiNote + 7 + static_cast<int>(std::round(patch.control2 * 17.0f));
                p2Note = midiNote - 5;
                p2Vol = 5u;
                noiseVol = static_cast<unsigned>(std::round(patch.control3 * 8.0f));
                enable = 0x0bu;
                break;
            case MacroKind::jump:
                p1Note = midiNote + static_cast<int>(std::round(patch.control2 * 12.0f));
                p2Vol = 0u;
                noiseVol = 0u;
                enable = 0x01u;
                break;
            case MacroKind::powerUp:
                p2Note = midiNote + 12;
                triNote = midiNote + 7;
                noiseVol = static_cast<unsigned>(std::round(patch.control3 * 5.0f));
                enable = 0x07u;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                enable = patch.control4 < 0.33f ? 0x04u : (patch.control4 > 0.66f ? 0x03u : 0x07u);
                break;
        }

        if (hasVrc7())
        {
            enable = 0u;
            if (sourceEnabled(patch, 0))
                enable = static_cast<unsigned>(enable | 0x01u);
            if (sourceEnabled(patch, 1))
                enable = static_cast<unsigned>(enable | 0x02u);
            if (sourceEnabled(patch, 2))
                enable = static_cast<unsigned>(enable | 0x04u);
            noiseVol = 0u;
        }
        else if (patch.nesDmcOnly)
        {
            enable = activeDmcSample().empty() || ! sourceEnabled(patch, dmcSourceIndex()) ? 0u : 0x10u;
            noiseVol = 0u;
        }
        else
        {
            const auto requestedSources = enabledSourceMaskOrRecipe(patch, static_cast<uint8_t>(enable & 0x0fu)) & 0x0fu;
            enable = requestedSources;
            if ((enable & 0x01u) != 0u)
                p1Vol = ensureNesRegisterVolume(p1Vol, 8u);
            if ((enable & 0x02u) != 0u)
                p2Vol = ensureNesRegisterVolume(p2Vol, std::max(5u, p1Vol));
            if ((enable & 0x08u) != 0u)
                noiseVol = ensureNesRegisterVolume(noiseVol, static_cast<unsigned>(std::max<int>(8, std::round(patch.control3 * 12.0f))));
            if (! activeDmcSample().empty() && sourceEnabled(patch, dmcSourceIndex()))
                enable |= 0x10u;
        }

        writePulseRegisters(0x4000, duty, p1Vol, p1Note);
        writePulseRegisters(0x4004, pulse2Duty, p2Vol, p2Note);

        const auto sweepShift = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(1.0f + patch.control2 * 2.0f)), 1, 3));
        const auto sweepPeriod = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control2 * 3.0f)), 0, 3) << 4u);
        auto pulse1Sweep = uint8_t { 0x08u };
        auto pulse2Sweep = uint8_t { 0x08u };
        if (patch.macro == MacroKind::coin || patch.macro == MacroKind::jump || patch.macro == MacroKind::powerUp)
            pulse1Sweep = static_cast<uint8_t>(0x80u | sweepPeriod | 0x08u | sweepShift);
        else if (patch.macro == MacroKind::laser || patch.macro == MacroKind::hit)
            pulse1Sweep = static_cast<uint8_t>(0x80u | sweepPeriod | sweepShift);

        writeRegister(0x4001, pulse1Sweep);
        writeRegister(0x4005, pulse2Sweep);
        writeTriangleRegisters(triNote);
        writeRegister(0x400c, nesNoiseEnvelopeValue(noiseVol));
        writeRegister(0x400e, nesNoiseRegisterForPatch(patch));
        writeRegister(0x400f, 0x18);
        regs[0x10] = static_cast<uint8_t>((regs[0x10] & 0xf0u) | static_cast<uint8_t>(patch.nesDmcRateIndex & 0x0f));
        writeStatusRegister(static_cast<uint8_t>(enable), ! suppressDmcRestartOnNoteOn);
        if (hasVrc6())
            triggerVrc6Stack(p1Note, p2Note, triNote);
        if (hasFds())
            triggerFdsStack(p1Note, p2Note, triNote);
        if (hasSunsoft5b())
            triggerSunsoft5bStack(p1Note, p2Note, triNote);
        if (hasMmc5())
            triggerMmc5Stack(p1Note, p2Note, triNote);
        if (hasVrc7())
            triggerVrc7Stack(p1Note, p2Note, triNote);
        updateTimers();
    }

    void replayHeldNote(int midiNote, float velocity) override
    {
        const auto previous = suppressDmcRestartOnNoteOn;
        suppressDmcRestartOnNoteOn = true;
        noteOn(midiNote, velocity);
        suppressDmcRestartOnNoteOn = previous;
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
        tickFrameUnits();
        tickDmcSample();

        const auto p1 = channelActive(0) ? renderPulse(0) * sourceLevel(patch, 0) : 0.0;
        const auto p2 = channelActive(1) ? renderPulse(1) * sourceLevel(patch, 1) : 0.0;
        const auto tri = triangleActive() ? renderTriangle() * sourceLevel(patch, 2) : 0.0;
        const auto noi = channelActive(3) ? renderNoise() * sourceLevel(patch, 3) : 0.0;
        const auto dmc = static_cast<double>(dmcMixerLevel());

        const auto pulseSum = p1 + p2;
        const auto tndSum = tri / 8227.0 + noi / 12241.0 + dmc / 22638.0;
        const auto pulseOut = pulseSum <= 0.0 ? 0.0 : 95.88 / ((8128.0 / pulseSum) + 100.0);
        const auto tndOut = tndSum <= 0.0 ? 0.0 : 159.79 / ((1.0 / tndSum) + 100.0);
        const auto expansionOut = hasVrc6()
            ? renderVrc6Mix()
            : (hasFds() ? renderFdsWave() : (hasSunsoft5b() ? renderSunsoft5bMix() : (hasMmc5() ? renderMmc5Mix() : (hasVrc7() ? renderVrc7Mix() : 0.0))));
        const auto mixed = static_cast<float>(applyOutputFilters(pulseOut + tndOut + (expansionOut * 0.04)) * 2.0);
        const auto outputGate = (dmcActive && noteVelocity <= 0.0f) ? 1.0f : noteVelocity;
        const auto scaled = mixed * outputGate;
        return { scaled, scaled };
    }

    std::vector<RegisterWrite> exportRegisterState() const override
    {
        std::vector<RegisterWrite> writes;
        writes.reserve(regs.size() + (hasVrc6() ? 9u : 0u) + (hasFds() ? 72u : 0u) + (hasSunsoft5b() ? 20u : 0u) + (hasMmc5() ? 8u : 0u) + (hasVrc7() ? 36u : 0u));
        for (size_t i = 0; i < regs.size(); ++i)
            writes.push_back({ 0, static_cast<uint16_t>(0x4000 + i), regs[i] });
        if (hasVrc6())
        {
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const auto base = static_cast<uint16_t>(ch == 0 ? 0x9000 : 0xa000);
                writes.push_back({ 0, base, static_cast<uint8_t>((vrc6Enabled[ch] ? 0x80u : 0x00u) | ((vrc6PulseDuty[ch] & 0x07u) << 4u) | (vrc6PulseVolume[ch] & 0x0fu)) });
                writes.push_back({ 0, static_cast<uint16_t>(base + 1u), static_cast<uint8_t>(vrc6Timer[ch] & 0xffu) });
                writes.push_back({ 0, static_cast<uint16_t>(base + 2u), static_cast<uint8_t>(((vrc6Timer[ch] >> 8u) & 0x0fu) | (vrc6Enabled[ch] ? 0x80u : 0x00u)) });
            }
            writes.push_back({ 0, 0xb000, vrc6SawRate });
            writes.push_back({ 0, 0xb001, static_cast<uint8_t>(vrc6Timer[2] & 0xffu) });
            writes.push_back({ 0, 0xb002, static_cast<uint8_t>(((vrc6Timer[2] >> 8u) & 0x0fu) | (vrc6Enabled[2] ? 0x80u : 0x00u)) });
        }
        if (hasFds())
        {
            for (size_t i = 0; i < fdsWaveRam.size(); ++i)
                writes.push_back({ 0, static_cast<uint16_t>(0x4040u + i), fdsWaveRam[i] });
            writes.push_back({ 0, 0x4080, static_cast<uint8_t>((fdsEnabled ? 0x80u : 0x00u) | (fdsVolume & 0x3fu)) });
            writes.push_back({ 0, 0x4082, static_cast<uint8_t>(fdsFrequency & 0xffu) });
            writes.push_back({ 0, 0x4083, static_cast<uint8_t>(((fdsFrequency >> 8u) & 0x0fu) | (fdsEnabled ? 0x80u : 0x00u)) });
            writes.push_back({ 0, 0x4084, static_cast<uint8_t>(fdsModDepth & 0x3fu) });
            writes.push_back({ 0, 0x4086, static_cast<uint8_t>(fdsModFrequency & 0xffu) });
            writes.push_back({ 0, 0x4087, static_cast<uint8_t>(((fdsModFrequency >> 8u) & 0x0fu) | (fdsEnabled ? 0x80u : 0x00u)) });
            writes.push_back({ 0, 0x4088, static_cast<uint8_t>(std::clamp<int>(fdsModTable[fdsModIndex] + 4, 0, 7)) });
            writes.push_back({ 0, 0x4089, static_cast<uint8_t>(0x80u | (fdsMasterVolume & 0x03u)) });
        }
        if (hasSunsoft5b())
        {
            const auto pushSunsoftReg = [&writes](uint8_t reg, uint8_t value)
            {
                writes.push_back({ 0, 0xc000, reg });
                writes.push_back({ 0, 0xe000, value });
            };

            for (uint8_t channel = 0; channel < 3; ++channel)
            {
                const auto period = sunsoft5bTonePeriod[channel] & 0x0fffu;
                pushSunsoftReg(static_cast<uint8_t>(channel * 2u), static_cast<uint8_t>(period & 0xffu));
                pushSunsoftReg(static_cast<uint8_t>(channel * 2u + 1u), static_cast<uint8_t>((period >> 8u) & 0x0fu));
            }
            pushSunsoftReg(7, sunsoft5bMixer);
            pushSunsoftReg(8, static_cast<uint8_t>(sunsoft5bVolume[0] & 0x0fu));
            pushSunsoftReg(9, static_cast<uint8_t>(sunsoft5bVolume[1] & 0x0fu));
            pushSunsoftReg(10, static_cast<uint8_t>(sunsoft5bVolume[2] & 0x0fu));
        }
        if (hasMmc5())
        {
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const auto base = static_cast<uint16_t>(ch == 0 ? 0x5000 : 0x5004);
                writes.push_back({ 0, base, static_cast<uint8_t>(((mmc5PulseDuty[ch] & 0x03u) << 6u) | 0x30u | (mmc5PulseVolume[ch] & 0x0fu)) });
                writes.push_back({ 0, static_cast<uint16_t>(base + 2u), static_cast<uint8_t>(mmc5PulseTimer[ch] & 0xffu) });
                writes.push_back({ 0, static_cast<uint16_t>(base + 3u), static_cast<uint8_t>(((mmc5PulseTimer[ch] >> 8u) & 0x07u) | (mmc5PulseEnabled[ch] ? 0x80u : 0x00u)) });
            }
            writes.push_back({ 0, 0x5010, static_cast<uint8_t>(mmc5PcmEnabled ? 0x00u : 0x00u) });
            writes.push_back({ 0, 0x5011, static_cast<uint8_t>(mmc5PcmLevel & 0x7fu) });
        }
        if (hasVrc7())
        {
            for (uint8_t reg = 0x10u; reg <= 0x35u; ++reg)
            {
                if (vrc7Regs[reg] == 0)
                    continue;

                writes.push_back({ 0, 0x9010, reg });
                writes.push_back({ 0, 0x9030, vrc7Regs[reg] });
            }
        }
        return writes;
    }

    ChipMode mode() const override { return selectedMode; }
    AccuracyMode requestedAccuracy() const override { return accuracy; }
    void setRequestedAccuracy(AccuracyMode requested) override { accuracy = requested; }
    std::string modeName() const override { return hasVrc6() ? "NES + VRC6" : (hasFds() ? "NES + FDS" : (hasSunsoft5b() ? "NES + Sunsoft 5B" : (hasMmc5() ? "NES + MMC5" : (hasVrc7() ? "NES + VRC7" : "NES / RP2A03")))); }
    std::string implementedAccuracy() const override { return "partial clean-room register-level"; }
    std::string limitations() const override
    {
        if (hasVrc6())
            return "Base RP2A03 pulse, triangle, noise, DMC, nonlinear mixer, and output filters use the existing partial clean-room NES model. VRC6 pulse 1, pulse 2, and saw lanes are clean-room musical expansion oscillators with source-card gating, Chip Poly allocation, pseudo-register export, and conservative expansion mixing; exact mapper bus timing, NSF register sequencing, expansion mixing resistor values, and hardware validation are not complete.";
        if (hasFds())
            return "Base RP2A03 pulse, triangle, noise, DMC, nonlinear mixer, and output filters use the existing partial clean-room NES model. The Famicom Disk System expansion lane is a clean-room musical 64-step 6-bit wavetable with a compact modulation table, source-card gating, Chip Poly allocation, pseudo-register export, and conservative expansion mixing; exact 2C33 register timing, wave-RAM write timing, modulation unit edge cases, master-volume nonlinearities, disk BIOS behavior, and hardware validation are not complete.";
        if (hasSunsoft5b())
            return "Base RP2A03 pulse, triangle, noise, DMC, nonlinear mixer, and output filters use the existing partial clean-room NES model. The Sunsoft 5B expansion lanes are clean-room AY-style square-tone oscillators with source-card gating, Chip Poly allocation, pseudo-register export through the $C000/$E000 mapper register ports, and conservative expansion mixing; exact Mapper 69 bus timing, PSG noise/envelope behavior, expansion mixing resistor values, and hardware validation are not complete.";
        if (hasMmc5())
            return "Base RP2A03 pulse, triangle, noise, DMC, nonlinear mixer, and output filters use the existing partial clean-room NES model. The MMC5 expansion lanes are clean-room extra pulse oscillators plus a conservative PCM/DAC thump lane with source-card gating, Chip Poly allocation across pitched lanes, pseudo-register export for $5000-$5007 and $5011, and conservative expansion mixing; exact MMC5 mapper timing, PCM read mode, IRQ behavior, expansion mixing resistor values, and hardware validation are not complete.";
        if (hasVrc7())
            return "Base RP2A03 pulse and triangle lanes use the existing partial clean-room NES model while the six VRC7 expansion melodic lanes use the vendored MIT emu2413 OPLL core with the VRC7 patch table and editable user-patch slot 0. This first slice exposes nine source-card lanes, source gating, Chip Poly allocation across pulse 1, pulse 2, triangle, and six VRC7 melodic lanes, pseudo-register export through $9010/$9030, and conservative expansion mixing. NES noise/DMC lanes, VRC7 rhythm behavior, exact mapper bus timing, native expansion mixer calibration, golden emulator comparison, and hardware validation are not complete.";
        return "Pulse, triangle, noise, timers, duty including explicit pulse 2 duty override, enable bits, simple envelopes, length counters, triangle linear counter, DMC direct DAC level, external DPCM byte stepping, basic pulse sweep updates/muting, $4017 frame-counter mode/inhibit behavior, nonlinear mixer, the documented NES output filter chain, and pulse/triangle allocation for Chip Poly play mode are approximated; exact DMC DMA/address/IRQ timing, exact frame sequencer cycle timing, advanced sweep edge cases, and hardware validation are not complete.";
    }

    std::string debugStateJson() const override
    {
        std::ostringstream json;
        json << "{"
             << "\"mode\":\"" << modeName() << "\","
             << "\"implementedAccuracy\":\"partial clean-room register-level\","
             << "\"clockHz\":" << clock << ","
             << "\"expansion\":\"" << (hasVrc6() ? "VRC6" : (hasFds() ? "FDS" : (hasSunsoft5b() ? "Sunsoft 5B" : (hasMmc5() ? "MMC5" : (hasVrc7() ? "VRC7" : "none"))))) << "\","
             << "\"macro\":\"" << toString(patch.macro) << "\","
             << "\"playMode\":\"" << toString(patch.playMode) << "\","
             << "\"pulseTimer1\":" << timer[0] << ","
             << "\"pulseTimer2\":" << timer[1] << ","
             << "\"triangleTimer\":" << timer[2] << ","
             << "\"pulseDuty1\":" << static_cast<int>(pulseDutyIndex(0)) << ","
             << "\"pulseDuty2\":" << static_cast<int>(pulseDutyIndex(1)) << ","
             << "\"pulse2DutyChoice\":" << std::clamp(patch.pulse2Duty, 0, 4) << ","
             << "\"noiseModeChoice\":" << std::clamp(patch.snNoiseMode, 0, 2) << ","
             << "\"noiseRegister\":" << static_cast<int>(regs[0x0e]) << ","
             << "\"noiseShortMode\":" << (((regs[0x0e] & 0x80u) != 0) ? 1 : 0) << ","
             << "\"noisePeriod\":" << static_cast<int>(regs[0x0e] & 0x0fu) << ","
             << "\"sourceEnabled1\":" << (sourceEnabled(patch, 0) ? 1 : 0) << ","
             << "\"sourceEnabled2\":" << (sourceEnabled(patch, 1) ? 1 : 0) << ","
             << "\"sourceEnabled3\":" << (sourceEnabled(patch, 2) ? 1 : 0) << ","
             << "\"sourceEnabled4\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
             << "\"sourceLevel1\":" << sourceLevel(patch, 0) << ","
             << "\"sourceLevel2\":" << sourceLevel(patch, 1) << ","
             << "\"sourceLevel3\":" << sourceLevel(patch, 2) << ","
             << "\"sourceLevel4\":" << sourceLevel(patch, 3) << ",";
        if (selectedMode == ChipMode::nes)
            json << "\"sourceEnabled5\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ",";
        if (hasVrc6())
        {
            json << "\"sourceEnabled5\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
                 << "\"sourceEnabled6\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
                 << "\"sourceEnabled7\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
                 << "\"sourceLevel5\":" << sourceLevel(patch, 4) << ","
                 << "\"sourceLevel6\":" << sourceLevel(patch, 5) << ","
                 << "\"sourceLevel7\":" << sourceLevel(patch, 6) << ","
                 << "\"vrc6Pulse1Timer\":" << vrc6Timer[0] << ","
                 << "\"vrc6Pulse2Timer\":" << vrc6Timer[1] << ","
                 << "\"vrc6SawTimer\":" << vrc6Timer[2] << ","
                 << "\"vrc6Pulse1Volume\":" << static_cast<int>(vrc6PulseVolume[0]) << ","
                 << "\"vrc6Pulse2Volume\":" << static_cast<int>(vrc6PulseVolume[1]) << ","
                 << "\"vrc6SawRate\":" << static_cast<int>(vrc6SawRate) << ","
                 << "\"vrc6SawAccumulator\":" << static_cast<int>(vrc6SawAccumulator) << ","
                 << "\"vrc6ActiveMask\":" << vrc6ActiveMask() << ","
                 << "\"assignedNoteVrc6Pulse1\":" << channelNotes[3] << ","
                 << "\"assignedNoteVrc6Pulse2\":" << channelNotes[4] << ","
                 << "\"assignedNoteVrc6Saw\":" << channelNotes[5] << ",";
        }
        if (hasFds())
        {
            json << "\"sourceEnabled5\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
                 << "\"sourceLevel5\":" << sourceLevel(patch, 4) << ","
                 << "\"fdsWaveEnabled\":" << (fdsEnabled ? 1 : 0) << ","
                 << "\"fdsFrequency\":" << fdsFrequency << ","
                 << "\"fdsVolume\":" << static_cast<int>(fdsVolume) << ","
                 << "\"fdsMasterVolume\":" << static_cast<int>(fdsMasterVolume) << ","
                 << "\"fdsModDepth\":" << static_cast<int>(fdsModDepth) << ","
                 << "\"fdsModFrequency\":" << fdsModFrequency << ","
                 << "\"fdsModAccumulator\":" << fdsModAccumulator << ","
                 << "\"fdsModIndex\":" << static_cast<int>(fdsModIndex) << ","
                 << "\"fdsWaveIndex\":" << currentFdsWaveIndex() << ","
                 << "\"fdsActiveMask\":" << fdsActiveMask() << ","
                 << "\"assignedNoteFdsWave\":" << channelNotes[3] << ",";
        }
        if (hasSunsoft5b())
        {
            json << "\"sourceEnabled5\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
                 << "\"sourceEnabled6\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
                 << "\"sourceEnabled7\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
                 << "\"sourceLevel5\":" << sourceLevel(patch, 4) << ","
                 << "\"sourceLevel6\":" << sourceLevel(patch, 5) << ","
                 << "\"sourceLevel7\":" << sourceLevel(patch, 6) << ","
                 << "\"sunsoft5bToneAPeriod\":" << sunsoft5bTonePeriod[0] << ","
                 << "\"sunsoft5bToneBPeriod\":" << sunsoft5bTonePeriod[1] << ","
                 << "\"sunsoft5bToneCPeriod\":" << sunsoft5bTonePeriod[2] << ","
                 << "\"sunsoft5bVolumeA\":" << static_cast<int>(sunsoft5bVolume[0]) << ","
                 << "\"sunsoft5bVolumeB\":" << static_cast<int>(sunsoft5bVolume[1]) << ","
                 << "\"sunsoft5bVolumeC\":" << static_cast<int>(sunsoft5bVolume[2]) << ","
                 << "\"sunsoft5bMixer\":" << static_cast<int>(sunsoft5bMixer) << ","
                 << "\"sunsoft5bActiveMask\":" << sunsoft5bActiveMask() << ","
                 << "\"assignedNoteSunsoft5bA\":" << channelNotes[3] << ","
                 << "\"assignedNoteSunsoft5bB\":" << channelNotes[4] << ","
                 << "\"assignedNoteSunsoft5bC\":" << channelNotes[5] << ",";
        }
        if (hasMmc5())
        {
            json << "\"sourceEnabled5\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
                 << "\"sourceEnabled6\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
                 << "\"sourceEnabled7\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
                 << "\"sourceLevel5\":" << sourceLevel(patch, 4) << ","
                 << "\"sourceLevel6\":" << sourceLevel(patch, 5) << ","
                 << "\"sourceLevel7\":" << sourceLevel(patch, 6) << ","
                 << "\"mmc5Pulse1Timer\":" << mmc5PulseTimer[0] << ","
                 << "\"mmc5Pulse2Timer\":" << mmc5PulseTimer[1] << ","
                 << "\"mmc5Pulse1Volume\":" << static_cast<int>(mmc5PulseVolume[0]) << ","
                 << "\"mmc5Pulse2Volume\":" << static_cast<int>(mmc5PulseVolume[1]) << ","
                 << "\"mmc5Pulse1Duty\":" << static_cast<int>(mmc5PulseDuty[0]) << ","
                 << "\"mmc5Pulse2Duty\":" << static_cast<int>(mmc5PulseDuty[1]) << ","
                 << "\"mmc5PcmLevel\":" << static_cast<int>(mmc5PcmLevel) << ","
                 << "\"mmc5PcmEnvelope\":" << mmc5PcmEnvelope << ","
                 << "\"mmc5ActiveMask\":" << mmc5ActiveMask() << ","
                 << "\"assignedNoteMmc5Pulse1\":" << channelNotes[3] << ","
                 << "\"assignedNoteMmc5Pulse2\":" << channelNotes[4] << ",";
        }
        if (hasVrc7())
        {
            json << "\"sourceEnabled4\":" << (sourceEnabled(patch, 3) ? 1 : 0) << ","
                 << "\"sourceEnabled5\":" << (sourceEnabled(patch, 4) ? 1 : 0) << ","
                 << "\"sourceEnabled6\":" << (sourceEnabled(patch, 5) ? 1 : 0) << ","
                 << "\"sourceEnabled7\":" << (sourceEnabled(patch, 6) ? 1 : 0) << ","
                 << "\"sourceEnabled8\":" << (sourceEnabled(patch, 7) ? 1 : 0) << ","
                 << "\"sourceEnabled9\":" << (sourceEnabled(patch, 8) ? 1 : 0) << ","
                 << "\"sourceLevel4\":" << sourceLevel(patch, 3) << ","
                 << "\"sourceLevel5\":" << sourceLevel(patch, 4) << ","
                 << "\"sourceLevel6\":" << sourceLevel(patch, 5) << ","
                 << "\"sourceLevel7\":" << sourceLevel(patch, 6) << ","
                 << "\"sourceLevel8\":" << sourceLevel(patch, 7) << ","
                 << "\"sourceLevel9\":" << sourceLevel(patch, 8) << ","
                 << "\"vrc7ClockHz\":" << vrc7ClockHz() << ","
                 << "\"vrc7InstrumentChoice\":" << std::clamp(patch.waveShape, 0, 15) << ","
                 << "\"vrc7CustomPatchEnabled\":" << (opllCustomPatchEnabledForPatch(patch) ? 1 : 0) << ","
                 << "\"vrc7CustomPatchByte0\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[0]) << ","
                 << "\"vrc7CustomPatchByte1\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[1]) << ","
                 << "\"vrc7CustomPatchByte2\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[2]) << ","
                 << "\"vrc7CustomPatchByte3\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[3]) << ","
                 << "\"vrc7CustomPatchByte4\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[4]) << ","
                 << "\"vrc7CustomPatchByte5\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[5]) << ","
                 << "\"vrc7CustomPatchByte6\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[6]) << ","
                 << "\"vrc7CustomPatchByte7\":" << static_cast<int>(opllCustomPatchBytesForPatch(patch)[7]) << ","
                 << "\"vrc7CustomModulatorMultiple\":" << static_cast<int>(opllOperatorMultipleForPatch(patch, 0)) << ","
                 << "\"vrc7CustomCarrierMultiple\":" << static_cast<int>(opllOperatorMultipleForPatch(patch, 1)) << ","
                 << "\"vrc7CustomModulatorTotalLevel\":" << static_cast<int>(opllModulatorTotalLevelForPatch(patch)) << ","
                 << "\"vrc7CustomFeedback\":" << static_cast<int>(opllFeedbackForPatch(patch)) << ","
                 << "\"vrc7Instrument0\":" << static_cast<int>(vrc7CurrentPatch[0]) << ","
                 << "\"vrc7Instrument5\":" << static_cast<int>(vrc7CurrentPatch[5]) << ","
                 << "\"vrc7Fnum0\":" << vrc7CurrentFnum[0] << ","
                 << "\"vrc7Block0\":" << static_cast<int>(vrc7CurrentBlock[0]) << ","
                 << "\"vrc7KeyOnMask\":" << static_cast<int>(vrc7KeyOnMask) << ","
                 << "\"vrc7ActiveMask\":" << vrc7ActiveMask() << ","
                 << "\"assignedNoteVrc7Ch1\":" << channelNotes[3] << ","
                 << "\"assignedNoteVrc7Ch2\":" << channelNotes[4] << ","
                 << "\"assignedNoteVrc7Ch3\":" << channelNotes[5] << ","
                 << "\"assignedNoteVrc7Ch4\":" << channelNotes[6] << ","
                 << "\"assignedNoteVrc7Ch5\":" << channelNotes[7] << ","
                 << "\"assignedNoteVrc7Ch6\":" << channelNotes[8] << ",";
        }
        json
             << "\"dmcDirectControl\":" << patch.nesDmcDirectLevel << ","
             << "\"nesDmcOnly\":" << (patch.nesDmcOnly ? 1 : 0) << ","
             << "\"dmcSampleLoaded\":" << (activeDmcSample().empty() ? 0 : 1) << ","
             << "\"dmcSampleBytes\":" << activeDmcSample().size() << ","
             << "\"dmcSampleActive\":" << (dmcActive ? 1 : 0) << ","
             << "\"dmcSampleCompleted\":" << (dmcSampleCompleted ? 1 : 0) << ","
             << "\"dmcSampleByteIndex\":" << dmcByteIndex << ","
             << "\"dmcSampleBitIndex\":" << static_cast<int>(dmcBitIndex) << ","
             << "\"dmcSampleBitsPlayed\":" << dmcBitsPlayed << ","
             << "\"dmcRateIndex\":" << static_cast<int>(dmcRateIndex()) << ","
             << "\"dmcRatePeriodCycles\":" << dmcRatePeriodCycles() << ","
             << "\"dmcLoopEnabled\":" << (dmcLoopEnabled() ? 1 : 0) << ","
             << "\"dmcControlRegister\":" << static_cast<int>(regs[0x10]) << ","
             << "\"enabled0\":" << (enabled[0] ? 1 : 0) << ","
             << "\"enabled1\":" << (enabled[1] ? 1 : 0) << ","
             << "\"enabled2\":" << (enabled[2] ? 1 : 0) << ","
             << "\"enabled3\":" << (enabled[3] ? 1 : 0) << ","
             << "\"activeChannels\":" << activeChipPolyChannels() << ","
             << "\"assignedNotePulse1\":" << channelNotes[0] << ","
             << "\"assignedNotePulse2\":" << channelNotes[1] << ","
             << "\"assignedNoteTriangle\":" << channelNotes[2] << ","
             << "\"triangleActive\":" << (triangleActive() ? 1 : 0) << ","
             << "\"linearCounter\":" << static_cast<int>(linearCounter) << ","
             << "\"linearReloadValue\":" << static_cast<int>(linearReloadValue()) << ","
             << "\"linearReloadFlag\":" << (linearReloadFlag ? 1 : 0) << ","
             << "\"linearControlFlag\":" << (linearControlFlag() ? 1 : 0) << ","
             << "\"frameCounterFiveStep\":" << (frameCounterFiveStep ? 1 : 0) << ","
             << "\"frameIrqInhibit\":" << (frameIrqInhibit ? 1 : 0) << ","
             << "\"frameIrqFlag\":" << (frameIrqFlag ? 1 : 0) << ","
             << "\"frameSequenceStep\":" << static_cast<int>(frameSequenceStep) << ","
             << "\"dmcLevel\":" << static_cast<int>(dmcOutputLevel()) << ","
             << "\"dmcMixerLevel\":" << static_cast<int>(dmcMixerLevel()) << ","
             << "\"sweepTarget0\":" << sweepTargetPeriod(0) << ","
             << "\"sweepTarget1\":" << sweepTargetPeriod(1) << ","
             << "\"sweepMuted0\":" << (pulseSweepMuted(0) ? 1 : 0) << ","
             << "\"sweepMuted1\":" << (pulseSweepMuted(1) ? 1 : 0) << ","
             << "\"sweepEnabled0\":" << (sweepUnitEnabled(0) ? 1 : 0) << ","
             << "\"sweepEnabled1\":" << (sweepUnitEnabled(1) ? 1 : 0) << ","
             << "\"sweepDivider0\":" << static_cast<int>(sweepDivider[0]) << ","
             << "\"sweepDivider1\":" << static_cast<int>(sweepDivider[1]) << ","
             << "\"envelope0\":" << static_cast<int>(envelopeVolume[0]) << ","
             << "\"envelope1\":" << static_cast<int>(envelopeVolume[1]) << ","
             << "\"envelopeNoise\":" << static_cast<int>(envelopeVolume[3]) << ","
             << "\"envelopeDecayControl\":" << patch.envelopeDecay << ","
             << "\"envelopePeriod0\":" << static_cast<int>(envelopePeriodForRegister(0x00)) << ","
             << "\"envelopePeriod1\":" << static_cast<int>(envelopePeriodForRegister(0x04)) << ","
             << "\"envelopePeriodNoise\":" << static_cast<int>(envelopePeriodForRegister(0x0c)) << ","
             << "\"envelopeConstant0\":" << (constantEnvelopeFlag(0x00) ? 1 : 0) << ","
             << "\"envelopeConstant1\":" << (constantEnvelopeFlag(0x04) ? 1 : 0) << ","
             << "\"envelopeConstantNoise\":" << (constantEnvelopeFlag(0x0c) ? 1 : 0) << ","
             << "\"length0\":" << static_cast<int>(lengthCounter[0]) << ","
             << "\"lengthTriangle\":" << static_cast<int>(lengthCounter[2]) << ","
             << "\"lengthNoise\":" << static_cast<int>(lengthCounter[3]) << ","
             << "\"limitations\":\"" << jsonEscape(limitations()) << "\""
             << "}";
        return json.str();
    }

    RuntimeTelemetry runtimeTelemetry() const noexcept override
    {
        return {
            dmcActive,
            dmcSampleCompleted,
            static_cast<int>(std::min<uint64_t>(dmcBitsPlayed, static_cast<uint64_t>(std::numeric_limits<int>::max())))
        };
    }

private:
    const std::vector<uint8_t>& activeDmcSample() const noexcept
    {
        static const std::vector<uint8_t> emptySample;
        return dmcSampleSlot >= 0 && dmcSampleSlot < static_cast<int>(dmcSampleBank.size())
            ? dmcSampleBank[static_cast<size_t>(dmcSampleSlot)]
            : emptySample;
    }

    size_t dmcSourceIndex() const noexcept
    {
        // The base RP2A03 surface has a dedicated fifth DMC lane. Expansion
        // modes retain their established source indices so existing projects
        // and their expansion-lane automation remain compatible.
        return selectedMode == ChipMode::nes ? 4u : 3u;
    }

    void updateTimers()
    {
        timer[0] = static_cast<uint16_t>(regs[0x02] | ((regs[0x03] & 0x07) << 8));
        timer[1] = static_cast<uint16_t>(regs[0x06] | ((regs[0x07] & 0x07) << 8));
        timer[2] = static_cast<uint16_t>(regs[0x0a] | ((regs[0x0b] & 0x07) << 8));
    }

    static uint8_t lengthFromIndex(uint8_t value)
    {
        static constexpr std::array<uint8_t, 32> table {
            10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
            12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
        };
        return table[(value >> 3u) & 0x1fu];
    }

    void triggerChannel(size_t channel, uint8_t value)
    {
        if (channel < lengthCounter.size())
        {
            lengthCounter[channel] = lengthFromIndex(value);
            envelopeStart[channel] = true;
        }
    }

    bool channelActive(size_t channel) const
    {
        return channel < enabled.size() && enabled[channel] && lengthCounter[channel] > 0;
    }

    bool triangleActive() const
    {
        return channelActive(2) && linearCounter > 0;
    }

    uint8_t dmcOutputLevel() const
    {
        return static_cast<uint8_t>(regs[0x11] & 0x7fu);
    }

    uint8_t dmcMixerLevel() const
    {
        // The DMC DAC holds its last output after a one-shot sample finishes.
        // Loop off stops byte stepping; it should not replace the held DAC with
        // the separate DMC Direct macro level.
        return dmcOutputLevel();
    }

    uint8_t dmcRateIndex() const
    {
        return static_cast<uint8_t>(regs[0x10] & 0x0fu);
    }

    int dmcRatePeriodCycles() const
    {
        static constexpr std::array<int, 16> ntscPeriods {
            428, 380, 340, 320, 286, 254, 226, 214,
            190, 160, 142, 128, 106, 85, 72, 54
        };
        return ntscPeriods[dmcRateIndex()];
    }

    bool dmcLoopEnabled() const
    {
        return (regs[0x10] & 0x40u) != 0;
    }

    void writeStatusRegister(uint8_t value, bool allowDmcStart)
    {
        regs[0x15] = value;
        enabled[0] = (value & 0x01) != 0;
        enabled[1] = (value & 0x02) != 0;
        enabled[2] = (value & 0x04) != 0;
        enabled[3] = (value & 0x08) != 0;
        if ((value & 0x10u) != 0)
        {
            if (allowDmcStart)
                startDmcSample();
        }
        else
        {
            stopDmcSample();
        }
        for (size_t i = 0; i < enabled.size(); ++i)
        {
            if (! enabled[i])
                lengthCounter[i] = 0;
        }
    }

    void startDmcSample()
    {
        if (activeDmcSample().empty() || dmcActive)
            return;

        dmcActive = true;
        dmcSampleCompleted = false;
        regs[0x15] = static_cast<uint8_t>(regs[0x15] | 0x10u);
        dmcByteIndex = 0;
        dmcBitIndex = 0;
        dmcPhase = 0.0;
        dmcBitsPlayed = 0;
    }

    void tickDmcSample()
    {
        if (! dmcActive || activeDmcSample().empty() || sampleRate <= 0.0)
            return;

        dmcPhase += clock / (static_cast<double>(dmcRatePeriodCycles()) * sampleRate);
        while (dmcPhase >= 1.0 && dmcActive)
        {
            dmcPhase -= 1.0;
            stepDmcBit();
        }
    }

    void stepDmcBit()
    {
        const auto& sample = activeDmcSample();
        if (dmcByteIndex >= sample.size())
        {
            if (dmcLoopEnabled())
            {
                dmcByteIndex = 0;
                dmcBitIndex = 0;
            }
            else
            {
                stopDmcSample(true);
                return;
            }
        }

        const auto byte = sample[dmcByteIndex];
        const auto bit = (byte >> dmcBitIndex) & 0x01u;
        auto level = dmcOutputLevel();
        if (bit != 0)
        {
            if (level <= 125u)
                level = static_cast<uint8_t>(level + 2u);
        }
        else if (level >= 2u)
        {
            level = static_cast<uint8_t>(level - 2u);
        }
        regs[0x11] = level;
        ++dmcBitsPlayed;

        ++dmcBitIndex;
        if (dmcBitIndex >= 8u)
        {
            dmcBitIndex = 0;
            ++dmcByteIndex;
            if (dmcByteIndex >= sample.size())
            {
                if (dmcLoopEnabled())
                {
                    dmcByteIndex = 0;
                }
                else
                {
                    stopDmcSample(true);
                }
            }
        }
    }

    void stopDmcSample(bool completed = false)
    {
        dmcActive = false;
        dmcSampleCompleted = completed;
        regs[0x15] = static_cast<uint8_t>(regs[0x15] & ~0x10u);
    }

    double highPass(double input, double cutoffHz, double& previousInput, double& previousOutput) const
    {
        if (sampleRate <= 0.0)
            return input;

        const auto rc = 1.0 / (twoPi * cutoffHz);
        const auto dt = 1.0 / sampleRate;
        const auto coefficient = rc / (rc + dt);
        const auto output = coefficient * (previousOutput + input - previousInput);
        previousInput = input;
        previousOutput = output;
        return output;
    }

    double lowPass(double input, double cutoffHz, double& previousOutput) const
    {
        if (sampleRate <= 0.0)
            return input;

        const auto rc = 1.0 / (twoPi * cutoffHz);
        const auto dt = 1.0 / sampleRate;
        const auto coefficient = dt / (rc + dt);
        previousOutput += coefficient * (input - previousOutput);
        return previousOutput;
    }

    double applyOutputFilters(double input)
    {
        auto output = highPass(input, 90.0, hp90Input, hp90Output);
        output = highPass(output, 440.0, hp440Input, hp440Output);
        return lowPass(output, 14000.0, lp14000Output);
    }

    int envelopeRegisterForChannel(size_t channel) const
    {
        if (channel == 0)
            return 0x00;
        if (channel == 1)
            return 0x04;
        return 0x0c;
    }

    void tickFrameUnits()
    {
        frameSequencerPhase += 240.0 / sampleRate;
        while (frameSequencerPhase >= 1.0)
        {
            frameSequencerPhase -= 1.0;
            tickFrameSequenceStep();
        }
    }

    void writeFrameCounter(uint8_t value)
    {
        frameCounterFiveStep = (value & 0x80u) != 0;
        frameIrqInhibit = (value & 0x40u) != 0;
        if (frameIrqInhibit)
            frameIrqFlag = false;

        frameSequencerPhase = 0.0;
        frameSequenceStep = 0;

        if (frameCounterFiveStep)
            clockQuarterAndHalfFrame();
    }

    void clockQuarterFrame()
    {
        tickEnvelopes();
        tickLinearCounter();
    }

    void clockHalfFrame()
    {
        tickSweeps();
        tickLengthCounters();
    }

    void clockQuarterAndHalfFrame()
    {
        clockQuarterFrame();
        clockHalfFrame();
    }

    void tickFrameSequenceStep()
    {
        if (frameCounterFiveStep)
        {
            switch (frameSequenceStep)
            {
                case 0:
                case 2:
                    clockQuarterFrame();
                    break;
                case 1:
                case 4:
                    clockQuarterAndHalfFrame();
                    break;
                default:
                    break;
            }
            frameSequenceStep = static_cast<uint8_t>((frameSequenceStep + 1u) % 5u);
            return;
        }

        clockQuarterFrame();
        if ((frameSequenceStep & 1u) != 0)
            clockHalfFrame();
        if (frameSequenceStep == 3 && ! frameIrqInhibit)
            frameIrqFlag = true;

        frameSequenceStep = static_cast<uint8_t>((frameSequenceStep + 1u) % 4u);
    }

    void tickEnvelopes()
    {
        for (const auto channel : { size_t(0), size_t(1), size_t(3) })
        {
            const auto reg = regs[static_cast<size_t>(envelopeRegisterForChannel(channel))];
            const auto period = static_cast<uint8_t>(reg & 0x0fu);
            const auto loop = (reg & 0x20u) != 0;

            if (envelopeStart[channel])
            {
                envelopeStart[channel] = false;
                envelopeVolume[channel] = 15;
                envelopeDivider[channel] = period;
                continue;
            }

            if (envelopeDivider[channel] > 0)
            {
                --envelopeDivider[channel];
                continue;
            }

            envelopeDivider[channel] = period;
            if (envelopeVolume[channel] > 0)
                --envelopeVolume[channel];
            else if (loop)
                envelopeVolume[channel] = 15;
        }
    }

    uint8_t linearReloadValue() const
    {
        return static_cast<uint8_t>(regs[0x08] & 0x7fu);
    }

    bool linearControlFlag() const
    {
        return (regs[0x08] & 0x80u) != 0;
    }

    void tickLinearCounter()
    {
        if (linearReloadFlag)
        {
            linearCounter = linearReloadValue();
        }
        else if (linearCounter > 0)
        {
            --linearCounter;
        }

        if (! linearControlFlag())
            linearReloadFlag = false;
    }

    void tickLengthCounters()
    {
        for (size_t channel = 0; channel < lengthCounter.size(); ++channel)
        {
            const auto haltReg = channel == 0 ? regs[0x00] : (channel == 1 ? regs[0x04] : (channel == 2 ? regs[0x08] : regs[0x0c]));
            const auto halted = (haltReg & 0x20u) != 0;
            if (! halted && lengthCounter[channel] > 0)
                --lengthCounter[channel];
        }
    }

    uint8_t sweepRegisterForChannel(size_t channel) const
    {
        return regs[channel == 0 ? 0x01 : 0x05];
    }

    uint8_t sweepPeriod(size_t channel) const
    {
        return static_cast<uint8_t>((sweepRegisterForChannel(channel) >> 4u) & 0x07u);
    }

    uint8_t sweepShift(size_t channel) const
    {
        return static_cast<uint8_t>(sweepRegisterForChannel(channel) & 0x07u);
    }

    bool sweepNegate(size_t channel) const
    {
        return (sweepRegisterForChannel(channel) & 0x08u) != 0;
    }

    bool sweepUnitEnabled(size_t channel) const
    {
        return (sweepRegisterForChannel(channel) & 0x80u) != 0 && sweepShift(channel) != 0;
    }

    int sweepTargetPeriod(size_t channel) const
    {
        if (channel >= 2)
            return 0;

        const auto current = static_cast<int>(timer[channel]);
        const auto change = current >> sweepShift(channel);
        const auto target = sweepNegate(channel)
            ? current - change - (channel == 0 ? 1 : 0)
            : current + change;

        return std::max(0, target);
    }

    bool pulseSweepMuted(size_t channel) const
    {
        return channel < 2 && (timer[channel] < 8 || sweepTargetPeriod(channel) > 0x7ff);
    }

    void setPulseTimer(size_t channel, uint16_t period)
    {
        if (channel >= 2)
            return;

        period = static_cast<uint16_t>(period & 0x07ffu);
        const auto loIndex = channel == 0 ? size_t(0x02) : size_t(0x06);
        const auto hiIndex = channel == 0 ? size_t(0x03) : size_t(0x07);
        regs[loIndex] = static_cast<uint8_t>(period & 0xffu);
        regs[hiIndex] = static_cast<uint8_t>((regs[hiIndex] & 0xf8u) | ((period >> 8u) & 0x07u));
        timer[channel] = period;
    }

    void tickSweeps()
    {
        for (size_t channel = 0; channel < 2; ++channel)
        {
            const auto dividerWasZero = sweepDivider[channel] == 0;
            if (dividerWasZero && sweepUnitEnabled(channel) && ! pulseSweepMuted(channel))
                setPulseTimer(channel, static_cast<uint16_t>(sweepTargetPeriod(channel)));

            if (dividerWasZero || sweepReload[channel])
            {
                sweepDivider[channel] = sweepPeriod(channel);
                sweepReload[channel] = false;
            }
            else
            {
                --sweepDivider[channel];
            }
        }
    }

    void writePulseRegisters(uint16_t baseAddress, NesPulseDuty duty, unsigned volume, int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto pulseTimer = static_cast<int>(std::max(0.0, std::round(clock / (16.0 * hz) - 1.0)));
        const auto constantVolume = ! envelopeDecayActive(patch)
            && (patch.macro == MacroKind::manual
                || patch.macro == MacroKind::lead
                || patch.macro == MacroKind::arp
                || patch.macro == MacroKind::bass
                || patch.macro == MacroKind::powerUp);
        const auto flags = constantVolume ? 0x10u : 0x00u;
        const auto periodOrVolume = envelopeDecayActive(patch)
            ? decayPeriodFromControl(patch.envelopeDecay, 15)
            : static_cast<uint8_t>(std::min<unsigned>(15u, volume) & 0x0fu);
        writeRegister(baseAddress, static_cast<uint8_t>((nesPulseDutyBits(duty) << 6u) | flags | periodOrVolume));
        writeRegister(static_cast<uint16_t>(baseAddress + 2), static_cast<uint8_t>(pulseTimer & 0xff));
        writeRegister(static_cast<uint16_t>(baseAddress + 3), static_cast<uint8_t>((pulseTimer >> 8) & 0x07));
    }

    void writePulseDutyRegister(uint16_t baseAddress, NesPulseDuty duty)
    {
        const auto regIndex = static_cast<size_t>(baseAddress - 0x4000);
        if (regIndex >= regs.size())
            return;

        const auto flagsAndVolume = static_cast<uint8_t>(regs[regIndex] & 0x3fu);
        writeRegister(baseAddress, static_cast<uint8_t>((nesPulseDutyBits(duty) << 6u) | flagsAndVolume));
    }

    uint8_t nesNoiseEnvelopeValue(unsigned volume) const
    {
        if (envelopeDecayActive(patch))
            return decayPeriodFromControl(patch.envelopeDecay, 15);

        return static_cast<uint8_t>(std::min<unsigned>(15u, volume));
    }

    uint8_t envelopePeriodForRegister(int regIndex) const
    {
        return static_cast<uint8_t>(regs[static_cast<size_t>(regIndex)] & 0x0fu);
    }

    bool constantEnvelopeFlag(int regIndex) const
    {
        return (regs[static_cast<size_t>(regIndex)] & 0x10u) != 0;
    }

    void writeTriangleRegisters(int midiNote)
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto triTimer = static_cast<int>(std::max(0.0, std::round(clock / (32.0 * hz) - 1.0)));
        writeRegister(0x4008, 0xff);
        writeRegister(0x400a, static_cast<uint8_t>(triTimer & 0xff));
        writeRegister(0x400b, static_cast<uint8_t>((triTimer >> 8) & 0x07));
    }

    bool hasVrc6() const
    {
        return selectedMode == ChipMode::nesVrc6;
    }

    bool hasFds() const
    {
        return selectedMode == ChipMode::nesFds;
    }

    bool hasSunsoft5b() const
    {
        return selectedMode == ChipMode::nesSunsoft5b;
    }

    bool hasMmc5() const
    {
        return selectedMode == ChipMode::nesMmc5;
    }

    bool hasVrc7() const
    {
        return selectedMode == ChipMode::nesVrc7;
    }

    size_t chipPolyVoiceCount() const
    {
        if (hasVrc7())
            return 9u;
        if (hasVrc6() || hasSunsoft5b())
            return 6u;
        if (hasMmc5())
            return 5u;
        if (hasFds())
            return 4u;
        return 3u;
    }

    size_t sourceIndexForChipPolyVoice(size_t voice) const
    {
        if (hasVrc7())
            return voice;
        static constexpr std::array<size_t, 6> vrc6Sources { 0u, 1u, 2u, 4u, 5u, 6u };
        if (hasVrc6() || hasSunsoft5b())
            return voice < vrc6Sources.size() ? vrc6Sources[voice] : voice;
        static constexpr std::array<size_t, 5> mmc5Sources { 0u, 1u, 2u, 4u, 5u };
        if (hasMmc5())
            return voice < mmc5Sources.size() ? mmc5Sources[voice] : voice;
        if (hasFds() && voice == 3u)
            return 4u;
        return voice;
    }

    int fdsWaveShapeChoice() const
    {
        const auto explicitChoice = std::clamp(patch.waveShape, 0, 4);
        if (explicitChoice > 0)
            return explicitChoice;

        switch (patch.macro)
        {
            case MacroKind::bass: return 2;
            case MacroKind::lead:
            case MacroKind::coin:
            case MacroKind::jump: return 3;
            case MacroKind::arp:
            case MacroKind::powerUp: return 4;
            case MacroKind::drum:
            case MacroKind::hit: return 1;
            case MacroKind::laser: return patch.control3 > 0.55f ? 4 : 3;
            case MacroKind::manual:
            default: return 1;
        }
    }

    void refreshFdsWaveRam()
    {
        if (! hasFds())
            return;

        const auto choice = fdsWaveShapeChoice();
        const auto skew = std::clamp(static_cast<double>(patch.control3), 0.0, 1.0);
        for (size_t i = 0; i < fdsWaveRam.size(); ++i)
        {
            const auto phaseValue = static_cast<double>(i) / static_cast<double>(fdsWaveRam.size());
            auto sample = 32;
            switch (choice)
            {
                case 2:
                    sample = i < 32u
                        ? static_cast<int>(std::round(63.0 * (static_cast<double>(i) / 31.0)))
                        : static_cast<int>(std::round(63.0 * (1.0 - static_cast<double>(i - 32u) / 31.0)));
                    break;
                case 3:
                    sample = i < static_cast<size_t>(std::round(8.0 + skew * 48.0)) ? 63 : 0;
                    break;
                case 4:
                    sample = static_cast<int>((static_cast<unsigned>(i) * 9u + static_cast<unsigned>(std::round(skew * 23.0))) & 63u);
                    break;
                case 1:
                default:
                    sample = static_cast<int>(std::round(31.5 + 31.5 * std::sin(twoPi * phaseValue)));
                    break;
            }
            fdsWaveRam[i] = static_cast<uint8_t>(std::clamp(sample, 0, 63));
        }

        refreshFdsModTable();
    }

    void refreshFdsModTable()
    {
        for (size_t i = 0; i < fdsModTable.size(); ++i)
        {
            auto value = 0;
            const auto phaseValue = static_cast<double>(i) / static_cast<double>(fdsModTable.size());
            switch (patch.macro)
            {
                case MacroKind::laser:
                    value = static_cast<int>(std::round(4.0 - phaseValue * 8.0));
                    break;
                case MacroKind::powerUp:
                case MacroKind::jump:
                    value = static_cast<int>(std::round(-3.0 + phaseValue * 6.0));
                    break;
                case MacroKind::arp:
                    value = (i % 4u) < 2u ? 2 : -2;
                    break;
                case MacroKind::hit:
                case MacroKind::drum:
                    value = (static_cast<int>(i * 5u) & 7) - 3;
                    break;
                case MacroKind::lead:
                    value = static_cast<int>(std::round(std::sin(twoPi * phaseValue) * 3.0));
                    break;
                case MacroKind::coin:
                    value = i < 8u ? 3 : (i < 16u ? 1 : -1);
                    break;
                case MacroKind::bass:
                case MacroKind::manual:
                default:
                    value = static_cast<int>(std::round(std::sin(twoPi * phaseValue) * 1.5));
                    break;
            }
            fdsModTable[i] = static_cast<int8_t>(std::clamp(value, -4, 4));
        }
    }

    uint16_t fdsFrequencyForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto reg = static_cast<int>(std::round((hz * 4194304.0) / clock));
        return static_cast<uint16_t>(std::clamp(reg, 1, 0x0fff));
    }

    double fdsHzFromRegister(uint16_t reg) const
    {
        return static_cast<double>(reg) * clock / 4194304.0;
    }

    void writeFdsWave(int midiNote, float velocity, uint8_t volume, uint8_t modDepth, int modNoteOffset)
    {
        if (! hasFds())
            return;

        refreshFdsWaveRam();
        fdsVolume = sourceEnabled(patch, 4)
            ? static_cast<uint8_t>(std::clamp<int>(volume, 0, 63))
            : uint8_t { 0u };
        fdsMasterVolume = static_cast<uint8_t>(std::clamp<int>(1 + static_cast<int>(std::round((1.0f - patch.control4) * 2.0f)), 0, 3));
        fdsModDepth = sourceEnabled(patch, 4)
            ? static_cast<uint8_t>(std::clamp<int>(modDepth, 0, 63))
            : uint8_t { 0u };
        fdsFrequency = fdsFrequencyForNote(midiNote);
        fdsModFrequency = fdsFrequencyForNote(midiNote + modNoteOffset);
        fdsEnabled = fdsVolume > 0 && sourceEnabled(patch, 4) && ! patch.nesDmcOnly && velocity > 0.0f;
        if (fdsEnabled)
        {
            fdsPhase = 0.0;
            fdsModPhase = 0.0;
            fdsModAccumulator = 0;
            fdsModIndex = 0;
        }
    }

    void triggerFdsStack(int p1Note, int p2Note, int triNote)
    {
        if (! hasFds() || patch.nesDmcOnly)
        {
            fdsEnabled = false;
            return;
        }

        auto fdsNote = p1Note + 12;
        auto volume = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(32.0f + patch.control4 * 31.0f)), 1, 63));
        auto depth = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(patch.control2 * 40.0f)), 0, 63));
        auto modOffset = 0;
        auto shouldEnable = true;

        switch (patch.macro)
        {
            case MacroKind::coin:
                fdsNote = p1Note + 19;
                volume = 48;
                depth = 14;
                modOffset = 12;
                break;
            case MacroKind::bass:
                fdsNote = triNote + 12;
                volume = 54;
                depth = 8;
                modOffset = -12;
                break;
            case MacroKind::arp:
                fdsNote = p2Note + 12;
                volume = 44;
                depth = 22;
                modOffset = 7;
                break;
            case MacroKind::drum:
                shouldEnable = false;
                break;
            case MacroKind::hit:
                fdsNote = p1Note + 7;
                volume = 42;
                depth = 30;
                modOffset = -5;
                break;
            case MacroKind::laser:
                fdsNote = p1Note + 12;
                volume = 50;
                depth = 42;
                modOffset = -12;
                break;
            case MacroKind::jump:
                fdsNote = p1Note + 12;
                volume = 46;
                depth = 18;
                modOffset = 12;
                break;
            case MacroKind::powerUp:
                fdsNote = p2Note + 12;
                volume = 48;
                depth = 32;
                modOffset = 19;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        writeFdsWave(fdsNote, shouldEnable ? noteVelocity : 0.0f, shouldEnable ? volume : 0u, shouldEnable ? depth : 0u, modOffset);
    }

    void triggerFdsVoice(int midiNote, float velocity)
    {
        const auto level = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round(clamp01(velocity) * 63.0)), 0, 63));
        const auto depth = static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((0.08f + patch.control2 * 0.72f) * 63.0f)), 0, 63));
        writeFdsWave(midiNote, velocity, level, depth, patch.macro == MacroKind::bass ? -12 : 0);
    }

    size_t currentFdsWaveIndex() const
    {
        return std::clamp(static_cast<size_t>(fdsPhase * static_cast<double>(fdsWaveRam.size())), size_t(0), fdsWaveRam.size() - 1u);
    }

    static uint8_t vrc6DutyFromNesDuty(NesPulseDuty duty, int offset)
    {
        static constexpr std::array<uint8_t, 4> duties { 1u, 2u, 4u, 6u };
        return static_cast<uint8_t>(std::clamp<int>(duties[static_cast<size_t>(duty)] + offset, 0, 7));
    }

    uint16_t vrc6TimerForNote(int midiNote, double divider) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto timerValue = static_cast<int>(std::max(1.0, std::round(clock / (divider * hz) - 1.0)));
        return static_cast<uint16_t>(std::clamp(timerValue, 1, 0x0fff));
    }

    uint8_t vrc6ExpansionMaskOrRecipe(uint8_t recipeMask) const
    {
        const auto requestedMask = sourceEnableMask(patch);
        if (requestedMask != 0u)
            return static_cast<uint8_t>(((requestedMask >> 4u) & 0x07u) & recipeMask);

        return recipeMask;
    }

    void writeVrc6Pulse(size_t channel, NesPulseDuty duty, unsigned volume, int midiNote)
    {
        if (! hasVrc6() || channel >= 2)
            return;

        const auto sourceIndex = 4u + channel;
        const auto clippedVolume = sourceEnabled(patch, sourceIndex)
            ? static_cast<uint8_t>(std::clamp<unsigned>(volume, 0u, 15u))
            : uint8_t { 0u };
        vrc6PulseDuty[channel] = vrc6DutyFromNesDuty(duty, channel == 0 ? 0 : 1);
        vrc6PulseVolume[channel] = clippedVolume;
        vrc6Timer[channel] = vrc6TimerForNote(midiNote, 16.0);
        vrc6Enabled[channel] = clippedVolume > 0;
        if (vrc6Enabled[channel])
            vrc6Phase[channel] = 0.0;
    }

    void writeVrc6Saw(unsigned rate, int midiNote)
    {
        if (! hasVrc6())
            return;

        const auto clippedRate = sourceEnabled(patch, 6)
            ? static_cast<uint8_t>(std::clamp<unsigned>(rate, 0u, 42u))
            : uint8_t { 0u };
        vrc6SawRate = clippedRate;
        vrc6Timer[2] = vrc6TimerForNote(midiNote, 14.0);
        vrc6Enabled[2] = clippedRate > 0;
        if (vrc6Enabled[2])
        {
            vrc6Phase[2] = 0.0;
            vrc6SawAccumulator = 0;
            vrc6SawStep = 0;
            vrc6SawStepPhase = 0.0;
        }
    }

    void triggerVrc6Stack(int p1Note, int p2Note, int sawNote)
    {
        if (! hasVrc6() || patch.nesDmcOnly)
        {
            vrc6Enabled.fill(false);
            return;
        }

        auto recipeMask = uint8_t { 0x07u };
        auto vrc6P1Note = p1Note + 12;
        auto vrc6P2Note = p2Note + 12;
        auto vrc6SawNote = sawNote;
        auto p1Vol = static_cast<unsigned>(std::round(8.0f + patch.control4 * 7.0f));
        auto p2Vol = static_cast<unsigned>(std::round(6.0f + patch.control4 * 6.0f));
        auto sawRate = static_cast<unsigned>(std::round(14.0f + patch.control4 * 28.0f));

        switch (patch.macro)
        {
            case MacroKind::coin:
                vrc6P1Note = p1Note + 12;
                vrc6P2Note = p1Note + 19;
                vrc6SawNote = p1Note + 24;
                p2Vol = 4u;
                sawRate = 18u;
                recipeMask = 0x03u;
                break;
            case MacroKind::bass:
                vrc6P1Note = p1Note;
                vrc6P2Note = p1Note + 12;
                vrc6SawNote = sawNote;
                p1Vol = 6u;
                p2Vol = 3u;
                sawRate = 34u;
                recipeMask = 0x05u;
                break;
            case MacroKind::drum:
                recipeMask = 0x00u;
                break;
            case MacroKind::hit:
                vrc6P1Note = p1Note + 7;
                vrc6P2Note = p1Note + 12;
                sawRate = 26u;
                recipeMask = 0x03u;
                break;
            case MacroKind::laser:
                vrc6P1Note = p1Note + 12;
                vrc6P2Note = p2Note + 7;
                vrc6SawNote = p2Note;
                sawRate = 30u;
                recipeMask = 0x07u;
                break;
            case MacroKind::jump:
                vrc6P1Note = p1Note + 12;
                p2Vol = 0u;
                sawRate = 0u;
                recipeMask = 0x01u;
                break;
            case MacroKind::powerUp:
            case MacroKind::arp:
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        const auto mask = vrc6ExpansionMaskOrRecipe(recipeMask);
        const auto duty = nesPulseDutyFromControl(patch.control1);
        writeVrc6Pulse(0, duty, (mask & 0x01u) != 0u ? p1Vol : 0u, vrc6P1Note);
        writeVrc6Pulse(1, duty, (mask & 0x02u) != 0u ? p2Vol : 0u, vrc6P2Note);
        writeVrc6Saw((mask & 0x04u) != 0u ? sawRate : 0u, vrc6SawNote);
    }

    void triggerVrc6Voice(size_t voice, int midiNote, float velocity)
    {
        const auto level = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(clamp01(velocity) * 15.0)), 0, 15));
        const auto duty = nesPulseDutyFromControl(patch.control1);
        if (voice == 0)
            writeVrc6Pulse(0, duty, level, midiNote);
        else if (voice == 1)
            writeVrc6Pulse(1, duty, level, midiNote);
        else if (voice == 2)
            writeVrc6Saw(static_cast<unsigned>(std::clamp(static_cast<int>(std::round(clamp01(velocity) * 42.0)), 1, 42)), midiNote);
    }

    uint16_t sunsoft5bTonePeriodForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto period = static_cast<int>(std::round(clock / (16.0 * hz)));
        return static_cast<uint16_t>(std::clamp(period, 1, 0x0fff));
    }

    uint8_t sunsoft5bExpansionMaskOrRecipe(uint8_t recipeMask) const
    {
        const auto requestedMask = sourceEnableMask(patch);
        if (requestedMask != 0u)
            return static_cast<uint8_t>(((requestedMask >> 4u) & 0x07u) & recipeMask);

        return recipeMask;
    }

    void refreshSunsoft5bMixer()
    {
        sunsoft5bMixer = 0x38;
        for (uint8_t channel = 0; channel < 3; ++channel)
        {
            if (! sunsoft5bEnabled[channel])
                sunsoft5bMixer = static_cast<uint8_t>(sunsoft5bMixer | (1u << channel));
        }
    }

    void writeSunsoft5bTone(size_t channel, int midiNote, unsigned volume)
    {
        if (! hasSunsoft5b() || channel >= sunsoft5bEnabled.size())
            return;

        const auto sourceIndex = 4u + channel;
        sunsoft5bTonePeriod[channel] = sunsoft5bTonePeriodForNote(midiNote);
        sunsoft5bVolume[channel] = sourceEnabled(patch, sourceIndex)
            ? static_cast<uint8_t>(std::clamp<unsigned>(volume, 0u, 15u))
            : uint8_t { 0u };
        sunsoft5bEnabled[channel] = sunsoft5bVolume[channel] > 0u
            && sourceEnabled(patch, sourceIndex)
            && ! patch.nesDmcOnly;
        if (sunsoft5bEnabled[channel])
            sunsoft5bPhase[channel] = 0.0;
        refreshSunsoft5bMixer();
    }

    void triggerSunsoft5bStack(int p1Note, int p2Note, int triNote)
    {
        if (! hasSunsoft5b() || patch.nesDmcOnly)
        {
            sunsoft5bEnabled.fill(false);
            refreshSunsoft5bMixer();
            return;
        }

        auto recipeMask = uint8_t { 0x07u };
        auto noteA = p1Note + 12;
        auto noteB = p2Note + 12;
        auto noteC = triNote + 12;
        auto volA = static_cast<unsigned>(std::round(8.0f + patch.control4 * 7.0f));
        auto volB = static_cast<unsigned>(std::round(6.0f + patch.control4 * 6.0f));
        auto volC = static_cast<unsigned>(std::round(5.0f + patch.control4 * 6.0f));

        switch (patch.macro)
        {
            case MacroKind::coin:
                noteA = p1Note + 19;
                noteB = p1Note + 24;
                noteC = p1Note + 31;
                volB = 5u;
                volC = 0u;
                recipeMask = 0x03u;
                break;
            case MacroKind::bass:
                noteA = p1Note;
                noteB = p1Note + 12;
                noteC = triNote;
                volA = 8u;
                volB = 4u;
                volC = 6u;
                recipeMask = 0x05u;
                break;
            case MacroKind::arp:
                noteA = p1Note;
                noteB = p1Note + 7;
                noteC = p1Note + 12;
                recipeMask = 0x07u;
                break;
            case MacroKind::drum:
                recipeMask = 0x00u;
                break;
            case MacroKind::hit:
                noteA = p1Note + 7;
                noteB = p1Note + 12;
                volC = 0u;
                recipeMask = 0x03u;
                break;
            case MacroKind::laser:
                noteA = p1Note + 12;
                noteB = p2Note + 7;
                noteC = p2Note;
                recipeMask = 0x07u;
                break;
            case MacroKind::jump:
                noteA = p1Note + 12;
                volB = 0u;
                volC = 0u;
                recipeMask = 0x01u;
                break;
            case MacroKind::powerUp:
                noteA = p1Note;
                noteB = p1Note + 5;
                noteC = p1Note + 12;
                recipeMask = 0x07u;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        const auto mask = sunsoft5bExpansionMaskOrRecipe(recipeMask);
        writeSunsoft5bTone(0, noteA, (mask & 0x01u) != 0u ? volA : 0u);
        writeSunsoft5bTone(1, noteB, (mask & 0x02u) != 0u ? volB : 0u);
        writeSunsoft5bTone(2, noteC, (mask & 0x04u) != 0u ? volC : 0u);
    }

    void triggerSunsoft5bVoice(size_t voice, int midiNote, float velocity)
    {
        const auto level = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(clamp01(velocity) * 15.0)), 0, 15));
        writeSunsoft5bTone(voice, midiNote, level);
    }

    uint16_t mmc5PulseTimerForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto timerValue = static_cast<int>(std::max(1.0, std::round(clock / (16.0 * hz) - 1.0)));
        return static_cast<uint16_t>(std::clamp(timerValue, 1, 0x07ff));
    }

    uint8_t mmc5ExpansionMaskOrRecipe(uint8_t recipeMask) const
    {
        const auto requestedMask = sourceEnableMask(patch);
        if (requestedMask != 0u)
            return static_cast<uint8_t>(((requestedMask >> 4u) & 0x07u) & recipeMask);

        return recipeMask;
    }

    void writeMmc5Pulse(size_t channel, NesPulseDuty duty, unsigned volume, int midiNote)
    {
        if (! hasMmc5() || channel >= mmc5PulseEnabled.size())
            return;

        const auto sourceIndex = 4u + channel;
        mmc5PulseDuty[channel] = static_cast<uint8_t>(duty);
        mmc5PulseTimer[channel] = mmc5PulseTimerForNote(midiNote);
        mmc5PulseVolume[channel] = sourceEnabled(patch, sourceIndex)
            ? static_cast<uint8_t>(std::clamp<unsigned>(volume, 0u, 15u))
            : uint8_t { 0u };
        mmc5PulseEnabled[channel] = mmc5PulseVolume[channel] > 0u
            && sourceEnabled(patch, sourceIndex)
            && ! patch.nesDmcOnly;
        if (mmc5PulseEnabled[channel])
            mmc5Phase[channel] = 0.0;
    }

    void writeMmc5Pcm(unsigned level)
    {
        if (! hasMmc5())
            return;

        const auto enabledPcm = sourceEnabled(patch, 6) && ! patch.nesDmcOnly && level > 0u;
        mmc5PcmLevel = enabledPcm
            ? static_cast<uint8_t>(std::clamp<unsigned>(level, 0u, 127u))
            : uint8_t { 64u };
        mmc5PcmEnvelope = enabledPcm ? 1.0 : 0.0;
        mmc5PcmEnabled = enabledPcm;
    }

    void triggerMmc5Stack(int p1Note, int p2Note, int triNote)
    {
        if (! hasMmc5() || patch.nesDmcOnly)
        {
            mmc5PulseEnabled.fill(false);
            writeMmc5Pcm(0u);
            return;
        }

        auto recipeMask = uint8_t { 0x03u };
        auto mmc5P1Note = p1Note + 12;
        auto mmc5P2Note = p2Note + 12;
        auto p1Vol = static_cast<unsigned>(std::round(8.0f + patch.control4 * 7.0f));
        auto p2Vol = static_cast<unsigned>(std::round(6.0f + patch.control4 * 6.0f));
        auto pcmLevel = 0u;

        switch (patch.macro)
        {
            case MacroKind::coin:
                mmc5P1Note = p1Note + 19;
                mmc5P2Note = p1Note + 24;
                p2Vol = 4u;
                pcmLevel = 100u;
                recipeMask = 0x07u;
                break;
            case MacroKind::bass:
                mmc5P1Note = p1Note;
                mmc5P2Note = triNote + 12;
                p1Vol = 9u;
                p2Vol = 4u;
                recipeMask = 0x03u;
                break;
            case MacroKind::arp:
                mmc5P1Note = p1Note + 7;
                mmc5P2Note = p1Note + 12;
                recipeMask = 0x03u;
                break;
            case MacroKind::drum:
                mmc5P1Note = p1Note - 12;
                p1Vol = 5u;
                p2Vol = 0u;
                pcmLevel = 122u;
                recipeMask = 0x05u;
                break;
            case MacroKind::hit:
                mmc5P1Note = p1Note + 7;
                p2Vol = 0u;
                pcmLevel = 116u;
                recipeMask = 0x05u;
                break;
            case MacroKind::laser:
                mmc5P1Note = p1Note + 12;
                mmc5P2Note = p2Note + 7;
                pcmLevel = 88u;
                recipeMask = 0x07u;
                break;
            case MacroKind::jump:
                mmc5P1Note = p1Note + 12;
                p2Vol = 0u;
                recipeMask = 0x01u;
                break;
            case MacroKind::powerUp:
                mmc5P1Note = p1Note + 5;
                mmc5P2Note = p1Note + 12;
                recipeMask = 0x03u;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        const auto mask = mmc5ExpansionMaskOrRecipe(recipeMask);
        const auto duty = nesPulseDutyFromControl(patch.control1);
        writeMmc5Pulse(0, duty, (mask & 0x01u) != 0u ? p1Vol : 0u, mmc5P1Note);
        writeMmc5Pulse(1, duty, (mask & 0x02u) != 0u ? p2Vol : 0u, mmc5P2Note);
        writeMmc5Pcm((mask & 0x04u) != 0u ? pcmLevel : 0u);
    }

    void triggerMmc5Voice(size_t voice, int midiNote, float velocity)
    {
        const auto level = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(clamp01(velocity) * 15.0)), 0, 15));
        const auto duty = voice == 0 ? nesPulseDutyFromControl(patch.control1) : nesPulse2DutyForPatch(patch, nesPulseDutyFromControl(patch.control1), false);
        writeMmc5Pulse(voice, duty, level, midiNote);
    }

    struct Vrc7Pitch
    {
        uint16_t fnum = 0;
        uint8_t block = 0;
    };

    double vrc7ClockHz() const
    {
        return clock * 2.0;
    }

    Vrc7Pitch vrc7PitchForNote(int midiNote) const
    {
        const auto hz = midiNoteToHz(std::clamp(midiNote, 0, 127));
        const auto base = (hz * 524288.0) / (vrc7ClockHz() / 72.0);
        auto block = 0;
        auto fnum = base;
        while (fnum > 511.0 && block < 7)
        {
            fnum *= 0.5;
            ++block;
        }
        while (fnum < 256.0 && block > 0)
        {
            fnum *= 2.0;
            --block;
        }

        return {
            static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(fnum)), 1, 511)),
            static_cast<uint8_t>(std::clamp(block, 0, 7))
        };
    }

    uint8_t vrc7InstrumentForPatch() const
    {
        return ::chipper::vrc7InstrumentForPatch(patch);
    }

    uint8_t vrc7VolumeForChannel(size_t channel, float velocity) const
    {
        const auto sourceIndex = 3u + channel;
        const auto carrierTrim = opllCustomPatchEnabledForPatch(patch)
            ? std::clamp(0.5f + static_cast<float>(clamp01(patch.fmOperatorLevels[1])), 0.25f, 1.5f)
            : 1.0f;
        const auto musicalLevel = std::clamp(static_cast<float>(clamp01(patch.control4) * clamp01(velocity) * sourceLevel(patch, sourceIndex) * carrierTrim), 0.0f, 1.0f);
        return static_cast<uint8_t>(std::clamp(static_cast<int>(std::round((1.0 - musicalLevel) * 15.0)), 0, 15));
    }

    void writeVrc7Register(uint8_t reg, uint8_t value)
    {
        const auto clippedReg = static_cast<uint8_t>(reg & 0x3fu);
        vrc7Regs[clippedReg] = value;
        if (vrc7Opll != nullptr)
            OPLL_writeReg(vrc7Opll, clippedReg, value);
    }

    void applyVrc7CustomPatchIfNeeded()
    {
        if (! hasVrc7() || ! opllCustomPatchEnabledForPatch(patch))
            return;

        const auto bytes = opllCustomPatchBytesForPatch(patch);
        for (size_t reg = 0; reg < bytes.size(); ++reg)
            writeVrc7Register(static_cast<uint8_t>(reg), bytes[reg]);
    }

    void triggerVrc7Channel(size_t channel, int midiNote, float velocity, bool shouldEnable)
    {
        if (! hasVrc7() || channel >= 6u || vrc7Opll == nullptr)
            return;

        const auto sourceIndex = 3u + channel;
        const auto detune = static_cast<int>(std::round((patch.control2 - 0.5f) * 12.0f));
        const auto pitch = vrc7PitchForNote(midiNote + detune);
        applyVrc7CustomPatchIfNeeded();
        const auto instrument = vrc7InstrumentForPatch();
        const auto volume = vrc7VolumeForChannel(channel, velocity);
        const auto keyBit = shouldEnable && sourceEnabled(patch, sourceIndex) && volume < 15 ? 0x10u : 0x00u;
        const auto channelReg = static_cast<uint8_t>(channel);

        vrc7CurrentPatch[channel] = instrument;
        vrc7CurrentFnum[channel] = pitch.fnum;
        vrc7CurrentBlock[channel] = pitch.block;

        writeVrc7Register(static_cast<uint8_t>(0x30u + channelReg), static_cast<uint8_t>((instrument << 4u) | volume));
        writeVrc7Register(static_cast<uint8_t>(0x10u + channelReg), static_cast<uint8_t>(pitch.fnum & 0xffu));
        writeVrc7Register(static_cast<uint8_t>(0x20u + channelReg),
                          static_cast<uint8_t>(keyBit | ((pitch.block & 0x07u) << 1u) | ((pitch.fnum >> 8u) & 0x01u)));

        if (keyBit != 0)
            vrc7KeyOnMask = static_cast<uint8_t>(vrc7KeyOnMask | (1u << channel));
        else
            vrc7KeyOnMask = static_cast<uint8_t>(vrc7KeyOnMask & ~(1u << channel));
    }

    void keyOffVrc7Channel(size_t channel)
    {
        if (! hasVrc7() || channel >= 6u)
            return;

        const auto reg = static_cast<uint8_t>(0x20u + channel);
        writeVrc7Register(reg, static_cast<uint8_t>(vrc7Regs[reg] & static_cast<uint8_t>(~0x10u)));
        vrc7KeyOnMask = static_cast<uint8_t>(vrc7KeyOnMask & ~(1u << channel));
    }

    uint8_t vrc7ExpansionMask(uint8_t recipeMask) const
    {
        uint8_t mask = 0;
        for (size_t channel = 0; channel < 6u; ++channel)
        {
            if (sourceEnabled(patch, 3u + channel))
                mask = static_cast<uint8_t>(mask | (1u << channel));
        }
        return static_cast<uint8_t>(mask & recipeMask);
    }

    void triggerVrc7Stack(int p1Note, int p2Note, int triNote)
    {
        if (! hasVrc7() || patch.nesDmcOnly)
        {
            for (size_t channel = 0; channel < 6u; ++channel)
                keyOffVrc7Channel(channel);
            return;
        }

        auto recipeMask = uint8_t { 0x3fu };
        std::array<int, 6> notes { p1Note + 12, p2Note + 12, triNote + 24, p1Note + 19, p2Note + 19, triNote + 31 };

        switch (patch.macro)
        {
            case MacroKind::coin:
                notes = { p1Note + 24, p1Note + 31, p1Note + 36, p1Note + 43, p1Note + 48, p1Note + 55 };
                recipeMask = 0x09u;
                break;
            case MacroKind::bass:
                notes = { p1Note, triNote, p1Note + 12, p1Note + 19, triNote + 12, p1Note + 24 };
                recipeMask = 0x15u;
                break;
            case MacroKind::arp:
                notes = { p1Note, p1Note + 4, p1Note + 7, p1Note + 12, p1Note + 16, p1Note + 19 };
                recipeMask = 0x3fu;
                break;
            case MacroKind::drum:
                notes = { p1Note - 12, p1Note - 5, p1Note, p1Note + 7, p1Note + 12, p1Note + 19 };
                recipeMask = 0x15u;
                break;
            case MacroKind::hit:
                notes = { p1Note - 5, p1Note + 7, p1Note + 12, p1Note + 19, p1Note + 24, p1Note + 31 };
                recipeMask = 0x0fu;
                break;
            case MacroKind::laser:
                notes = { p1Note + 24, p2Note + 12, p1Note, p2Note - 12, p1Note + 31, p2Note + 19 };
                recipeMask = 0x33u;
                break;
            case MacroKind::jump:
                notes = { p1Note + 24, p1Note + 31, p1Note + 36, p1Note + 43, p1Note + 48, p1Note + 55 };
                recipeMask = 0x01u;
                break;
            case MacroKind::powerUp:
                notes = { p1Note, p1Note + 5, p1Note + 12, p1Note + 17, p1Note + 24, p1Note + 29 };
                recipeMask = 0x3fu;
                break;
            case MacroKind::lead:
            case MacroKind::manual:
            default:
                break;
        }

        const auto mask = vrc7ExpansionMask(recipeMask);
        for (size_t channel = 0; channel < notes.size(); ++channel)
            triggerVrc7Channel(channel, notes[channel], noteVelocity, (mask & (1u << channel)) != 0u);
    }

    void triggerVrc7Voice(size_t voice, int midiNote, float velocity)
    {
        triggerVrc7Channel(voice, midiNote, velocity, true);
    }

    int vrc7ActiveMask() const
    {
        return static_cast<int>(vrc7KeyOnMask & 0x3fu);
    }

    int selectChipPolyChannel(int midiNote) const
    {
        const auto voiceCount = chipPolyVoiceCount();
        for (size_t channel = 0; channel < voiceCount; ++channel)
        {
            if (! sourceEnabled(patch, sourceIndexForChipPolyVoice(channel)))
                continue;

            if (channelNotes[channel] == midiNote)
                return static_cast<int>(channel);
        }

        for (size_t channel = 0; channel < voiceCount; ++channel)
        {
            if (! sourceEnabled(patch, sourceIndexForChipPolyVoice(channel)))
                continue;

            if (channelNotes[channel] < 0)
                return static_cast<int>(channel);
        }

        uint64_t oldestStamp = std::numeric_limits<uint64_t>::max();
        auto oldestVoice = -1;
        for (size_t channel = 0; channel < voiceCount; ++channel)
        {
            if (! sourceEnabled(patch, sourceIndexForChipPolyVoice(channel)))
                continue;

            if (channelStamp[channel] < oldestStamp)
            {
                oldestStamp = channelStamp[channel];
                oldestVoice = static_cast<int>(channel);
            }
        }

        return oldestVoice;
    }

    int activeChipPolyChannels() const
    {
        const auto voiceCount = chipPolyVoiceCount();
        int active = 0;
        for (size_t channel = 0; channel < voiceCount; ++channel)
        {
            if (sourceEnabled(patch, sourceIndexForChipPolyVoice(channel)) && channelNotes[channel] >= 0)
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
        for (auto& length : lengthCounter)
            length = 0;
        vrc6Enabled.fill(false);
        fdsEnabled = false;
        sunsoft5bEnabled.fill(false);
        mmc5PulseEnabled.fill(false);
        mmc5PcmEnabled = false;
        mmc5PcmEnvelope = 0.0;
        for (size_t channel = 0; channel < 6u; ++channel)
            keyOffVrc7Channel(channel);
        refreshSunsoft5bMixer();
        linearCounter = 0;
        linearReloadFlag = false;
    }

    void noteOnChipPoly(int midiNote, float velocity)
    {
        auto enable = uint8_t { 0u };
        if (sourceEnabled(patch, 0))
            enable = static_cast<uint8_t>(enable | 0x01u);
        if (sourceEnabled(patch, 1))
            enable = static_cast<uint8_t>(enable | 0x02u);
        if (sourceEnabled(patch, 2))
            enable = static_cast<uint8_t>(enable | 0x04u);
        if (! hasVrc7() && sourceEnabled(patch, 3))
            enable = static_cast<uint8_t>(enable | 0x08u);
        if (! hasVrc7() && ! activeDmcSample().empty() && sourceEnabled(patch, dmcSourceIndex()))
            enable = static_cast<uint8_t>(enable | 0x10u);
        regs[0x10] = static_cast<uint8_t>((regs[0x10] & 0xf0u) | static_cast<uint8_t>(patch.nesDmcRateIndex & 0x0f));
        writeStatusRegister(enable, ! suppressDmcRestartOnNoteOn);

        const auto channel = selectChipPolyChannel(midiNote);
        if (channel < 0)
        {
            noteVelocity = dmcActive ? static_cast<float>(clamp01(velocity)) : 0.0f;
            updateTimers();
            return;
        }

        const auto index = static_cast<size_t>(channel);
        channelNotes[index] = std::clamp(midiNote, 0, 127);
        channelVelocity[index] = static_cast<float>(clamp01(velocity));
        channelStamp[index] = ++noteStamp;

        if (channel == 0)
        {
            const auto duty = nesPulseDutyFromControl(patch.control1);
            const auto volume = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
            writeRegister(0x4001, 0x08);
            writePulseRegisters(0x4000, duty, volume, channelNotes[index]);
        }
        else if (channel == 1)
        {
            const auto duty = nesPulse2DutyForPatch(patch, nesPulseDutyFromControl(patch.control1), false);
            const auto volume = static_cast<unsigned>(std::clamp(static_cast<int>(std::round(channelVelocity[index] * 15.0f)), 0, 15));
            writeRegister(0x4005, 0x08);
            writePulseRegisters(0x4004, duty, volume, channelNotes[index]);
        }
        else if (channel == 2)
        {
            writeTriangleRegisters(channelNotes[index]);
        }
        else
        {
            if (hasVrc6())
                triggerVrc6Voice(static_cast<size_t>(channel - 3), channelNotes[index], channelVelocity[index]);
            else if (hasSunsoft5b())
                triggerSunsoft5bVoice(static_cast<size_t>(channel - 3), channelNotes[index], channelVelocity[index]);
            else if (hasMmc5())
                triggerMmc5Voice(static_cast<size_t>(channel - 3), channelNotes[index], channelVelocity[index]);
            else if (hasVrc7())
                triggerVrc7Voice(static_cast<size_t>(channel - 3), channelNotes[index], channelVelocity[index]);
            else if (hasFds() && channel == 3)
                triggerFdsVoice(channelNotes[index], channelVelocity[index]);
        }

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
            if (channel < 3u)
                lengthCounter[channel] = 0;
            if (hasVrc6() && channel >= 3u && channel - 3u < vrc6Enabled.size())
                vrc6Enabled[channel - 3u] = false;
            if (hasSunsoft5b() && channel >= 3u && channel - 3u < sunsoft5bEnabled.size())
            {
                sunsoft5bEnabled[channel - 3u] = false;
                refreshSunsoft5bMixer();
            }
            if (hasMmc5() && channel >= 3u && channel - 3u < mmc5PulseEnabled.size())
                mmc5PulseEnabled[channel - 3u] = false;
            if (hasVrc7() && channel >= 3u && channel - 3u < 6u)
                keyOffVrc7Channel(channel - 3u);
            if (hasFds() && channel == 3u)
                fdsEnabled = false;
            if (channel == 2)
            {
                linearCounter = 0;
                linearReloadFlag = false;
            }
        }

        noteVelocity = activeChipPolyChannels() > 0 ? 1.0f : 0.0f;
    }

    void refreshActivePulseDuties()
    {
        const auto pulse1Duty = nesPulseDutyFromControl(patch.control1);

        if (patch.playMode == PlayMode::chipPoly)
        {
            if (channelNotes[0] >= 0)
                writePulseDutyRegister(0x4000, pulse1Duty);
            if (channelNotes[1] >= 0)
                writePulseDutyRegister(0x4004, nesPulse2DutyForPatch(patch, pulse1Duty, false));
            if (hasVrc6())
            {
                if (channelNotes[3] >= 0)
                    vrc6PulseDuty[0] = vrc6DutyFromNesDuty(pulse1Duty, 0);
                if (channelNotes[4] >= 0)
                    vrc6PulseDuty[1] = vrc6DutyFromNesDuty(pulse1Duty, 1);
            }
            if (hasMmc5())
            {
                if (channelNotes[3] >= 0)
                    mmc5PulseDuty[0] = static_cast<uint8_t>(pulse1Duty);
                if (channelNotes[4] >= 0)
                    mmc5PulseDuty[1] = static_cast<uint8_t>(nesPulse2DutyForPatch(patch, pulse1Duty, false));
            }
            return;
        }

        if (heldNote < 0)
            return;

        if (enabled[0])
            writePulseDutyRegister(0x4000, pulse1Duty);
        if (enabled[1])
            writePulseDutyRegister(0x4004, nesPulse2DutyForPatch(patch, pulse1Duty, true));
        if (hasVrc6())
        {
            vrc6PulseDuty[0] = vrc6DutyFromNesDuty(pulse1Duty, 0);
            vrc6PulseDuty[1] = vrc6DutyFromNesDuty(pulse1Duty, 1);
        }
        if (hasMmc5())
        {
            mmc5PulseDuty[0] = static_cast<uint8_t>(pulse1Duty);
            mmc5PulseDuty[1] = static_cast<uint8_t>(nesPulse2DutyForPatch(patch, pulse1Duty, true));
        }
        if (hasFds())
            refreshFdsWaveRam();
    }

    uint8_t pulseDutyIndex(int index) const
    {
        const auto regIndex = index == 0 ? 0x00 : 0x04;
        return static_cast<uint8_t>((regs[regIndex] >> 6u) & 0x03u);
    }

    double dutyForPulse(int index) const
    {
        static constexpr std::array<double, 4> duty = { 0.125, 0.25, 0.5, 0.75 };
        return duty[pulseDutyIndex(index)];
    }

    double constantVolume(int regIndex) const
    {
        const auto reg = regs[static_cast<size_t>(regIndex)];
        if ((reg & 0x10u) != 0)
            return static_cast<double>(reg & 0x0f) / 15.0;

        const auto channel = regIndex == 0x00 ? size_t(0) : (regIndex == 0x04 ? size_t(1) : size_t(3));
        return static_cast<double>(envelopeVolume[channel]) / 15.0;
    }

    double renderPulse(int index)
    {
        if (pulseSweepMuted(static_cast<size_t>(index)))
            return 0.0;

        const auto period = timer[index] + 1;
        const auto hz = clock / (16.0 * static_cast<double>(period));
        phase[index] = wrapPhase(phase[index] + hz / sampleRate);
        const auto value = phase[index] < dutyForPulse(index) ? 1.0 : 0.0;
        return value * 15.0 * constantVolume(index == 0 ? 0x00 : 0x04);
    }

    double renderTriangle()
    {
        static constexpr std::array<double, 32> sequence {
            15, 14, 13, 12, 11, 10, 9, 8,
            7, 6, 5, 4, 3, 2, 1, 0,
            0, 1, 2, 3, 4, 5, 6, 7,
            8, 9, 10, 11, 12, 13, 14, 15
        };

        const auto period = timer[2] + 1;
        const auto hz = clock / (32.0 * static_cast<double>(period));
        phase[2] = wrapPhase(phase[2] + hz / sampleRate);
        const auto step = std::clamp(static_cast<size_t>(phase[2] * sequence.size()), size_t(0), sequence.size() - 1);
        return sequence[step];
    }

    double renderNoise()
    {
        static constexpr std::array<int, 16> ntscPeriods = {
            4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
        };

        const auto noiseReg = regs[0x0e];
        const auto period = ntscPeriods[noiseReg & 0x0f];
        const auto hz = clock / static_cast<double>(period);
        phase[3] += hz / sampleRate;

        while (phase[3] >= 1.0)
        {
            phase[3] -= 1.0;
            const auto tap = (noiseReg & 0x80) != 0 ? 6u : 1u;
            const auto feedback = (lfsr & 1u) ^ ((lfsr >> tap) & 1u);
            lfsr = static_cast<uint16_t>((lfsr >> 1u) | (feedback << 14u));
            if (lfsr == 0)
                lfsr = 1;
        }

        const auto value = (~lfsr) & 1u;
        return static_cast<double>(value) * 15.0 * constantVolume(0x0c);
    }

    double renderVrc6Pulse(size_t channel)
    {
        if (channel >= 2 || ! vrc6Enabled[channel] || ! sourceEnabled(patch, 4u + channel))
            return 0.0;

        const auto period = static_cast<double>(std::max<uint16_t>(1u, static_cast<uint16_t>(vrc6Timer[channel] + 1u)));
        const auto hz = clock / (16.0 * period);
        vrc6Phase[channel] = wrapPhase(vrc6Phase[channel] + hz / sampleRate);
        const auto duty = (static_cast<double>(vrc6PulseDuty[channel]) + 1.0) / 16.0;
        const auto amp = static_cast<double>(vrc6PulseVolume[channel]) / 15.0;
        return (vrc6Phase[channel] < duty ? 1.0 : -1.0) * amp * sourceLevel(patch, 4u + channel);
    }

    double renderVrc6Saw()
    {
        if (! vrc6Enabled[2] || ! sourceEnabled(patch, 6))
            return 0.0;

        const auto period = static_cast<double>(std::max<uint16_t>(1u, static_cast<uint16_t>(vrc6Timer[2] + 1u)));
        const auto hz = clock / (14.0 * period);
        vrc6SawStepPhase += (hz * 14.0) / sampleRate;
        while (vrc6SawStepPhase >= 1.0)
        {
            vrc6SawStepPhase -= 1.0;
            vrc6SawStep = static_cast<uint8_t>((vrc6SawStep + 1u) % 14u);
            if (vrc6SawStep == 0)
                vrc6SawAccumulator = 0;
            else if ((vrc6SawStep & 1u) == 0)
                vrc6SawAccumulator = static_cast<uint16_t>(std::min<int>(255, static_cast<int>(vrc6SawAccumulator) + static_cast<int>(vrc6SawRate)));
        }

        const auto stepped = static_cast<double>((vrc6SawAccumulator >> 3u) & 0x1fu) / 31.0;
        return ((stepped * 2.0) - 1.0) * sourceLevel(patch, 6);
    }

    int vrc6ActiveMask() const
    {
        return (vrc6Enabled[0] ? 0x01 : 0)
            | (vrc6Enabled[1] ? 0x02 : 0)
            | (vrc6Enabled[2] ? 0x04 : 0);
    }

    double renderVrc6Mix()
    {
        return renderVrc6Pulse(0) + renderVrc6Pulse(1) + renderVrc6Saw();
    }

    int fdsActiveMask() const
    {
        return fdsEnabled ? 0x01 : 0;
    }

    double renderFdsWave()
    {
        if (! hasFds() || ! fdsEnabled || ! sourceEnabled(patch, 4) || sampleRate <= 0.0)
            return 0.0;

        if (fdsModFrequency > 0 && fdsModDepth > 0)
        {
            fdsModPhase = wrapPhase(fdsModPhase + fdsHzFromRegister(fdsModFrequency) / sampleRate);
            fdsModIndex = static_cast<size_t>(std::clamp(static_cast<int>(std::floor(fdsModPhase * fdsModTable.size())), 0, static_cast<int>(fdsModTable.size() - 1u)));
            fdsModAccumulator = static_cast<int>(fdsModTable[fdsModIndex]) * static_cast<int>(fdsModDepth);
        }
        else
        {
            fdsModAccumulator = 0;
        }

        const auto bendSemitones = std::clamp(static_cast<double>(fdsModAccumulator) / 256.0, -6.0, 6.0);
        const auto hz = fdsHzFromRegister(fdsFrequency) * std::pow(2.0, bendSemitones / 12.0);
        fdsPhase = wrapPhase(fdsPhase + hz / sampleRate);

        const auto sample = static_cast<double>(fdsWaveRam[currentFdsWaveIndex()]) / 63.0;
        const std::array<double, 4> masterScale { 1.0, 0.75, 0.50, 0.37 };
        const auto volume = static_cast<double>(fdsVolume) / 63.0;
        return ((sample * 2.0) - 1.0) * volume * masterScale[fdsMasterVolume & 0x03u] * sourceLevel(patch, 4);
    }

    int sunsoft5bActiveMask() const
    {
        return (sunsoft5bEnabled[0] ? 0x01 : 0)
            | (sunsoft5bEnabled[1] ? 0x02 : 0)
            | (sunsoft5bEnabled[2] ? 0x04 : 0);
    }

    double renderSunsoft5bTone(size_t channel)
    {
        if (! hasSunsoft5b() || channel >= sunsoft5bEnabled.size() || ! sunsoft5bEnabled[channel] || ! sourceEnabled(patch, 4u + channel) || sampleRate <= 0.0)
            return 0.0;

        const auto period = static_cast<double>(std::max<uint16_t>(1u, sunsoft5bTonePeriod[channel]));
        const auto hz = clock / (16.0 * period);
        sunsoft5bPhase[channel] = wrapPhase(sunsoft5bPhase[channel] + hz / sampleRate);
        const auto amplitude = static_cast<double>(sunsoft5bVolume[channel]) / 15.0;
        return (sunsoft5bPhase[channel] < 0.5 ? 1.0 : -1.0) * amplitude * sourceLevel(patch, 4u + channel);
    }

    double renderSunsoft5bMix()
    {
        return renderSunsoft5bTone(0) + renderSunsoft5bTone(1) + renderSunsoft5bTone(2);
    }

    int mmc5ActiveMask() const
    {
        return (mmc5PulseEnabled[0] ? 0x01 : 0)
            | (mmc5PulseEnabled[1] ? 0x02 : 0)
            | (mmc5PcmEnabled && mmc5PcmEnvelope > 0.001 ? 0x04 : 0);
    }

    double renderMmc5Pulse(size_t channel)
    {
        if (! hasMmc5() || channel >= mmc5PulseEnabled.size() || ! mmc5PulseEnabled[channel] || ! sourceEnabled(patch, 4u + channel) || sampleRate <= 0.0)
            return 0.0;

        const auto period = static_cast<double>(std::max<uint16_t>(1u, static_cast<uint16_t>(mmc5PulseTimer[channel] + 1u)));
        const auto hz = clock / (16.0 * period);
        mmc5Phase[channel] = wrapPhase(mmc5Phase[channel] + hz / sampleRate);
        static constexpr std::array<double, 4> duties { 0.125, 0.25, 0.5, 0.75 };
        const auto duty = duties[std::clamp<size_t>(mmc5PulseDuty[channel], 0u, duties.size() - 1u)];
        const auto amp = static_cast<double>(mmc5PulseVolume[channel]) / 15.0;
        return (mmc5Phase[channel] < duty ? 1.0 : -1.0) * amp * sourceLevel(patch, 4u + channel);
    }

    double renderMmc5Pcm()
    {
        if (! hasMmc5() || ! mmc5PcmEnabled || ! sourceEnabled(patch, 6) || sampleRate <= 0.0)
            return 0.0;

        const auto level = (static_cast<double>(mmc5PcmLevel) / 63.5) - 1.0;
        const auto value = level * mmc5PcmEnvelope * sourceLevel(patch, 6);
        const auto decaySamples = std::max(1.0, sampleRate * (0.030 + 0.090 * static_cast<double>(patch.envelopeDecay)));
        mmc5PcmEnvelope = std::max(0.0, mmc5PcmEnvelope - (1.0 / decaySamples));
        if (mmc5PcmEnvelope <= 0.001)
        {
            mmc5PcmEnvelope = 0.0;
            mmc5PcmEnabled = false;
        }
        return value;
    }

    double renderMmc5Mix()
    {
        return renderMmc5Pulse(0) + renderMmc5Pulse(1) + renderMmc5Pcm();
    }

    double renderVrc7Mix()
    {
        if (! hasVrc7() || vrc7Opll == nullptr)
            return 0.0;

        int32_t stereo[2] {};
        OPLL_calcStereo(vrc7Opll, stereo);
        constexpr auto scale = 1.0 / 8192.0;
        return std::clamp((static_cast<double>(stereo[0]) + static_cast<double>(stereo[1])) * 0.5 * scale, -1.0, 1.0);
    }

    AccuracyMode accuracy;
    ChipMode selectedMode = ChipMode::nes;
    double sampleRate = 48000.0;
    double clock = 1789773.0;
    std::array<uint8_t, 0x18> regs {};
    std::array<double, 4> phase {};
    std::array<uint16_t, 3> timer {};
    std::array<bool, 4> enabled {};
    std::array<uint8_t, 4> envelopeVolume {};
    std::array<uint8_t, 4> envelopeDivider {};
    std::array<bool, 4> envelopeStart {};
    std::array<uint8_t, 4> lengthCounter {};
    std::array<uint8_t, 2> sweepDivider {};
    std::array<bool, 2> sweepReload {};
    uint8_t linearCounter = 0;
    bool linearReloadFlag = false;
    double frameSequencerPhase = 0.0;
    uint8_t frameSequenceStep = 0;
    bool frameCounterFiveStep = false;
    bool frameIrqInhibit = false;
    bool frameIrqFlag = false;
    double hp90Input = 0.0;
    double hp90Output = 0.0;
    double hp440Input = 0.0;
    double hp440Output = 0.0;
    double lp14000Output = 0.0;
    uint16_t lfsr = 1;
    int heldNote = -1;
    float noteVelocity = 0.0f;
    std::array<int, 9> channelNotes { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    std::array<float, 9> channelVelocity {};
    std::array<uint64_t, 9> channelStamp {};
    std::array<double, 3> vrc6Phase {};
    std::array<uint16_t, 3> vrc6Timer {};
    std::array<bool, 3> vrc6Enabled {};
    std::array<uint8_t, 2> vrc6PulseVolume {};
    std::array<uint8_t, 2> vrc6PulseDuty {};
    uint8_t vrc6SawRate = 0;
    uint16_t vrc6SawAccumulator = 0;
    uint8_t vrc6SawStep = 0;
    double vrc6SawStepPhase = 0.0;
    std::array<uint8_t, 64> fdsWaveRam {};
    std::array<int8_t, 32> fdsModTable {};
    double fdsPhase = 0.0;
    double fdsModPhase = 0.0;
    uint16_t fdsFrequency = 0;
    uint16_t fdsModFrequency = 0;
    uint8_t fdsVolume = 0;
    uint8_t fdsMasterVolume = 1;
    uint8_t fdsModDepth = 0;
    int fdsModAccumulator = 0;
    size_t fdsModIndex = 0;
    bool fdsEnabled = false;
    std::array<double, 3> sunsoft5bPhase {};
    std::array<uint16_t, 3> sunsoft5bTonePeriod { 1, 1, 1 };
    std::array<uint8_t, 3> sunsoft5bVolume {};
    std::array<bool, 3> sunsoft5bEnabled {};
    uint8_t sunsoft5bMixer = 0x38;
    std::array<double, 2> mmc5Phase {};
    std::array<uint16_t, 2> mmc5PulseTimer { 1, 1 };
    std::array<uint8_t, 2> mmc5PulseVolume {};
    std::array<uint8_t, 2> mmc5PulseDuty {};
    std::array<bool, 2> mmc5PulseEnabled {};
    uint8_t mmc5PcmLevel = 64;
    double mmc5PcmEnvelope = 0.0;
    bool mmc5PcmEnabled = false;
    OPLL* vrc7Opll = nullptr;
    std::array<uint8_t, 0x40> vrc7Regs {};
    std::array<uint8_t, 6> vrc7CurrentPatch {};
    std::array<uint16_t, 6> vrc7CurrentFnum {};
    std::array<uint8_t, 6> vrc7CurrentBlock {};
    uint8_t vrc7KeyOnMask = 0;
    uint64_t noteStamp = 0;
    std::vector<std::vector<uint8_t>> dmcSampleBank;
    int dmcSampleSlot = -1;
    bool dmcActive = false;
    bool dmcSampleCompleted = false;
    bool suppressDmcRestartOnNoteOn = false;
    size_t dmcByteIndex = 0;
    uint8_t dmcBitIndex = 0;
    double dmcPhase = 0.0;
    uint64_t dmcBitsPlayed = 0;
    PatchConfig patch;
};

}
std::unique_ptr<ChipCore> makeNesApuCore(AccuracyMode accuracy, ChipMode mode)
{
    return std::make_unique<NesApuCore>(accuracy, mode);
}
}
