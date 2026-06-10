#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

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

void sendNoteOn(ChipperAudioProcessor& processor, int note, float velocity = 1.0f)
{
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), 0);
    processor.processBlock(buffer, midi);
}

float renderNoteOnPeak(ChipperAudioProcessor& processor, int note, float velocity = 1.0f)
{
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), 0);
    processor.processBlock(buffer, midi);

    auto peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
    }

    return peak;
}

float renderHeldNoteTailPeak(ChipperAudioProcessor& processor, int note, int blockCount = 80, float velocity = 1.0f)
{
    juce::AudioBuffer<float> buffer(2, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), 0);
    processor.processBlock(buffer, midi);

    auto tailPeak = 0.0f;
    for (int block = 0; block < blockCount; ++block)
    {
        buffer.clear();
        juce::MidiBuffer emptyMidi;
        processor.processBlock(buffer, emptyMidi);
        if (block == blockCount - 1)
        {
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    tailPeak = std::max(tailPeak, std::abs(buffer.getSample(channel, sample)));
            }
        }
    }

    return tailPeak;
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

float scopePeak(const ChipperAudioProcessor::OutputScopeSnapshot& snapshot)
{
    auto peak = 0.0f;
    for (const auto sample : snapshot)
        peak = std::max(peak, std::abs(sample));

    return peak;
}

bool writeDmcFixture(const juce::File& file, uint8_t seed)
{
    const std::array<uint8_t, 4> bytes { seed, static_cast<uint8_t>(seed ^ 0x55u), 0xf0u, 0x0fu };
    return file.replaceWithData(bytes.data(), bytes.size());
}

bool writeBrrFixture(const juce::File& file, uint8_t seed)
{
    const std::array<uint8_t, 9> bytes {
        0x81u,
        seed,
        static_cast<uint8_t>(seed ^ 0x11u),
        static_cast<uint8_t>(seed ^ 0x22u),
        static_cast<uint8_t>(seed ^ 0x33u),
        static_cast<uint8_t>(seed ^ 0x44u),
        static_cast<uint8_t>(seed ^ 0x55u),
        static_cast<uint8_t>(seed ^ 0x66u),
        static_cast<uint8_t>(seed ^ 0x77u)
    };
    return file.replaceWithData(bytes.data(), bytes.size());
}

bool writeWavFixture(const juce::File& file, float frequency)
{
    juce::AudioBuffer<float> audio(1, 256);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto phase = static_cast<float>(i) * frequency / 48000.0f;
        audio.setSample(0, i, std::sin(phase * juce::MathConstants<float>::twoPi) * 0.75f);
    }

    if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
    {
        juce::WavAudioFormat format;
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(), 48000.0, 1, 16, {}, 0)))
        {
            stream.release();
            return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
        }
    }

    return false;
}

void appendBigEndian16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
}

void appendBigEndian32(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
}

void appendAscii(std::vector<uint8_t>& bytes, const char* text)
{
    while (*text != '\0')
        bytes.push_back(static_cast<uint8_t>(*text++));
}

bool write8svxFixture(const juce::File& file, uint8_t seed)
{
    std::vector<uint8_t> body;
    body.reserve(256u);
    for (int i = 0; i < 256; ++i)
        body.push_back(static_cast<uint8_t>(static_cast<int8_t>(static_cast<int>(seed) + ((i % 32) - 16))));

    std::vector<uint8_t> data;
    data.reserve(12u + 8u + 20u + 8u + body.size());
    appendAscii(data, "FORM");
    appendBigEndian32(data, static_cast<uint32_t>(4u + 8u + 20u + 8u + body.size()));
    appendAscii(data, "8SVX");
    appendAscii(data, "VHDR");
    appendBigEndian32(data, 20u);
    appendBigEndian32(data, static_cast<uint32_t>(body.size()));
    appendBigEndian32(data, 0u);
    appendBigEndian32(data, 0u);
    appendBigEndian16(data, 8363u);
    data.push_back(1u);
    data.push_back(0u);
    appendBigEndian32(data, 0x00010000u);
    appendAscii(data, "BODY");
    appendBigEndian32(data, static_cast<uint32_t>(body.size()));
    data.insert(data.end(), body.begin(), body.end());
    if ((body.size() & 1u) != 0u)
        data.push_back(0u);

    return file.replaceWithData(data.data(), data.size());
}

void rewritePresetSamplePaths(juce::XmlElement& xml, const juce::String& sampleTagName, const juce::String& fileName)
{
    if (xml.hasTagName(sampleTagName))
    {
        xml.setAttribute("path", "Z:/missing/chipper/" + fileName);
        xml.setAttribute("relativePath", "Samples/" + fileName);
    }

    for (auto* child : xml.getChildIterator())
    {
        if (child != nullptr)
            rewritePresetSamplePaths(*child, sampleTagName, fileName);
    }
}

