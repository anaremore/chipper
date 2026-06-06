#include "PluginProcessor.h"

#include "Engine/ChipDescriptors.h"
#include "PluginEditor.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace
{
constexpr auto coreStateTag = "CHIPPER_CORE_REGISTERS";
constexpr auto registerTag = "REG";

bool sourceLevelsMatch(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    for (size_t i = 0; i < a.sourceLevels.size(); ++i)
    {
        if (std::abs(a.sourceLevels[i] - b.sourceLevels[i]) >= tolerance)
            return false;
    }

    return true;
}

bool patchMatches(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    return a.macro == b.macro
        && std::abs(a.control1 - b.control1) < tolerance
        && std::abs(a.control2 - b.control2) < tolerance
        && std::abs(a.control3 - b.control3) < tolerance
        && std::abs(a.control4 - b.control4) < tolerance
        && a.playMode == b.playMode
        && a.sourceEnabled == b.sourceEnabled
        && sourceLevelsMatch(a, b)
        && std::abs(a.stereoSpread - b.stereoSpread) < tolerance
        && std::abs(a.envelopeDecay - b.envelopeDecay) < tolerance
        && a.sidAttack == b.sidAttack
        && a.sidDecay == b.sidDecay
        && a.sidSustain == b.sidSustain
        && a.sidRelease == b.sidRelease
        && a.waveShape == b.waveShape
        && a.dmgWaveLevel == b.dmgWaveLevel
        && a.dmgStereoRoute == b.dmgStereoRoute
        && a.ymEnvelopeShape == b.ymEnvelopeShape
        && a.snNoiseMode == b.snNoiseMode
        && a.sidVoice2WaveShape == b.sidVoice2WaveShape
        && a.sidVoice3WaveShape == b.sidVoice3WaveShape;
}

bool patchControlsMatch(const chipper::PatchConfig& a, const chipper::PatchConfig& b)
{
    constexpr auto tolerance = 0.0001f;
    return std::abs(a.control1 - b.control1) < tolerance
        && std::abs(a.control2 - b.control2) < tolerance
        && std::abs(a.control3 - b.control3) < tolerance
        && std::abs(a.control4 - b.control4) < tolerance
        && a.playMode == b.playMode
        && a.sourceEnabled == b.sourceEnabled
        && sourceLevelsMatch(a, b)
        && std::abs(a.stereoSpread - b.stereoSpread) < tolerance
        && std::abs(a.envelopeDecay - b.envelopeDecay) < tolerance
        && a.sidAttack == b.sidAttack
        && a.sidDecay == b.sidDecay
        && a.sidSustain == b.sidSustain
        && a.sidRelease == b.sidRelease
        && a.waveShape == b.waveShape
        && a.dmgWaveLevel == b.dmgWaveLevel
        && a.dmgStereoRoute == b.dmgStereoRoute
        && a.ymEnvelopeShape == b.ymEnvelopeShape
        && a.snNoiseMode == b.snNoiseMode
        && a.sidVoice2WaveShape == b.sidVoice2WaveShape
        && a.sidVoice3WaveShape == b.sidVoice3WaveShape;
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
    hasObservedMacroSnapshot = false;
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

    const auto readOutputGain = [this]
    {
        const auto outputDb = apvts.getRawParameterValue(chipper::parameters::id::outputDb)->load();
        return juce::Decibels::decibelsToGain(outputDb);
    };

    int renderedUntil = 0;
    for (const auto metadata : midiMessages)
    {
        const auto position = std::clamp(metadata.samplePosition, 0, buffer.getNumSamples());
        renderRange(buffer, renderedUntil, position, readOutputGain());
        handleMidiMessage(metadata.getMessage());
        renderedUntil = position;
    }

    renderRange(buffer, renderedUntil, buffer.getNumSamples(), readOutputGain());
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
        const auto note = message.getNoteNumber();
        const auto velocity = message.getFloatVelocity();
        rememberHeldNote(note, velocity);
        core->noteOn(note, velocity);
    }
    else if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        core->noteOff(note);
        forgetHeldNote(note);
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        for (const auto held : heldNotes)
            core->noteOff(held.note);
        heldNotes.clear();
    }
    else if (message.isController() && handleMidiController(message))
    {
        ensureCore();
    }
}

bool ChipperAudioProcessor::handleMidiController(const juce::MidiMessage& message)
{
    const auto* parameterId = chipper::parameters::parameterIdForMidiController(message.getControllerNumber());
    if (parameterId == nullptr)
        return false;

    return setParameterFromMidiCc(parameterId, message.getControllerValue());
}

bool ChipperAudioProcessor::setParameterFromMidiCc(const char* parameterId, int controllerValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        const auto normalized = juce::jlimit(0.0f, 1.0f, static_cast<float>(controllerValue) / 127.0f);
        parameter->setValueNotifyingHost(normalized);

        if (std::string_view(parameterId) == chipper::parameters::id::macro)
            applyCurrentMacroTemplateToParameters();

        return true;
    }

    return false;
}

bool ChipperAudioProcessor::setPlainParameterValue(const char* parameterId, float plainValue)
{
    if (auto* parameter = apvts.getParameter(parameterId))
    {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        return true;
    }

    return false;
}

