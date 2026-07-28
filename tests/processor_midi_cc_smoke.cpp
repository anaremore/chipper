#include "PluginProcessor.h"
#include "State/MotionState.h"
#include "State/PluginStateSchema.h"
#include "State/WavetableState.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace allocation_probe
{
std::atomic<bool> enabled { false };
std::atomic<size_t> count { 0 };
}

void* operator new(std::size_t size)
{
    if (allocation_probe::enabled.load(std::memory_order_relaxed))
        allocation_probe::count.fetch_add(1u, std::memory_order_relaxed);

    if (auto* memory = std::malloc(size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete[](void* memory) noexcept
{
    ::operator delete(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

void operator delete[](void* memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

namespace
{
bool expect(bool condition, const std::string& message)
{
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

std::unique_ptr<juce::XmlElement> stateXmlFromBinary(const juce::MemoryBlock& state)
{
    if (state.isEmpty() || state.getSize() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return {};

    return juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
}

size_t countElementsNamed(const juce::XmlElement& element, const juce::String& tagName)
{
    auto count = element.hasTagName(tagName) ? size_t { 1 } : size_t { 0 };
    for (const auto* child : element.getChildIterator())
        if (child != nullptr)
            count += countElementsNamed(*child, tagName);
    return count;
}

juce::XmlElement* findEmbeddedPayloadAt(juce::XmlElement& element, size_t requestedIndex, size_t& currentIndex)
{
    if (element.hasTagName(chipper::state::embeddedSampleStateTag))
    {
        if (currentIndex == requestedIndex)
            return &element;
        ++currentIndex;
    }

    for (auto* child : element.getChildIterator())
    {
        if (child == nullptr)
            continue;
        if (auto* match = findEmbeddedPayloadAt(*child, requestedIndex, currentIndex))
            return match;
    }
    return nullptr;
}

std::unique_ptr<juce::XmlElement> loadStateFixture(const char* fileName)
{
    return juce::XmlDocument::parse(juce::File(CHIPPER_STATE_FIXTURE_DIR).getChildFile(fileName));
}

std::vector<std::string> loadParameterIdContract()
{
    const auto file = juce::File(CHIPPER_STATE_FIXTURE_DIR).getSiblingFile("parameter-id-contract.txt");
    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    std::vector<std::string> ids;
    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();
        if (trimmed.isNotEmpty() && ! trimmed.startsWithChar('#'))
            ids.push_back(trimmed.toStdString());
    }
    return ids;
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

size_t processAllocationCount(ChipperAudioProcessor& processor, juce::MidiBuffer& midi);

bool expectSteadyStateProcessingDoesNotAllocate()
{
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    bool ok = true;

    const auto chipCount = chipper::parameters::chipModeChoices().size();
    for (int chipChoice = 0; chipChoice < chipCount; ++chipChoice)
    {
        setPlainFromHost(processor, chipper::parameters::id::chipMode, static_cast<float>(chipChoice));
        allocation_probe::count.store(0u, std::memory_order_relaxed);
        allocation_probe::enabled.store(true, std::memory_order_release);
        processor.processBlock(buffer, midi);
        allocation_probe::enabled.store(false, std::memory_order_release);
        const auto switchAllocations = allocation_probe::count.load(std::memory_order_relaxed);
        ok &= expect(switchAllocations == 0u,
                     "Chip-mode switch allocated " + std::to_string(switchAllocations)
                         + " times in processBlock for " + chipper::parameters::chipModeChoices()[chipChoice].toStdString());
        processor.processBlock(buffer, midi);

        allocation_probe::count.store(0u, std::memory_order_relaxed);
        allocation_probe::enabled.store(true, std::memory_order_release);
        for (int block = 0; block < 4; ++block)
            processor.processBlock(buffer, midi);
        allocation_probe::enabled.store(false, std::memory_order_release);

        const auto allocations = allocation_probe::count.load(std::memory_order_relaxed);
        ok &= expect(allocations == 0u,
                     "Steady-state processBlock allocated " + std::to_string(allocations)
                         + " times for " + chipper::parameters::chipModeChoices()[chipChoice].toStdString());
    }

    for (int strictness = 0; strictness < 3; ++strictness)
    {
        setPlainFromHost(processor, chipper::parameters::id::accuracy, static_cast<float>(strictness));
        const auto allocations = processAllocationCount(processor, midi);
        ok &= expect(allocations == 0u,
                     "Strictness switch allocated " + std::to_string(allocations) + " times in processBlock");
    }

    setPlainFromHost(processor, chipper::parameters::id::clockHz, 8001000.0f);
    const auto clockAllocations = processAllocationCount(processor, midi);
    ok &= expect(clockAllocations == 0u,
                 "Clock change allocated " + std::to_string(clockAllocations) + " times in processBlock");

    setPlainFromHost(processor, chipper::parameters::id::chipMode, 0.0f);
    processor.processBlock(buffer, midi);
    juce::MidiBuffer controlMidi;
    controlMidi.addEvent(juce::MidiMessage::controllerEvent(1, 76, 96), 0);
    auto midiCcAllocations = processAllocationCount(processor, controlMidi);
    ok &= expect(midiCcAllocations == 0u,
                 "Audio-thread MIDI CC update allocated " + std::to_string(midiCcAllocations) + " times");

    controlMidi.clear();
    controlMidi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 0);
    midiCcAllocations = processAllocationCount(processor, controlMidi);
    ok &= expect(midiCcAllocations == 0u,
                 "Audio-thread macro CC update allocated " + std::to_string(midiCcAllocations) + " times");

    return ok;
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
bool expectOpmDirectLfoAndFeedbackPitchNeutrality()
{
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, 12));
    setPlainFromHost(processor, chipper::parameters::id::waveShape, 5.0f);
    setPlainFromHost(processor, chipper::parameters::id::stereoSpread, 0.65f);
    setPlainFromHost(processor, chipper::parameters::id::fmOperator1Multiplier, 1.0f);
    setPlainFromHost(processor, chipper::parameters::id::fmOperator2Multiplier, 16.0f);
    setPlainFromHost(processor, chipper::parameters::id::fmOperator3SustainRate, 6.0f);
    setPlainFromHost(processor, chipper::parameters::id::fmOperator4SustainRate, 32.0f);
    sendNoteOn(processor, 69);
    const auto before = processor.currentCoreDebugStateJson();
    const auto keyCode = jsonIntValue(before, "keyCode0");
    const auto keyFraction = jsonIntValue(before, "keyFraction0");
    sendController(processor, 12, controllerValueForChoice(processor, chipper::parameters::id::opmOperator1Dt1, 4));
    sendController(processor, 13, controllerValueForChoice(processor, chipper::parameters::id::opmOperator2Dt1, 8));
    sendController(processor, 14, controllerValueForChoice(processor, chipper::parameters::id::opmOperator3Dt1, 0));
    sendController(processor, 15, controllerValueForChoice(processor, chipper::parameters::id::opmOperator4Dt1, 0));
    sendController(processor, 16, controllerValueForChoice(processor, chipper::parameters::id::opmOperator1Dt2, 0));
    sendController(processor, 17, controllerValueForChoice(processor, chipper::parameters::id::opmOperator2Dt2, 0));
    sendController(processor, 18, controllerValueForChoice(processor, chipper::parameters::id::opmOperator3Dt2, 3));
    sendController(processor, 19, controllerValueForChoice(processor, chipper::parameters::id::opmOperator4Dt2, 4));
    const auto detune = processor.currentCoreDebugStateJson();
    sendController(processor, 20, controllerValueForChoice(processor, chipper::parameters::id::opmLfoWaveform, 4));
    sendController(processor, 21, controllerValueForChoice(processor, chipper::parameters::id::opmLfoPms, 8));
    sendController(processor, 22, controllerValueForChoice(processor, chipper::parameters::id::opmLfoAms, 4));
    const auto direct = processor.currentCoreDebugStateJson();
    auto ok = true;
    ok &= expect(jsonIntValue(detune, "opmOperatorDt1Choice0") == 4
                     && jsonIntValue(detune, "opmOperatorDt1Choice1") == 8
                     && jsonIntValue(detune, "opmOperatorDt2Choice2") == 3
                     && jsonIntValue(detune, "opmOperatorDt2Choice3") == 4,
                 "CC12-19 should apply the exact per-operator YM2151 DT1/DT2 choices to a held note");
    ok &= expect(jsonIntValue(detune, "operatorMultipleDt1Register0") == 0x30
                     && jsonIntValue(detune, "operatorMultipleDt1Register1") == 0x7f
                     && jsonIntValue(detune, "operatorDt2SustainRateRegister2") == 0x85
                     && jsonIntValue(detune, "operatorDt2SustainRateRegister3") == 0xdf,
                 "YM2151 detune MIDI CCs should write exact packed $40/$48/$D0/$D8 register bytes");
    ok &= expect(jsonIntValue(detune, "operatorMultiple0") == 0
                     && jsonIntValue(detune, "operatorMultiple1") == 15
                     && jsonIntValue(detune, "operatorSustainRate2") == 5
                     && jsonIntValue(detune, "operatorSustainRate3") == 31,
                 "YM2151 detune writes should preserve neighboring MULT and D2R fields");
    ok &= expect(jsonIntValue(detune, "keyCode0") == keyCode
                     && jsonIntValue(detune, "keyFraction0") == keyFraction,
                 "Changing YM2151 DT1/DT2 choices should preserve the held note's base KC/KF pitch registers");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::opmOperator1Dt1), 4.0f, 0.001f,
                     "CC12 should remain visible in the host parameter value");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::opmOperator4Dt2), 4.0f, 0.001f,
                     "CC19 should remain visible in the host parameter value");
    ok &= expect(jsonIntValue(direct, "opmLfoWaveformChoice") == 4
                     && jsonIntValue(direct, "opmLfoPmsChoice") == 8
                     && jsonIntValue(direct, "opmLfoAmsChoice") == 4,
                 "CC20-22 should apply the exact direct YM2151 LFO choices to a held note");
    ok &= expect(jsonIntValue(direct, "lfoWaveform") == 3
                     && jsonIntValue(direct, "lfoPmSensitivity") == 7
                     && jsonIntValue(direct, "lfoAmSensitivity") == 3
                     && jsonIntValue(direct, "lfoRegister1B") == 3
                     && jsonIntValue(direct, "lfoChannelRegister0") == 0x73,
                 "Direct YM2151 LFO choices should write exact $1B and $38+n register values");
    ok &= expect(jsonIntValue(direct, "keyCode0") == keyCode
                     && jsonIntValue(direct, "keyFraction0") == keyFraction,
                 "Changing direct YM2151 LFO choices should preserve held-note pitch");
    sendController(processor, 77, 127);
    const auto feedback = processor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(feedback, "feedback0") == 7
                     && jsonIntValue(feedback, "algorithmFeedbackRegister0") == 0xfcu,
                 "Maximum YM2151 feedback should update the native feedback register");
    ok &= expect(jsonIntValue(feedback, "keyCode0") == keyCode
                     && jsonIntValue(feedback, "keyFraction0") == keyFraction,
                 "YM2151 feedback must remain timbral and pitch-neutral for a held note");
    return ok;
}

bool expectFeedbackPitchNeutrality(int chipChoice, const char* label)
{
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    sendController(processor, 70, controllerValueForChoice(processor, chipper::parameters::id::chipMode, chipChoice));
    sendController(processor, 77, 0);
    sendNoteOn(processor, 69);

    const auto feedback0 = processor.currentCoreDebugStateJson();
    const auto fnum = jsonIntValue(feedback0, "fnum0");
    const auto block = jsonIntValue(feedback0, "block0");
    auto ok = expect(jsonIntValue(feedback0, "feedback0") == 0,
                     std::string(label) + " feedback-neutrality fixture should begin at native feedback 0");
    ok &= expect(fnum > 0,
                 std::string(label) + " feedback-neutrality fixture should expose a valid held-note pitch");

    sendController(processor, 77, 127);
    const auto feedback7 = processor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(feedback7, "feedback0") == 7,
                 std::string(label) + " maximum feedback should update the native feedback field");
    ok &= expect(jsonIntValue(feedback7, "fnum0") == fnum
                     && jsonIntValue(feedback7, "block0") == block,
                 std::string(label) + " feedback must remain timbral and pitch-neutral for a held note");
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

bool writeBinaryFixture(const juce::File& file, size_t byteCount)
{
    std::vector<uint8_t> data;
    data.reserve(byteCount);
    for (size_t i = 0; i < byteCount; ++i)
        data.push_back(static_cast<uint8_t>(i & 0xffu));

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

size_t processAllocationCount(ChipperAudioProcessor& processor, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> buffer(2, 64);
    allocation_probe::count.store(0u, std::memory_order_relaxed);
    allocation_probe::enabled.store(true, std::memory_order_release);
    processor.processBlock(buffer, midi);
    allocation_probe::enabled.store(false, std::memory_order_release);
    return allocation_probe::count.load(std::memory_order_relaxed);
}

float bufferPeak(const juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    auto peak = 0.0f;
    const auto start = std::clamp(startSample, 0, buffer.getNumSamples());
    const auto end = std::clamp(endSample, start, buffer.getNumSamples());
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = start; sample < end; ++sample)
            peak = std::max(peak, std::abs(buffer.getSample(channel, sample)));
    }
    return peak;
}

class FixedTempoPlayHead final : public juce::AudioPlayHead
{
public:
    explicit FixedTempoPlayHead(double tempo) : bpm(tempo) {}

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo position;
        position.setBpm(bpm);
        position.setIsPlaying(true);
        return position;
    }

private:
    double bpm = 120.0;
};

bool expectMotionPlaybackAndState()
{
    auto ok = true;
    chipper::MotionPattern pattern;
    pattern.enabled = true;
    pattern.rate = chipper::MotionRate::sixtyFourth;
    pattern.length = 2u;
    pattern.steps[0].pitch = 0;
    pattern.steps[0].level = 0u;
    pattern.steps[0].gate = chipper::MotionGate::hold;
    pattern.steps[1].pitch = 12;
    pattern.steps[1].level = chipper::motionMaximumLevel;
    pattern.steps[1].gate = chipper::MotionGate::retrigger;

    ok &= expectNear(static_cast<float>(chipper::motionStepSamples(chipper::MotionRate::sixtyFourth,
                                                                   240.0,
                                                                   48000.0)),
                     750.0f,
                     0.001f,
                     "1/64 tracker motion should resolve to exactly 750 samples at 240 BPM / 48 kHz");

    chipper::state::MotionState everyModeState;
    for (size_t index = 0; index < everyModeState.patterns.size(); ++index)
    {
        auto modePattern = pattern;
        modePattern.enabled = index % 2u == 0u;
        modePattern.steps[0].pitch = static_cast<int8_t>(static_cast<int>(index % 49u) - 24);
        everyModeState.patterns[index] = modePattern;
    }
    const auto everyModeXml = chipper::state::createMotionStateXml(everyModeState);
    chipper::state::MotionState everyModeRestored;
    ok &= expect(everyModeXml != nullptr
                     && everyModeXml->getNumChildElements()
                         == static_cast<int>(chipper::state::motionStateModeCount),
                 "Motion state should serialize one canonical entry for every chip mode");
    if (everyModeXml != nullptr)
        ok &= expect(chipper::state::restoreMotionStateXml(*everyModeXml, everyModeRestored).wasOk()
                         && everyModeRestored.patterns == everyModeState.patterns,
                     "Every canonical chip motion ID should round-trip without display-name coupling");

    auto processor = std::make_unique<ChipperAudioProcessor>();
    processor->prepareToPlay(48000.0, 900);
    ok &= expect(processor->setMotionPattern(chipper::ChipMode::nes, pattern),
                 "Processor should accept a sanitized per-chip motion pattern");
    const auto initialSnapshot = processor->motionSnapshot(chipper::ChipMode::nes);
    ok &= expect(initialSnapshot.pattern == pattern && initialSnapshot.revision > 0u,
                 "Motion snapshot should publish the exact edited NES pattern");

    const auto stateXml = processor->createStateXml();
    ok &= expect(stateXml != nullptr
                     && stateXml->getIntAttribute(chipper::state::schemaVersionAttribute) == chipper::state::currentSchemaVersion
                     && stateXml->getChildByName(chipper::state::motionStateTag) != nullptr,
                 "Current-schema processor state should embed edited tracker motion");
    if (stateXml != nullptr)
    {
        auto restored = std::make_unique<ChipperAudioProcessor>();
        restored->prepareToPlay(48000.0, 64);
        ok &= expect(restored->restoreStateXml(*stateXml).wasOk(),
                     "Tracker motion should restore successfully from project state");
        ok &= expect(restored->motionSnapshot(chipper::ChipMode::nes).pattern == pattern,
                     "Tracker motion should survive an exact host-state round trip");

        auto malformed = std::make_unique<juce::XmlElement>(*stateXml);
        if (auto* motion = malformed->getChildByName(chipper::state::motionStateTag))
            if (auto* savedPattern = motion->getChildByName(chipper::state::motionPatternStateTag))
                savedPattern->setAttribute("p0", chipper::motionMaximumPitch + 1);
        auto malformedRestore = std::make_unique<ChipperAudioProcessor>();
        malformedRestore->prepareToPlay(48000.0, 64);
        ok &= expect(malformedRestore->restoreStateXml(*malformed).failed(),
                     "Out-of-range tracker motion state should fail explicitly");
    }

    FixedTempoPlayHead playHead(240.0);
    processor->setPlayHead(&playHead);
    juce::AudioBuffer<float> buffer(2, 900);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    processor->processBlock(buffer, midi);
    const auto playingSnapshot = processor->motionSnapshot(chipper::ChipMode::nes);
    ok &= expect(bufferPeak(buffer, 0, 750) <= 0.000001f,
                 "Motion level zero should mute every sample before the exact step boundary");
    ok &= expect(bufferPeak(buffer, 750, 900) > 0.001f,
                 "The retriggered second motion step should become audible at its sample boundary");
    ok &= expect(playingSnapshot.activeStep == 1
                     && playingSnapshot.hostTempo
                     && std::abs(playingSnapshot.bpm - 240.0) <= 0.001,
                 "Motion snapshot should expose the active step and host tempo");
    processor->setPlayHead(nullptr);

    juce::MidiBuffer emptyMidi;
    const auto allocations = processAllocationCount(*processor, emptyMidi);
    ok &= expect(allocations == 0u,
                 "Active sample-accurate tracker motion should not allocate in processBlock");

    auto chipPolyProcessor = std::make_unique<ChipperAudioProcessor>();
    chipPolyProcessor->prepareToPlay(48000.0, 256);
    setPlainFromHost(*chipPolyProcessor, chipper::parameters::id::playMode, 1.0f);
    ok &= expect(chipPolyProcessor->setMotionPattern(chipper::ChipMode::nes, pattern),
                 "Chip Poly processor should retain edited motion even while playback is bypassed");
    juce::AudioBuffer<float> chipPolyBuffer(2, 256);
    juce::MidiBuffer chipPolyMidi;
    chipPolyMidi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
    chipPolyProcessor->processBlock(chipPolyBuffer, chipPolyMidi);
    const auto bypassSnapshot = chipPolyProcessor->motionSnapshot(chipper::ChipMode::nes);
    ok &= expect(bufferPeak(chipPolyBuffer, 0, chipPolyBuffer.getNumSamples()) > 0.001f,
                 "Chip Poly should remain audible when a level-zero motion step is safely bypassed");
    ok &= expect(bypassSnapshot.bypassedForChipPoly && bypassSnapshot.activeStep == -1,
                 "Motion snapshot should explicitly report the Chip Poly bypass");

    return ok;
}

