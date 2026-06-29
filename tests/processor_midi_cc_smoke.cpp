#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
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

void sendNoteOff(ChipperAudioProcessor& processor, int note)
{
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
    processor.processBlock(buffer, midi);
}

float renderEmptyBlocksPeak(ChipperAudioProcessor& processor, int blockCount = 80)
{
    juce::AudioBuffer<float> buffer(2, 256);
    auto peak = 0.0f;

    for (int block = 0; block < blockCount; ++block)
    {
        buffer.clear();
        juce::MidiBuffer emptyMidi;
        processor.processBlock(buffer, emptyMidi);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
        }
    }

    return peak;
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

float renderNoteOnHighStateRatio(ChipperAudioProcessor& processor, int note, float velocity = 1.0f)
{
    juce::AudioBuffer<float> buffer(2, 256);
    std::vector<float> samples;
    samples.reserve(2048);

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, note, velocity), 0);

        processor.processBlock(buffer, midi);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            samples.push_back(buffer.getSample(0, sample));
    }

    const auto [minIt, maxIt] = std::minmax_element(samples.begin(), samples.end());
    if (minIt == samples.end() || maxIt == samples.end() || std::abs(*maxIt - *minIt) < 0.00001f)
        return 0.0f;

    const auto threshold = (*minIt + *maxIt) * 0.5f;
    const auto highCount = std::count_if(samples.begin(), samples.end(),
                                         [threshold](float sample) { return sample > threshold; });
    return static_cast<float>(highCount) / static_cast<float>(samples.size());
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

int jsonIntValue(const std::string& json, const std::string& key, int fallback = -1)
{
    const auto marker = "\"" + key + "\":";
    const auto start = json.find(marker);
    if (start == std::string::npos)
        return fallback;

    const auto valueStart = start + marker.size();
    const auto valueEnd = json.find_first_of(",}", valueStart);
    if (valueEnd == std::string::npos)
        return fallback;

    try
    {
        return std::stoi(json.substr(valueStart, valueEnd - valueStart));
    }
    catch (...)
    {
        return fallback;
    }
}

bool expectFourOperatorCarrierRoleDebug(int chipChoice, const char* label)
{
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, chipChoice));
    setPlainFromHost(processor, chipper::parameters::id::waveShape, 5.0f);
    sendNoteOn(processor, 60);

    const auto debug = processor.currentCoreDebugStateJson();
    auto ok = true;
    ok &= expect(jsonIntValue(debug, "algorithm0") == 4,
                 std::string(label) + " explicit algorithm choice should resolve algorithm 4 in core debug JSON");
    ok &= expect(jsonIntValue(debug, "operatorCarrier0") == 0
                     && jsonIntValue(debug, "operatorCarrier1") == 1
                     && jsonIntValue(debug, "operatorCarrier2") == 0
                     && jsonIntValue(debug, "operatorCarrier3") == 1,
                 std::string(label) + " algorithm 4 debug JSON should expose M/C/M/C operator roles");
    return ok;
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

void appendLittleEndian32(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
    bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

bool writeLoopedWavFixture(const juce::File& file, float frequency, uint32_t loopStart, uint32_t loopEndExclusive)
{
    if (! writeWavFixture(file, frequency))
        return false;

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block) || block.getSize() < 12u)
        return false;

    std::vector<uint8_t> data(static_cast<const uint8_t*>(block.getData()),
                              static_cast<const uint8_t*>(block.getData()) + block.getSize());
    if (std::memcmp(data.data(), "RIFF", 4u) != 0 || std::memcmp(data.data() + 8u, "WAVE", 4u) != 0)
        return false;

    const auto loopEndInclusive = loopEndExclusive > 0u ? loopEndExclusive - 1u : 0u;
    const auto appendText = [](std::vector<uint8_t>& bytes, const char* text)
    {
        while (*text != '\0')
            bytes.push_back(static_cast<uint8_t>(*text++));
    };

    std::vector<uint8_t> smpl;
    smpl.reserve(8u + 36u + 24u);
    appendText(smpl, "smpl");
    appendLittleEndian32(smpl, 36u + 24u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 60u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 1u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 1u);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, loopStart);
    appendLittleEndian32(smpl, loopEndInclusive);
    appendLittleEndian32(smpl, 0u);
    appendLittleEndian32(smpl, 0u);
    data.insert(data.end(), smpl.begin(), smpl.end());

    const auto riffSize = static_cast<uint32_t>(data.size() - 8u);
    data[4] = static_cast<uint8_t>(riffSize & 0xffu);
    data[5] = static_cast<uint8_t>((riffSize >> 8u) & 0xffu);
    data[6] = static_cast<uint8_t>((riffSize >> 16u) & 0xffu);
    data[7] = static_cast<uint8_t>((riffSize >> 24u) & 0xffu);
    return file.replaceWithData(data.data(), data.size());
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

bool writeAiffFixture(const juce::File& file, float frequency)
{
    juce::AudioBuffer<float> audio(1, 256);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto phase = static_cast<float>(i) * frequency / 48000.0f;
        audio.setSample(0, i, std::sin(phase * juce::MathConstants<float>::twoPi) * 0.75f);
    }

    if (auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream()))
    {
        juce::AiffAudioFormat format;
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(), 48000.0, 1, 16, {}, 0)))
        {
            stream.release();
            return writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
        }
    }

    return false;
}

bool writeLoopedAiffFixture(const juce::File& file, float frequency, uint32_t loopStart, uint32_t loopEnd)
{
    if (! writeAiffFixture(file, frequency))
        return false;

    juce::MemoryBlock block;
    if (! file.loadFileAsData(block) || block.getSize() < 12u)
        return false;

    std::vector<uint8_t> data(static_cast<const uint8_t*>(block.getData()),
                              static_cast<const uint8_t*>(block.getData()) + block.getSize());
    if (std::memcmp(data.data(), "FORM", 4u) != 0
        || (std::memcmp(data.data() + 8u, "AIFF", 4u) != 0 && std::memcmp(data.data() + 8u, "AIFC", 4u) != 0))
        return false;

    std::vector<uint8_t> mark;
    mark.reserve(8u + 2u + 16u);
    appendAscii(mark, "MARK");
    appendBigEndian32(mark, 18u);
    appendBigEndian16(mark, 2u);
    appendBigEndian16(mark, 1u);
    appendBigEndian32(mark, loopStart);
    mark.push_back(0u);
    mark.push_back(0u);
    appendBigEndian16(mark, 2u);
    appendBigEndian32(mark, loopEnd);
    mark.push_back(0u);
    mark.push_back(0u);
    data.insert(data.end(), mark.begin(), mark.end());

    std::vector<uint8_t> inst;
    inst.reserve(8u + 20u);
    appendAscii(inst, "INST");
    appendBigEndian32(inst, 20u);
    inst.push_back(60u);
    inst.push_back(0u);
    inst.push_back(0u);
    inst.push_back(127u);
    inst.push_back(1u);
    inst.push_back(127u);
    appendBigEndian16(inst, 0u);
    appendBigEndian16(inst, 1u);
    appendBigEndian16(inst, 1u);
    appendBigEndian16(inst, 2u);
    appendBigEndian16(inst, 0u);
    appendBigEndian16(inst, 0u);
    appendBigEndian16(inst, 0u);
    data.insert(data.end(), inst.begin(), inst.end());

    const auto formSize = static_cast<uint32_t>(data.size() - 8u);
    data[4] = static_cast<uint8_t>((formSize >> 24u) & 0xffu);
    data[5] = static_cast<uint8_t>((formSize >> 16u) & 0xffu);
    data[6] = static_cast<uint8_t>((formSize >> 8u) & 0xffu);
    data[7] = static_cast<uint8_t>(formSize & 0xffu);
    return file.replaceWithData(data.data(), data.size());
}

