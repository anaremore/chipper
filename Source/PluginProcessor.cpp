#include "PluginProcessor.h"

#include "Engine/ChipDescriptors.h"
#include "PluginEditor.h"

namespace
{
constexpr auto coreStateTag = "CHIPPER_CORE_REGISTERS";
constexpr auto registerTag = "REG";

bool patchMatches(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    return a.macro == b.macro
        && std::abs(a.control1 - b.control1) < tolerance
        && std::abs(a.control2 - b.control2) < tolerance
        && std::abs(a.control3 - b.control3) < tolerance
        && std::abs(a.control4 - b.control4) < tolerance
        && a.playMode == b.playMode;
}
}

ChipperAudioProcessor::ChipperAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ChipperState", chipper::parameters::createLayout())
{
}

void ChipperAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    core.reset();
    ensureCore();
}

void ChipperAudioProcessor::releaseResources()
{
}

bool ChipperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void ChipperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    ensureCore();

    const auto outputDb = apvts.getRawParameterValue(chipper::parameters::id::outputDb)->load();
    const auto outputGain = juce::Decibels::decibelsToGain(outputDb);

    int renderedUntil = 0;
    for (const auto metadata : midiMessages)
    {
        const auto position = std::clamp(metadata.samplePosition, 0, buffer.getNumSamples());
        renderRange(buffer, renderedUntil, position, outputGain);
        handleMidiMessage(metadata.getMessage());
        renderedUntil = position;
    }

    renderRange(buffer, renderedUntil, buffer.getNumSamples(), outputGain);
}

void ChipperAudioProcessor::renderRange(juce::AudioBuffer<float>& buffer, int startSample, int endSample, float outputGain)
{
    if (core == nullptr || endSample <= startSample)
        return;

    const auto channels = buffer.getNumChannels();
    for (int sample = startSample; sample < endSample; ++sample)
    {
        const auto frame = core->renderSample();
        const auto left = juce::jlimit(-1.0f, 1.0f, frame.left * outputGain);
        const auto right = juce::jlimit(-1.0f, 1.0f, frame.right * outputGain);

        if (channels > 0)
            buffer.setSample(0, sample, left);
        if (channels > 1)
            buffer.setSample(1, sample, right);
    }
}

void ChipperAudioProcessor::handleMidiMessage(const juce::MidiMessage& message)
{
    if (core == nullptr)
        return;

    if (message.isNoteOn())
    {
        activeNote = message.getNoteNumber();
        activeVelocity = message.getFloatVelocity();
        core->noteOn(activeNote, activeVelocity);
    }
    else if (message.isNoteOff())
    {
        core->noteOff(message.getNoteNumber());
        if (activeNote == message.getNoteNumber())
        {
            activeNote = -1;
            activeVelocity = 0.0f;
        }
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        if (activeNote >= 0)
            core->noteOff(activeNote);
        activeNote = -1;
        activeVelocity = 0.0f;
    }
}

void ChipperAudioProcessor::ensureCore()
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto accuracyChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::accuracy)->load()));
    const auto selectedMode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto selectedAccuracy = chipper::parameters::accuracyFromChoice(accuracyChoice);
    const auto clockOverride = apvts.getRawParameterValue(chipper::parameters::id::clockHz)->load();
    const auto selectedClock = clockOverride > 0.0f ? static_cast<double>(clockOverride) : chipper::parameters::defaultClockForMode(selectedMode);
    const auto selectedPatch = currentPatchFromParameters();

    if (core != nullptr && selectedMode == activeMode && selectedAccuracy == activeAccuracy && std::abs(selectedClock - activeClock) < 0.5)
    {
        if (! patchMatches(selectedPatch, activePatch))
        {
            activePatch = selectedPatch;
            core->setPatch(activePatch);

            if (activeNote >= 0)
                core->noteOn(activeNote, activeVelocity);
        }

        return;
    }

    activeMode = selectedMode;
    activeAccuracy = selectedAccuracy;
    activeClock = selectedClock;
    activePatch = selectedPatch;
    core = chipper::createChipCore(activeMode, activeAccuracy);
    core->reset(currentSampleRate, activeClock);
    core->setPatch(activePatch);
    replayPendingRegisterState();
    activeNote = -1;
    activeVelocity = 0.0f;
}

chipper::PatchConfig ChipperAudioProcessor::currentPatchFromParameters() const
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    const auto playModeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::playMode)->load()));

    return chipper::makePatchConfig(
        chipper::parameters::chipModeFromChoice(modeChoice),
        chipper::parameters::macroFromChoice(macroChoice),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl1)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl2)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl3)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::macroControl4)->load(),
        chipper::parameters::playModeFromChoice(playModeChoice));
}

void ChipperAudioProcessor::replayPendingRegisterState()
{
    if (core == nullptr || pendingRegisterState.empty())
        return;

    for (const auto& write : pendingRegisterState)
        core->writeRegister(write.address, write.value);

    pendingRegisterState.clear();
}

juce::AudioProcessorEditor* ChipperAudioProcessor::createEditor()
{
    return new ChipperAudioProcessorEditor(*this);
}

void ChipperAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml(state.createXml());
    while (auto* existingCoreState = xml->getChildByName(coreStateTag))
        xml->removeChildElement(existingCoreState, true);

    if (core != nullptr)
    {
        auto* coreState = new juce::XmlElement(coreStateTag);
        coreState->setAttribute("mode", core->modeName());
        coreState->setAttribute("implementedAccuracy", core->implementedAccuracy());

        const auto writes = core->exportRegisterState();
        coreState->setAttribute("count", static_cast<int>(writes.size()));
        for (const auto& write : writes)
        {
            auto* reg = new juce::XmlElement(registerTag);
            reg->setAttribute("address", static_cast<int>(write.address));
            reg->setAttribute("value", static_cast<int>(write.value));
            coreState->addChildElement(reg);
        }

        xml->addChildElement(coreState);
    }
    copyXmlToBinary(*xml, destData);
}

void ChipperAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        pendingRegisterState.clear();
        if (auto* coreState = xml->getChildByName(coreStateTag))
        {
            for (const auto* child : coreState->getChildIterator())
            {
                if (child != nullptr && child->hasTagName(registerTag))
                {
                    pendingRegisterState.push_back({
                        0,
                        static_cast<uint16_t>(child->getIntAttribute("address") & 0xffff),
                        static_cast<uint8_t>(child->getIntAttribute("value") & 0xff)
                    });
                }
            }
            xml->removeChildElement(coreState, true);
        }

        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        core.reset();
    }
}

std::string ChipperAudioProcessor::currentCoreStatus() const
{
    if (core == nullptr)
        return "No core loaded";

    const auto& descriptor = chipper::descriptorFor(activeMode);
    return descriptor.displayName + ": " + core->implementedAccuracy() + ". " + descriptor.summary + " " + core->limitations();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipperAudioProcessor();
}
