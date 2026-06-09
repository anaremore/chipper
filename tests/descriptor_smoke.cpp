#include "Engine/ChipDescriptors.h"
#include "Engine/ControlRegistry.h"
#include "Presets.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>

namespace
{

bool expect(bool condition, const std::string& message)
{
    if (! condition)
        std::cerr << "descriptor_smoke: " << message << "\n";

    return condition;
}

bool expectSegmentedRegister(chipper::ChipMode mode,
                             chipper::ChipParameterRole role,
                             size_t expectedChoices,
                             const std::string& firstChoice)
{
    const auto* spec = chipper::parameterSpecFor(mode, role);
    bool ok = true;
    ok &= expect(spec != nullptr, "missing parameter spec");
    if (spec == nullptr)
        return false;

    ok &= expect(spec->kind == chipper::ParameterKind::chipRegister, spec->id + " is not a chip register");
    ok &= expect(spec->surface == chipper::ControlSurface::segmentedChoice, spec->id + " is not segmented");
    ok &= expect(spec->choices.size() == expectedChoices, spec->id + " has unexpected choice count");
    if (! spec->choices.empty())
        ok &= expect(spec->choices.front().label == firstChoice, spec->id + " has unexpected first choice");

    return ok;
}

bool expectChoiceRegister(chipper::ChipMode mode,
                          chipper::ChipParameterRole role,
                          chipper::ControlSurface surface,
                          size_t expectedChoices,
                          const std::string& firstChoice)
{
    const auto* spec = chipper::parameterSpecFor(mode, role);
    bool ok = true;
    ok &= expect(spec != nullptr, "missing parameter spec");
    if (spec == nullptr)
        return false;

    ok &= expect(spec->kind == chipper::ParameterKind::chipRegister, spec->id + " is not a chip register");
    ok &= expect(spec->surface == surface, spec->id + " has unexpected surface");
    ok &= expect(spec->choices.size() == expectedChoices, spec->id + " has unexpected choice count");
    if (! spec->choices.empty())
        ok &= expect(spec->choices.front().label == firstChoice, spec->id + " has unexpected first choice");

    return ok;
}

bool expectSpec(chipper::ChipMode mode,
                chipper::ChipParameterRole role,
                chipper::ParameterKind kind,
                chipper::ControlSurface surface,
                const std::string& label)
{
    const auto* spec = chipper::parameterSpecFor(mode, role);
    bool ok = true;
    ok &= expect(spec != nullptr, "missing parameter spec");
    if (spec == nullptr)
        return false;

    ok &= expect(spec->kind == kind, spec->id + " has unexpected kind");
    ok &= expect(spec->surface == surface, spec->id + " has unexpected surface");
    ok &= expect(spec->label == label, spec->id + " has unexpected label");
    ok &= expect(! spec->help.empty(), spec->id + " should have user-facing help text");
    return ok;
}

bool expectSpecGroup(chipper::ChipMode mode, chipper::ChipParameterRole role, const std::string& group)
{
    const auto* spec = chipper::parameterSpecFor(mode, role);
    bool ok = true;
    ok &= expect(spec != nullptr, "missing parameter spec");
    if (spec == nullptr)
        return false;

    ok &= expect(spec->group == group, spec->id + " should stay in the " + group + " group");
    return ok;
}

bool expectSpecHelpContains(chipper::ChipMode mode,
                            chipper::ChipParameterRole role,
                            const std::string& needle,
                            const std::string& message)
{
    const auto* spec = chipper::parameterSpecFor(mode, role);
    bool ok = true;
    ok &= expect(spec != nullptr, "missing parameter spec");
    if (spec == nullptr)
        return false;

    ok &= expect(spec->help.find(needle) != std::string::npos, message);
    return ok;
}

bool expectPreset(chipper::ChipMode mode, const std::string& presetId)
{
    const auto* preset = chipper::presetById(presetId);
    bool ok = true;
    ok &= expect(preset != nullptr, "missing factory preset " + presetId);
    if (preset == nullptr)
        return false;

    ok &= expect(preset->chip == mode, presetId + " is assigned to the wrong chip");
    ok &= expect(! preset->name.empty(), presetId + " should have a user-facing name");
    ok &= expect(! preset->note.empty(), presetId + " should explain the sound");
    return ok;
}

bool expectPresetBrowserCatalog(chipper::ChipMode preferredChip, const std::string& firstPresetId)
{
    const auto browserPresets = chipper::presetBrowserCatalog(preferredChip);
    const auto chipPresets = chipper::presetsForChip(preferredChip);
    bool ok = true;

    ok &= expect(browserPresets.size() == chipPresets.size(), "preset browser should expose the selected chip catalog");
    ok &= expect(! browserPresets.empty(), "preset browser should not be empty");
    if (browserPresets.empty())
        return false;

    ok &= expect(browserPresets.front() != nullptr && browserPresets.front()->id == firstPresetId,
                 "preset browser should put the active chip group first");

    std::vector<std::string> ids;
    ids.reserve(browserPresets.size());
    for (const auto* preset : browserPresets)
    {
        ok &= expect(preset != nullptr, "preset browser should not contain null entries");
        if (preset == nullptr)
            continue;

        ok &= expect(preset->chip == preferredChip, "preset browser should only include the selected chip");
        ok &= expect(std::find(ids.begin(), ids.end(), preset->id) == ids.end(),
                     "preset browser should not duplicate " + preset->id);
        ids.push_back(preset->id);
    }

    for (const auto* preset : chipPresets)
    {
        ok &= expect(preset != nullptr, "selected chip preset list should not contain null entries");
        if (preset == nullptr)
            continue;

        ok &= expect(std::find(ids.begin(), ids.end(), preset->id) != ids.end(),
                     "preset browser should include " + preset->id);
    }

    return ok;
}

bool expectSourceLaneCounts(chipper::ChipMode mode, size_t visibleCount, size_t nativeCount)
{
    bool ok = true;
    ok &= expect(chipper::visibleSourceCountForMode(mode) == visibleCount, "unexpected visible source count");
    ok &= expect(chipper::nativeSourceCountForMode(mode) == nativeCount, "unexpected native source count");
    ok &= expect(chipper::hasInternalSourceLanes(mode) == (nativeCount > visibleCount),
                 "internal source lane flag should match native/visible counts");
    return ok;
}

bool expectChoiceLabels(chipper::ChipMode mode,
                        chipper::ChipParameterRole role,
                        const std::vector<std::string>& expected,
                        const std::string& message)
{
    const auto* spec = chipper::parameterSpecFor(mode, role);
    bool ok = true;
    ok &= expect(spec != nullptr, "missing parameter spec");
    if (spec == nullptr)
        return false;

    ok &= expect(spec->choices.size() == expected.size(), message + " has unexpected choice count");
    const auto count = std::min(spec->choices.size(), expected.size());
    for (size_t i = 0; i < count; ++i)
        ok &= expect(spec->choices[i].label == expected[i],
                     message + " choice " + std::to_string(i) + " expected " + expected[i] + ", got " + spec->choices[i].label);

    return ok;
}

bool expectMacroSourceMask(chipper::ChipMode mode, chipper::MacroKind macro, std::array<bool, 4> expected)
{
    const auto& templ = chipper::macroTemplateFor(mode, macro);
    bool matches = true;
    for (size_t index = 0; index < expected.size(); ++index)
        matches = matches && templ.sourceEnabled[index] == expected[index];
    return expect(matches, templ.label + " has unexpected source mask");
}

bool expectMacroLabel(chipper::ChipMode mode, chipper::MacroKind macro, const std::string& expected)
{
    const auto& templ = chipper::macroTemplateFor(mode, macro);
    return expect(templ.label == expected, "macro label expected " + expected + ", got " + templ.label);
}

bool expectAutomatableDescriptorParametersHaveMidiCc()
{
    bool ok = true;

    for (const auto mode : chipper::chipModeOrder())
    {
        const auto& descriptor = chipper::descriptorFor(mode);
        for (const auto& spec : descriptor.parameters)
        {
            if (! spec.automatable)
                continue;

            const auto* parameterId = chipper::parameterIdForChipParameterRole(spec.role);
            ok &= expect(parameterId != nullptr, spec.id + " has no APVTS parameter id for MIDI CC control");
            if (parameterId == nullptr)
                continue;

            ok &= expect(chipper::midiControllerForParameterId(parameterId) >= 0,
                         spec.id + " maps to APVTS parameter " + parameterId + " without a default MIDI CC");
        }
    }

    return ok;
}

bool expectLiveSourceLevelSpecs()
{
    bool ok = true;
    constexpr std::array liveModes {
        chipper::ChipMode::nes,
        chipper::ChipMode::dmg,
        chipper::ChipMode::sid,
        chipper::ChipMode::ym2149,
        chipper::ChipMode::sn76489,
        chipper::ChipMode::ym2612,
        chipper::ChipMode::opl3,
        chipper::ChipMode::ym2151,
        chipper::ChipMode::ym2413,
        chipper::ChipMode::spc700,
        chipper::ChipMode::pokey,
        chipper::ChipMode::paula,
        chipper::ChipMode::huc6280,
        chipper::ChipMode::namcoWsg,
        chipper::ChipMode::scc
    };
    constexpr std::array sourceLevelRoles {
        chipper::ChipParameterRole::source1Level,
        chipper::ChipParameterRole::source2Level,
        chipper::ChipParameterRole::source3Level,
        chipper::ChipParameterRole::source4Level,
        chipper::ChipParameterRole::source5Level,
        chipper::ChipParameterRole::source6Level,
        chipper::ChipParameterRole::source7Level,
        chipper::ChipParameterRole::source8Level,
        chipper::ChipParameterRole::source9Level
    };

    for (const auto mode : liveModes)
    {
        const auto expectedSourceCount = mode == chipper::ChipMode::sid ? 3u : (chipper::visibleSourceCountForMode(mode));
        for (size_t index = 0; index < expectedSourceCount; ++index)
        {
            const auto role = sourceLevelRoles[index];
            const auto* spec = chipper::parameterSpecFor(mode, role);
            ok &= expect(spec != nullptr, "live chip missing source level spec");
            if (spec == nullptr)
                continue;

            ok &= expect(spec->kind == chipper::ParameterKind::continuous, spec->id + " should be a continuous trim");
            ok &= expect(spec->surface == chipper::ControlSurface::slider, spec->id + " should use a slider surface");
            ok &= expect(spec->group == "Sources", spec->id + " should stay in the Sources group");

            const auto* parameterId = chipper::parameterIdForChipParameterRole(role);
            ok &= expect(parameterId != nullptr, spec->id + " has no APVTS parameter id");
            if (parameterId != nullptr)
                ok &= expect(chipper::midiControllerForParameterId(parameterId) >= 0, spec->id + " has no MIDI CC mapping");
        }

        if (mode == chipper::ChipMode::sid)
            ok &= expect(chipper::parameterSpecFor(mode, chipper::ChipParameterRole::source4Level) == nullptr,
                         "SID should expose only its three oscillator voice trims");
    }

    return ok;
}

bool expectVerificationDisclosure()
{
    bool ok = true;

    constexpr std::array liveModes {
        chipper::ChipMode::nes,
        chipper::ChipMode::dmg,
        chipper::ChipMode::sid,
        chipper::ChipMode::ym2149,
        chipper::ChipMode::sn76489,
        chipper::ChipMode::pokey,
        chipper::ChipMode::huc6280,
        chipper::ChipMode::namcoWsg,
        chipper::ChipMode::ym2612,
        chipper::ChipMode::opl3,
        chipper::ChipMode::ym2151,
        chipper::ChipMode::ym2413,
        chipper::ChipMode::scc
    };

    for (const auto mode : liveModes)
    {
        const auto& disclosure = chipper::descriptorFor(mode).verification;
        ok &= expect(disclosure.badge == "Verified partial", "live chip should disclose verified partial status");
        ok &= expect(! disclosure.summary.empty(), "live chip verification summary should not be empty");
        ok &= expect(! disclosure.evidence.empty(), "live chip verification evidence should not be empty");
        ok &= expect(! disclosure.verifiedBehaviors.empty(), "live chip should list verified behavior");
        ok &= expect(! disclosure.knownGaps.empty(), "live chip should list known gaps");
        ok &= expect(! disclosure.hardwareValidated, "live chip should not claim hardware validation yet");
        ok &= expect(! disclosure.cycleAccurate, "live chip should not claim cycle accuracy yet");
    }

    return ok;
}

bool expectFmRegisterHelpers()
{
    bool ok = true;

    const auto opn2Lead = chipper::makePatchConfig(chipper::ChipMode::ym2612,
                                                   chipper::MacroKind::lead,
                                                   0.1f,
                                                   0.6f,
                                                   0.5f,
                                                   0.8f);
    ok &= expect(chipper::ym2612AlgorithmForPatch(opn2Lead) == 4u, "YM2612 lead macro should resolve to algorithm 4");
    ok &= expect(chipper::fmFeedbackForPatch(opn2Lead) == 4u, "FM feedback helper should quantize control 2 to 0-7");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2612, opn2Lead, 0) == 8u, "YM2612 helper should resolve operator 1 multiple");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2612, opn2Lead, 3) == 12u, "YM2612 helper should resolve operator 4 multiple");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2612, opn2Lead, 0) == 38u, "YM2612 helper should resolve modulator total level");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2612, opn2Lead, 3) == 6u, "YM2612 helper should resolve carrier total level");
    ok &= expect(chipper::ym2612PanBitsForPatch(opn2Lead, 0) == 0xc0u, "YM2612 lead macro should resolve to centered pan bits");
    ok &= expect(chipper::ym2612DacModeForPatch(opn2Lead) == 1u, "YM2612 lead macro should keep channel 6 in FM mode");
    const auto opn2LeadModulatorEnvelope = chipper::ym2612EnvelopeRegistersForPatch(opn2Lead, 0);
    ok &= expect(opn2LeadModulatorEnvelope.attackRate == 0x1fu, "YM2612 lead envelope should use fast modulator attack");
    ok &= expect(opn2LeadModulatorEnvelope.decayRate == 0x06u, "YM2612 lead modulator decay should be softened from the carrier envelope");
    ok &= expect(opn2LeadModulatorEnvelope.sustainRate == 0x04u, "YM2612 lead envelope should use medium sustain rate");
    ok &= expect(opn2LeadModulatorEnvelope.sustainRelease == 0xb6u, "YM2612 lead modulator release byte should retain a higher sustain level");

    const auto opmArp = chipper::makePatchConfig(chipper::ChipMode::ym2151,
                                                 chipper::MacroKind::arp,
                                                 0.0f,
                                                 0.4f,
                                                 0.5f,
                                                 0.7f);
    ok &= expect(chipper::ym2151AlgorithmForPatch(opmArp) == 7u, "YM2151 arp macro should resolve to algorithm 7");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmArp, 0) == 8u, "YM2151 helper should resolve operator 1 multiple");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmArp, 3) == 13u, "YM2151 helper should resolve operator 4 multiple");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2151, opmArp, 0) == 42u, "YM2151 helper should resolve modulator total level");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2151, opmArp, 3) == 7u, "YM2151 helper should resolve carrier total level");

    const auto explicitAlgorithm = chipper::makePatchConfig(chipper::ChipMode::ym2612,
                                                            chipper::MacroKind::manual,
                                                            0.0f,
                                                            0.0f,
                                                            0.0f,
                                                            0.0f,
                                                            chipper::PlayMode::stack,
                                                            { true, true, true, true },
                                                            { 1.0f, 1.0f, 1.0f, 1.0f },
                                                            0.0f,
                                                            0.0f,
                                                            8);
    ok &= expect(chipper::ym2612AlgorithmForPatch(explicitAlgorithm) == 7u, "YM2612 explicit algorithm choice should override macro controls");
    const auto explicitPan = chipper::makePatchConfig(chipper::ChipMode::ym2612,
                                                      chipper::MacroKind::lead,
                                                      0.5f,
                                                      0.5f,
                                                      0.5f,
                                                      0.5f,
                                                      chipper::PlayMode::stack,
                                                      { true, true, true, true },
                                                      { 1.0f, 1.0f, 1.0f, 1.0f },
                                                      0.0f,
                                                      0.0f,
                                                      0,
                                                      0,
                                                      0,
                                                      4);
    ok &= expect(chipper::ym2612PanBitsForPatch(explicitPan, 0) == 0x80u, "YM2612 alternating pan should send channel 1 left");
    ok &= expect(chipper::ym2612PanBitsForPatch(explicitPan, 1) == 0x40u, "YM2612 alternating pan should send channel 2 right");
    const auto explicitDac = chipper::makePatchConfig(chipper::ChipMode::ym2612,
                                                      chipper::MacroKind::lead,
                                                      0.5f,
                                                      0.5f,
                                                      0.5f,
                                                      0.5f,
                                                      chipper::PlayMode::stack,
                                                      { true, true, true, true },
                                                      { 1.0f, 1.0f, 1.0f, 1.0f },
                                                      0.0f,
                                                      0.0f,
                                                      0,
                                                      0,
                                                      0,
                                                      0,
                                                      0,
                                                      0,
                                                      0,
                                                      0,
                                                      2);
    ok &= expect(chipper::ym2612DacModeForPatch(explicitDac) == 2u, "YM2612 explicit DAC mode should override macro controls");
    const auto explicitPadEnvelope = chipper::makePatchConfig(chipper::ChipMode::ym2612,
                                                              chipper::MacroKind::manual,
                                                              0.5f,
                                                              0.5f,
                                                              0.5f,
                                                              0.5f,
                                                              chipper::PlayMode::stack,
                                                              { true, true, true, true },
                                                              { 1.0f, 1.0f, 1.0f, 1.0f },
                                                              0.0f,
                                                              0.0f,
                                                              0,
                                                              0,
                                                              0,
                                                              0,
                                                              3);
    const auto opn2PadEnvelope = chipper::ym2612EnvelopeRegistersForPatch(explicitPadEnvelope, 0);
    ok &= expect(opn2PadEnvelope.attackRate == 0x14u, "YM2612 Pad envelope should lower operator attack rate");
    ok &= expect(opn2PadEnvelope.decayRate == 0x04u, "YM2612 Pad modulator decay should map to register $60");
    ok &= expect(opn2PadEnvelope.sustainRate == 0x02u, "YM2612 Pad sustain-rate should map to register $70");
    ok &= expect(opn2PadEnvelope.sustainRelease == 0x85u, "YM2612 Pad modulator sustain/release should map to register $80");

    const auto oplLead = chipper::makePatchConfig(chipper::ChipMode::opl3,
                                                  chipper::MacroKind::lead,
                                                  0.7f,
                                                  0.5f,
                                                  0.0f,
                                                  0.75f);
    ok &= expect(chipper::oplWaveformForPatch(oplLead) == 3u, "OPL lead macro should resolve to waveform 3");
    ok &= expect(chipper::oplConnectionForPatch(oplLead) == 1u, "OPL connection helper should expose parallel mode above midpoint");
    ok &= expect(chipper::oplModulatorMultipleForPatch(oplLead) == 1u, "OPL helper should resolve modulator multiple");
    ok &= expect(chipper::oplModulatorTotalLevelForPatch(oplLead) == 58u, "OPL helper should resolve modulator total level");
    ok &= expect(chipper::oplCarrierTotalLevelForPatch(oplLead) == 6u, "OPL helper should resolve carrier total level");

    const auto opllBass = chipper::makePatchConfig(chipper::ChipMode::ym2413,
                                                   chipper::MacroKind::bass,
                                                   0.0f,
                                                   0.0f,
                                                   0.0f,
                                                   0.6f);
    ok &= expect(chipper::ym2413InstrumentForPatch(opllBass) == 13u, "YM2413 bass macro should resolve to OPLL instrument 13");
    ok &= expect(chipper::ym2413VolumeNibbleForPatch(opllBass, 0) == 6u, "YM2413 helper should resolve volume nibble from output level");

    const auto opllExplicitBell = chipper::makePatchConfig(chipper::ChipMode::ym2413,
                                                           chipper::MacroKind::manual,
                                                           0.0f,
                                                           0.0f,
                                                           0.0f,
                                                           0.6f,
                                                           chipper::PlayMode::stack,
                                                           { true, true, true, true },
                                                           { 1.0f, 0.5f, 1.0f, 1.0f },
                                                           0.0f,
                                                           0.0f,
                                                           1);
    ok &= expect(chipper::ym2413InstrumentForPatch(opllExplicitBell) == 12u, "YM2413 explicit Bell choice should resolve to OPLL instrument 12");
    ok &= expect(chipper::ym2413VolumeNibbleForPatch(opllExplicitBell, 1) == 11u, "YM2413 helper should include source trim in volume nibble");

    return ok;
}