bool writeProTrackerModFixture(const juce::File& file)
{
    std::vector<uint8_t> data(1084u + 1024u, 0u);
    const auto writeText = [&data](size_t offset, size_t width, const char* text)
    {
        for (size_t i = 0; i < width && text[i] != '\0'; ++i)
            data[offset + i] = static_cast<uint8_t>(text[i]);
    };
    const auto writeU16 = [&data](size_t offset, uint16_t value)
    {
        data[offset] = static_cast<uint8_t>((value >> 8u) & 0xffu);
        data[offset + 1u] = static_cast<uint8_t>(value & 0xffu);
    };
    const auto writeSampleHeader = [&](size_t index,
                                       const char* name,
                                       uint16_t lengthWords,
                                       uint8_t volume,
                                       uint16_t loopStartWords,
                                       uint16_t loopLengthWords)
    {
        const auto offset = 20u + index * 30u;
        writeText(offset, 22u, name);
        writeU16(offset + 22u, lengthWords);
        data[offset + 24u] = 0u;
        data[offset + 25u] = volume;
        writeU16(offset + 26u, loopStartWords);
        writeU16(offset + 28u, loopLengthWords);
    };

    writeText(0u, 20u, "Chipper MOD Test");
    writeSampleHeader(0u, "Loop Bass", 32u, 64u, 8u, 16u);
    writeSampleHeader(1u, "One Shot", 16u, 48u, 0u, 1u);
    data[950u] = 1u;
    writeText(1080u, 4u, "M.K.");

    for (int i = 0; i < 64; ++i)
        data.push_back(static_cast<uint8_t>(static_cast<int8_t>((i % 32) - 16)));
    for (int i = 0; i < 32; ++i)
        data.push_back(static_cast<uint8_t>(static_cast<int8_t>(31 - i * 2)));

    return file.replaceWithData(data.data(), data.size());
}

bool write8svxFixture(const juce::File& file, uint8_t seed, uint32_t oneShotSamples = 256u, uint32_t repeatSamples = 0u)
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
    appendBigEndian32(data, oneShotSamples);
    appendBigEndian32(data, repeatSamples);
    appendBigEndian32(data, repeatSamples);
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

