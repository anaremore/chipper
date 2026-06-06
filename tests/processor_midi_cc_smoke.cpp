#include "PluginProcessor.h"

#include <algorithm>
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

float toleranceFor(float expected)
{
    return std::max(0.0001f, std::abs(expected) * 0.000001f);
}

bool expectNear(float actual, float expected, float tolerance, const std::string& message)
{
    return expect(std::abs(actual - expected) <= tolerance,
                  message + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
}

void sendController(ChipperAudioProcessor& processor, int controller, int value)
{
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, controller, value), 0);
    processor.processBlock(buffer, midi);
}

void processEmptyBlock(ChipperAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}

void setPlainFromHost(ChipperAudioProcessor& processor, const char* parameterId, float plainValue)
{
    if (auto* parameter = processor.getValueTreeState().getParameter(parameterId))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

int controllerValueForChoice(ChipperAudioProcessor& processor, const char* parameterId, int choice)
{
    auto* parameter = processor.getValueTreeState().getParameter(parameterId);
    if (parameter == nullptr)
        return 0;

    for (int value = 0; value <= 127; ++value)
    {
        const auto plain = parameter->convertFrom0to1(static_cast<float>(value) / 127.0f);
        if (static_cast<int>(std::round(plain)) == choice)
            return value;
    }

    return 0;
}
}

