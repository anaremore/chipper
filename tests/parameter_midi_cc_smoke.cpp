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
    const auto& mappings = chipper::parameters::midiCcMappings();
    const auto parameterCount = static_cast<size_t>(processor.getParameters().size());

    auto ok = true;
    ok &= expect(parameterCount == mappings.size(),
                 "APVTS parameter count does not match MIDI CC mapping count");

    std::set<int> controllers;
    std::set<std::string> parameterIds;

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

    ok &= expect(chipper::parameters::parameterIdForMidiController(67) == nullptr,
                 "Unexpected MIDI CC mapping below the Chipper control block");
    ok &= expect(chipper::parameters::parameterIdForMidiController(120) == nullptr,
                 "Unexpected MIDI CC mapping above the Chipper control block");
    ok &= expect(chipper::parameters::midiControllerForParameterId("missingParameter") < 0,
                 "Unexpected MIDI CC mapping for missing parameter id");

    return ok ? 0 : 1;
}
