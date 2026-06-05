#include "PluginProcessor.h"

#include "PluginEditor.h"

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
        core->noteOn(activeNote, message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        core->noteOff(message.getNoteNumber());
        if (activeNote == message.getNoteNumber())
            activeNote = -1;
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        if (activeNote >= 0)
            core->noteOff(activeNote);
        activeNote = -1;
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

    if (core != nullptr && selectedMode == activeMode && selectedAccuracy == activeAccuracy && std::abs(selectedClock - activeClock) < 0.5)
        return;

    activeMode = selectedMode;
    activeAccuracy = selectedAccuracy;
    activeClock = selectedClock;
    core = chipper::createChipCore(activeMode, activeAccuracy);
    core->reset(currentSampleRate, activeClock);
    activeNote = -1;
}

juce::AudioProcessorEditor* ChipperAudioProcessor::createEditor()
{
    return new ChipperAudioProcessorEditor(*this);
}

void ChipperAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ChipperAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        core.reset();
    }
}

std::string ChipperAudioProcessor::currentCoreStatus() const
{
    if (core == nullptr)
        return "No core loaded";

    return core->modeName() + ": " + core->implementedAccuracy() + ". " + core->limitations();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipperAudioProcessor();
}
