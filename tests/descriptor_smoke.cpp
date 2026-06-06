#include "Engine/ChipDescriptors.h"
#include "Engine/ControlRegistry.h"

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

bool expectMacroSourceMask(chipper::ChipMode mode, chipper::MacroKind macro, std::array<bool, 4> expected)
{
    const auto& templ = chipper::macroTemplateFor(mode, macro);
    return expect(templ.sourceEnabled == expected, templ.label + " has unexpected source mask");
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

} // namespace

int main()
{
    bool ok = true;

    ok &= expect(chipper::descriptorFor(chipper::ChipMode::nes).implemented, "NES descriptor should be implemented");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl1, 4, "12.5%");
    ok &= expectSegmentedRegister(chipper::ChipMode::nes, chipper::ChipParameterRole::snNoiseMode, 3, "Macro");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl1, 4, "12.5%");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::waveShape, 5, "RAM");
    ok &= expectSegmentedRegister(chipper::ChipMode::dmg, chipper::ChipParameterRole::snNoiseMode, 3, "Macro");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2149, chipper::ChipParameterRole::macroControl4, 3, "Noise");
    ok &= expectSegmentedRegister(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymEnvelopeShape, 5, "Fixed");
    ok &= expectSegmentedRegister(chipper::ChipMode::sn76489, chipper::ChipParameterRole::snNoiseMode, 5, "Macro");

    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Sweep Motion");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Period");
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Clock");
    ok &= expectSpec(chipper::ChipMode::dmg, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Envelope Level");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Pitch");
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::envelopeDecay, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Envelope Speed");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Tone Stack");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Level");

    ok &= expectMacroSourceMask(chipper::ChipMode::nes, chipper::MacroKind::drum, { false, false, true, true });
    ok &= expectMacroSourceMask(chipper::ChipMode::dmg, chipper::MacroKind::bass, { false, false, true, false });
    ok &= expectMacroSourceMask(chipper::ChipMode::ym2149, chipper::MacroKind::drum, { true, false, false, true });
    ok &= expectMacroSourceMask(chipper::ChipMode::sn76489, chipper::MacroKind::drum, { false, false, false, true });
    ok &= expectMacroLabel(chipper::ChipMode::nes, chipper::MacroKind::coin, "NES Coin Blip");
    ok &= expectMacroLabel(chipper::ChipMode::dmg, chipper::MacroKind::bass, "DMG Wave Bass");
    ok &= expectMacroLabel(chipper::ChipMode::ym2149, chipper::MacroKind::drum, "YM Noise Perc");
    ok &= expectMacroLabel(chipper::ChipMode::sn76489, chipper::MacroKind::drum, "PSG Drum");

    ok &= expect(chipper::chipHasParameterSurface(chipper::ChipMode::ym2149,
                                                  chipper::ChipParameterRole::source1Enabled,
                                                  chipper::ControlSurface::sourceCards),
                 "YM2149 channel A should expose source cards");
    ok &= expect(chipper::chipHasParameterSurface(chipper::ChipMode::sn76489,
                                                  chipper::ChipParameterRole::source4Enabled,
                                                  chipper::ControlSurface::sourceCards),
                 "SN76489 noise should expose source cards");
    ok &= expect(! chipper::chipHasParameterSurface(chipper::ChipMode::sid,
                                                    chipper::ChipParameterRole::source1Enabled,
                                                    chipper::ControlSurface::sourceCards),
                 "planned SID should not expose live source cards yet");
    ok &= expectAutomatableDescriptorParametersHaveMidiCc();

    return ok ? 0 : 1;
}
