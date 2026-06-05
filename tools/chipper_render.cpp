#include "Engine/ChipCore.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace
{

struct Options
{
    chipper::ChipMode chip = chipper::ChipMode::nes;
    chipper::AccuracyMode accuracy = chipper::AccuracyMode::hybrid;
    double clock = 1789773.0;
    double sampleRate = 48000.0;
    double seconds = 1.0;
    int note = 69;
    std::filesystem::path eventFile;
    std::filesystem::path wavPath = "chipper-render.wav";
    std::filesystem::path debugPath = "chipper-render.json";
};

struct ScheduledWrite
{
    uint64_t sample = 0;
    uint16_t address = 0;
    uint8_t value = 0;
};

template <typename T>
bool parseNumber(const std::string& text, T& out)
{
    if constexpr (std::is_integral_v<T>)
    {
        int base = 10;
        std::string_view view(text);
        if (view.size() > 2 && view[0] == '0' && (view[1] == 'x' || view[1] == 'X'))
        {
            base = 16;
            view.remove_prefix(2);
        }

        T value {};
        const auto* first = view.data();
        const auto* last = view.data() + view.size();
        const auto result = std::from_chars(first, last, value, base);
        if (result.ec != std::errc() || result.ptr != last)
            return false;
        out = value;
        return true;
    }
    else
    {
        char* end = nullptr;
        const auto value = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0')
            return false;
        out = static_cast<T>(value);
        return true;
    }
}

void printUsage()
{
    std::cerr
        << "Usage: chipper_render --chip nes --accuracy authentic --clock 1789773 --rate 48000 --seconds 1 --note 69 --out out.wav --debug out.json [--events events.txt]\n"
        << "\nEvent file lines:\n"
        << "  write <sample> <address> <value>\n"
        << "Addresses and values may be decimal or 0x-prefixed hex.\n";
}

bool parseArgs(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto requireValue = [&](const char* name) -> const char*
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--chip")
        {
            const auto* value = requireValue("--chip");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseChipMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown chip mode: " << value << "\n";
                return false;
            }
            options.chip = *parsed;
        }
        else if (arg == "--accuracy")
        {
            const auto* value = requireValue("--accuracy");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseAccuracyMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown accuracy mode: " << value << "\n";
                return false;
            }
            options.accuracy = *parsed;
        }
        else if (arg == "--clock")
        {
            const auto* value = requireValue("--clock");
            if (value == nullptr || ! parseNumber(std::string(value), options.clock))
                return false;
        }
        else if (arg == "--rate")
        {
            const auto* value = requireValue("--rate");
            if (value == nullptr || ! parseNumber(std::string(value), options.sampleRate))
                return false;
        }
        else if (arg == "--seconds")
        {
            const auto* value = requireValue("--seconds");
            if (value == nullptr || ! parseNumber(std::string(value), options.seconds))
                return false;
        }
        else if (arg == "--note")
        {
            const auto* value = requireValue("--note");
            if (value == nullptr || ! parseNumber(std::string(value), options.note))
                return false;
        }
        else if (arg == "--events")
        {
            const auto* value = requireValue("--events");
            if (value == nullptr)
                return false;
            options.eventFile = value;
        }
        else if (arg == "--out")
        {
            const auto* value = requireValue("--out");
            if (value == nullptr)
                return false;
            options.wavPath = value;
        }
        else if (arg == "--debug")
        {
            const auto* value = requireValue("--debug");
            if (value == nullptr)
                return false;
            options.debugPath = value;
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage();
            return false;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }

    return options.sampleRate > 0.0 && options.clock >= 0.0 && options.seconds > 0.0;
}

std::vector<ScheduledWrite> loadEvents(const std::filesystem::path& path)
{
    std::vector<ScheduledWrite> writes;
    if (path.empty())
        return writes;

    std::ifstream in(path);
    if (! in)
        throw std::runtime_error("Could not open event file: " + path.string());

    std::string line;
    int lineNumber = 0;
    while (std::getline(in, line))
    {
        ++lineNumber;
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream parts(line);
        std::string kind;
        std::string sampleText;
        std::string addressText;
        std::string valueText;
        parts >> kind >> sampleText >> addressText >> valueText;

        if (kind != "write")
            throw std::runtime_error("Unsupported event kind on line " + std::to_string(lineNumber));

        ScheduledWrite write;
        uint32_t address = 0;
        uint32_t value = 0;
        if (! parseNumber(sampleText, write.sample) || ! parseNumber(addressText, address) || ! parseNumber(valueText, value))
            throw std::runtime_error("Bad event value on line " + std::to_string(lineNumber));

        write.address = static_cast<uint16_t>(address & 0xffffu);
        write.value = static_cast<uint8_t>(value & 0xffu);
        writes.push_back(write);
    }

    std::sort(writes.begin(), writes.end(), [](const auto& a, const auto& b) { return a.sample < b.sample; });
    return writes;
}

void writeU16(std::ofstream& out, uint16_t value)
{
    out.put(static_cast<char>(value & 0xffu));
    out.put(static_cast<char>((value >> 8u) & 0xffu));
}

