#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>
#include <typeinfo>

namespace
{
constexpr int expectedEditorHeight = 860;
constexpr int expectedEditorSidHeight = 880;
constexpr int expectedEditorMinimumWidth = 1180;
constexpr int expectedEditorMaximumHeight = expectedEditorSidHeight;

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
    case chipper::ChipMode::nesVrc6: return "NES + VRC6";
    case chipper::ChipMode::spc700: return "SPC700";
    case chipper::ChipMode::paula: return "Paula";
    case chipper::ChipMode::ym2203: return "YM2203";
    default: return "chip";
    }
}

int expectedHeightForChipMode(int chipMode)
{
    const auto mode = chipper::parameters::chipModeFromChoice(chipMode);
    if (mode == chipper::ChipMode::sid)
        return expectedEditorSidHeight;

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

    const auto isNesFamily = mode == chipper::ChipMode::nes || mode == chipper::ChipMode::nesVrc6;
    if (isNesFamily || mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
        requirePanel(editor.getModuleBoundsForLayoutTest(5), "sample bank", isNesFamily ? 176 : 132);

    const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
    const auto minimumPerformanceHeight = isNesFamily ? 220 : (mode == chipper::ChipMode::sid ? 96 : ((mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula) ? 84 : 108));
    requirePanel(performanceBounds, "performance macros", minimumPerformanceHeight);
    if (performanceBounds.getBottom() > footerTop)
    {
        std::cerr << "editor_size_smoke: performance panel overlaps footer reserve: "
                  << performanceBounds.toString() << '\n';
        ok = false;
    }

    for (size_t moduleIndex = 0; moduleIndex < 6; ++moduleIndex)
    {
        const auto bounds = editor.getModuleBoundsForLayoutTest(moduleIndex);
        if (bounds.isEmpty() || performanceBounds.isEmpty())
            continue;

        if (bounds.getBottom() > performanceBounds.getY())
        {
            std::cerr << "editor_size_smoke: module panel overlaps performance macros: module "
                      << moduleIndex << ' ' << bounds.toString()
                      << " performance " << performanceBounds.toString() << '\n';
            ok = false;
        }
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

bool expectVisibleSourceCardsInsideDeck(const ChipperAudioProcessorEditor& editor,
                                        chipper::ChipMode mode)
{
    const auto sourceDeckBounds = editor.getModuleBoundsForLayoutTest(1);
    auto ok = true;

    if (sourceDeckBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: missing source deck for visible source-card containment check\n";
        return false;
    }

    const auto visibleSources = chipper::visibleSourceCountForMode(mode);
    for (size_t channel = 0; channel < visibleSources; ++channel)
    {
        const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
        if (sourceBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing visible source card " << channel
                      << " for " << chipper::toString(mode) << '\n';
            ok = false;
            continue;
        }

        if (! sourceDeckBounds.expanded(2).contains(sourceBounds))
        {
            std::cerr << "editor_size_smoke: source card " << channel
                      << " escapes the source deck for " << chipper::toString(mode)
                      << "; deck " << sourceDeckBounds.toString()
                      << " card " << sourceBounds.toString() << '\n';
            ok = false;
        }
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
    case chipper::ChipMode::nesVrc6:
        if (mode == chipper::ChipMode::nesVrc6)
            ok &= expectVisibleSourceCardsInsideDeck(editor, mode);
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
        if (! editor.getModuleBoundsForLayoutTest(2).isEmpty())
        {
            std::cerr << "editor_size_smoke: SN76489 should not show a separate tone/noise module once Noise Mode is owned by the Noise channel\n";
            ok = false;
        }
        break;

    default:
        break;
    }

    return ok;
}

bool checkYm2149ToneNoiseMixLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2149);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2149 chip mode choice unavailable\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));
    editor.runEditorUpdateForLayoutTest();

    for (size_t channel = 0; channel < 3; ++channel)
    {
        const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
        const auto mixBounds = editor.getYmChannelMixBoundsForLayoutTest(channel);
        const auto levelBounds = editor.getSourceLevelBoundsForLayoutTest(channel);

        if (sourceBounds.isEmpty() || mixBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: YM2149 channel " << channel
                      << " mix control is missing from its source card\n";
            ok = false;
            continue;
        }

        if (! sourceBounds.expanded(2).contains(mixBounds))
        {
            std::cerr << "editor_size_smoke: YM2149 channel " << channel
                      << " mix control is not owned by its source card: control "
                      << mixBounds.toString() << " source " << sourceBounds.toString() << '\n';
            ok = false;
        }

        if (mixBounds.getWidth() < 96 || mixBounds.getHeight() < 24)
        {
            std::cerr << "editor_size_smoke: YM2149 channel " << channel
                      << " mix control below readable source-card size: " << mixBounds.toString() << '\n';
            ok = false;
        }

        if (! levelBounds.isEmpty() && mixBounds.intersects(levelBounds))
        {
            std::cerr << "editor_size_smoke: YM2149 channel " << channel
                      << " mix control overlaps its level lane: mix " << mixBounds.toString()
                      << " level " << levelBounds.toString() << '\n';
            ok = false;
        }
    }

    const auto mixerBounds = editor.getModuleBoundsForLayoutTest(2);
    const auto envelopeModuleBounds = editor.getModuleBoundsForLayoutTest(3);
    const auto outputModuleBounds = editor.getModuleBoundsForLayoutTest(5);
    const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
    const auto toneNoiseBounds = editor.getToneNoiseMixBoundsForLayoutTest();
    const auto noisePitchBounds = editor.getNativeSliderBoundsForLayoutTest(2);
    const auto retiredMacroSliderBounds = editor.getNativeSliderBoundsForLayoutTest(3);
    const auto legacyReadoutBounds = editor.getNativeValueLabelBoundsForLayoutTest(3);
    const auto envelopeShapeBounds = editor.getYmEnvelopeShapeBoundsForLayoutTest();
    const auto envelopeSpeedBounds = editor.getEnvelopeDecayBoundsForLayoutTest();
    const auto envelopePreviewBounds = editor.getYmEnvelopePreviewBoundsForLayoutTest();

    if (mixerBounds.isEmpty() || mixerBounds.getHeight() < 160)
    {
        std::cerr << "editor_size_smoke: YM2149 mixer module is missing useful space: "
                  << mixerBounds.toString() << '\n';
        ok = false;
    }

    if (toneNoiseBounds.isEmpty() || toneNoiseBounds.getWidth() < 240 || toneNoiseBounds.getHeight() < 24)
    {
        std::cerr << "editor_size_smoke: YM2149 mixer tone/noise mix is not readable: "
                  << toneNoiseBounds.toString() << '\n';
        ok = false;
    }

    if (! mixerBounds.expanded(2).contains(toneNoiseBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 tone/noise mix escaped mixer module: control "
                  << toneNoiseBounds.toString() << " mixer " << mixerBounds.toString() << '\n';
        ok = false;
    }

    if (noisePitchBounds.isEmpty()
        || noisePitchBounds.getWidth() < 180
        || noisePitchBounds.getHeight() < 18
        || ! mixerBounds.expanded(2).contains(noisePitchBounds)
        || noisePitchBounds.intersects(toneNoiseBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 noise pitch is not readable/owned by mixer module: noise "
                  << noisePitchBounds.toString() << " tone/noise " << toneNoiseBounds.toString()
                  << " mixer " << mixerBounds.toString() << '\n';
        ok = false;
    }

    if (! legacyReadoutBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: YM2149 hidden tone/noise macro readout still occupies space: "
                  << legacyReadoutBounds.toString() << '\n';
        ok = false;
    }

    if (! retiredMacroSliderBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: YM2149 tone/noise macro slider still occupies performance space: "
                  << retiredMacroSliderBounds.toString() << '\n';
        ok = false;
    }

    if (envelopeModuleBounds.isEmpty() || envelopeModuleBounds.getHeight() < 160)
    {
        std::cerr << "editor_size_smoke: YM2149 envelope module is missing useful space: "
                  << envelopeModuleBounds.toString() << '\n';
        ok = false;
    }

    if (envelopeShapeBounds.isEmpty()
        || envelopeShapeBounds.getWidth() < 180
        || envelopeShapeBounds.getHeight() < 28
        || ! envelopeModuleBounds.expanded(2).contains(envelopeShapeBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 envelope shape menu is not readable/owned by envelope module: shape "
                  << envelopeShapeBounds.toString() << " envelope " << envelopeModuleBounds.toString() << '\n';
        ok = false;
    }

    if (envelopeSpeedBounds.isEmpty()
        || envelopeSpeedBounds.getWidth() < 180
        || envelopeSpeedBounds.getHeight() < 18
        || ! envelopeModuleBounds.expanded(2).contains(envelopeSpeedBounds)
        || envelopeSpeedBounds.intersects(envelopeShapeBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 envelope speed is not readable/owned by envelope module: speed "
                  << envelopeSpeedBounds.toString() << " shape " << envelopeShapeBounds.toString()
                  << " envelope " << envelopeModuleBounds.toString() << '\n';
        ok = false;
    }

    if (envelopePreviewBounds.isEmpty()
        || envelopePreviewBounds.getWidth() < 180
        || envelopePreviewBounds.getHeight() < 24
        || ! envelopeModuleBounds.expanded(2).contains(envelopePreviewBounds)
        || envelopePreviewBounds.intersects(envelopeSpeedBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 envelope preview is not readable/owned by envelope module: preview "
                  << envelopePreviewBounds.toString() << " speed " << envelopeSpeedBounds.toString()
                  << " envelope " << envelopeModuleBounds.toString() << '\n';
        ok = false;
    }

    if (! outputModuleBounds.isEmpty()
        && ((! mixerBounds.isEmpty() && mixerBounds.getBottom() > outputModuleBounds.getY())
            || (! envelopeModuleBounds.isEmpty() && envelopeModuleBounds.getBottom() > outputModuleBounds.getY())))
    {
        std::cerr << "editor_size_smoke: YM2149 middle modules overlap the output module: mixer "
                  << mixerBounds.toString() << " envelope " << envelopeModuleBounds.toString()
                  << " output " << outputModuleBounds.toString() << '\n';
        ok = false;
    }

    if (! performanceBounds.isEmpty() && ! outputModuleBounds.isEmpty() && outputModuleBounds.getBottom() > performanceBounds.getY())
    {
        std::cerr << "editor_size_smoke: YM2149 output module overlaps performance macros: output "
                  << outputModuleBounds.toString() << " performance " << performanceBounds.toString() << '\n';
        ok = false;
    }

    return ok;
}

bool checkYm2612DacModeLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2612);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2612 chip mode choice unavailable\n";
        return false;
    }

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));
    editor.runEditorUpdateForLayoutTest();

    const auto dacBounds = editor.getSnNoiseModeBoundsForLayoutTest();
    if (dacBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: YM2612 DAC mode control is missing\n";
        ok = false;
    }
    else if (dacBounds.getWidth() < 240 || dacBounds.getHeight() < 20)
    {
        std::cerr << "editor_size_smoke: YM2612 DAC mode control below readable size: "
                  << dacBounds.toString() << '\n';
        ok = false;
    }

    return ok;
}

