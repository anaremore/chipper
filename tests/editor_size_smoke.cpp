#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <iostream>
#include <string>
#include <typeinfo>

namespace
{
constexpr int expectedEditorHeight = 820;
constexpr int expectedEditorMinimumHeight = expectedEditorHeight;

bool expect(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "editor_size_smoke: " << message << '\n';

    return condition;
}

bool setChoiceParameter(ChipperAudioProcessor& processor, const juce::String& parameterId, int choice)
{
    auto* parameter = processor.getValueTreeState().getParameter(parameterId);
    auto* choiceParameter = dynamic_cast<juce::AudioParameterChoice*>(parameter);
    if (choiceParameter == nullptr)
    {
        std::cerr << "editor_size_smoke: missing choice parameter " << parameterId << '\n';
        return false;
    }

    const auto choiceCount = choiceParameter->choices.size();
    if (choice < 0 || choice >= choiceCount)
    {
        std::cerr << "editor_size_smoke: choice out of range for " << parameterId << '\n';
        return false;
    }

    const auto normalised = choiceCount > 1 ? static_cast<float>(choice) / static_cast<float>(choiceCount - 1) : 0.0f;
    choiceParameter->beginChangeGesture();
    choiceParameter->setValueNotifyingHost(normalised);
    choiceParameter->endChangeGesture();
    return true;
}

bool setPlainParameter(ChipperAudioProcessor& processor, const juce::String& parameterId, float plainValue)
{
    auto* parameter = processor.getValueTreeState().getParameter(parameterId);
    if (parameter == nullptr)
    {
        std::cerr << "editor_size_smoke: missing parameter " << parameterId << '\n';
        return false;
    }

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
    parameter->endChangeGesture();
    return true;
}

float plainParameterValue(const ChipperAudioProcessor& processor, const juce::String& parameterId)
{
    if (const auto* value = processor.getValueTreeState().getRawParameterValue(parameterId))
        return value->load();

    std::cerr << "editor_size_smoke: missing raw parameter " << parameterId << '\n';
    return 0.0f;
}

int chipModeChoiceFor(chipper::ChipMode target)
{
    const auto choiceCount = chipper::parameters::chipModeChoices().size();
    for (int choice = 0; choice < choiceCount; ++choice)
    {
        if (chipper::parameters::chipModeFromChoice(choice) == target)
            return choice;
    }

    return -1;
}

const char* chipModeName(chipper::ChipMode mode)
{
    switch (mode)
    {
    case chipper::ChipMode::spc700: return "SPC700";
    case chipper::ChipMode::paula: return "Paula";
    default: return "chip";
    }
}

int expectedHeightForChipMode(int chipMode)
{
    juce::ignoreUnused(chipMode);
    return expectedEditorHeight;
}

bool checkPrimaryPanelStack(const ChipperAudioProcessorEditor& editor, chipper::ChipMode mode)
{
    bool ok = true;
    auto lastBottom = 0;
    const auto footerTop = editor.getHeight() - 16 - 44;

    const auto requirePanel = [&](juce::Rectangle<int> bounds, const char* name, int minimumHeight)
    {
        if (bounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing " << name << " panel\n";
            ok = false;
            return;
        }

        if (bounds.getHeight() < minimumHeight)
        {
            std::cerr << "editor_size_smoke: " << name << " panel below useful height: "
                      << bounds.toString() << '\n';
            ok = false;
        }

        if (bounds.getY() < lastBottom)
        {
            std::cerr << "editor_size_smoke: " << name << " panel overlaps previous stack item: "
                      << bounds.toString() << '\n';
            ok = false;
        }

        lastBottom = bounds.getBottom();
    };

    requirePanel(editor.getModuleBoundsForLayoutTest(1), "source/channel", 118);

    if (mode == chipper::ChipMode::nes || mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
        requirePanel(editor.getModuleBoundsForLayoutTest(5), "sample bank", mode == chipper::ChipMode::nes ? 176 : 132);

    requirePanel(editor.getPerformanceBoundsForLayoutTest(), "performance macros", mode == chipper::ChipMode::nes ? 220 : ((mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula) ? 84 : 108));
    if (editor.getPerformanceBoundsForLayoutTest().getBottom() > footerTop)
    {
        std::cerr << "editor_size_smoke: performance panel overlaps footer reserve: "
                  << editor.getPerformanceBoundsForLayoutTest().toString() << '\n';
        ok = false;
    }

    return ok;
}

bool checkVisibleChildGeometry(const juce::Component& root,
                               const juce::Component& component,
                               juce::Point<int> parentOrigin,
                               const std::string& path)
{
    bool ok = true;

    for (auto childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
    {
        const auto* child = component.getChildComponent(childIndex);
        if (child == nullptr || ! child->isVisible())
            continue;

        const auto bounds = child->getBounds();
        const auto absoluteBounds = bounds.translated(parentOrigin.x, parentOrigin.y);
        const auto childPath = path + "/" + std::to_string(childIndex);

        if (! bounds.isEmpty())
        {
            if (! root.getLocalBounds().expanded(2).contains(absoluteBounds))
            {
                std::cerr << "editor_size_smoke: visible child out of editor bounds at "
                          << childPath << " bounds " << absoluteBounds.toString() << '\n';
                ok = false;
            }

            const auto* comboBox = dynamic_cast<const juce::ComboBox*>(child);
            const auto* textButton = dynamic_cast<const juce::TextButton*>(child);
            const auto* slider = dynamic_cast<const juce::Slider*>(child);
            if (comboBox != nullptr)
            {
                if (bounds.getWidth() < 48 || bounds.getHeight() < 28)
                {
                    std::cerr << "editor_size_smoke: dropdown below readable standard size at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString()
                              << " text \"" << comboBox->getText() << "\"\n";
                    ok = false;
                }
            }

            if (textButton != nullptr)
            {
                if (bounds.getWidth() < 24 || bounds.getHeight() < 18)
                {
                    std::cerr << "editor_size_smoke: button below readable minimum size at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString()
                              << " text \"" << textButton->getButtonText() << "\"\n";
                    ok = false;
                }
            }

            if (slider != nullptr)
            {
                const auto isHorizontalSlider = bounds.getWidth() >= bounds.getHeight();
                const auto minimumWidth = isHorizontalSlider ? 48 : 12;
                const auto minimumHeight = isHorizontalSlider ? 6 : 48;
                if (bounds.getWidth() < minimumWidth || bounds.getHeight() < minimumHeight)
                {
                    std::cerr << "editor_size_smoke: slider below readable minimum size at "
                              << childPath << " type " << typeid(*child).name()
                              << " bounds " << absoluteBounds.toString() << "\n";
                    ok = false;
                }
            }
        }

        ok &= checkVisibleChildGeometry(root, *child, absoluteBounds.getPosition(), childPath);
    }

    return ok;
}

bool expectControlOwnedBySourceChannel(const ChipperAudioProcessorEditor& editor,
                                       size_t channel,
                                       juce::Rectangle<int> controlBounds,
                                       const char* controlName)
{
    const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
    auto ok = true;

    if (sourceBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: missing source channel bounds for " << controlName << '\n';
        return false;
    }

    if (controlBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: missing channel-owned control bounds for " << controlName << '\n';
        return false;
    }

    if (controlBounds.getWidth() < 40 || controlBounds.getHeight() < 18)
    {
        std::cerr << "editor_size_smoke: channel-owned control below readable size for "
                  << controlName << " bounds " << controlBounds.toString() << '\n';
        ok = false;
    }

    if (! sourceBounds.expanded(2).contains(controlBounds))
    {
        std::cerr << "editor_size_smoke: " << controlName
                  << " is not owned by its channel card; source "
                  << sourceBounds.toString() << " control "
                  << controlBounds.toString() << '\n';
        ok = false;
    }

    return ok;
}

bool checkChannelOwnedControlLayout(chipper::ChipMode mode)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: chip mode choice unavailable for ownership layout check\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));

