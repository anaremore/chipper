#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <new>
#include <numeric>
#include <string>
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
using Clock = std::chrono::steady_clock;

constexpr double testSampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int patternsPerCapture = 8;
constexpr int steadyBlocksPerMode = 64;
constexpr int modeSwitchBlocks = 1024;

struct Capture
{
    std::vector<double> callbackMilliseconds;
    std::vector<size_t> allocationsByMode;
    size_t allocations = 0;
    size_t nonFiniteSamples = 0;
    size_t deadlineMisses = 0;
    float peak = 0.0f;
};

void setPlainFromHost(ChipperAudioProcessor& processor, const char* parameterId, float plainValue)
{
    if (auto* parameter = processor.getValueTreeState().getParameter(parameterId))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

std::array<juce::MidiBuffer, patternsPerCapture> makeDenseMidiPatterns()
{
    std::array<juce::MidiBuffer, patternsPerCapture> patterns;

    for (int patternIndex = 0; patternIndex < patternsPerCapture; ++patternIndex)
    {
        auto& midi = patterns[static_cast<size_t>(patternIndex)];
        const auto baseNote = 36 + patternIndex * 3;

        for (int lane = 0; lane < 8; ++lane)
        {
            const auto offset = lane * 32;
            const auto note = juce::jlimit(0, 127, baseNote + lane * 2);
            midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.35f + 0.08f * static_cast<float>(lane)), offset);
            midi.addEvent(juce::MidiMessage::noteOff(1, note), juce::jmin(blockSize - 1, offset + 24));
        }

        midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, (patternIndex * 17) & 0x7f), 7);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, (127 - patternIndex * 11) & 0x7f), 71);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 76, (patternIndex * 23) & 0x7f), 135);
        midi.addEvent(juce::MidiMessage::pitchWheel(1, 4096 + patternIndex * 1024), 199);
    }

    return patterns;
}

void inspectOutput(const juce::AudioBuffer<float>& buffer, Capture& capture)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* samples = buffer.getReadPointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = samples[sample];
            if (! std::isfinite(value))
                ++capture.nonFiniteSamples;
            else
                capture.peak = std::max(capture.peak, std::abs(value));
        }
    }
}

void processMeasuredBlock(ChipperAudioProcessor& processor,
                          juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midi,
                          Capture& capture)
{
    buffer.clear();
    allocation_probe::count.store(0u, std::memory_order_relaxed);
    allocation_probe::enabled.store(true, std::memory_order_release);
    const auto start = Clock::now();
    processor.processBlock(buffer, midi);
    const auto finish = Clock::now();
    allocation_probe::enabled.store(false, std::memory_order_release);

    const auto elapsed = std::chrono::duration<double, std::milli>(finish - start).count();
    const auto deadline = 1000.0 * static_cast<double>(blockSize) / testSampleRate;
    capture.callbackMilliseconds.push_back(elapsed);
    capture.allocations += allocation_probe::count.load(std::memory_order_relaxed);
    capture.deadlineMisses += static_cast<size_t>(elapsed > deadline);
    inspectOutput(buffer, capture);
}

Capture captureSteadyState(ChipperAudioProcessor& processor,
                           std::array<juce::MidiBuffer, patternsPerCapture>& patterns)
{
    Capture capture;
    const auto chipCount = chipper::parameters::chipModeChoices().size();
    capture.callbackMilliseconds.reserve(static_cast<size_t>(chipCount * steadyBlocksPerMode));
    capture.allocationsByMode.resize(static_cast<size_t>(chipCount), 0u);
    juce::AudioBuffer<float> buffer(2, blockSize);

    for (int chipChoice = 0; chipChoice < chipCount; ++chipChoice)
    {
        const auto allocationsBefore = capture.allocations;
        setPlainFromHost(processor, chipper::parameters::id::chipMode, static_cast<float>(chipChoice));
        processor.processBlock(buffer, patterns.front());

        for (int block = 0; block < steadyBlocksPerMode; ++block)
            processMeasuredBlock(processor, buffer, patterns[static_cast<size_t>(block % patternsPerCapture)], capture);
        capture.allocationsByMode[static_cast<size_t>(chipChoice)] = capture.allocations - allocationsBefore;
    }

    return capture;
}