bool checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode mode)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: four-operator FM chip mode choice unavailable\n";
        return false;
    }
    const auto modeLabel = chipper::parameters::chipModeChoices()[chipChoice].toStdString();

    ChipperAudioProcessor processor;
    auto ok = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
    ok &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);

    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(1240, expectedHeightForChipMode(chipChoice));
    editor.runEditorUpdateForLayoutTest();

    const auto envelopeModuleBounds = editor.getModuleBoundsForLayoutTest(3);
    if (envelopeModuleBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: " << modeLabel
                  << " operator surface module is missing\n";
        return false;
    }

    const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
    const auto checkOperatorControl = [&](size_t sliderIndex, const char* controlName)
    {
        auto controlOk = true;
        const auto sliderBounds = editor.getNativeSliderBoundsForLayoutTest(sliderIndex);
        const auto groupBounds = editor.getNativeGroupLabelBoundsForLayoutTest(sliderIndex);
        const auto labelBounds = editor.getNativeLabelBoundsForLayoutTest(sliderIndex);
        const auto valueBounds = editor.getNativeValueLabelBoundsForLayoutTest(sliderIndex);

        if (! groupBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: " << modeLabel << ' ' << controlName
                      << " should use compact Operator EG ownership without a macro group label: "
                      << groupBounds.toString() << '\n';
            controlOk = false;
        }

        if (sliderBounds.isEmpty() || labelBounds.isEmpty() || valueBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: " << modeLabel << ' ' << controlName
                      << " operator control is missing from Operator EG: slider "
                      << sliderBounds.toString() << " label " << labelBounds.toString()
                      << " readout " << valueBounds.toString() << '\n';
            return false;
        }

        if (! envelopeModuleBounds.expanded(2).contains(sliderBounds)
            || ! envelopeModuleBounds.expanded(2).contains(labelBounds)
            || ! envelopeModuleBounds.expanded(2).contains(valueBounds))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << ' ' << controlName
                      << " operator control escaped Operator EG: slider "
                      << sliderBounds.toString() << " label " << labelBounds.toString()
                      << " readout " << valueBounds.toString()
                      << " module " << envelopeModuleBounds.toString() << '\n';
            controlOk = false;
        }

        if (! performanceBounds.isEmpty()
            && (sliderBounds.intersects(performanceBounds)
                || labelBounds.intersects(performanceBounds)
                || valueBounds.intersects(performanceBounds)))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << ' ' << controlName
                      << " operator control still occupies Performance Macros: slider "
                      << sliderBounds.toString() << " performance "
                      << performanceBounds.toString() << '\n';
            controlOk = false;
        }

        if (sliderBounds.getWidth() < 160 || sliderBounds.getHeight() < 18
            || labelBounds.getWidth() < 72 || labelBounds.getHeight() < 12
            || valueBounds.getWidth() < 58 || valueBounds.getHeight() < 12
            || sliderBounds.intersects(labelBounds)
            || sliderBounds.intersects(valueBounds))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << ' ' << controlName
                      << " operator control is not readable: slider "
                      << sliderBounds.toString() << " label " << labelBounds.toString()
                      << " readout " << valueBounds.toString() << '\n';
            controlOk = false;
        }

        return controlOk;
    };

    ok &= checkOperatorControl(2, "Operator Tone");
    ok &= checkOperatorControl(3, "FM Level");

    const auto operatorToneBounds = editor.getNativeSliderBoundsForLayoutTest(2);
    const auto fmLevelBounds = editor.getNativeSliderBoundsForLayoutTest(3);

    auto carrierCount = 0;
    auto modulatorCount = 0;
    auto lastBottom = envelopeModuleBounds.getY();

    for (size_t op = 0; op < 4; ++op)
    {
        const auto nameBounds = editor.getFmOperatorNameBoundsForLayoutTest(op);
        const auto valueBounds = editor.getFmOperatorValueBoundsForLayoutTest(op);
        const auto levelSliderBounds = editor.getFmOperatorLevelSliderBoundsForLayoutTest(op);
        const auto multiplierBounds = editor.getFmOperatorMultiplierBoundsForLayoutTest(op);
        const auto attackRateBounds = editor.getFmOperatorAttackRateBoundsForLayoutTest(op);
        const auto levelValueBounds = editor.getFmOperatorLevelValueBoundsForLayoutTest(op);
        const auto nameText = editor.getFmOperatorNameTextForLayoutTest(op);
        const auto valueText = editor.getFmOperatorValueTextForLayoutTest(op);
        const auto levelValueText = editor.getFmOperatorLevelValueTextForLayoutTest(op);

        if (nameBounds.isEmpty() || valueBounds.isEmpty()
            || levelSliderBounds.isEmpty() || multiplierBounds.isEmpty()
            || attackRateBounds.isEmpty() || levelValueBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " is missing from the FM readout/control surface: name "
                      << nameBounds.toString() << " level readout "
                      << levelValueBounds.toString() << " slider "
                      << levelSliderBounds.toString() << " multiplier "
                      << multiplierBounds.toString() << " attack "
                      << attackRateBounds.toString() << " value "
                      << valueBounds.toString() << '\n';
            ok = false;
            continue;
        }

        if (! envelopeModuleBounds.expanded(2).contains(nameBounds)
            || ! envelopeModuleBounds.expanded(2).contains(levelValueBounds)
            || ! envelopeModuleBounds.expanded(2).contains(levelSliderBounds)
            || ! envelopeModuleBounds.expanded(2).contains(multiplierBounds)
            || ! envelopeModuleBounds.expanded(2).contains(attackRateBounds)
            || ! envelopeModuleBounds.expanded(2).contains(valueBounds))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " escaped the envelope/FM module: module " << envelopeModuleBounds.toString()
                      << " name " << nameBounds.toString()
                      << " level readout " << levelValueBounds.toString()
                      << " slider " << levelSliderBounds.toString()
                      << " multiplier " << multiplierBounds.toString()
                      << " attack " << attackRateBounds.toString()
                      << " value "
                      << valueBounds.toString() << '\n';
            ok = false;
        }

        if (nameBounds.getWidth() < 40 || nameBounds.getHeight() < 12
            || levelValueBounds.getWidth() < 30 || levelValueBounds.getHeight() < 12
            || levelSliderBounds.getWidth() < 56 || levelSliderBounds.getHeight() < 12
            || multiplierBounds.getWidth() < 46 || multiplierBounds.getHeight() < 12
            || attackRateBounds.getWidth() < 38 || attackRateBounds.getHeight() < 12
            || valueBounds.getWidth() < 92 || valueBounds.getHeight() < 12)
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " is below readable size: name " << nameBounds.toString()
                      << " level readout " << levelValueBounds.toString()
                      << " slider " << levelSliderBounds.toString()
                      << " multiplier " << multiplierBounds.toString()
                      << " attack " << attackRateBounds.toString()
                      << " value " << valueBounds.toString() << '\n';
            ok = false;
        }

        if (nameBounds.intersects(operatorToneBounds)
            || valueBounds.intersects(operatorToneBounds)
            || levelValueBounds.intersects(operatorToneBounds)
            || levelSliderBounds.intersects(operatorToneBounds)
            || multiplierBounds.intersects(operatorToneBounds)
            || attackRateBounds.intersects(operatorToneBounds)
            || nameBounds.intersects(fmLevelBounds)
            || valueBounds.intersects(fmLevelBounds)
            || levelValueBounds.intersects(fmLevelBounds)
            || levelSliderBounds.intersects(fmLevelBounds)
            || multiplierBounds.intersects(fmLevelBounds)
            || attackRateBounds.intersects(fmLevelBounds))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " overlaps editable operator controls: name " << nameBounds.toString()
                      << " level readout " << levelValueBounds.toString()
                      << " slider " << levelSliderBounds.toString()
                      << " multiplier " << multiplierBounds.toString()
                      << " attack " << attackRateBounds.toString()
                      << " value " << valueBounds.toString()
                      << " tone " << operatorToneBounds.toString()
                      << " level " << fmLevelBounds.toString() << '\n';
            ok = false;
        }

        if (nameBounds.intersects(levelValueBounds)
            || nameBounds.intersects(levelSliderBounds)
            || nameBounds.intersects(multiplierBounds)
            || nameBounds.intersects(attackRateBounds)
            || nameBounds.intersects(valueBounds)
            || levelValueBounds.intersects(levelSliderBounds)
            || levelValueBounds.intersects(multiplierBounds)
            || levelValueBounds.intersects(attackRateBounds)
            || levelValueBounds.intersects(valueBounds)
            || levelSliderBounds.intersects(multiplierBounds)
            || levelSliderBounds.intersects(attackRateBounds)
            || levelSliderBounds.intersects(valueBounds)
            || multiplierBounds.intersects(attackRateBounds)
            || multiplierBounds.intersects(valueBounds)
            || attackRateBounds.intersects(valueBounds))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " has overlapping row controls: name " << nameBounds.toString()
                      << " level readout " << levelValueBounds.toString()
                      << " slider " << levelSliderBounds.toString()
                      << " multiplier " << multiplierBounds.toString()
                      << " attack " << attackRateBounds.toString()
                      << " value " << valueBounds.toString() << '\n';
            ok = false;
        }

        if (nameBounds.getY() < lastBottom - 1
            || valueBounds.getY() < lastBottom - 1
            || levelValueBounds.getY() < lastBottom - 1
            || levelSliderBounds.getY() < lastBottom - 1
            || multiplierBounds.getY() < lastBottom - 1
            || attackRateBounds.getY() < lastBottom - 1)
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " overlaps the previous operator row\n";
            ok = false;
        }
        lastBottom = std::max({ nameBounds.getBottom(), levelValueBounds.getBottom(), levelSliderBounds.getBottom(), multiplierBounds.getBottom(), attackRateBounds.getBottom(), valueBounds.getBottom() });

        const auto expectedPrefix = juce::String("OP") + juce::String(static_cast<int>(op + 1u));
        if (! nameText.startsWith(expectedPrefix) || (! nameText.endsWith(" C") && ! nameText.endsWith(" M")))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " should expose OP number and carrier/modulator role, got "
                      << nameText.toStdString() << '\n';
            ok = false;
        }

        if (nameText.endsWith(" C"))
            ++carrierCount;
        if (nameText.endsWith(" M"))
            ++modulatorCount;

        if (! valueText.contains("MULT") || ! valueText.contains("TL") || ! valueText.contains("AR"))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " should expose resolved register fields, got "
                      << valueText.toStdString() << '\n';
            ok = false;
        }

        if (! levelValueText.contains("%"))
        {
            std::cerr << "editor_size_smoke: " << modeLabel << " operator row " << op
                      << " should expose operator level percent, got "
                      << levelValueText.toStdString() << '\n';
            ok = false;
        }
    }

    if (carrierCount == 0 || modulatorCount == 0)
    {
        std::cerr << "editor_size_smoke: " << modeLabel
                  << " mixed algorithm should show both carriers and modulators, got C="
                  << carrierCount << " M=" << modulatorCount << '\n';
        ok = false;
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
    editor.runEditorUpdateForLayoutTest();

    const auto sourceDeckBounds = editor.getModuleBoundsForLayoutTest(1);
    if (sourceDeckBounds.getHeight() > 264)
    {
        std::cerr << "editor_size_smoke: wavetable source deck is reserving empty vertical space: "
                  << sourceDeckBounds.toString() << '\n';
        ok = false;
    }

    const auto visibleSources = chipper::visibleSourceCountForMode(mode);
    const auto expectedFourthWaveChoice = mode == chipper::ChipMode::huc6280 ? juce::String("Square") : juce::String("Pulse");
    const auto expectedFifthWaveChoice = mode == chipper::ChipMode::huc6280 ? juce::String("Noise") : juce::String("Steps");
    const auto actualFourthWaveChoice = editor.getSourceWaveSelectorItemTextForLayoutTest(0, 3);
    const auto actualFifthWaveChoice = editor.getSourceWaveSelectorItemTextForLayoutTest(0, 4);
    if (actualFourthWaveChoice != expectedFourthWaveChoice || actualFifthWaveChoice != expectedFifthWaveChoice)
    {
        std::cerr << "editor_size_smoke: wavetable selector labels do not match chip vocabulary for "
                  << chipper::toString(mode) << ": got "
                  << actualFourthWaveChoice << "/" << actualFifthWaveChoice
                  << ", expected " << expectedFourthWaveChoice << "/" << expectedFifthWaveChoice << '\n';
        ok = false;
    }

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

        if (sourceBounds.getHeight() < 96)
        {
            std::cerr << "editor_size_smoke: wavetable source card collapsed below readable height: "
                      << sourceBounds.toString() << '\n';
            ok = false;
        }

        if (sourceBounds.getHeight() > 116)
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

        if (! levelBounds.isEmpty() && levelBounds.getY() - sourceBounds.getY() < 70)
        {
            std::cerr << "editor_size_smoke: wavetable level lane is crowding the wave selector stack for channel "
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

    if (mode == chipper::ChipMode::huc6280)
    {
        const auto envelopeBounds = editor.getModuleBoundsForLayoutTest(3);
        const auto motionBounds = editor.getModuleBoundsForLayoutTest(4);
        const auto outputModuleBounds = editor.getModuleBoundsForLayoutTest(5);
        const auto lfoBounds = editor.getDmgStereoRouteBoundsForLayoutTest();
        const auto stereoSpreadBounds = editor.getStereoSpreadBoundsForLayoutTest();
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
        const auto lowerModuleBottom = std::max({ envelopeBounds.getBottom(), motionBounds.getBottom(), outputModuleBounds.getBottom() });

        if (motionBounds.isEmpty() || motionBounds.getHeight() < 150)
        {
            std::cerr << "editor_size_smoke: HuC6280 motion panel is missing or too short for Ch 1/2 LFO: "
                      << motionBounds.toString() << '\n';
            ok = false;
        }

        if (lfoBounds.isEmpty()
            || lfoBounds.getWidth() < 180
            || lfoBounds.getHeight() < 24
            || ! motionBounds.expanded(2).contains(lfoBounds))
        {
            std::cerr << "editor_size_smoke: HuC6280 Ch 1/2 LFO control is not readable/owned by Motion: motion "
                      << motionBounds.toString() << " control " << lfoBounds.toString() << '\n';
            ok = false;
        }

        if (outputModuleBounds.isEmpty() || outputModuleBounds.getHeight() < 150)
        {
            std::cerr << "editor_size_smoke: HuC6280 output panel is missing or too short: "
                      << outputModuleBounds.toString() << '\n';
            ok = false;
        }

        if (stereoSpreadBounds.isEmpty()
            || stereoSpreadBounds.getWidth() < 180
            || stereoSpreadBounds.getHeight() < 18
            || ! outputModuleBounds.expanded(2).contains(stereoSpreadBounds))
        {
            std::cerr << "editor_size_smoke: HuC6280 stereo spread is not readable/owned by Output: output "
                      << outputModuleBounds.toString() << " control " << stereoSpreadBounds.toString() << '\n';
            ok = false;
        }

        if (! performanceBounds.isEmpty()
            && lowerModuleBottom > 0
            && performanceBounds.getY() - lowerModuleBottom > 36)
        {
            std::cerr << "editor_size_smoke: HuC6280 module stack leaves excessive dead space before performance macros: modules bottom "
                      << lowerModuleBottom << " performance " << performanceBounds.toString() << '\n';
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

        if (levelBounds.isEmpty()
            || levelBounds.getHeight() < 16
            || levelBounds.getWidth() < 120
            || ! sourceBounds.expanded(2).contains(levelBounds))
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
            && levelBounds.getY() < selectorForLevel.getBottom() + 2)
        {
            std::cerr << "editor_size_smoke: sampler level lane overlaps its selector for channel "
                      << channel << " selector " << selectorForLevel.toString()
                      << " level " << levelBounds.toString() << '\n';
            ok = false;
        }

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
    const auto envelopeBounds = editor.getEnvelopeDecayBoundsForLayoutTest();

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
    expectOwnedStandardControl(editor.getSampleFileButtonBoundsForLayoutTest(), "sample file button");
    expectOwnedStandardControl(editor.getSampleFolderButtonBoundsForLayoutTest(), "sample folder button");
    expectOwnedStandardControl(editor.getSampleBankButtonBoundsForLayoutTest(), "sample bank button");

    if (mode == chipper::ChipMode::spc700)
        expectOwnedStandardControl(editor.getSampleLoopToggleBoundsForLayoutTest(), "SPC700 loop toggle");

    if (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
    {
        const auto envelopePanel = editor.getModuleBoundsForLayoutTest(3);
        if (envelopeBounds.isEmpty()
            || envelopeBounds.getHeight() < 16
            || ! envelopePanel.expanded(2).contains(envelopeBounds))
        {
            std::cerr << "editor_size_smoke: " << chipModeName(mode)
                      << " sampler envelope/gain control is not visible/owned: panel "
                      << envelopePanel.toString() << " control "
                      << envelopeBounds.toString() << '\n';
            ok = false;
        }
    }

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

bool checkPerformanceMacroSliderLayout()
{
    auto ok = true;
    const auto chipModeCount = chipper::parameters::chipModeChoices().size();

    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        const auto mode = chipper::parameters::chipModeFromChoice(chipMode);
        ChipperAudioProcessor processor;
        ok &= setChoiceParameter(processor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(1240, expectedHeightForChipMode(chipMode));
        editor.runEditorUpdateForLayoutTest();

        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
        const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();
        if (performanceBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: missing performance macro strip for mode "
                      << chipper::parameters::chipModeChoices()[chipMode] << '\n';
            ok = false;
            continue;
        }

        std::vector<size_t> expectedMacroSliders;
        switch (mode)
        {
            case chipper::ChipMode::nes:
            case chipper::ChipMode::nesVrc6:
            case chipper::ChipMode::dmg:
                expectedMacroSliders = { 1, 2, 3 };
                break;
            case chipper::ChipMode::sid:
                expectedMacroSliders = { 0, 1, 3 };
                break;
            case chipper::ChipMode::spc700:
            case chipper::ChipMode::paula:
                expectedMacroSliders = { 0, 2, 3 };
                break;
            case chipper::ChipMode::ym2149:
                expectedMacroSliders = { 0, 1 };
                break;
            case chipper::ChipMode::ym2612:
            case chipper::ChipMode::ym2151:
            case chipper::ChipMode::ym2203:
                expectedMacroSliders = { 0 };
                break;
            default:
                expectedMacroSliders = { 0, 1, 2, 3 };
                break;
        }

        for (const auto sliderIndex : expectedMacroSliders)
        {
            const auto sliderBounds = editor.getNativeSliderBoundsForLayoutTest(sliderIndex);
            const auto groupBounds = editor.getNativeGroupLabelBoundsForLayoutTest(sliderIndex);
            const auto labelBounds = editor.getNativeLabelBoundsForLayoutTest(sliderIndex);
            const auto valueBounds = editor.getNativeValueLabelBoundsForLayoutTest(sliderIndex);
            const auto compactMacroCell = groupBounds.isEmpty();
            const auto shouldShowMacroReadout = mode == chipper::ChipMode::ym2149
                || mode == chipper::ChipMode::sn76489
                || mode == chipper::ChipMode::ym2612
                || mode == chipper::ChipMode::opl3
                || mode == chipper::ChipMode::ym2151
                || mode == chipper::ChipMode::ym2413
                || mode == chipper::ChipMode::ym2203
                || mode == chipper::ChipMode::pokey
                || mode == chipper::ChipMode::huc6280
                || mode == chipper::ChipMode::namcoWsg
                || mode == chipper::ChipMode::scc;
            if (sliderBounds.isEmpty())
            {
                std::cerr << "editor_size_smoke: missing performance macro slider "
                          << sliderIndex << " for mode "
                          << chipper::parameters::chipModeChoices()[chipMode] << '\n';
                ok = false;
                continue;
            }

            if (! performanceBounds.expanded(2).contains(sliderBounds)
                || sliderBounds.getWidth() < 96
                || sliderBounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: performance macro slider "
                          << sliderIndex << " is not readable/owned by performance strip for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": slider " << sliderBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }

            if (! groupBounds.isEmpty() && ! performanceBounds.expanded(2).contains(groupBounds))
            {
                std::cerr << "editor_size_smoke: performance macro group label "
                          << sliderIndex << " escaped performance strip for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": group " << groupBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }

            if (labelBounds.isEmpty()
                || ! performanceBounds.expanded(2).contains(labelBounds)
                || labelBounds.getHeight() < 12
                || labelBounds.intersects(sliderBounds))
            {
                std::cerr << "editor_size_smoke: performance macro label "
                          << sliderIndex << " is not readable/owned by performance strip for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": label " << labelBounds.toString()
                          << " slider " << sliderBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }

            if (! valueBounds.isEmpty()
                && (! performanceBounds.expanded(2).contains(valueBounds)
                    || valueBounds.getHeight() < 12
                    || valueBounds.intersects(sliderBounds)))
            {
                std::cerr << "editor_size_smoke: performance macro readout "
                          << sliderIndex << " overlaps/escapes its slider area for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": readout " << valueBounds.toString()
                          << " slider " << sliderBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }

            if (compactMacroCell && ! valueBounds.isEmpty())
            {
                std::cerr << "editor_size_smoke: compact performance macro cell "
                          << sliderIndex << " should hide secondary readout text for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": readout " << valueBounds.toString()
                          << " slider " << sliderBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }

            if (shouldShowMacroReadout
                && ! compactMacroCell
                && (valueBounds.isEmpty()
                    || valueBounds.getY() <= sliderBounds.getBottom()))
            {
                std::cerr << "editor_size_smoke: performance macro readout "
                          << sliderIndex << " should be visible below its slider for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": readout " << valueBounds.toString()
                          << " slider " << sliderBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }
        }

        if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2203)
        {
            const auto feedbackBounds = editor.getFmFeedbackBoundsForLayoutTest();
            const auto feedbackSliderBounds = editor.getNativeSliderBoundsForLayoutTest(1);
            if (feedbackBounds.isEmpty()
                || ! performanceBounds.expanded(2).contains(feedbackBounds)
                || feedbackBounds.getWidth() < 96
                || feedbackBounds.getHeight() < 20)
            {
                std::cerr << "editor_size_smoke: FM feedback menu is not readable/owned by performance strip for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": feedback " << feedbackBounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                ok = false;
            }

            if (! feedbackSliderBounds.isEmpty())
            {
                std::cerr << "editor_size_smoke: FM feedback should use the native feedback menu, not macro slider 1, for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": slider " << feedbackSliderBounds.toString() << '\n';
                ok = false;
            }
        }

        if (outputBounds.isEmpty()
            || ! performanceBounds.expanded(2).contains(outputBounds)
            || outputBounds.getWidth() < 96
            || outputBounds.getHeight() < 18)
        {
            std::cerr << "editor_size_smoke: output slider is not readable/owned by performance strip for mode "
                      << chipper::parameters::chipModeChoices()[chipMode]
                      << ": output " << outputBounds.toString()
                      << " performance " << performanceBounds.toString() << '\n';
            ok = false;
        }
    }

    return ok;
}

bool checkSidAdsrLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::sid);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: SID chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto adsrModuleBounds = editor.getModuleBoundsForLayoutTest(3);
        const auto adsrContentBounds = editor.getSidAdsrContentBoundsForLayoutTest();
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();

        if (adsrModuleBounds.isEmpty() || adsrContentBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: SID ADSR module/content is missing at width "
                      << editorWidth << "; module " << adsrModuleBounds.toString()
                      << " content " << adsrContentBounds.toString() << '\n';
            return false;
        }

        if (adsrModuleBounds.getHeight() < 260)
        {
            std::cerr << "editor_size_smoke: SID ADSR module is too short for readable per-voice controls at width "
                      << editorWidth << ": " << adsrModuleBounds.toString() << '\n';
            widthOk = false;
        }

        if (! performanceBounds.isEmpty() && adsrModuleBounds.getBottom() > performanceBounds.getY() - 8)
        {
            std::cerr << "editor_size_smoke: SID ADSR panel crowds performance macros at width "
                      << editorWidth << ": module " << adsrModuleBounds.toString()
                      << " performance " << performanceBounds.toString() << '\n';
            widthOk = false;
        }

        if (! adsrModuleBounds.expanded(2).contains(adsrContentBounds))
        {
            std::cerr << "editor_size_smoke: SID ADSR content escaped its module at width "
                      << editorWidth << ": module " << adsrModuleBounds.toString()
                      << " content " << adsrContentBounds.toString() << '\n';
            widthOk = false;
        }

        if (! performanceBounds.isEmpty() && adsrContentBounds.getBottom() > performanceBounds.getY() - 4)
        {
            std::cerr << "editor_size_smoke: SID ADSR content overlaps performance macros at width "
                      << editorWidth << ": content " << adsrContentBounds.toString()
                      << " performance " << performanceBounds.toString() << '\n';
            widthOk = false;
        }

        for (size_t voice = 0; voice < 3; ++voice)
        {
            const auto preview = editor.getSidEnvelopePreviewBoundsForLayoutTest(voice);
            if (preview.isEmpty() || preview.getWidth() < 160 || preview.getHeight() < 46)
            {
                std::cerr << "editor_size_smoke: SID envelope preview is not readable for voice "
                          << (voice + 1u) << " at width " << editorWidth
                          << ": " << preview.toString() << '\n';
                widthOk = false;
            }

            if (! adsrModuleBounds.expanded(2).contains(preview))
            {
                std::cerr << "editor_size_smoke: SID envelope preview escaped ADSR module for voice "
                          << (voice + 1u) << " at width " << editorWidth
                          << ": module " << adsrModuleBounds.toString()
                          << " preview " << preview.toString() << '\n';
                widthOk = false;
            }

            auto sliderTop = std::numeric_limits<int>::max();
            for (size_t field = 0; field < 4; ++field)
            {
                const auto slider = editor.getSidAdsrSliderBoundsForLayoutTest((voice * 4u) + field);
                if (slider.isEmpty() || slider.getHeight() < 62)
                {
                    std::cerr << "editor_size_smoke: SID ADSR slider is not readable for voice "
                              << (voice + 1u) << ", field " << (field + 1u)
                              << " at width " << editorWidth << ": " << slider.toString() << '\n';
                    widthOk = false;
                }

                if (! adsrModuleBounds.expanded(2).contains(slider))
                {
                    std::cerr << "editor_size_smoke: SID ADSR slider escaped ADSR module for voice "
                              << (voice + 1u) << ", field " << (field + 1u)
                              << " at width " << editorWidth
                              << ": module " << adsrModuleBounds.toString()
                              << " slider " << slider.toString() << '\n';
                    widthOk = false;
                }

                if (slider.getY() < sliderTop)
                    sliderTop = slider.getY();
            }

            if (! preview.isEmpty() && preview.getBottom() > sliderTop - 4)
            {
                std::cerr << "editor_size_smoke: SID envelope preview overlaps or crowds ADSR sliders for voice "
                          << (voice + 1u) << " at width " << editorWidth
                          << ": slider top " << sliderTop
                          << " preview " << preview.toString() << '\n';
                widthOk = false;
            }
        }

        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);

    return ok;
}

bool checkCompactChipLayouts()
{
    bool ok = true;
    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        const auto mode = chipper::parameters::chipModeFromChoice(chipMode);
        const auto chipPath = chipper::parameters::chipModeChoices()[chipMode].toStdString();
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        chipEditor.setSize(expectedEditorMinimumWidth, expectedHeightForChipMode(chipMode));
        chipEditor.runEditorUpdateForLayoutTest();

        ok &= expect(chipEditor.getWidth() == expectedEditorMinimumWidth, "compact editor width was not preserved");
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "compact editor height changed");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/compact/" + chipPath);
        ok &= checkPrimaryPanelStack(chipEditor, mode);
    }

    return ok;
}

