#include "PluginProcessor.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
bool expect(bool condition, const std::string& message)
{
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

float parameterValue(ChipperAudioProcessor& processor, const char* parameterId)
{
    if (const auto* value = processor.getValueTreeState().getRawParameterValue(parameterId))
        return value->load();

    return -9999.0f;
}

bool expectNear(float actual, float expected, float tolerance, const std::string& message)
{
    return expect(std::abs(actual - expected) <= tolerance,
                  message + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
}
}

int main()
{
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);

    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 76, 127), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 79, 0), 8);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 80, 0), 16);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 81, 127), 24);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 32);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 75, 127), 40);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 85, 127), 48);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 87, 127), 56);

    processor.processBlock(buffer, midi);

    auto ok = true;
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 1.0f, 0.0001f,
                     "CC76 should set Native Control 1 to maximum");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl4), 0.0f, 0.0001f,
                     "CC79 should set Native Control 4 to minimum");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source1Enabled), 0.0f, 0.0001f,
                     "CC80 should switch Source 1 off below the midpoint");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source2Enabled), 1.0f, 0.0001f,
                     "CC81 should switch Source 2 on above the midpoint");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macro), 9.0f, 0.0001f,
                     "CC74 should select the last Musical Macro choice at maximum");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::playMode), 1.0f, 0.0001f,
                     "CC75 should select Chip Poly at maximum");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::waveShape), 4.0f, 0.0001f,
                     "CC85 should select the last Wave Shape choice at maximum");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::snNoiseMode), 4.0f, 0.0001f,
                     "CC87 should select the last SN76489 Noise Mode choice at maximum");

    juce::MidiBuffer unmappedMidi;
    unmappedMidi.addEvent(juce::MidiMessage::controllerEvent(1, 69, 127), 0);
    processor.processBlock(buffer, unmappedMidi);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 1.0f, 0.0001f,
                     "Unmapped CC69 should not change mapped parameters");

    return ok ? 0 : 1;
}
