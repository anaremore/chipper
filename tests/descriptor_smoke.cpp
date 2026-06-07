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

bool expectLiveSourceLevelSpecs()
{
    bool ok = true;
    constexpr std::array liveModes {
        chipper::ChipMode::nes,
        chipper::ChipMode::dmg,
        chipper::ChipMode::sid,
        chipper::ChipMode::ym2149,
        chipper::ChipMode::sn76489
    };
    constexpr std::array sourceLevelRoles {
        chipper::ChipParameterRole::source1Level,
        chipper::ChipParameterRole::source2Level,
        chipper::ChipParameterRole::source3Level,
        chipper::ChipParameterRole::source4Level
    };

    for (const auto mode : liveModes)
    {
        const auto expectedSourceCount = mode == chipper::ChipMode::sid ? 3u : 4u;
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
        chipper::ChipMode::sn76489
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

    const auto& planned = chipper::descriptorFor(chipper::ChipMode::ym2612).verification;
    ok &= expect(planned.badge == "Unverified planned", "planned chip should disclose unverified status");
    ok &= expect(! planned.cycleAccurate, "planned chip should not claim cycle accuracy");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;

    ok &= expect(chipper::descriptorFor(chipper::ChipMode::nes).implemented, "NES descriptor should be implemented");
    ok &= expect(chipper::descriptorFor(chipper::ChipMode::sid).implemented, "SID descriptor should be partially implemented");
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
    ok &= expectSegmentedRegister(chipper::ChipMode::sn76489, chipper::ChipParameterRole::snNoiseMode, 5, "Follow");

    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::macroControl2, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Sweep Motion");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::pulse2Duty, chipper::ParameterKind::chipRegister, chipper::ControlSurface::segmentedChoice, "Pulse 2 Duty");
    ok &= expectSpec(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcDirectLevel, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "DMC Direct Level");
    ok &= expectSpecGroup(chipper::ChipMode::nes, chipper::ChipParameterRole::nesDmcDirectLevel, "DMC");
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
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2PulseWidth, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Voice 2 PW");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice2PulseWidth, "Voices");
    ok &= expectSpec(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3PulseWidth, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Voice 3 PW");
    ok &= expectSpecGroup(chipper::ChipMode::sid, chipper::ChipParameterRole::sidVoice3PulseWidth, "Voices");
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
    if (const auto* spec = chipper::parameterSpecFor(chipper::ChipMode::ym2149, chipper::ChipParameterRole::ymEnvelopeShape))
    {
        ok &= expect(spec->choices.size() == 21u, "YM envelope shape should expose friendly shortcuts plus 16 hardware codes");
        ok &= expect(! spec->choices.empty() && spec->choices.front().label == "Fixed", "YM envelope shape should keep Fixed as the inherited choice");
        ok &= expect(spec->choices.size() > 20u && spec->choices[20].label == "0x0F", "YM envelope shape should expose register code 0x0F");
    }
    ok &= expectSpec(chipper::ChipMode::ym2149, chipper::ChipParameterRole::stereoSpread, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Stereo Spread");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl1, chipper::ParameterKind::macro, chipper::ControlSurface::slider, "Tone Stack");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl3, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Bias");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::macroControl4, chipper::ParameterKind::chipRegister, chipper::ControlSurface::slider, "Noise Level");
    ok &= expectSpec(chipper::ChipMode::sn76489, chipper::ChipParameterRole::stereoSpread, chipper::ParameterKind::continuous, chipper::ControlSurface::slider, "Stereo Spread");

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

    return ok ? 0 : 1;
}