bool checkPresetRoleFilterLayout()
{
    bool ok = true;
    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        chipEditor.setSize(expectedEditorMinimumWidth, expectedHeightForChipMode(chipMode));
        chipEditor.runEditorUpdateForLayoutTest();

        const auto bounds = chipEditor.getPresetFilterBoundsForLayoutTest();
        if (bounds.getWidth() < 86 || bounds.getHeight() < 28)
        {
            std::cerr << "editor_size_smoke: preset metadata filter below readable size at compact width: "
                      << bounds.toString() << '\n';
            ok = false;
        }

        if (chipEditor.getPresetFilterTextForLayoutTest().isEmpty())
        {
            std::cerr << "editor_size_smoke: preset metadata filter should default to All\n";
            ok = false;
        }

        const auto searchBounds = chipEditor.getPresetSearchBoundsForLayoutTest();
        if (searchBounds.getWidth() < 86 || searchBounds.getHeight() < 28)
        {
            std::cerr << "editor_size_smoke: preset search box below readable size at compact width: "
                      << searchBounds.toString() << '\n';
            ok = false;
        }

        if (chipEditor.getPresetSearchTextForLayoutTest().isNotEmpty())
        {
            std::cerr << "editor_size_smoke: preset search should default to empty text\n";
            ok = false;
        }

        const auto favoriteBounds = chipEditor.getPresetFavoriteBoundsForLayoutTest();
        if (favoriteBounds.getWidth() < 32 || favoriteBounds.getHeight() < 28)
        {
            std::cerr << "editor_size_smoke: preset favorite button below readable size at compact width: "
                      << favoriteBounds.toString() << '\n';
            ok = false;
        }

        if (chipEditor.getPresetFavoriteToggleStateForLayoutTest())
        {
            std::cerr << "editor_size_smoke: preset favorite button should default to untoggled\n";
            ok = false;
        }

        if (chipMode == 0)
        {
            chipEditor.clearPresetFavoritesForLayoutTest();
            const auto unfilteredPresetCount = chipEditor.getDisplayedFactoryPresetCountForLayoutTest();
            if (! chipEditor.setFactoryPresetFavoriteForLayoutTest("nes-hero-pulse", true)
                || ! chipEditor.selectPresetFilterForLayoutTest("favorite", "favorites"))
            {
                std::cerr << "editor_size_smoke: preset metadata filter should expose in-memory Favorites\n";
                ok = false;
            }
            const auto favoriteCount = chipEditor.getDisplayedFactoryPresetCountForLayoutTest();
            const auto favoriteName = chipEditor.getFirstDisplayedFactoryPresetNameForLayoutTest();
            if (favoriteCount != 1 || favoriteName != "NES Hero Pulse")
            {
                std::cerr << "editor_size_smoke: Favorites filter should narrow NES presets to the marked favorite, got count "
                          << favoriteCount << " first " << favoriteName.toStdString() << '\n';
                ok = false;
            }

            if (! chipEditor.selectPresetFilterForLayoutTest("all", ""))
            {
                std::cerr << "editor_size_smoke: preset metadata filter should return to All after Favorites\n";
                ok = false;
            }

            if (! chipEditor.selectPresetFilterForLayoutTest("tag", "noise"))
            {
                std::cerr << "editor_size_smoke: preset metadata filter should expose the NES noise tag\n";
                ok = false;
            }
            const auto noiseTagCount = chipEditor.getDisplayedFactoryPresetCountForLayoutTest();
            const auto noiseTagName = chipEditor.getFirstDisplayedFactoryPresetNameForLayoutTest();
            if (noiseTagCount <= 0 || noiseTagCount >= unfilteredPresetCount || ! noiseTagName.containsIgnoreCase("noise"))
            {
                std::cerr << "editor_size_smoke: preset tag filter should narrow NES presets to noise entries, got count "
                          << noiseTagCount << " first " << noiseTagName.toStdString() << '\n';
                ok = false;
            }

            if (! chipEditor.selectPresetFilterForLayoutTest("all", ""))
            {
                std::cerr << "editor_size_smoke: preset metadata filter should return to All\n";
                ok = false;
            }

            if (! chipEditor.userPresetMetadataMatchesFilterForLayoutTest("role", "Bass", "Bass", "RP2A03 APU", "factory,bass,apu"))
            {
                std::cerr << "editor_size_smoke: user preset role metadata should participate in the preset filter\n";
                ok = false;
            }

            if (! chipEditor.userPresetMetadataMatchesFilterForLayoutTest("engine", "RP2A03 APU", "Bass", "RP2A03 APU", "factory,bass,apu"))
            {
                std::cerr << "editor_size_smoke: user preset engine metadata should participate in the preset filter\n";
                ok = false;
            }

            if (! chipEditor.userPresetMetadataMatchesFilterForLayoutTest("tag", "dmc", "Bass", "RP2A03 APU", "factory,bass,dmc"))
            {
                std::cerr << "editor_size_smoke: user preset tag metadata should participate in the preset filter\n";
                ok = false;
            }

            chipEditor.setPresetSearchTextForLayoutTest("organ");
            const auto organSearchCount = chipEditor.getDisplayedFactoryPresetCountForLayoutTest();
            const auto organSearchName = chipEditor.getFirstDisplayedFactoryPresetNameForLayoutTest();
            if (organSearchCount != 1 || organSearchName != "NES Pulse Organ")
            {
                std::cerr << "editor_size_smoke: preset search should narrow NES presets to the organ entry, got count "
                          << organSearchCount << " first " << organSearchName.toStdString() << '\n';
                ok = false;
            }

            chipEditor.setPresetSearchTextForLayoutTest("no-such-preset-token");
            const auto emptySearchCount = chipEditor.getDisplayedFactoryPresetCountForLayoutTest();
            if (emptySearchCount != 0)
            {
                std::cerr << "editor_size_smoke: preset search should report no factory matches for unknown tokens, got count "
                          << emptySearchCount << '\n';
                ok = false;
            }
        }
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
    ok &= expect(editor.getHeight() == expectedHeightForChipMode(0), "host-restored default editor height was not clamped to the chip preferred size");

    editor.setSize(1000, 600);
    ok &= expect(editor.getWidth() >= expectedEditorMinimumWidth, "editor width was not clamped to minimum");
    ok &= expect(editor.getHeight() >= expectedHeightForChipMode(0), "editor height was not clamped to the default chip preferred size");

    const auto chipModeCount = chipper::parameters::chipModeChoices().size();
    for (auto chipMode = 0; chipMode < chipModeCount; ++chipMode)
    {
        const auto chipPath = chipper::parameters::chipModeChoices()[chipMode].toStdString();
        ChipperAudioProcessor chipProcessor;
        ok &= setChoiceParameter(chipProcessor, chipper::parameters::id::chipMode, chipMode);

        ChipperAudioProcessorEditor chipEditor(chipProcessor);
        ok &= expect(chipEditor.getWidth() == 1240, "chip default width changed");
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip default height changed");
        ok &= expect(chipEditor.getHeight() <= expectedEditorMaximumHeight, "chip default height exceeded DAW-friendly cap");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/" + chipPath);
        ok &= checkPrimaryPanelStack(chipEditor, chipper::parameters::chipModeFromChoice(chipMode));
        chipEditor.setSize(1240, 1200);
        ok &= expect(chipEditor.getHeight() == expectedHeightForChipMode(chipMode), "chip-switched editor height was not clamped to the chip preferred size");
        ok &= expect(chipEditor.getWidth() == 1240, "chip-switched editor width unexpectedly changed");
        ok &= checkVisibleChildGeometry(chipEditor, chipEditor, juce::Point<int> {}, "editor/restored/" + chipPath);
        ok &= checkPrimaryPanelStack(chipEditor, chipper::parameters::chipModeFromChoice(chipMode));
    }

    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nes);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nesVrc6);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::dmg);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::sn76489);
    ok &= checkYm2149ToneNoiseMixLayout();
    ok &= checkYm2612DacModeLayout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2612);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2151);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2203);
    ok &= checkWavetableSourceDeck(chipper::ChipMode::huc6280);
    ok &= checkWavetableSourceDeck(chipper::ChipMode::namcoWsg);
    ok &= checkWavetableSourceDeck(chipper::ChipMode::scc);
    ok &= checkSamplerSourceDeck(chipper::ChipMode::spc700);
    ok &= checkSamplerSourceDeck(chipper::ChipMode::paula);
    ok &= checkSamplerBankLayout(chipper::ChipMode::spc700);
    ok &= checkSamplerBankLayout(chipper::ChipMode::paula);
    ok &= checkNesDmcAndPerformanceLayout();
    ok &= checkPerformanceMacroSliderLayout();
    ok &= checkSidAdsrLayout();
    ok &= checkCompactChipLayouts();
    ok &= checkPresetRoleFilterLayout();
    ok &= checkChipSwitchPreservesEditorSettings();

    return ok ? 0 : 1;
}