bool expectMappedSampleNoteProcessingDoesNotAllocate()
{
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("chipper-rt-sample-map", {}, false);
    if (! root.createDirectory())
        return expect(false, "Could not create real-time sample-map fixture directory");

    bool ok = true;
    const auto dmcDir = root.getChildFile("dmc");
    const auto brrDir = root.getChildFile("brr");
    const auto paulaDir = root.getChildFile("paula");
    dmcDir.createDirectory();
    brrDir.createDirectory();
    paulaDir.createDirectory();
    ok &= writeDmcFixture(dmcDir.getChildFile("one.dmc"), 0x55u);
    ok &= writeDmcFixture(dmcDir.getChildFile("two.dmc"), 0xaau);
    ok &= writeBrrFixture(brrDir.getChildFile("one.brr"), 0x11u);
    ok &= writeBrrFixture(brrDir.getChildFile("two.brr"), 0x44u);
    ok &= writeWavFixture(paulaDir.getChildFile("one.wav"), 220.0f);
    ok &= writeWavFixture(paulaDir.getChildFile("two.wav"), 330.0f);

    const auto checkMode = [&](int chipChoice, auto loadBank, const std::string& label)
    {
        ChipperAudioProcessor processor;
        processor.prepareToPlay(48000.0, 64);
        setPlainFromHost(processor, chipper::parameters::id::chipMode, static_cast<float>(chipChoice));
        processEmptyBlock(processor);
        ok &= loadBank(processor).wasOk();
        setPlainFromHost(processor, chipper::parameters::id::nesDmcPlaybackMode, 1.0f);
        setPlainFromHost(processor, chipper::parameters::id::nesDmcMapRoot, 36.0f);
        processEmptyBlock(processor);

        juce::MidiBuffer noteOn;
        noteOn.addEvent(juce::MidiMessage::noteOn(1, 37, 1.0f), 0);
        const auto allocations = processAllocationCount(processor, noteOn);
        ok &= expect(allocations == 0u,
                     label + " mapped note allocated " + std::to_string(allocations) + " times in processBlock");

        juce::MidiBuffer emptyMidi;
        setPlainFromHost(processor, chipper::parameters::id::accuracy, 2.0f);
        const auto strictnessAllocations = processAllocationCount(processor, emptyMidi);
        ok &= expect(strictnessAllocations == 0u,
                     label + " loaded-bank Strictness switch allocated " + std::to_string(strictnessAllocations)
                         + " times in processBlock");

        setPlainFromHost(processor, chipper::parameters::id::chipMode, 1.0f);
        processEmptyBlock(processor);
        setPlainFromHost(processor, chipper::parameters::id::chipMode, static_cast<float>(chipChoice));
        const auto returnAllocations = processAllocationCount(processor, emptyMidi);
        ok &= expect(returnAllocations == 0u,
                     label + " loaded-bank chip return allocated " + std::to_string(returnAllocations)
                         + " times in processBlock");

        if (chipChoice == 0)
        {
            processor.clearNesDmcSampleSelection();
            processor.invertNesDmcSampleSelection();
        }
        else if (chipChoice == 7)
        {
            processor.clearSpc700BrrSampleSelection();
            processor.invertSpc700BrrSampleSelection();
        }
        else
        {
            processor.clearPaulaSampleSelection();
            processor.invertPaulaSampleSelection();
        }

        const auto bankMutationAllocations = processAllocationCount(processor, emptyMidi);
        ok &= expect(bankMutationAllocations == 0u,
                     label + " sample-bank mutation left " + std::to_string(bankMutationAllocations)
                         + " allocations for processBlock");
    };

    checkMode(0, [&](ChipperAudioProcessor& processor) { return processor.loadNesDmcSampleDirectory(dmcDir); }, "NES DMC");
    checkMode(7, [&](ChipperAudioProcessor& processor) { return processor.loadSpc700BrrSampleDirectory(brrDir); }, "SPC700");
    checkMode(9, [&](ChipperAudioProcessor& processor) { return processor.loadPaulaSampleDirectory(paulaDir); }, "Paula");

    root.deleteRecursively();
    return ok;
}

bool expectConcurrentSampleMutationDoesNotDeadlock()
{
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("chipper-rt-sample-concurrency", {}, false);
    if (! root.createDirectory())
        return expect(false, "Could not create concurrent sample-mutation fixture directory");

    bool ok = writeDmcFixture(root.getChildFile("one.dmc"), 0x55u)
        && writeDmcFixture(root.getChildFile("two.dmc"), 0xaau);
    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);
    ok &= processor.loadNesDmcSampleDirectory(root).wasOk();

    std::atomic<bool> start { false };
    std::thread mutator([&]
    {
        while (! start.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (int iteration = 0; iteration < 200; ++iteration)
            processor.invertNesDmcSampleSelection();
    });

    start.store(true, std::memory_order_release);
    juce::MidiBuffer emptyMidi;
    for (int block = 0; block < 500; ++block)
    {
        juce::AudioBuffer<float> buffer(2, 64);
        const juce::ScopedLock callbackGuard(processor.getCallbackLock());
        processor.processBlock(buffer, emptyMidi);
    }
    mutator.join();

    root.deleteRecursively();
    return expect(ok, "Concurrent sample-bank mutation should complete without deadlock or load failure");
}

#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool expectOpnaAdpcmARegionBank(const juce::File& root)
{
    auto ok = true;
    const auto opnaKick = root.getChildFile("opna-kick.wav");
    const auto opnaTom = root.getChildFile("opna-tom.wav");
    const auto opnaWrong = root.getChildFile("opna-wrong.bin");
    ok &= expect(writeWavFixture(opnaKick, 110.0f), "Should write OPNA kick region fixture");
    const auto opnaPacked = root.getChildFile("opna-packed.bin");
    ok &= expect(writeWavFixture(opnaTom, 165.0f), "Should write OPNA tom region fixture");
    ok &= expect(writeBinaryFixture(opnaWrong, 447u), "Should write malformed OPNA region fixture");
    ok &= expect(writeBinaryFixture(opnaPacked, 8192u), "Should write legacy packed OPNA fixture");

    auto opnaStorage = std::make_unique<ChipperAudioProcessor>();
    auto& opna = *opnaStorage;
    opna.prepareToPlay(48000.0, 64);
    setPlainFromHost(opna, chipper::parameters::id::chipMode, 17.0f);
    setPlainFromHost(opna, chipper::parameters::id::macro, 5.0f);
    processEmptyBlock(opna);
    ok &= expect(opna.loadOpnaAdpcmARegionFile(0, opnaKick).wasOk(),
                 "OPNA should convert WAV into the fixed bass-drum region");
    ok &= expect(opna.loadOpnaAdpcmARegionFile(4, opnaTom).wasOk(),
                 "OPNA should convert WAV into the slower fixed tom region");

    const auto kickInfo = opna.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
    const auto tomInfo = opna.adpcmARegionInfo(chipper::ChipMode::ym2608, 4);
    ok &= expect(kickInfo.loaded && kickInfo.convertedFromPcm
                     && kickInfo.encodedByteCount == 448 && kickInfo.decodedSampleCount == 99
                     && kickInfo.startByte == 0x0000 && kickInfo.endByteInclusive == 0x01bf
                     && kickInfo.loadedRegionCount == 2 && kickInfo.editableBankActive,
                 "OPNA bass drum should use its exact 448-byte ROM window and 18.489 kHz conversion");
    ok &= expect(std::abs(kickInfo.sampleRateHz - (7987200.0 / 432.0)) < 0.001,
                 "OPNA bass drum should report the /432 hardware playback rate");
    ok &= expect(tomInfo.loaded && tomInfo.convertedFromPcm
                     && tomInfo.encodedByteCount == 640 && tomInfo.decodedSampleCount == 49
                     && tomInfo.startByte == 0x1d00 && tomInfo.endByteInclusive == 0x1f7f,
                 "OPNA tom should use its exact 640-byte ROM window and slower conversion");
    ok &= expect(std::abs(tomInfo.sampleRateHz - (7987200.0 / 864.0)) < 0.001,
                 "OPNA tom should report the /864 hardware playback rate");
    const auto kickPreview = opna.adpcmARegionWaveformSnapshot(chipper::ChipMode::ym2608, 0);
    ok &= expect(kickPreview.loaded && kickPreview.sourceSampleCount == 99
                     && kickPreview.label.contains("Decoded Bass drum"),
                 "OPNA region preview should decode the heard ADPCM-A bytes and trim padding");

    const auto kickName = kickInfo.sampleName;
    ok &= expect(opna.loadOpnaAdpcmARegionFile(0, opnaWrong).failed(),
                 "OPNA raw region import should reject a payload that does not exactly fill its ROM window");
    const auto kickAfterFailure = opna.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
    ok &= expect(kickAfterFailure.loaded && kickAfterFailure.sampleName == kickName
                     && kickAfterFailure.encodedByteCount == 448,
                 "Failed OPNA region replacement should leave the prior region untouched");
    sendNoteOn(opna, 60);
    const auto opnaDebug = opna.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(opnaDebug, "opnaAdpcmAUserRomLoaded") == 1
                     && jsonIntValue(opnaDebug, "opnaAdpcmARomProvidedBytes") == 8192
                     && jsonIntValue(opnaDebug, "opnaAdpcmARomCopiedBytes") == 8192,
                 "Two OPNA overrides should compile over the generated canonical 8 KiB rhythm ROM");

    auto opnaReference = opna.createStateXml();
    auto opnaProject = opna.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
    ok &= expect(opnaReference != nullptr
                     && countElementsNamed(*opnaReference, "CHIPPER_ADPCM_A_REGION") == 2u
                     && countElementsNamed(*opnaReference, chipper::state::embeddedSampleStateTag) == 0u,
                 "OPNA region preset state should store two indexed references without embedded bytes");
    ok &= expect(opnaProject != nullptr
                     && countElementsNamed(*opnaProject, "CHIPPER_ADPCM_A_REGION") == 2u
                     && countElementsNamed(*opnaProject, chipper::state::embeddedSampleStateTag) == 2u,
                 "OPNA project state should embed each canonical encoded override independently");

    if (opnaReference != nullptr)
    {
        auto duplicate = std::make_unique<juce::XmlElement>(*opnaReference);
        if (auto* bank = duplicate->getChildByName("CHIPPER_OPNA_RHYTHM_ROM"))
            if (auto* first = bank->getChildByName("CHIPPER_ADPCM_A_REGION"))
                bank->addChildElement(new juce::XmlElement(*first));
        const auto beforeRejectedRegister = jsonIntValue(opna.currentCoreDebugStateJson(),
                                                         "algorithmFeedbackRegister0");
        const auto rejectedRegisterValue = beforeRejectedRegister == 0 ? 0xff : 0;
        if (auto* coreState = duplicate->getChildByName("CHIPPER_CORE_REGISTERS"))
        {
            auto* rejectedRegister = new juce::XmlElement("REG");
            rejectedRegister->setAttribute("address", 0xb0);
            rejectedRegister->setAttribute("value", rejectedRegisterValue);
            coreState->addChildElement(rejectedRegister);
        }
        auto duplicateRestoreStorage = std::make_unique<ChipperAudioProcessor>();
        auto& duplicateRestore = *duplicateRestoreStorage;
        duplicateRestore.prepareToPlay(48000.0, 64);
        const auto beforeRejectedRestore = opna.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        ok &= expect(duplicateRestore.restoreStateXml(*duplicate).failed()
                         && opna.restoreStateXml(*duplicate).failed(),
                     "Schema 8 should reject duplicate OPNA region indices");
        setPlainFromHost(opna, chipper::parameters::id::chipMode, 0.0f);
        processEmptyBlock(opna);
        setPlainFromHost(opna, chipper::parameters::id::chipMode, 17.0f);
        processEmptyBlock(opna);
        const auto afterRejectedRestore = opna.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        ok &= expect(afterRejectedRestore.loaded
                         && afterRejectedRestore.sampleName == beforeRejectedRestore.sampleName
                         && jsonIntValue(opna.currentCoreDebugStateJson(), "algorithmFeedbackRegister0")
                                == beforeRejectedRegister,
                     "Rejected schema-8 restore should preserve both the region bank and pending register state");

        auto outOfRange = std::make_unique<juce::XmlElement>(*opnaReference);
        if (auto* bank = outOfRange->getChildByName("CHIPPER_OPNA_RHYTHM_ROM"))
            if (auto* first = bank->getChildByName("CHIPPER_ADPCM_A_REGION"))
                first->setAttribute("index", 6);
        auto outOfRangeRestoreStorage = std::make_unique<ChipperAudioProcessor>();
        auto& outOfRangeRestore = *outOfRangeRestoreStorage;
        outOfRangeRestore.prepareToPlay(48000.0, 64);
        ok &= expect(outOfRangeRestore.restoreStateXml(*outOfRange).failed(),
                     "Schema 8 should reject out-of-range OPNA region indices");
    }

    std::unique_ptr<juce::XmlElement> opnaPackedReference;
    {
        auto legacyStorage = std::make_unique<ChipperAudioProcessor>();
        auto& legacy = *legacyStorage;
        legacy.prepareToPlay(48000.0, 64);
        ok &= expect(legacy.loadOpnaRhythmRomFile(opnaPacked).wasOk(),
                     "Legacy packed OPNA import should remain supported");
        opnaPackedReference = legacy.createStateXml();
        const auto packedInfo = legacy.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        const auto packedName = legacy.opnaRhythmRomInfo().sampleName;
        ok &= expect(packedInfo.legacyBankActive && ! packedInfo.editableBankActive
                         && packedInfo.statusLine.containsIgnoreCase("confirmation"),
                     "OPNA region metadata should disclose the active packed bank and required confirmation");
        ok &= expect(legacy.loadOpnaAdpcmARegionFile(0, opnaKick).failed()
                         && legacy.opnaRhythmRomInfo().loaded
                         && legacy.opnaRhythmRomInfo().sampleName == packedName,
                     "Unconfirmed OPNA region import must preserve the active packed bank");
        ok &= expect(legacy.loadOpnaAdpcmARegionFile(0, opnaKick, true).wasOk()
                         && ! legacy.adpcmARegionInfo(chipper::ChipMode::ym2608, 0).legacyBankActive,
                     "Confirmed OPNA region import should atomically replace the packed bank after conversion succeeds");
    }

    opnaPacked.deleteFile();
    if (opnaPackedReference != nullptr)
    {
        auto missingLegacyStorage = std::make_unique<ChipperAudioProcessor>();
        auto& missingLegacy = *missingLegacyStorage;
        missingLegacy.prepareToPlay(48000.0, 64);
        ok &= expect(missingLegacy.restoreStateXml(*opnaPackedReference).wasOk(),
                     "A missing legacy OPNA packed reference should restore as a tombstone");
        const auto missingPackedInfo = missingLegacy.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        ok &= expect(! missingPackedInfo.legacyBankActive && missingPackedInfo.legacyBankMissing
                         && missingLegacy.loadOpnaAdpcmARegionFile(0, opnaKick).wasOk(),
                     "Missing OPNA packed references must not masquerade as active banks or require replacement confirmation");
    }

    {
        auto inactiveStorage = std::make_unique<ChipperAudioProcessor>();
        auto& inactive = *inactiveStorage;
        inactive.prepareToPlay(48000.0, 64);
        ok &= expect(inactive.loadOpnaAdpcmARegionFile(0, opnaKick).wasOk(),
                     "OPNA region should load while another chip is active");
        setPlainFromHost(inactive, chipper::parameters::id::chipMode, 17.0f);
        juce::MidiBuffer emptyMidi;
        const auto allocations = processAllocationCount(inactive, emptyMidi);
        ok &= expect(allocations == 0u,
                     "Switching to an inactive OPNA core with loaded regions must not allocate or compile in processBlock");
        inactive.clearOpnaAdpcmARegion(0);
        const auto resetInfo = inactive.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        ok &= expect(! resetInfo.loaded && ! resetInfo.editableBankActive
                         && resetInfo.statusLine.containsIgnoreCase("generated"),
                     "Clearing the final OPNA override should restore the generated region bank");
    }

    opnaKick.deleteFile();
    opnaTom.deleteFile();
    if (opnaReference != nullptr)
    {
        auto missingStorage = std::make_unique<ChipperAudioProcessor>();
        auto& missing = *missingStorage;
        missing.prepareToPlay(48000.0, 64);
        ok &= expect(missing.restoreStateXml(*opnaReference).wasOk(),
                     "Missing OPNA region references should restore as indexed tombstones");
        const auto missingKick = missing.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        const auto missingTom = missing.adpcmARegionInfo(chipper::ChipMode::ym2608, 4);
        ok &= expect(! missingKick.loaded && ! missingTom.loaded
                         && missingKick.sampleName == "opna-kick.wav"
                         && missingTom.sampleName == "opna-tom.wav"
                         && missingKick.editableBankActive
                         && missingKick.statusLine.containsIgnoreCase("does not exist"),
                     "Reference-only OPNA restore should preserve region identity and expose missing files");
    }
    if (opnaProject != nullptr)
    {
        auto embeddedStorage = std::make_unique<ChipperAudioProcessor>();
        auto& embedded = *embeddedStorage;
        embedded.prepareToPlay(48000.0, 64);
        ok &= expect(embedded.restoreStateXml(*opnaProject).wasOk(),
                     "Embedded OPNA region project state should restore after source deletion");
        const auto embeddedKick = embedded.adpcmARegionInfo(chipper::ChipMode::ym2608, 0);
        const auto embeddedTom = embedded.adpcmARegionInfo(chipper::ChipMode::ym2608, 4);
        ok &= expect(embeddedKick.loaded && embeddedTom.loaded
                         && embeddedKick.encodedByteCount == 448
                         && embeddedTom.encodedByteCount == 640
                         && embeddedKick.statusLine.contains("Using embedded project copy"),
                     "Embedded OPNA restoration should preserve canonical bytes, roles, and conversion metadata");
    }

    return ok;
}