    switch (mode)
    {
    case chipper::ChipMode::nes:
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getPulseDutyBoundsForLayoutTest(), "NES pulse 1 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 1, editor.getPulse2DutyBoundsForLayoutTest(), "NES pulse 2 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeBoundsForLayoutTest(), "NES noise mode");
        break;

    case chipper::ChipMode::dmg:
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getPulseDutyBoundsForLayoutTest(), "DMG pulse 1 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 1, editor.getPulse2DutyBoundsForLayoutTest(), "DMG pulse 2 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 2, editor.getDmgWaveLevelBoundsForLayoutTest(), "DMG wave level");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeBoundsForLayoutTest(), "DMG noise mode");
        break;

    case chipper::ChipMode::sn76489:
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeMenuBoundsForLayoutTest(), "SN76489 noise mode");
        break;

    default:
        break;
    }

    return ok;
}

bool checkWavetableSourceDeck(chipper::ChipMode mode)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: wavetable chip mode choice unavailable\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));

    const auto visibleSources = chipper::visibleSourceCountForMode(mode);
    for (size_t channel = 0; channel < visibleSources; ++channel)
    {
        const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
        const auto levelBounds = editor.getSourceLevelBoundsForLayoutTest(channel);
        const auto waveSelectorBounds = editor.getSourceWaveSelectorBoundsForLayoutTest(channel);

        if (sourceBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing wavetable source card for channel "
                      << channel << '\n';
            ok = false;
            continue;
        }

        if (sourceBounds.getHeight() > 120)
        {
            std::cerr << "editor_size_smoke: wavetable source card grew into empty vertical space: "
                      << sourceBounds.toString() << '\n';
            ok = false;
        }

        if (levelBounds.isEmpty() || levelBounds.getHeight() < 16 || ! sourceBounds.expanded(2).contains(levelBounds))
        {
            std::cerr << "editor_size_smoke: wavetable level lane is not readable/owned for channel "
                      << channel << " source " << sourceBounds.toString()
                      << " level " << levelBounds.toString() << '\n';
            ok = false;
        }

        if (waveSelectorBounds.isEmpty()
            || waveSelectorBounds.getHeight() < 28
            || waveSelectorBounds.getWidth() < 96
            || ! sourceBounds.expanded(2).contains(waveSelectorBounds))
        {
            std::cerr << "editor_size_smoke: wavetable wave selector is not standard-size/owned for channel "
                      << channel << " source " << sourceBounds.toString()
                      << " selector " << waveSelectorBounds.toString() << '\n';
            ok = false;
        }

        if (! levelBounds.isEmpty()
            && ! waveSelectorBounds.isEmpty()
            && levelBounds.getY() - waveSelectorBounds.getBottom() > 28)
        {
            std::cerr << "editor_size_smoke: wavetable level lane drifted away from its selector for channel "
                      << channel << " selector " << waveSelectorBounds.toString()
                      << " level " << levelBounds.toString() << '\n';
            ok = false;
        }
    }

    return ok;
}

