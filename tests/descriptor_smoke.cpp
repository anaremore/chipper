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

bool expectControlGroup(chipper::ChipMode mode,
                        const std::string& id,
                        const std::string& expectedGroup)
{
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto iter = std::find_if(descriptor.controls.begin(),
                                   descriptor.controls.end(),
                                   [&](const chipper::ControlDescriptor& control)
                                   {
                                       return control.id == id;
                                   });

    bool ok = true;
    ok &= expect(iter != descriptor.controls.end(), id + " control descriptor should exist");
    if (iter == descriptor.controls.end())
        return false;

    ok &= expect(iter->group == expectedGroup,
                 id + " control descriptor should use " + expectedGroup + " wording");
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

bool expectChipCatalogIsProductReady()
{
    constexpr std::array expectedModes {
        chipper::ChipMode::nes,
        chipper::ChipMode::dmg,
        chipper::ChipMode::sid,
        chipper::ChipMode::ym2149,
        chipper::ChipMode::sn76489,
        chipper::ChipMode::ym2612,
        chipper::ChipMode::opl3,
        chipper::ChipMode::spc700,
        chipper::ChipMode::pokey,
        chipper::ChipMode::paula,
        chipper::ChipMode::huc6280,
        chipper::ChipMode::namcoWsg,
        chipper::ChipMode::ym2151,
        chipper::ChipMode::ym2413,
        chipper::ChipMode::scc
    };

    const auto modes = chipper::chipModeOrder();
    bool ok = true;
    ok &= expect(modes.size() == expectedModes.size(), "chip selector should expose only named hardware families");

    const auto count = std::min(modes.size(), expectedModes.size());
    for (size_t index = 0; index < count; ++index)
    {
        ok &= expect(modes[index] == expectedModes[index],
                     "chip selector order changed at index " + std::to_string(index));

        const auto& descriptor = chipper::descriptorFor(modes[index]);
        ok &= expect(descriptor.displayName.find("Arcade Hybrid") == std::string::npos,
                     descriptor.displayName + " should not resurrect the undefined Arcade Hybrid mode");
        ok &= expect(descriptor.displayName.find("Custom") == std::string::npos,
                     descriptor.displayName + " should not resurrect the undefined Custom mode");

        const auto presets = chipper::presetsForChip(modes[index]);
        ok &= expect(! presets.empty(), descriptor.displayName + " should have audible factory presets in the UI");
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

bool expectWavetableWaveSpec(chipper::ChipMode mode,
                             chipper::ChipParameterRole role,
                             const std::string& label,
                             const std::vector<std::string>& expectedChoices)
{
    bool ok = true;
    ok &= expectSpec(mode, role, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, label);
    ok &= expectSpecGroup(mode, role, mode == chipper::ChipMode::paula ? "Sample" : "Wave");
    ok &= expectChoiceLabels(mode, role, expectedChoices, label);
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
            ok &= expect(! spec->label.empty(), spec->id + " should have a visible source-level label");
            ok &= expect(spec->label.find("Level") != std::string::npos, spec->id + " should name the visible Level control");
            ok &= expect(! spec->help.empty(), spec->id + " should explain the native trim path");

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

bool expectLiveSourceCardSpecs()
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
    constexpr std::array sourceEnabledRoles {
        chipper::ChipParameterRole::source1Enabled,
        chipper::ChipParameterRole::source2Enabled,
        chipper::ChipParameterRole::source3Enabled,
        chipper::ChipParameterRole::source4Enabled,
        chipper::ChipParameterRole::source5Enabled,
        chipper::ChipParameterRole::source6Enabled,
        chipper::ChipParameterRole::source7Enabled,
        chipper::ChipParameterRole::source8Enabled,
        chipper::ChipParameterRole::source9Enabled
    };

    for (const auto mode : liveModes)
    {
        const auto expectedSourceCount = mode == chipper::ChipMode::sid ? 3u : chipper::visibleSourceCountForMode(mode);
        for (size_t index = 0; index < expectedSourceCount; ++index)
        {
            const auto role = sourceEnabledRoles[index];
            const auto* spec = chipper::parameterSpecFor(mode, role);
            ok &= expect(spec != nullptr, "live chip missing source-card enable spec");
            if (spec == nullptr)
                continue;

            ok &= expect(spec->kind == chipper::ParameterKind::booleanToggle, spec->id + " should be a source-card toggle");
            ok &= expect(spec->surface == chipper::ControlSurface::sourceCards, spec->id + " should stay on source cards");
            ok &= expect(spec->group == "Sources", spec->id + " should stay in the Sources group");
            ok &= expect(! spec->label.empty(), spec->id + " should have a visible source-card label");
            ok &= expect(! spec->help.empty(), spec->id + " should explain the native lane");

            const auto* parameterId = chipper::parameterIdForChipParameterRole(role);
            ok &= expect(parameterId != nullptr, spec->id + " has no APVTS parameter id");
            if (parameterId != nullptr)
                ok &= expect(chipper::midiControllerForParameterId(parameterId) >= 0, spec->id + " has no MIDI CC mapping");
        }

        if (mode == chipper::ChipMode::sid)
            ok &= expect(chipper::parameterSpecFor(mode, chipper::ChipParameterRole::source4Enabled) == nullptr,
                         "SID should expose only its three oscillator voice toggles");
    }

    return ok;
}

bool expectSpc700VoiceSampleSlotSpecs()
{
    bool ok = true;
    constexpr std::array sampleSlotRoles {
        chipper::ChipParameterRole::spc700Voice1SampleSlot,
        chipper::ChipParameterRole::spc700Voice2SampleSlot,
        chipper::ChipParameterRole::spc700Voice3SampleSlot,
        chipper::ChipParameterRole::spc700Voice4SampleSlot,
        chipper::ChipParameterRole::spc700Voice5SampleSlot,
        chipper::ChipParameterRole::spc700Voice6SampleSlot,
        chipper::ChipParameterRole::spc700Voice7SampleSlot,
        chipper::ChipParameterRole::spc700Voice8SampleSlot
    };

    for (size_t index = 0; index < sampleSlotRoles.size(); ++index)
    {
        const auto role = sampleSlotRoles[index];
        const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::spc700, role);
        ok &= expect(spec != nullptr, "SPC700 missing per-voice sample slot spec");
        if (spec == nullptr)
            continue;

        const auto expectedLabel = "Voice " + std::to_string(index + 1) + " Sample";
        ok &= expect(spec->label == expectedLabel, spec->id + " should keep a per-voice sample label");
        ok &= expect(spec->kind == chipper::ParameterKind::steppedNumeric, spec->id + " should be a stepped sample-bank index");
        ok &= expect(spec->surface == chipper::ControlSurface::menu, spec->id + " should use the standard menu surface");
        ok &= expect(spec->group == "Sample", spec->id + " should stay in the Sample group");
        ok &= expect(spec->minValue == 0.0f && spec->maxValue == 32.0f, spec->id + " should cover preset plus 32 bank slots");
        ok &= expect(! spec->help.empty(), spec->id + " should explain sample-bank pinning");
        ok &= expect(spec->help.find("loaded sample-bank slot") != std::string::npos,
                     spec->id + " should mention loaded sample-bank slots");

        const auto* parameterId = chipper::parameterIdForChipParameterRole(role);
        ok &= expect(parameterId != nullptr, spec->id + " has no APVTS parameter id");
        if (parameterId != nullptr)
            ok &= expect(chipper::midiControllerForParameterId(parameterId) >= 0, spec->id + " has no MIDI CC mapping");
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

bool expectEnvelopeModels()
{
    bool ok = true;

    ok &= expect(chipper::envelopeModelFor(chipper::ChipMode::sid) == chipper::EnvelopeModel::nativeAdsr,
                 "SID should be classified as native ADSR");
    ok &= expect(chipper::envelopeModelFor(chipper::ChipMode::spc700) == chipper::EnvelopeModel::nativeAdsr,
                 "SPC700/S-DSP should be classified as native ADSR/GAIN");

    for (const auto mode : { chipper::ChipMode::ym2612,
                             chipper::ChipMode::opl3,
                             chipper::ChipMode::ym2151,
                             chipper::ChipMode::ym2413 })
    {
        ok &= expect(chipper::envelopeModelFor(mode) == chipper::EnvelopeModel::nativeOperatorEg,
                     "FM chips should be classified as native operator EG");
    }

    for (const auto mode : { chipper::ChipMode::nes,
                             chipper::ChipMode::dmg,
                             chipper::ChipMode::ym2149 })
    {
        ok &= expect(chipper::envelopeModelFor(mode) == chipper::EnvelopeModel::nativeNonAdsr,
                     "NES, DMG, and YM/AY should be classified as native non-ADSR envelope chips");
    }

    for (const auto mode : { chipper::ChipMode::sn76489,
                             chipper::ChipMode::pokey,
                             chipper::ChipMode::paula,
                             chipper::ChipMode::huc6280,
                             chipper::ChipMode::namcoWsg,
                             chipper::ChipMode::scc })
    {
        ok &= expect(chipper::envelopeModelFor(mode) == chipper::EnvelopeModel::chipperAmpHelper,
                     "chips without native ADSR/envelope generators should use Chipper helper envelope labels");
    }

    ok &= expect(std::string(chipper::envelopeModelLabel(chipper::EnvelopeModel::nativeAdsr)) == "Native ADSR / GAIN",
                 "native ADSR envelope label should stay user-facing");
    ok &= expect(std::string(chipper::envelopeModelLabel(chipper::EnvelopeModel::nativeOperatorEg)) == "Native Operator EG",
                 "operator EG envelope label should stay user-facing");
    ok &= expect(std::string(chipper::envelopeModelLabel(chipper::EnvelopeModel::nativeNonAdsr)) == "Native Envelope",
                 "native non-ADSR envelope label should stay honest");
    ok &= expect(std::string(chipper::envelopeModelLabel(chipper::EnvelopeModel::chipperAmpHelper)) == "Chipper Amp Env",
                 "helper envelope label should stay honest");

    ok &= expect(chipper::descriptorFor(chipper::ChipMode::spc700).modules[3].title == "ADSR / Gain",
                 "SPC700 envelope module should expose native S-DSP ADSR/GAIN wording");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2612).modules[3].title == "Operator EG",
                 "YM2612 envelope module should expose native operator EG wording");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2413).modules[3].title == "ROM Envelope",
                 "YM2413 envelope module should expose ROM envelope wording until custom patch EG exists");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::pokey).modules[3].title == "AUDV Gate",
                 "POKEY envelope module should name the AUDV helper path and avoid ADSR wording");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::paula).modules[3].title == "Tracker Amp Env",
                 "Paula envelope module should identify the tracker helper layer");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::huc6280).modules[3].title == "Shared Amp Env",
                 "HuC6280 envelope module should identify its shared volume helper");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::namcoWsg).modules[3].title == "Shared Amp Env",
                 "Namco WSG envelope module should identify its shared lane-volume helper");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::scc).modules[3].title == "Shared Amp Env",
                 "SCC envelope module should identify its shared wave-channel helper");

    return ok;
}