bool expectWavetableRegisterHelpers()
{
    bool ok = true;

    const auto hucLead = chipper::makePatchConfig(chipper::ChipMode::huc6280,
                                                  chipper::MacroKind::lead,
                                                  0.0f,
                                                  0.0f,
                                                  0.0f,
                                                  0.76f,
                                                  chipper::PlayMode::stack,
                                                  { true, true, true, false },
                                                  { 1.0f, 0.5f, 1.0f, 1.0f },
                                                  0.0f,
                                                  0.0f,
                                                  1);
    ok &= expect((chipper::huc6280ControlForPatch(hucLead, 0) & 0x80u) != 0, "HuC6280 helper should set the channel on bit");
    ok &= expect((chipper::huc6280ControlForPatch(hucLead, 0) & 0x1fu) == 24u, "HuC6280 helper should resolve 5-bit volume");
    ok &= expect(chipper::wavetableRamSampleForPatch(chipper::ChipMode::huc6280, hucLead, 0, 31) == 30u, "HuC6280 ramp Wave RAM tail should follow the 32-sample core formula");
    ok &= expect(! chipper::huc6280ChannelUsesNoiseForPatch(hucLead, 3), "HuC6280 exposed melodic channels should not claim noise for lead macro");

    const auto sccPulse = chipper::makePatchConfig(chipper::ChipMode::scc,
                                                   chipper::MacroKind::manual,
                                                   0.0f,
                                                   0.0f,
                                                   0.5f,
                                                   0.80f,
                                                   chipper::PlayMode::stack,
                                                   { true, false, true, true },
                                                   { 1.0f, 0.5f, 1.0f, 1.0f },
                                                   0.0f,
                                                   0.0f,
                                                   3);
    ok &= expect(chipper::sccVolumeForPatch(sccPulse, 0) == 12u, "SCC helper should resolve 4-bit channel volume");
    ok &= expect(! chipper::sccChannelKeyOnForPatch(sccPulse, 1), "SCC helper should honor source mute for exposed channels");
    ok &= expect(chipper::wavetableRamSampleForPatch(chipper::ChipMode::scc, sccPulse, 0, 0) == 255u, "SCC pulse Wave RAM head should resolve high");
    ok &= expect(chipper::wavetableRamSampleForPatch(chipper::ChipMode::scc, sccPulse, 0, 31) == 0u, "SCC pulse Wave RAM tail should resolve low");

    const auto namcoArp = chipper::makePatchConfig(chipper::ChipMode::namcoWsg,
                                                   chipper::MacroKind::arp,
                                                   0.0f,
                                                   0.0f,
                                                   0.5f,
                                                   0.76f,
                                                   chipper::PlayMode::stack,
                                                   { true, true, true, true },
                                                   { 1.0f, 0.5f, 1.0f, 1.0f },
                                                   0.0f,
                                                   0.0f,
                                                   4);
    ok &= expect(chipper::namcoWsgVolumeForPatch(namcoArp, 0) == 11u, "Namco WSG helper should resolve 4-bit lane volume");
    ok &= expect(chipper::namcoWsgChannelEnabledForPatch(namcoArp, 7), "Namco WSG arp macro should expose internal extra lanes as enabled");
    ok &= expect(chipper::wavetableRamSampleForPatch(chipper::ChipMode::namcoWsg, namcoArp, 1, 0) == 3u, "Namco WSG stepped RAM should include channel offset");
    ok &= expect(chipper::wavetableRamSampleForPatch(chipper::ChipMode::namcoWsg, namcoArp, 1, 31) == 14u, "Namco WSG stepped RAM tail should match core formula");

    return ok;
}