int main()
{
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);

    auto ok = true;

    for (const auto& mapping : chipper::parameters::midiCcMappings())
    {
        auto* parameter = processor.getValueTreeState().getParameter(mapping.parameterId);
        ok &= expect(parameter != nullptr,
                     std::string("Missing mapped parameter: ") + mapping.parameterId);
        if (parameter == nullptr)
            continue;

        sendController(processor, mapping.controller, 0);
        const auto expectedMinimum = parameter->convertFrom0to1(0.0f);
        ok &= expectNear(parameterValue(processor, mapping.parameterId),
                         expectedMinimum,
                         toleranceFor(expectedMinimum),
                         "CC" + std::to_string(mapping.controller) + " should set " + mapping.label + " to minimum");

        sendController(processor, mapping.controller, 127);
        const auto expectedMaximum = parameter->convertFrom0to1(1.0f);
        ok &= expectNear(parameterValue(processor, mapping.parameterId),
                         expectedMaximum,
                         toleranceFor(expectedMaximum),
                         "CC" + std::to_string(mapping.controller) + " should set " + mapping.label + " to maximum");
    }

    sendController(processor, 69, 127);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 1.0f, 0.0001f,
                     "Unmapped CC69 should not change mapped parameters");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 0));
    sendController(processor, 76, 127);
    sendController(processor, 77, 127);
    sendController(processor, 78, 0);
    sendController(processor, 79, 0);
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 5));

    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 0.40f, 0.001f,
                     "CC74 Drum macro should apply NES control 1 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl2), 0.15f, 0.001f,
                     "CC74 Drum macro should apply NES control 2 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl3), 0.80f, 0.001f,
                     "CC74 Drum macro should apply NES control 3 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl4), 0.45f, 0.001f,
                     "CC74 Drum macro should apply NES control 4 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source1Enabled), 0.0f, 0.0001f,
                     "CC74 Drum macro should mute NES pulse 1 source");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source2Enabled), 0.0f, 0.0001f,
                     "CC74 Drum macro should mute NES pulse 2 source");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source3Enabled), 1.0f, 0.0001f,
                     "CC74 Drum macro should enable NES triangle source");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source4Enabled), 1.0f, 0.0001f,
                     "CC74 Drum macro should enable NES noise source");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::envelopeDecay), 0.78f, 0.001f,
                     "CC74 Drum macro should apply NES envelope decay template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::dmgWaveLevel), 0.0f, 0.0001f,
                     "CC74 Drum macro should reset DMG Wave Level to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::dmgStereoRoute), 0.0f, 0.0001f,
                     "CC74 NES Drum macro should keep DMG Stereo Routing at Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::stereoSpread), 0.0f, 0.0001f,
                     "CC74 NES Drum macro should reset stereo/spread role to the chip macro template");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 2));
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::waveShape), 3.0f, 0.0001f,
                     "CC74 SID Bass macro should apply SID pulse waveform template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2WaveShape), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 2 waveform to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3WaveShape), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 3 waveform to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidAttack), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Attack to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidDecay), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Decay to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidSustain), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Sustain to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidRelease), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Release to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymEnvelopeShape), 1.0f, 0.0001f,
                     "CC74 SID Bass macro should apply SID LP filter-mode template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::snNoiseMode), 1.0f, 0.0001f,
                     "CC74 SID Bass macro should apply SID oscillator-mod template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::dmgStereoRoute), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should keep SID Model on Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::stereoSpread), 0.58f, 0.001f,
                     "CC74 SID Bass macro should apply SID resonance template");

    sendController(processor, 90, controllerValueForChoice(processor, chipper::parameters::id::ymEnvelopeShape, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymEnvelopeShape), 2.0f, 0.0001f,
                     "CC90 should control the SID Filter Mode/YM Envelope Shape choice parameter");
    sendController(processor, 91, controllerValueForChoice(processor, chipper::parameters::id::snNoiseMode, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::snNoiseMode), 4.0f, 0.0001f,
                     "CC91 should control the SID Osc Mod/Noise Mode choice parameter");
    sendController(processor, 94, controllerValueForChoice(processor, chipper::parameters::id::dmgStereoRoute, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::dmgStereoRoute), 2.0f, 0.0001f,
                     "CC94 should control the SID Model/DMG Stereo Route choice parameter");
    sendController(processor, 93, 127);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::stereoSpread), 1.0f, 0.0001f,
                     "CC93 should control the SID Resonance/Stereo Spread parameter");
    sendController(processor, 95, controllerValueForChoice(processor, chipper::parameters::id::sidVoice2WaveShape, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2WaveShape), 2.0f, 0.0001f,
                     "CC95 should control SID Voice 2 waveform");
    sendController(processor, 96, controllerValueForChoice(processor, chipper::parameters::id::sidVoice3WaveShape, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3WaveShape), 4.0f, 0.0001f,
                     "CC96 should control SID Voice 3 waveform");
    sendController(processor, 97, controllerValueForChoice(processor, chipper::parameters::id::sidAttack, 16));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidAttack), 16.0f, 0.0001f,
                     "CC97 should control SID Attack nibble choice");
    sendController(processor, 98, controllerValueForChoice(processor, chipper::parameters::id::sidRelease, 1));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidRelease), 1.0f, 0.0001f,
                     "CC98 should control SID Release nibble choice");
    sendController(processor, 99, controllerValueForChoice(processor, chipper::parameters::id::sidDecay, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidDecay), 4.0f, 0.0001f,
                     "CC99 should control SID Decay nibble choice");
    sendController(processor, 100, controllerValueForChoice(processor, chipper::parameters::id::sidSustain, 13));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidSustain), 13.0f, 0.0001f,
                     "CC100 should control SID Sustain nibble choice");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 3));
    sendController(processor, 101, controllerValueForChoice(processor, chipper::parameters::id::ymChannelAMix, 1));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelAMix), 1.0f, 0.0001f,
                     "CC101 should control YM Channel A mixer choice");
    sendController(processor, 102, controllerValueForChoice(processor, chipper::parameters::id::ymChannelBMix, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelBMix), 2.0f, 0.0001f,
                     "CC102 should control YM Channel B mixer choice");
    sendController(processor, 103, controllerValueForChoice(processor, chipper::parameters::id::ymChannelCMix, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelCMix), 4.0f, 0.0001f,
                     "CC103 should control YM Channel C mixer choice");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelAMix), 0.0f, 0.0001f,
                     "CC74 YM macro change should reset Channel A mixer to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelBMix), 0.0f, 0.0001f,
                     "CC74 YM macro change should reset Channel B mixer to Macro");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelCMix), 0.0f, 0.0001f,
                     "CC74 YM macro change should reset Channel C mixer to Macro");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 0));
    sendController(processor, 104, controllerValueForChoice(processor, chipper::parameters::id::pulse2Duty, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::pulse2Duty), 4.0f, 0.0001f,
                     "CC104 should control NES Pulse 2 Duty");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::pulse2Duty), 0.0f, 0.0001f,
                     "CC74 NES macro change should reset Pulse 2 Duty to Macro");

    setPlainFromHost(processor, chipper::parameters::id::macroControl1, 0.91f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl2, 0.92f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl3, 0.93f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl4, 0.94f);
    processEmptyBlock(processor);

    setPlainFromHost(processor, chipper::parameters::id::macro, 1.0f);
    processEmptyBlock(processor);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 0.15f, 0.001f,
                     "Host macro-only change should apply NES Coin control 1 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl2), 0.85f, 0.001f,
                     "Host macro-only change should apply NES Coin control 2 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl3), 0.10f, 0.001f,
                     "Host macro-only change should apply NES Coin control 3 template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::source3Enabled), 0.0f, 0.0001f,
                     "Host macro-only change should apply NES Coin source template");

    setPlainFromHost(processor, chipper::parameters::id::macro, 3.0f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl1, 0.12f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl2, 0.34f);
    processEmptyBlock(processor);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 0.12f, 0.001f,
                     "Host macro plus control snapshot should preserve control 1");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl2), 0.34f, 0.001f,
                     "Host macro plus control snapshot should preserve control 2");

    return ok ? 0 : 1;
}
