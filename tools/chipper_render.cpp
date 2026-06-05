#include "Engine/ChipCore.h"
#include "Engine/ChipDescriptors.h"

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
    chipper::MacroKind macro = chipper::MacroKind::manual;
    chipper::PlayMode playMode = chipper::PlayMode::stack;
    float control1 = 0.5f;
    float control2 = 0.5f;
    float control3 = 0.5f;
    float control4 = 0.5f;
    double clock = 1789773.0;
    double sampleRate = 48000.0;
    double seconds = 1.0;
    int note = 69;
    std::filesystem::path eventFile;
    std::filesystem::path wavPath = "chipper-render.wav";
    std::filesystem::path debugPath = "chipper-render.json";
};

enum class EventType
{
    write,
    noteOff,
    noteOn
};

struct ScheduledEvent
{
    EventType type = EventType::write;
    uint64_t sample = 0;
    uint16_t address = 0;
    uint8_t value = 0;
    int note = 60;
    float velocity = 1.0f;
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
        << "       Optional: --macro coin --play-mode chip-poly --control1 0.2 --control2 0.8 --control3 0.1 --control4 0.5\n"
        << "\nEvent file lines:\n"
        << "  write <sample> <address> <value>\n"
        << "  note_on <sample> <note> <velocity>\n"
        << "  note_off <sample> <note>\n"
        << "  note <sample> <note> <velocity> <lengthSamples>\n"
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
        else if (arg == "--macro")
        {
            const auto* value = requireValue("--macro");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parseMacroKind(value);
            if (! parsed)
            {
                std::cerr << "Unknown macro: " << value << "\n";
                return false;
            }
            options.macro = *parsed;
        }
        else if (arg == "--play-mode")
        {
            const auto* value = requireValue("--play-mode");
            if (value == nullptr)
                return false;
            const auto parsed = chipper::parsePlayMode(value);
            if (! parsed)
            {
                std::cerr << "Unknown play mode: " << value << "\n";
                return false;
            }
            options.playMode = *parsed;
        }
        else if (arg == "--control1")
        {
            const auto* value = requireValue("--control1");
            if (value == nullptr || ! parseNumber(std::string(value), options.control1))
                return false;
        }
        else if (arg == "--control2")
        {
            const auto* value = requireValue("--control2");
            if (value == nullptr || ! parseNumber(std::string(value), options.control2))
                return false;
        }
        else if (arg == "--control3")
        {
            const auto* value = requireValue("--control3");
            if (value == nullptr || ! parseNumber(std::string(value), options.control3))
                return false;
        }
        else if (arg == "--control4")
        {
            const auto* value = requireValue("--control4");
            if (value == nullptr || ! parseNumber(std::string(value), options.control4))
                return false;
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

std::vector<ScheduledEvent> loadEvents(const std::filesystem::path& path)
{
    std::vector<ScheduledEvent> events;
    if (path.empty())
        return events;

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
        parts >> kind >> sampleText;

        ScheduledEvent event;
        if (! parseNumber(sampleText, event.sample))
            throw std::runtime_error("Bad event sample on line " + std::to_string(lineNumber));

        if (kind == "write")
        {
            std::string addressText;
            std::string valueText;
            uint32_t address = 0;
            uint32_t value = 0;
            parts >> addressText >> valueText;
            if (! parseNumber(addressText, address) || ! parseNumber(valueText, value))
                throw std::runtime_error("Bad write event value on line " + std::to_string(lineNumber));

            event.type = EventType::write;
            event.address = static_cast<uint16_t>(address & 0xffffu);
            event.value = static_cast<uint8_t>(value & 0xffu);
            events.push_back(event);
        }
        else if (kind == "note_on")
        {
            std::string noteText;
            std::string velocityText;
            parts >> noteText >> velocityText;
            if (! parseNumber(noteText, event.note) || ! parseNumber(velocityText, event.velocity))
                throw std::runtime_error("Bad note_on event value on line " + std::to_string(lineNumber));

            event.type = EventType::noteOn;
            events.push_back(event);
        }
        else if (kind == "note_off")
        {
            std::string noteText;
            parts >> noteText;
            if (! parseNumber(noteText, event.note))
                throw std::runtime_error("Bad note_off event value on line " + std::to_string(lineNumber));

            event.type = EventType::noteOff;
            events.push_back(event);
        }
        else if (kind == "note")
        {
            std::string noteText;
            std::string velocityText;
            std::string lengthText;
            uint64_t lengthSamples = 0;
            parts >> noteText >> velocityText >> lengthText;
            if (! parseNumber(noteText, event.note) || ! parseNumber(velocityText, event.velocity) || ! parseNumber(lengthText, lengthSamples))
                throw std::runtime_error("Bad note event value on line " + std::to_string(lineNumber));

            event.type = EventType::noteOn;
            events.push_back(event);

            ScheduledEvent off = event;
            off.type = EventType::noteOff;
            off.sample += lengthSamples;
            off.velocity = 0.0f;
            events.push_back(off);
        }
        else
        {
            throw std::runtime_error("Unsupported event kind on line " + std::to_string(lineNumber));
        }
    }

    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b)
    {
        if (a.sample != b.sample)
            return a.sample < b.sample;
        return static_cast<int>(a.type) < static_cast<int>(b.type);
    });
    return events;
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
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;
    float previous = 0.0f;
    bool hasPrevious = false;

    for (const auto& frame : frames)
    {
        const auto mono = (frame.left + frame.right) * 0.5f;
        stats.peak = std::max(stats.peak, static_cast<double>(std::abs(mono)));
        stats.leftPeak = std::max(stats.leftPeak, static_cast<double>(std::abs(frame.left)));
        stats.rightPeak = std::max(stats.rightPeak, static_cast<double>(std::abs(frame.right)));
        energy += static_cast<double>(mono) * static_cast<double>(mono);
        leftEnergy += static_cast<double>(frame.left) * static_cast<double>(frame.left);
        rightEnergy += static_cast<double>(frame.right) * static_cast<double>(frame.right);

        if (hasPrevious && ((previous <= 0.0f && mono > 0.0f) || (previous >= 0.0f && mono < 0.0f)))
            ++stats.zeroCrossings;

        previous = mono;
        hasPrevious = true;
    }

    stats.renderedSamples = static_cast<uint64_t>(frames.size());
    stats.rms = frames.empty() ? 0.0 : std::sqrt(energy / static_cast<double>(frames.size()));
    stats.leftRms = frames.empty() ? 0.0 : std::sqrt(leftEnergy / static_cast<double>(frames.size()));
    stats.rightRms = frames.empty() ? 0.0 : std::sqrt(rightEnergy / static_cast<double>(frames.size()));
    return stats;
}