bool expectSampleRegisterHelpers()
{
    bool ok = true;

    const auto spcLead = chipper::makePatchConfig(chipper::ChipMode::spc700,
                                                  chipper::MacroKind::lead,
                                                  0.0f,
                                                  0.0f,
                                                  0.0f,
                                                  0.80f,
                                                  chipper::PlayMode::stack,
                                                  { true, false, true, true },
                                                  { 1.0f, 0.5f, 1.0f, 1.0f },
                                                  0.0f,
                                                  0.20f);
    ok &= expect(chipper::sampleTemplateForPatch(chipper::ChipMode::spc700, spcLead) == 1u, "SPC700 lead macro should resolve to template 1");
    ok &= expect(chipper::spc700VoiceVolumeForPatch(spcLead, 0) == 102u, "SPC700 helper should resolve 7-bit voice volume");
    ok &= expect(chipper::spc700EnvelopeShapeForPatch(spcLead) == 2u, "SPC700 lead macro should resolve to lead envelope shape");
    ok &= expect(chipper::spc700AdsrForPatch(spcLead) == 0xbbu, "SPC700 helper should resolve simplified ADSR byte");
    ok &= expect(chipper::spc700GainForPatch(spcLead) == 102u, "SPC700 helper should resolve GAIN from output control");
    ok &= expect(! chipper::spc700VoiceEnabledForPatch(spcLead, 1), "SPC700 helper should honor exposed source mute");
    ok &= expect(chipper::generatedSampleValueForPatch(chipper::ChipMode::spc700, spcLead, 0, 0) == -15, "SPC700 generated sample head should match core template");
    ok &= expect(chipper::spc700SamplePlaybackModeForPatch(spcLead) == 1u, "SPC700 lead macro should resolve to looped playback");

    const auto spcExplicitNoise = chipper::makePatchConfig(chipper::ChipMode::spc700,
                                                           chipper::MacroKind::manual,
                                                           0.0f,
                                                           0.0f,
                                                           0.0f,
                                                           0.80f,
                                                           chipper::PlayMode::stack,
                                                           { true, true, true, true },
                                                           { 1.0f, 1.0f, 1.0f, 1.0f },
                                                           0.0f,
                                                           0.0f,
                                                           4);
    ok &= expect(chipper::sampleTemplateForPatch(chipper::ChipMode::spc700, spcExplicitNoise) == 4u, "SPC700 explicit sample shape should override macro");
    const auto spcDrum = chipper::makePatchConfig(chipper::ChipMode::spc700,
                                                  chipper::MacroKind::drum,
                                                  0.5f,
                                                  0.5f,
                                                  0.5f,
                                                  0.5f);
    ok &= expect(chipper::spc700SamplePlaybackModeForPatch(spcDrum) == 2u, "SPC700 drum macro should resolve to one-shot playback");
    ok &= expect(chipper::spc700EnvelopeShapeForPatch(spcDrum) == 4u, "SPC700 drum macro should resolve to percussion envelope shape");
    const auto spcForcedOneShot = chipper::makePatchConfig(chipper::ChipMode::spc700,
                                                           chipper::MacroKind::lead,
                                                           0.5f,
                                                           0.5f,
                                                           0.5f,
                                                           0.5f,
                                                           chipper::PlayMode::stack,
                                                           { true, true, true, true },
                                                           { 1.0f, 1.0f, 1.0f, 1.0f },
                                                           0.0f,
                                                           0.0f,
                                                           0,
                                                           0,
                                                           0,
                                                           2);
    ok &= expect(! chipper::spc700SampleLoopsForPatch(spcForcedOneShot), "SPC700 explicit one-shot playback should override the template");

    const auto paulaBass = chipper::makePatchConfig(chipper::ChipMode::paula,
                                                    chipper::MacroKind::bass,
                                                    0.0f,
                                                    0.0f,
                                                    0.60f,
                                                    0.84f,
                                                    chipper::PlayMode::stack,
                                                    { true, true, false, true },
                                                    { 1.0f, 0.5f, 1.0f, 1.0f });
    ok &= expect(chipper::sampleTemplateForPatch(chipper::ChipMode::paula, paulaBass) == 2u, "Paula bass macro should resolve to template 2");
    ok &= expect(chipper::paulaChannelVolumeForPatch(paulaBass, 0) == 54u, "Paula helper should resolve 6-bit channel volume");
    ok &= expect(chipper::paulaLoopForPatch(paulaBass), "Paula bass macro should resolve to looped playback");
    ok &= expect(chipper::paulaControlForPatch(paulaBass, 2) == 0x02u, "Paula muted channel should keep loop bit without enable bit");
    ok &= expect(chipper::generatedSampleValueForPatch(chipper::ChipMode::paula, paulaBass, 0, 0) == -127, "Paula triangle sample head should match core template");
    ok &= expect(chipper::generatedSampleValueForPatch(chipper::ChipMode::paula, paulaBass, 0, 32) == 127, "Paula triangle sample midpoint should match core template");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;

    ok &= expect(chipper::descriptorFor(chipper::ChipMode::nes).implemented, "NES descriptor should be implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::sid).implemented, "SID descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::spc700).implemented, "SPC700 descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::spc700).supportsChipPoly, "SPC700 should support Chip Poly across exposed sample voices");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::pokey).implemented, "POKEY descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::pokey).supportsChipPoly, "POKEY should support Chip Poly across four channels");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::paula).implemented, "Paula descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::paula).supportsChipPoly, "Paula should support Chip Poly across four sample channels");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::huc6280).implemented, "HuC6280 descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::huc6280).supportsChipPoly, "HuC6280 should support Chip Poly across six native wavetable channels");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::namcoWsg).implemented, "Namco WSG descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::namcoWsg).supportsChipPoly, "Namco WSG should support Chip Poly across exposed wavetable lanes");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2413).implemented, "YM2413 descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2413).supportsChipPoly, "YM2413 should support Chip Poly across exposed melodic channels");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2151).implemented, "YM2151 descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2151).supportsChipPoly, "YM2151 should support Chip Poly across exposed melodic channels");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::scc).implemented, "SCC descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::scc).supportsChipPoly, "SCC should support Chip Poly across exposed wavetable channels");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl1, 4, "12.5%");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::pulse2Duty, 5, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::snNoiseMode, 3, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl1, 4, "12.5%");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::waveShape, 5, "RAM");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::dmgWaveLevel, 5, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::dmgStereoRoute, 5, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::snNoiseMode, 3, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::sid, chipper::ChipParameterRole::waveShape, 9, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::sid, chipper::ChipParameterRole::snNoiseMode, 5, "Follow");
    ok &= expectSegmentedRegister(chipper::ChipMode::sid, chipper::ChipParameterRole::dmgStereoRoute, 3, "Auto");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2149, chipper::ChipParameterRole::macroControl4, 3, "Noise");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymChannelAMix, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "A Mix");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymChannelBMix, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "B Mix");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymChannelCMix, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "C Mix");
    ok &= expectChoiceRegister(chipper::ChipMode::sn76489, chipper::ChipParameterRole::snNoiseMode, chipper::ControlSurface::menu, 5, "Follow");

    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Sweep Motion");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::pulse2Duty, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Pulse 2 Duty");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::source4Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Noise / DMC");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::source4Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Noise Level");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::source4Enabled, "external DPCM sample lane", "NES source 4 should disclose the DMC sample lane");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::source4Level, "noise source", "NES source 4 level should stay honest about trimming noise");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcDirectLevel, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "DMC Direct Level");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcDirectLevel, "DMC");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcDirectLevel, "external user-supplied .dmc bytes", "NES DMC direct help should mention external sample byte stepping");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcSampleSlot, chipper::ParameterKind::steppedNumeric, chipper::ControlSurface::steppedSlider, "DMC Sample Slot");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcSampleSlot, "DMC");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcSampleSlot, "checked .dmc sample", "NES DMC sample slot help should disclose checked bank behavior");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcRateIndex, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "DMC Rate");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcRateIndex, "DMC");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcRateIndex, "$4010", "NES DMC rate help should mention the hardware rate register");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcPlaybackMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "DMC Playback");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcPlaybackMode, "DMC");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcPlaybackMode, "MIDI notes", "NES DMC playback mode help should explain note mapping");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcMapRoot, chipper::ParameterKind::steppedNumeric, chipper::ControlSurface::menu, "DMC Map Root");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcMapRoot, "DMC");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcMapRoot, "Note Map starts", "NES DMC map root help should explain the keyboard offset");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcLoop, chipper::ParameterKind::chipRegister, chipper::ControlSurface::toggle, "DMC Loop");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcLoop, "DMC");
    ok &= expectSpecHelpContains(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcLoop, "$4010 loop bit", "NES DMC loop help should mention the hardware loop bit");
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Sweep Shift");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Period");
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Clock");
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Envelope Level");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Pulse Width");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Cutoff");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::macroControl3, "Filter");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Sustain");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::envelopeDecay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "ADSR Speed");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidAttack, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V1 Attack");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidDecay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V1 Decay");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidSustain, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V1 Sustain");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidRelease, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V1 Release");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2Attack, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V2 Attack");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2Decay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V2 Decay");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2Sustain, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V2 Sustain");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2Release, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V2 Release");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3Attack, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V3 Attack");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3Decay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V3 Decay");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3Sustain, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V3 Sustain");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3Release, chipper::ParameterKind::chipRegister, chipper::ControlSurface::steppedSlider, "V3 Release");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2WaveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Voice 2 Wave");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3WaveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Voice 3 Wave");
    const std::vector<std::string> expectedSidWaveChoices {
        "Follow",
        "Tri",
        "Saw",
        "Pulse",
        "Noise",
        "Tri+Saw",
        "Tri+Pulse",
        "Saw+Pulse",
        "Tri+Saw+Pulse"
    };
    ok &= expectChoiceLabels(chipper::ChipMode::sid,
                             chipper::ChipParameterRole::waveShape,
                             expectedSidWaveChoices,
                             "SID voice 1 waveform choices");
    ok &= expectChoiceLabels(chipper::ChipMode::sid,
                             chipper::ChipParameterRole::sidVoice2WaveShape,
                             expectedSidWaveChoices,
                             "SID voice 2 waveform choices");
    ok &= expectChoiceLabels(chipper::ChipMode::sid,
                             chipper::ChipParameterRole::sidVoice3WaveShape,
                             expectedSidWaveChoices,
                             "SID voice 3 waveform choices");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2PulseWidth, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Voice 2 PW");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2PulseWidth, "Voices");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3PulseWidth, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Voice 3 PW");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3PulseWidth, "Voices");
    ok &= expectSpecHelpContains(chipper::ChipMode::sid,
                                 chipper::ChipParameterRole::source3Enabled,
                                 "OSC3/ENV3",
                                 "SID voice 3 enable help should disclose utility-read behavior is separate from audio output");
    ok &= expectSpecHelpContains(chipper::ChipMode::sid,
                                 chipper::ChipParameterRole::source3Level,
                                 "audible SID voice 3",
                                 "SID voice 3 level help should clarify it trims audible voice 3");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Filter Mode");
    if (const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::sid, chipper::ChipParameterRole::ymEnvelopeShape))
    {
        ok &= expect(spec->choices.size() == 9u, "SID filter mode should expose nine choices");
        ok &= expect(! spec->choices.empty() && spec->choices.front().label == "Follow", "SID filter mode should use Follow as the inherited choice");
    }
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::stereoSpread, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Resonance");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::stereoSpread, "Filter");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidFilterRouting, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Filter Routing");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::sidFilterRouting, "Filter");
    if (const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::sid, chipper::ChipParameterRole::sidFilterRouting))
        ok &= expect(spec->choices.size() == 9u, "SID filter routing should expose nine choices");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "SID Model");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Pitch");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::envelopeDecay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Envelope Speed");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Envelope Shape");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::source1Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel A");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::source2Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel B");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::source3Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel C");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::source4Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Shared Noise");
    if (const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymEnvelopeShape))
    {
        ok &= expect(spec->choices.size() == 21u, "YM envelope shape should expose friendly shortcuts plus 16 hardware codes");
        ok &= expect(! spec->choices.empty() && spec->choices.front().label == "Fixed", "YM envelope shape should keep Fixed as the inherited choice");
        ok &= expect(spec->choices.size() > 20u && spec->choices[20].label == "0x0F", "YM envelope shape should expose register code 0x0F");
    }
    ok &= expect(chipper::ym2149EnvelopeShapeCodeForChoice(1) == 0x09u, "YM Fall shortcut should map to shape code 0x09");
    ok &= expect(chipper::ym2149EnvelopeShapeCodeForChoice(2) == 0x0du, "YM Rise shortcut should map to shape code 0x0D");
    ok &= expect(chipper::ym2149EnvelopeShapeCodeForChoice(3) == 0x08u, "YM Saw shortcut should map to shape code 0x08");
    ok &= expect(chipper::ym2149EnvelopeShapeCodeForChoice(4) == 0x0eu, "YM Tri shortcut should map to shape code 0x0E");
    ok &= expect(chipper::ym2149EnvelopeShapeCodeForChoice(20) == 0x0fu, "YM exact 0x0F choice should map to shape code 0x0F");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::stereoSpread, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Stereo Spread");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Tone Stack");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Bias");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Level");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::stereoSpread, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Stereo Spread");
    ok &= expectSpecHelpContains(chipper::ChipMode::sn76489,
                                 chipper::ChipParameterRole::source3Enabled,
                                 "clock the native noise generator",
                                 "SN76489 Tone 3 source help should disclose tone-3-clocked noise behavior");
    ok &= expectSpecHelpContains(chipper::ChipMode::sn76489,
                                 chipper::ChipParameterRole::source4Enabled,
                                 "noise-control register",
                                 "SN76489 noise source help should disclose the native noise register");

    ok &= expectMacroSourceMask(chipper::ChipMode::nes, chipper::MacroKind::drum, { false, false, true, true });
    ok &= expectMacroSourceMask(chipper::ChipMode::dmg, chipper::MacroKind::bass, { false, false, true, false });
    ok &= expectMacroSourceMask(chipper::ChipMode::sid, chipper::MacroKind::drum, { false, false, true, false });
    ok &= expectMacroSourceMask(chipper::ChipMode::ym2149, chipper::MacroKind::drum, { true, false, false, true });
    ok &= expectMacroSourceMask(chipper::ChipMode::sn76489, chipper::MacroKind::drum, { false, false, false, true });
    ok &= expectMacroLabel(chipper::ChipMode::nes, chipper::MacroKind::coin, "NES Coin Blip");
    ok &= expectMacroLabel(chipper::ChipMode::dmg, chipper::MacroKind::bass, "DMG Wave Bass");
    ok &= expectMacroLabel(chipper::ChipMode::sid, chipper::MacroKind::bass, "SID Dirty Bass");
    ok &= expectMacroLabel(chipper::ChipMode::ym2149, chipper::MacroKind::drum, "YM Noise Perc");
    ok &= expectMacroLabel(chipper::ChipMode::sn76489, chipper::MacroKind::drum, "PSG Drum");
    ok &= expectMacroLabel(chipper::ChipMode::ym2612, chipper::MacroKind::bass, "OPN2 Feedback Bass");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Pan");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::dmgStereoRoute, "Output");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::dmgStereoRoute, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Envelope Shape");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::ymEnvelopeShape, "Envelope");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::snNoiseMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "DAC Mode");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::snNoiseMode, "Output");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::snNoiseMode, 3, "Follow");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source5Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "FM Ch 5");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source6Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "FM Ch 6");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source5Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "FM Ch 5 Level");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source6Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "FM Ch 6 Level");
    ok &= expectMacroLabel(chipper::ChipMode::opl3, chipper::MacroKind::drum, "OPL2 Rhythm Kit");
    ok &= expectMacroLabel(chipper::ChipMode::spc700, chipper::MacroKind::drum, "SPC700 Drum Map");
    ok &= expectPreset(chipper::ChipMode::spc700, "spc700-stage-clear");
    ok &= expectPreset(chipper::ChipMode::spc700, "spc700-noise-snare");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Sample Shape");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::waveShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Playback");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, "Sample");
    ok &= expectChoiceRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, chipper::ControlSurface::menu, 3, "Follow Template");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Envelope Shape");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::ymEnvelopeShape, "Envelope");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Sample Playback");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, "Sample");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, "manual slot", "SPC700 sample playback help should explain manual-slot behavior");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::snNoiseMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Noise Source");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::snNoiseMode, "Noise");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::snNoiseMode, 5, "Follow");
    const auto spcDescriptorDrum = chipper::makePatchConfig(chipper::ChipMode::spc700,
                                                            chipper::MacroKind::drum,
                                                            0.5f,
                                                            0.5f,
                                                            0.5f,
                                                            0.5f);
    ok &= expect(chipper::spc700NoiseModeForPatch(spcDescriptorDrum) == 4u, "SPC700 drum macro should resolve to high noise mode");
    ok &= expect(chipper::spc700NoiseClockForPatch(spcDescriptorDrum) == 26u, "SPC700 high noise mode should resolve to noise clock 26");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcMapRoot, chipper::ParameterKind::steppedNumeric, chipper::ControlSurface::menu, "Sample Map Root");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcMapRoot, "Sample");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcMapRoot, "folder bank starts", "SPC700 sample map root help should explain folder-bank note mapping");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::source7Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Voice 7");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::source8Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Voice 8");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::source7Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Voice 7 Level");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::source8Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Voice 8 Level");
    ok &= expectMacroLabel(chipper::ChipMode::pokey, chipper::MacroKind::lead, "POKEY Buzzy Lead");
    ok &= expectPreset(chipper::ChipMode::pokey, "pokey-alien-laser");
    ok &= expectSpec(chipper::ChipMode::pokey, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Distortion Code");
    ok &= expectSegmentedRegister(chipper::ChipMode::pokey, chipper::ChipParameterRole::waveShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::pokey, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "AUDCTL Pairing");
    ok &= expectSegmentedRegister(chipper::ChipMode::pokey, chipper::ChipParameterRole::dmgStereoRoute, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::pokey, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "AUDCTL Filter");
    ok &= expectSegmentedRegister(chipper::ChipMode::pokey, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Follow");
    ok &= expectMacroLabel(chipper::ChipMode::paula, chipper::MacroKind::arp, "Paula Tracker Arp");
    ok &= expectPreset(chipper::ChipMode::paula, "paula-stab-chord");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Sample Shape");
    ok &= expectSegmentedRegister(chipper::ChipMode::paula, chipper::ChipParameterRole::waveShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source1Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 1 L");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source2Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 2 R");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source3Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 3 R");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source4Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 4 L");
    ok &= expectMacroLabel(chipper::ChipMode::huc6280, chipper::MacroKind::lead, "HuC6280 Glass Lead");
    ok &= expectPreset(chipper::ChipMode::huc6280, "huc-boss-alert");
    ok &= expectSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Wave Shape");
    ok &= expectSegmentedRegister(chipper::ChipMode::huc6280, chipper::ChipParameterRole::waveShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::source5Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 5");
    ok &= expectSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::source6Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 6");
    ok &= expectMacroLabel(chipper::ChipMode::namcoWsg, chipper::MacroKind::arp, "Namco Tracker Arp");
    ok &= expectPreset(chipper::ChipMode::namcoWsg, "namco-start-button");
    ok &= expectSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Wave Shape");
    ok &= expectSegmentedRegister(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::waveShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::source8Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Lane 8");
    ok &= expectSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::source8Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Lane 8 Level");
    ok &= expectMacroLabel(chipper::ChipMode::ym2151, chipper::MacroKind::lead, "OPM Metallic Lead");
    ok &= expectPreset(chipper::ChipMode::ym2151, "opm-metallic-lead");
    ok &= expectMacroSourceMask(chipper::ChipMode::ym2151, chipper::MacroKind::drum, { false, false, true, true });
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Algorithm");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2151, chipper::ChipParameterRole::waveShape, chipper::ControlSurface::menu, 9, "Follow");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Algorithm Bias");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Feedback");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Operator Tone");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "FM Level");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source7Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPM Ch 7");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source8Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPM Ch 8");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source7Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "OPM Ch 7 Level");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source8Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "OPM Ch 8 Level");
    ok &= expectMacroLabel(chipper::ChipMode::ym2413, chipper::MacroKind::coin, "OPLL UI Chime");
    ok &= expectPreset(chipper::ChipMode::ym2413, "opll-soft-keys");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Instrument");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2413, chipper::ChipParameterRole::waveShape, chipper::ControlSurface::menu, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Rhythm Mode");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2413, chipper::ChipParameterRole::ymEnvelopeShape, 3, "Follow");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::source9Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPLL Ch 9");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::source9Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Ch 9 Level");
    ok &= expectSpec(chipper::ChipMode::opl3, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Rhythm Mode");
    ok &= expectSegmentedRegister(chipper::ChipMode::opl3, chipper::ChipParameterRole::ymEnvelopeShape, 3, "Follow");
    ok &= expectPreset(chipper::ChipMode::opl3, "opl2-rhythm-kit");
    ok &= expectSpec(chipper::ChipMode::opl3, chipper::ChipParameterRole::source9Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPL Ch 9 / TOM+CYM");
    ok &= expectSpec(chipper::ChipMode::opl3, chipper::ChipParameterRole::source9Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Ch 9 / TOM+CYM Level");
    ok &= expectMacroLabel(chipper::ChipMode::scc, chipper::MacroKind::powerUp, "SCC Power Wave");
    ok &= expectPreset(chipper::ChipMode::scc, "scc-power-wave");
    ok &= expectSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Wave Shape");
    ok &= expectSegmentedRegister(chipper::ChipMode::scc, chipper::ChipParameterRole::waveShape, 5, "Follow");
    ok &= expectSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::source5Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 5");
    ok &= expectSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::source5Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Channel 5 Level");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::nes, "nes-hero-pulse");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::dmg, "dmg-wave-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::sid, "sid-dirty-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::ym2149, "ym-three-voice-arp");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::sn76489, "sn-psg-lead");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::ym2612, "opn2-feedback-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::opl3, "opl2-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::spc700, "spc700-soft-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::pokey, "pokey-buzzy-lead");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::paula, "paula-tracker-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::huc6280, "huc-glass-lead");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::namcoWsg, "namco-arcade-lead");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::ym2151, "opm-arcade-bass");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::ym2413, "opll-ui-chime");
    ok &= expectPresetBrowserCatalog(chipper::ChipMode::scc, "scc-arcade-lead");
    ok &= expectSourceLaneCounts(chipper::ChipMode::nes, 4u, 4u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::sid, 3u, 3u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::spc700, 8u, 8u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::huc6280, 6u, 6u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::namcoWsg, 8u, 8u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::ym2612, 6u, 6u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::opl3, 9u, 9u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::ym2151, 8u, 8u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::ym2413, 9u, 9u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::scc, 5u, 5u);
    ok &= expectSourceLaneCounts(chipper::ChipMode::paula, 4u, 4u);

    ok &= expect(chipper::chipHasParameterSurface(chipper::ChipMode::ym2149,
                                                  chipper::ChipParameterRole::source1Enabled,
                                                  chipper::ControlSurface::sourceCards),
                 "YM2149 channel A should expose source cards");
    ok &= expect(chipper::chipHasParameterSurface(chipper::ChipMode::sn76489,
                                                  chipper::ChipParameterRole::source4Enabled,
                                                  chipper::ControlSurface::sourceCards),
                 "SN76489 noise should expose source cards");
    ok &= expect(chipper::chipHasParameterSurface(chipper::ChipMode::sid,
                                                  chipper::ChipParameterRole::source1Enabled,
                                                  chipper::ControlSurface::sourceCards),
                 "SID voice 1 should expose source cards");
    ok &= expect(! chipper::chipHasParameterSurface(chipper::ChipMode::sid,
                                                    chipper::ChipParameterRole::source4Enabled,
                                                    chipper::ControlSurface::sourceCards),
                 "SID should not expose a fourth voice source card yet");
    ok &= expectLiveSourceLevelSpecs();
    ok &= expectAutomatableDescriptorParametersHaveMidiCc();
    ok &= expectVerificationDisclosure();
    ok &= expectFmRegisterHelpers();
    ok &= expectWavetableRegisterHelpers();
    ok &= expectSampleRegisterHelpers();

    return ok ? 0 : 1;
}