void writeU32(std::ofstream& out, uint32_t value)
{
    writeU16(out, static_cast<uint16_t>(value & 0xffffu));
    writeU16(out, static_cast<uint16_t>((value >> 16u) & 0xffffu));
}

void writeWav(const std::filesystem::path& path, const std::vector<chipper::StereoFrame>& frames, double sampleRate)
{
    std::ofstream out(path, std::ios::binary);
    if (! out)
        throw std::runtime_error("Could not write WAV: " + path.string());

    const auto dataBytes = static_cast<uint32_t>(frames.size() * 2u * sizeof(int16_t));
    out.write("RIFF", 4);
    writeU32(out, 36u + dataBytes);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, 2);
    writeU32(out, static_cast<uint32_t>(std::round(sampleRate)));
    writeU32(out, static_cast<uint32_t>(std::round(sampleRate)) * 2u * sizeof(int16_t));
    writeU16(out, 2u * sizeof(int16_t));
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, dataBytes);

    for (const auto& frame : frames)
    {
        const auto left = static_cast<int16_t>(std::round(std::clamp(frame.left, -1.0f, 1.0f) * 32767.0f));
        const auto right = static_cast<int16_t>(std::round(std::clamp(frame.right, -1.0f, 1.0f) * 32767.0f));
        writeU16(out, static_cast<uint16_t>(left));
        writeU16(out, static_cast<uint16_t>(right));
    }
}

chipper::RenderStats calculateStats(const std::vector<chipper::StereoFrame>& frames)
{
    chipper::RenderStats stats;
    double energy = 0.0;
    float previous = 0.0f;
    bool hasPrevious = false;

    for (const auto& frame : frames)
    {
        const auto mono = (frame.left + frame.right) * 0.5f;
        stats.peak = std::max(stats.peak, static_cast<double>(std::abs(mono)));
        energy += static_cast<double>(mono) * static_cast<double>(mono);

        if (hasPrevious && ((previous <= 0.0f && mono > 0.0f) || (previous >= 0.0f && mono < 0.0f)))
            ++stats.zeroCrossings;

        previous = mono;
        hasPrevious = true;
    }

    stats.renderedSamples = static_cast<uint64_t>(frames.size());
    stats.rms = frames.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(frames.size()));
    return stats;
}

void writeDebugJson(const std::filesystem::path& path,
                    const Options& options,
                    const chipper::ChipCore& core,
                    const chipper::RenderStats& stats,
                    size_t registerWriteCount)
{
    std::ofstream out(path);
    if (! out)
        throw std::runtime_error("Could not write debug JSON: " + path.string());

    out << "{\n"
        << "  \"chip\": \"" << chipper::toString(options.chip) << "\",\n"
        << "  \"requestedAccuracy\": \"" << chipper::toString(options.accuracy) << "\",\n"
        << "  \"implementedAccuracy\": \"" << core.implementedAccuracy() << "\",\n"
        << "  \"clockHz\": " << options.clock << ",\n"
        << "  \"sampleRate\": " << options.sampleRate << ",\n"
        << "  \"seconds\": " << options.seconds << ",\n"
        << "  \"note\": " << options.note << ",\n"
        << "  \"registerWriteCount\": " << registerWriteCount << ",\n"
        << "  \"renderedSamples\": " << stats.renderedSamples << ",\n"
        << "  \"peak\": " << stats.peak << ",\n"
        << "  \"rms\": " << stats.rms << ",\n"
        << "  \"zeroCrossings\": " << stats.zeroCrossings << ",\n"
        << "  \"coreState\": " << core.debugStateJson() << "\n"
        << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        Options options;
        if (! parseArgs(argc, argv, options))
        {
            printUsage();
            return 2;
        }

        auto core = chipper::createChipCore(options.chip, options.accuracy);
        core->reset(options.sampleRate, options.clock);
        const auto writes = loadEvents(options.eventFile);

        const auto sampleCount = static_cast<uint64_t>(std::ceil(options.seconds * options.sampleRate));
        std::vector<chipper::StereoFrame> frames;
        frames.reserve(static_cast<size_t>(sampleCount));

        core->noteOn(options.note, 1.0f);

        size_t writeIndex = 0;
        for (uint64_t sample = 0; sample < sampleCount; ++sample)
        {
            while (writeIndex < writes.size() && writes[writeIndex].sample == sample)
            {
                core->writeRegister(writes[writeIndex].address, writes[writeIndex].value);
                ++writeIndex;
            }

            frames.push_back(core->renderSample());
        }

        core->noteOff(options.note);
        const auto stats = calculateStats(frames);
        writeWav(options.wavPath, frames, options.sampleRate);
        writeDebugJson(options.debugPath, options, *core, stats, writes.size());

        std::cout << "Rendered " << frames.size() << " samples to " << options.wavPath.string()
                  << " and " << options.debugPath.string() << "\n";
        std::cout << core->modeName() << " -> " << core->implementedAccuracy() << "\n";
        if (! core->limitations().empty())
            std::cout << "Limitations: " << core->limitations() << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "chipper_render failed: " << e.what() << "\n";
        return 1;
    }
}