bool expectHelperEnvelopeModulesStayHonest()
{
    bool ok = true;

    for (const auto mode : { chipper::ChipMode::sn76489,
                             chipper::ChipMode::pokey,
                             chipper::ChipMode::paula,
                             chipper::ChipMode::huc6280,
                             chipper::ChipMode::namcoWsg,
                             chipper::ChipMode::scc })
    {
        const auto& descriptor = chipper::descriptorFor(mode);
        const auto module = std::find_if(descriptor.modules.begin(),
                                         descriptor.modules.end(),
                                         [] (const chipper::ModuleDescriptor& candidate)
                                         {
                                             return candidate.id == "envelope";
                                         });

        ok &= expect(module != descriptor.modules.end(),
                     descriptor.displayName + " should expose an honest helper envelope module");

        if (module == descriptor.modules.end())
            continue;

        ok &= expect(module->title.find("ADSR") == std::string::npos,
                     descriptor.displayName + " helper envelope module should not claim native ADSR");
        ok &= expect(module->summary.find("NES") == std::string::npos,
                     descriptor.displayName + " helper envelope module should not reuse NES decay copy");
        ok &= expect(module->summary.find("helper") != std::string::npos
                         || module->summary.find("Helper") != std::string::npos
                         || module->summary.find("gate") != std::string::npos
                         || module->summary.find("Gate") != std::string::npos,
                     descriptor.displayName + " helper envelope module should disclose the musical helper layer");

        const auto* envelopeSpec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::envelopeDecay);
        ok &= expect(envelopeSpec != nullptr,
                     descriptor.displayName + " should expose its helper envelope parameter spec");
        if (envelopeSpec == nullptr)
            continue;

        ok &= expect(envelopeSpec->label.find("ADSR") == std::string::npos,
                     descriptor.displayName + " helper envelope parameter should not claim native ADSR");
        ok &= expect(envelopeSpec->help.find("NES") == std::string::npos,
                     descriptor.displayName + " helper envelope parameter should not reuse NES decay help");
        ok &= expect(envelopeSpec->help.find("helper") != std::string::npos
                         || envelopeSpec->help.find("Helper") != std::string::npos
                         || envelopeSpec->help.find("gate") != std::string::npos
                         || envelopeSpec->help.find("Gate") != std::string::npos,
                     descriptor.displayName + " helper envelope parameter should explain the helper layer");
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
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2612, opn2Lead, 1) == 6u, "YM2612 algorithm 4 should treat operator 2 as a carrier");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2612, opn2Lead, 3) == 6u, "YM2612 helper should resolve carrier total level");
    auto opn2Trimmed = opn2Lead;
    opn2Trimmed.fmOperatorLevels = { 0.0f, 0.5f, 0.5f, 1.0f };
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2612, opn2Trimmed, 0) == 62u, "YM2612 operator 1 level trim should attenuate total level around neutral");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2612, opn2Trimmed, 3) == 0u, "YM2612 operator 4 level trim should boost total level around neutral");
    auto opn2MultiplierOverride = opn2Lead;
    opn2MultiplierOverride.fmOperatorMultipliers = { 1, 0, 0, 16 };
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2612, opn2MultiplierOverride, 0) == 0u, "YM2612 operator 1 multiplier override should write native 0.5x nibble");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2612, opn2MultiplierOverride, 1) == 9u, "YM2612 operator 2 multiplier should follow Operator Tone when not overridden");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2612, opn2MultiplierOverride, 3) == 15u, "YM2612 operator 4 multiplier override should write native 15x nibble");
    auto opn2AttackOverride = opn2Lead;
    opn2AttackOverride.fmOperatorAttackRates = { 1, 0, 0, 32 };
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opn2AttackOverride, 0).attackRate == 0u, "YM2612 operator 1 attack override should write native AR 0");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opn2AttackOverride, 1).attackRate == 31u, "YM2612 operator 2 attack should follow Envelope Shape when not overridden");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opn2AttackOverride, 3).attackRate == 31u, "YM2612 operator 4 attack override should write native AR 31");
    auto opn2DecayOverride = opn2Lead;
    opn2DecayOverride.fmOperatorDecayRates = { 1, 0, 0, 32 };
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opn2DecayOverride, 0).decayRate == 0u, "YM2612 operator 1 decay override should write native D1R 0");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opn2DecayOverride, 1).decayRate == chipper::ym2612EnvelopeRegistersForPatch(opn2Lead, 1).decayRate,
                 "YM2612 operator 2 decay should follow Envelope Shape when not overridden");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opn2DecayOverride, 3).decayRate == 31u, "YM2612 operator 4 decay override should write native D1R 31");
    if (const auto* preset = chipper::presetById("opn2-feedback-bass"))
    {
        const auto levels = chipper::fmOperatorLevelsForPreset(*preset);
        ok &= expect(levels == std::array<float, 4> { 0.58f, 0.66f, 0.46f, 0.62f },
                     "OPN2 Feedback Bass should carry curated operator-level trims");
        ok &= expect(chipper::patchConfigForPreset(*preset).fmOperatorLevels == levels,
                     "OPN2 preset patch config should apply curated operator-level trims");
    }
    else
    {
        ok &= expect(false, "OPN2 Feedback Bass preset should exist");
    }
    if (const auto* preset = chipper::presetById("opn2-neutral-patch"))
    {
        ok &= expect(chipper::fmOperatorLevelsForPreset(*preset) == std::array<float, 4> { 0.5f, 0.5f, 0.5f, 0.5f },
                     "OPN2 Neutral Patch should keep neutral operator-level trims");
    }
    else
    {
        ok &= expect(false, "OPN2 Neutral Patch preset should exist");
    }
    ok &= expect(chipper::ym2612PanBitsForPatch(opn2Lead, 0) == 0xc0u, "YM2612 lead macro should resolve to centered pan bits");
    ok &= expect(chipper::ym2612DacModeForPatch(opn2Lead) == 1u, "YM2612 lead macro should keep channel 6 in FM mode");
    const auto opn2LeadModulatorEnvelope = chipper::ym2612EnvelopeRegistersForPatch(opn2Lead, 0);
    ok &= expect(opn2LeadModulatorEnvelope.attackRate == 0x1fu, "YM2612 lead envelope should use fast modulator attack");
    ok &= expect(opn2LeadModulatorEnvelope.decayRate == 0x04u, "YM2612 lead modulator decay should keep sustained FM notes playable");
    ok &= expect(opn2LeadModulatorEnvelope.sustainRate == 0x00u, "YM2612 lead envelope should hold sustain while a note is gated");
    ok &= expect(opn2LeadModulatorEnvelope.sustainRelease == 0x34u, "YM2612 lead modulator release byte should retain a stronger playable sustain level");

    const auto opmArp = chipper::makePatchConfig(chipper::ChipMode::ym2151,
                                                 chipper::MacroKind::arp,
                                                 0.0f,
                                                 0.4f,
                                                 0.5f,
                                                 0.7f);
    ok &= expect(chipper::ym2151AlgorithmForPatch(opmArp) == 7u, "YM2151 arp macro should resolve to algorithm 7");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmArp, 0) == 8u, "YM2151 helper should resolve operator 1 multiple");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmArp, 3) == 13u, "YM2151 helper should resolve operator 4 multiple");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2151, opmArp, 0) == 7u, "YM2151 algorithm 7 should treat operator 1 as a carrier");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2151, opmArp, 3) == 7u, "YM2151 helper should resolve carrier total level");
    auto opmTrimmed = opmArp;
    opmTrimmed.fmOperatorLevels = { 1.0f, 0.5f, 0.5f, 0.0f };
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2151, opmTrimmed, 0) == 0u, "YM2151 operator 1 level trim should boost total level around neutral");
    ok &= expect(chipper::fmOperatorTotalLevelForPatch(chipper::ChipMode::ym2151, opmTrimmed, 3) == 31u, "YM2151 operator 4 level trim should attenuate total level around neutral");
    auto opmMultiplierOverride = opmArp;
    opmMultiplierOverride.fmOperatorMultipliers = { 2, 0, 0, 16 };
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmMultiplierOverride, 0) == 1u, "YM2151 operator 1 multiplier override should write native 1x nibble");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmMultiplierOverride, 1) == 9u, "YM2151 operator 2 multiplier should follow Operator Tone when not overridden");
    ok &= expect(chipper::fmOperatorMultipleForPatch(chipper::ChipMode::ym2151, opmMultiplierOverride, 3) == 15u, "YM2151 operator 4 multiplier override should write native 15x nibble");
    auto opmAttackOverride = opmArp;
    opmAttackOverride.fmOperatorAttackRates = { 6, 0, 0, 32 };
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opmAttackOverride, 0).attackRate == 5u, "YM2151 shared helper operator 1 attack override should write native AR 5");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opmAttackOverride, 1).attackRate == 31u, "YM2151 shared helper operator 2 attack should follow Envelope Shape when not overridden");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opmAttackOverride, 3).attackRate == 31u, "YM2151 shared helper operator 4 attack override should write native AR 31");
    auto opmDecayOverride = opmArp;
    opmDecayOverride.fmOperatorDecayRates = { 6, 0, 0, 32 };
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opmDecayOverride, 0).decayRate == 5u, "YM2151 shared helper operator 1 decay override should write native D1R 5");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opmDecayOverride, 1).decayRate == chipper::ym2612EnvelopeRegistersForPatch(opmArp, 1).decayRate,
                 "YM2151 shared helper operator 2 decay should follow Envelope Shape when not overridden");
    ok &= expect(chipper::ym2612EnvelopeRegistersForPatch(opmDecayOverride, 3).decayRate == 31u, "YM2151 shared helper operator 4 decay override should write native D1R 31");
    if (const auto* preset = chipper::presetById("opm-hollow-pad"))
    {
        const auto levels = chipper::fmOperatorLevelsForPreset(*preset);
        ok &= expect(levels == std::array<float, 4> { 0.38f, 0.70f, 0.42f, 0.68f },
                     "OPM Hollow Pad should carry curated operator-level trims");
        ok &= expect(chipper::patchConfigForPreset(*preset).fmOperatorLevels == levels,
                     "OPM preset patch config should apply curated operator-level trims");
    }
    else
    {
        ok &= expect(false, "OPM Hollow Pad preset should exist");
    }
    const auto opmArpEnvelope = chipper::ym2612EnvelopeRegistersForPatch(opmArp, 0);
    ok &= expect(opmArpEnvelope.sustainRate == 0x00u, "YM2151 shared FM envelope helper should hold sustained arp notes");

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
    ok &= expect(opn2PadEnvelope.sustainRate == 0x00u, "YM2612 Pad sustain-rate should hold instead of fading held notes away");
    ok &= expect(opn2PadEnvelope.sustainRelease == 0x55u, "YM2612 Pad modulator sustain/release should map to register $80");

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
    ok &= expect(chipper::ym2413InstrumentForPatch(opllBass) == 13u, "YM2413 bass macro should resolve to Synth Bass OPLL instrument 13");
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
                                                           12);
    ok &= expect(chipper::ym2413InstrumentForPatch(opllExplicitBell) == 12u, "YM2413 explicit Vibes choice should resolve to OPLL instrument 12");
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
    const auto hucSplit = chipper::makePatchConfig(chipper::ChipMode::huc6280,
                                                   chipper::MacroKind::manual,
                                                   0.0f,
                                                   0.0f,
                                                   0.5f,
                                                   0.8f,
                                                   chipper::PlayMode::stack,
                                                   { true, true, true, true, true, true, true, true, true },
                                                   { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
                                                   0.0f,
                                                   0.0f,
                                                   1,
                                                   3,
                                                   4,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   2,
                                                   4,
                                                   2);
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::huc6280, hucSplit, 0) == 1u, "HuC6280 channel 1 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::huc6280, hucSplit, 1) == 4u, "HuC6280 channel 2 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::huc6280, hucSplit, 2) == 2u, "HuC6280 channel 3 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::huc6280, hucSplit, 3) == 3u, "HuC6280 channel 4 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::huc6280, hucSplit, 4) == 4u, "HuC6280 channel 5 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::huc6280, hucSplit, 5) == 2u, "HuC6280 channel 6 should use its own wave choice");

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
    const auto sccSplit = chipper::makePatchConfig(chipper::ChipMode::scc,
                                                   chipper::MacroKind::manual,
                                                   0.0f,
                                                   0.0f,
                                                   0.5f,
                                                   0.8f,
                                                   chipper::PlayMode::stack,
                                                   { true, true, true, true, true, true, true, true, true },
                                                   { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
                                                   0.0f,
                                                   0.0f,
                                                   1,
                                                   3,
                                                   4,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   4,
                                                   2);
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::scc, sccSplit, 0) == 1u, "SCC channel 1 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::scc, sccSplit, 1) == 4u, "SCC channel 2 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::scc, sccSplit, 2) == 2u, "SCC channel 3 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::scc, sccSplit, 3) == 3u, "SCC channel 4 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::scc, sccSplit, 4) == 4u, "SCC channel 5 should use its own wave choice");

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
    const auto namcoSplit = chipper::makePatchConfig(chipper::ChipMode::namcoWsg,
                                                     chipper::MacroKind::manual,
                                                     0.0f,
                                                     0.0f,
                                                     0.5f,
                                                     0.8f,
                                                     chipper::PlayMode::stack,
                                                     { true, true, true, true, true, true, true, true, true },
                                                     { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
                                                     0.0f,
                                                     0.0f,
                                                     1,
                                                     3,
                                                     4,
                                                     2,
                                                     3,
                                                     0,
                                                     0,
                                                     0,
                                                     2,
                                                     4,
                                                     2);
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 0) == 1u, "Namco WSG lane 1 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 1) == 4u, "Namco WSG lane 2 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 2) == 2u, "Namco WSG lane 3 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 3) == 3u, "Namco WSG lane 4 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 4) == 4u, "Namco WSG lane 5 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 5) == 2u, "Namco WSG lane 6 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 6) == 3u, "Namco WSG lane 7 should use its own wave choice");
    ok &= expect(chipper::wavetableWaveShapeForChannel(chipper::ChipMode::namcoWsg, namcoSplit, 7) == 2u, "Namco WSG lane 8 should use its own wave choice");

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

    ok &= expectChipCatalogIsProductReady();
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
    ok &= expectControlGroup(chipper::ChipMode::ym2413, "core", "Provenance");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2151).implemented, "YM2151 descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::ym2151).supportsChipPoly, "YM2151 should support Chip Poly across exposed melodic channels");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::scc).implemented, "SCC descriptor should be partially implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::scc).supportsChipPoly, "SCC should support Chip Poly across exposed wavetable channels");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl1, 4, "12.5%");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::pulse2Duty, 5, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::snNoiseMode, 3, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl1, 4, "12.5%");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::pulse2Duty, 5, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::waveShape, 5, "RAM");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::dmgWaveLevel, 5, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::dmgStereoRoute, 5, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::snNoiseMode, 3, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::sid, chipper::ChipParameterRole::waveShape, 9, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::sid, chipper::ChipParameterRole::snNoiseMode, 5, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::sid, chipper::ChipParameterRole::dmgStereoRoute, 3, "Preset");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2149, chipper::ChipParameterRole::macroControl4, 3, "Noise");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymChannelAMix, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "A Mix");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymChannelBMix, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "B Mix");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymChannelCMix, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "C Mix");
    ok &= expectChoiceRegister(chipper::ChipMode::sn76489, chipper::ChipParameterRole::snNoiseMode, chipper::ControlSurface::menu, 5, "Preset");

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
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Pulse 1 Duty");
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::pulse2Duty, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Pulse 2 Duty");
    ok &= expectSpecHelpContains(chipper::ChipMode::dmg, chipper::ChipParameterRole::pulse2Duty, "NR21", "DMG pulse 2 duty help should mention the independent channel-2 duty register");
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
        "Preset",
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
        ok &= expect(! spec->choices.empty() && spec->choices.front().label == "Preset", "SID filter mode should use Preset as the inherited choice");
    }
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::stereoSpread, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Resonance");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::stereoSpread, "Filter");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidFilterRouting, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Filter Routing");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::sidFilterRouting, "Filter");
    if (const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::sid, chipper::ChipParameterRole::sidFilterRouting))
        ok &= expect(spec->choices.size() == 9u, "SID filter routing should expose nine choices");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "SID Model");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Pitch");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::envelopeDecay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Hardware Envelope Speed");
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
    ok &= expectPreset(chipper::ChipMode::ym2612, "opn2-dac-kick");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Pan");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::dmgStereoRoute, "Output");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::dmgStereoRoute, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Envelope Shape");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::ymEnvelopeShape, "Envelope");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::snNoiseMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "DAC Mode");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::snNoiseMode, "Output");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::snNoiseMode, 3, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source5Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "FM Ch 5");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source6Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "FM Ch 6");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source5Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "FM Ch 5 Level");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::source6Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "FM Ch 6 Level");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1Level, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "OP1 Level");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator4Level, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "OP4 Level");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1Level, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1Multiplier, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP1 Mult");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator4Multiplier, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP4 Mult");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1Multiplier, chipper::ControlSurface::menu, 17, "Follow");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1Multiplier, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1AttackRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP1 Attack");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator4AttackRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP4 Attack");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1AttackRate, chipper::ControlSurface::menu, 33, "Follow");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1AttackRate, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1DecayRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP1 Decay");
    ok &= expectSpec(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator4DecayRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP4 Decay");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1DecayRate, chipper::ControlSurface::menu, 33, "Follow");
    ok &= expectSpecGroup(chipper::ChipMode::ym2612, chipper::ChipParameterRole::fmOperator1DecayRate, "Operators");
    ok &= expectMacroLabel(chipper::ChipMode::opl3, chipper::MacroKind::drum, "OPL2 Rhythm Kit");
    ok &= expectMacroLabel(chipper::ChipMode::spc700, chipper::MacroKind::drum, "SPC700 Drum Map");
    ok &= expectPreset(chipper::ChipMode::spc700, "spc700-stage-clear");
    ok &= expectPreset(chipper::ChipMode::spc700, "spc700-noise-snare");
    ok &= expectPreset(chipper::ChipMode::spc700, "spc700-pmon-shimmer");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Sample Shape");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::waveShape, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Loop Mode");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, "Sample");
    ok &= expectChoiceRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, chipper::ControlSurface::menu, 3, "Preset");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Envelope Shape");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::ymEnvelopeShape, "Envelope");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Pitch / PMON");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700,
                                 chipper::ChipParameterRole::macroControl2,
                                 "partial PMON-style",
                                 "SPC700 pitch control help should disclose PMON mapping");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Echo Color");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::macroControl3, "Echo");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700,
                                 chipper::ChipParameterRole::macroControl3,
                                 "stereo echo helper",
                                 "SPC700 Echo Color help should disclose the current echo approximation");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Sample Playback");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, "Sample");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, 3, "Manual");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700, chipper::ChipParameterRole::nesDmcPlaybackMode, "manual slot", "SPC700 sample playback help should explain manual-slot behavior");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700, chipper::ChipParameterRole::dmgStereoRoute, "separate from Sample Playback", "SPC700 loop mode help should distinguish voice lifetime from bank mapping");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::snNoiseMode, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Noise Source");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::snNoiseMode, "Noise");
    ok &= expectSegmentedRegister(chipper::ChipMode::spc700, chipper::ChipParameterRole::snNoiseMode, 5, "Preset");
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
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::spc700Voice1SampleSlot, chipper::ParameterKind::steppedNumeric, chipper::ControlSurface::menu, "Voice 1 Sample");
    ok &= expectSpec(chipper::ChipMode::spc700, chipper::ChipParameterRole::spc700Voice8SampleSlot, chipper::ParameterKind::steppedNumeric, chipper::ControlSurface::menu, "Voice 8 Sample");
    ok &= expectSpecGroup(chipper::ChipMode::spc700, chipper::ChipParameterRole::spc700Voice1SampleSlot, "Sample");
    ok &= expectSpecHelpContains(chipper::ChipMode::spc700, chipper::ChipParameterRole::spc700Voice8SampleSlot, "loaded sample-bank slot", "SPC700 voice sample slot help should explain per-voice bank pinning");
    ok &= expectMacroLabel(chipper::ChipMode::pokey, chipper::MacroKind::lead, "POKEY Buzzy Lead");
    ok &= expectPreset(chipper::ChipMode::pokey, "pokey-alien-laser");
    ok &= expectSpec(chipper::ChipMode::pokey, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Distortion Code");
    ok &= expectSegmentedRegister(chipper::ChipMode::pokey, chipper::ChipParameterRole::waveShape, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::pokey, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "AUDCTL Pairing");
    ok &= expectSegmentedRegister(chipper::ChipMode::pokey, chipper::ChipParameterRole::dmgStereoRoute, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::pokey, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "AUDCTL Filter");
    ok &= expectSegmentedRegister(chipper::ChipMode::pokey, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Preset");
    ok &= expectMacroLabel(chipper::ChipMode::paula, chipper::MacroKind::arp, "Paula Tracker Arp");
    ok &= expectPreset(chipper::ChipMode::paula, "paula-stab-chord");
    ok &= expectWavetableWaveSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::waveShape, "Ch 1 Sample", { "Preset", "Ramp", "Tri", "Sine", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::sidVoice2WaveShape, "Ch 2 Sample", { "Preset", "Ramp", "Tri", "Sine", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::sidVoice3WaveShape, "Ch 3 Sample", { "Preset", "Ramp", "Tri", "Sine", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::pulse2Duty, "Ch 4 Sample", { "Preset", "Ramp", "Tri", "Sine", "Noise" });
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source1Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 1 L");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source2Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 2 R");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source3Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 3 R");
    ok &= expectSpec(chipper::ChipMode::paula, chipper::ChipParameterRole::source4Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 4 L");
    ok &= expectMacroLabel(chipper::ChipMode::huc6280, chipper::MacroKind::lead, "HuC6280 Glass Lead");
    ok &= expectPreset(chipper::ChipMode::huc6280, "huc-boss-alert");
    ok &= expectWavetableWaveSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::waveShape, "Ch 1 Wave", { "Preset", "Ramp", "Tri", "Square", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::sidVoice2WaveShape, "Ch 2 Wave", { "Preset", "Ramp", "Tri", "Square", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::sidVoice3WaveShape, "Ch 3 Wave", { "Preset", "Ramp", "Tri", "Square", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::pulse2Duty, "Ch 4 Wave", { "Preset", "Ramp", "Tri", "Square", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::dmgWaveLevel, "Ch 5 Wave", { "Preset", "Ramp", "Tri", "Square", "Noise" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::snNoiseMode, "Ch 6 Wave", { "Preset", "Ramp", "Tri", "Square", "Noise" });
    ok &= expectSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::dmgStereoRoute, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Ch 1/2 LFO");
    ok &= expectSegmentedRegister(chipper::ChipMode::huc6280, chipper::ChipParameterRole::dmgStereoRoute, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::source5Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 5");
    ok &= expectSpec(chipper::ChipMode::huc6280, chipper::ChipParameterRole::source6Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Channel 6");
    ok &= expectMacroLabel(chipper::ChipMode::namcoWsg, chipper::MacroKind::arp, "Namco Tracker Arp");
    ok &= expectPreset(chipper::ChipMode::namcoWsg, "namco-start-button");
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::waveShape, "Lane 1 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::sidVoice2WaveShape, "Lane 2 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::sidVoice3WaveShape, "Lane 3 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::pulse2Duty, "Lane 4 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::dmgWaveLevel, "Lane 5 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::snNoiseMode, "Lane 6 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::ymEnvelopeShape, "Lane 7 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::dmgStereoRoute, "Lane 8 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::source8Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "Lane 8");
    ok &= expectSpec(chipper::ChipMode::namcoWsg, chipper::ChipParameterRole::source8Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Lane 8 Level");
    ok &= expectMacroLabel(chipper::ChipMode::ym2151, chipper::MacroKind::lead, "OPM Metallic Lead");
    ok &= expectPreset(chipper::ChipMode::ym2151, "opm-metallic-lead");
    ok &= expectMacroSourceMask(chipper::ChipMode::ym2151, chipper::MacroKind::drum, { false, false, true, true });
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Algorithm");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2151, chipper::ChipParameterRole::waveShape, chipper::ControlSurface::menu, 9, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Algorithm Bias");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Feedback");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Operator Tone");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "FM Level");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1Level, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "OP1 Level");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator4Level, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "OP4 Level");
    ok &= expectSpecGroup(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1Level, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1Multiplier, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP1 Mult");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator4Multiplier, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP4 Mult");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1Multiplier, chipper::ControlSurface::menu, 17, "Follow");
    ok &= expectSpecGroup(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1Multiplier, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1AttackRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP1 Attack");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator4AttackRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP4 Attack");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1AttackRate, chipper::ControlSurface::menu, 33, "Follow");
    ok &= expectSpecGroup(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1AttackRate, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1DecayRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP1 Decay");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator4DecayRate, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "OP4 Decay");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1DecayRate, chipper::ControlSurface::menu, 33, "Follow");
    ok &= expectSpecGroup(chipper::ChipMode::ym2151, chipper::ChipParameterRole::fmOperator1DecayRate, "Operators");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Envelope Shape");
    ok &= expectSpecGroup(chipper::ChipMode::ym2151, chipper::ChipParameterRole::ymEnvelopeShape, "Envelope");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2151, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source7Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPM Ch 7");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source8Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPM Ch 8");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source7Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "OPM Ch 7 Level");
    ok &= expectSpec(chipper::ChipMode::ym2151, chipper::ChipParameterRole::source8Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "OPM Ch 8 Level");
    ok &= expectMacroLabel(chipper::ChipMode::ym2413, chipper::MacroKind::coin, "OPLL UI Chime");
    ok &= expectPreset(chipper::ChipMode::ym2413, "opll-soft-keys");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::waveShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::menu, "Instrument");
    ok &= expectChoiceRegister(chipper::ChipMode::ym2413, chipper::ChipParameterRole::waveShape, chipper::ControlSurface::menu, 16, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Rhythm Mode");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2413, chipper::ChipParameterRole::ymEnvelopeShape, 3, "Preset");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::source9Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPLL Ch 9");
    ok &= expectSpec(chipper::ChipMode::ym2413, chipper::ChipParameterRole::source9Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Ch 9 Level");
    ok &= expectSpec(chipper::ChipMode::opl3, chipper::ChipParameterRole::ymEnvelopeShape, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Rhythm Mode");
    ok &= expectSegmentedRegister(chipper::ChipMode::opl3, chipper::ChipParameterRole::ymEnvelopeShape, 3, "Preset");
    ok &= expectPreset(chipper::ChipMode::opl3, "opl2-rhythm-kit");
    ok &= expectSpec(chipper::ChipMode::opl3, chipper::ChipParameterRole::source9Enabled, chipper::ParameterKind::booleanToggle, chipper::ControlSurface::sourceCards, "OPL Ch 9 / TOM+CYM");
    ok &= expectSpec(chipper::ChipMode::opl3, chipper::ChipParameterRole::source9Level, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Ch 9 / TOM+CYM Level");
    ok &= expectMacroLabel(chipper::ChipMode::scc, chipper::MacroKind::powerUp, "SCC Power Wave");
    ok &= expectPreset(chipper::ChipMode::scc, "scc-power-wave");
    ok &= expectWavetableWaveSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::waveShape, "Ch 1 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::sidVoice2WaveShape, "Ch 2 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::sidVoice3WaveShape, "Ch 3 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::pulse2Duty, "Ch 4 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
    ok &= expectWavetableWaveSpec(chipper::ChipMode::scc, chipper::ChipParameterRole::dmgWaveLevel, "Ch 5 Wave", { "Preset", "Ramp", "Tri", "Pulse", "Steps" });
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
    ok &= expectLiveSourceCardSpecs();
    ok &= expectLiveSourceLevelSpecs();
    ok &= expectSpc700VoiceSampleSlotSpecs();
    ok &= expectAutomatableDescriptorParametersHaveMidiCc();
    ok &= expectVerificationDisclosure();
    ok &= expectEnvelopeModels();
    ok &= expectHelperEnvelopeModulesStayHonest();
    ok &= expectFmRegisterHelpers();
    ok &= expectWavetableRegisterHelpers();
    ok &= expectSampleRegisterHelpers();

    return ok ? 0 : 1;
}
