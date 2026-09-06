#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

namespace
{
constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 256;

bool expect(bool condition, const std::string& message)
{
    if (condition)
        return true;

    std::cerr << message << '\n';
    return false;
}

juce::File findVst3Bundle(juce::File target)
{
    for (auto candidate = target; candidate != juce::File{}; candidate = candidate.getParentDirectory())
    {
        if (candidate.isDirectory() && candidate.hasFileExtension("vst3"))
            return candidate;

        const auto parent = candidate.getParentDirectory();
        if (parent == candidate)
            break;
    }

    return {};
}

juce::AudioProcessorParameter* findParameterByName(juce::AudioProcessor& processor, const juce::String& name)
{
    for (auto* parameter : processor.getParameters())
    {
        if (parameter != nullptr && parameter->getName(128) == name)
            return parameter;
    }

    return nullptr;
}

void automate(juce::AudioProcessorParameter& parameter, float normalizedValue)
{
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalizedValue));
    parameter.endChangeGesture();
}

bool writeDmcFixture(const juce::File& file)
{
    static constexpr std::array<uint8_t, 32> bytes {
        0xaa, 0x55, 0xf0, 0x0f, 0xcc, 0x33, 0x81, 0x7e,
        0x18, 0xe7, 0x42, 0xbd, 0x24, 0xdb, 0x66, 0x99,
        0x3c, 0xc3, 0x5a, 0xa5, 0x7f, 0x80, 0x11, 0xee,
        0x69, 0x96, 0x87, 0x78, 0xd2, 0x2d, 0xb4, 0x4b
    };

    juce::FileOutputStream output(file);
    if (! output.openedOk())
        return false;

    if (! output.write(bytes.data(), bytes.size()))
        return false;
    output.flush();
    return output.getStatus().wasOk();
}

bool injectDmcAsset(juce::MemoryBlock& state, const juce::File& asset)
{
    auto hostState = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    if (hostState == nullptr || ! hostState->hasTagName("VST3PluginState"))
        return false;

    auto* componentState = hostState->getChildByName("IComponent");
    juce::MemoryBlock componentBytes;
    if (componentState == nullptr || ! componentBytes.fromBase64Encoding(componentState->getAllSubText()))
        return false;

    auto xml = juce::AudioProcessor::getXmlFromBinary(componentBytes.getData(),
                                                      static_cast<int>(componentBytes.getSize()));
    if (xml == nullptr || ! xml->hasTagName("ChipperState"))
        return false;

    while (auto* existing = xml->getChildByName("CHIPPER_DMC_BANK"))
        xml->removeChildElement(existing, true);

    auto* bank = new juce::XmlElement("CHIPPER_DMC_BANK");
    bank->setAttribute("count", 1);
    auto* sample = new juce::XmlElement("DMC_SAMPLE");
    sample->setAttribute("path", asset.getFullPathName());
    sample->setAttribute("included", 1);
    bank->addChildElement(sample);
    xml->addChildElement(bank);
    juce::AudioProcessor::copyXmlToBinary(*xml, componentBytes);
    componentState->deleteAllChildElements();
    componentState->addTextElement(componentBytes.toBase64Encoding());
    juce::AudioProcessor::copyXmlToBinary(*hostState, state);
    return true;
}

bool stateContainsDmcAsset(const juce::MemoryBlock& state, const juce::File& asset, bool requireEmbeddedPayload)
{
    const auto hostState = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    if (hostState == nullptr || ! hostState->hasTagName("VST3PluginState"))
        return false;

    const auto* componentState = hostState->getChildByName("IComponent");
    juce::MemoryBlock componentBytes;
    if (componentState == nullptr || ! componentBytes.fromBase64Encoding(componentState->getAllSubText()))
        return false;

    const auto xml = juce::AudioProcessor::getXmlFromBinary(componentBytes.getData(),
                                                            static_cast<int>(componentBytes.getSize()));
    if (xml == nullptr || ! xml->hasTagName("ChipperState"))
        return false;

    const auto* bank = xml->getChildByName("CHIPPER_DMC_BANK");
    if (bank == nullptr || bank->getIntAttribute("count") != 1)
        return false;

    const auto* sample = bank->getChildByName("DMC_SAMPLE");
    const auto pathMatches = sample != nullptr
        && sample->getBoolAttribute("included", false)
        && juce::File(sample->getStringAttribute("path")) == asset;
    const auto* payload = sample != nullptr ? sample->getChildByName("CHIPPER_EMBEDDED_SAMPLE") : nullptr;
    return pathMatches
        && (! requireEmbeddedPayload
            || (payload != nullptr && payload->getIntAttribute("formatVersion") == 1
                && payload->getIntAttribute("byteCount") == 32
                && payload->getAllSubText().isNotEmpty()));
}

