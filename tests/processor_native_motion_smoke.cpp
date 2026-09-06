#include "PluginProcessor.h"
#include "State/MotionState.h"
#include "State/PluginStateSchema.h"
#include <iostream>

namespace
{
bool check(bool value, const char* message)
{
    if (! value) std::cerr << message << '\n';
    return value;
}
int field(const std::string& json, const char* key)
{
    return static_cast<int>(juce::JSON::parse(juce::String(json))[key]);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    using namespace chipper;
    auto ok = true;
    auto core = createChipCore(ChipMode::ym2149, AccuracyMode::authentic);
    core->reset(48000, 1773400);
    core->writeRegister(6, 9);
    const auto resets = field(core->debugStateJson(), "envelopeResetCount");
    for (uint8_t choice = 1; choice <= 32; ++choice)
    {
        core->setYmNoiseMotion(choice);
        ok &= check(field(core->debugStateJson(), "noisePeriod") == choice - 1, "Native period mapping failed");
        ok &= check(core->exportRegisterState()[6].value == 9, "Transient Motion leaked into exported base registers");
    }
    core->writeRegister(6, 17);
    ok &= check(field(core->debugStateJson(), "noisePeriod") == 31, "Patch write displaced active Motion");
    core->setYmNoiseMotion(0);
    ok &= check(field(core->debugStateJson(), "noisePeriod") == 17, "Preset did not restore the latest base period");
    ok &= check(field(core->debugStateJson(), "envelopeResetCount") == resets, "Noise changes retriggered the envelope");

    auto processor = std::make_unique<ChipperAudioProcessor>();
    const auto setChoice = [&](const char* id, int choice)
    {
        auto* parameter = processor->getValueTreeState().getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(choice)));
    };
    for (int i = 0; i < parameters::chipModeChoices().size(); ++i)
        if (parameters::chipModeFromChoice(i) == ChipMode::ym2149) setChoice(parameters::id::chipMode, i);
    for (int i = 0; i < parameters::macroChoices().size(); ++i)
        if (parameters::macroFromChoice(i) == MacroKind::drum) setChoice(parameters::id::macro, i);
    processor->prepareToPlay(48000, 1500);
    MotionPattern pattern;
    pattern.enabled = true;
    pattern.rate = MotionRate::sixtyFourth;
    pattern.length = 2;
    pattern.steps[0].ymNoisePeriod = 2;
    pattern.steps[1].ymNoisePeriod = 32;
    processor->setMotionPattern(ChipMode::ym2149, pattern);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
    juce::AudioBuffer<float> first(2, 1500);
    processor->processBlock(first, midi);
    ok &= check(field(processor->currentCoreDebugStateJson(), "noisePeriod") == 1, "Step changed before frame 1500");
    const auto initialResets = field(processor->currentCoreDebugStateJson(), "envelopeResetCount");
    juce::AudioBuffer<float> next(2, 1);
    midi.clear();
    processor->processBlock(next, midi);
    ok &= check(field(processor->currentCoreDebugStateJson(), "noisePeriod") == 31, "Step did not change exactly at frame 1500");
    ok &= check(field(processor->currentCoreDebugStateJson(), "envelopeResetCount") == initialResets, "Hold step retriggered the envelope");

    auto xml = processor->createStateXml();
    auto restored = std::make_unique<ChipperAudioProcessor>();
    ok &= check(restored->restoreStateXml(*xml).wasOk()
                && restored->motionSnapshot(ChipMode::ym2149).pattern == pattern, "Native Motion state round trip failed");
    auto* saved = xml->getChildByName(state::motionStateTag)->getChildByName(state::motionPatternStateTag);
    saved->setAttribute("yn0", 33);
    ok &= check(restored->restoreStateXml(*xml).failed()
                && restored->motionSnapshot(ChipMode::ym2149).pattern == pattern, "Invalid native state mutated the processor");
    saved->setAttribute("yn0", 2);
    saved->setAttribute("mode", "nes");
    ok &= check(restored->restoreStateXml(*xml).failed(), "Unsupported chip accepted a native destination");
    saved->setAttribute("mode", "ym2149");
    xml->setAttribute(state::schemaVersionAttribute, 9);
    for (int i = 0; i < 8; ++i) saved->removeAttribute("yn" + juce::String(i));
    ok &= check(restored->restoreStateXml(*xml).wasOk()
                && restored->motionSnapshot(ChipMode::ym2149).pattern.steps[0].ymNoisePeriod == 0, "Schema 9 did not retain preset noise");
    pattern.enabled = false;
    processor->setMotionPattern(ChipMode::ym2149, pattern);
    processor->processBlock(next, midi);
    ok &= check(processor->motionSnapshot(ChipMode::ym2149).activeStep == -1, "Disabled motion remained active");
    return ok ? 0 : 1;
}
