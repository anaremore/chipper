#include "Parameters.h"

#include <array>
#include <iostream>
#include <set>
#include <string>

namespace
{
class ParameterSmokeProcessor final : public juce::AudioProcessor
{
public:
    ParameterSmokeProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "ParameterSmokeState", chipper::parameters::createLayout())
    {
    }

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "ParameterSmokeProcessor"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState state;
};

bool expect(bool condition, const std::string& message)
{
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}
}

int main()
{
    ParameterSmokeProcessor processor;
    const auto chipModes = chipper::parameters::chipModeChoices();
    const auto& mappings = chipper::parameters::midiCcMappings();
    std::set<std::string> hostParameterIds;

    auto ok = true;
    ok &= expect(chipModes.size() == 22, "Chip Mode dropdown should only expose the 22 named chip targets");
    for (const auto& chipMode : chipModes)
    {
        ok &= expect(chipMode != "Arcade Hybrid", "Generic Arcade Hybrid chip mode should not be exposed");
        ok &= expect(chipMode != "Custom", "Generic Custom chip mode should not be exposed");
    }
    ok &= expect(processor.state.getParameter(chipper::parameters::id::nesDmcSampleSlot)->getName(64).toStdString()
                     == "Sample Slot",
                 "Shared sample slot host parameter should not be NES-only named");
    ok &= expect(processor.state.getParameter(chipper::parameters::id::nesDmcPlaybackMode)->getName(64).toStdString()
                     == "Sample Playback Mode",
                 "Shared sample playback host parameter should not be NES-only named");
    ok &= expect(processor.state.getParameter(chipper::parameters::id::nesDmcMapRoot)->getName(64).toStdString()
                     == "Sample Bank Root",
                 "Shared sample map root host parameter should not be NES-only named");
    ok &= expect(chipper::parameters::samplePlaybackModeChoices(chipper::ChipMode::nes).joinIntoString("|").toStdString()
                     == "Manual Slot|Note Map|Sample Map Only",
                 "NES sample playback labels should expose DMC Sample Map Only");
    ok &= expect(chipper::parameters::samplePlaybackModeChoices(chipper::ChipMode::spc700).joinIntoString("|").toStdString()
                     == "Manual Slot|Note Map|Drum Map",
                 "SPC700 sample playback labels should expose Note Map and Drum Map");
    ok &= expect(chipper::parameters::samplePlaybackModeChoices(chipper::ChipMode::paula).joinIntoString("|").toStdString()
                     == "Manual Slot|Key Map|Tracker Map",
                 "Paula sample playback labels should expose Key Map and Tracker Map");

    std::set<int> controllers;
    std::set<std::string> parameterIds;

    for (auto* parameter : processor.getParameters())
    {
        const auto* parameterWithId = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
        ok &= expect(parameterWithId != nullptr, "Host parameter is missing a stable parameter id");

        if (parameterWithId != nullptr)
            ok &= expect(hostParameterIds.insert(parameterWithId->paramID.toStdString()).second,
                         "Duplicate host parameter id");
    }

    for (const auto& mapping : mappings)
    {
        ok &= expect(mapping.controller >= 0 && mapping.controller <= 119,
                     "MIDI CC mapping uses a reserved or invalid controller number");
        ok &= expect(controllers.insert(mapping.controller).second,
                     "Duplicate MIDI CC controller mapping");
        ok &= expect(mapping.parameterId != nullptr && mapping.parameterId[0] != '\0',
                     "MIDI CC mapping has an empty parameter id");
        ok &= expect(parameterIds.insert(mapping.parameterId).second,
                     "Duplicate MIDI CC parameter mapping");
        ok &= expect(processor.state.getParameter(mapping.parameterId) != nullptr,
                     std::string("MIDI CC mapping points at missing parameter: ") + mapping.parameterId);
        ok &= expect(chipper::parameters::parameterIdForMidiController(mapping.controller) == mapping.parameterId,
                     "MIDI CC lookup does not return the mapped parameter id");
        ok &= expect(chipper::parameters::midiControllerForParameterId(mapping.parameterId) == mapping.controller,
                     "MIDI CC reverse lookup does not return the mapped controller");
    }

    for (const auto& parameterId : hostParameterIds)
        ok &= expect(parameterIds.count(parameterId) == 1,
                     "Host parameter has no MIDI CC mapping: " + parameterId);

    for (const auto& parameterId : parameterIds)
        ok &= expect(hostParameterIds.count(parameterId) == 1,
                     "MIDI CC mapping targets a non-host parameter: " + parameterId);

    ok &= expect(hostParameterIds.size() == mappings.size(),
                 "Host parameter count does not match MIDI CC mapping count");

    ok &= expect(chipper::parameters::parameterIdForMidiController(23) == nullptr,
                 "Unexpected MIDI CC mapping below the Chipper control block");
    ok &= expect(chipper::parameters::parameterIdForMidiController(120) == nullptr,
                 "Unexpected MIDI CC mapping above the Chipper control block");
    ok &= expect(chipper::parameters::midiControllerForParameterId("missingParameter") < 0,
                 "Unexpected MIDI CC mapping for missing parameter id");

    return ok ? 0 : 1;
}