void rewriteDmcPresetSamplePaths(juce::XmlElement& xml, const juce::String& fileName)
{
    rewritePresetSamplePaths(xml, "DMC_SAMPLE", fileName);
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

    sendController(processor, 63, 127);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 1.0f, 0.0001f,
                     "Unmapped CC63 should not change mapped parameters");

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
                     "CC74 Drum macro should reset DMG Wave Level to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::dmgStereoRoute), 0.0f, 0.0001f,
                     "CC74 NES Drum macro should keep DMG Stereo Routing at Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::stereoSpread), 0.0f, 0.0001f,
                     "CC74 NES Drum macro should reset stereo/spread role to the chip macro template");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 2));
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::waveShape), 3.0f, 0.0001f,
                     "CC74 SID Bass macro should apply SID pulse waveform template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2WaveShape), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 2 waveform to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3WaveShape), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 3 waveform to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2PulseWidth), 0.48f, 0.001f,
                     "CC74 SID Bass macro should apply SID Voice 2 pulse-width template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3PulseWidth), 0.48f, 0.001f,
                     "CC74 SID Bass macro should apply SID Voice 3 pulse-width template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidAttack), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Attack to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidDecay), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Decay to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidSustain), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Sustain to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidRelease), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Release to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2Attack), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 2 Attack to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2Decay), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 2 Decay to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2Sustain), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 2 Sustain to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2Release), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 2 Release to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3Attack), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 3 Attack to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3Decay), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 3 Decay to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3Sustain), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 3 Sustain to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3Release), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Voice 3 Release to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidFilterRouting), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should reset SID Filter Routing to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymEnvelopeShape), 1.0f, 0.0001f,
                     "CC74 SID Bass macro should apply SID LP filter-mode template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::snNoiseMode), 1.0f, 0.0001f,
                     "CC74 SID Bass macro should apply SID oscillator-mod template");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::dmgStereoRoute), 0.0f, 0.0001f,
                     "CC74 SID Bass macro should keep SID Model on Auto");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::stereoSpread), 0.58f, 0.001f,
                     "CC74 SID Bass macro should apply SID resonance template");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 7));
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 5));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 2.0f, 0.0001f,
                     "CC74 SPC700 Drum macro should show sample playback as Drum Map");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 3));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 1.0f, 0.0001f,
                     "CC74 SPC700 Lead macro should show sample playback as Note Map");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 2));
    sendController(processor, 90, controllerValueForChoice(processor, chipper::parameters::id::ymEnvelopeShape, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymEnvelopeShape), 2.0f, 0.0001f,
                     "CC90 should control the SID Filter Mode/YM Envelope Shape choice parameter");
    sendController(processor, 90, controllerValueForChoice(processor, chipper::parameters::id::ymEnvelopeShape, 5));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymEnvelopeShape), 5.0f, 0.0001f,
                     "CC90 should reach SID combined filter-mode choices");
    sendController(processor, 91, controllerValueForChoice(processor, chipper::parameters::id::snNoiseMode, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::snNoiseMode), 4.0f, 0.0001f,
                     "CC91 should control the SID Osc Interaction/Noise Mode choice parameter");
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
    sendController(processor, 105, controllerValueForChoice(processor, chipper::parameters::id::sidVoice2Attack, 7));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2Attack), 7.0f, 0.0001f,
                     "CC105 should control SID Voice 2 Attack nibble choice");
    sendController(processor, 108, controllerValueForChoice(processor, chipper::parameters::id::sidVoice2Release, 9));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2Release), 9.0f, 0.0001f,
                     "CC108 should control SID Voice 2 Release nibble choice");
    sendController(processor, 109, controllerValueForChoice(processor, chipper::parameters::id::sidVoice3Attack, 5));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3Attack), 5.0f, 0.0001f,
                     "CC109 should control SID Voice 3 Attack nibble choice");
    sendController(processor, 112, controllerValueForChoice(processor, chipper::parameters::id::sidVoice3Release, 11));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3Release), 11.0f, 0.0001f,
                     "CC112 should control SID Voice 3 Release nibble choice");
    sendController(processor, 113, controllerValueForChoice(processor, chipper::parameters::id::sidFilterRouting, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidFilterRouting), 4.0f, 0.0001f,
                     "CC113 should control SID Filter Routing choice");
    sendController(processor, 114, 32);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice2PulseWidth), 32.0f / 127.0f, 0.0001f,
                     "CC114 should control SID Voice 2 pulse width");
    sendController(processor, 115, 96);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::sidVoice3PulseWidth), 96.0f / 127.0f, 0.0001f,
                     "CC115 should control SID Voice 3 pulse width");
    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 0));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::chipMode), 0.0f, 0.0001f,
                     "DMC bank behavior should be exercised in NES mode");
    setPlainFromHost(processor, chipper::parameters::id::clockHz, 0.0f);
    processEmptyBlock(processor);

    sendController(processor, 116, 100);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcDirectLevel), 0.787f, 0.0001f,
                     "CC116 should control NES DMC Direct Level");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 12));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcSampleSlot), 12.0f, 0.0001f,
                     "CC117 should control NES DMC Sample Slot");
    sendController(processor, 118, controllerValueForChoice(processor, chipper::parameters::id::nesDmcRateIndex, 3));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcRateIndex), 3.0f, 0.0001f,
                     "CC118 should control NES DMC Rate Index");
    sendController(processor, 119, controllerValueForChoice(processor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 1.0f, 0.0001f,
                     "CC119 should control NES DMC Playback Mode");
    sendController(processor, 119, controllerValueForChoice(processor, chipper::parameters::id::nesDmcPlaybackMode, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 2.0f, 0.0001f,
                     "CC119 should reach NES DMC Sample Map Only mode");
    sendController(processor, 119, controllerValueForChoice(processor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    sendController(processor, 69, controllerValueForChoice(processor, chipper::parameters::id::nesDmcMapRoot, 36));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcMapRoot), 36.0f, 0.0001f,
                     "CC69 should control NES DMC Map Root");
    sendController(processor, 68, 127);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcLoop), 1.0f, 0.0001f,
                     "CC68 should control NES DMC Loop");

    auto dmcDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-dmc-bank-curation-test");
    dmcDir.deleteRecursively();
    ok &= expect(dmcDir.createDirectory().wasOk(), "Should create temporary DMC bank test directory");
    for (int i = 0; i < 34; ++i)
    {
        const auto name = "sample-" + juce::String(i).paddedLeft('0', 2) + ".dmc";
        ok &= expect(writeDmcFixture(dmcDir.getChildFile(name), static_cast<uint8_t>(i + 1)),
                     "Should write temporary DMC fixture " + name.toStdString());
    }

    ok &= expect(processor.loadNesDmcSampleDirectory(dmcDir).wasOk(), "Should load DMC sample directory");
    auto entryInfo = processor.nesDmcSampleEntryInfo();
    auto activeNames = processor.nesDmcSampleNames();
    ok &= expect(entryInfo.size() == 34u, "DMC folder should stage more than 32 library entries");
    ok &= expect(activeNames.size() == 32, "DMC active bank should default to 32 checked slots");
    ok &= expect(entryInfo.size() > 32u && ! entryInfo[32].included && ! entryInfo[32].activeSlot,
                 "DMC entries after the first 32 should default unchecked and inactive");
    auto playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.activeSlot == 0, "DMC playback info should report the selected active slot");
    ok &= expect(playbackInfo.activeSlotCount == 32, "DMC playback info should report active bank count");
    ok &= expect(playbackInfo.byteCount == 4, "DMC playback info should report selected byte count");
    ok &= expect(playbackInfo.bitCount == 32, "DMC playback info should report selected bit count");
    ok &= expect(playbackInfo.rateIndex == 3, "DMC playback info should follow the CC118-selected rate index");
    ok &= expect(playbackInfo.loopEnabled, "DMC playback info should follow the CC68 loop toggle");
    ok &= expect(playbackInfo.bitRateHz > 5500.0 && playbackInfo.bitRateHz < 5600.0,
                 "DMC playback info should estimate bit clock from the selected rate index");
    ok &= expect(playbackInfo.statusLine.contains("bytes") && playbackInfo.statusLine.contains("ms @ rate 3") && playbackInfo.statusLine.contains("Loop"),
                 "DMC status should include bytes, duration, rate, and loop state");

    processor.clearNesDmcSampleSelection();
    activeNames = processor.nesDmcSampleNames();
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(activeNames.isEmpty(), "DMC clear action should uncheck every active sample");
    ok &= expect(playbackInfo.statusLine == "No DMC samples checked", "DMC status should report an empty checked bank");

    processor.invertNesDmcSampleSelection();
    activeNames = processor.nesDmcSampleNames();
    entryInfo = processor.nesDmcSampleEntryInfo();
    ok &= expect(activeNames.size() == 32, "DMC invert action should activate the first 32 checked slots after clearing");
    ok &= expect(entryInfo[32].included && ! entryInfo[32].activeSlot,
                 "DMC invert action should keep checked entries beyond 32 staged but inactive");

    processor.selectFirstNesDmcSamples(8);
    activeNames = processor.nesDmcSampleNames();
    entryInfo = processor.nesDmcSampleEntryInfo();
    ok &= expect(activeNames.size() == 8 && activeNames[7] == "sample-07.dmc",
                 "DMC first-N action should check the requested leading subset");
    ok &= expect(! entryInfo[8].included, "DMC first-N action should uncheck entries after the requested count");

    processor.selectFirstNesDmcSamples(32);
    activeNames = processor.nesDmcSampleNames();
    ok &= expect(activeNames.size() == 32 && activeNames[31] == "sample-31.dmc",
                 "DMC first-32 action should restore the default active bank");

    sendController(processor, 118, controllerValueForChoice(processor, chipper::parameters::id::nesDmcRateIndex, 15));
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.rateIndex == 15, "DMC playback info should update after rate CC changes");
    ok &= expect(playbackInfo.durationMs < 1.1 && playbackInfo.durationMs > 0.9,
                 "DMC playback info should estimate short fixture duration at the fastest rate");

    juce::AudioBuffer<float> dmcMapBuffer(2, 64);
    juce::MidiBuffer dmcMapMidi;
    dmcMapMidi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(dmcMapBuffer, dmcMapMidi);
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.activeSlot == 0 && playbackInfo.sampleName == "sample-00.dmc",
                 "DMC Note Map should map C1 to active slot 1");

    dmcMapMidi.clear();
    dmcMapMidi.addEvent(juce::MidiMessage::noteOn(1, 40, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(dmcMapBuffer, dmcMapMidi);
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.activeSlot == 4 && playbackInfo.sampleName == "sample-04.dmc",
                 "DMC Note Map should map E1 to active slot 5");

    dmcMapMidi.clear();
    dmcMapMidi.addEvent(juce::MidiMessage::noteOn(1, 96, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(dmcMapBuffer, dmcMapMidi);
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.activeSlot == -1 && playbackInfo.statusLine.contains("No mapped DMC sample"),
                 "DMC Note Map should leave notes above the mapped bank silent");
    ok &= expect(playbackInfo.statusLine.contains("Map C1-G3"),
                 "DMC Note Map status should show the selected key span");

    dmcMapMidi.clear();
    dmcMapMidi.addEvent(juce::MidiMessage::noteOn(1, 35, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(dmcMapBuffer, dmcMapMidi);
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.activeSlot == -1 && playbackInfo.statusLine.contains("No mapped DMC sample"),
                 "DMC Note Map should leave notes below the map root silent");

    sendController(processor, 69, controllerValueForChoice(processor, chipper::parameters::id::nesDmcMapRoot, 40));
    dmcMapMidi.clear();
    dmcMapMidi.addEvent(juce::MidiMessage::noteOn(1, 40, static_cast<juce::uint8>(100)), 0);
    processor.processBlock(dmcMapBuffer, dmcMapMidi);
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.activeSlot == 0 && playbackInfo.sampleName == "sample-00.dmc",
                 "DMC Note Map root should remap the selected root note to active slot 1");
    ok &= expect(playbackInfo.statusLine.contains("Map E1-B3"),
                 "DMC Note Map status should follow the remapped key span");

    processor.setNesDmcSampleIncluded(0, false);
    activeNames = processor.nesDmcSampleNames();
    ok &= expect(activeNames.size() == 31, "Unchecking a DMC slot should remove it from the active bank");
    ok &= expect(activeNames[0] == "sample-01.dmc", "DMC active bank should close gaps after an entry is unchecked");

    processor.setNesDmcSampleIncluded(32, true);
    activeNames = processor.nesDmcSampleNames();
    entryInfo = processor.nesDmcSampleEntryInfo();
    ok &= expect(activeNames.size() == 32, "Checking a staged DMC entry should refill the active bank");
    ok &= expect(activeNames[31] == "sample-32.dmc", "Checked staged DMC entry should become the final active slot");
    ok &= expect(entryInfo[32].included && entryInfo[32].activeSlot, "Checked staged DMC entry should report active");

    juce::MemoryBlock savedState;
    processor.getStateInformation(savedState);
    ChipperAudioProcessor restoredProcessor;
    restoredProcessor.prepareToPlay(48000.0, 64);
    restoredProcessor.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    auto restoredEntries = restoredProcessor.nesDmcSampleEntryInfo();
    auto restoredActiveNames = restoredProcessor.nesDmcSampleNames();
    ok &= expect(restoredEntries.size() == 34u, "DMC state restore should reload staged sample paths");
    ok &= expect(restoredActiveNames.size() == 32, "DMC state restore should preserve active slot count");
    ok &= expect(restoredEntries[0].included == false, "DMC state restore should preserve unchecked entries");
    ok &= expect(restoredEntries[32].included && restoredEntries[32].activeSlot, "DMC state restore should preserve checked staged entries");
    ok &= expect(restoredActiveNames[31] == "sample-32.dmc", "DMC state restore should preserve active bank ordering");

    auto portablePresetDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-portable-preset-test");
    portablePresetDir.deleteRecursively();
    ok &= expect(portablePresetDir.getChildFile("Samples").createDirectory().wasOk(), "Should create portable preset sample folder");
    ok &= expect(writeDmcFixture(portablePresetDir.getChildFile("Samples").getChildFile("portable.dmc"), 0x7f),
                 "Should write portable DMC fixture beside preset");
    ChipperAudioProcessor portablePresetProcessor;
    portablePresetProcessor.prepareToPlay(48000.0, 64);
    ok &= expect(portablePresetProcessor.loadNesDmcSampleFile(dmcDir.getChildFile("sample-00.dmc")).wasOk(),
                 "Should load DMC source before creating portable preset XML");
    auto portablePresetXml = portablePresetProcessor.createStateXml();
    ok &= expect(portablePresetXml != nullptr, "Should create portable preset XML");
    if (portablePresetXml != nullptr)
    {
        rewriteDmcPresetSamplePaths(*portablePresetXml, "portable.dmc");
        ChipperAudioProcessor restoredPortablePresetProcessor;
        restoredPortablePresetProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(restoredPortablePresetProcessor.restoreStateXml(*portablePresetXml, portablePresetDir).wasOk(),
                     "Portable preset restore should accept relative sample references");
        const auto portableNames = restoredPortablePresetProcessor.nesDmcSampleNames();
        ok &= expect(portableNames.size() == 1 && portableNames[0] == "portable.dmc",
                     "Portable preset restore should load samples from preset-relative Samples folder");
    }
    portablePresetDir.deleteRecursively();
    dmcDir.deleteRecursively();

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 7));
    sendController(processor, 119, controllerValueForChoice(processor, chipper::parameters::id::nesDmcPlaybackMode, 0));
    auto brrDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-spc700-brr-bank-test");
    brrDir.deleteRecursively();
    ok &= expect(brrDir.createDirectory().wasOk(), "Should create temporary SPC700 BRR bank test directory");
    for (int i = 0; i < 3; ++i)
    {
        const auto name = "brr-" + juce::String(i).paddedLeft('0', 2) + ".brr";
        ok &= expect(writeBrrFixture(brrDir.getChildFile(name), static_cast<uint8_t>(0x20 + i)),
                     "Should write temporary BRR fixture " + name.toStdString());
    }

    ok &= expect(processor.loadSpc700BrrSampleDirectory(brrDir).wasOk(), "Should load SPC700 BRR sample directory");
    auto brrNames = processor.spc700BrrSampleNames();
    ok &= expect(brrNames.size() == 3, "SPC700 BRR folder should expose every readable BRR sample up to the slot limit");
    ok &= expect(brrNames[0] == "brr-00.brr" && brrNames[2] == "brr-02.brr",
                 "SPC700 BRR names should preserve sorted bank order");
    auto brrEntries = processor.spc700BrrSampleEntryInfo();
    ok &= expect(brrEntries.size() == 3u && brrEntries[0].activeSlot && brrEntries[2].activeSlot,
                 "SPC700 BRR bank editor should expose checked files as active playable slots");
    processor.setSpc700BrrSampleIncluded(1, false);
    brrNames = processor.spc700BrrSampleNames();
    brrEntries = processor.spc700BrrSampleEntryInfo();
    ok &= expect(brrNames.size() == 2 && brrNames[0] == "brr-00.brr" && brrNames[1] == "brr-02.brr",
                 "Unchecking an SPC700 BRR slot should remove it from the playable bank");
    ok &= expect(! brrEntries[1].included && ! brrEntries[1].activeSlot,
                 "Unchecked SPC700 BRR entries should report inactive");
    processor.setSpc700BrrSampleIncluded(1, true);
    brrNames = processor.spc700BrrSampleNames();
    ok &= expect(brrNames.size() == 3 && brrNames[1] == "brr-01.brr",
                 "Rechecking an SPC700 BRR slot should restore it to the playable bank");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 2));
    auto brrInfo = processor.spc700BrrSampleInfo();
    ok &= expect(brrInfo.loaded && brrInfo.sampleName == "brr-02.brr",
                 "CC117 should select the active SPC700 BRR bank slot while in SNES mode");
    ok &= expect(brrInfo.blockCount == 1 && brrInfo.statusLine.contains("Slot 3/3"),
                 "SPC700 BRR status should report the selected slot and block count");
    ok &= expect(brrInfo.bankByteCount == 27 && brrInfo.bankBrrBlockCount == 3 && ! brrInfo.nearAramBudget && ! brrInfo.exceedsAramBudget,
                 "SPC700 BRR sample info should report active bank payload against the SNES audio-RAM budget");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 0));
    sendNoteOn(processor, 37);
    brrInfo = processor.spc700BrrSampleInfo();
    ok &= expect(brrInfo.loaded && brrInfo.sampleName == "brr-00.brr",
                 "SPC700 Manual Slot playback should keep using the selected BRR slot instead of note mapping");
    ok &= expect(brrInfo.playbackMode == 0,
                 "SPC700 BRR sample info should report Manual Slot playback mode");
    sendController(processor, 119, controllerValueForChoice(processor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 1.0f, 0.0001f,
                     "CC119 should control SPC700 BRR Playback mode");
    sendNoteOn(processor, 37);
    brrInfo = processor.spc700BrrSampleInfo();
    ok &= expect(brrInfo.loaded && brrInfo.sampleName == "brr-01.brr",
                 "SPC700 note map should select BRR slot 2 for C#1 when root is C1");
    ok &= expect(brrInfo.bankCount == 3 && brrInfo.selectedSlot == 1 && brrInfo.playbackMode == 1 && brrInfo.mapRootNote == 36 && brrInfo.mapHighNote == 38,
                 "SPC700 BRR sample info should expose selected slot, playback mode, and visible note-map span");

    ChipperAudioProcessor spcMapAuditionProcessor;
    spcMapAuditionProcessor.prepareToPlay(48000.0, 256);
    sendController(spcMapAuditionProcessor, 70, controllerValueForChoice(spcMapAuditionProcessor, chipper::parameters::id::chipMode, 7));
    sendController(spcMapAuditionProcessor, 119, controllerValueForChoice(spcMapAuditionProcessor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    ok &= expect(spcMapAuditionProcessor.loadSpc700BrrSampleDirectory(brrDir).wasOk(),
                 "Should load SPC700 BRR sample directory for isolated note-map audio audition");
    const auto outOfRangeMappedPeak = renderNoteOnPeak(spcMapAuditionProcessor, 50);
    const auto outOfRangeMappedInfo = spcMapAuditionProcessor.spc700BrrSampleInfo();
    ok &= expect(outOfRangeMappedPeak <= 0.0001f,
                 "SPC700 note map should leave notes above the loaded BRR bank span silent instead of clamping to the last slot");
    ok &= expect(outOfRangeMappedInfo.selectedSlot == -1 && outOfRangeMappedInfo.statusLine.contains("No mapped SPC700 sample"),
                 "SPC700 note map status should report no mapped sample after an out-of-range note");
    const auto inRangeMappedPeak = renderNoteOnPeak(spcMapAuditionProcessor, 38);
    ok &= expect(inRangeMappedPeak > 0.0001f,
                 "SPC700 note map should produce audio for notes inside the loaded BRR bank span");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 2));

    juce::MemoryBlock savedSpcState;
    processor.getStateInformation(savedSpcState);
    ChipperAudioProcessor restoredSpcProcessor;
    restoredSpcProcessor.prepareToPlay(48000.0, 64);
    restoredSpcProcessor.setStateInformation(savedSpcState.getData(), static_cast<int>(savedSpcState.getSize()));
    processEmptyBlock(restoredSpcProcessor);
    auto restoredBrrNames = restoredSpcProcessor.spc700BrrSampleNames();
    auto restoredBrrInfo = restoredSpcProcessor.spc700BrrSampleInfo();
    ok &= expect(restoredBrrNames.size() == 3, "SPC700 BRR state restore should reload staged sample paths");
    ok &= expect(restoredBrrInfo.loaded && restoredBrrInfo.sampleName == "brr-02.brr",
                 "SPC700 BRR state restore should preserve the selected slot after processing resumes");

    auto portableSpcPresetDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-portable-spc700-preset-test");
    portableSpcPresetDir.deleteRecursively();
    ok &= expect(portableSpcPresetDir.getChildFile("Samples").createDirectory().wasOk(), "Should create portable SPC700 preset sample folder");
    ok &= expect(writeBrrFixture(portableSpcPresetDir.getChildFile("Samples").getChildFile("portable.brr"), 0x44u),
                 "Should write portable SPC700 BRR fixture beside preset");
    ChipperAudioProcessor portableSpcPresetProcessor;
    portableSpcPresetProcessor.prepareToPlay(48000.0, 64);
    sendController(portableSpcPresetProcessor, 70, controllerValueForChoice(portableSpcPresetProcessor, chipper::parameters::id::chipMode, 7));
    ok &= expect(portableSpcPresetProcessor.loadSpc700BrrSampleFile(brrDir.getChildFile("brr-00.brr")).wasOk(),
                 "Should load SPC700 BRR source before creating portable preset XML");
    auto portableSpcPresetXml = portableSpcPresetProcessor.createStateXml();
    ok &= expect(portableSpcPresetXml != nullptr, "Should create portable SPC700 preset XML");
    if (portableSpcPresetXml != nullptr)
    {
        rewritePresetSamplePaths(*portableSpcPresetXml, "BRR_SAMPLE", "portable.brr");
        ChipperAudioProcessor restoredPortableSpcPresetProcessor;
        restoredPortableSpcPresetProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(restoredPortableSpcPresetProcessor.restoreStateXml(*portableSpcPresetXml, portableSpcPresetDir).wasOk(),
                     "Portable preset restore should accept relative SPC700 sample references");
        const auto portableSpcNames = restoredPortableSpcPresetProcessor.spc700BrrSampleNames();
        ok &= expect(portableSpcNames.size() == 1 && portableSpcNames[0] == "portable.brr",
                     "Portable preset restore should load SPC700 samples from preset-relative Samples folder");
    }
    portableSpcPresetDir.deleteRecursively();
    brrDir.deleteRecursively();

    auto spcWavDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-spc700-wav-bank-test");
    spcWavDir.deleteRecursively();
    ok &= expect(spcWavDir.createDirectory().wasOk(), "Should create temporary SPC700 WAV bank test directory");
    for (int i = 0; i < 3; ++i)
    {
        const auto name = "spc-wav-" + juce::String(i).paddedLeft('0', 2) + ".wav";
        ok &= expect(writeWavFixture(spcWavDir.getChildFile(name), 330.0f + static_cast<float>(i) * 110.0f),
                     "Should write temporary SPC700 WAV fixture " + name.toStdString());
    }

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 7));
    sendController(processor, 119, controllerValueForChoice(processor, chipper::parameters::id::nesDmcPlaybackMode, 0));
    ok &= expect(processor.loadSpc700BrrSampleDirectory(spcWavDir).wasOk(), "Should load SPC700 WAV sample directory");
    auto spcWavNames = processor.spc700BrrSampleNames();
    ok &= expect(spcWavNames.size() == 3 && spcWavNames[0] == "spc-wav-00.wav" && spcWavNames[2] == "spc-wav-02.wav",
                 "SPC700 folder import should expose sorted WAV samples as playable slots");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 2));
    auto spcWavInfo = processor.spc700BrrSampleInfo();
    ok &= expect(spcWavInfo.loaded && spcWavInfo.sampleName == "spc-wav-02.wav" && spcWavInfo.byteCount == 256,
                 "CC117 should select the active SPC700 WAV-imported sample slot");
    ok &= expect(spcWavInfo.blockCount == 0 && spcWavInfo.statusLine.contains("imported 8-bit samples"),
                 "SPC700 WAV-imported sample status should report 8-bit sample memory instead of BRR blocks");
    ok &= expect(spcWavInfo.bankByteCount == 768 && spcWavInfo.bankBrrBlockCount == 0,
                 "SPC700 WAV-imported sample bank should report its active imported sample-memory payload");

    ChipperAudioProcessor spcWavMapAuditionProcessor;
    spcWavMapAuditionProcessor.prepareToPlay(48000.0, 256);
    sendController(spcWavMapAuditionProcessor, 70, controllerValueForChoice(spcWavMapAuditionProcessor, chipper::parameters::id::chipMode, 7));
    sendController(spcWavMapAuditionProcessor, 119, controllerValueForChoice(spcWavMapAuditionProcessor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    ok &= expect(spcWavMapAuditionProcessor.loadSpc700BrrSampleDirectory(spcWavDir).wasOk(),
                 "Should load SPC700 WAV sample directory for isolated note-map audio audition");
    const auto spcWavOutOfRangePeak = renderNoteOnPeak(spcWavMapAuditionProcessor, 50);
    const auto spcWavOutOfRangeInfo = spcWavMapAuditionProcessor.spc700BrrSampleInfo();
    ok &= expect(spcWavOutOfRangePeak <= 0.0001f,
                 "SPC700 WAV note map should leave notes above the loaded sample bank span silent instead of clamping to the last slot");
    ok &= expect(spcWavOutOfRangeInfo.selectedSlot == -1 && spcWavOutOfRangeInfo.statusLine.contains("No mapped SPC700 sample"),
                 "SPC700 WAV note map status should report no mapped sample after an out-of-range note");
    const auto spcWavInRangePeak = renderNoteOnPeak(spcWavMapAuditionProcessor, 38);
    ok &= expect(spcWavInRangePeak > 0.0001f,
                 "SPC700 WAV note map should produce audio for notes inside the loaded sample bank span");

    ChipperAudioProcessor spcWavKeyMapLoopProcessor;
    spcWavKeyMapLoopProcessor.prepareToPlay(48000.0, 256);
    sendController(spcWavKeyMapLoopProcessor, 70, controllerValueForChoice(spcWavKeyMapLoopProcessor, chipper::parameters::id::chipMode, 7));
    sendController(spcWavKeyMapLoopProcessor, 74, controllerValueForChoice(spcWavKeyMapLoopProcessor, chipper::parameters::id::macro, 3));
    sendController(spcWavKeyMapLoopProcessor, 119, controllerValueForChoice(spcWavKeyMapLoopProcessor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    setPlainFromHost(spcWavKeyMapLoopProcessor, chipper::parameters::id::dmgStereoRoute, 0.0f);
    ok &= expect(spcWavKeyMapLoopProcessor.loadSpc700BrrSampleDirectory(spcWavDir).wasOk(),
                 "Should load SPC700 WAV sample directory for Note Map loop audition");
    const auto spcWavKeyMapTailPeak = renderHeldNoteTailPeak(spcWavKeyMapLoopProcessor, 38);
    ok &= expect(spcWavKeyMapTailPeak > 0.0001f,
                 "SPC700 Note Map should keep melodic Follow Template playback looping while a note is held");

    ChipperAudioProcessor spcWavDrumMapOneShotProcessor;
    spcWavDrumMapOneShotProcessor.prepareToPlay(48000.0, 256);
    sendController(spcWavDrumMapOneShotProcessor, 70, controllerValueForChoice(spcWavDrumMapOneShotProcessor, chipper::parameters::id::chipMode, 7));
    sendController(spcWavDrumMapOneShotProcessor, 74, controllerValueForChoice(spcWavDrumMapOneShotProcessor, chipper::parameters::id::macro, 3));
    sendController(spcWavDrumMapOneShotProcessor, 119, controllerValueForChoice(spcWavDrumMapOneShotProcessor, chipper::parameters::id::nesDmcPlaybackMode, 2));
    setPlainFromHost(spcWavDrumMapOneShotProcessor, chipper::parameters::id::dmgStereoRoute, 0.0f);
    ok &= expect(spcWavDrumMapOneShotProcessor.loadSpc700BrrSampleDirectory(spcWavDir).wasOk(),
                 "Should load SPC700 WAV sample directory for Drum Map one-shot audition");
    const auto spcWavDrumMapTailPeak = renderHeldNoteTailPeak(spcWavDrumMapOneShotProcessor, 38);
    ok &= expect(spcWavDrumMapTailPeak <= 0.0001f,
                 "SPC700 Drum Map should resolve Follow Template playback to one-shot behavior while still using the mapped sample bank");

    juce::MemoryBlock savedSpcWavState;
    processor.getStateInformation(savedSpcWavState);
    ChipperAudioProcessor restoredSpcWavProcessor;
    restoredSpcWavProcessor.prepareToPlay(48000.0, 64);
    restoredSpcWavProcessor.setStateInformation(savedSpcWavState.getData(), static_cast<int>(savedSpcWavState.getSize()));
    processEmptyBlock(restoredSpcWavProcessor);
    auto restoredSpcWavNames = restoredSpcWavProcessor.spc700BrrSampleNames();
    auto restoredSpcWavInfo = restoredSpcWavProcessor.spc700BrrSampleInfo();
    ok &= expect(restoredSpcWavNames.size() == 3, "SPC700 WAV state restore should reload staged sample paths");
    ok &= expect(restoredSpcWavInfo.loaded && restoredSpcWavInfo.sampleName == "spc-wav-02.wav",
                 "SPC700 WAV state restore should preserve the selected slot after processing resumes");
    spcWavDir.deleteRecursively();

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 9));
    auto paulaDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-paula-sample-bank-test");
    paulaDir.deleteRecursively();
    ok &= expect(paulaDir.createDirectory().wasOk(), "Should create temporary Paula sample bank test directory");
    for (int i = 0; i < 4; ++i)
    {
        const auto name = "paula-" + juce::String(i).paddedLeft('0', 2) + ".wav";
        ok &= expect(writeWavFixture(paulaDir.getChildFile(name), 220.0f + static_cast<float>(i) * 55.0f),
                     "Should write temporary Paula WAV fixture " + name.toStdString());
    }
    ok &= expect(write8svxFixture(paulaDir.getChildFile("zz-paula-native.8svx"), 0x20u),
                 "Should write temporary Paula 8SVX fixture");

    ok &= expect(processor.loadPaulaSampleDirectory(paulaDir).wasOk(), "Should load Paula sample directory");
    auto paulaNames = processor.paulaSampleNames();
    ok &= expect(paulaNames.size() == 5 && paulaNames[0] == "paula-00.wav" && paulaNames[3] == "paula-03.wav" && paulaNames[4] == "zz-paula-native.8svx",
                 "Paula folder should expose sorted WAV and 8SVX samples as playable slots");
    auto paulaEntries = processor.paulaSampleEntryInfo();
    ok &= expect(paulaEntries.size() == 5u && paulaEntries[0].activeSlot && paulaEntries[4].activeSlot,
                 "Paula bank editor should expose checked WAV and 8SVX files as active playable slots");
    processor.setPaulaSampleIncluded(2, false);
    paulaNames = processor.paulaSampleNames();
    paulaEntries = processor.paulaSampleEntryInfo();
    ok &= expect(paulaNames.size() == 4 && paulaNames[2] == "paula-03.wav" && paulaNames[3] == "zz-paula-native.8svx",
                 "Unchecking a Paula sample should remove it from the playable bank and close gaps");
    ok &= expect(! paulaEntries[2].included && ! paulaEntries[2].activeSlot,
                 "Unchecked Paula entries should report inactive");
    processor.setPaulaSampleIncluded(2, true);
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 3));
    auto paulaInfo = processor.paulaSampleInfo();
    ok &= expect(paulaInfo.loaded && paulaInfo.sampleName == "paula-03.wav" && paulaInfo.byteCount == 256,
                 "CC117 should select the active Paula sample bank slot");
    ok &= expect(paulaInfo.statusLine.contains("8-bit samples"),
                 "Paula status should report imported 8-bit sample count");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 4));
    paulaInfo = processor.paulaSampleInfo();
    ok &= expect(paulaInfo.loaded && paulaInfo.sampleName == "zz-paula-native.8svx" && paulaInfo.byteCount == 256,
                 "CC117 should select an imported Paula 8SVX sample bank slot");

    ChipperAudioProcessor paulaMapAuditionProcessor;
    paulaMapAuditionProcessor.prepareToPlay(48000.0, 256);
    sendController(paulaMapAuditionProcessor, 70, controllerValueForChoice(paulaMapAuditionProcessor, chipper::parameters::id::chipMode, 9));
    sendController(paulaMapAuditionProcessor, 119, controllerValueForChoice(paulaMapAuditionProcessor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    ok &= expect(paulaMapAuditionProcessor.loadPaulaSampleDirectory(paulaDir).wasOk(),
                 "Should load Paula sample directory for isolated note-map audio audition");
    const auto paulaOutOfRangeMappedPeak = renderNoteOnPeak(paulaMapAuditionProcessor, 50);
    const auto paulaOutOfRangeMappedInfo = paulaMapAuditionProcessor.paulaSampleInfo();
    ok &= expect(paulaOutOfRangeMappedPeak <= 0.0001f,
                 "Paula note map should leave notes above the loaded sample span silent instead of clamping to the last slot");
    ok &= expect(paulaOutOfRangeMappedInfo.selectedSlot == -1 && paulaOutOfRangeMappedInfo.statusLine.contains("No mapped Paula sample"),
                 "Paula note map status should report no mapped sample after an out-of-range note");
    const auto paulaInRangeMappedPeak = renderNoteOnPeak(paulaMapAuditionProcessor, 39);
    ok &= expect(paulaInRangeMappedPeak > 0.0001f,
                 "Paula note map should produce audio for notes inside the loaded sample bank span");

    ChipperAudioProcessor paulaKeyMapLoopProcessor;
    paulaKeyMapLoopProcessor.prepareToPlay(48000.0, 256);
    sendController(paulaKeyMapLoopProcessor, 70, controllerValueForChoice(paulaKeyMapLoopProcessor, chipper::parameters::id::chipMode, 9));
    sendController(paulaKeyMapLoopProcessor, 74, controllerValueForChoice(paulaKeyMapLoopProcessor, chipper::parameters::id::macro, 3));
    sendController(paulaKeyMapLoopProcessor, 119, controllerValueForChoice(paulaKeyMapLoopProcessor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    setPlainFromHost(paulaKeyMapLoopProcessor, chipper::parameters::id::dmgStereoRoute, 0.0f);
    ok &= expect(paulaKeyMapLoopProcessor.loadPaulaSampleDirectory(paulaDir).wasOk(),
                 "Should load Paula sample directory for Key Map loop audition");
    const auto paulaKeyMapTailPeak = renderHeldNoteTailPeak(paulaKeyMapLoopProcessor, 39);
    ok &= expect(paulaKeyMapTailPeak > 0.0001f,
                 "Paula Key Map should keep melodic Follow Template playback looping while a note is held");

    ChipperAudioProcessor paulaTrackerMapOneShotProcessor;
    paulaTrackerMapOneShotProcessor.prepareToPlay(48000.0, 256);
    sendController(paulaTrackerMapOneShotProcessor, 70, controllerValueForChoice(paulaTrackerMapOneShotProcessor, chipper::parameters::id::chipMode, 9));
    sendController(paulaTrackerMapOneShotProcessor, 74, controllerValueForChoice(paulaTrackerMapOneShotProcessor, chipper::parameters::id::macro, 3));
    sendController(paulaTrackerMapOneShotProcessor, 119, controllerValueForChoice(paulaTrackerMapOneShotProcessor, chipper::parameters::id::nesDmcPlaybackMode, 2));
    setPlainFromHost(paulaTrackerMapOneShotProcessor, chipper::parameters::id::dmgStereoRoute, 0.0f);
    ok &= expect(paulaTrackerMapOneShotProcessor.loadPaulaSampleDirectory(paulaDir).wasOk(),
                 "Should load Paula sample directory for Tracker Map one-shot audition");
    const auto paulaTrackerMapTailPeak = renderHeldNoteTailPeak(paulaTrackerMapOneShotProcessor, 39);
    ok &= expect(paulaTrackerMapTailPeak <= 0.0001f,
                 "Paula Tracker Map should resolve Follow Template playback to one-shot behavior while still using the mapped sample bank");

    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 3));
    juce::MemoryBlock savedPaulaState;
    processor.getStateInformation(savedPaulaState);
    ChipperAudioProcessor restoredPaulaProcessor;
    restoredPaulaProcessor.prepareToPlay(48000.0, 64);
    restoredPaulaProcessor.setStateInformation(savedPaulaState.getData(), static_cast<int>(savedPaulaState.getSize()));
    processEmptyBlock(restoredPaulaProcessor);
    auto restoredPaulaNames = restoredPaulaProcessor.paulaSampleNames();
    auto restoredPaulaInfo = restoredPaulaProcessor.paulaSampleInfo();
    ok &= expect(restoredPaulaNames.size() == 5, "Paula state restore should reload staged WAV and 8SVX sample paths");
    ok &= expect(restoredPaulaInfo.loaded && restoredPaulaInfo.sampleName == "paula-03.wav",
                 "Paula state restore should preserve the selected slot after processing resumes");

    auto portablePaulaPresetDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-portable-paula-preset-test");
    portablePaulaPresetDir.deleteRecursively();
    ok &= expect(portablePaulaPresetDir.getChildFile("Samples").createDirectory().wasOk(), "Should create portable Paula preset sample folder");
    ok &= expect(writeWavFixture(portablePaulaPresetDir.getChildFile("Samples").getChildFile("portable-paula.wav"), 330.0f),
                 "Should write portable Paula WAV fixture beside preset");
    ChipperAudioProcessor portablePaulaPresetProcessor;
    portablePaulaPresetProcessor.prepareToPlay(48000.0, 64);
    sendController(portablePaulaPresetProcessor, 70, controllerValueForChoice(portablePaulaPresetProcessor, chipper::parameters::id::chipMode, 9));
    ok &= expect(portablePaulaPresetProcessor.loadPaulaSampleFile(paulaDir.getChildFile("paula-00.wav")).wasOk(),
                 "Should load Paula source before creating portable preset XML");
    auto portablePaulaPresetXml = portablePaulaPresetProcessor.createStateXml();
    ok &= expect(portablePaulaPresetXml != nullptr, "Should create portable Paula preset XML");
    if (portablePaulaPresetXml != nullptr)
    {
        rewritePresetSamplePaths(*portablePaulaPresetXml, "PAULA_SAMPLE", "portable-paula.wav");
        ChipperAudioProcessor restoredPortablePaulaPresetProcessor;
        restoredPortablePaulaPresetProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(restoredPortablePaulaPresetProcessor.restoreStateXml(*portablePaulaPresetXml, portablePaulaPresetDir).wasOk(),
                     "Portable preset restore should accept relative Paula sample references");
        const auto portablePaulaNames = restoredPortablePaulaPresetProcessor.paulaSampleNames();
        ok &= expect(portablePaulaNames.size() == 1 && portablePaulaNames[0] == "portable-paula.wav",
                     "Portable preset restore should load Paula samples from preset-relative Samples folder");
    }
    portablePaulaPresetDir.deleteRecursively();
    paulaDir.deleteRecursively();

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
                     "CC74 YM macro change should reset Channel A mixer to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelBMix), 0.0f, 0.0001f,
                     "CC74 YM macro change should reset Channel B mixer to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::ymChannelCMix), 0.0f, 0.0001f,
                     "CC74 YM macro change should reset Channel C mixer to Follow");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 0));
    sendController(processor, 104, controllerValueForChoice(processor, chipper::parameters::id::pulse2Duty, 4));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::pulse2Duty), 4.0f, 0.0001f,
                     "CC104 should control NES Pulse 2 Duty");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::pulse2Duty), 0.0f, 0.0001f,
                     "CC74 NES macro change should reset Pulse 2 Duty to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcDirectLevel), 0.0f, 0.0001f,
                     "CC74 NES Bass macro should keep DMC Direct silent");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 0.0f, 0.0001f,
                     "CC74 NES macro change should reset DMC playback mode to Manual Slot");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcMapRoot), 36.0f, 0.0001f,
                     "CC74 NES macro change should reset DMC Map Root to C1");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcLoop), 0.0f, 0.0001f,
                     "CC74 NES macro change should reset DMC Loop to one-shot");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 5));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcDirectLevel), 0.32f, 0.001f,
                     "CC74 NES Drum macro should apply DMC Direct template level");

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
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcDirectLevel), 0.0f, 0.0001f,
                     "Host macro-only Coin change should reset DMC Direct template level");

    setPlainFromHost(processor, chipper::parameters::id::macro, 3.0f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl1, 0.12f);
    setPlainFromHost(processor, chipper::parameters::id::macroControl2, 0.34f);
    processEmptyBlock(processor);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl1), 0.12f, 0.001f,
                     "Host macro plus control snapshot should preserve control 1");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::macroControl2), 0.34f, 0.001f,
                     "Host macro plus control snapshot should preserve control 2");

    ChipperAudioProcessor scopeProcessor;
    scopeProcessor.prepareToPlay(48000.0, 128);
    ok &= expectNear(scopePeak(scopeProcessor.outputScopeSnapshot()), 0.0f, 0.0001f,
                     "Output scope should start silent");

    juce::AudioBuffer<float> scopeBuffer(2, 128);
    juce::MidiBuffer scopeMidi;
    scopeMidi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
    scopeProcessor.processBlock(scopeBuffer, scopeMidi);
    ok &= expect(scopePeak(scopeProcessor.outputScopeSnapshot()) > 0.001f,
                 "Output scope should capture rendered post-trim audio");

    return ok ? 0 : 1;
}