Capture captureModeSwitches(ChipperAudioProcessor& processor,
                            std::array<juce::MidiBuffer, patternsPerCapture>& patterns)
{
    Capture capture;
    capture.callbackMilliseconds.reserve(modeSwitchBlocks);
    juce::AudioBuffer<float> buffer(2, blockSize);
    const auto chipCount = chipper::parameters::chipModeChoices().size();
    capture.allocationsByMode.resize(static_cast<size_t>(chipCount), 0u);

    for (int block = 0; block < modeSwitchBlocks; ++block)
    {
        const auto chipChoice = (block * 11 + 3) % chipCount;
        const auto allocationsBefore = capture.allocations;
        setPlainFromHost(processor, chipper::parameters::id::chipMode, static_cast<float>(chipChoice));
        processMeasuredBlock(processor, buffer, patterns[static_cast<size_t>(block % patternsPerCapture)], capture);
        capture.allocationsByMode[static_cast<size_t>(chipChoice)] += capture.allocations - allocationsBefore;
    }

    return capture;
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const auto index = static_cast<size_t>(std::ceil(fraction * static_cast<double>(values.size())) - 1.0);
    return values[std::min(index, values.size() - 1u)];
}

bool reportAndValidate(const char* label, const Capture& capture)
{
    const auto deadline = 1000.0 * static_cast<double>(blockSize) / testSampleRate;
    const auto total = std::accumulate(capture.callbackMilliseconds.begin(),
                                       capture.callbackMilliseconds.end(),
                                       0.0);
    const auto mean = capture.callbackMilliseconds.empty()
                          ? 0.0
                          : total / static_cast<double>(capture.callbackMilliseconds.size());
    const auto maximum = capture.callbackMilliseconds.empty()
                             ? 0.0
                             : *std::max_element(capture.callbackMilliseconds.begin(),
                                                 capture.callbackMilliseconds.end());
    const auto p99 = percentile(capture.callbackMilliseconds, 0.99);
    const auto realtimeLoad = deadline > 0.0 ? 100.0 * mean / deadline : 0.0;

    std::cout << std::fixed << std::setprecision(4)
              << "PERFORMANCE_CAPTURE"
              << " phase=" << label
              << " sampleRate=" << testSampleRate
              << " blockSize=" << blockSize
              << " callbacks=" << capture.callbackMilliseconds.size()
              << " allocations=" << capture.allocations
              << " deadlineMisses=" << capture.deadlineMisses
              << " nonFiniteSamples=" << capture.nonFiniteSamples
              << " meanMs=" << mean
              << " p99Ms=" << p99
              << " maxMs=" << maximum
              << " deadlineMs=" << deadline
              << " meanRealtimeLoadPercent=" << realtimeLoad
              << " peak=" << capture.peak
              << '\n';

    const auto modeNames = chipper::parameters::chipModeChoices();
    for (size_t mode = 0; mode < capture.allocationsByMode.size()
                          && mode < static_cast<size_t>(modeNames.size()); ++mode)
    {
        if (capture.allocationsByMode[mode] != 0u)
            std::cerr << "ALLOCATION_DETAIL phase=" << label << " mode=" << modeNames[static_cast<int>(mode)]
                      << " allocations=" << capture.allocationsByMode[mode] << '\n';
    }

    auto ok = true;
    if (capture.allocations != 0u)
    {
        std::cerr << label << " allocated " << capture.allocations << " times inside processBlock\n";
        ok = false;
    }
    if (capture.deadlineMisses != 0u)
    {
        std::cerr << label << " missed the " << deadline << " ms callback deadline "
                  << capture.deadlineMisses << " times\n";
        ok = false;
    }
    if (capture.nonFiniteSamples != 0u)
    {
        std::cerr << label << " produced " << capture.nonFiniteSamples << " non-finite samples\n";
        ok = false;
    }
    if (capture.peak > 1.0001f)
    {
        std::cerr << label << " exceeded the normalized output range with peak " << capture.peak << '\n';
        ok = false;
    }

    return ok;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    ChipperAudioProcessor processor;
    processor.prepareToPlay(testSampleRate, blockSize);
    auto patterns = makeDenseMidiPatterns();
    juce::AudioBuffer<float> warmupBuffer(2, blockSize);

    for (int chipChoice = 0; chipChoice < chipper::parameters::chipModeChoices().size(); ++chipChoice)
    {
        setPlainFromHost(processor, chipper::parameters::id::chipMode, static_cast<float>(chipChoice));
        processor.processBlock(warmupBuffer, patterns.front());
        processor.processBlock(warmupBuffer, patterns[1]);
    }

    const auto steadyState = captureSteadyState(processor, patterns);
    const auto modeSwitches = captureModeSwitches(processor, patterns);
    auto ok = reportAndValidate("steady-dense-midi", steadyState);
    ok = reportAndValidate("mode-switch-dense-midi", modeSwitches) && ok;
    return ok ? 0 : 1;
}