bool checkChipSwitchPreservesEditorSettings()
{
    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);
    editor.runEditorUpdateForLayoutTest();

    auto ok = true;
    const auto nesChoice = chipModeChoiceFor(chipper::ChipMode::nes);
    const auto sidChoice = chipModeChoiceFor(chipper::ChipMode::sid);
    ok &= expect(nesChoice >= 0 && sidChoice >= 0, "NES/SID choices unavailable for chip-switch preservation check");

    ok &= setPlainParameter(processor, chipper::parameters::id::macroControl1, 0.73f);
    ok &= setPlainParameter(processor, chipper::parameters::id::outputDb, -12.0f);
    ok &= setChoiceParameter(processor, chipper::parameters::id::pulse2Duty, 3);

    ok &= setChoiceParameter(processor, chipper::parameters::id::chipMode, sidChoice);
    editor.runEditorUpdateForLayoutTest();
    ok &= setPlainParameter(processor, chipper::parameters::id::macroControl1, 0.18f);
    ok &= setPlainParameter(processor, chipper::parameters::id::outputDb, -6.0f);
    ok &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 2);

    ok &= setChoiceParameter(processor, chipper::parameters::id::chipMode, nesChoice);
    editor.runEditorUpdateForLayoutTest();
    ok &= expect(std::abs(plainParameterValue(processor, chipper::parameters::id::macroControl1) - 0.73f) < 0.001f,
                 "switching back to NES should restore its local macro control value");
    ok &= expect(std::abs(plainParameterValue(processor, chipper::parameters::id::outputDb) - -12.0f) < 0.001f,
                 "switching back to NES should restore its local output trim");
    ok &= expect(std::abs(plainParameterValue(processor, chipper::parameters::id::pulse2Duty) - 3.0f) < 0.001f,
                 "switching back to NES should restore its local pulse 2 duty choice");

    ok &= setChoiceParameter(processor, chipper::parameters::id::chipMode, sidChoice);
    editor.runEditorUpdateForLayoutTest();
    ok &= expect(std::abs(plainParameterValue(processor, chipper::parameters::id::macroControl1) - 0.18f) < 0.001f,
                 "switching back to SID should restore its local macro control value");
    ok &= expect(std::abs(plainParameterValue(processor, chipper::parameters::id::outputDb) - -6.0f) < 0.001f,
                 "switching back to SID should restore its local output trim");
    ok &= expect(std::abs(plainParameterValue(processor, chipper::parameters::id::waveShape) - 2.0f) < 0.001f,
                 "switching back to SID should restore its local waveform choice");

    return ok;
}

