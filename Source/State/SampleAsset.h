#pragma once
#include <JuceHeader.h>
#include "Engine/ChipCore.h"
#include <memory>
#include <vector>

namespace chipper::state
{
// Published samples are immutable. Copying a bank for project save retains its
// buffers without copying megabytes or tying serialization to the audio lock.
class SharedSampleBytes
{
public:
    SharedSampleBytes() = default;
    SharedSampleBytes(std::vector<uint8_t> bytes) { *this = std::move(bytes); }
    SharedSampleBytes& operator=(std::vector<uint8_t> bytes)
    {
        storage = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
        return *this;
    }
    void assign(const void* data, size_t count)
    {
        if (count == 0) { storage.reset(); return; }
        const auto* begin = static_cast<const uint8_t*>(data);
        *this = std::vector<uint8_t>(begin, begin + count);
    }
    const std::vector<uint8_t>& view() const noexcept
    {
        static const std::vector<uint8_t> empty;
        return storage ? *storage : empty;
    }
    operator const std::vector<uint8_t>&() const noexcept { return view(); }
    bool empty() const noexcept { return view().empty(); }
    size_t size() const noexcept { return view().size(); }
    const uint8_t* data() const noexcept { return view().data(); }
    uint8_t operator[](size_t index) const noexcept { return view()[index]; }
    auto begin() const noexcept { return view().begin(); }
    auto end() const noexcept { return view().end(); }
private:
    std::shared_ptr<const std::vector<uint8_t>> storage;
};

struct SampleAsset
{
    juce::String name;
    juce::String path;
    SharedSampleBytes bytes;
    chipper::ExternalSampleEncoding encoding = chipper::ExternalSampleEncoding::rawBytes;
    bool hasLoop = false;
    size_t loopStart = 0;
    size_t loopEnd = 0;
    bool included = true;
    int sourceSampleIndex = -1;
    size_t sourceByteCount = 0u;
    size_t sourceSampleCount = 0u;
    double sourceRateHz = 0.0;
    int rootNote = 60;
    size_t trimStart = 0u;
    size_t trimEnd = 0u;
    bool holdLastValue = false;
};

} // namespace chipper::state