void addMissingSampleReference(juce::XmlElement& xml,
                               const juce::String& bankTagName,
                               const juce::String& sampleTagName,
                               const juce::File& missingFile)
{
    auto* bank = new juce::XmlElement(bankTagName);
    bank->setAttribute("count", 1);

    auto* sample = new juce::XmlElement(sampleTagName);
    sample->setAttribute("path", missingFile.getFullPathName());
    bank->addChildElement(sample);
    xml.addChildElement(bank);
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
    sendController(processor, 40, 127);
    sendController(processor, 41, 0);
    sendController(processor, 42, 96);
    sendController(processor, 43, 32);
    sendController(processor, 44, 127);
    sendController(processor, 45, 0);
    sendController(processor, 32, 127);
    sendController(processor, 33, 0);
    sendController(processor, 36, 127);
    sendController(processor, 37, 0);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1Level), 1.0f, 0.0001f,
                     "CC40 should control FM Operator 1 Level");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2Level), 0.0f, 0.0001f,
                     "CC41 should control FM Operator 2 Level");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1Multiplier), 16.0f, 0.0001f,
                     "CC44 should control FM Operator 1 Multiplier");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2Multiplier), 0.0f, 0.0001f,
                     "CC45 should control FM Operator 2 Multiplier");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1AttackRate), 32.0f, 0.0001f,
                     "CC36 should control FM Operator 1 Attack Rate");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2AttackRate), 0.0f, 0.0001f,
                     "CC37 should control FM Operator 2 Attack Rate");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1DecayRate), 32.0f, 0.0001f,
                     "CC32 should control FM Operator 1 Decay Rate");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2DecayRate), 0.0f, 0.0001f,
                     "CC33 should control FM Operator 2 Decay Rate");
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
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1Level), 0.5f, 0.0001f,
                     "CC74 macro change should reset FM Operator 1 Level to neutral");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2Level), 0.5f, 0.0001f,
                     "CC74 macro change should reset FM Operator 2 Level to neutral");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator3Level), 0.5f, 0.0001f,
                     "CC74 macro change should reset FM Operator 3 Level to neutral");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator4Level), 0.5f, 0.0001f,
                     "CC74 macro change should reset FM Operator 4 Level to neutral");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1Multiplier), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 1 Multiplier to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2Multiplier), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 2 Multiplier to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator3Multiplier), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 3 Multiplier to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator4Multiplier), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 4 Multiplier to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1AttackRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 1 Attack Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2AttackRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 2 Attack Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator3AttackRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 3 Attack Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator4AttackRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 4 Attack Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1DecayRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 1 Decay Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2DecayRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 2 Decay Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator3DecayRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 3 Decay Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator4DecayRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 4 Decay Rate to Follow");

    {
        ChipperAudioProcessor nesPulse2Processor;
        nesPulse2Processor.prepareToPlay(48000.0, 64);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::chipMode, 0.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::macro, 5.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::source1Enabled, 0.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::source2Enabled, 1.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::source3Enabled, 0.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::source4Enabled, 0.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::source2Level, 1.0f);
        setPlainFromHost(nesPulse2Processor, chipper::parameters::id::pulse2Duty, 4.0f);
        const auto nesPulse2OnlyPeak = renderNoteOnPeak(nesPulse2Processor, 60);
        ok &= expect(nesPulse2OnlyPeak > 0.0001f,
                     "NES Pulse 2 source card should produce audio when explicitly enabled by the UI");
    }

    {
        ChipperAudioProcessor dmgPulse2Processor;
        dmgPulse2Processor.prepareToPlay(48000.0, 64);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::chipMode, 1.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::macro, 2.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source1Enabled, 0.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source2Enabled, 1.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source3Enabled, 0.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source4Enabled, 0.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source2Level, 1.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::pulse2Duty, 4.0f);
        const auto pulse2OnlyPeak = renderNoteOnPeak(dmgPulse2Processor, 60);
        ok &= expect(pulse2OnlyPeak > 0.0001f,
                     "DMG Pulse 2 source card should produce audio when explicitly enabled by the UI");
    }

    auto renderDmgPulse2DutyRatio = [](float pulse2DutyChoice, float pulse1Duty = 2.0f / 3.0f) {
        ChipperAudioProcessor dmgPulse2Processor;
        dmgPulse2Processor.prepareToPlay(48000.0, 64);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::chipMode, 1.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::macro, 2.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::macroControl1, pulse1Duty);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source1Enabled, 0.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source2Enabled, 1.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source3Enabled, 0.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source4Enabled, 0.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::source2Level, 1.0f);
        setPlainFromHost(dmgPulse2Processor, chipper::parameters::id::pulse2Duty, pulse2DutyChoice);
        return renderNoteOnHighStateRatio(dmgPulse2Processor, 60);
    };

    const auto dmgPulse2ThinDutyRatio = renderDmgPulse2DutyRatio(1.0f);
    const auto dmgPulse2WideDutyRatio = renderDmgPulse2DutyRatio(4.0f);
    ok &= expect(dmgPulse2WideDutyRatio - dmgPulse2ThinDutyRatio > 0.35f,
                 "DMG Pulse 2 explicit duty choices should independently change the rendered waveform high-state ratio");
    const auto dmgPulse2ExplicitWideWithThinP1 = renderDmgPulse2DutyRatio(4.0f, 0.0f);
    const auto dmgPulse2ExplicitWideWithSquareP1 = renderDmgPulse2DutyRatio(4.0f, 2.0f / 3.0f);
    ok &= expect(std::abs(dmgPulse2ExplicitWideWithThinP1 - dmgPulse2ExplicitWideWithSquareP1) < 0.05f,
                 "DMG Pulse 2 explicit duty should stay independent when Pulse 1 duty changes");

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
                     "CC74 SID Bass macro should keep SID Model on Preset");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::stereoSpread), 0.58f, 0.001f,
                     "CC74 SID Bass macro should apply SID resonance template");

    ChipperAudioProcessor fmMonoPriorityProcessor;
    fmMonoPriorityProcessor.prepareToPlay(48000.0, 256);
    sendController(fmMonoPriorityProcessor, 70, controllerValueForChoice(fmMonoPriorityProcessor, chipper::parameters::id::chipMode, 5));
    sendController(fmMonoPriorityProcessor, 74, controllerValueForChoice(fmMonoPriorityProcessor, chipper::parameters::id::macro, 2));
    setPlainFromHost(fmMonoPriorityProcessor, chipper::parameters::id::playMode, 0.0f);
    sendNoteOn(fmMonoPriorityProcessor, 48);
    sendNoteOn(fmMonoPriorityProcessor, 52);
    sendNoteOff(fmMonoPriorityProcessor, 52);
    const auto fmMonoFallbackPeak = renderEmptyBlocksPeak(fmMonoPriorityProcessor, 100);
    ok &= expect(fmMonoFallbackPeak > 0.002f,
                 "Big Mono should restore the previous held OPN2 note after releasing the newest note");

    const std::array<std::pair<int, const char*>, 4> fmChipChoices {
        std::pair<int, const char*> { 5, "OPN2" },
        std::pair<int, const char*> { 6, "OPL" },
        std::pair<int, const char*> { 12, "OPM" },
        std::pair<int, const char*> { 13, "OPLL" }
    };
    for (const auto [chipChoice, label] : fmChipChoices)
    {
        ChipperAudioProcessor duplicateNoteProcessor;
        duplicateNoteProcessor.prepareToPlay(48000.0, 256);
        sendController(duplicateNoteProcessor, 70, controllerValueForChoice(duplicateNoteProcessor, chipper::parameters::id::chipMode, chipChoice));
        sendController(duplicateNoteProcessor, 74, controllerValueForChoice(duplicateNoteProcessor, chipper::parameters::id::macro, 2));
        setPlainFromHost(duplicateNoteProcessor, chipper::parameters::id::playMode, 0.0f);
        sendNoteOn(duplicateNoteProcessor, 48);
        sendNoteOn(duplicateNoteProcessor, 48);
        sendNoteOff(duplicateNoteProcessor, 48);
        const auto duplicateNotePeak = renderEmptyBlocksPeak(duplicateNoteProcessor, 160);
        ok &= expect(duplicateNotePeak > 0.002f,
                     std::string(label) + " should keep a retriggered duplicate note audible after the first matching note-off");
    }
    ok &= expectFourOperatorCarrierRoleDebug(5, "OPN2");
    ok &= expectFourOperatorCarrierRoleDebug(12, "OPM");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 7));
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 5));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 2.0f, 0.0001f,
                     "CC74 SPC700 Drum macro should show sample playback as Drum Map");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 3));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 1.0f, 0.0001f,
                     "CC74 SPC700 Lead macro should show sample playback as Note Map");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 9));
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 2));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 1.0f, 0.0001f,
                     "CC74 Paula Bass macro should show sample playback as Key Map");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 5));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 2.0f, 0.0001f,
                     "CC74 Paula Drum macro should show sample playback as Tracker Map");
    sendController(processor, 74, controllerValueForChoice(processor, chipper::parameters::id::macro, 0));
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcPlaybackMode), 0.0f, 0.0001f,
                     "CC74 Paula Manual macro should return sample playback to Manual Slot");

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

    sendController(processor, 68, 0);
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::nesDmcLoop), 0.0f, 0.0001f,
                     "CC68 should turn NES DMC Loop back off before one-shot status checks");
    sendController(processor, 118, controllerValueForChoice(processor, chipper::parameters::id::nesDmcRateIndex, 15));
    playbackInfo = processor.nesDmcSamplePlaybackInfo();
    ok &= expect(playbackInfo.rateIndex == 15, "DMC playback info should update after rate CC changes");
    ok &= expect(playbackInfo.durationMs < 1.1 && playbackInfo.durationMs > 0.9,
                 "DMC playback info should estimate short fixture duration at the fastest rate");
    ok &= expect(playbackInfo.statusLine.contains("One-shot, no loop; DAC holds"),
                 "DMC status should describe loop-off playback as one-shot with DAC hold");
    auto dmcPreview = processor.sampleWaveformSnapshot(chipper::ChipMode::nes);
    ok &= expect(dmcPreview.loaded && ! dmcPreview.hasLoop,
                 "NES DMC waveform preview should not display loop markers while Loop is off");

    sendController(processor, 68, 127);
    dmcPreview = processor.sampleWaveformSnapshot(chipper::ChipMode::nes);
    ok &= expect(dmcPreview.loaded && dmcPreview.hasLoop && dmcPreview.loopStart == 0.0f && dmcPreview.loopEnd == 1.0f,
                 "NES DMC waveform preview should display full-sample loop markers only when Loop is on");
    sendController(processor, 68, 0);

    ChipperAudioProcessor dmcOneShotProcessor;
    dmcOneShotProcessor.prepareToPlay(48000.0, 256);
    ok &= expect(dmcOneShotProcessor.loadNesDmcSampleDirectory(dmcDir).wasOk(),
                 "Should load DMC sample directory for one-shot replay regression");
    sendController(dmcOneShotProcessor, 68, 0);
    sendController(dmcOneShotProcessor, 118, controllerValueForChoice(dmcOneShotProcessor, chipper::parameters::id::nesDmcRateIndex, 15));
    sendNoteOn(dmcOneShotProcessor, 48);
    for (int i = 0; i < 8; ++i)
        processEmptyBlock(dmcOneShotProcessor);
    auto dmcDebug = dmcOneShotProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 0,
                 "NES DMC one-shot should stop after the selected sample ends when Loop is off");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleCompleted") == 1,
                 "NES DMC one-shot should report that the sample stream completed when Loop is off");
    ok &= expect(jsonIntValue(dmcDebug, "dmcLoopEnabled") == 0,
                 "NES DMC one-shot regression should keep the loop bit disabled");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") == 32,
                 "NES DMC one-shot should report all fixture bits consumed before held-note replay");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleByteIndex") == 4,
                 "NES DMC one-shot should stop at the exact end of the selected fixture");
    ok &= expect(jsonIntValue(dmcDebug, "dmcMixerLevel") == jsonIntValue(dmcDebug, "dmcLevel"),
                 "NES DMC one-shot should hold the final DAC level after Loop-off playback stops");
    auto dmcOneShotInfo = dmcOneShotProcessor.nesDmcSamplePlaybackInfo();
    ok &= expect(! dmcOneShotInfo.sampleActive && dmcOneShotInfo.sampleCompleted && dmcOneShotInfo.bitsPlayed == 32,
                 "NES DMC playback info should expose stopped one-shot state for the UI");
    ok &= expect(dmcOneShotInfo.statusLine.contains("One-shot, no loop; DAC holds") && dmcOneShotInfo.statusLine.contains("stopped"),
                 "NES DMC playback info should describe stopped one-shot playback without implying a loop");
    ok &= expect(jsonIntValue(dmcDebug, "dmcMixerLevel") > 0,
                 "NES DMC one-shot regression fixture should leave a held DAC value to prove this is not silent by coincidence");
    sendNoteOff(dmcOneShotProcessor, 48);
    ok &= expect(renderEmptyBlocksPeak(dmcOneShotProcessor, 4) <= 0.000001f,
                 "NES DMC completed one-shot should not keep the plugin voice audibly open after note release when Loop is off");

    ChipperAudioProcessor dmcNoRestartProcessor;
    dmcNoRestartProcessor.prepareToPlay(48000.0, 256);
    ok &= expect(dmcNoRestartProcessor.loadNesDmcSampleDirectory(dmcDir).wasOk(),
                 "Should load DMC sample directory for active one-shot restart regression");
    sendController(dmcNoRestartProcessor, 68, 0);
    sendController(dmcNoRestartProcessor, 118, controllerValueForChoice(dmcNoRestartProcessor, chipper::parameters::id::nesDmcRateIndex, 0));
    sendNoteOn(dmcNoRestartProcessor, 48);
    auto dmcNoRestartDebug = dmcNoRestartProcessor.currentCoreDebugStateJson();
    const auto dmcBitsAfterFirstTrigger = jsonIntValue(dmcNoRestartDebug, "dmcSampleBitsPlayed");
    ok &= expect(jsonIntValue(dmcNoRestartDebug, "dmcSampleActive") == 1,
                 "NES DMC slow one-shot should still be active after the first short trigger block");
    ok &= expect(dmcBitsAfterFirstTrigger > 0 && dmcBitsAfterFirstTrigger < 32,
                 "NES DMC slow one-shot fixture should be mid-stream before the retrigger regression");
    auto dmcActiveOneShotInfo = dmcNoRestartProcessor.nesDmcSamplePlaybackInfo();
    ok &= expect(dmcActiveOneShotInfo.sampleActive && ! dmcActiveOneShotInfo.sampleCompleted,
                 "NES DMC playback info should expose active one-shot state while bits are still stepping");
    sendNoteOn(dmcNoRestartProcessor, 48);
    dmcNoRestartDebug = dmcNoRestartProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcNoRestartDebug, "dmcSampleActive") == 1,
                 "NES DMC repeated note-on should keep the active one-shot stream running");
    ok &= expect(jsonIntValue(dmcNoRestartDebug, "dmcSampleBitsPlayed") > dmcBitsAfterFirstTrigger,
                 "NES DMC repeated note-on should not rewind the active one-shot sample to byte 0");
    ok &= expect(jsonIntValue(dmcNoRestartDebug, "dmcLoopEnabled") == 0,
                 "NES DMC repeated note-on regression should keep the loop bit disabled");

    setPlainFromHost(dmcOneShotProcessor, chipper::parameters::id::macroControl2, 0.73f);
    processEmptyBlock(dmcOneShotProcessor);
    dmcDebug = dmcOneShotProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 0,
                 "NES DMC one-shot should not restart when a host parameter change replays held notes");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") == 32,
                 "NES DMC held-note replay should preserve the completed one-shot bit position");
    ok &= expect(jsonIntValue(dmcDebug, "dmcMixerLevel") == jsonIntValue(dmcDebug, "dmcLevel"),
                 "NES DMC held-note replay should preserve the completed one-shot DAC hold");
    for (int i = 0; i < 16; ++i)
        processEmptyBlock(dmcOneShotProcessor);
    dmcDebug = dmcOneShotProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 0 && jsonIntValue(dmcDebug, "dmcSampleCompleted") == 1,
                 "NES DMC loop-off held note should remain stopped/completed after additional audio blocks");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") == 32 && jsonIntValue(dmcDebug, "dmcSampleByteIndex") == 4,
                 "NES DMC loop-off held note should not wrap or advance after the one-shot endpoint");

    ChipperAudioProcessor dmcMonoRestoreProcessor;
    dmcMonoRestoreProcessor.prepareToPlay(48000.0, 256);
    ok &= expect(dmcMonoRestoreProcessor.loadNesDmcSampleDirectory(dmcDir).wasOk(),
                 "Should load DMC sample directory for mono-note restore one-shot regression");
    sendController(dmcMonoRestoreProcessor, 68, 0);
    sendController(dmcMonoRestoreProcessor, 118, controllerValueForChoice(dmcMonoRestoreProcessor, chipper::parameters::id::nesDmcRateIndex, 15));
    sendNoteOn(dmcMonoRestoreProcessor, 48);
    for (int i = 0; i < 8; ++i)
        processEmptyBlock(dmcMonoRestoreProcessor);
    sendNoteOn(dmcMonoRestoreProcessor, 52);
    for (int i = 0; i < 8; ++i)
        processEmptyBlock(dmcMonoRestoreProcessor);
    auto dmcBeforeMonoRestoreDebug = dmcMonoRestoreProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcBeforeMonoRestoreDebug, "dmcSampleActive") == 0
                     && jsonIntValue(dmcBeforeMonoRestoreDebug, "dmcSampleCompleted") == 1
                     && jsonIntValue(dmcBeforeMonoRestoreDebug, "dmcSampleBitsPlayed") == 32,
                 "NES DMC mono restore regression setup should finish the second one-shot before note restore");
    sendNoteOff(dmcMonoRestoreProcessor, 52);
    dmcDebug = dmcMonoRestoreProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 0,
                 "NES DMC one-shot should not restart when Big Mono restores a previous held note");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleCompleted") == 1,
                 "NES DMC one-shot should remain completed after Big Mono note restore");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") == 32 && jsonIntValue(dmcDebug, "dmcSampleByteIndex") == 4,
                 "NES DMC Big Mono restore should preserve the stopped one-shot endpoint");

    ChipperAudioProcessor dmcNoteMapOneShotProcessor;
    dmcNoteMapOneShotProcessor.prepareToPlay(48000.0, 256);
    ok &= expect(dmcNoteMapOneShotProcessor.loadNesDmcSampleDirectory(dmcDir).wasOk(),
                 "Should load DMC sample directory for note-map one-shot regression");
    sendController(dmcNoteMapOneShotProcessor, 68, 0);
    sendController(dmcNoteMapOneShotProcessor, 118, controllerValueForChoice(dmcNoteMapOneShotProcessor, chipper::parameters::id::nesDmcRateIndex, 15));
    sendController(dmcNoteMapOneShotProcessor, 119, controllerValueForChoice(dmcNoteMapOneShotProcessor, chipper::parameters::id::nesDmcPlaybackMode, 1));
    sendController(dmcNoteMapOneShotProcessor, 69, controllerValueForChoice(dmcNoteMapOneShotProcessor, chipper::parameters::id::nesDmcMapRoot, 48));
    sendNoteOn(dmcNoteMapOneShotProcessor, 48);
    for (int i = 0; i < 8; ++i)
        processEmptyBlock(dmcNoteMapOneShotProcessor);
    dmcDebug = dmcNoteMapOneShotProcessor.currentCoreDebugStateJson();
    auto dmcNoteMapInfo = dmcNoteMapOneShotProcessor.nesDmcSamplePlaybackInfo();
    ok &= expect(dmcNoteMapInfo.activeSlot == 0 && dmcNoteMapInfo.statusLine.contains("One-shot, no loop; DAC holds"),
                 "NES DMC Note Map loop-off should keep one-shot status for the mapped slot");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 0,
                 "NES DMC Note Map loop-off should stop after the mapped sample ends");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleCompleted") == 1,
                 "NES DMC Note Map loop-off should report a completed one-shot stream");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") == 32,
                 "NES DMC Note Map loop-off should consume one mapped fixture pass without wrapping");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleByteIndex") == 4,
                 "NES DMC Note Map loop-off should stop at the exact mapped fixture end");

    for (int i = 0; i < 8; ++i)
        processEmptyBlock(dmcNoteMapOneShotProcessor);
    dmcDebug = dmcNoteMapOneShotProcessor.currentCoreDebugStateJson();
    dmcNoteMapInfo = dmcNoteMapOneShotProcessor.nesDmcSamplePlaybackInfo();
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 0
                     && jsonIntValue(dmcDebug, "dmcSampleCompleted") == 1
                     && jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") == 32
                     && jsonIntValue(dmcDebug, "dmcSampleByteIndex") == 4,
                 "NES DMC Note Map loop-off should not retrigger or wrap while the mapped note remains held");
    ok &= expect(! dmcNoteMapInfo.sampleActive && dmcNoteMapInfo.sampleCompleted && dmcNoteMapInfo.bitsPlayed == 32,
                 "NES DMC Note Map playback info should keep reporting a stopped one-shot while the mapped note remains held");
    ChipperAudioProcessor dmcLoopProcessor;
    dmcLoopProcessor.prepareToPlay(48000.0, 256);
    ok &= expect(dmcLoopProcessor.loadNesDmcSampleDirectory(dmcDir).wasOk(),
                 "Should load DMC sample directory for loop regression");
    sendController(dmcLoopProcessor, 68, 127);
    sendController(dmcLoopProcessor, 118, controllerValueForChoice(dmcLoopProcessor, chipper::parameters::id::nesDmcRateIndex, 15));
    sendNoteOn(dmcLoopProcessor, 48);
    for (int block = 0; block < 14; ++block)
        processEmptyBlock(dmcLoopProcessor);
    dmcDebug = dmcLoopProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleActive") == 1,
                 "NES DMC loop should remain active after the selected sample end when Loop is on");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleCompleted") == 0,
                 "NES DMC loop should not report one-shot completion while wrapping is enabled");
    auto dmcLoopInfo = dmcLoopProcessor.nesDmcSamplePlaybackInfo();
    ok &= expect(dmcLoopInfo.loopEnabled && dmcLoopInfo.sampleActive && ! dmcLoopInfo.sampleCompleted,
                 "NES DMC playback info should expose active loop state while the loop bit is enabled");
    ok &= expect(jsonIntValue(dmcDebug, "dmcLoopEnabled") == 1,
                 "NES DMC loop regression should keep the loop bit enabled");
    ok &= expect(jsonIntValue(dmcDebug, "dmcSampleBitsPlayed") > 32,
                 "NES DMC loop should continue stepping bits after the first fixture pass");
    ok &= expect(jsonIntValue(dmcDebug, "dmcMixerLevel") > 0,
                 "NES DMC loop should keep the sample audible through the DMC mixer lane");

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

    ChipperAudioProcessor missingDmcXmlProcessor;
    missingDmcXmlProcessor.prepareToPlay(48000.0, 64);
    auto missingDmcPresetXml = missingDmcXmlProcessor.createStateXml();
    ok &= expect(missingDmcPresetXml != nullptr, "Should create DMC preset XML for missing-sample restore warning");
    if (missingDmcPresetXml != nullptr)
    {
        addMissingSampleReference(*missingDmcPresetXml,
                                  "CHIPPER_DMC_BANK",
                                  "DMC_SAMPLE",
                                  portablePresetDir.getChildFile("missing-dmc.dmc"));
        ChipperAudioProcessor missingDmcPresetProcessor;
        missingDmcPresetProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(missingDmcPresetProcessor.restoreStateXml(*missingDmcPresetXml, portablePresetDir).wasOk(),
                     "DMC preset restore should not fail the whole preset when a sample reference is missing");
        auto missingDmcInfo = missingDmcPresetProcessor.nesDmcSamplePlaybackInfo();
        ok &= expect(missingDmcInfo.statusLine.contains("DMC sample restore issue")
                         && missingDmcInfo.statusLine.contains("missing-dmc.dmc"),
                     "DMC preset restore should expose missing sample references in the sample status line");
        const auto missingDmcPreview = missingDmcPresetProcessor.sampleWaveformSnapshot(chipper::ChipMode::nes);
        ok &= expect(missingDmcPreview.label.contains("DMC sample restore issue")
                         && missingDmcPreview.label.contains("missing-dmc.dmc"),
                     "DMC waveform preview should expose missing sample references in its label");
        ok &= expect(missingDmcPresetProcessor.loadNesDmcSampleFile(dmcDir.getChildFile("sample-00.dmc")).wasOk(),
                     "DMC manual sample load should succeed after a missing-reference restore warning");
        missingDmcInfo = missingDmcPresetProcessor.nesDmcSamplePlaybackInfo();
        ok &= expect(! missingDmcInfo.statusLine.contains("restore issue"),
                     "DMC manual sample load should clear stale missing-reference restore warnings");
    }
    portablePresetDir.deleteRecursively();
    dmcDir.deleteRecursively();

    ChipperAudioProcessor emptySpcSampleProcessor;
    emptySpcSampleProcessor.prepareToPlay(48000.0, 64);
    sendController(emptySpcSampleProcessor, 70, controllerValueForChoice(emptySpcSampleProcessor, chipper::parameters::id::chipMode, 7));
    auto emptySpcInfo = emptySpcSampleProcessor.spc700BrrSampleInfo();
    ok &= expect(! emptySpcInfo.loaded && emptySpcInfo.statusLine.contains("Generated SPC700 template active"),
                 "SPC700 empty sample status should clarify that generated templates remain playable without an external bank");

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
    ok &= expect(outOfRangeMappedInfo.bankCount == 3 && outOfRangeMappedInfo.bankByteCount == 27 && outOfRangeMappedInfo.bankBrrBlockCount == 3,
                 "SPC700 out-of-range note-map status should retain loaded BRR bank counts");
    ok &= expect(outOfRangeMappedInfo.mapRootNote == 36 && outOfRangeMappedInfo.mapHighNote == 38,
                 "SPC700 out-of-range note-map status should retain the visible mapped key span");
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

    ChipperAudioProcessor missingSpcXmlProcessor;
    missingSpcXmlProcessor.prepareToPlay(48000.0, 64);
    auto missingSpcPresetXml = missingSpcXmlProcessor.createStateXml();
    ok &= expect(missingSpcPresetXml != nullptr, "Should create SPC700 preset XML for missing-sample restore warning");
    if (missingSpcPresetXml != nullptr)
    {
        addMissingSampleReference(*missingSpcPresetXml,
                                  "CHIPPER_SPC700_BRR_BANK",
                                  "BRR_SAMPLE",
                                  portableSpcPresetDir.getChildFile("missing-spc.brr"));
        ChipperAudioProcessor missingSpcPresetProcessor;
        missingSpcPresetProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(missingSpcPresetProcessor.restoreStateXml(*missingSpcPresetXml, portableSpcPresetDir).wasOk(),
                     "SPC700 preset restore should not fail the whole preset when a sample reference is missing");
        auto missingSpcInfo = missingSpcPresetProcessor.spc700BrrSampleInfo();
        ok &= expect(missingSpcInfo.statusLine.contains("SPC700 sample restore issue")
                         && missingSpcInfo.statusLine.contains("missing-spc.brr"),
                     "SPC700 preset restore should expose missing sample references in the sample status line");
        const auto missingSpcPreview = missingSpcPresetProcessor.sampleWaveformSnapshot(chipper::ChipMode::spc700);
        ok &= expect(missingSpcPreview.label.contains("SPC700 sample restore issue")
                         && missingSpcPreview.label.contains("missing-spc.brr"),
                     "SPC700 waveform preview should expose missing sample references in its label");
        ok &= expect(missingSpcPresetProcessor.loadSpc700BrrSampleFile(brrDir.getChildFile("brr-00.brr")).wasOk(),
                     "SPC700 manual sample load should succeed after a missing-reference restore warning");
        missingSpcInfo = missingSpcPresetProcessor.spc700BrrSampleInfo();
        ok &= expect(! missingSpcInfo.statusLine.contains("restore issue"),
                     "SPC700 manual sample load should clear stale missing-reference restore warnings");
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
    ok &= expect(spcWavOutOfRangeInfo.bankCount == 3 && spcWavOutOfRangeInfo.bankByteCount == 768 && spcWavOutOfRangeInfo.bankBrrBlockCount == 0,
                 "SPC700 WAV out-of-range note-map status should retain loaded sample bank counts");
    ok &= expect(spcWavOutOfRangeInfo.mapRootNote == 36 && spcWavOutOfRangeInfo.mapHighNote == 38,
                 "SPC700 WAV out-of-range note-map status should retain the visible mapped key span");
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

    ChipperAudioProcessor emptyPaulaSampleProcessor;
    emptyPaulaSampleProcessor.prepareToPlay(48000.0, 64);
    sendController(emptyPaulaSampleProcessor, 70, controllerValueForChoice(emptyPaulaSampleProcessor, chipper::parameters::id::chipMode, 9));
    auto emptyPaulaInfo = emptyPaulaSampleProcessor.paulaSampleInfo();
    ok &= expect(! emptyPaulaInfo.loaded && emptyPaulaInfo.statusLine.contains("No external Paula sample bank"),
                 "Paula empty sample status should clarify only the external bank is missing because generated templates remain playable");

    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 9));
    auto paulaDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-paula-sample-bank-test");
    paulaDir.deleteRecursively();
    ok &= expect(paulaDir.createDirectory().wasOk(), "Should create temporary Paula sample bank test directory");
    for (int i = 0; i < 4; ++i)
    {
        const auto name = "paula-" + juce::String(i).paddedLeft('0', 2) + ".wav";
        const auto wroteFixture = i == 3
            ? writeLoopedWavFixture(paulaDir.getChildFile(name), 220.0f + static_cast<float>(i) * 55.0f, 32u, 160u)
            : writeWavFixture(paulaDir.getChildFile(name), 220.0f + static_cast<float>(i) * 55.0f);
        ok &= expect(wroteFixture,
                     "Should write temporary Paula WAV fixture " + name.toStdString());
    }
    ok &= expect(write8svxFixture(paulaDir.getChildFile("zz-paula-native.8svx"), 0x20u, 64u, 128u),
                 "Should write temporary Paula 8SVX fixture");
    ok &= expect(writeLoopedAiffFixture(paulaDir.getChildFile("paula-04.aiff"), 440.0f, 48u, 176u),
                 "Should write temporary Paula AIFF fixture with loop markers");

    ok &= expect(processor.loadPaulaSampleDirectory(paulaDir).wasOk(), "Should load Paula sample directory");
    auto paulaNames = processor.paulaSampleNames();
    ok &= expect(paulaNames.size() == 6 && paulaNames[0] == "paula-00.wav" && paulaNames[3] == "paula-03.wav"
                     && paulaNames[4] == "paula-04.aiff" && paulaNames[5] == "zz-paula-native.8svx",
                 "Paula folder should expose sorted WAV, AIFF, and 8SVX samples as playable slots");
    auto paulaEntries = processor.paulaSampleEntryInfo();
    ok &= expect(paulaEntries.size() == 6u && paulaEntries[0].activeSlot && paulaEntries[5].activeSlot,
                 "Paula bank editor should expose checked WAV, AIFF, and 8SVX files as active playable slots");
    processor.setPaulaSampleIncluded(2, false);
    paulaNames = processor.paulaSampleNames();
    paulaEntries = processor.paulaSampleEntryInfo();
    ok &= expect(paulaNames.size() == 5 && paulaNames[2] == "paula-03.wav"
                     && paulaNames[3] == "paula-04.aiff" && paulaNames[4] == "zz-paula-native.8svx",
                 "Unchecking a Paula sample should remove it from the playable bank and close gaps");
    ok &= expect(! paulaEntries[2].included && ! paulaEntries[2].activeSlot,
                 "Unchecked Paula entries should report inactive");
    processor.setPaulaSampleIncluded(2, true);
    setPlainFromHost(processor, chipper::parameters::id::spc700Voice1SampleSlot, 0.0f);
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 3));
    auto paulaInfo = processor.paulaSampleInfo();
    ok &= expect(paulaInfo.loaded && paulaInfo.sampleName == "paula-03.wav" && paulaInfo.byteCount == 256,
                 "CC117 should select the active Paula sample bank slot");
    ok &= expect(paulaInfo.hasLoop && paulaInfo.loopStartSample == 32 && paulaInfo.loopEndSample == 160
                     && paulaInfo.statusLine.contains("Loop 32-160"),
                 "Paula WAV import should expose smpl loop metadata in status");
    ok &= expect(paulaInfo.statusLine.contains("8-bit samples"),
                 "Paula status should report imported 8-bit sample count");
    const auto paulaWavLoopPreview = processor.sampleWaveformSnapshot(chipper::ChipMode::paula);
    ok &= expect(paulaWavLoopPreview.loaded && paulaWavLoopPreview.hasLoop && paulaWavLoopPreview.selectedSlot == 3,
                 "Paula waveform preview should expose loop metadata for the selected WAV sample");
    ok &= expectNear(paulaWavLoopPreview.loopStart, 32.0f / 255.0f, 0.002f,
                     "Paula waveform preview should normalize imported WAV loop start");
    ok &= expectNear(paulaWavLoopPreview.loopEnd, 160.0f / 255.0f, 0.002f,
                     "Paula waveform preview should normalize imported WAV loop end");
    const auto paulaWavLoopDebug = processor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(paulaWavLoopDebug, "externalSampleLoopMetadataSelected") == 1
                     && jsonIntValue(paulaWavLoopDebug, "externalSampleLoopStartSelected") == 32
                     && jsonIntValue(paulaWavLoopDebug, "externalSampleLoopEndSelected") == 160,
                 "Paula core debug state should retain imported WAV loop metadata for the selected bank slot");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 4));
    paulaInfo = processor.paulaSampleInfo();
    ok &= expect(paulaInfo.loaded && paulaInfo.sampleName == "paula-04.aiff" && paulaInfo.byteCount == 256,
                 "CC117 should select an imported Paula AIFF sample bank slot");
    ok &= expect(paulaInfo.hasLoop && paulaInfo.loopStartSample == 48 && paulaInfo.loopEndSample == 176
                     && paulaInfo.statusLine.contains("Loop 48-176"),
                 "Paula AIFF import should expose INST/MARK loop metadata in status");
    const auto paulaAiffLoopPreview = processor.sampleWaveformSnapshot(chipper::ChipMode::paula);
    ok &= expect(paulaAiffLoopPreview.loaded && paulaAiffLoopPreview.hasLoop && paulaAiffLoopPreview.selectedSlot == 4,
                 "Paula waveform preview should expose loop metadata for the selected AIFF sample");
    ok &= expectNear(paulaAiffLoopPreview.loopStart, 48.0f / 255.0f, 0.002f,
                     "Paula waveform preview should normalize imported AIFF loop start");
    ok &= expectNear(paulaAiffLoopPreview.loopEnd, 176.0f / 255.0f, 0.002f,
                     "Paula waveform preview should normalize imported AIFF loop end");
    const auto paulaAiffLoopDebug = processor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(paulaAiffLoopDebug, "externalSampleLoopMetadataSelected") == 1
                     && jsonIntValue(paulaAiffLoopDebug, "externalSampleLoopStartSelected") == 48
                     && jsonIntValue(paulaAiffLoopDebug, "externalSampleLoopEndSelected") == 176,
                 "Paula core debug state should retain imported AIFF loop metadata for the selected bank slot");
    sendController(processor, 117, controllerValueForChoice(processor, chipper::parameters::id::nesDmcSampleSlot, 5));
    paulaInfo = processor.paulaSampleInfo();
    ok &= expect(paulaInfo.loaded && paulaInfo.sampleName == "zz-paula-native.8svx" && paulaInfo.byteCount == 256,
                 "CC117 should select an imported Paula 8SVX sample bank slot");
    ok &= expect(paulaInfo.hasLoop && paulaInfo.loopStartSample == 64 && paulaInfo.loopEndSample == 192
                     && paulaInfo.statusLine.contains("Loop 64-192"),
                 "Paula 8SVX import should expose VHDR loop metadata in status");
    const auto paulaLoopPreview = processor.sampleWaveformSnapshot(chipper::ChipMode::paula);
    ok &= expect(paulaLoopPreview.loaded && paulaLoopPreview.hasLoop && paulaLoopPreview.selectedSlot == 5,
                 "Paula waveform preview should expose loop metadata for the selected 8SVX sample");
    ok &= expectNear(paulaLoopPreview.loopStart, 64.0f / 255.0f, 0.002f,
                     "Paula waveform preview should normalize imported 8SVX loop start");
    ok &= expectNear(paulaLoopPreview.loopEnd, 192.0f / 255.0f, 0.002f,
                     "Paula waveform preview should normalize imported 8SVX loop end");
    const auto paulaLoopDebug = processor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(paulaLoopDebug, "externalSampleLoopMetadataSelected") == 1
                     && jsonIntValue(paulaLoopDebug, "externalSampleLoopStartSelected") == 64
                     && jsonIntValue(paulaLoopDebug, "externalSampleLoopEndSelected") == 192,
                 "Paula core debug state should retain imported 8SVX loop metadata for the selected bank slot");
    ok &= expect(jsonIntValue(paulaLoopDebug, "sampleLoopStart0") == 64
                     && jsonIntValue(paulaLoopDebug, "sampleLoopEnd0") == 192,
                 "Paula core debug state should apply imported 8SVX loop metadata to channel playback");

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
    ok &= expect(paulaOutOfRangeMappedInfo.bankCount == 6 && paulaOutOfRangeMappedInfo.bankByteCount == 1536,
                 "Paula out-of-range note-map status should retain loaded sample bank counts");
    ok &= expect(paulaOutOfRangeMappedInfo.mapRootNote == 36 && paulaOutOfRangeMappedInfo.mapHighNote == 41,
                 "Paula out-of-range note-map status should retain the visible mapped key span");
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
    ok &= expect(restoredPaulaNames.size() == 6, "Paula state restore should reload staged WAV, AIFF, and 8SVX sample paths");
    ok &= expect(restoredPaulaInfo.loaded && restoredPaulaInfo.sampleName == "paula-03.wav",
                 "Paula state restore should preserve the selected slot after processing resumes");
    ok &= expect(restoredPaulaInfo.hasLoop && restoredPaulaInfo.loopStartSample == 32 && restoredPaulaInfo.loopEndSample == 160,
                 "Paula state restore should preserve imported WAV smpl loop metadata after reloading paths");

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

    ChipperAudioProcessor missingPaulaXmlProcessor;
    missingPaulaXmlProcessor.prepareToPlay(48000.0, 64);
    auto missingPaulaPresetXml = missingPaulaXmlProcessor.createStateXml();
    ok &= expect(missingPaulaPresetXml != nullptr, "Should create Paula preset XML for missing-sample restore warning");
    if (missingPaulaPresetXml != nullptr)
    {
        addMissingSampleReference(*missingPaulaPresetXml,
                                  "CHIPPER_PAULA_SAMPLE_BANK",
                                  "PAULA_SAMPLE",
                                  portablePaulaPresetDir.getChildFile("missing-paula.wav"));
        ChipperAudioProcessor missingPaulaPresetProcessor;
        missingPaulaPresetProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(missingPaulaPresetProcessor.restoreStateXml(*missingPaulaPresetXml, portablePaulaPresetDir).wasOk(),
                     "Paula preset restore should not fail the whole preset when a sample reference is missing");
        auto missingPaulaInfo = missingPaulaPresetProcessor.paulaSampleInfo();
        ok &= expect(missingPaulaInfo.statusLine.contains("Paula sample restore issue")
                         && missingPaulaInfo.statusLine.contains("missing-paula.wav"),
                     "Paula preset restore should expose missing sample references in the sample status line");
        const auto missingPaulaPreview = missingPaulaPresetProcessor.sampleWaveformSnapshot(chipper::ChipMode::paula);
        ok &= expect(missingPaulaPreview.label.contains("Paula sample restore issue")
                         && missingPaulaPreview.label.contains("missing-paula.wav"),
                     "Paula waveform preview should expose missing sample references in its label");
        ok &= expect(missingPaulaPresetProcessor.loadPaulaSampleFile(paulaDir.getChildFile("paula-00.wav")).wasOk(),
                     "Paula manual sample load should succeed after a missing-reference restore warning");
        missingPaulaInfo = missingPaulaPresetProcessor.paulaSampleInfo();
        ok &= expect(! missingPaulaInfo.statusLine.contains("restore issue"),
                     "Paula manual sample load should clear stale missing-reference restore warnings");
    }

    auto paulaModFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chipper-paula-mod-sample-test.mod");
    paulaModFile.deleteFile();
    ok &= expect(writeProTrackerModFixture(paulaModFile), "Should write temporary ProTracker MOD fixture");
    ChipperAudioProcessor paulaModProcessor;
    paulaModProcessor.prepareToPlay(48000.0, 64);
    sendController(paulaModProcessor, 70, controllerValueForChoice(paulaModProcessor, chipper::parameters::id::chipMode, 9));
    ok &= expect(paulaModProcessor.loadPaulaSampleFile(paulaModFile).wasOk(),
                 "Paula MOD import should extract sample instruments into the playable bank");
    auto paulaModNames = paulaModProcessor.paulaSampleNames();
    ok &= expect(paulaModNames.size() == 2 && paulaModNames[0].contains("#01 Loop Bass")
                     && paulaModNames[1].contains("#02 One Shot"),
                 "Paula MOD import should expose non-empty instruments as named sample slots");
    auto paulaModInfo = paulaModProcessor.paulaSampleInfo();
    ok &= expect(paulaModInfo.loaded && paulaModInfo.sampleName.contains("#01 Loop Bass") && paulaModInfo.byteCount == 64,
                 "Paula MOD import should select the first extracted sample by default");
    ok &= expect(paulaModInfo.hasLoop && paulaModInfo.loopStartSample == 16 && paulaModInfo.loopEndSample == 48,
                 "Paula MOD import should preserve ProTracker sample repeat points");
    const auto paulaModDebug = paulaModProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(paulaModDebug, "externalSampleLoopMetadataSelected") == 1
                     && jsonIntValue(paulaModDebug, "externalSampleLoopStartSelected") == 16
                     && jsonIntValue(paulaModDebug, "externalSampleLoopEndSelected") == 48,
                 "Paula core debug state should expose loop metadata from the selected MOD sample");
    sendController(paulaModProcessor, 117, controllerValueForChoice(paulaModProcessor, chipper::parameters::id::nesDmcSampleSlot, 1));
    paulaModInfo = paulaModProcessor.paulaSampleInfo();
    ok &= expect(paulaModInfo.loaded && paulaModInfo.sampleName.contains("#02 One Shot") && paulaModInfo.byteCount == 32
                     && ! paulaModInfo.hasLoop,
                 "CC117 should select one-shot MOD samples without inventing loop metadata");
    juce::MemoryBlock savedPaulaModState;
    paulaModProcessor.getStateInformation(savedPaulaModState);
    ChipperAudioProcessor restoredPaulaModProcessor;
    restoredPaulaModProcessor.prepareToPlay(48000.0, 64);
    restoredPaulaModProcessor.setStateInformation(savedPaulaModState.getData(), static_cast<int>(savedPaulaModState.getSize()));
    processEmptyBlock(restoredPaulaModProcessor);
    auto restoredPaulaModNames = restoredPaulaModProcessor.paulaSampleNames();
    auto restoredPaulaModInfo = restoredPaulaModProcessor.paulaSampleInfo();
    ok &= expect(restoredPaulaModNames.size() == 2, "Paula state restore should reload all extracted MOD sample slots");
    ok &= expect(restoredPaulaModInfo.loaded && restoredPaulaModInfo.sampleName.contains("#02 One Shot")
                     && restoredPaulaModInfo.byteCount == 32 && ! restoredPaulaModInfo.hasLoop,
                 "Paula state restore should preserve the selected extracted MOD sample slot");
    paulaModFile.deleteFile();
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
                     "CC74 NES macro change should reset Pulse 2 Duty to Preset");
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