void ChipperAudioProcessor::applyCurrentMacroTemplateToParameters()
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);

    if (! chipper::descriptorFor(mode).implemented)
        return;

    const auto& templ = chipper::macroTemplateFor(mode, chipper::parameters::macroFromChoice(macroChoice));
    const std::array<const char*, 4> controlIds {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };
    const std::array<const char*, 4> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled
    };
    const std::array<const char*, 4> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level
    };

    for (size_t i = 0; i < controlIds.size(); ++i)
        setPlainParameterValue(controlIds[i], templ.controls[i]);
    for (size_t i = 0; i < sourceIds.size(); ++i)
        setPlainParameterValue(sourceIds[i], templ.sourceEnabled[i] ? 1.0f : 0.0f);
    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setPlainParameterValue(sourceLevelIds[i], 1.0f);

    setPlainParameterValue(chipper::parameters::id::envelopeDecay, templ.envelopeDecay);
    setPlainParameterValue(chipper::parameters::id::sidAttack, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidDecay, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidSustain, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidRelease, 0.0f);
    setPlainParameterValue(chipper::parameters::id::stereoSpread, templ.stereoSpread);
    setPlainParameterValue(chipper::parameters::id::waveShape, static_cast<float>(templ.waveShape));
    setPlainParameterValue(chipper::parameters::id::sidVoice2WaveShape, 0.0f);
    setPlainParameterValue(chipper::parameters::id::sidVoice3WaveShape, 0.0f);
    setPlainParameterValue(chipper::parameters::id::dmgWaveLevel, 0.0f);
    setPlainParameterValue(chipper::parameters::id::dmgStereoRoute, static_cast<float>(templ.dmgStereoRoute));
    setPlainParameterValue(chipper::parameters::id::ymEnvelopeShape, static_cast<float>(templ.ymEnvelopeShape));
    setPlainParameterValue(chipper::parameters::id::snNoiseMode, static_cast<float>(templ.snNoiseMode));
}

void ChipperAudioProcessor::synchronizeMacroTemplateFromParameters()
{
    const auto modeChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::chipMode)->load()));
    const auto macroChoice = static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::macro)->load()));
    auto patch = currentPatchFromParameters();

    if (! hasObservedMacroSnapshot)
    {
        lastObservedMacroModeChoice = modeChoice;
        lastObservedMacroChoice = macroChoice;
        lastObservedMacroPatch = patch;
        hasObservedMacroSnapshot = true;
        return;
    }

    const auto changedTemplateSelector = modeChoice != lastObservedMacroModeChoice || macroChoice != lastObservedMacroChoice;
    if (changedTemplateSelector && patchControlsMatch(patch, lastObservedMacroPatch))
    {
        applyCurrentMacroTemplateToParameters();
        patch = currentPatchFromParameters();
    }

    lastObservedMacroModeChoice = modeChoice;
    lastObservedMacroChoice = macroChoice;
    lastObservedMacroPatch = patch;
}

void ChipperAudioProcessor::ensureCore()
{
    synchronizeMacroTemplateFromParameters();

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
            replayHeldNotes();
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
    replayHeldNotes();
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
        chipper::parameters::playModeFromChoice(playModeChoice),
        {
            apvts.getRawParameterValue(chipper::parameters::id::source1Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source2Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source3Enabled)->load() >= 0.5f,
            apvts.getRawParameterValue(chipper::parameters::id::source4Enabled)->load() >= 0.5f
        },
        {
            apvts.getRawParameterValue(chipper::parameters::id::source1Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source2Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source3Level)->load(),
            apvts.getRawParameterValue(chipper::parameters::id::source4Level)->load()
        },
        apvts.getRawParameterValue(chipper::parameters::id::stereoSpread)->load(),
        apvts.getRawParameterValue(chipper::parameters::id::envelopeDecay)->load(),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::waveShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::dmgWaveLevel)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::dmgStereoRoute)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::ymEnvelopeShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::snNoiseMode)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice2WaveShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidVoice3WaveShape)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidAttack)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidDecay)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidSustain)->load())),
        static_cast<int>(std::round(apvts.getRawParameterValue(chipper::parameters::id::sidRelease)->load())));
}

void ChipperAudioProcessor::replayPendingRegisterState()
{
    if (core == nullptr || pendingRegisterState.empty())
        return;

    for (const auto& write : pendingRegisterState)
        core->writeRegister(write.address, write.value);

    pendingRegisterState.clear();
}

void ChipperAudioProcessor::replayHeldNotes()
{
    if (core == nullptr)
        return;

    for (const auto held : heldNotes)
        core->noteOn(held.note, held.velocity);
}

void ChipperAudioProcessor::rememberHeldNote(int note, float velocity)
{
    const auto existing = std::find_if(heldNotes.begin(), heldNotes.end(), [note](const auto held) { return held.note == note; });
    if (existing != heldNotes.end())
    {
        existing->velocity = velocity;
        return;
    }

    heldNotes.push_back({ note, velocity });
}

void ChipperAudioProcessor::forgetHeldNote(int note)
{
    heldNotes.erase(std::remove_if(heldNotes.begin(), heldNotes.end(), [note](const auto held) { return held.note == note; }),
                    heldNotes.end());
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
        hasObservedMacroSnapshot = false;
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
