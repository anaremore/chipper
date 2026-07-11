#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"
#include "Presets.h"
#include "UI/ChipUiModel.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <typeinfo>

namespace
{
constexpr int expectedEditorHeight = 860;
constexpr int expectedEditorDmgHeight = 720;
constexpr int expectedEditorSidHeight = 880;
constexpr int expectedEditorMinimumWidth = 1180;
constexpr int expectedEditorMaximumHeight = expectedEditorSidHeight;

bool expect(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "editor_size_smoke: " << message << '\n';

    return condition;
}

bool checkAccessibleFocusContract(juce::Component& root, const juce::String& idPrefix)
{
    bool ok = true;
    int checkedControls = 0;
    std::function<void(juce::Component&)> visit;
    visit = [&](juce::Component& parent)
    {
        for (auto childIndex = 0; childIndex < parent.getNumChildComponents(); ++childIndex)
        {
            auto* child = parent.getChildComponent(childIndex);
            if (child == nullptr)
                continue;
            const auto matches = child->getComponentID().startsWith(idPrefix);
            if (matches && child->isVisible() && child->isEnabled() && child->getWantsKeyboardFocus())
            {
                ++checkedControls;
                if (child->getName().trim().isEmpty() || child->getExplicitFocusOrder() <= 0)
                {
                    std::cerr << "editor_size_smoke: accessible focus contract missing for "
                              << child->getComponentID().toStdString() << " name="
                              << child->getName().toStdString() << " order="
                              << child->getExplicitFocusOrder() << '\n';
                    ok = false;
                }
            }
            visit(*child);
        }
    };
    visit(root);
    if (checkedControls == 0)
    {
        std::cerr << "editor_size_smoke: no focusable controls found for accessibility prefix "
                  << idPrefix.toStdString() << '\n';
        ok = false;
    }
    return ok;
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
    case chipper::ChipMode::nesFds: return "NES + FDS";
    case chipper::ChipMode::nesSunsoft5b: return "NES + Sunsoft 5B";
    case chipper::ChipMode::nesMmc5: return "NES + MMC5";
    case chipper::ChipMode::nesVrc7: return "NES + VRC7";
    case chipper::ChipMode::spc700: return "SPC700";
    case chipper::ChipMode::paula: return "Paula";
    case chipper::ChipMode::ym2203: return "YM2203";
    case chipper::ChipMode::ym2608: return "YM2608";
    case chipper::ChipMode::ym2610: return "YM2610";
    case chipper::ChipMode::ym2610b: return "YM2610B";
    default: return "chip";
    }
}