void processSilentBlock(juce::AudioPluginInstance& instance)
{
    juce::AudioBuffer<float> buffer(2, testBlockSize);
    juce::MidiBuffer midi;
    instance.processBlock(buffer, midi);
}

std::vector<float> renderAudition(juce::AudioPluginInstance& instance)
{
    constexpr int blocks = 16;
    std::vector<float> rendered;
    rendered.reserve(static_cast<size_t>(blocks * testBlockSize * 2));
    juce::AudioBuffer<float> buffer(2, testBlockSize);

    for (int block = 0; block < blocks; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
        if (block == 12)
            midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);

        instance.processBlock(buffer, midi);
        for (int sample = 0; sample < testBlockSize; ++sample)
        {
            rendered.push_back(buffer.getSample(0, sample));
            rendered.push_back(buffer.getSample(1, sample));
        }
    }

    return rendered;
}

float peakOf(const std::vector<float>& samples)
{
    auto peak = 0.0f;
    for (const auto sample : samples)
        peak = std::max(peak, std::abs(sample));
    return peak;
}

double normalizedRmse(const std::vector<float>& reference, const std::vector<float>& candidate)
{
    if (reference.size() != candidate.size() || reference.empty())
        return std::numeric_limits<double>::infinity();

    auto squaredError = 0.0;
    auto squaredReference = 0.0;
    for (size_t index = 0; index < reference.size(); ++index)
    {
        const auto difference = static_cast<double>(reference[index]) - static_cast<double>(candidate[index]);
        squaredError += difference * difference;
        squaredReference += static_cast<double>(reference[index]) * static_cast<double>(reference[index]);
    }

    return std::sqrt(squaredError / std::max(squaredReference, 1.0e-18));
}

bool verifyMonoRouting(juce::VST3PluginFormatHeadless& format, const juce::PluginDescription& description)
{
    auto ok = true;
    for (const auto blockSize : { 64, 128, 256 })
    {
        for (const auto* route : { "Both", "Left", "Right" })
        {
            std::array<std::vector<float>, 2> captures;
            for (int channels : { 2, 1 })
            {
                juce::String error;
                auto instance = format.createInstanceFromDescription(description, testSampleRate, blockSize, error);
                if (! expect(instance != nullptr, "Mono routing instance failed: " + error.toStdString()))
                    return false;
                auto layout = instance->getBusesLayout();
                layout.outputBuses.getReference(0) = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
                if (! expect(instance->setBusesLayout(layout), "Host failed to negotiate output layout"))
                    return false;
                for (const auto& [name, value] : std::array<std::pair<juce::String, juce::String>, 2> {
                         std::pair { juce::String("Chip Mode"), juce::String("Game Boy / DMG") },
                         std::pair { juce::String("Chip Choice / Route"), juce::String(route) } })
                {
                    auto* parameter = findParameterByName(*instance, name);
                    if (! expect(parameter != nullptr, "Missing routing parameter: " + name.toStdString()))
                        return false;
                    automate(*parameter, parameter->getValueForText(value));
                }
                instance->prepareToPlay(testSampleRate, blockSize);
                juce::AudioBuffer<float> buffer(channels, blockSize);
                juce::MidiBuffer midi;
                auto& capture = captures[static_cast<size_t>(channels - 1)];
                for (int block = 0; block < 32; ++block)
                {
                    buffer.clear();
                    midi.clear();
                    if (block == 0)
                        midi.addEvent(juce::MidiMessage::noteOn(1, 69, 0.8f), 0);
                    instance->processBlock(buffer, midi);
                    for (int sample = 0; sample < blockSize; ++sample)
                        capture.push_back(channels == 1 ? buffer.getSample(0, sample)
                                          : 0.5f * (buffer.getSample(0, sample) + buffer.getSample(1, sample)));
                }
                instance->releaseResources();
            }
            double maximumError = 0.0;
            float peak = 0.0f;
            for (size_t sample = 0; sample < captures[0].size(); ++sample)
            {
                maximumError = std::max(maximumError, std::abs(static_cast<double>(captures[0][sample] - captures[1][sample])));
                peak = std::max(peak, std::abs(captures[0][sample]));
            }
            ok &= expect(peak > 0.01f && maximumError < 1.0e-6,
                         "Mono must equal 0.5*(L+R) for DMG route " + std::string(route)
                         + " at block size " + std::to_string(blockSize));
        }
    }
    return ok;
}