#if defined(_MSC_VER)
__declspec(noinline)
#endif
bool expectOpnbAdpcmARegionBank(const juce::File& root)
{
    auto ok = true;
    const auto opnbOne = root.getChildFile("opnb-one.bin");
    const auto opnbThree = root.getChildFile("opnb-three.bin");
    const auto opnbSix = root.getChildFile("opnb-six.bin");
    const auto opnbWrong = root.getChildFile("opnb-wrong.bin");
    ok &= expect(writeBinaryFixture(opnbOne, 256u), "Should write OPNB region 1 fixture");
    const auto opnbPacked = root.getChildFile("opnb-packed.bin");
    ok &= expect(writeBinaryFixture(opnbThree, 512u), "Should write OPNB region 3 fixture");
    ok &= expect(writeBinaryFixture(opnbSix, 256u), "Should write OPNB region 6 fixture");
    ok &= expect(writeBinaryFixture(opnbWrong, 255u), "Should write malformed OPNB page fixture");
    ok &= expect(writeBinaryFixture(opnbPacked, 1536u), "Should write legacy packed OPNB fixture");

    auto opnbStorage = std::make_unique<ChipperAudioProcessor>();
    auto& opnb = *opnbStorage;
    opnb.prepareToPlay(48000.0, 64);
    setPlainFromHost(opnb, chipper::parameters::id::chipMode, 18.0f);
    setPlainFromHost(opnb, chipper::parameters::id::macro, 5.0f);
    processEmptyBlock(opnb);
    ok &= expect(opnb.loadOpnbAdpcmARegionFile(0, opnbOne).wasOk()
                     && opnb.loadOpnbAdpcmARegionFile(2, opnbThree).wasOk()
                     && opnb.loadOpnbAdpcmARegionFile(5, opnbSix).wasOk(),
                 "OPNB should load sparse page-aligned logical regions");
    const auto opnbOneInfo = opnb.adpcmARegionInfo(chipper::ChipMode::ym2610, 0);
    ok &= expect(opnb.loadOpnbAdpcmARegionFile(0, opnbWrong).failed(),
                 "OPNB raw region import should reject incomplete 256-byte pages");
    ok &= expect(opnb.adpcmARegionInfo(chipper::ChipMode::ym2610, 0).sampleName == opnbOneInfo.sampleName,
                 "Failed OPNB region replacement should leave the prior region untouched");
    sendNoteOn(opnb, 60);
    const auto opnbDebug = opnb.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(opnbDebug, "opnbAdpcmARegionMode") == 1
                     && jsonIntValue(opnbDebug, "opnbAdpcmAActiveMask") == 0x25
                     && jsonIntValue(opnbDebug, "opnbAdpcmAKeyBits") == 0x25
                     && jsonIntValue(opnbDebug, "opnbAdpcmAProvidedBytes") == 1024
                     && jsonIntValue(opnbDebug, "opnbAdpcmAStartRegister0") == 0
                     && jsonIntValue(opnbDebug, "opnbAdpcmAEndRegister0") == 0
                     && jsonIntValue(opnbDebug, "opnbAdpcmAStartRegister2") == 1
                     && jsonIntValue(opnbDebug, "opnbAdpcmAEndRegister2") == 2
                     && jsonIntValue(opnbDebug, "opnbAdpcmAStartRegister5") == 3
                     && jsonIntValue(opnbDebug, "opnbAdpcmAEndRegister5") == 3,
                 "OPNB sparse regions should pack consecutively while preserving logical channel identity");

    auto opnbReference = opnb.createStateXml();
    auto opnbProject = opnb.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
    ok &= expect(opnbReference != nullptr
                     && countElementsNamed(*opnbReference, "CHIPPER_ADPCM_A_REGION") == 3u
                     && countElementsNamed(*opnbReference, chipper::state::embeddedSampleStateTag) == 0u,
                 "OPNB region preset state should store three indexed references");
    ok &= expect(opnbProject != nullptr
                     && countElementsNamed(*opnbProject, "CHIPPER_ADPCM_A_REGION") == 3u
                     && countElementsNamed(*opnbProject, chipper::state::embeddedSampleStateTag) == 3u,
                 "OPNB project state should embed three independently encoded logical regions");

    setPlainFromHost(opnb, chipper::parameters::id::chipMode, 26.0f);
    processEmptyBlock(opnb);
    sendNoteOn(opnb, 60);
    const auto opnb2Debug = opnb.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(opnb2Debug, "opnbAdpcmARegionMode") == 1
                     && jsonIntValue(opnb2Debug, "opnbAdpcmAActiveMask") == 0x25
                     && jsonIntValue(opnb2Debug, "opnbAdpcmAStartRegister2") == 1
                     && jsonIntValue(opnb2Debug, "opnbAdpcmAEndRegister5") == 3,
                 "OPNB and OPNB2 should share identical six-region ADPCM-A memory semantics");

    std::unique_ptr<juce::XmlElement> opnbPackedReference;
    {
        auto legacyStorage = std::make_unique<ChipperAudioProcessor>();
        auto& legacy = *legacyStorage;
        legacy.prepareToPlay(48000.0, 64);
        ok &= expect(legacy.loadOpnbAdpcmASampleFile(opnbPacked).wasOk(),
                     "Legacy packed OPNB import should remain supported");
        opnbPackedReference = legacy.createStateXml();
        const auto packedInfo = legacy.adpcmARegionInfo(chipper::ChipMode::ym2610, 0);
        const auto packedName = legacy.opnbAdpcmASampleInfo().sampleName;
        ok &= expect(packedInfo.legacyBankActive && ! packedInfo.editableBankActive
                         && packedInfo.statusLine.containsIgnoreCase("confirmation"),
                     "OPNB region metadata should disclose the active packed bank and required confirmation");
        ok &= expect(legacy.loadOpnbAdpcmARegionFile(0, opnbOne).failed()
                         && legacy.opnbAdpcmASampleInfo().loaded
                         && legacy.opnbAdpcmASampleInfo().sampleName == packedName,
                     "Unconfirmed OPNB region import must preserve the active packed bank");
        ok &= expect(legacy.loadOpnbAdpcmARegionFile(0, opnbOne, true).wasOk()
                         && ! legacy.adpcmARegionInfo(chipper::ChipMode::ym2610, 0).legacyBankActive,
                     "Confirmed OPNB region import should atomically replace the packed bank after validation succeeds");
    }

    opnbPacked.deleteFile();
    if (opnbPackedReference != nullptr)
    {
        auto missingLegacyStorage = std::make_unique<ChipperAudioProcessor>();
        auto& missingLegacy = *missingLegacyStorage;
        missingLegacy.prepareToPlay(48000.0, 64);
        ok &= expect(missingLegacy.restoreStateXml(*opnbPackedReference).wasOk(),
                     "A missing legacy OPNB packed reference should restore as a tombstone");
        const auto missingPackedInfo = missingLegacy.adpcmARegionInfo(chipper::ChipMode::ym2610, 0);
        ok &= expect(! missingPackedInfo.legacyBankActive && missingPackedInfo.legacyBankMissing
                         && missingLegacy.loadOpnbAdpcmARegionFile(0, opnbOne).wasOk(),
                     "Missing OPNB packed references must not masquerade as active banks or require replacement confirmation");
    }

    {
        auto inactiveStorage = std::make_unique<ChipperAudioProcessor>();
        auto& inactive = *inactiveStorage;
        inactive.prepareToPlay(48000.0, 64);
        ok &= expect(inactive.loadOpnbAdpcmARegionFile(5, opnbSix).wasOk(),
                     "OPNB region should load while another chip is active");
        setPlainFromHost(inactive, chipper::parameters::id::chipMode, 18.0f);
        juce::MidiBuffer emptyMidi;
        const auto opnbAllocations = processAllocationCount(inactive, emptyMidi);
        ok &= expect(opnbAllocations == 0u,
                     "Switching to an inactive OPNB core with loaded regions must not allocate or pack in processBlock");
        setPlainFromHost(inactive, chipper::parameters::id::chipMode, 26.0f);
        const auto opnb2Allocations = processAllocationCount(inactive, emptyMidi);
        ok &= expect(opnb2Allocations == 0u,
                     "Switching to OPNB2 with loaded regions must reuse its prepared bank without audio-thread allocation");
        inactive.clearOpnbAdpcmARegion(5);
        const auto clearedInfo = inactive.adpcmARegionInfo(chipper::ChipMode::ym2610, 5);
        ok &= expect(! clearedInfo.loaded && ! clearedInfo.editableBankActive
                         && clearedInfo.statusLine.containsIgnoreCase("empty"),
                     "Clearing the final OPNB region should return the logical bank to empty");
    }

    {
        auto concurrentStorage = std::make_unique<ChipperAudioProcessor>();
        auto& concurrent = *concurrentStorage;
        concurrent.prepareToPlay(48000.0, 64);
        std::atomic<bool> start { false };
        std::atomic<bool> firstLoaded { false };
        std::atomic<bool> secondLoaded { false };
        std::thread first([&]
        {
            while (! start.load(std::memory_order_acquire))
                std::this_thread::yield();
            firstLoaded.store(concurrent.loadOpnbAdpcmARegionFile(0, opnbOne).wasOk(), std::memory_order_release);
        });
        std::thread second([&]
        {
            while (! start.load(std::memory_order_acquire))
                std::this_thread::yield();
            secondLoaded.store(concurrent.loadOpnbAdpcmARegionFile(2, opnbThree).wasOk(), std::memory_order_release);
        });
        start.store(true, std::memory_order_release);
        first.join();
        second.join();
        ok &= expect(firstLoaded.load(std::memory_order_acquire)
                         && secondLoaded.load(std::memory_order_acquire)
                         && concurrent.adpcmARegionInfo(chipper::ChipMode::ym2610, 0).loaded
                         && concurrent.adpcmARegionInfo(chipper::ChipMode::ym2610, 2).loaded
                         && concurrent.adpcmARegionInfo(chipper::ChipMode::ym2610, 0).loadedRegionCount == 2,
                     "Concurrent OPNB region imports must serialize without losing either logical update");
    }

    opnbThree.deleteFile();
    if (opnbReference != nullptr)
    {
        auto sparseMissingStorage = std::make_unique<ChipperAudioProcessor>();
        auto& sparseMissing = *sparseMissingStorage;
        sparseMissing.prepareToPlay(48000.0, 64);
        ok &= expect(sparseMissing.restoreStateXml(*opnbReference).wasOk(),
                     "A missing middle OPNB region should not shift later logical region identities");
        processEmptyBlock(sparseMissing);
        sendNoteOn(sparseMissing, 60);
        const auto missingThree = sparseMissing.adpcmARegionInfo(chipper::ChipMode::ym2610, 2);
        const auto survivingSix = sparseMissing.adpcmARegionInfo(chipper::ChipMode::ym2610, 5);
        const auto sparseDebug = sparseMissing.currentCoreDebugStateJson();
        ok &= expect(! missingThree.loaded && missingThree.sampleName == "opnb-three.bin"
                         && survivingSix.loaded && survivingSix.sampleName == "opnb-six.bin"
                         && survivingSix.loadedRegionCount == 2
                         && jsonIntValue(sparseDebug, "opnbAdpcmAActiveMask") == 0x21
                         && jsonIntValue(sparseDebug, "opnbAdpcmAStartRegister5") == 1,
                     "Missing OPNB region 3 should remain a tombstone while region 6 repacks without renumbering");
    }

    opnbOne.deleteFile();
    opnbSix.deleteFile();
    if (opnbProject != nullptr)
    {
        auto embeddedStorage = std::make_unique<ChipperAudioProcessor>();
        auto& embedded = *embeddedStorage;
        embedded.prepareToPlay(48000.0, 64);
        ok &= expect(embedded.restoreStateXml(*opnbProject).wasOk(),
                     "Embedded OPNB region project state should restore after all source files are deleted");
        processEmptyBlock(embedded);
        sendNoteOn(embedded, 60);
        const auto debug = embedded.currentCoreDebugStateJson();
        ok &= expect(embedded.adpcmARegionInfo(chipper::ChipMode::ym2610, 0).loaded
                         && embedded.adpcmARegionInfo(chipper::ChipMode::ym2610, 2).loaded
                         && embedded.adpcmARegionInfo(chipper::ChipMode::ym2610, 5).loaded
                         && jsonIntValue(debug, "opnbAdpcmAActiveMask") == 0x25
                         && jsonIntValue(debug, "opnbAdpcmAProvidedBytes") == 1024,
                     "Embedded OPNB restoration should preserve all sparse logical regions and packed windows");
    }

    return ok;
}

bool expectYamahaAdpcmARegionBanks()
{
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("chipper-adpcm-a-regions", {}, false);
    if (! root.createDirectory())
        return expect(false, "Could not create Yamaha ADPCM-A region fixture directory");

    auto ok = expectOpnaAdpcmARegionBank(root);
    ok &= expectOpnbAdpcmARegionBank(root);
    root.deleteRecursively();
    return ok;
}
}