int expectedHeightForChipMode(int chipMode)
{
    const auto mode = chipper::parameters::chipModeFromChoice(chipMode);
    if (mode == chipper::ChipMode::sid)
        return expectedEditorSidHeight;
    if (mode == chipper::ChipMode::dmg)
        return expectedEditorDmgHeight;

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

    const auto isNesFamily = mode == chipper::ChipMode::nes
        || mode == chipper::ChipMode::nesVrc6
        || mode == chipper::ChipMode::nesFds
        || mode == chipper::ChipMode::nesSunsoft5b
        || mode == chipper::ChipMode::nesMmc5
        || mode == chipper::ChipMode::nesVrc7;
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

        const auto interactive = dynamic_cast<const juce::Button*>(child) != nullptr
            || dynamic_cast<const juce::Slider*>(child) != nullptr
            || dynamic_cast<const juce::ComboBox*>(child) != nullptr
            || dynamic_cast<const juce::TextEditor*>(child) != nullptr
            || dynamic_cast<const juce::ListBox*>(child) != nullptr;
        if (bounds.isEmpty() && interactive && child->isEnabled() && child->getWantsKeyboardFocus())
        {
            std::cerr << "editor_size_smoke: zero-bounds control remained keyboard-focusable at "
                      << childPath << " type " << typeid(*child).name() << '\n';
            ok = false;
        }

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
    case chipper::ChipMode::nesFds:
    case chipper::ChipMode::nesSunsoft5b:
    case chipper::ChipMode::nesMmc5:
    case chipper::ChipMode::nesVrc7:
        if (mode == chipper::ChipMode::nesVrc6
            || mode == chipper::ChipMode::nesFds
            || mode == chipper::ChipMode::nesSunsoft5b
            || mode == chipper::ChipMode::nesMmc5
            || mode == chipper::ChipMode::nesVrc7)
            ok &= expectVisibleSourceCardsInsideDeck(editor, mode);
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getPulseDutyBoundsForLayoutTest(), "NES pulse 1 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 1, editor.getPulse2DutyBoundsForLayoutTest(), "NES pulse 2 duty");
        if (mode != chipper::ChipMode::nesVrc7)
            ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeBoundsForLayoutTest(), "NES noise mode");
        if (mode == chipper::ChipMode::nes)
            ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getNativeSliderBoundsForLayoutTest(2), "NES noise period");
        break;

    case chipper::ChipMode::dmg:
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getPulseDutyBoundsForLayoutTest(), "DMG pulse 1 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 0, editor.getNativeSliderBoundsForLayoutTest(1), "DMG pulse 1 sweep shift");
        ok &= expectControlOwnedBySourceChannel(editor, 1, editor.getPulse2DutyBoundsForLayoutTest(), "DMG pulse 2 duty");
        ok &= expectControlOwnedBySourceChannel(editor, 2, editor.getDmgWaveLevelBoundsForLayoutTest(), "DMG wave level");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeBoundsForLayoutTest(), "DMG noise mode");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getNativeSliderBoundsForLayoutTest(2), "DMG noise clock");
        {
            const auto envelopeBounds = editor.getModuleBoundsForLayoutTest(3);
            const auto initialLevelBounds = editor.getNativeSliderBoundsForLayoutTest(3);
            const auto envelopeDecayBounds = editor.getEnvelopeDecayBoundsForLayoutTest();
            if (envelopeBounds.isEmpty()
                || ! envelopeBounds.expanded(2).contains(initialLevelBounds)
                || ! envelopeBounds.expanded(2).contains(envelopeDecayBounds))
            {
                std::cerr << "editor_size_smoke: DMG shared envelope helpers must stay inside the Pulse + Noise Envelopes module\n";
                ok = false;
            }

            const auto routeBounds = editor.getDmgStereoRouteBoundsForLayoutTest();
            const auto outputModuleBounds = editor.getModuleBoundsForLayoutTest(5);
            if (routeBounds.isEmpty() || ! outputModuleBounds.expanded(2).contains(routeBounds))
            {
                std::cerr << "editor_size_smoke: DMG NR51 routing must stay inside its output module\n";
                ok = false;
            }
        }
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(3).contains("15/7-bit LFSR"),
                     "DMG mono Noise card should disclose its native LFSR identity");
        ok &= expect(editor.getClockTextForLayoutTest() == "Default",
                     "DMG zero clock override should read Default instead of 0 Hz");
        ok &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("note 1"),
                     "DMG Chip Poly should identify Pulse 1 as the first allocated note lane");
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(2).contains("note 3"),
                     "DMG Chip Poly should identify Wave as the third allocated note lane");
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(3).contains("not note-allocated"),
                     "DMG Chip Poly should disclose that Noise remains an SFX lane");
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

    for (const auto sliderIndex : { 2u, 3u })
    {
        const auto sliderBounds = editor.getNativeSliderBoundsForLayoutTest(sliderIndex);
        const auto groupBounds = editor.getNativeGroupLabelBoundsForLayoutTest(sliderIndex);
        const auto labelBounds = editor.getNativeLabelBoundsForLayoutTest(sliderIndex);
        const auto valueBounds = editor.getNativeValueLabelBoundsForLayoutTest(sliderIndex);
        if (! sliderBounds.isEmpty() || ! groupBounds.isEmpty() || ! labelBounds.isEmpty() || ! valueBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: " << modeLabel
                      << " should keep universal musical macros in Play instead of crowding the native operator grid\n";
            ok = false;
        }
    }

    auto carrierCount = 0;
    auto modulatorCount = 0;
    std::array<juce::Rectangle<int>, 4> operatorCards;

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
        const auto cardBounds = editor.getFmOperatorCardBoundsForLayoutTest(op);
        operatorCards[op] = cardBounds;

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
            || ! envelopeModuleBounds.expanded(2).contains(cardBounds)
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

        if (cardBounds.getWidth() < 250 || cardBounds.getHeight() < 48
            || nameBounds.getWidth() < 70 || nameBounds.getHeight() < 12
            || levelValueBounds.getWidth() < 80 || levelValueBounds.getHeight() < 12
            || levelSliderBounds.getWidth() < 120 || levelSliderBounds.getHeight() < 16
            || multiplierBounds.getWidth() < 48 || multiplierBounds.getHeight() < 16
            || attackRateBounds.getWidth() < 40 || attackRateBounds.getHeight() < 16
            || valueBounds.getWidth() < 200 || valueBounds.getHeight() < 12)
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

    for (size_t left = 0; left < operatorCards.size(); ++left)
    {
        for (size_t right = left + 1u; right < operatorCards.size(); ++right)
        {
            if (operatorCards[left].intersects(operatorCards[right]))
            {
                std::cerr << "editor_size_smoke: " << modeLabel
                          << " operator cards overlap in the shared 2x2 editor\n";
                ok = false;
            }
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
                expectedMacroSliders = { 1, 3 };
                break;
            case chipper::ChipMode::nesVrc6:
            case chipper::ChipMode::nesFds:
            case chipper::ChipMode::nesSunsoft5b:
            case chipper::ChipMode::nesMmc5:
            case chipper::ChipMode::nesVrc7:
                expectedMacroSliders = { 1, 2, 3 };
                break;
            case chipper::ChipMode::dmg:
                // DMG register controls live with Pulse 1, Noise, and the shared
                // envelope module instead of the global performance strip.
                expectedMacroSliders = {};
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
            case chipper::ChipMode::ym2608:
            case chipper::ChipMode::ym2610:
            case chipper::ChipMode::ym2610b:
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
                || mode == chipper::ChipMode::ym2608
                || mode == chipper::ChipMode::ym2610
                || mode == chipper::ChipMode::ym2610b
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

        if (mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2203 || mode == chipper::ChipMode::ym2608 || mode == chipper::ChipMode::ym2610 || mode == chipper::ChipMode::ym2610b)
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

        const auto browserButtonBounds = chipEditor.getPresetBrowserButtonBoundsForLayoutTest();
        if (browserButtonBounds.getWidth() < 64 || browserButtonBounds.getHeight() < 28)
        {
            std::cerr << "editor_size_smoke: global preset browser button below readable size at compact width: "
                      << browserButtonBounds.toString() << '\n';
            ok = false;
        }

        if (! chipEditor.getPresetFilterBoundsForLayoutTest().isEmpty()
            || ! chipEditor.getPresetSearchBoundsForLayoutTest().isEmpty())
        {
            std::cerr << "editor_size_smoke: legacy preset filters should not crowd the compact header\n";
            ok = false;
        }

        if (chipMode == 0)
        {
            chipEditor.showPresetBrowserForLayoutTest();
            const auto searchBounds = chipEditor.getGlobalPresetBrowserSearchBoundsForLayoutTest();
            const auto chipListBounds = chipEditor.getGlobalPresetBrowserChipListBoundsForLayoutTest();
            const auto resultListBounds = chipEditor.getGlobalPresetBrowserResultListBoundsForLayoutTest();
            const auto detailBounds = chipEditor.getGlobalPresetBrowserDetailBoundsForLayoutTest();
            if (! chipEditor.isPresetBrowserVisibleForLayoutTest()
                || searchBounds.getWidth() < 300 || searchBounds.getHeight() < 28
                || chipListBounds.getWidth() < 180 || chipListBounds.getHeight() < 400
                || resultListBounds.getWidth() < 300 || resultListBounds.getHeight() < 400
                || detailBounds.getWidth() < 300 || detailBounds.getHeight() < 400
                || chipEditor.getGlobalPresetBrowserResultCountForLayoutTest() <= 0)
            {
                std::cerr << "editor_size_smoke: global preset browser is incomplete at compact width\n";
                ok = false;
            }
            chipEditor.closePresetBrowserForLayoutTest();
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

bool checkGlobalPresetBrowserWorkflow()
{
    bool ok = true;
    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);
    editor.setSize(expectedEditorMinimumWidth, expectedEditorHeight);
    editor.showPresetBrowserForLayoutTest();
    editor.runEditorUpdateForLayoutTest();
    if (! editor.isPresetBrowserVisibleForLayoutTest())
    {
        std::cerr << "editor_size_smoke: global browser did not remain open above the unified editor after the periodic UI update\n";
        ok = false;
    }
    editor.closePresetBrowserForLayoutTest();

    const auto crossChipPreset = std::find_if(chipper::presetCatalog().begin(),
                                              chipper::presetCatalog().end(),
                                              [](const chipper::PresetInfo& preset)
                                              {
                                                  return preset.chip == chipper::ChipMode::sid;
                                              });
    if (crossChipPreset == chipper::presetCatalog().end())
        return expect(false, "missing SID preset for global browser workflow test");

    editor.showPresetBrowserForLayoutTest();
    ok &= checkAccessibleFocusContract(editor, "presetBrowser.");
    editor.showBrowserSearchFocusOutlineForLayoutTest();
    if (editor.getFocusOutlineBoundsForLayoutTest().isEmpty())
    {
        std::cerr << "editor_size_smoke: global browser search focus has no visible focus outline\n";
        ok = false;
    }
    editor.selectAllGlobalPresetBrowserChipsForLayoutTest();
    editor.setGlobalPresetBrowserSearchForLayoutTest(juce::String(crossChipPreset->name));
    if (editor.getGlobalPresetBrowserResultCountForLayoutTest() <= 0)
    {
        std::cerr << "editor_size_smoke: global browser did not find a cross-chip preset\n";
        ok = false;
    }
    editor.applyFirstGlobalPresetBrowserResultForLayoutTest();

    const auto selectedChip = static_cast<int>(std::round(plainParameterValue(processor, chipper::parameters::id::chipMode)));
    if (chipper::parameters::chipModeFromChoice(selectedChip) != chipper::ChipMode::sid
        || editor.isPresetBrowserVisibleForLayoutTest())
    {
        std::cerr << "editor_size_smoke: global browser did not explicitly load and close a cross-chip preset\n";
        ok = false;
    }

    editor.runEditorUpdateForLayoutTest();
    editor.showPresetBrowserForLayoutTest();
    editor.setGlobalPresetBrowserSearchForLayoutTest({});
    editor.setGlobalPresetBrowserScopeForLayoutTest(3);
    if (editor.getGlobalPresetBrowserResultCountForLayoutTest() <= 0)
    {
        std::cerr << "editor_size_smoke: global browser did not retain explicit preset history\n";
        ok = false;
    }
    editor.closePresetBrowserForLayoutTest();
    return ok;
}

bool checkUnifiedEditorContract()
{
    bool ok = true;
    const auto chipModeCount = chipper::parameters::chipModeChoices().size();

    for (int chipChoice = 0; chipChoice < chipModeCount; ++chipChoice)
    {
        ChipperAudioProcessor processor;
        ok &= setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(1240, expectedHeightForChipMode(chipChoice));

        std::vector<float> parameterValues;
        parameterValues.reserve(processor.getParameters().size());
        for (const auto* parameter : processor.getParameters())
            parameterValues.push_back(parameter != nullptr ? parameter->getValue() : 0.0f);

        editor.runEditorUpdateForLayoutTest();
        if (! editor.getWorkspaceSelectorBoundsForLayoutTest().isEmpty())
        {
            std::cerr << "editor_size_smoke: unified editor still reserves a workspace selector for chip choice "
                      << chipChoice << '\n';
            ok = false;
        }

        for (const auto workspace : { ChipperEditorWorkspace::play,
                                      ChipperEditorWorkspace::edit,
                                      ChipperEditorWorkspace::inspect })
        {
            if (! editor.getWorkspaceButtonBoundsForLayoutTest(workspace).isEmpty())
            {
                std::cerr << "editor_size_smoke: unified editor still exposes a workspace button for chip choice "
                          << chipChoice << '\n';
                ok = false;
            }
        }

        if (editor.getWorkspaceForLayoutTest() != ChipperEditorWorkspace::edit
            || editor.isWorkspaceDeckVisibleForLayoutTest()
            || ! editor.isModuleTitleVisibleForLayoutTest(1))
        {
            std::cerr << "editor_size_smoke: unified chip surface is not authoritative for chip choice "
                      << chipChoice << '\n';
            ok = false;
        }

        const auto mode = chipper::parameters::chipModeFromChoice(chipChoice);
        const auto expectedSources = chipper::visibleSourceCountForMode(mode);
        for (size_t source = 0; source < expectedSources; ++source)
        {
            const auto bounds = editor.getSourceChannelBoundsForLayoutTest(source);
            if (bounds.getWidth() < 80 || bounds.getHeight() < 60)
            {
                std::cerr << "editor_size_smoke: unified source card is unreadable for chip choice "
                          << chipChoice << " source " << source << ": " << bounds.toString() << '\n';
                ok = false;
            }
        }
        if (editor.getPerformanceBoundsForLayoutTest().getHeight() < 100
            || editor.getOutputSliderBoundsForLayoutTest().getHeight() < 16)
        {
            std::cerr << "editor_size_smoke: unified performance/output path is unreadable for chip choice "
                      << chipChoice << '\n';
            ok = false;
        }

        if (mode == chipper::ChipMode::nes
            && (! editor.isDmcEmptyStateButtonVisibleForLayoutTest()
                || editor.getDmcEmptyStateButtonBoundsForLayoutTest().getWidth() < 150
                || editor.getDmcEmptyStateButtonBoundsForLayoutTest().getHeight() < 24
                || editor.getSourceChannelBoundsForLayoutTest(4).isEmpty()))
        {
            std::cerr << "editor_size_smoke: NES DMC fifth channel or empty state is not actionable\n";
            ok = false;
        }

        editor.setWorkspaceForLayoutTest(ChipperEditorWorkspace::play);
        editor.runEditorUpdateForLayoutTest();
        editor.setWorkspaceForLayoutTest(ChipperEditorWorkspace::inspect);
        editor.runEditorUpdateForLayoutTest();
        if (editor.getWorkspaceForLayoutTest() != ChipperEditorWorkspace::edit
            || editor.isWorkspaceDeckVisibleForLayoutTest())
        {
            std::cerr << "editor_size_smoke: obsolete workspace requests escaped the unified editor contract for chip choice "
                      << chipChoice << '\n';
            ok = false;
        }

        if (chipChoice == 0)
        {
            ok &= checkAccessibleFocusContract(editor, "header.");
            const auto command = juce::ModifierKeys(juce::ModifierKeys::commandModifier);
            editor.keyPressed(juce::KeyPress('1', command, 0));
            ok &= expect(editor.getWorkspaceForLayoutTest() == ChipperEditorWorkspace::edit,
                         "Ctrl/Cmd+1 should not leave the unified editor");
            editor.keyPressed(juce::KeyPress('3', command, 0));
            ok &= expect(editor.getWorkspaceForLayoutTest() == ChipperEditorWorkspace::edit,
                         "Ctrl/Cmd+3 should not leave the unified editor");
            editor.keyPressed(juce::KeyPress('B', command, 0));
            ok &= expect(editor.isPresetBrowserVisibleForLayoutTest(),
                         "Ctrl/Cmd+B did not open the global browser");
            editor.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
            ok &= expect(! editor.isPresetBrowserVisibleForLayoutTest(),
                         "Escape did not close the global browser");
        }

        size_t parameterIndex = 0;
        for (const auto* parameter : processor.getParameters())
        {
            const auto before = parameterValues[parameterIndex++];
            const auto after = parameter != nullptr ? parameter->getValue() : 0.0f;
            if (std::abs(before - after) > 0.000001f)
            {
                std::cerr << "editor_size_smoke: unified-surface checks changed parameter state for chip choice "
                          << chipChoice << '\n';
                ok = false;
                break;
            }
        }
    }

    return ok;
}

bool checkWorkflowTools()
{
    bool ok = true;
    ChipperAudioProcessor processor;
    ChipperAudioProcessorEditor editor(processor);

    for (const auto width : { expectedEditorMinimumWidth, 1240 })
    {
        editor.setSize(width, expectedEditorHeight);
        const auto barBounds = editor.getWorkflowBarBoundsForLayoutTest();
        if (barBounds.getWidth() < 300 || barBounds.getHeight() < 20)
        {
            std::cerr << "editor_size_smoke: workflow bar is unreadable at width " << width
                      << ": " << barBounds.toString() << '\n';
            ok = false;
        }
        for (size_t button = 0; button < 8u; ++button)
        {
            const auto bounds = editor.getWorkflowButtonBoundsForLayoutTest(button);
            if (bounds.getWidth() < 24 || bounds.getHeight() < 20)
            {
                std::cerr << "editor_size_smoke: workflow button " << button
                          << " is unreadable at width " << width << ": " << bounds.toString() << '\n';
                ok = false;
            }
        }
    }
    ok &= checkAccessibleFocusContract(editor, "workflow.");

    const auto macro1 = chipper::parameters::id::macroControl1;
    ok &= setPlainParameter(processor, macro1, 0.2f);
    editor.copyWorkflowStateForLayoutTest();
    ok &= setPlainParameter(processor, macro1, 0.8f);
    editor.pasteWorkflowStateForLayoutTest();
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.2f) < 0.0001f,
                 "workflow paste did not restore the copied sound");

    editor.undoWorkflowForLayoutTest();
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.8f) < 0.0001f,
                 "workflow undo did not restore the pre-paste sound");
    editor.redoWorkflowForLayoutTest();
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.2f) < 0.0001f,
                 "workflow redo did not reapply the pasted sound");

    editor.switchWorkflowSlotForLayoutTest(1);
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.5f) < 0.0001f,
                 "A/B slot B did not begin from the initial sound");
    ok &= setPlainParameter(processor, macro1, 0.7f);
    editor.switchWorkflowSlotForLayoutTest(0);
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.2f) < 0.0001f,
                 "A/B slot A did not retain its edited sound");
    editor.switchWorkflowSlotForLayoutTest(1);
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.7f) < 0.0001f,
                 "A/B slot B did not retain its edited sound");

    ok &= setChoiceParameter(processor, chipper::parameters::id::accuracy, 2);
    ok &= setChoiceParameter(processor, chipper::parameters::id::snNoiseMode, 3);
    ok &= setPlainParameter(processor, macro1, 0.93f);
    const auto chipBeforeInit = plainParameterValue(processor, chipper::parameters::id::chipMode);
    const auto accuracyBeforeInit = plainParameterValue(processor, chipper::parameters::id::accuracy);
    const auto noiseBeforeInit = plainParameterValue(processor, chipper::parameters::id::snNoiseMode);
    editor.initializeWorkflowSectionForLayoutTest(2);
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.5f) < 0.0001f,
                 "musical section init did not reset performance controls");
    ok &= expect(plainParameterValue(processor, chipper::parameters::id::chipMode) == chipBeforeInit
                     && plainParameterValue(processor, chipper::parameters::id::accuracy) == accuracyBeforeInit
                     && plainParameterValue(processor, chipper::parameters::id::snNoiseMode) == noiseBeforeInit,
                 "musical section init changed protected chip settings");

    const std::array<const char*, 4> macroIds {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };
    std::array<float, 4> macroValues {};
    for (size_t i = 0; i < macroIds.size(); ++i)
        macroValues[i] = plainParameterValue(processor, macroIds[i]);
    const auto sourceEnabledBefore = plainParameterValue(processor, chipper::parameters::id::source1Enabled);
    const auto clockBefore = plainParameterValue(processor, chipper::parameters::id::clockHz);
    editor.applySafeVariationForLayoutTest(0x43485052u);
    auto changedMacro = false;
    for (size_t i = 0; i < macroIds.size(); ++i)
    {
        const auto varied = plainParameterValue(processor, macroIds[i]);
        changedMacro = changedMacro || std::abs(varied - macroValues[i]) > 0.0001f;
        if (varied < 0.0f || varied > 1.0f || std::abs(varied - macroValues[i]) > 0.0801f)
        {
            std::cerr << "editor_size_smoke: safe variation exceeded its macro bounds\n";
            ok = false;
        }
    }
    ok &= expect(changedMacro, "safe variation did not change a musical macro");
    ok &= expect(plainParameterValue(processor, chipper::parameters::id::chipMode) == chipBeforeInit
                     && plainParameterValue(processor, chipper::parameters::id::accuracy) == accuracyBeforeInit
                     && plainParameterValue(processor, chipper::parameters::id::snNoiseMode) == noiseBeforeInit
                     && plainParameterValue(processor, chipper::parameters::id::source1Enabled) == sourceEnabledBefore
                     && plainParameterValue(processor, chipper::parameters::id::clockHz) == clockBefore,
                 "safe variation changed protected chip or routing state");

    editor.copyWorkflowStateForLayoutTest();
    ok &= setChoiceParameter(processor, chipper::parameters::id::chipMode, chipModeChoiceFor(chipper::ChipMode::sid));
    editor.runEditorUpdateForLayoutTest();
    ok &= setPlainParameter(processor, macro1, 0.91f);
    editor.pasteWorkflowStateForLayoutTest();
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.91f) < 0.0001f,
                 "workflow paste crossed chip boundaries");
    editor.switchWorkflowSlotForLayoutTest(1);
    ok &= expect(chipper::parameters::chipModeFromChoice(static_cast<int>(std::round(
                     plainParameterValue(processor, chipper::parameters::id::chipMode)))) == chipper::ChipMode::sid,
                 "A/B switching restored a stale bank from another chip");
    ok &= setPlainParameter(processor, macro1, 0.77f);
    editor.switchWorkflowSlotForLayoutTest(0);
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.91f) < 0.0001f,
                 "per-chip A/B slot A did not retain the SID sound");
    editor.switchWorkflowSlotForLayoutTest(1);
    ok &= expect(std::abs(plainParameterValue(processor, macro1) - 0.77f) < 0.0001f,
                 "per-chip A/B slot B did not retain the SID sound");

    return ok;
}