std::unique_ptr<juce::AudioPluginInstance> createInstance(juce::VST3PluginFormatHeadless& format,
                                                          const juce::PluginDescription& description,
                                                          juce::String& error)
{
    auto instance = format.createInstanceFromDescription(description, testSampleRate, testBlockSize, error);
    if (instance != nullptr)
    {
        instance->setNonRealtime(true);
        instance->prepareToPlay(testSampleRate, testBlockSize);
    }
    return instance;
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    auto ok = true;
    ok &= expect(argc == 2, "Usage: chipper_vst3_host_state_smoke <Chipper VST3 target file>");
    if (! ok)
        return 1;

    const auto bundle = findVst3Bundle(juce::File(argv[1]));
    ok &= expect(bundle.isDirectory(), "Could not resolve the Chipper.vst3 bundle from the build target path");
    if (! ok)
        return 1;

    juce::VST3PluginFormatHeadless format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, bundle.getFullPathName());
    ok &= expect(descriptions.size() == 1, "The built Chipper VST3 bundle should expose exactly one plugin class");
    if (! ok)
        return 1;

    const auto description = *descriptions.getFirst();
    ok &= verifyMonoRouting(format, description);
    ok &= expect(description.name == "Chipper", "The scanned VST3 class should be named Chipper");
    ok &= expect(description.pluginFormatName == "VST3", "The scanned binary should identify as VST3");

    const auto fixtureRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getNonexistentChildFile("chipper-vst3-host-state", {}, false);
    ok &= expect(fixtureRoot.createDirectory().wasOk(), "Could not create the VST3 host-state fixture directory");
    const auto dmcAsset = fixtureRoot.getChildFile("host-state.dmc");
    ok &= expect(writeDmcFixture(dmcAsset), "Could not create the external DMC fixture");

    juce::String creationError;
    auto original = createInstance(format, description, creationError);
    ok &= expect(original != nullptr, "Could not instantiate the built VST3: " + creationError.toStdString());
    if (original == nullptr)
    {
        fixtureRoot.deleteRecursively();
        return 1;
    }

    ok &= expect(original->getTotalNumInputChannels() == 0, "Chipper VST3 should load as a zero-input instrument");
    ok &= expect(original->getTotalNumOutputChannels() == 2, "Chipper VST3 should load with its default stereo output");
    ok &= expect(original->getParameters().size() >= 96,
                 "The VST3 host should expose all Chipper parameters in addition to any JUCE MIDI proxy parameters");

    const std::array<std::pair<juce::String, float>, 8> automatedParameters {
        std::pair { juce::String("Output Level"), 0.71f },
        std::pair { juce::String("Behavior Strictness (Reserved)"), 1.0f },
        std::pair { juce::String("Native Control 1"), 0.83f },
        std::pair { juce::String("Source 1 Enabled"), 0.0f },
        std::pair { juce::String("Source 2 Enabled"), 0.0f },
        std::pair { juce::String("Source 3 Enabled"), 0.0f },
        std::pair { juce::String("Source 4 Enabled"), 0.0f },
        std::pair { juce::String("Source 5 Enabled"), 1.0f }
    };
    for (const auto& [name, value] : automatedParameters)
    {
        auto* parameter = findParameterByName(*original, name);
        ok &= expect(parameter != nullptr, "VST3 host could not find automatable parameter: " + name.toStdString());
        if (parameter != nullptr)
            automate(*parameter, value);
    }

    juce::MemoryBlock stateWithAsset;
    original->getStateInformation(stateWithAsset);
    ok &= expect(! stateWithAsset.isEmpty(), "VST3 host save should return non-empty component state");
    ok &= expect(injectDmcAsset(stateWithAsset, dmcAsset), "VST3 component state should remain a readable Chipper XML payload");
    original->setStateInformation(stateWithAsset.getData(), static_cast<int>(stateWithAsset.getSize()));
    processSilentBlock(*original);

    juce::MemoryBlock savedProjectState;
    original->getStateInformation(savedProjectState);
    ok &= expect(stateContainsDmcAsset(savedProjectState, dmcAsset, true),
                 "VST3 project state should retain the DMC source path and its embedded project payload");

    std::array<float, automatedParameters.size()> savedParameterValues {};
    for (size_t index = 0; index < automatedParameters.size(); ++index)
    {
        if (auto* parameter = findParameterByName(*original, automatedParameters[index].first))
            savedParameterValues[index] = parameter->getValue();
    }
    const auto originalRender = renderAudition(*original);
    ok &= expect(dmcAsset.deleteFile(),
                 "The VST3 host-state regression should delete the DMC source before reopening the project");
    original->releaseResources();
    original.reset();

    creationError.clear();
    auto reopened = createInstance(format, description, creationError);
    ok &= expect(reopened != nullptr, "Could not reopen a second Chipper VST3 instance: " + creationError.toStdString());
    if (reopened == nullptr)
    {
        fixtureRoot.deleteRecursively();
        return 1;
    }

    reopened->setStateInformation(savedProjectState.getData(), static_cast<int>(savedProjectState.getSize()));
    processSilentBlock(*reopened);
    for (size_t index = 0; index < automatedParameters.size(); ++index)
    {
        auto* parameter = findParameterByName(*reopened, automatedParameters[index].first);
        ok &= expect(parameter != nullptr, "Reopened VST3 lost parameter: " + automatedParameters[index].first.toStdString());
        if (parameter != nullptr)
            ok &= expect(std::abs(parameter->getValue() - savedParameterValues[index]) <= 1.0e-6f,
                         "Reopened VST3 did not restore automation for "
                             + automatedParameters[index].first.toStdString());
    }

    juce::MemoryBlock reopenedState;
    reopened->getStateInformation(reopenedState);
    ok &= expect(stateContainsDmcAsset(reopenedState, dmcAsset, true),
                 "Reopened VST3 should re-save the missing source reference and embedded DMC project payload");

    const auto reopenedRender = renderAudition(*reopened);
    const auto renderError = normalizedRmse(originalRender, reopenedRender);
    ok &= expect(peakOf(originalRender) > 0.001f && peakOf(reopenedRender) > 0.001f,
                 "Original and reopened VST3 instances should both render audible output");
    ok &= expect(renderError <= 1.0e-5,
                 "Reopened VST3 render diverged from saved state; normalized RMSE="
                     + std::to_string(renderError));

    std::cout << "VST3_HOST_STATE"
              << " bundle=" << bundle.getFullPathName()
              << " parameters=" << reopened->getParameters().size()
              << " stateBytes=" << reopenedState.getSize()
              << " externalAsset=" << dmcAsset.getFileName()
              << " renderPeak=" << peakOf(reopenedRender)
              << " normalizedRmse=" << renderError
              << '\n';

    reopened->releaseResources();
    reopened.reset();
    fixtureRoot.deleteRecursively();
    return ok ? 0 : 1;
}