bool checkSamplerSourceDeck(chipper::ChipMode mode)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: sampler chip mode choice unavailable\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));

    const auto visibleSources = chipper::visibleSourceCountForMode(mode);
    for (size_t channel = 0; channel < visibleSources; ++channel)
    {
        const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
        const auto levelBounds = editor.getSourceLevelBoundsForLayoutTest(channel);
        const auto waveSelectorBounds = editor.getSourceWaveSelectorBoundsForLayoutTest(channel);
        const auto sampleSelectorBounds = editor.getPaulaSourceSampleSelectorBoundsForLayoutTest(channel);

        if (sourceBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing sampler source card for channel "
                      << channel << '\n';
            ok = false;
            continue;
        }

        if (sourceBounds.getHeight() < 90)
        {
            std::cerr << "editor_size_smoke: sampler source card below useful height: "
                      << sourceBounds.toString() << '\n';
            ok = false;
        }

        if (sourceBounds.getHeight() > 150)
        {
            std::cerr << "editor_size_smoke: sampler source card grew into empty vertical space: "
                      << sourceBounds.toString() << '\n';
            ok = false;
        }

        if (waveSelectorBounds.isEmpty()
            || waveSelectorBounds.getHeight() < 28
            || waveSelectorBounds.getWidth() < 96
            || ! sourceBounds.expanded(2).contains(waveSelectorBounds))
        {
            std::cerr << "editor_size_smoke: sampler wave/sample selector is not readable/owned for channel "
                      << channel << " source " << sourceBounds.toString()
                      << " selector " << waveSelectorBounds.toString() << '\n';
            ok = false;
        }

        if (mode == chipper::ChipMode::paula
            && (sampleSelectorBounds.isEmpty()
                || sampleSelectorBounds.getHeight() < 28
                || sampleSelectorBounds.getWidth() < 96
                || ! sourceBounds.expanded(2).contains(sampleSelectorBounds)))
        {
            std::cerr << "editor_size_smoke: Paula sample selector is not readable/owned for channel "
                      << channel << " source " << sourceBounds.toString()
                      << " selector " << sampleSelectorBounds.toString() << '\n';
            ok = false;
        }

        if (levelBounds.isEmpty() || levelBounds.getHeight() < 10 || ! sourceBounds.expanded(2).contains(levelBounds))
        {
            std::cerr << "editor_size_smoke: sampler level lane is not readable/owned for channel "
                      << channel << " source " << sourceBounds.toString()
                      << " level " << levelBounds.toString() << '\n';
            ok = false;
        }

        const auto selectorForLevel = mode == chipper::ChipMode::paula && ! sampleSelectorBounds.isEmpty()
            ? sampleSelectorBounds
            : waveSelectorBounds;
        if (! levelBounds.isEmpty()
            && ! selectorForLevel.isEmpty()
            && levelBounds.getY() - selectorForLevel.getBottom() > 28)
        {
            std::cerr << "editor_size_smoke: sampler level lane drifted away from its selector for channel "
                      << channel << " selector " << selectorForLevel.toString()
                      << " level " << levelBounds.toString() << '\n';
            ok = false;
        }
    }

    return ok;
}