int runSmoke()
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
    sendController(processor, 24, 127);
    sendController(processor, 25, 0);
    sendController(processor, 28, 127);
    sendController(processor, 29, 0);
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
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1SustainRate), 32.0f, 0.0001f,
                     "CC24 should control FM Operator 1 Sustain Rate");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2SustainRate), 0.0f, 0.0001f,
                     "CC25 should control FM Operator 2 Sustain Rate");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1ReleaseRate), 16.0f, 0.0001f,
                     "CC28 should control FM Operator 1 Release Rate");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2ReleaseRate), 0.0f, 0.0001f,
                     "CC29 should control FM Operator 2 Release Rate");
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
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1SustainRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 1 Sustain Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2SustainRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 2 Sustain Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator3SustainRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 3 Sustain Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator4SustainRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 4 Sustain Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator1ReleaseRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 1 Release Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator2ReleaseRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 2 Release Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator3ReleaseRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 3 Release Rate to Follow");
    ok &= expectNear(parameterValue(processor, chipper::parameters::id::fmOperator4ReleaseRate), 0.0f, 0.0001f,
                     "CC74 macro change should reset FM Operator 4 Release Rate to Follow");

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
    ok &= expectOpmDirectLfoAndFeedbackPitchNeutrality();
    {
        ChipperAudioProcessor oplRouteProcessor;
        oplRouteProcessor.prepareToPlay(48000.0, 256);
        sendController(oplRouteProcessor, 70, controllerValueForChoice(oplRouteProcessor, chipper::parameters::id::chipMode, 6));
        setPlainFromHost(oplRouteProcessor, chipper::parameters::id::macroControl1, 1.0f);
        setPlainFromHost(oplRouteProcessor, chipper::parameters::id::macroControl2, 1.0f);
        setPlainFromHost(oplRouteProcessor, chipper::parameters::id::ymEnvelopeShape, 3.0f);
        sendNoteOn(oplRouteProcessor, 60);
        sendController(oplRouteProcessor, 94, controllerValueForChoice(oplRouteProcessor, chipper::parameters::id::dmgStereoRoute, 4));
        const auto oplRouteDebug = oplRouteProcessor.currentCoreDebugStateJson();
        ok &= expectNear(parameterValue(oplRouteProcessor, chipper::parameters::id::dmgStereoRoute), 4.0f, 0.001f,
                         "CC94 should remain visible as the OPL3 Alt stereo-route choice");
        ok &= expect(jsonIntValue(oplRouteDebug, "opl3StereoRouteChoice") == 4
                         && jsonIntValue(oplRouteDebug, "opl3OutputSelect0") == 0x50
                         && jsonIntValue(oplRouteDebug, "opl3OutputSelect1") == 0xa0
                         && jsonIntValue(oplRouteDebug, "opl3HighBankOutputSelect0") == 0xa0
                         && jsonIntValue(oplRouteDebug, "connectionRegister0") == 0x5f
                         && jsonIntValue(oplRouteDebug, "connectionRegister3") == 0xaf
                         && jsonIntValue(oplRouteDebug, "highBankConnectionRegister0") == 0xaf,
                     "CC94 should update held OPL3 low/high-bank channels to exact alternating $50/$A0 output bits without losing feedback or connection");

        setPlainFromHost(oplRouteProcessor, chipper::parameters::id::ymEnvelopeShape, 2.0f);
        sendController(oplRouteProcessor, 94, controllerValueForChoice(oplRouteProcessor, chipper::parameters::id::dmgStereoRoute, 1));
        sendNoteOn(oplRouteProcessor, 64);
        const auto rhythmBeforeRoute = oplRouteProcessor.currentCoreDebugStateJson();
        sendController(oplRouteProcessor, 94, controllerValueForChoice(oplRouteProcessor, chipper::parameters::id::dmgStereoRoute, 4));
        const auto rhythmAfterRoute = oplRouteProcessor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(rhythmBeforeRoute, "connectionRegister6") == 0xff
                         && jsonIntValue(rhythmBeforeRoute, "connectionRegister7") == 0xff
                         && jsonIntValue(rhythmBeforeRoute, "connectionRegister8") == 0xff
                         && jsonIntValue(rhythmBeforeRoute, "rhythmKeyBits") != 0,
                     "Triggered OPL3 percussion should begin on the Both route with nonzero feedback and connection bits");
        ok &= expect(jsonIntValue(rhythmAfterRoute, "connectionRegister6") == 0x5f
                         && jsonIntValue(rhythmAfterRoute, "connectionRegister7") == 0xaf
                         && jsonIntValue(rhythmAfterRoute, "connectionRegister8") == 0x5f
                         && jsonIntValue(rhythmAfterRoute, "rhythmKeyBits") == jsonIntValue(rhythmBeforeRoute, "rhythmKeyBits"),
                     "CC94 should reroute active OPL3 percussion immediately while preserving each $C0 low nibble and rhythm key state");
    }
    {
        ChipperAudioProcessor oplNativeProcessor;
        oplNativeProcessor.prepareToPlay(48000.0, 256);
        sendController(oplNativeProcessor, 70, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::chipMode, 6));
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::ymEnvelopeShape, 4.0f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::macroControl1, 0.67f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::macroControl2, 0.35f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::macroControl3, 0.35f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::macroControl4, 0.9f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator1Level, 0.65f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator2Level, 0.45f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator3Level, 0.75f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator4Level, 0.55f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator1Multiplier, 3.0f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator2Multiplier, 4.0f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator3Multiplier, 5.0f);
        setPlainFromHost(oplNativeProcessor, chipper::parameters::id::fmOperator4Multiplier, 6.0f);
        sendNoteOn(oplNativeProcessor, 60);
        const auto beforeNativeFields = oplNativeProcessor.currentCoreDebugStateJson();

        sendController(oplNativeProcessor, 12, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator1Dt1, 8));
        sendController(oplNativeProcessor, 13, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator2Dt1, 2));
        sendController(oplNativeProcessor, 14, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator3Dt1, 3));
        sendController(oplNativeProcessor, 15, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator4Dt1, 5));
        sendController(oplNativeProcessor, 16, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator1Dt2, 2));
        sendController(oplNativeProcessor, 17, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator2Dt2, 3));
        sendController(oplNativeProcessor, 18, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator3Dt2, 4));
        sendController(oplNativeProcessor, 19, controllerValueForChoice(oplNativeProcessor, chipper::parameters::id::opmOperator4Dt2, 1));
        const auto nativeFields = oplNativeProcessor.currentCoreDebugStateJson();

        ok &= expect(jsonIntValue(nativeFields, "operatorControl0") == 0xf2
                         && jsonIntValue(nativeFields, "operatorControl1") == 0xa3
                         && jsonIntValue(nativeFields, "operatorControl2") == 0x64
                         && jsonIntValue(nativeFields, "operatorControl3") == 0x35,
                     "CC12-15 should update a held OPL3 note with exact AM/VIB/KSR + EGT + MULT $20 bytes");
        ok &= expect(jsonIntValue(nativeFields, "operatorTotalLevelRegister0") == 0xa6
                         && jsonIntValue(nativeFields, "operatorTotalLevelRegister1") == 0x43
                         && jsonIntValue(nativeFields, "operatorTotalLevelRegister2") == 0xe4
                         && jsonIntValue(nativeFields, "operatorTotalLevelRegister3") == 0x01,
                     "CC16-19 should update a held OPL3 note with exact KSL + TL $40 bytes");
        ok &= expect(jsonIntValue(nativeFields, "operatorFlagChoice0") == 8
                         && jsonIntValue(nativeFields, "operatorFlagChoice1") == 2
                         && jsonIntValue(nativeFields, "operatorFlagChoice2") == 3
                         && jsonIntValue(nativeFields, "operatorFlagChoice3") == 5
                         && jsonIntValue(nativeFields, "operatorKslChoice0") == 2
                         && jsonIntValue(nativeFields, "operatorKslChoice1") == 3
                         && jsonIntValue(nativeFields, "operatorKslChoice2") == 4
                         && jsonIntValue(nativeFields, "operatorKslChoice3") == 1,
                     "OPL3 debug state should disclose all raw native-field parameter choices");
        ok &= expect(jsonIntValue(nativeFields, "fnum0") == jsonIntValue(beforeNativeFields, "fnum0")
                         && jsonIntValue(nativeFields, "block0") == jsonIntValue(beforeNativeFields, "block0")
                         && jsonIntValue(nativeFields, "connectionRegister0") == jsonIntValue(beforeNativeFields, "connectionRegister0")
                         && jsonIntValue(nativeFields, "connectionRegister3") == jsonIntValue(beforeNativeFields, "connectionRegister3")
                         && jsonIntValue(nativeFields, "opl3FourOperatorRegister") == jsonIntValue(beforeNativeFields, "opl3FourOperatorRegister"),
                     "Changing held OPL3 operator fields should preserve pitch, topology, feedback, connection, and routing registers");
        ok &= expectNear(parameterValue(oplNativeProcessor, chipper::parameters::id::opmOperator1Dt1), 8.0f, 0.001f,
                         "CC12 should remain visible as the OPL3 OP1 flag choice");
        ok &= expectNear(parameterValue(oplNativeProcessor, chipper::parameters::id::opmOperator4Dt2), 1.0f, 0.001f,
                         "CC19 should remain visible as the OPL3 OP4 KSL choice");
    }
    {
        ChipperAudioProcessor oplRhythmNativeProcessor;
        oplRhythmNativeProcessor.prepareToPlay(48000.0, 256);
        sendController(oplRhythmNativeProcessor, 70, controllerValueForChoice(oplRhythmNativeProcessor, chipper::parameters::id::chipMode, 6));
        setPlainFromHost(oplRhythmNativeProcessor, chipper::parameters::id::ymEnvelopeShape, 2.0f);
        setPlainFromHost(oplRhythmNativeProcessor, chipper::parameters::id::fmOperator1Multiplier, 3.0f);
        setPlainFromHost(oplRhythmNativeProcessor, chipper::parameters::id::fmOperator2Multiplier, 5.0f);
        sendNoteOn(oplRhythmNativeProcessor, 60);
        const auto rhythmNativeBefore = oplRhythmNativeProcessor.currentCoreDebugStateJson();

        sendController(oplRhythmNativeProcessor, 12, controllerValueForChoice(oplRhythmNativeProcessor, chipper::parameters::id::opmOperator1Dt1, 8));
        sendController(oplRhythmNativeProcessor, 13, controllerValueForChoice(oplRhythmNativeProcessor, chipper::parameters::id::opmOperator2Dt1, 3));
        sendController(oplRhythmNativeProcessor, 16, controllerValueForChoice(oplRhythmNativeProcessor, chipper::parameters::id::opmOperator1Dt2, 2));
        sendController(oplRhythmNativeProcessor, 17, controllerValueForChoice(oplRhythmNativeProcessor, chipper::parameters::id::opmOperator2Dt2, 4));
        processEmptyBlock(oplRhythmNativeProcessor);
        const auto rhythmNativeAfter = oplRhythmNativeProcessor.currentCoreDebugStateJson();

        ok &= expect(jsonIntValue(rhythmNativeBefore, "rhythmKeyBits") != 0
                         && jsonIntValue(rhythmNativeAfter, "rhythmKeyBits") == jsonIntValue(rhythmNativeBefore, "rhythmKeyBits")
                         && jsonIntValue(rhythmNativeAfter, "rhythmRegister") == jsonIntValue(rhythmNativeBefore, "rhythmRegister"),
                     "CC12-17 should preserve active OPL3 percussion key bits and the $BD rhythm register");
        ok &= expect(jsonIntValue(rhythmNativeAfter, "rhythmOperatorFlagBits0") == 0xd0
                         && jsonIntValue(rhythmNativeAfter, "rhythmOperatorFlagBits1") == 0x40
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl2") & 0xd0) == 0xd0
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl3") & 0xd0) == 0x40
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl4") & 0xd0) == 0xd0
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl5") & 0xd0) == 0x40,
                     "CC12-13 should immediately update all active OPL3 percussion operators with exact AM/VIB/KSR bits");
        ok &= expect((jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl0") & 0x2f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorControl0") & 0x2f)
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl1") & 0x2f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorControl1") & 0x2f)
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl2") & 0x2f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorControl2") & 0x2f)
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl3") & 0x2f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorControl3") & 0x2f)
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl4") & 0x2f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorControl4") & 0x2f)
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorControl5") & 0x2f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorControl5") & 0x2f),
                     "Live OPL3 percussion flag edits should preserve each operator's EGT and MULT fields");
        ok &= expect((jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister0") & 0xc0) == 0x80
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister1") & 0xc0) == 0xc0
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister2") & 0xc0) == 0x80
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister3") & 0xc0) == 0xc0
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister4") & 0xc0) == 0x80
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister5") & 0xc0) == 0xc0,
                     std::string("CC16-17 should immediately update all active OPL3 percussion operators with exact KSL bits actual=")
                         + std::to_string(jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister0")) + ","
                         + std::to_string(jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister1")) + ","
                         + std::to_string(jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister2")) + ","
                         + std::to_string(jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister3")) + ","
                         + std::to_string(jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister4")) + ","
                         + std::to_string(jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister5")));
        ok &= expect((jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister0") & 0x3f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorTotalLevelRegister0") & 0x3f)
                         && (jsonIntValue(rhythmNativeAfter, "rhythmOperatorTotalLevelRegister1") & 0x3f) == (jsonIntValue(rhythmNativeBefore, "rhythmOperatorTotalLevelRegister1") & 0x3f)
                         && jsonIntValue(rhythmNativeAfter, "rhythmBdLevel") == jsonIntValue(rhythmNativeBefore, "rhythmBdLevel")
                         && jsonIntValue(rhythmNativeAfter, "rhythmHatLevel") == jsonIntValue(rhythmNativeBefore, "rhythmHatLevel")
                         && jsonIntValue(rhythmNativeAfter, "rhythmSnareLevel") == jsonIntValue(rhythmNativeBefore, "rhythmSnareLevel")
                         && jsonIntValue(rhythmNativeAfter, "rhythmTomLevel") == jsonIntValue(rhythmNativeBefore, "rhythmTomLevel")
                         && jsonIntValue(rhythmNativeAfter, "rhythmCymLevel") == jsonIntValue(rhythmNativeBefore, "rhythmCymLevel"),
                     "Live OPL3 percussion KSL edits should preserve rhythm total levels");

        sendNoteOn(oplRhythmNativeProcessor, 60);
        const auto rhythmNativeRetriggered = oplRhythmNativeProcessor.currentCoreDebugStateJson();
        const std::array<const char*, 6> rhythmControlKeys {
            "rhythmOperatorControl0", "rhythmOperatorControl1", "rhythmOperatorControl2",
            "rhythmOperatorControl3", "rhythmOperatorControl4", "rhythmOperatorControl5"
        };
        const std::array<const char*, 6> rhythmTotalLevelKeys {
            "rhythmOperatorTotalLevelRegister0", "rhythmOperatorTotalLevelRegister1", "rhythmOperatorTotalLevelRegister2",
            "rhythmOperatorTotalLevelRegister3", "rhythmOperatorTotalLevelRegister4", "rhythmOperatorTotalLevelRegister5"
        };
        for (const auto* key : rhythmControlKeys)
            ok &= expect(jsonIntValue(rhythmNativeRetriggered, key) == jsonIntValue(rhythmNativeAfter, key),
                         std::string("Retriggered OPL3 percussion should preserve exact AM/VIB/KSR, EGT, and MULT byte: ") + key);
        for (const auto* key : rhythmTotalLevelKeys)
            ok &= expect(jsonIntValue(rhythmNativeRetriggered, key) == jsonIntValue(rhythmNativeAfter, key),
                         std::string("Retriggered OPL3 percussion should preserve exact KSL and TL byte: ") + key);
        ok &= expect(jsonIntValue(rhythmNativeRetriggered, "rhythmKeyBits") == jsonIntValue(rhythmNativeAfter, "rhythmKeyBits")
                         && jsonIntValue(rhythmNativeRetriggered, "rhythmRegister") == jsonIntValue(rhythmNativeAfter, "rhythmRegister"),
                     "Retriggered OPL3 percussion should preserve the active $BD key state");
    }
    ok &= expectFeedbackPitchNeutrality(5, "OPN2");
    ok &= expectFeedbackPitchNeutrality(6, "OPL3");

    {
        auto opn2RawFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("chipper-opn2-dac-test.raw");
        auto opn2WavFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("chipper-opn2-dac-test.wav");
        opn2RawFile.deleteFile();
        opn2WavFile.deleteFile();
        ok &= expect(writeBinaryFixture(opn2RawFile, 270000u),
                     "Should write temporary OPN2 DAC raw fixture");
        ok &= expect(writeWavFixture(opn2WavFile, 523.25f),
                     "Should write temporary OPN2 DAC WAV fixture");

        ChipperAudioProcessor opn2Processor;
        opn2Processor.prepareToPlay(48000.0, 64);
        setPlainFromHost(opn2Processor, chipper::parameters::id::chipMode, 5.0f);
        setPlainFromHost(opn2Processor, chipper::parameters::id::snNoiseMode, 2.0f);
        processEmptyBlock(opn2Processor);
        ok &= expect(opn2Processor.loadOpn2DacSampleFile(opn2RawFile).wasOk(),
                     "OPN2 raw DAC sample load should succeed");

        const auto defaultOpn2Info = opn2Processor.opn2DacSampleInfo();
        ok &= expect(defaultOpn2Info.loaded && defaultOpn2Info.byteCount == 270000
                         && defaultOpn2Info.copiedByteCount == 262144
                         && defaultOpn2Info.truncated,
                     "OPN2 DAC status should report source bytes and the 256 KiB playback window");
        ok &= expect(defaultOpn2Info.sourceRateHz == 0.0 && defaultOpn2Info.rootNote == 60
                         && defaultOpn2Info.trimStart == 0u && defaultOpn2Info.trimEnd == 0u
                         && ! defaultOpn2Info.holdLastValue,
                     "Raw OPN2 DAC imports should retain legacy native-rate, C4, full-trim, centered-tail semantics");
        ok &= expect(opn2Processor.configureOpn2DacSample(48, 8u, 128u, true).wasOk(),
                     "OPN2 DAC playback settings should accept bounded root, trim, and tail values");
        ok &= expect(opn2Processor.configureOpn2DacSample(128, 0u, 0u, false).failed()
                         && opn2Processor.configureOpn2DacSample(60, 262144u, 0u, false).failed(),
                     "OPN2 DAC playback settings should reject invalid root notes and trim bounds");
        const auto opn2Info = opn2Processor.opn2DacSampleInfo();
        ok &= expect(opn2Info.rootNote == 48 && opn2Info.trimStart == 8u && opn2Info.trimEnd == 128u
                         && opn2Info.holdLastValue && opn2Info.statusLine.contains("Tail Hold"),
                     "OPN2 DAC status should expose configured root, trim, and tail behavior");
        const auto opn2Preview = opn2Processor.sampleWaveformSnapshot(chipper::ChipMode::ym2612);
        ok &= expect(opn2Preview.loaded && opn2Preview.sourceSampleCount == 270000
                         && opn2Preview.label.contains("chipper-opn2-dac-test.raw"),
                     "OPN2 DAC waveform preview should expose the loaded user sample");

        sendNoteOn(opn2Processor, 60);
        const auto opn2Debug = opn2Processor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(opn2Debug, "dacExternalSampleLoaded") == 1
                         && jsonIntValue(opn2Debug, "dacExternalSampleBytes") == 262144
                         && jsonIntValue(opn2Debug, "dacSampleSourceUser") == 1,
                     "OPN2 DAC core should receive the bounded user bytes and select them for playback");

        auto opn2StateXml = opn2Processor.createStateXml();
        const auto* savedOpn2Sample = opn2StateXml != nullptr
            ? opn2StateXml->getChildByName("CHIPPER_OPN2_DAC_SAMPLE") : nullptr;
        ok &= expect(savedOpn2Sample != nullptr,
                     "OPN2 state XML should save the loaded DAC sample path");
        ok &= expect(savedOpn2Sample != nullptr
                         && savedOpn2Sample->getDoubleAttribute("sourceRateHz", -1.0) == 0.0
                         && savedOpn2Sample->getIntAttribute("rootNote", -1) == 48
                         && savedOpn2Sample->getIntAttribute("trimStart", -1) == 8
                         && savedOpn2Sample->getIntAttribute("trimEnd", -1) == 128
                         && savedOpn2Sample->getStringAttribute("tailBehavior") == "hold",
                     "OPN2 state XML should save source-rate, root, trim, and tail metadata");
        auto opn2ProjectXml = opn2Processor.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
        ok &= expect(opn2StateXml != nullptr
                         && countElementsNamed(*opn2StateXml, chipper::state::embeddedSampleStateTag) == 0u,
                     "Shareable OPN2 preset XML should remain reference-only");
        ok &= expect(opn2ProjectXml != nullptr
                         && countElementsNamed(*opn2ProjectXml, chipper::state::embeddedSampleStateTag) == 1u,
                     "OPN2 project state should embed the loaded DAC bytes");


        ChipperAudioProcessor restoredOpn2Processor;
        restoredOpn2Processor.prepareToPlay(48000.0, 64);
        if (opn2StateXml != nullptr)
            ok &= expect(restoredOpn2Processor.restoreStateXml(*opn2StateXml).wasOk(),
                         "OPN2 DAC sample state restore should succeed");
        processEmptyBlock(restoredOpn2Processor);
        const auto restoredOpn2Info = restoredOpn2Processor.opn2DacSampleInfo();
        const auto restoredOpn2Debug = restoredOpn2Processor.currentCoreDebugStateJson();
        ok &= expect(restoredOpn2Info.loaded && restoredOpn2Info.byteCount == 270000
                         && restoredOpn2Info.sourceRateHz == 0.0 && restoredOpn2Info.rootNote == 48
                         && restoredOpn2Info.trimStart == 8u && restoredOpn2Info.trimEnd == 128u
                         && restoredOpn2Info.holdLastValue,
                     "OPN2 DAC state restore should reload the user sample path and playback metadata");
        ok &= expect(jsonIntValue(restoredOpn2Debug, "dacExternalSampleLoaded") == 1
                         && jsonIntValue(restoredOpn2Debug, "dacExternalSampleBytes") == 262144,
                     "Restored OPN2 core should receive the bounded user sample bytes");

        auto portableOpn2PresetDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                         .getChildFile("chipper-portable-opn2-preset-test");
        portableOpn2PresetDir.deleteRecursively();
        const auto portableOpn2Sample = portableOpn2PresetDir.getChildFile("Samples")
                                            .getChildFile("portable-opn2.raw");
        ok &= expect(portableOpn2Sample.getParentDirectory().createDirectory().wasOk()
                         && writeBinaryFixture(portableOpn2Sample, 512u),
                     "Should write portable OPN2 DAC fixture beside preset");
        if (opn2StateXml != nullptr)
        {
            rewritePresetSamplePaths(*opn2StateXml, "CHIPPER_OPN2_DAC_SAMPLE", "portable-opn2.raw");
            ChipperAudioProcessor portableOpn2Processor;
            portableOpn2Processor.prepareToPlay(48000.0, 64);
            ok &= expect(portableOpn2Processor.restoreStateXml(*opn2StateXml, portableOpn2PresetDir).wasOk(),
                         "Portable preset restore should accept a relative OPN2 DAC sample reference");
            const auto portableOpn2Info = portableOpn2Processor.opn2DacSampleInfo();
            ok &= expect(portableOpn2Info.loaded && portableOpn2Info.sampleName == "portable-opn2.raw"
                             && portableOpn2Info.byteCount == 512,
                         "Portable OPN2 preset restore should load the sample from its Samples folder");
        }

        ChipperAudioProcessor missingOpn2XmlProcessor;
        missingOpn2XmlProcessor.prepareToPlay(48000.0, 64);
        auto missingOpn2Xml = missingOpn2XmlProcessor.createStateXml();
        ok &= expect(missingOpn2Xml != nullptr, "Should create OPN2 state XML for missing-sample warning");
        if (missingOpn2Xml != nullptr)
        {
            auto* missingSample = new juce::XmlElement("CHIPPER_OPN2_DAC_SAMPLE");
            missingSample->setAttribute("path", portableOpn2PresetDir.getChildFile("missing-opn2.raw").getFullPathName());
            missingSample->setAttribute("sourceRateHz", 32000.0);
            missingSample->setAttribute("rootNote", 51);
            missingSample->setAttribute("trimStart", 11);
            missingSample->setAttribute("trimEnd", 77);
            missingSample->setAttribute("tailBehavior", "hold");
            missingOpn2Xml->addChildElement(missingSample);
            ChipperAudioProcessor missingOpn2Processor;
            missingOpn2Processor.prepareToPlay(48000.0, 64);
            ok &= expect(missingOpn2Processor.restoreStateXml(*missingOpn2Xml, portableOpn2PresetDir).wasOk(),
                         "Missing OPN2 DAC sample should not fail the whole preset restore");
            auto missingOpn2Info = missingOpn2Processor.opn2DacSampleInfo();
            const auto missingOpn2Preview = missingOpn2Processor.sampleWaveformSnapshot(chipper::ChipMode::ym2612);
            ok &= expect(missingOpn2Info.statusLine.contains("OPN2 DAC sample restore issue")
                             && missingOpn2Info.statusLine.contains("missing-opn2.raw"),
                         "OPN2 DAC status should expose missing sample references");
            ok &= expect(missingOpn2Preview.label.contains("OPN2 DAC sample restore issue"),
                         "OPN2 DAC waveform preview should expose missing sample references");
            auto missingOpn2ResavedXml = missingOpn2Processor.createStateXml();
            const auto* missingOpn2ResavedSample = missingOpn2ResavedXml != nullptr
                ? missingOpn2ResavedXml->getChildByName("CHIPPER_OPN2_DAC_SAMPLE") : nullptr;
            ok &= expect(missingOpn2ResavedSample != nullptr
                             && missingOpn2ResavedSample->getStringAttribute("path").contains("missing-opn2.raw")
                             && missingOpn2ResavedSample->getDoubleAttribute("sourceRateHz", -1.0) == 32000.0
                             && missingOpn2ResavedSample->getIntAttribute("rootNote", -1) == 51
                             && missingOpn2ResavedSample->getIntAttribute("trimStart", -1) == 11
                             && missingOpn2ResavedSample->getIntAttribute("trimEnd", -1) == 77
                             && missingOpn2ResavedSample->getStringAttribute("tailBehavior") == "hold"
                             && countElementsNamed(*missingOpn2ResavedSample, chipper::state::embeddedSampleStateTag) == 0u,
                         "Re-saving a missing OPN2 asset should retain its reference and playback metadata without inventing bytes");
            ok &= expect(missingOpn2Processor.loadOpn2DacSampleFile(opn2WavFile).wasOk(),
                         "Manual OPN2 WAV load should succeed after a restore warning");
            missingOpn2Info = missingOpn2Processor.opn2DacSampleInfo();
            ok &= expect(missingOpn2Info.loaded && missingOpn2Info.byteCount == 256
                             && std::abs(missingOpn2Info.sourceRateHz - 48000.0) < 0.001
                             && missingOpn2Info.rootNote == 60
                             && missingOpn2Info.trimStart == 0u && missingOpn2Info.trimEnd == 0u
                             && ! missingOpn2Info.holdLastValue
                             && ! missingOpn2Info.statusLine.contains("restore issue"),
                         "OPN2 WAV import should preserve its sample rate, use neutral playback defaults, and clear stale restore warnings");
            auto embeddedWavState = missingOpn2Processor.createStateXml(
                ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
            opn2WavFile.deleteFile();
            ChipperAudioProcessor embeddedWavProcessor;
            embeddedWavProcessor.prepareToPlay(48000.0, 64);
            ok &= expect(embeddedWavState != nullptr
                             && embeddedWavProcessor.restoreStateXml(*embeddedWavState).wasOk(),
                         "Embedded OPN2 WAV state should restore without the original source file");
            const auto embeddedWavInfo = embeddedWavProcessor.opn2DacSampleInfo();
            ok &= expect(embeddedWavInfo.loaded && embeddedWavInfo.byteCount == 256
                             && std::abs(embeddedWavInfo.sourceRateHz - 48000.0) < 0.001,
                         "Embedded OPN2 WAV state should preserve signed PCM bytes and source rate");
        }

        opn2RawFile.deleteFile();
        if (opn2ProjectXml != nullptr)
        {
            ChipperAudioProcessor embeddedOpn2Processor;
            embeddedOpn2Processor.prepareToPlay(48000.0, 64);
            ok &= expect(embeddedOpn2Processor.restoreStateXml(*opn2ProjectXml).wasOk(),
                         "OPN2 project state should restore after its source file is deleted");
            processEmptyBlock(embeddedOpn2Processor);
            const auto embeddedInfo = embeddedOpn2Processor.opn2DacSampleInfo();
            ok &= expect(embeddedInfo.loaded && embeddedInfo.byteCount == 270000u
                             && embeddedInfo.rootNote == 48
                             && embeddedInfo.trimStart == 8u && embeddedInfo.trimEnd == 128u
                             && embeddedInfo.holdLastValue
                             && embeddedInfo.statusLine.contains("Using embedded project copy"),
                         "OPN2 deleted-source restore should use the bounded embedded project copy with its playback metadata");
            embeddedOpn2Processor.clearOpn2DacSample();
            ok &= expect(! embeddedOpn2Processor.opn2DacSampleInfo().loaded
                             && embeddedOpn2Processor.createStateXml()->getChildByName("CHIPPER_OPN2_DAC_SAMPLE") == nullptr,
                         "Clearing the OPN2 DAC sample should clear playback data and its saved reference");
        }

        portableOpn2PresetDir.deleteRecursively();
        opn2RawFile.deleteFile();
        opn2WavFile.deleteFile();
    }

    {
        auto opnaRomFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("chipper-opna-rhythm-rom-test.bin");
        auto opnaAdpcmBFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("chipper-opna-adpcm-b-test.bin");
        opnaRomFile.deleteFile();
        opnaAdpcmBFile.deleteFile();
        ok &= expect(writeBinaryFixture(opnaRomFile, 9000u),
                     "Should write temporary OPNA rhythm ROM fixture");
        ok &= expect(writeBinaryFixture(opnaAdpcmBFile, 270000u),
                     "Should write temporary OPNA ADPCM-B fixture");

        ChipperAudioProcessor opnaProcessor;
        opnaProcessor.prepareToPlay(48000.0, 64);
        setPlainFromHost(opnaProcessor, chipper::parameters::id::chipMode, 17.0f);
        processEmptyBlock(opnaProcessor);
        ok &= expect(opnaProcessor.loadOpnaRhythmRomFile(opnaRomFile).wasOk(),
                     "OPNA rhythm ROM file load should succeed");
        ok &= expect(opnaProcessor.loadOpnaAdpcmBSampleFile(opnaAdpcmBFile).wasOk(),
                     "OPNA ADPCM-B sample file load should succeed");

        auto opnaInfo = opnaProcessor.opnaRhythmRomInfo();
        ok &= expect(opnaInfo.loaded && opnaInfo.byteCount == 9000u
                         && opnaInfo.copiedByteCount == 8192u
                         && opnaInfo.truncated,
                     "OPNA rhythm ROM info should report user bytes and 8 KB copy window");

        auto opnaAdpcmBInfo = opnaProcessor.opnaAdpcmBSampleInfo();
        ok &= expect(opnaAdpcmBInfo.loaded && opnaAdpcmBInfo.byteCount == 270000u
                         && opnaAdpcmBInfo.copiedByteCount == 262144u
                         && opnaAdpcmBInfo.truncated,
                     "OPNA ADPCM-B info should report user bytes and 256 KiB memory window");

        const auto opnaDebug = opnaProcessor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(opnaDebug, "opnaAdpcmAUserRomLoaded") == 1
                         && jsonIntValue(opnaDebug, "opnaAdpcmARomProvidedBytes") == 9000
                         && jsonIntValue(opnaDebug, "opnaAdpcmARomCopiedBytes") == 8192,
                     "OPNA core debug state should expose loaded user rhythm ROM bytes");
        ok &= expect(jsonIntValue(opnaDebug, "opnaAdpcmBLoaded") == 1
                         && jsonIntValue(opnaDebug, "opnaAdpcmBProvidedBytes") == 270000
                         && jsonIntValue(opnaDebug, "opnaAdpcmBCopiedBytes") == 262144,
                     "OPNA core debug state should expose loaded ADPCM-B sample bytes");

        auto opnaStateXml = opnaProcessor.createStateXml();
        ok &= expect(opnaStateXml != nullptr, "OPNA state XML should save loaded ADPCM paths");
        auto opnaProjectXml = opnaProcessor.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
        ok &= expect(opnaStateXml != nullptr
                         && countElementsNamed(*opnaStateXml, chipper::state::embeddedSampleStateTag) == 0u,
                     "Shareable OPNA preset XML should remain reference-only");
        ok &= expect(opnaProjectXml != nullptr
                         && countElementsNamed(*opnaProjectXml, chipper::state::embeddedSampleStateTag) == 2u,
                     "OPNA project state should embed both loaded sample-memory assets");


        ChipperAudioProcessor restoredOpnaProcessor;
        restoredOpnaProcessor.prepareToPlay(48000.0, 64);
        if (opnaStateXml != nullptr)
            ok &= expect(restoredOpnaProcessor.restoreStateXml(*opnaStateXml).wasOk(),
                         "OPNA rhythm ROM state restore should succeed");

        processEmptyBlock(restoredOpnaProcessor);
        auto restoredOpnaInfo = restoredOpnaProcessor.opnaRhythmRomInfo();
        ok &= expect(restoredOpnaInfo.loaded && restoredOpnaInfo.byteCount == 9000u
                         && restoredOpnaInfo.copiedByteCount == 8192u,
                     "OPNA rhythm ROM state restore should reload the user ROM path");

        auto restoredOpnaAdpcmBInfo = restoredOpnaProcessor.opnaAdpcmBSampleInfo();
        ok &= expect(restoredOpnaAdpcmBInfo.loaded && restoredOpnaAdpcmBInfo.byteCount == 270000u
                         && restoredOpnaAdpcmBInfo.copiedByteCount == 262144u,
                     "OPNA ADPCM-B state restore should reload the user sample path");

        const auto restoredOpnaDebug = restoredOpnaProcessor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(restoredOpnaDebug, "opnaAdpcmAUserRomLoaded") == 1
                         && jsonIntValue(restoredOpnaDebug, "opnaAdpcmARomCopiedBytes") == 8192,
                     "Restored OPNA core should receive the user rhythm ROM bytes");
        ok &= expect(jsonIntValue(restoredOpnaDebug, "opnaAdpcmBLoaded") == 1
                         && jsonIntValue(restoredOpnaDebug, "opnaAdpcmBCopiedBytes") == 262144,
                     "Restored OPNA core should receive the user ADPCM-B sample bytes");

        opnaRomFile.deleteFile();
        opnaAdpcmBFile.deleteFile();
        if (opnaProjectXml != nullptr)
        {
            ChipperAudioProcessor embeddedOpnaProcessor;
            embeddedOpnaProcessor.prepareToPlay(48000.0, 64);
            ok &= expect(embeddedOpnaProcessor.restoreStateXml(*opnaProjectXml).wasOk(),
                         "OPNA project state should restore after both source files are deleted");
            processEmptyBlock(embeddedOpnaProcessor);
            const auto embeddedRhythm = embeddedOpnaProcessor.opnaRhythmRomInfo();
            const auto embeddedAdpcmB = embeddedOpnaProcessor.opnaAdpcmBSampleInfo();
            ok &= expect(embeddedRhythm.loaded && embeddedRhythm.byteCount == 9000u
                             && embeddedRhythm.statusLine.contains("Using embedded project copy")
                             && embeddedAdpcmB.loaded && embeddedAdpcmB.byteCount == 270000u
                             && embeddedAdpcmB.statusLine.contains("Using embedded project copy"),
                         "OPNA deleted-source restore should use both bounded embedded project copies");
        }

        opnaRomFile.deleteFile();
        opnaAdpcmBFile.deleteFile();
    }

    {
        auto opnbAdpcmAFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("chipper-opnb-adpcm-a-test.bin");
        auto opnbAdpcmBFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("chipper-opnb-adpcm-b-test.bin");
        opnbAdpcmAFile.deleteFile();
        opnbAdpcmBFile.deleteFile();
        ok &= expect(writeBinaryFixture(opnbAdpcmAFile, 1050000u),
                     "Should write temporary OPNB ADPCM-A fixture");
        ok &= expect(writeBinaryFixture(opnbAdpcmBFile, 8192u),
                     "Should write temporary OPNB ADPCM-B fixture");

        ChipperAudioProcessor opnbProcessor;
        opnbProcessor.prepareToPlay(48000.0, 64);
        setPlainFromHost(opnbProcessor, chipper::parameters::id::chipMode, 18.0f);
        processEmptyBlock(opnbProcessor);
        ok &= expect(opnbProcessor.loadOpnbAdpcmASampleFile(opnbAdpcmAFile).wasOk(),
                     "OPNB ADPCM-A sample file load should succeed");
        ok &= expect(opnbProcessor.loadOpnbAdpcmBSampleFile(opnbAdpcmBFile).wasOk(),
                     "OPNB ADPCM-B sample file load should succeed");

        auto opnbAdpcmAInfo = opnbProcessor.opnbAdpcmASampleInfo();
        ok &= expect(opnbAdpcmAInfo.loaded && opnbAdpcmAInfo.byteCount == 1050000u
                         && opnbAdpcmAInfo.copiedByteCount == 1048576u
                         && opnbAdpcmAInfo.truncated,
                     "OPNB ADPCM-A info should report user bytes and 1 MiB memory window");

        auto opnbAdpcmBInfo = opnbProcessor.opnbAdpcmBSampleInfo();
        ok &= expect(opnbAdpcmBInfo.loaded && opnbAdpcmBInfo.byteCount == 8192u
                         && opnbAdpcmBInfo.copiedByteCount == 8192u
                         && opnbAdpcmBInfo.memoryByteCount == 16777216
                         && ! opnbAdpcmBInfo.truncated,
                     "OPNB ADPCM-B info should report user bytes and 16 MiB memory window");

        const auto opnbDebug = opnbProcessor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(opnbDebug, "opnbAdpcmALoaded") == 1
                         && jsonIntValue(opnbDebug, "opnbAdpcmAProvidedBytes") == 1050000
                         && jsonIntValue(opnbDebug, "opnbAdpcmACopiedBytes") == 1048576,
                     "OPNB core debug state should expose loaded ADPCM-A sample bytes");
        ok &= expect(jsonIntValue(opnbDebug, "opnbAdpcmBLoaded") == 1
                         && jsonIntValue(opnbDebug, "opnbAdpcmBProvidedBytes") == 8192
                         && jsonIntValue(opnbDebug, "opnbAdpcmBCopiedBytes") == 8192,
                     "OPNB core debug state should expose loaded ADPCM-B sample bytes");

        auto opnbStateXml = opnbProcessor.createStateXml();
        ok &= expect(opnbStateXml != nullptr, "OPNB state XML should save loaded ADPCM paths");
        auto opnbProjectXml = opnbProcessor.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
        ok &= expect(opnbStateXml != nullptr
                         && countElementsNamed(*opnbStateXml, chipper::state::embeddedSampleStateTag) == 0u,
                     "Shareable OPNB preset XML should remain reference-only");
        ok &= expect(opnbProjectXml != nullptr
                         && countElementsNamed(*opnbProjectXml, chipper::state::embeddedSampleStateTag) == 2u,
                     "OPNB project state should embed both loaded sample-memory assets");


        ChipperAudioProcessor restoredOpnbProcessor;
        restoredOpnbProcessor.prepareToPlay(48000.0, 64);
        if (opnbStateXml != nullptr)
            ok &= expect(restoredOpnbProcessor.restoreStateXml(*opnbStateXml).wasOk(),
                         "OPNB ADPCM state restore should succeed");

        processEmptyBlock(restoredOpnbProcessor);
        auto restoredOpnbAdpcmAInfo = restoredOpnbProcessor.opnbAdpcmASampleInfo();
        ok &= expect(restoredOpnbAdpcmAInfo.loaded && restoredOpnbAdpcmAInfo.byteCount == 1050000u
                         && restoredOpnbAdpcmAInfo.copiedByteCount == 1048576u,
                     "OPNB ADPCM-A state restore should reload the user sample path");

        auto restoredOpnbAdpcmBInfo = restoredOpnbProcessor.opnbAdpcmBSampleInfo();
        ok &= expect(restoredOpnbAdpcmBInfo.loaded && restoredOpnbAdpcmBInfo.byteCount == 8192u
                         && restoredOpnbAdpcmBInfo.copiedByteCount == 8192u,
                     "OPNB ADPCM-B state restore should reload the user sample path");

        const auto restoredOpnbDebug = restoredOpnbProcessor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(restoredOpnbDebug, "opnbAdpcmALoaded") == 1
                         && jsonIntValue(restoredOpnbDebug, "opnbAdpcmACopiedBytes") == 1048576,
                     "Restored OPNB core should receive the user ADPCM-A sample bytes");
        ok &= expect(jsonIntValue(restoredOpnbDebug, "opnbAdpcmBLoaded") == 1
                         && jsonIntValue(restoredOpnbDebug, "opnbAdpcmBCopiedBytes") == 8192,
                     "Restored OPNB core should receive the user ADPCM-B sample bytes");

        opnbAdpcmAFile.deleteFile();
        opnbAdpcmBFile.deleteFile();
        if (opnbProjectXml != nullptr)
        {
            ChipperAudioProcessor embeddedOpnbProcessor;
            embeddedOpnbProcessor.prepareToPlay(48000.0, 64);
            ok &= expect(embeddedOpnbProcessor.restoreStateXml(*opnbProjectXml).wasOk(),
                         "OPNB project state should restore after both source files are deleted");
            processEmptyBlock(embeddedOpnbProcessor);
            const auto embeddedAdpcmA = embeddedOpnbProcessor.opnbAdpcmASampleInfo();
            const auto embeddedAdpcmB = embeddedOpnbProcessor.opnbAdpcmBSampleInfo();
            ok &= expect(embeddedAdpcmA.loaded && embeddedAdpcmA.byteCount == 1050000u
                             && embeddedAdpcmA.statusLine.contains("Using embedded project copy")
                             && embeddedAdpcmB.loaded && embeddedAdpcmB.byteCount == 8192u
                             && embeddedAdpcmB.statusLine.contains("Using embedded project copy"),
                         "OPNB deleted-source restore should use both bounded embedded project copies");
        }

        opnbAdpcmAFile.deleteFile();
        opnbAdpcmBFile.deleteFile();
    }

    {
        const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto opnaWav = temp.getChildFile("chipper-opna-adpcm-b-convert.wav");
        auto opnaAiff = temp.getChildFile("chipper-opna-adpcm-b-convert.aiff");
        auto malformedWav = temp.getChildFile("chipper-opna-adpcm-b-malformed.wav");
        opnaWav.deleteFile();
        opnaAiff.deleteFile();
        malformedWav.deleteFile();
        ok &= expect(writeWavFixture(opnaWav, 440.0f), "Should write OPNA ADPCM-B WAV conversion fixture");
        ok &= expect(writeAiffFixture(opnaAiff, 440.0f), "Should write OPNA ADPCM-B AIFF conversion fixture");
        ok &= expect(writeBinaryFixture(malformedWav, 64u), "Should write malformed OPNA WAV fixture");

        ChipperAudioProcessor opnaConverted;
        opnaConverted.prepareToPlay(48000.0, 64);
        setPlainFromHost(opnaConverted, chipper::parameters::id::chipMode, 17.0f);
        setPlainFromHost(opnaConverted, chipper::parameters::id::macro, 5.0f);
        processEmptyBlock(opnaConverted);
        ok &= expect(opnaConverted.loadOpnaAdpcmBSampleFile(opnaWav).wasOk(),
                     "OPNA ADPCM-B should convert WAV input");
        auto opnaConvertedInfo = opnaConverted.opnaAdpcmBSampleInfo();
        ok &= expect(opnaConvertedInfo.loaded && opnaConvertedInfo.convertedFromPcm
                         && opnaConvertedInfo.byteCount == 16 && opnaConvertedInfo.copiedByteCount == 16
                         && opnaConvertedInfo.decodedSampleCount == 28 && ! opnaConvertedInfo.truncated,
                     "OPNA WAV conversion should resample to 5200 Hz and align encoded memory to four bytes");
        ok &= expect(opnaConvertedInfo.statusLine.contains("converted")
                         && opnaConvertedInfo.statusLine.contains("5.2 kHz"),
                     "OPNA converted status should expose rate and provenance");
        auto opnaConvertedPreview = opnaConverted.sampleWaveformSnapshot(chipper::ChipMode::ym2608);
        ok &= expect(opnaConvertedPreview.loaded && opnaConvertedPreview.sourceSampleCount == 28
                         && opnaConvertedPreview.label.contains("Decoded OPNA ADPCM-B"),
                     "OPNA preview should decode heard ADPCM-B samples and trim alignment padding");
        sendNoteOn(opnaConverted, 60);
        const auto opnaWavDebug = opnaConverted.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(opnaWavDebug, "opnaAdpcmBProvidedBytes") == 16
                         && jsonIntValue(opnaWavDebug, "opnaAdpcmBCopiedBytes") == 16
                         && jsonIntValue(opnaWavDebug, "opnaAdpcmBEndRegister") == 3
                         && jsonIntValue(opnaWavDebug, "opnaAdpcmBReadCount") > 0,
                     "OPNA converted bytes should reach the aligned ymfm memory window and be read");
        const auto opnaWavChecksum = jsonIntValue(opnaWavDebug, "opnaAdpcmBChecksum");
        auto opnaConvertedReferenceXml = opnaConverted.createStateXml();
        auto opnaConvertedProjectXml = opnaConverted.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
        ok &= expect(opnaConvertedProjectXml != nullptr
                         && countElementsNamed(*opnaConvertedProjectXml, chipper::state::embeddedSampleStateTag) == 1u,
                     "OPNA converted project state should embed the canonical encoded fallback only");

        ok &= expect(opnaConverted.loadOpnaAdpcmBSampleFile(opnaAiff).wasOk(),
                     "OPNA ADPCM-B should convert AIFF input");
        sendNoteOn(opnaConverted, 60);
        const auto opnaAiffDebug = opnaConverted.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(opnaAiffDebug, "opnaAdpcmBChecksum") == opnaWavChecksum
                         && opnaConverted.opnaAdpcmBSampleInfo().byteCount == 16,
                     "Equivalent WAV and AIFF PCM should produce identical deterministic OPNA ADPCM-B bytes");
        const auto opnaChecksumBeforeMalformed = jsonIntValue(opnaAiffDebug, "opnaAdpcmBChecksum");
        ok &= expect(opnaConverted.loadOpnaAdpcmBSampleFile(malformedWav).failed(),
                     "Malformed WAV should fail instead of loading its container bytes as ADPCM-B");
        ok &= expect(jsonIntValue(opnaConverted.currentCoreDebugStateJson(), "opnaAdpcmBChecksum")
                         == opnaChecksumBeforeMalformed,
                     "Failed OPNA conversion must leave the previously loaded sample untouched");

        opnaWav.deleteFile();
        if (opnaConvertedReferenceXml != nullptr)
        {
            ChipperAudioProcessor missingOpnaReference;
            missingOpnaReference.prepareToPlay(48000.0, 64);
            ok &= expect(missingOpnaReference.restoreStateXml(*opnaConvertedReferenceXml).wasOk(),
                         "Missing OPNA reference-only state should restore parameters with an asset warning");
            const auto missingInfo = missingOpnaReference.opnaAdpcmBSampleInfo();
            ok &= expect(! missingInfo.loaded && missingInfo.statusLine.containsIgnoreCase("does not exist"),
                         "Reference-only OPNA restore should not invent converted bytes after source deletion");
        }
        if (opnaConvertedProjectXml != nullptr)
        {
            ChipperAudioProcessor embeddedOpnaConverted;
            embeddedOpnaConverted.prepareToPlay(48000.0, 64);
            ok &= expect(embeddedOpnaConverted.restoreStateXml(*opnaConvertedProjectXml).wasOk(),
                         "OPNA converted project state should restore after source deletion");
            processEmptyBlock(embeddedOpnaConverted);
            const auto embeddedInfo = embeddedOpnaConverted.opnaAdpcmBSampleInfo();
            const auto embeddedPreview = embeddedOpnaConverted.sampleWaveformSnapshot(chipper::ChipMode::ym2608);
            ok &= expect(embeddedInfo.loaded && embeddedInfo.convertedFromPcm
                             && embeddedInfo.byteCount == 16 && embeddedInfo.decodedSampleCount == 28
                             && embeddedInfo.statusLine.contains("Using embedded project copy")
                             && embeddedPreview.loaded && embeddedPreview.sourceSampleCount == 28,
                         "OPNA embedded fallback should preserve encoded bytes and trimmed preview metadata");
        }

        opnaAiff.deleteFile();
        malformedWav.deleteFile();
    }

    {
        const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto opnbWav = temp.getChildFile("chipper-opnb-adpcm-b-convert.wav");
        auto opnbAiff = temp.getChildFile("chipper-opnb-adpcm-b-convert.aiff");
        opnbWav.deleteFile();
        opnbAiff.deleteFile();
        ok &= expect(writeWavFixture(opnbWav, 330.0f), "Should write OPNB ADPCM-B WAV conversion fixture");
        ok &= expect(writeAiffFixture(opnbAiff, 330.0f), "Should write OPNB ADPCM-B AIFF conversion fixture");

        ChipperAudioProcessor opnbConverted;
        opnbConverted.prepareToPlay(48000.0, 64);
        setPlainFromHost(opnbConverted, chipper::parameters::id::chipMode, 18.0f);
        setPlainFromHost(opnbConverted, chipper::parameters::id::macro, 5.0f);
        processEmptyBlock(opnbConverted);
        ok &= expect(opnbConverted.loadOpnbAdpcmBSampleFile(opnbWav).wasOk(),
                     "OPNB ADPCM-B should convert WAV input");
        const auto opnbConvertedInfo = opnbConverted.opnbAdpcmBSampleInfo();
        ok &= expect(opnbConvertedInfo.loaded && opnbConvertedInfo.convertedFromPcm
                         && opnbConvertedInfo.byteCount == 256 && opnbConvertedInfo.copiedByteCount == 256
                         && opnbConvertedInfo.decodedSampleCount == 28 && ! opnbConvertedInfo.truncated,
                     "OPNB WAV conversion should resample to 5208.333 Hz and align encoded memory to 256 bytes");
        const auto opnbPreview = opnbConverted.sampleWaveformSnapshot(chipper::ChipMode::ym2610);
        ok &= expect(opnbPreview.loaded && opnbPreview.sourceSampleCount == 28
                         && opnbPreview.label.contains("Decoded OPNB ADPCM-B"),
                     "OPNB preview should decode heard ADPCM-B samples and trim alignment padding");
        sendNoteOn(opnbConverted, 60);
        const auto opnbWavDebug = opnbConverted.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(opnbWavDebug, "opnbAdpcmBProvidedBytes") == 256
                         && jsonIntValue(opnbWavDebug, "opnbAdpcmBCopiedBytes") == 256
                         && jsonIntValue(opnbWavDebug, "opnbAdpcmBEndRegister") == 0
                         && jsonIntValue(opnbWavDebug, "opnbAdpcmBReadCount") > 0,
                     "OPNB converted bytes should reach the aligned ymfm memory window and be read");
        const auto opnbWavChecksum = jsonIntValue(opnbWavDebug, "opnbAdpcmBChecksum");
        auto opnbConvertedProjectXml = opnbConverted.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);

        ok &= expect(opnbConverted.loadOpnbAdpcmBSampleFile(opnbAiff).wasOk(),
                     "OPNB ADPCM-B should convert AIFF input");
        sendNoteOn(opnbConverted, 60);
        ok &= expect(jsonIntValue(opnbConverted.currentCoreDebugStateJson(), "opnbAdpcmBChecksum") == opnbWavChecksum,
                     "Equivalent WAV and AIFF PCM should produce identical deterministic OPNB ADPCM-B bytes");

        opnbWav.deleteFile();
        if (opnbConvertedProjectXml != nullptr)
        {
            ChipperAudioProcessor embeddedOpnbConverted;
            embeddedOpnbConverted.prepareToPlay(48000.0, 64);
            ok &= expect(embeddedOpnbConverted.restoreStateXml(*opnbConvertedProjectXml).wasOk(),
                         "OPNB converted project state should restore after source deletion");
            processEmptyBlock(embeddedOpnbConverted);
            const auto embeddedInfo = embeddedOpnbConverted.opnbAdpcmBSampleInfo();
            ok &= expect(embeddedInfo.loaded && embeddedInfo.convertedFromPcm
                             && embeddedInfo.byteCount == 256 && embeddedInfo.decodedSampleCount == 28
                             && embeddedInfo.statusLine.contains("Using embedded project copy"),
                         "OPNB embedded fallback should preserve aligned converted bytes and metadata");
        }

        opnbAiff.deleteFile();
    }

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

    ChipperAudioProcessor dmcLaneGateProcessor;
    dmcLaneGateProcessor.prepareToPlay(48000.0, 256);
    ok &= expect(dmcLaneGateProcessor.loadNesDmcSampleDirectory(dmcDir).wasOk(),
                 "Should load a DMC sample bank for independent channel-gate coverage");
    setPlainFromHost(dmcLaneGateProcessor, chipper::parameters::id::source4Enabled, 1.0f);
    setPlainFromHost(dmcLaneGateProcessor, chipper::parameters::id::source5Enabled, 0.0f);
    sendNoteOn(dmcLaneGateProcessor, 48);
    auto dmcLaneGateDebug = dmcLaneGateProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(dmcLaneGateDebug, "sourceEnabled4") == 1
                     && jsonIntValue(dmcLaneGateDebug, "sourceEnabled5") == 0
                     && jsonIntValue(dmcLaneGateDebug, "dmcSampleActive") == 0,
                 "NES DMC source 5 should mute independently while the noise source remains enabled");
    setPlainFromHost(dmcLaneGateProcessor, chipper::parameters::id::source4Enabled, 0.0f);
    setPlainFromHost(dmcLaneGateProcessor, chipper::parameters::id::source5Enabled, 1.0f);
    sendNoteOn(dmcLaneGateProcessor, 52);
    dmcLaneGateDebug = dmcLaneGateProcessor.currentCoreDebugStateJson();
    const auto independentDmcGateWorked = jsonIntValue(dmcLaneGateDebug, "sourceEnabled4") == 0
        && jsonIntValue(dmcLaneGateDebug, "sourceEnabled5") == 1
        && jsonIntValue(dmcLaneGateDebug, "dmcSampleCompleted") == 1
        && jsonIntValue(dmcLaneGateDebug, "dmcSampleBitsPlayed") == 32
        && jsonIntValue(dmcLaneGateDebug, "dmcMixerLevel") > 0;
    ok &= expect(independentDmcGateWorked,
                 "NES DMC source 5 should play independently while the noise source is muted");

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
    auto dmcPresetXml = processor.createStateXml();
    auto dmcProjectXml = stateXmlFromBinary(savedState);
    ok &= expect(dmcPresetXml != nullptr
                     && countElementsNamed(*dmcPresetXml, chipper::state::embeddedSampleStateTag) == 0u,
                 "Shareable DMC preset XML should remain reference-only");
    ok &= expect(dmcProjectXml != nullptr
                     && countElementsNamed(*dmcProjectXml, chipper::state::embeddedSampleStateTag) == 32u,
                 "DMC host project state should embed exactly the first 32 included bank slots");
    if (dmcProjectXml != nullptr)
    {
        ChipperAudioProcessor presetPolicyProcessor;
        presetPolicyProcessor.prepareToPlay(48000.0, 64);
        setPlainFromHost(presetPolicyProcessor, chipper::parameters::id::chipMode, 9.0f);
        const auto beforeRejectedRestore = parameterValue(presetPolicyProcessor, chipper::parameters::id::chipMode);
        const auto presetPolicyResult = presetPolicyProcessor.restoreStateXml(*dmcProjectXml, dmcDir);
        ok &= expect(presetPolicyResult.failed()
                         && parameterValue(presetPolicyProcessor, chipper::parameters::id::chipMode) == beforeRejectedRestore,
                     "Preset restore should reject embedded payloads before mutating processor state");

        auto oversizedProjectXml = std::make_unique<juce::XmlElement>(*dmcProjectXml);
        auto payloadIndex = size_t { 0 };
        auto* oversizedPayload = findEmbeddedPayloadAt(*oversizedProjectXml, 0u, payloadIndex);
        if (oversizedPayload != nullptr)
            oversizedPayload->setAttribute("byteCount", static_cast<int>(chipper::state::maxEmbeddedProjectBytes + 1u));
        ChipperAudioProcessor oversizedStateProcessor;
        oversizedStateProcessor.prepareToPlay(48000.0, 64);
        setPlainFromHost(oversizedStateProcessor, chipper::parameters::id::chipMode, 9.0f);
        const auto oversizedResult = oversizedStateProcessor.restoreStateXml(*oversizedProjectXml);
        ok &= expect(oversizedPayload != nullptr && oversizedResult.failed()
                         && parameterValue(oversizedStateProcessor, chipper::parameters::id::chipMode) == 9.0f,
                     "Oversized embedded state should fail atomically before parameter mutation");
    }

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
    if (dmcProjectXml != nullptr)
    {
        ChipperAudioProcessor embeddedDmcProcessor;
        embeddedDmcProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(embeddedDmcProcessor.restoreStateXml(*dmcProjectXml).wasOk(),
                     "DMC project state should restore after its source directory is deleted");
        processEmptyBlock(embeddedDmcProcessor);
        const auto embeddedEntries = embeddedDmcProcessor.nesDmcSampleEntryInfo();
        const auto embeddedNames = embeddedDmcProcessor.nesDmcSampleNames();
        const auto embeddedInfo = embeddedDmcProcessor.nesDmcSamplePlaybackInfo();
        ok &= expect(embeddedEntries.size() == 34u && embeddedNames.size() == 32
                         && embeddedNames[31] == "sample-32.dmc"
                         && embeddedInfo.statusLine.contains("Using embedded project copy"),
                     "DMC deleted-source restore should preserve staged order and the bounded playable bank");

        auto corruptedProjectXml = std::make_unique<juce::XmlElement>(*dmcProjectXml);
        auto corruptedIndex = size_t { 0 };
        auto* corruptedPayload = findEmbeddedPayloadAt(*corruptedProjectXml, 10u, corruptedIndex);
        if (corruptedPayload != nullptr)
            corruptedPayload->setAttribute("checksum", "fnv1a32:00000000");
        ok &= expect(dmcDir.createDirectory().wasOk(),
                     "Should recreate DMC sources around the intentionally corrupt middle slot");
        for (int i = 0; i < 34; ++i)
        {
            if (i == 11)
                continue;
            const auto name = "sample-" + juce::String(i).paddedLeft('0', 2) + ".dmc";
            ok &= expect(writeDmcFixture(dmcDir.getChildFile(name), static_cast<uint8_t>(i + 1)),
                         "Should recreate non-corrupt DMC fixture " + name.toStdString());
        }

        ChipperAudioProcessor corruptedDmcProcessor;
        corruptedDmcProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(corruptedDmcProcessor.restoreStateXml(*corruptedProjectXml).wasOk(),
                     "A corrupt embedded DMC slot should degrade locally instead of rejecting the project");
        setPlainFromHost(corruptedDmcProcessor, chipper::parameters::id::nesDmcSampleSlot, 10.0f);
        setPlainFromHost(corruptedDmcProcessor, chipper::parameters::id::nesDmcPlaybackMode, 0.0f);
        processEmptyBlock(corruptedDmcProcessor);
        const auto corruptedInfo = corruptedDmcProcessor.nesDmcSamplePlaybackInfo();
        const auto corruptedNames = corruptedDmcProcessor.nesDmcSampleNames();
        ok &= expect(corruptedPayload != nullptr && corruptedInfo.activeSlot == 10 && corruptedInfo.byteCount == 0
                         && corruptedInfo.statusLine.contains("sample-11.dmc")
                         && corruptedNames.size() == 32 && corruptedNames[31] == "sample-32.dmc",
                     "A corrupt middle payload should become a silent tombstone without shifting later note-map slots"
                         " active=" + std::to_string(corruptedInfo.activeSlot)
                         + " bytes=" + std::to_string(corruptedInfo.byteCount)
                         + " names=" + std::to_string(corruptedNames.size())
                         + " status=" + corruptedInfo.statusLine.toStdString());
    }

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
    auto spcPresetXml = processor.createStateXml();
    auto spcProjectXml = stateXmlFromBinary(savedSpcState);
    ok &= expect(spcPresetXml != nullptr
                     && countElementsNamed(*spcPresetXml, chipper::state::embeddedSampleStateTag) == 0u,
                 "Shareable SPC700 preset XML should remain reference-only");
    const auto* spcProjectBank = spcProjectXml != nullptr ? spcProjectXml->getChildByName("CHIPPER_SPC700_BRR_BANK") : nullptr;
    ok &= expect(spcProjectBank != nullptr
                     && countElementsNamed(*spcProjectBank, chipper::state::embeddedSampleStateTag) == 3u,
                 "SPC700 host project state should embed every playable BRR bank slot");
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
    if (spcProjectXml != nullptr)
    {
        ChipperAudioProcessor embeddedSpcProcessor;
        embeddedSpcProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(embeddedSpcProcessor.restoreStateXml(*spcProjectXml).wasOk(),
                     "SPC700 BRR project state should restore after its source directory is deleted");
        processEmptyBlock(embeddedSpcProcessor);
        const auto embeddedBrrNames = embeddedSpcProcessor.spc700BrrSampleNames();
        const auto embeddedBrrInfo = embeddedSpcProcessor.spc700BrrSampleInfo();
        ok &= expect(embeddedBrrNames.size() == 3 && embeddedBrrInfo.loaded && embeddedBrrInfo.blockCount == 1
                         && embeddedBrrInfo.sampleName == "brr-02.brr"
                         && embeddedBrrInfo.statusLine.contains("Using embedded project copy"),
                     "SPC700 deleted-source restore should preserve BRR encoding, selection, and bank order");
    }

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
    auto spcWavProjectXml = stateXmlFromBinary(savedSpcWavState);
    const auto* spcWavProjectBank = spcWavProjectXml != nullptr ? spcWavProjectXml->getChildByName("CHIPPER_SPC700_BRR_BANK") : nullptr;
    ok &= expect(spcWavProjectBank != nullptr
                     && countElementsNamed(*spcWavProjectBank, chipper::state::embeddedSampleStateTag) == 3u,
                 "SPC700 host project state should embed every playable imported PCM slot");
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
    if (spcWavProjectXml != nullptr)
    {
        ChipperAudioProcessor embeddedSpcWavProcessor;
        embeddedSpcWavProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(embeddedSpcWavProcessor.restoreStateXml(*spcWavProjectXml).wasOk(),
                     "SPC700 PCM project state should restore after its source directory is deleted");
        processEmptyBlock(embeddedSpcWavProcessor);
        const auto embeddedPcmNames = embeddedSpcWavProcessor.spc700BrrSampleNames();
        const auto embeddedPcmInfo = embeddedSpcWavProcessor.spc700BrrSampleInfo();
        ok &= expect(embeddedPcmNames.size() == 3 && embeddedPcmInfo.loaded && embeddedPcmInfo.blockCount == 0
                         && embeddedPcmInfo.byteCount == 256 && embeddedPcmInfo.sampleName == "spc-wav-02.wav"
                         && embeddedPcmInfo.statusLine.contains("Using embedded project copy"),
                     "SPC700 deleted-source restore should preserve imported signed-PCM encoding and selection");
    }

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
    auto paulaPresetXml = processor.createStateXml();
    auto paulaProjectXml = stateXmlFromBinary(savedPaulaState);
    ok &= expect(paulaPresetXml != nullptr
                     && countElementsNamed(*paulaPresetXml, chipper::state::embeddedSampleStateTag) == 0u,
                 "Shareable Paula preset XML should remain reference-only");
    const auto* paulaProjectBank = paulaProjectXml != nullptr ? paulaProjectXml->getChildByName("CHIPPER_PAULA_SAMPLE_BANK") : nullptr;
    ok &= expect(paulaProjectBank != nullptr
                     && countElementsNamed(*paulaProjectBank, chipper::state::embeddedSampleStateTag) == 6u,
                 "Paula host project state should embed every playable sample-bank slot");
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
    auto paulaModProjectXml = stateXmlFromBinary(savedPaulaModState);
    ok &= expect(paulaModProjectXml != nullptr
                     && countElementsNamed(*paulaModProjectXml, chipper::state::embeddedSampleStateTag) == 2u,
                 "Paula MOD project state should embed each extracted instrument independently");
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
    if (paulaModProjectXml != nullptr)
    {
        ChipperAudioProcessor embeddedPaulaModProcessor;
        embeddedPaulaModProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(embeddedPaulaModProcessor.restoreStateXml(*paulaModProjectXml).wasOk(),
                     "Paula MOD project state should restore after its module source is deleted");
        processEmptyBlock(embeddedPaulaModProcessor);
        const auto embeddedModNames = embeddedPaulaModProcessor.paulaSampleNames();
        const auto embeddedOneShot = embeddedPaulaModProcessor.paulaSampleInfo();
        ok &= expect(embeddedModNames.size() == 2 && embeddedOneShot.loaded
                         && embeddedOneShot.sampleName.contains("#02 One Shot") && embeddedOneShot.byteCount == 32
                         && embeddedOneShot.statusLine.contains("Using embedded project copy"),
                     "Paula MOD deleted-source restore should preserve the selected one-shot instrument");
        setPlainFromHost(embeddedPaulaModProcessor, chipper::parameters::id::nesDmcSampleSlot, 0.0f);
        processEmptyBlock(embeddedPaulaModProcessor);
        const auto embeddedLoopedMod = embeddedPaulaModProcessor.paulaSampleInfo();
        ok &= expect(embeddedLoopedMod.loaded && embeddedLoopedMod.sampleName.contains("#01 Loop Bass")
                         && embeddedLoopedMod.hasLoop && embeddedLoopedMod.loopStartSample == 16
                         && embeddedLoopedMod.loopEndSample == 48,
                     "Paula MOD embedded restore should preserve source-instrument identity and loop metadata");
    }

    paulaDir.deleteRecursively();
    if (paulaProjectXml != nullptr)
    {
        ChipperAudioProcessor embeddedPaulaProcessor;
        embeddedPaulaProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(embeddedPaulaProcessor.restoreStateXml(*paulaProjectXml).wasOk(),
                     "Paula project state should restore after its sample directory is deleted");
        processEmptyBlock(embeddedPaulaProcessor);
        const auto embeddedPaulaNames = embeddedPaulaProcessor.paulaSampleNames();
        const auto embeddedPaulaInfo = embeddedPaulaProcessor.paulaSampleInfo();
        ok &= expect(embeddedPaulaNames.size() == 6 && embeddedPaulaInfo.loaded
                         && embeddedPaulaInfo.sampleName == "paula-03.wav"
                         && embeddedPaulaInfo.hasLoop && embeddedPaulaInfo.loopStartSample == 32
                         && embeddedPaulaInfo.loopEndSample == 160
                         && embeddedPaulaInfo.statusLine.contains("Using embedded project copy"),
                     "Paula deleted-source restore should preserve PCM encoding, bank order, selection, and loop metadata");
    }

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

    ChipperAudioProcessor tailProcessor;
    tailProcessor.prepareToPlay(48000.0, 64);
    ok &= expect(tailProcessor.getTailLengthSeconds() >= 2.0,
                 "NES processor tail should account for envelope and sample completion");
    setPlainFromHost(tailProcessor, chipper::parameters::id::chipMode, 2.0f);
    setPlainFromHost(tailProcessor, chipper::parameters::id::sidRelease, 16.0f);
    setPlainFromHost(tailProcessor, chipper::parameters::id::sidVoice2Release, 16.0f);
    setPlainFromHost(tailProcessor, chipper::parameters::id::sidVoice3Release, 16.0f);
    ok &= expect(tailProcessor.getTailLengthSeconds() >= 24.0,
                 "SID processor tail should include the longest selected release nibble");

    ChipperAudioProcessor waveStateProcessor;
    waveStateProcessor.prepareToPlay(48000.0, 64);
    setPlainFromHost(waveStateProcessor, chipper::parameters::id::chipMode,
                     static_cast<float>(static_cast<int>(chipper::ChipMode::huc6280)));
    chipper::WavetableLane customWave {};
    customWave[0] = 31u;
    customWave[15] = 12u;
    customWave[31] = 7u;
    ok &= expect(waveStateProcessor.setWavetableLane(chipper::ChipMode::huc6280, 0u, customWave),
                 "Processor should accept a native HuC6280 custom Wave RAM lane");
    const auto customSnapshot = waveStateProcessor.wavetableSnapshot(chipper::ChipMode::huc6280, 0u);
    ok &= expect(customSnapshot.custom && customSnapshot.samples == customWave,
                 "Processor Wave RAM snapshot should expose the custom native samples");
    processEmptyBlock(waveStateProcessor);
    const auto customWaveDebug = waveStateProcessor.currentCoreDebugStateJson();
    ok &= expect(jsonIntValue(customWaveDebug, "waveRam0") == 31
                     && jsonIntValue(customWaveDebug, "waveRam31") == 7,
                 "Processor custom Wave RAM should reach the active HuC6280 core");
    const auto waveStateXml = waveStateProcessor.createStateXml();
    ok &= expect(waveStateXml != nullptr
                     && waveStateXml->getChildByName(chipper::state::wavetableStateTag) != nullptr,
                 "Saved processor state should embed custom Wave RAM");
    if (waveStateXml != nullptr)
    {
        ChipperAudioProcessor waveRestoreProcessor;
        waveRestoreProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(waveRestoreProcessor.restoreStateXml(*waveStateXml).wasOk(),
                     "Custom Wave RAM state should restore successfully");
        const auto restoredWave = waveRestoreProcessor.wavetableSnapshot(chipper::ChipMode::huc6280, 0u);
        ok &= expect(restoredWave.custom && restoredWave.samples == customWave,
                     "Custom Wave RAM should survive a host-state round trip exactly");
        processEmptyBlock(waveRestoreProcessor);
        const auto restoredWaveDebug = waveRestoreProcessor.currentCoreDebugStateJson();
        ok &= expect(jsonIntValue(restoredWaveDebug, "waveRam0") == 31
                         && jsonIntValue(restoredWaveDebug, "waveRam31") == 7,
                     "Restored custom Wave RAM should reach the active HuC6280 core");

        auto invalidWaveState = std::make_unique<juce::XmlElement>(*waveStateXml);
        if (auto* waveBank = invalidWaveState->getChildByName(chipper::state::wavetableStateTag))
            if (auto* wave = waveBank->getChildByName(chipper::state::wavetableLaneStateTag))
                wave->setAttribute("data", "FF");
        ChipperAudioProcessor invalidWaveRestoreProcessor;
        invalidWaveRestoreProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(invalidWaveRestoreProcessor.restoreStateXml(*invalidWaveState).failed(),
                     "Malformed custom Wave RAM state should fail explicitly");
    }

    {
        ChipperAudioProcessor detuneStateSource;
        detuneStateSource.prepareToPlay(48000.0, 64);
        setPlainFromHost(detuneStateSource, chipper::parameters::id::chipMode, 12.0f);
        setPlainFromHost(detuneStateSource, chipper::parameters::id::fmOperator1Multiplier, 1.0f);
        setPlainFromHost(detuneStateSource, chipper::parameters::id::fmOperator1SustainRate, 6.0f);
        setPlainFromHost(detuneStateSource, chipper::parameters::id::opmOperator1Dt1, 4.0f);
        setPlainFromHost(detuneStateSource, chipper::parameters::id::opmOperator1Dt2, 3.0f);
        sendNoteOn(detuneStateSource, 69);
        const auto detuneStateSourceDebug = detuneStateSource.currentCoreDebugStateJson();
        auto detuneStateXml = detuneStateSource.createStateXml();
        ok &= expect(detuneStateXml != nullptr,
                     "Current-schema state should save explicit YM2151 DT1/DT2 choices");
        if (detuneStateXml != nullptr)
        {
            ChipperAudioProcessor detuneStateRestored;
            detuneStateRestored.prepareToPlay(48000.0, 64);
            ok &= expect(detuneStateRestored.restoreStateXml(*detuneStateXml).wasOk(),
                         "Current-schema state should restore explicit YM2151 DT1/DT2 choices");
            ok &= expectNear(parameterValue(detuneStateRestored, chipper::parameters::id::opmOperator1Dt1),
                             4.0f, 0.001f, "YM2151 DT1 should survive a current-schema state round trip");
            ok &= expectNear(parameterValue(detuneStateRestored, chipper::parameters::id::opmOperator1Dt2),
                             3.0f, 0.001f, "YM2151 DT2 should survive a current-schema state round trip");
            sendNoteOn(detuneStateRestored, 69);
            const auto detuneStateRestoredDebug = detuneStateRestored.currentCoreDebugStateJson();
            ok &= expect(jsonIntValue(detuneStateRestoredDebug, "operatorMultipleDt1Register0") == 0x30
                             && jsonIntValue(detuneStateRestoredDebug, "operatorDt2SustainRateRegister0") == 0x85
                             && jsonIntValue(detuneStateRestoredDebug, "operatorMultipleDt1Register0")
                                 == jsonIntValue(detuneStateSourceDebug, "operatorMultipleDt1Register0")
                             && jsonIntValue(detuneStateRestoredDebug, "operatorDt2SustainRateRegister0")
                                 == jsonIntValue(detuneStateSourceDebug, "operatorDt2SustainRateRegister0"),
                         "Restored YM2151 DT1/DT2 should reproduce the exact packed $40/$C0 bytes");
        }
    }
    {
        ChipperAudioProcessor oplStateSource;
        oplStateSource.prepareToPlay(48000.0, 64);
        setPlainFromHost(oplStateSource, chipper::parameters::id::chipMode, 6.0f);
        const std::array<const char*, 8> nativeFieldIds {
            chipper::parameters::id::opmOperator1Dt1, chipper::parameters::id::opmOperator2Dt1,
            chipper::parameters::id::opmOperator3Dt1, chipper::parameters::id::opmOperator4Dt1,
            chipper::parameters::id::opmOperator1Dt2, chipper::parameters::id::opmOperator2Dt2,
            chipper::parameters::id::opmOperator3Dt2, chipper::parameters::id::opmOperator4Dt2
        };
        const std::array<float, 8> nativeFieldValues { 8.0f, 2.0f, 3.0f, 5.0f, 2.0f, 3.0f, 4.0f, 1.0f };
        for (size_t i = 0; i < nativeFieldIds.size(); ++i)
            setPlainFromHost(oplStateSource, nativeFieldIds[i], nativeFieldValues[i]);

        auto oplStateXml = oplStateSource.createStateXml();
        ok &= expect(oplStateXml != nullptr,
                     "Current-schema state should save explicit OPL3 AM/VIB/KSR and KSL choices");
        if (oplStateXml != nullptr)
        {
            ChipperAudioProcessor oplStateRestored;
            oplStateRestored.prepareToPlay(48000.0, 64);
            ok &= expect(oplStateRestored.restoreStateXml(*oplStateXml).wasOk(),
                         "Current-schema state should restore explicit OPL3 operator-field choices");
            for (size_t i = 0; i < nativeFieldIds.size(); ++i)
                ok &= expectNear(parameterValue(oplStateRestored, nativeFieldIds[i]), nativeFieldValues[i], 0.001f,
                                 "OPL3 operator-field choice should survive a current-schema round trip");
            sendNoteOn(oplStateRestored, 60);
            const auto restoredNativeDebug = oplStateRestored.currentCoreDebugStateJson();
            ok &= expect(jsonIntValue(restoredNativeDebug, "operatorFlagChoice0") == 8
                             && jsonIntValue(restoredNativeDebug, "operatorFlagChoice3") == 5
                             && jsonIntValue(restoredNativeDebug, "operatorKslChoice0") == 2
                             && jsonIntValue(restoredNativeDebug, "operatorKslChoice3") == 1,
                         "Restored OPL3 operator fields should reproduce the saved raw choices");
        }
    }
    auto versionedState = processor.createStateXml();
    ok &= expect(versionedState != nullptr && versionedState->getIntAttribute("stateSchemaVersion") == chipper::state::currentSchemaVersion,
                 "Saved processor state should declare the current schema version");
    if (versionedState != nullptr)
    {
        auto schema8Opn2State = std::make_unique<juce::XmlElement>(*versionedState);
        schema8Opn2State->setAttribute(chipper::state::schemaVersionAttribute, 8);
        while (auto* existing = schema8Opn2State->getChildByName("CHIPPER_OPN2_DAC_SAMPLE"))
            schema8Opn2State->removeChildElement(existing, true);
        auto* legacyOpn2Sample = new juce::XmlElement("CHIPPER_OPN2_DAC_SAMPLE");
        legacyOpn2Sample->setAttribute("path", "legacy-opn2.raw");
        schema8Opn2State->addChildElement(legacyOpn2Sample);
        const auto schema8Migration = chipper::state::validateAndMigrate(
            *schema8Opn2State, juce::Identifier(schema8Opn2State->getTagName()));
        const auto* migratedOpn2Sample = schema8Opn2State->getChildByName("CHIPPER_OPN2_DAC_SAMPLE");
        ok &= expect(schema8Migration.wasOk()
                         && schema8Opn2State->getIntAttribute(chipper::state::schemaVersionAttribute)
                             == chipper::state::currentSchemaVersion
                         && migratedOpn2Sample != nullptr
                         && migratedOpn2Sample->getDoubleAttribute("sourceRateHz", -1.0) == 0.0
                         && migratedOpn2Sample->getIntAttribute("rootNote", -1) == 60
                         && migratedOpn2Sample->getIntAttribute("trimStart", -1) == 0
                         && migratedOpn2Sample->getIntAttribute("trimEnd", -1) == 0
                         && migratedOpn2Sample->getStringAttribute("tailBehavior") == "center",
                     "Schema-v8 OPN2 DAC references should migrate to explicit legacy playback semantics");

        auto legacyState = std::make_unique<juce::XmlElement>(*versionedState);
        legacyState->removeAttribute("stateSchemaVersion");
        ChipperAudioProcessor legacyRestoreProcessor;
        legacyRestoreProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(legacyRestoreProcessor.restoreStateXml(*legacyState).wasOk(),
                     "Unversioned schema-1 state should migrate successfully");
        const auto migratedState = legacyRestoreProcessor.createStateXml();
        ok &= expect(migratedState != nullptr && migratedState->getIntAttribute("stateSchemaVersion") == chipper::state::currentSchemaVersion,
                     "Migrated state should be re-saved at the current schema version");
        ChipperAudioProcessor opmLegacySource;
        opmLegacySource.prepareToPlay(48000.0, 64);
        auto legacyOpmState = opmLegacySource.createStateXml();
        ok &= expect(legacyOpmState != nullptr, "Should create legacy OPM migration fixture state");
        if (legacyOpmState != nullptr)
        {
            legacyOpmState->setAttribute(chipper::state::schemaVersionAttribute, 4);
            int removedOpmParameters = 0;
            for (auto* child = legacyOpmState->getFirstChildElement(); child != nullptr;)
            {
                auto* next = child->getNextElement();
                const auto id = child->getStringAttribute("id");
                if (id == chipper::parameters::id::opmLfoWaveform
                    || id == chipper::parameters::id::opmLfoPms
                    || id == chipper::parameters::id::opmLfoAms)
                {
                    legacyOpmState->removeChildElement(child, true);
                    ++removedOpmParameters;
                }
                child = next;
            }
            ok &= expect(removedOpmParameters == 3, "Legacy OPM migration fixture should omit all three schema-v5 choices");
            ChipperAudioProcessor staleOpmProcessor;
            staleOpmProcessor.prepareToPlay(48000.0, 64);
            setPlainFromHost(staleOpmProcessor, chipper::parameters::id::opmLfoWaveform, 4.0f);
            setPlainFromHost(staleOpmProcessor, chipper::parameters::id::opmLfoPms, 8.0f);
            setPlainFromHost(staleOpmProcessor, chipper::parameters::id::opmLfoAms, 4.0f);
            ok &= expect(parameterValue(staleOpmProcessor, chipper::parameters::id::opmLfoWaveform) == 4.0f
                             && parameterValue(staleOpmProcessor, chipper::parameters::id::opmLfoPms) == 8.0f
                             && parameterValue(staleOpmProcessor, chipper::parameters::id::opmLfoAms) == 4.0f,
                         "OPM migration regression setup should begin with stale direct choices");
            ok &= expect(staleOpmProcessor.restoreStateXml(*legacyOpmState).wasOk(),
                         "Schema-v4 state missing direct OPM choices should migrate successfully");
            ok &= expectNear(parameterValue(staleOpmProcessor, chipper::parameters::id::opmLfoWaveform), 0.0f, 0.001f, "Schema-v4 migration should reset OPM waveform to Preset");
            ok &= expectNear(parameterValue(staleOpmProcessor, chipper::parameters::id::opmLfoPms), 0.0f, 0.001f, "Schema-v4 migration should reset OPM PMS to Preset");
            ok &= expectNear(parameterValue(staleOpmProcessor, chipper::parameters::id::opmLfoAms), 0.0f, 0.001f, "Schema-v4 migration should reset OPM AMS to Preset");
        }

        auto legacyDetuneState = std::make_unique<juce::XmlElement>(*versionedState);
        legacyDetuneState->setAttribute(chipper::state::schemaVersionAttribute, 5);
        const std::array<const char*, 8> opmDetuneIds {
            chipper::parameters::id::opmOperator1Dt1, chipper::parameters::id::opmOperator2Dt1,
            chipper::parameters::id::opmOperator3Dt1, chipper::parameters::id::opmOperator4Dt1,
            chipper::parameters::id::opmOperator1Dt2, chipper::parameters::id::opmOperator2Dt2,
            chipper::parameters::id::opmOperator3Dt2, chipper::parameters::id::opmOperator4Dt2
        };
        int removedDetuneParameters = 0;
        for (auto* child = legacyDetuneState->getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();
            const auto id = child->getStringAttribute("id");
            const auto isDetuneParameter = std::any_of(opmDetuneIds.begin(), opmDetuneIds.end(),
                                                       [&id](const char* parameterId) { return id == parameterId; });
            if (isDetuneParameter)
            {
                legacyDetuneState->removeChildElement(child, true);
                ++removedDetuneParameters;
            }
            child = next;
        }
        ok &= expect(removedDetuneParameters == 8,
                     "Schema-v5 detune migration fixture should omit all eight YM2151 DT1/DT2 choices");
        ChipperAudioProcessor staleDetuneProcessor;
        staleDetuneProcessor.prepareToPlay(48000.0, 64);
        for (const auto* parameterId : opmDetuneIds)
            setPlainFromHost(staleDetuneProcessor, parameterId, 1.0f);
        ok &= expect(std::all_of(opmDetuneIds.begin(), opmDetuneIds.end(),
                                 [&staleDetuneProcessor](const char* parameterId) {
                                     return parameterValue(staleDetuneProcessor, parameterId) > 0.0f;
                                 }),
                     "Schema-v5 detune migration setup should begin with stale non-Preset values");
        ok &= expect(staleDetuneProcessor.restoreStateXml(*legacyDetuneState).wasOk(),
                     "Schema-v5 state missing YM2151 detune choices should migrate successfully");
        ok &= expect(std::all_of(opmDetuneIds.begin(), opmDetuneIds.end(),
                                 [&staleDetuneProcessor](const char* parameterId) {
                                     return std::abs(parameterValue(staleDetuneProcessor, parameterId)) < 0.001f;
                                 }),
                     "Schema-v5 migration should reset every YM2151 DT1/DT2 choice to Preset");

        ChipperAudioProcessor schema6OplSource;
        schema6OplSource.prepareToPlay(48000.0, 64);
        setPlainFromHost(schema6OplSource, chipper::parameters::id::chipMode, 6.0f);
        for (const auto* parameterId : opmDetuneIds)
            setPlainFromHost(schema6OplSource, parameterId, 1.0f);
        auto schema6OplState = schema6OplSource.createStateXml();
        ok &= expect(schema6OplState != nullptr, "Should create schema-v6 OPL3 migration fixture state");
        if (schema6OplState != nullptr)
        {
            schema6OplState->setAttribute(chipper::state::schemaVersionAttribute, 6);
            ChipperAudioProcessor schema6OplRestored;
            schema6OplRestored.prepareToPlay(48000.0, 64);
            ok &= expect(schema6OplRestored.restoreStateXml(*schema6OplState).wasOk(),
                         "Schema-v6 OPL3 state should migrate successfully");
            ok &= expect(std::all_of(opmDetuneIds.begin(), opmDetuneIds.end(),
                                     [&schema6OplRestored](const char* parameterId) {
                                         return std::abs(parameterValue(schema6OplRestored, parameterId)) < 0.001f;
                                     }),
                         "Schema-v7 migration should neutralize latent non-YM2151 DT values before OPL3 reuses them");
        }

        ChipperAudioProcessor schema6OpmSource;
        schema6OpmSource.prepareToPlay(48000.0, 64);
        setPlainFromHost(schema6OpmSource, chipper::parameters::id::chipMode, 12.0f);
        for (const auto* parameterId : opmDetuneIds)
            setPlainFromHost(schema6OpmSource, parameterId, 1.0f);
        auto schema6OpmState = schema6OpmSource.createStateXml();
        ok &= expect(schema6OpmState != nullptr, "Should create schema-v6 YM2151 migration fixture state");
        if (schema6OpmState != nullptr)
        {
            schema6OpmState->setAttribute(chipper::state::schemaVersionAttribute, 6);
            ChipperAudioProcessor schema6OpmRestored;
            schema6OpmRestored.prepareToPlay(48000.0, 64);
            ok &= expect(schema6OpmRestored.restoreStateXml(*schema6OpmState).wasOk(),
                         "Schema-v6 YM2151 state should migrate successfully");
            ok &= expect(std::all_of(opmDetuneIds.begin(), opmDetuneIds.end(),
                                     [&schema6OpmRestored](const char* parameterId) {
                                         return std::abs(parameterValue(schema6OpmRestored, parameterId) - 1.0f) < 0.001f;
                                     }),
                         "Schema-v7 migration should preserve genuine YM2151 DT1/DT2 values");
        }

        auto futureState = std::make_unique<juce::XmlElement>(*versionedState);
        futureState->setAttribute("stateSchemaVersion", 999);
        ChipperAudioProcessor futureRestoreProcessor;
        futureRestoreProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(futureRestoreProcessor.restoreStateXml(*futureState).failed(),
                     "State from a newer unsupported schema should fail explicitly");

        auto oversizedRegisterState = std::make_unique<juce::XmlElement>(*versionedState);
        if (auto* existingCoreState = oversizedRegisterState->getChildByName("CHIPPER_CORE_REGISTERS"))
            oversizedRegisterState->removeChildElement(existingCoreState, true);
        auto* coreState = new juce::XmlElement("CHIPPER_CORE_REGISTERS");
        for (size_t index = 0; index <= chipper::state::maxRestoredRegisterWrites; ++index)
        {
            auto* registerState = new juce::XmlElement("REG");
            registerState->setAttribute("address", static_cast<int>(index & 0xffffu));
            registerState->setAttribute("value", static_cast<int>(index & 0xffu));
            coreState->addChildElement(registerState);
        }
        oversizedRegisterState->addChildElement(coreState);
        ChipperAudioProcessor oversizedRegisterRestoreProcessor;
        oversizedRegisterRestoreProcessor.prepareToPlay(48000.0, 64);
        ok &= expect(oversizedRegisterRestoreProcessor.restoreStateXml(*oversizedRegisterState).failed(),
                     "State with an excessive register-write payload should fail explicitly");
    }

    for (const auto& fixture : {
             std::pair { "legacy-v1-minimal.xml", true },
             std::pair { "current-v4-minimal.xml", true },
             std::pair { "current-v3-minimal.xml", true },
             std::pair { "current-v2-minimal.xml", true },
             std::pair { "future-v999.xml", false },
             std::pair { "invalid-version.xml", false },
             std::pair { "wrong-root.xml", false },
         })
    {
        auto fixtureXml = loadStateFixture(fixture.first);
        ok &= expect(fixtureXml != nullptr, std::string("Should parse state fixture ") + fixture.first);
        if (fixtureXml != nullptr)
        {
            const auto migration = chipper::state::validateAndMigrate(*fixtureXml, juce::Identifier("ChipperState"));
            ok &= expect(migration.wasOk() == fixture.second,
                         std::string("State fixture should have expected compatibility result: ") + fixture.first);
            if (migration.wasOk())
                ok &= expect(fixtureXml->getIntAttribute(chipper::state::schemaVersionAttribute)
                                 == chipper::state::currentSchemaVersion,
                             std::string("Accepted state fixture should normalize to current schema: ") + fixture.first);
        }
    }

    auto expectedParameterIds = loadParameterIdContract();
    std::vector<std::string> actualParameterIds;
    for (const auto* parameter : processor.getParameters())
    {
        if (const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter))
            actualParameterIds.push_back(identified->paramID.toStdString());
    }
    std::sort(expectedParameterIds.begin(), expectedParameterIds.end());
    std::sort(actualParameterIds.begin(), actualParameterIds.end());
    ok &= expect(! expectedParameterIds.empty(), "Parameter-ID compatibility fixture should not be empty");
    ok &= expect(actualParameterIds == expectedParameterIds,
                 "Every host automation parameter ID should match the checked-in compatibility fixture");

    auto monoLayout = processor.getBusesLayout();
    monoLayout.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    ok &= expect(processor.isBusesLayoutSupported(monoLayout),
                 "Host bus contract should continue to support one mono output bus");
    auto stereoLayout = processor.getBusesLayout();
    stereoLayout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();
    ok &= expect(processor.isBusesLayoutSupported(stereoLayout),
                 "Host bus contract should continue to support one stereo output bus");
    auto surroundLayout = processor.getBusesLayout();
    surroundLayout.outputBuses.getReference(0) = juce::AudioChannelSet::create5point1();
    ok &= expect(! processor.isBusesLayoutSupported(surroundLayout),
                 "Host bus contract should reject undeclared surround or multi-output layouts");

    const auto portableAssetRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getNonexistentChildFile("chipper-portable-asset-schema", {}, false);
    const auto portableSamples = portableAssetRoot.getChildFile("Samples");
    ok &= expect(portableSamples.createDirectory().wasOk(), "Should create portable asset schema fixture folder");
    const auto portableAsset = portableSamples.getChildFile("asset.bin");
    ok &= expect(writeDmcFixture(portableAsset, 0x55u), "Should create portable asset schema fixture");
    juce::XmlElement portableAssetState("STATE");
    for (const auto* tag : { "DMC_SAMPLE", "BRR_SAMPLE", "PAULA_SAMPLE", "CHIPPER_SPC700_BRR",
                             "CHIPPER_OPN2_DAC_SAMPLE", "CHIPPER_OPNA_RHYTHM_ROM", "CHIPPER_OPNA_ADPCM_B_SAMPLE",
                             "CHIPPER_OPNB_ADPCM_A_SAMPLE", "CHIPPER_OPNB_ADPCM_B_SAMPLE" })
    {
        auto* asset = new juce::XmlElement(tag);
        asset->setAttribute("path", portableAssetRoot.getSiblingFile("elsewhere").getChildFile("asset.bin").getFullPathName());
        portableAssetState.addChildElement(asset);
    }
    chipper::state::annotatePortableAssetReferences(portableAssetState, portableAssetRoot);
    for (const auto* asset : portableAssetState.getChildIterator())
        ok &= expect(asset != nullptr && asset->getStringAttribute("relativePath") == "Samples/asset.bin",
                     "Every external asset type should receive a portable Samples reference");

    const auto escapedAsset = portableAssetRoot.getParentDirectory().getChildFile("escaped-asset.dmc");
    ok &= expect(writeDmcFixture(escapedAsset, 0xaau), "Should create path traversal guard fixture");
    juce::XmlElement traversalState("DMC_SAMPLE");
    traversalState.setAttribute("path", portableAssetRoot.getChildFile("missing.dmc").getFullPathName());
    traversalState.setAttribute("relativePath", "../escaped-asset.dmc");
    ok &= expect(chipper::state::resolvePortableAssetPath(traversalState, portableAssetRoot) != escapedAsset,
                 "Portable asset resolution should reject paths that escape the preset directory");
    escapedAsset.deleteFile();
    portableAssetRoot.deleteRecursively();

    ok &= expectMotionPlaybackAndState();
    ok &= expectSteadyStateProcessingDoesNotAllocate();
    ok &= expectMappedSampleNoteProcessingDoesNotAllocate();
    ok &= expectConcurrentSampleMutationDoesNotDeadlock();

    return ok ? 0 : 1;
}

int main()
{
    // Build the large static descriptor table on a shallow stack. The smoke
    // body intentionally keeps many processor fixtures alive in one function.
    (void) chipper::descriptorFor(chipper::ChipMode::ym2610b);
    if (! expectYamahaAdpcmARegionBanks())
        return 1;
    return runSmoke();
}