void writeDebugJson(const std::filesystem::path& path,
                    const Options& options,
                    const chipper::PatchConfig& patch,
                    const chipper::ChipCore& core,
                    const chipper::RenderStats& stats,
                    size_t registerWriteCount,
                    size_t noteEventCount)
{
    std::ofstream out(path);
    if (! out)
        throw std::runtime_error("Could not write debug JSON: " + path.string());

    out << "{\n"
        << "  \"chip\": \"" << chipper::toString(options.chip) << "\",\n"
        << "  \"requestedAccuracy\": \"" << chipper::toString(options.accuracy) << "\",\n"
        << "  \"macro\": \"" << chipper::toString(patch.macro) << "\",\n"
        << "  \"playMode\": \"" << chipper::toString(patch.playMode) << "\",\n"
        << "  \"requestedPlayMode\": \"" << chipper::toString(options.playMode) << "\",\n"
        << "  \"implementedAccuracy\": \"" << core.implementedAccuracy() << "\",\n"
        << "  \"clockHz\": " << options.clock << ",\n"
        << "  \"sampleRate\": " << options.sampleRate << ",\n"
        << "  \"seconds\": " << options.seconds << ",\n"
        << "  \"note\": " << options.note << ",\n"
        << "  \"registerWriteCount\": " << registerWriteCount << ",\n"
        << "  \"noteEventCount\": " << noteEventCount << ",\n"
        << "  \"renderedSamples\": " << stats.renderedSamples << ",\n"
        << "  \"peak\": " << stats.peak << ",\n"
        << "  \"rms\": " << stats.rms << ",\n"
        << "  \"leftPeak\": " << stats.leftPeak << ",\n"
        << "  \"rightPeak\": " << stats.rightPeak << ",\n"
        << "  \"leftRms\": " << stats.leftRms << ",\n"
        << "  \"rightRms\": " << stats.rightRms << ",\n"
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
        const auto patch = chipper::makePatchConfig(options.chip,
                                                    options.macro,
                                                    options.control1,
                                                    options.control2,
                                                    options.control3,
                                                    options.control4,
                                                    options.playMode);
        core->setPatch(patch);
        const auto events = loadEvents(options.eventFile);
        const auto registerWriteCount = static_cast<size_t>(std::count_if(events.begin(), events.end(), [](const auto& event) { return event.type == EventType::write; }));
        const auto noteEventCount = events.size() - registerWriteCount;

        const auto sampleCount = static_cast<uint64_t>(std::ceil(options.seconds * options.sampleRate));
        std::vector<chipper::StereoFrame> frames;
        frames.reserve(static_cast<size_t>(sampleCount));

        if (noteEventCount == 0)
            core->noteOn(options.note, 1.0f);

        size_t eventIndex = 0;
        for (uint64_t sample = 0; sample < sampleCount; ++sample)
        {
            while (eventIndex < events.size() && events[eventIndex].sample == sample)
            {
                const auto& event = events[eventIndex];
                if (event.type == EventType::write)
                    core->writeRegister(event.address, event.value);
                else if (event.type == EventType::noteOn)
                    core->noteOn(event.note, event.velocity);
                else if (event.type == EventType::noteOff)
                    core->noteOff(event.note);

                ++eventIndex;
            }

            frames.push_back(core->renderSample());
        }

        core->noteOff(options.note);
        const auto stats = calculateStats(frames);
        writeWav(options.wavPath, frames, options.sampleRate);
        writeDebugJson(options.debugPath, options, patch, *core, stats, registerWriteCount, noteEventCount);

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