bool checkSamplerBankLayout(chipper::ChipMode mode)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: sampler chip mode choice unavailable for sample-bank check\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));

    const auto sampleBankBounds = editor.getSampleBankBoundsForLayoutTest();
    const auto playbackBounds = editor.getSamplePlaybackModeBoundsForLayoutTest();
    const auto slotBounds = editor.getSampleSlotBoundsForLayoutTest();
    const auto rootBounds = editor.getSampleRootBoundsForLayoutTest();
    const auto waveformBounds = editor.getSampleWaveformBoundsForLayoutTest();
    const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();

    const auto expectOwnedStandardControl = [&](juce::Rectangle<int> bounds, const char* name)
    {
        if (bounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing " << name << " in sample bank\n";
            ok = false;
            return;
        }

        if (bounds.getHeight() < 28 || bounds.getWidth() < 72)
        {
            std::cerr << "editor_size_smoke: " << chipModeName(mode) << ' ' << name
                      << " below standard sample-bank control size: "
                      << bounds.toString() << '\n';
            ok = false;
        }

        if (! sampleBankBounds.expanded(2).contains(bounds))
        {
            std::cerr << "editor_size_smoke: " << name
                      << " is not owned by the sample bank; sample bank "
                      << sampleBankBounds.toString() << " control "
                      << bounds.toString() << '\n';
            ok = false;
        }
    };

    expectOwnedStandardControl(playbackBounds, "sample playback mode");
    expectOwnedStandardControl(slotBounds, "sample slot");
    expectOwnedStandardControl(rootBounds, "sample root");

    if (mode == chipper::ChipMode::spc700)
        expectOwnedStandardControl(editor.getSampleLoopToggleBoundsForLayoutTest(), "SPC700 loop toggle");

    if (waveformBounds.isEmpty() || waveformBounds.getHeight() < 108 || waveformBounds.getWidth() < 420)
    {
        std::cerr << "editor_size_smoke: " << chipModeName(mode)
                  << " sample waveform preview below useful size: "
                  << waveformBounds.toString() << '\n';
        ok = false;
    }

    if (! sampleBankBounds.expanded(2).contains(waveformBounds))
    {
        std::cerr << "editor_size_smoke: sample waveform preview is not owned by sample bank; sample bank "
                  << sampleBankBounds.toString() << " waveform "
                  << waveformBounds.toString() << '\n';
        ok = false;
    }

    if (! performanceBounds.isEmpty() && sampleBankBounds.getBottom() > performanceBounds.getY())
    {
        std::cerr << "editor_size_smoke: sample bank overlaps performance macros: sample bank "
                  << sampleBankBounds.toString() << " performance "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    return ok;
}

bool checkNesDmcAndPerformanceLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::nes);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: NES chip mode choice unavailable for DMC layout check\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedEditorHeight);

    const auto sampleBankBounds = editor.getSampleBankBoundsForLayoutTest();
    const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
    const auto playbackBounds = editor.getSamplePlaybackModeBoundsForLayoutTest();
    const auto slotBounds = editor.getSampleSlotBoundsForLayoutTest();
    const auto rootBounds = editor.getSampleRootBoundsForLayoutTest();
    const auto loopBounds = editor.getDmcLoopToggleBoundsForLayoutTest();
    const auto rateBounds = editor.getDmcRateBoundsForLayoutTest();
    const auto waveformBounds = editor.getSampleWaveformBoundsForLayoutTest();
    const auto apuDecayBounds = editor.getEnvelopeDecayBoundsForLayoutTest();
    const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();
    const auto scopeBounds = editor.getOutputScopeBoundsForLayoutTest();

    if (performanceBounds.isEmpty() || performanceBounds.getHeight() < 220)
    {
        std::cerr << "editor_size_smoke: NES performance macros strip below usable height: "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    const auto expectOwnedReadable = [&](juce::Rectangle<int> bounds,
                                         const char* name,
                                         int minWidth,
                                         int minHeight)
    {
        if (bounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing NES " << name << '\n';
            ok = false;
            return;
        }

        if (bounds.getWidth() < minWidth || bounds.getHeight() < minHeight)
        {
            std::cerr << "editor_size_smoke: NES " << name
                      << " below readable size: " << bounds.toString() << '\n';
            ok = false;
        }

        if (! sampleBankBounds.expanded(2).contains(bounds))
        {
            std::cerr << "editor_size_smoke: NES " << name
                      << " is not owned by the DMC sample bank; sample bank "
                      << sampleBankBounds.toString() << " control "
                      << bounds.toString() << '\n';
            ok = false;
        }
    };

    expectOwnedReadable(playbackBounds, "playback mode", 112, 28);
    expectOwnedReadable(slotBounds, "sample slot", 240, 28);
    expectOwnedReadable(rootBounds, "root note", 72, 28);
    expectOwnedReadable(loopBounds, "loop toggle", 104, 28);
    expectOwnedReadable(rateBounds, "DMC rate", 136, 28);
    expectOwnedReadable(waveformBounds, "waveform preview", 520, 150);

    if (! performanceBounds.expanded(2).contains(apuDecayBounds)
        || apuDecayBounds.getWidth() < 160
        || apuDecayBounds.getHeight() < 18)
    {
        std::cerr << "editor_size_smoke: NES APU decay slider is not readable/owned by performance macros: "
                  << apuDecayBounds.toString() << " performance "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    if (! performanceBounds.expanded(2).contains(outputBounds)
        || outputBounds.getWidth() < 160
        || outputBounds.getHeight() < 18)
    {
        std::cerr << "editor_size_smoke: NES output slider is not readable/owned by performance macros: "
                  << outputBounds.toString() << " performance "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    if (! performanceBounds.expanded(2).contains(scopeBounds)
        || scopeBounds.getWidth() < 420
        || scopeBounds.getHeight() < 28)
    {
        std::cerr << "editor_size_smoke: NES output scope is not readable/owned by performance macros: "
                  << scopeBounds.toString() << " performance "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    if (apuDecayBounds.intersects(outputBounds) || apuDecayBounds.intersects(scopeBounds))
    {
        std::cerr << "editor_size_smoke: NES APU decay overlaps output controls: decay "
                  << apuDecayBounds.toString() << " output "
                  << outputBounds.toString() << " scope "
                  << scopeBounds.toString() << '\n';
        ok = false;
    }

    if (! performanceBounds.isEmpty() && sampleBankBounds.getBottom() > performanceBounds.getY())
    {
        std::cerr << "editor_size_smoke: NES DMC sample bank overlaps performance macros: sample bank "
                  << sampleBankBounds.toString() << " performance "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    return ok;
}

}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;

    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);

    bool ok = true;
    ok &= expect(editor.getWidth() == 1240, "unexpected default width");
    ok &= expect(editor.getHeight() == expectedHeightForChipMode(0), "unexpected default height");

    editor.setSize(1240, 1200);
    ok &= expect(editor.getHeight() == expectedEditorHeight, "host-restored default editor height was not clamped to the DAW-friendly cap");

    editor.setSize(1000, 600);
    ok &= expect(editor.getWidth() >= 1180, "editor width was not clamped to minimum");
    ok &= expect(editor.getHeight() >= expectedEditorMinimumHeight, "editor height was not clamped to minimum");

    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        const auto chipPath = chipper::parameters::chipModeChoices()[chipMode].toStdString();
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        ok &= expect(chipEditor.getWidth() == 1240, "chip default width changed");
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip default height changed");
        ok &= expect(chipEditor.getHeight() <= expectedEditorHeight, "chip default height exceeded DAW-friendly cap");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/" + chipPath);
        ok &= checkPrimaryPanelStack(chipEditor, chipper::parameters::chipModeFromChoice(chipMode));
        chipEditor.setSize(1240, 1200);
        ok &= expect(chipEditor.getHeight() == expectedEditorHeight, "chip-switched editor height was not clamped to the DAW-friendly cap");
        ok &= expect(chipEditor.getWidth() == 1240, "chip-switched editor width unexpectedly changed");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/restored/" + chipPath);
        ok &= checkPrimaryPanelStack(chipEditor, chipper::parameters::chipModeFromChoice(chipMode));
    }

    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nes);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::dmg);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::sn76489);
    ok &= checkWavetableSourceDeck(chipper::ChipMode::huc6280);
    ok &= checkWavetableSourceDeck(chipper::ChipMode::namcoWsg);
    ok &= checkWavetableSourceDeck(chipper::ChipMode::scc);
    ok &= checkSamplerSourceDeck(chipper::ChipMode::spc700);
    ok &= checkSamplerSourceDeck(chipper::ChipMode::paula);
    ok &= checkSamplerBankLayout(chipper::ChipMode::spc700);
    ok &= checkSamplerBankLayout(chipper::ChipMode::paula);
    ok &= checkNesDmcAndPerformanceLayout();
    ok &= checkChipSwitchPreservesEditorSettings();

    return ok ? 0 : 1;
}
