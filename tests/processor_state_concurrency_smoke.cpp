#include "PluginProcessor.h"
#include "State/PluginStateSchema.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    auto ok = true;
    const auto expect = [&ok](bool condition, const char* message)
    {
        if (! condition) { std::cerr << message << '\n'; ok = false; }
    };
    chipper::state::SharedSampleBytes original(std::vector<uint8_t>(262144, 0xaa));
    auto retained = original;
    expect(original.data() == retained.data(), "Sample snapshots must retain immutable storage without copying");
    original = std::vector<uint8_t>(262144, 0x55);
    expect(retained[0] == 0xaa && original[0] == 0x55, "Replacing an asset must not mutate a retained snapshot");

    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getNonexistentChildFile("chipper-concurrent-save", {}, false);
    if (! directory.createDirectory().wasOk()) return 1;
    struct Cleanup { juce::File directory; ~Cleanup() { directory.deleteRecursively(); } } cleanup { directory };
    for (int index = 0; index < 32; ++index)
    {
        const auto file = directory.getChildFile(juce::String(index) + ".dmc");
        if (! file.replaceWithData(retained.data(), retained.size())) return 1;
    }

    ChipperAudioProcessor processor;
    processor.prepareToPlay(48000, 256);
    if (processor.loadNesDmcSampleDirectory(directory).failed()) return 1;
    for (const auto blockSize : { 64, 128, 256 })
    {
        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, 0.8f), 0);
        processor.processBlock(buffer, midi);
        midi.clear();
        std::atomic<bool> ready { false }, started { false }, finished { false };
        std::unique_ptr<juce::XmlElement> state;
        double saveMs = 0.0;
        std::thread saver([&]
        {
            ready.store(true, std::memory_order_release);
            while (! started.load(std::memory_order_acquire)) std::this_thread::yield();
            const auto before = std::chrono::steady_clock::now();
            state = processor.createStateXml(ChipperAudioProcessor::StateAssetPolicy::embedProjectAssets);
            saveMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count();
            finished.store(true, std::memory_order_release);
        });
        while (! ready.load(std::memory_order_acquire)) std::this_thread::yield();
        started.store(true, std::memory_order_release);
        auto concurrentCallbacks = 0;
        auto maxWaitMs = 0.0;
        auto totalWaitMs = 0.0;
        auto finite = true;
        while (! finished.load(std::memory_order_acquire))
        {
            const auto before = std::chrono::steady_clock::now();
            {
                // Match the VST3 wrapper's lock, rather than measuring only
                // direct processBlock calls that cannot see save contention.
                const juce::ScopedLock lock(processor.getCallbackLock());
                const auto acquired = std::chrono::steady_clock::now();
                const auto waitMs = std::chrono::duration<double, std::milli>(acquired - before).count();
                maxWaitMs = std::max(maxWaitMs, waitMs);
                totalWaitMs += waitMs;
                processor.processBlock(buffer, midi);
                if (! finished.load(std::memory_order_acquire)) ++concurrentCallbacks;
            }
            for (int channel = 0; channel < 2; ++channel)
                for (int sample = 0; sample < blockSize; ++sample)
                    finite &= std::isfinite(buffer.getSample(channel, sample));
        }
        saver.join();
        expect(finite, "Concurrent save produced non-finite audio");
        // Scheduler throughput is not the property under test. In particular,
        // macOS yield() can deschedule this loop for most of a short save.
        // Require overlap, then gate actual lock occupancy below.
        expect(concurrentCallbacks > 0, "The sample-heavy save never overlapped an audio callback");
        // Relative rather than an absolute deadline: tolerate slow/sanitized
        // runners, but fail if serialization occupies most of the audio lock.
        expect(totalWaitMs < saveMs * 0.75, "Serialization occupied most of the audio callback lock");
        expect(state != nullptr, "Concurrent save returned no state");
        if (! state) continue;
        auto* bank = state->getChildByName("CHIPPER_DMC_BANK");
        expect(bank != nullptr && bank->getNumChildElements() == 32, "Save lost sample-bank slots");
        if (bank)
            for (const auto* sample : bank->getChildIterator())
            {
                const auto* payload = sample->getChildByName(chipper::state::embeddedSampleStateTag);
                expect(payload != nullptr && payload->getIntAttribute("byteCount") == 262144,
                       "Save lost embedded sample data");
            }
        expect(processor.restoreStateXml(*state).wasOk(), "Concurrent snapshot failed subsequent project recall");
        std::cout << "CONCURRENT_SAVE blockSize=" << blockSize << " callbacks=" << concurrentCallbacks
                  << " maxCallbackLockWaitMs=" << maxWaitMs << " totalWaitMs=" << totalWaitMs
                  << " saveMs=" << saveMs << '\n';
    }
    return ok ? 0 : 1;
}