bool checkChipUiProfiles()
{
    bool ok = true;
    std::set<chipper::ChipMode> groupedModes;
    const auto modeOrder = chipper::chipModeOrder();

    for (const auto group : chipper::ui::browserGroupOrder())
    {
        const auto modes = chipper::ui::modesInBrowserGroup(group);
        if (modes.empty() || chipper::ui::labelFor(group).empty())
        {
            std::cerr << "editor_size_smoke: empty chip browser group\n";
            ok = false;
        }

        for (const auto mode : modes)
        {
            if (! groupedModes.insert(mode).second)
            {
                std::cerr << "editor_size_smoke: chip appears in more than one browser group\n";
                ok = false;
            }
        }
    }

    if (groupedModes.size() != modeOrder.size())
    {
        std::cerr << "editor_size_smoke: chip browser groups do not cover every mode\n";
        ok = false;
    }

    for (const auto mode : modeOrder)
    {
        const auto profile = chipper::ui::profileFor(mode);
        if (profile.mode != mode
            || profile.familyLabel.empty()
            || profile.browserGroupLabel.empty()
            || profile.visibleSourceCount != chipper::visibleSourceCountForMode(mode)
            || profile.nativeSourceCount != chipper::nativeSourceCountForMode(mode)
            || profile.playSourceColumns < 1
            || profile.playSourceColumns > 5
            || profile.performanceStripHeight <= 0
            || profile.maximumModulesHeight <= 0
            || profile.usesMasterDetailSources != (profile.visibleSourceCount >= 7u || profile.sampler || profile.wavetable))
        {
            std::cerr << "editor_size_smoke: invalid shared UI profile for a chip mode\n";
            ok = false;
        }
    }

    const auto vrc7 = chipper::ui::profileFor(chipper::ChipMode::nesVrc7);
    if (! vrc7.nesFamily
        || vrc7.family != chipper::ui::ChipUiFamily::consoleApu
        || vrc7.browserGroup != chipper::ui::ChipBrowserGroup::nesExpansion
        || ! vrc7.opllOperatorEdit)
    {
        std::cerr << "editor_size_smoke: VRC7 profile lost its NES shell or OPLL edit model\n";
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
    ok &= checkChipUiProfiles();
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
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nesFds);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nesSunsoft5b);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nesMmc5);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::nesVrc7);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::dmg);
    ok &= checkChannelOwnedControlLayout(chipper::ChipMode::sn76489);
    ok &= checkYm2149ToneNoiseMixLayout();
    ok &= checkYm2612DacModeLayout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2612);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2151);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2203);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2608);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2610);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2610b);
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
    ok &= checkGlobalPresetBrowserWorkflow();
    ok &= checkChipSwitchPreservesEditorSettings();
    ok &= checkUnifiedEditorContract();
    ok &= checkWorkflowTools();

    return ok ? 0 : 1;
}
