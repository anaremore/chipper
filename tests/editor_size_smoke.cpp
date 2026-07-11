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
constexpr int expectedEditorSn76489Height = 720;
constexpr int expectedEditorYm2149Height = 720;
constexpr int expectedEditorSaa1099Height = 780;
constexpr int expectedEditorPcSpeakerHeight = 720;
constexpr int expectedEditorZxSpectrumBeeperHeight = 720;
constexpr int expectedEditorSpc700Height = 900;
constexpr int expectedEditorPaulaHeight = 900;
constexpr int expectedEditorYm2608Height = 900;
constexpr int expectedEditorYm2610Height = 900;
constexpr int expectedEditorYm2610bHeight = 900;
constexpr int expectedEditorNamcoWsgHeight = 720;
constexpr int expectedEditorSccHeight = 720;
constexpr int expectedEditorSidHeight = 880;
constexpr int expectedEditorMinimumWidth = 1180;
constexpr int expectedEditorMaximumHeight = expectedEditorSpc700Height;

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
    if (mode == chipper::ChipMode::sn76489)
        return expectedEditorSn76489Height;
    if (mode == chipper::ChipMode::ym2149)
        return expectedEditorYm2149Height;
    if (mode == chipper::ChipMode::saa1099)
        return expectedEditorSaa1099Height;
    if (mode == chipper::ChipMode::pcSpeaker)
        return expectedEditorPcSpeakerHeight;
    if (mode == chipper::ChipMode::zxSpectrumBeeper)
        return expectedEditorZxSpectrumBeeperHeight;
    if (mode == chipper::ChipMode::spc700)
        return expectedEditorSpc700Height;
    if (mode == chipper::ChipMode::paula)
        return expectedEditorPaulaHeight;
    if (mode == chipper::ChipMode::ym2608)
        return expectedEditorYm2608Height;
    if (mode == chipper::ChipMode::ym2610)
        return expectedEditorYm2610Height;
    if (mode == chipper::ChipMode::ym2610b)
        return expectedEditorYm2610bHeight;
    if (mode == chipper::ChipMode::namcoWsg)
        return expectedEditorNamcoWsgHeight;
    if (mode == chipper::ChipMode::scc)
        return expectedEditorSccHeight;

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
    const auto minimumPerformanceHeight = isNesFamily ? 220
        : ((mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2203 || mode == chipper::ChipMode::ym2608 || mode == chipper::ChipMode::ym2610 || mode == chipper::ChipMode::ym2610b) ? 80
        : (mode == chipper::ChipMode::sid ? 96
        : ((mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula) ? 84 : 108)));
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
                          << childPath << " bounds " << absoluteBounds.toString()
                          << " id \"" << child->getComponentID() << "\" name \"" << child->getName()
                          << "\" type " << typeid(*child).name() << '\n';
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
        {
            const auto channelsBounds = editor.getModuleBoundsForLayoutTest(1);
            ok &= expect(channelsBounds.expanded(2).contains(editor.getNativeSliderBoundsForLayoutTest(0)),
                         "SN76489 Tone Stack should stay spatially owned by the tone-channel group");
            ok &= expect(channelsBounds.expanded(2).contains(editor.getNativeSliderBoundsForLayoutTest(1)),
                         "SN76489 Pitch Motion should stay spatially owned by the tone-channel group");
        }
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getSnNoiseModeMenuBoundsForLayoutTest(), "SN76489 noise mode");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getNativeSliderBoundsForLayoutTest(2), "SN76489 preset noise bias");
        ok &= expectControlOwnedBySourceChannel(editor, 3, editor.getNativeSliderBoundsForLayoutTest(3), "SN76489 native noise level");
        ok &= expect(editor.getSourceChannelBoundsForLayoutTest(3).getHeight() > editor.getSourceChannelBoundsForLayoutTest(0).getHeight() + 60,
                     "SN76489 Noise card should be deeper than the compact tone cards");
        ok &= expect(editor.isNativeSliderEnabledForLayoutTest(2),
                     "SN76489 Preset Noise Bias should be enabled while Noise Mode follows the preset");
        ok &= setChoiceParameter(processor, chipper::parameters::id::snNoiseMode, 4);
        editor.runEditorUpdateForLayoutTest();
        ok &= expect(! editor.isNativeSliderEnabledForLayoutTest(2),
                     "SN76489 explicit Noise Mode should disable the preset-only Noise Bias control");
        ok &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("note 1"),
                     "SN76489 Chip Poly should identify Tone 1 as the first allocated note lane");
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(2).contains("note 3"),
                     "SN76489 Chip Poly should identify Tone 3 as the third allocated note lane");
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(3).contains("not note-allocated"),
                     "SN76489 Chip Poly should disclose that Noise remains an SFX lane");
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

    const auto channelsModuleBounds = editor.getModuleBoundsForLayoutTest(1);
    const auto mixerBounds = editor.getModuleBoundsForLayoutTest(2);
    const auto envelopeModuleBounds = editor.getModuleBoundsForLayoutTest(3);
    const auto outputModuleBounds = editor.getModuleBoundsForLayoutTest(5);
    const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
    const auto sharedNoiseBounds = editor.getSourceChannelBoundsForLayoutTest(3);
    const auto toneNoiseBounds = editor.getToneNoiseMixBoundsForLayoutTest();
    const auto noisePitchBounds = editor.getNativeSliderBoundsForLayoutTest(2);
    const auto retiredMacroSliderBounds = editor.getNativeSliderBoundsForLayoutTest(3);
    const auto legacyReadoutBounds = editor.getNativeValueLabelBoundsForLayoutTest(3);
    const auto envelopeShapeBounds = editor.getYmEnvelopeShapeBoundsForLayoutTest();
    const auto envelopeSpeedBounds = editor.getEnvelopeDecayBoundsForLayoutTest();
    const auto envelopePreviewBounds = editor.getYmEnvelopePreviewBoundsForLayoutTest();
    const auto stereoSpreadBounds = editor.getStereoSpreadBoundsForLayoutTest();
    const auto clockBounds = editor.getClockSliderBoundsForLayoutTest();
    const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();

    for (size_t channel = 0; channel < 3; ++channel)
    {
        const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(channel);
        if (channelsModuleBounds.isEmpty() || ! channelsModuleBounds.expanded(2).contains(sourceBounds))
        {
            std::cerr << "editor_size_smoke: YM2149 output channel " << channel
                      << " is not owned by the three-channel module: source " << sourceBounds.toString()
                      << " module " << channelsModuleBounds.toString() << '\n';
            ok = false;
        }
    }

    if (mixerBounds.isEmpty() || mixerBounds.getHeight() < 220)
    {
        std::cerr << "editor_size_smoke: YM2149 shared-noise/routing module is missing useful space: "
                  << mixerBounds.toString() << '\n';
        ok = false;
    }

    if (sharedNoiseBounds.isEmpty()
        || ! mixerBounds.expanded(2).contains(sharedNoiseBounds)
        || channelsModuleBounds.expanded(2).contains(sharedNoiseBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 shared noise must be a generator inside routing, not a fourth output lane: noise "
                  << sharedNoiseBounds.toString() << " routing " << mixerBounds.toString()
                  << " channels " << channelsModuleBounds.toString() << '\n';
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

    if (envelopeModuleBounds.isEmpty() || envelopeModuleBounds.getHeight() < 220)
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

    if (! outputModuleBounds.isEmpty())
    {
        std::cerr << "editor_size_smoke: YM2149 should not reserve a separate output destination below the signal path: "
                  << outputModuleBounds.toString() << '\n';
        ok = false;
    }

    if (performanceBounds.isEmpty()
        || stereoSpreadBounds.isEmpty()
        || clockBounds.isEmpty()
        || outputBounds.isEmpty()
        || ! performanceBounds.expanded(2).contains(stereoSpreadBounds)
        || ! performanceBounds.expanded(2).contains(clockBounds)
        || ! performanceBounds.expanded(2).contains(outputBounds))
    {
        std::cerr << "editor_size_smoke: YM2149 compact performance/output strip is incomplete: spread "
                  << stereoSpreadBounds.toString() << " clock " << clockBounds.toString()
                  << " output " << outputBounds.toString() << " strip " << performanceBounds.toString() << '\n';
        ok = false;
    }

    editor.setSize(expectedEditorMinimumWidth, expectedHeightForChipMode(chipChoice));
    editor.runEditorUpdateForLayoutTest();

    const auto compactChannels = editor.getModuleBoundsForLayoutTest(1);
    const auto compactRouting = editor.getModuleBoundsForLayoutTest(2);
    const auto compactEnvelope = editor.getModuleBoundsForLayoutTest(3);
    const auto compactPerformance = editor.getPerformanceBoundsForLayoutTest();
    const auto compactSharedNoise = editor.getSourceChannelBoundsForLayoutTest(3);
    const auto compactToneNoise = editor.getToneNoiseMixBoundsForLayoutTest();
    const auto compactNoisePitch = editor.getNativeSliderBoundsForLayoutTest(2);
    const auto compactEnvelopeShape = editor.getYmEnvelopeShapeBoundsForLayoutTest();
    const auto compactEnvelopeSpeed = editor.getEnvelopeDecayBoundsForLayoutTest();
    const auto compactEnvelopePreview = editor.getYmEnvelopePreviewBoundsForLayoutTest();
    const auto compactStereoSpread = editor.getStereoSpreadBoundsForLayoutTest();
    const auto compactClock = editor.getClockSliderBoundsForLayoutTest();
    const auto compactOutput = editor.getOutputSliderBoundsForLayoutTest();

    for (size_t channel = 0; channel < 3; ++channel)
        ok &= expect(compactChannels.expanded(2).contains(editor.getSourceChannelBoundsForLayoutTest(channel)),
                     "YM2149 compact width lost an A/B/C output channel");

    ok &= expect(compactRouting.expanded(2).contains(compactSharedNoise),
                 "YM2149 compact width detached shared noise from routing");
    ok &= expect(compactRouting.expanded(2).contains(compactToneNoise),
                 "YM2149 compact width detached default routing from shared noise");
    ok &= expect(compactRouting.expanded(2).contains(compactNoisePitch),
                 "YM2149 compact width detached register-6 pitch from shared noise");
    ok &= expect(compactEnvelope.expanded(2).contains(compactEnvelopeShape)
                     && compactEnvelope.expanded(2).contains(compactEnvelopeSpeed)
                     && compactEnvelope.expanded(2).contains(compactEnvelopePreview),
                 "YM2149 compact width lost shared-envelope controls");
    ok &= expect(compactPerformance.expanded(2).contains(compactStereoSpread)
                     && compactPerformance.expanded(2).contains(compactClock)
                     && compactPerformance.expanded(2).contains(compactOutput),
                 "YM2149 compact width lost performance/output controls");

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
    ok &= setChoiceParameter(processor, chipper::parameters::id::snNoiseMode, 2);
    ok &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);
    ok &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
    ChipperAudioProcessorEditor editor(processor);

    for (const auto width : { expectedEditorMinimumWidth, 1240 })
    {
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto channelModule = editor.getModuleBoundsForLayoutTest(1);
        const auto patchModule = editor.getModuleBoundsForLayoutTest(2);
        const auto operatorModule = editor.getModuleBoundsForLayoutTest(3);
        const auto routeModule = editor.getModuleBoundsForLayoutTest(5);
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
        const auto algorithmBounds = editor.getFmAlgorithmBoundsForLayoutTest();
        const auto algorithmPreviewBounds = editor.getFmAlgorithmPreviewBoundsForLayoutTest();
        const auto feedbackBounds = editor.getFmFeedbackBoundsForLayoutTest();
        const auto dacBounds = editor.getSnNoiseModeBoundsForLayoutTest();
        const auto envelopeBounds = editor.getYmEnvelopeShapeBoundsForLayoutTest();
        const auto lfoBounds = editor.getStereoSpreadBoundsForLayoutTest();
        const auto panBounds = editor.getDmgStereoRouteBoundsForLayoutTest();
        const auto clockBounds = editor.getClockSliderBoundsForLayoutTest();
        const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();

        ok &= expect(editor.getModuleTitleTextForLayoutTest(2) == "Shared Four-Operator Patch"
                         && editor.getModuleTitleTextForLayoutTest(3) == "Shared Operator Matrix"
                         && editor.getModuleTitleTextForLayoutTest(5) == "Envelope, DAC + Routing",
                     "YM2612 dedicated signal-path module titles are missing");
        ok &= expect(editor.getModuleSummaryTextForLayoutTest(5).containsIgnoreCase("VST sample-file loading is not available"),
                     "YM2612 DAC surface should disclose the VST sample-loading limitation");
        ok &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Clock + Output",
                     "YM2612 global strip should contain only clock and output");

        for (size_t channel = 0; channel < 6u; ++channel)
            ok &= expect(channelModule.expanded(2).contains(editor.getSourceChannelBoundsForLayoutTest(channel)),
                         "YM2612 source channel escaped the six-channel deck");
        ok &= expect(editor.getSourceChannelButtonTextForLayoutTest(5).contains("DAC $2A stream"),
                     "YM2612 channel 6 should visibly become the DAC lane");

        for (const auto control : {
                 algorithmBounds,
                 algorithmPreviewBounds,
                 feedbackBounds,
                 editor.getNativeSliderBoundsForLayoutTest(0),
                 editor.getNativeSliderBoundsForLayoutTest(2),
                 editor.getNativeSliderBoundsForLayoutTest(3) })
        {
            ok &= expect(! control.isEmpty() && patchModule.expanded(2).contains(control),
                         "YM2612 shared patch control escaped its owning module");
        }

        for (size_t op = 0; op < 4u; ++op)
            ok &= expect(operatorModule.expanded(2).contains(editor.getFmOperatorCardBoundsForLayoutTest(op)),
                         "YM2612 operator card escaped the shared operator matrix");

        for (const auto& [control, label] : std::array<std::pair<juce::Rectangle<int>, const char*>, 4> {
                 std::pair { envelopeBounds, "Envelope Shape" },
                 std::pair { dacBounds, "DAC Mode" },
                 std::pair { lfoBounds, "LFO Depth" },
                 std::pair { panBounds, "Pan" } })
        {
            if (control.isEmpty() || ! routeModule.expanded(2).contains(control))
            {
                std::cerr << "editor_size_smoke: YM2612 " << label
                          << " is missing from Envelope, DAC + Routing\n";
                ok = false;
            }
        }

        ok &= expect(dacBounds.getWidth() >= 240 && dacBounds.getHeight() >= 20,
                     "YM2612 DAC mode control is below readable size");
        ok &= expect(performanceBounds.expanded(2).contains(clockBounds)
                         && performanceBounds.expanded(2).contains(outputBounds),
                     "YM2612 clock/output controls escaped the compact global strip");
        if (editor.isNativeSliderEnabledForLayoutTest(0)
            || ! editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only")
            || (! editor.getNativeValueLabelTextForLayoutTest(0).contains("explicit Alg")
                && ! editor.getNativeValueLabelTextForLayoutTest(0).contains("Recipe owns Algorithm")))
        {
            std::cerr << "editor_size_smoke: YM2612 Algorithm Bias should be contextual when an explicit Algorithm owns the register: enabled="
                      << editor.isNativeSliderEnabledForLayoutTest(0)
                      << " label='" << editor.getNativeLabelTextForLayoutTest(0)
                      << "' value='" << editor.getNativeValueLabelTextForLayoutTest(0) << "'\n";
            ok = false;
        }
    }

    ok &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
    editor.runEditorUpdateForLayoutTest();
    ok &= expect(editor.isNativeSliderEnabledForLayoutTest(0)
                     && editor.getNativeLabelTextForLayoutTest(0) == "Algorithm Bias",
                 "YM2612 Algorithm Bias should activate for the Manual recipe in Preset mode");

    return ok;
}

bool checkOpl3UnifiedTopologyLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::opl3);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: OPL3 chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 1);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto laneModule = editor.getModuleBoundsForLayoutTest(1);
        const auto patchModule = editor.getModuleBoundsForLayoutTest(2);
        const auto operatorModule = editor.getModuleBoundsForLayoutTest(3);
        const auto pathModule = editor.getModuleBoundsForLayoutTest(5);
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();

        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Nine OPL Lanes"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Topology + Shared Patch"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Operator Register State"
                              && editor.getModuleTitleTextForLayoutTest(5) == "Active Signal Path",
                          "OPL3 dedicated signal-path module titles are missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty(),
                          "OPL3 should not restore detached profile or motion destinations");
        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Clock + Output",
                          "OPL3 global strip should contain only clock and output");

        for (size_t lane = 0; lane < 9u; ++lane)
        {
            const auto card = editor.getSourceChannelBoundsForLayoutTest(lane);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(lane);
            if (card.isEmpty()
                || ! laneModule.expanded(2).contains(card)
                || card.getWidth() < 300
                || card.getHeight() < 68
                || level.isEmpty()
                || ! card.expanded(2).contains(level)
                || level.getHeight() < 10)
            {
                std::cerr << "editor_size_smoke: OPL3 lane " << (lane + 1u)
                          << " is not readable inside the 3x3 lane matrix at width " << editorWidth
                          << ": card " << card.toString() << " level " << level.toString() << '\n';
                widthOk = false;
            }
        }

        const std::array<juce::Rectangle<int>, 7> patchControls {
            editor.getYmEnvelopeShapeBoundsForLayoutTest(),
            editor.getOplWaveformBoundsForLayoutTest(),
            editor.getOplWaveformPreviewBoundsForLayoutTest(),
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getNativeSliderBoundsForLayoutTest(1),
            editor.getNativeSliderBoundsForLayoutTest(2),
            editor.getNativeSliderBoundsForLayoutTest(3)
        };
        for (const auto& control : patchControls)
        {
            if (control.isEmpty()
                || ! patchModule.expanded(2).contains(control)
                || control.getWidth() < 96
                || control.getHeight() < 16)
            {
                std::cerr << "editor_size_smoke: OPL3 topology/shared-patch control escaped or collapsed at width "
                          << editorWidth << ": control " << control.toString()
                          << " patch " << patchModule.toString() << '\n';
                widthOk = false;
            }
        }

        for (size_t row = 0; row < 3u; ++row)
        {
            const auto card = editor.getFmOperatorCardBoundsForLayoutTest(row);
            if (card.isEmpty()
                || ! operatorModule.expanded(2).contains(card)
                || card.getWidth() < 280
                || card.getHeight() < 36)
            {
                std::cerr << "editor_size_smoke: OPL3 operator-state row " << row
                          << " is unreadable at width " << editorWidth
                          << ": card " << card.toString() << " module " << operatorModule.toString() << '\n';
                widthOk = false;
            }
        }

        const auto clockBounds = editor.getClockSliderBoundsForLayoutTest();
        const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();
        widthOk &= expect(performanceBounds.expanded(2).contains(clockBounds)
                              && performanceBounds.expanded(2).contains(outputBounds)
                              && ! clockBounds.intersects(outputBounds),
                          "OPL3 clock/output controls escaped the compact global strip");
        widthOk &= expect(editor.getStereoSpreadBoundsForLayoutTest().isEmpty()
                              && editor.getDmgStereoRouteBoundsForLayoutTest().isEmpty(),
                          "OPL3 should not leave detached generic routing controls in Active Signal Path");

        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("Ch 1 | 2-op voice")
                              && editor.getSourceChannelButtonTextForLayoutTest(8).contains("Ch 9 | 2-op voice")
                              && editor.getModuleSummaryTextForLayoutTest(5).containsIgnoreCase("Nine independent two-operator voices"),
                          "OPL3 melodic topology is not explained by its lanes and active signal path");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 2);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(6).contains("Bass Drum")
                              && editor.getSourceChannelButtonTextForLayoutTest(7).contains("Hi-Hat + Snare")
                              && editor.getSourceChannelButtonTextForLayoutTest(8).contains("Tom + Cymbal")
                              && editor.getModuleSummaryTextForLayoutTest(5).contains("$BD percussion"),
                          "OPL3 rhythm topology does not expose the five native percussion roles");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 3);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("Layer 1+10")
                              && editor.getSourceChannelButtonTextForLayoutTest(8).contains("Layer 9+18")
                              && editor.getModuleSummaryTextForLayoutTest(5).containsIgnoreCase("Nine paired layers"),
                          "OPL3 18-channel layer topology does not explain the low/high bank pairing");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 4);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("Pair 1+4")
                              && editor.getSourceChannelButtonTextForLayoutTest(3).contains("Ops 3-4")
                              && ! editor.isSourceChannelButtonEnabledForLayoutTest(3)
                              && editor.isSourceChannelButtonEnabledForLayoutTest(6)
                              && editor.getModuleSummaryTextForLayoutTest(5).containsIgnoreCase("Three linked 4-op voices"),
                          "OPL3 4-op topology does not distinguish key lanes, paired stages, and remaining 2-op voices");

        widthOk &= expect(! pathModule.isEmpty()
                              && ! editor.getModuleSummaryBoundsForLayoutTest(5).isEmpty(),
                          "OPL3 active signal-path explanation is missing");
        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);
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
        if (mode == chipper::ChipMode::ym2612)
        {
            const auto patchModuleBounds = editor.getModuleBoundsForLayoutTest(2);
            if (sliderBounds.isEmpty()
                || ! patchModuleBounds.expanded(2).contains(sliderBounds)
                || ! patchModuleBounds.expanded(2).contains(groupBounds)
                || ! patchModuleBounds.expanded(2).contains(labelBounds)
                || ! patchModuleBounds.expanded(2).contains(valueBounds))
            {
                std::cerr << "editor_size_smoke: " << modeLabel
                          << " shared Operator Tone/FM Level controls should live beside Algorithm and Feedback\n";
                std::cerr << "  module " << patchModuleBounds.toString()
                          << " slider " << sliderBounds.toString()
                          << " group " << groupBounds.toString()
                          << " label " << labelBounds.toString()
                          << " value " << valueBounds.toString() << '\n';
                ok = false;
            }
        }
        else if (mode == chipper::ChipMode::ym2151)
        {
            const auto patchModuleBounds = editor.getModuleBoundsForLayoutTest(2);
            const auto optionalFieldEscapes = [&patchModuleBounds](juce::Rectangle<int> field)
            {
                return ! field.isEmpty() && ! patchModuleBounds.expanded(2).contains(field);
            };
            if (sliderBounds.isEmpty()
                || labelBounds.isEmpty()
                || ! patchModuleBounds.expanded(2).contains(sliderBounds)
                || ! patchModuleBounds.expanded(2).contains(labelBounds)
                || optionalFieldEscapes(groupBounds)
                || optionalFieldEscapes(valueBounds))
            {
                std::cerr << "editor_size_smoke: YM2151 shared Operator Tone/FM Level controls should live beside Algorithm and Feedback\n";
                ok = false;
            }
        }
        else if (mode == chipper::ChipMode::ym2203 || mode == chipper::ChipMode::ym2608 || mode == chipper::ChipMode::ym2610 || mode == chipper::ChipMode::ym2610b)
        {
            const auto footer = editor.getPerformanceBoundsForLayoutTest();
            const auto optionalFieldEscapes = [&footer](juce::Rectangle<int> field)
            {
                return ! field.isEmpty() && ! footer.expanded(2).contains(field);
            };
            if (sliderBounds.isEmpty()
                || labelBounds.isEmpty()
                || ! footer.expanded(2).contains(sliderBounds)
                || ! footer.expanded(2).contains(labelBounds)
                || optionalFieldEscapes(groupBounds)
                || optionalFieldEscapes(valueBounds))
            {
                std::cerr << "editor_size_smoke: OPN/OPNA cross-engine FM/SSG controls should live in the compact shared footer\n";
                ok = false;
            }
        }
        else if (! sliderBounds.isEmpty() || ! groupBounds.isEmpty() || ! labelBounds.isEmpty() || ! valueBounds.isEmpty())
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

bool checkYm2151UnifiedOpmLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2151);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2151 chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto channels = editor.getModuleBoundsForLayoutTest(1);
        const auto patch = editor.getModuleBoundsForLayoutTest(2);
        const auto operators = editor.getModuleBoundsForLayoutTest(3);
        const auto routing = editor.getModuleBoundsForLayoutTest(5);
        const auto footer = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! channels.isEmpty() && channels.getHeight() >= 254,
                          "YM2151 should reserve two readable rows for all eight OPM channels");
        widthOk &= expect(! patch.isEmpty() && ! operators.isEmpty() && ! routing.isEmpty(),
                          "YM2151 unified patch, operator matrix, or routing module is missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty(),
                          "YM2151 should retire detached profile and motion destinations");
        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Eight OPM Channels"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Shared Four-Operator Patch"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Shared Operator Matrix"
                              && editor.getModuleTitleTextForLayoutTest(5) == "Shared LFO + Stereo Routing",
                          "YM2151 module titles should explain OPM ownership and signal flow");

        std::array<juce::Rectangle<int>, 8> cards {};
        for (size_t channel = 0; channel < cards.size(); ++channel)
        {
            cards[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(channel);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(channel);
            widthOk &= expect(cards[channel].getHeight() >= 98
                                  && channels.expanded(2).contains(cards[channel]),
                              "YM2151 channel card should remain readable and owned by the channel bank");
            widthOk &= expect(! level.isEmpty()
                                  && level.getHeight() >= 12
                                  && cards[channel].expanded(2).contains(level),
                              "YM2151 channel trim should remain inside its owning card");
            widthOk &= expect(header.startsWith("OPM " + juce::String(static_cast<int>(channel + 1u)) + " | A")
                                  && (header.contains("L+R") || header.contains(" | L |") || header.contains(" | R |")),
                              "YM2151 channel header should expose channel, algorithm, and native pan state");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "YM2151 channel cards should not overlap");

        const auto noise = editor.getSnNoiseModeBoundsForLayoutTest();
        widthOk &= expect(! noise.isEmpty()
                              && noise.getHeight() >= 18
                              && cards[7].expanded(2).contains(noise),
                          "YM2151 native noise selector must belong to channel 8");
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(7).contains("Sine"),
                          "YM2151 channel 8 should disclose when operator 4 remains tonal");

        const std::array<juce::Rectangle<int>, 6> sharedPatchControls {
            editor.getFmAlgorithmBoundsForLayoutTest(),
            editor.getFmAlgorithmPreviewBoundsForLayoutTest(),
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getFmFeedbackBoundsForLayoutTest(),
            editor.getNativeSliderBoundsForLayoutTest(2),
            editor.getNativeSliderBoundsForLayoutTest(3)
        };
        for (const auto& control : sharedPatchControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && patch.expanded(2).contains(control),
                              "YM2151 shared four-operator control should remain readable and owned by its patch");
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(0),
                          "YM2151 Algorithm Bias should be active for Manual + Preset");

        for (size_t op = 0; op < 4u; ++op)
            widthOk &= expect(! editor.getFmOperatorCardBoundsForLayoutTest(op).isEmpty()
                                  && operators.expanded(2).contains(editor.getFmOperatorCardBoundsForLayoutTest(op)),
                              "YM2151 operator card should remain inside the shared operator matrix");

        const auto lfo = editor.getStereoSpreadBoundsForLayoutTest();
        const auto pan = editor.getDmgStereoRouteBoundsForLayoutTest();
        widthOk &= expect(! lfo.isEmpty() && lfo.getHeight() >= 16 && routing.expanded(2).contains(lfo),
                          "YM2151 LFO Depth should live in shared modulation/routing");
        widthOk &= expect(! pan.isEmpty() && pan.getHeight() >= 18 && routing.expanded(2).contains(pan),
                          "YM2151 native pan pattern should live in shared modulation/routing");
        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Clock + Output"
                              && footer.getHeight() <= 90,
                          "YM2151 footer should be the compact clock/output stage");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only"),
                          "YM2151 explicit Algorithm should visibly take ownership from Algorithm Bias");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::snNoiseMode, 4);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(7).contains("Noise")
                              && editor.getSourceChannelButtonTextForLayoutTest(7).containsIgnoreCase("note 8"),
                          "YM2151 channel 8 should disclose native noise and Chip Poly allocation together");
        for (size_t channel = 0; channel < cards.size(); ++channel)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(channel).containsIgnoreCase("note " + juce::String(static_cast<int>(channel + 1u))),
                              "YM2151 Chip Poly headers should expose all eight allocation lanes");

        ok &= widthOk;
    }

    return ok;
}

bool checkYm2413UnifiedOpllLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2413);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2413 chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 1);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 0);
        widthOk &= setPlainParameter(processor, chipper::parameters::id::macroControl2, 0.5f);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto lanes = editor.getModuleBoundsForLayoutTest(1);
        const auto topology = editor.getModuleBoundsForLayoutTest(2);
        const auto userPatch = editor.getModuleBoundsForLayoutTest(3);
        const auto footer = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! lanes.isEmpty() && lanes.getHeight() >= 276,
                          "YM2413 should reserve a readable three-row bank for all nine OPLL lanes");
        widthOk &= expect(! topology.isEmpty() && ! userPatch.isEmpty(),
                          "YM2413 instrument/topology or shared User0 module is missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(5).isEmpty(),
                          "YM2413 should retire detached profile, motion, and output destinations");
        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Nine OPLL Lanes"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Instrument + Topology"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Shared User0 Patch",
                          "YM2413 module titles should explain OPLL lane and shared-patch ownership");

        std::array<juce::Rectangle<int>, 9> cards {};
        for (size_t channel = 0; channel < cards.size(); ++channel)
        {
            cards[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(channel);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(channel);
            widthOk &= expect(cards[channel].getHeight() >= 70
                                  && cards[channel].getWidth() >= 330
                                  && lanes.expanded(2).contains(cards[channel]),
                              "YM2413 lane card should remain readable and owned by the lane bank");
            widthOk &= expect(! level.isEmpty()
                                  && level.getHeight() >= 12
                                  && cards[channel].expanded(2).contains(level),
                              "YM2413 lane level should remain inside its owning card");
            widthOk &= expect(header.startsWith("OPLL " + juce::String(static_cast<int>(channel + 1u)) + " | I")
                                  && header.contains("melodic layer"),
                              "YM2413 melodic lane header should expose instrument, volume, and allocation role");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "YM2413 lane cards should not overlap");

        const auto instrument = editor.getOpllInstrumentBoundsForLayoutTest();
        const auto rhythm = editor.getYmEnvelopeShapeBoundsForLayoutTest();
        widthOk &= expect(! instrument.isEmpty()
                              && instrument.getHeight() >= 24
                              && topology.expanded(2).contains(instrument),
                          "YM2413 ROM/User0 instrument selector should live in Instrument + Topology");
        widthOk &= expect(! rhythm.isEmpty()
                              && rhythm.getHeight() >= 24
                              && topology.expanded(2).contains(rhythm),
                          "YM2413 native rhythm selector should remain visible in Instrument + Topology");

        for (size_t op = 0; op < 2u; ++op)
        {
            const auto card = editor.getFmOperatorCardBoundsForLayoutTest(op);
            widthOk &= expect(! card.isEmpty()
                                  && card.getHeight() >= 90
                                  && userPatch.expanded(2).contains(card),
                              "YM2413 Mod/Carrier card should remain inside the shared User0 patch");
            widthOk &= expect(! editor.getFmOperatorLevelSliderBoundsForLayoutTest(op).isEmpty()
                                  && ! editor.getFmOperatorMultiplierBoundsForLayoutTest(op).isEmpty()
                                  && ! editor.getFmOperatorAttackRateBoundsForLayoutTest(op).isEmpty(),
                              "YM2413 User0 row should expose level, multiplier, and operator EG controls");
            widthOk &= expect(editor.isFmOperatorLevelEnabledForLayoutTest(op),
                              "YM2413 User0 controls should be editable while Preset/Custom is selected");
        }
        widthOk &= expect(editor.getFmOperatorNameTextForLayoutTest(0) == "Mod"
                              && editor.getFmOperatorNameTextForLayoutTest(1) == "Car",
                          "YM2413 User0 rows should identify modulator and carrier roles");

        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Performance + Output"
                              && footer.getHeight() == 124,
                          "YM2413 footer should be the compact shared performance/output stage");
        widthOk &= expect(footer.expanded(2).contains(editor.getClockSliderBoundsForLayoutTest())
                              && footer.expanded(2).contains(editor.getOutputSliderBoundsForLayoutTest()),
                          "YM2413 clock and output should remain in the compact footer");
        widthOk &= expect(editor.getNativeLabelTextForLayoutTest(1) == "Tuning Offset"
                              && editor.getNativeSliderTextForLayoutTest(1) == "+0 st"
                              && editor.getNativeValueLabelTextForLayoutTest(1).contains("Tuning +0 st"),
                          "YM2413 should describe the engine's global -6..+6 semitone offset truthfully");
        widthOk &= expect(editor.getNativeSliderTextForLayoutTest(0).startsWith("I")
                              && editor.getNativeSliderTextForLayoutTest(3).startsWith("V"),
                          "YM2413 compact footer should use instrument and native volume vocabulary");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 12);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isFmOperatorLevelEnabledForLayoutTest(0)
                              && editor.getFmOperatorValueTextForLayoutTest(0).contains("User0 editor inactive"),
                          "YM2413 explicit ROM instruments should visibly take ownership from User0");
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only"),
                          "YM2413 explicit ROM instrument should disable Instrument Bias without losing its value");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setPlainParameter(processor, chipper::parameters::id::fmOperator1Level, 0.72f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isFmOperatorLevelEnabledForLayoutTest(0)
                              && editor.getSourceChannelButtonTextForLayoutTest(0).contains("User0")
                              && editor.getWaveShapeValueTextForLayoutTest().contains("Custom slot 0"),
                          "YM2413 operator override should activate and disclose the shared User0 patch");
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.isNativeSliderEnabledForLayoutTest(2),
                          "YM2413 Manual User0 should disable ROM bias and enable the Follow motion control");

        widthOk &= setPlainParameter(processor, chipper::parameters::id::fmOperator1Level, 0.5f);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 5);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 2);
        editor.runEditorUpdateForLayoutTest();
        for (size_t channel = 0; channel < 6u; ++channel)
            widthOk &= expect(! editor.isSourceChannelButtonEnabledForLayoutTest(channel)
                                  && editor.getSourceChannelButtonTextForLayoutTest(channel).contains("idle"),
                              "YM2413 Drum/Hit rhythm should visibly retire melodic lanes 1-6");
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(6).startsWith("BD | $36")
                              && editor.getSourceChannelButtonTextForLayoutTest(7).startsWith("HH+SD | $37")
                              && editor.getSourceChannelButtonTextForLayoutTest(8).startsWith("TOM+CYM | $38"),
                          "YM2413 rhythm cards should expose their native instrument pairs and volume registers");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymEnvelopeShape, 2);
        editor.runEditorUpdateForLayoutTest();
        for (size_t channel = 0; channel < 6u; ++channel)
            widthOk &= expect(editor.isSourceChannelButtonEnabledForLayoutTest(channel)
                                  && editor.getSourceChannelButtonTextForLayoutTest(channel).contains("melodic + rhythm"),
                              "YM2413 Big Mono explicit rhythm should retain melodic lanes for non-drum recipes");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t channel = 0; channel < 6u; ++channel)
            widthOk &= expect(! editor.isSourceChannelButtonEnabledForLayoutTest(channel)
                                  && editor.getSourceChannelButtonTextForLayoutTest(channel).contains("rhythm owns notes"),
                              "YM2413 Chip Poly rhythm should show that every note triggers the rhythm set");

        editor.showPresetBrowserForLayoutTest();
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isPresetBrowserVisibleForLayoutTest()
                              && editor.isPresetBrowserAboveWorkspaceForLayoutTest()
                              && ! editor.getGlobalPresetBrowserSearchBoundsForLayoutTest().isEmpty(),
                          "YM2413 preset browser should remain open as the sole overlay until dismissed");

        ok &= widthOk;
    }

    return ok;
}

bool checkYm2203UnifiedOpnLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2203);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2203 chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 0);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto lanes = editor.getModuleBoundsForLayoutTest(1);
        const auto fmPatch = editor.getModuleBoundsForLayoutTest(2);
        const auto operators = editor.getModuleBoundsForLayoutTest(3);
        const auto ssgGenerator = editor.getModuleBoundsForLayoutTest(5);
        const auto footer = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! lanes.isEmpty() && lanes.getHeight() >= 230,
                          "YM2203 should reserve two readable rows for its three FM and three SSG lanes");
        widthOk &= expect(! fmPatch.isEmpty() && ! operators.isEmpty() && ! ssgGenerator.isEmpty(),
                          "YM2203 unified FM patch, operator matrix, or SSG generator module is missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty(),
                          "YM2203 should retire detached profile and motion destinations");
        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Three FM + Three SSG Lanes"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Shared FM Patch"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Shared Operator Matrix"
                              && editor.getModuleTitleTextForLayoutTest(5) == "Shared SSG Generator",
                          "YM2203 module titles should explain FM, SSG, and shared ownership");

        std::array<juce::Rectangle<int>, 6> cards {};
        for (size_t lane = 0; lane < cards.size(); ++lane)
        {
            cards[lane] = editor.getSourceChannelBoundsForLayoutTest(lane);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(lane);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(lane);
            widthOk &= expect(cards[lane].getHeight() >= 86
                                  && cards[lane].getWidth() >= 350
                                  && lanes.expanded(2).contains(cards[lane]),
                              "YM2203 FM/SSG lane card should remain readable inside the lane bank");
            widthOk &= expect(! level.isEmpty()
                                  && level.getHeight() >= 12
                                  && cards[lane].expanded(2).contains(level),
                              "YM2203 lane level should remain inside its owning card");
            if (lane < 3u)
                widthOk &= expect(header.startsWith("FM " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("| A"),
                                  "YM2203 FM headers should expose lane, Big Mono stack role, and algorithm");
            else
                widthOk &= expect(header.startsWith("SSG ")
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane - 2u)))
                                      && (header.contains("Tone") || header.contains("Noise") || header.contains("T+N") || header.contains("Off")),
                                  "YM2203 SSG headers should expose paired stack role and resolved mixer state");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "YM2203 FM/SSG lane cards should not overlap");

        for (size_t ssg = 0; ssg < 3u; ++ssg)
        {
            const auto mix = editor.getYmChannelMixBoundsForLayoutTest(ssg);
            widthOk &= expect(! mix.isEmpty()
                                  && mix.getHeight() >= 24
                                  && cards[ssg + 3u].expanded(2).contains(mix),
                              "Each YM2203 SSG lane must own its Tone/Noise mix selector");
        }

        const std::array<juce::Rectangle<int>, 5> fmPatchControls {
            editor.getFmAlgorithmBoundsForLayoutTest(),
            editor.getFmAlgorithmPreviewBoundsForLayoutTest(),
            editor.getYmEnvelopeShapeBoundsForLayoutTest(),
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getFmFeedbackBoundsForLayoutTest()
        };
        for (const auto& control : fmPatchControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && fmPatch.expanded(2).contains(control),
                              "YM2203 FM control should remain readable and owned by Shared FM Patch");
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(0),
                          "YM2203 Algorithm Bias should be active for Manual + Preset");

        for (size_t op = 0; op < 4u; ++op)
            widthOk &= expect(! editor.getFmOperatorCardBoundsForLayoutTest(op).isEmpty()
                                  && operators.expanded(2).contains(editor.getFmOperatorCardBoundsForLayoutTest(op)),
                              "YM2203 operator card should remain inside the shared operator matrix");

        const auto ssgEnvelope = editor.getSnNoiseModeBoundsForLayoutTest();
        const auto ssgEnvelopePeriod = editor.getEnvelopeDecayBoundsForLayoutTest();
        widthOk &= expect(! ssgEnvelope.isEmpty()
                              && ssgEnvelope.getHeight() >= 20
                              && ssgGenerator.expanded(2).contains(ssgEnvelope),
                          "YM2203 SSG envelope shape must live in Shared SSG Generator");
        widthOk &= expect(editor.isEnvelopeDecayVisibleForLayoutTest()
                              && ! ssgEnvelopePeriod.isEmpty()
                              && ssgEnvelopePeriod.getHeight() >= 16
                              && ssgGenerator.expanded(2).contains(ssgEnvelopePeriod),
                          "YM2203 SSG envelope period must live in Shared SSG Generator");

        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Shared FM/SSG + Output"
                              && footer.getHeight() == 88,
                          "YM2203 footer should contain only cross-engine color, clock, and output");
        for (const auto control : { editor.getNativeSliderBoundsForLayoutTest(2),
                                    editor.getNativeSliderBoundsForLayoutTest(3),
                                    editor.getClockSliderBoundsForLayoutTest(),
                                    editor.getOutputSliderBoundsForLayoutTest() })
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && footer.expanded(2).contains(control),
                              "YM2203 shared bridge/clock/output control escaped the compact footer");
        widthOk &= expect(editor.getNativeLabelTextForLayoutTest(2) == "FM Tone + SSG Noise"
                              && editor.getNativeSliderTextForLayoutTest(2).startsWith("N")
                              && editor.getNativeSliderTextForLayoutTest(3).startsWith("V"),
                          "YM2203 shared bridge controls should use FM/SSG and native register vocabulary");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only"),
                          "YM2203 explicit Algorithm should visibly take ownership from Algorithm Bias");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::ymChannelAMix, 2);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::snNoiseMode, 4);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(3).contains("Noise Env"),
                          "YM2203 SSG A should disclose its explicit Noise mix and shared envelope state");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t lane = 0; lane < 6u; ++lane)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(lane).containsIgnoreCase("note " + juce::String(static_cast<int>(lane + 1u))),
                              "YM2203 Chip Poly headers should expose all six allocation lanes");

        editor.showPresetBrowserForLayoutTest();
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isPresetBrowserVisibleForLayoutTest()
                              && editor.isPresetBrowserAboveWorkspaceForLayoutTest()
                              && ! editor.getGlobalPresetBrowserSearchBoundsForLayoutTest().isEmpty(),
                          "YM2203 preset browser should remain open as the sole overlay until dismissed");

        ok &= widthOk;
    }

    return ok;
}

bool checkYm2608UnifiedOpnaLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2608);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2608 chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 0);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto lanes = editor.getModuleBoundsForLayoutTest(1);
        const auto fmPatch = editor.getModuleBoundsForLayoutTest(2);
        const auto operators = editor.getModuleBoundsForLayoutTest(3);
        const auto ssgGenerator = editor.getModuleBoundsForLayoutTest(4);
        const auto adpcmLayers = editor.getModuleBoundsForLayoutTest(5);
        const auto footer = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(editor.getHeight() == expectedEditorYm2608Height
                              && ! lanes.isEmpty() && lanes.getHeight() >= 304,
                          "YM2608 should use its 900px three-row FM/SSG lane surface");
        widthOk &= expect(! fmPatch.isEmpty() && ! operators.isEmpty() && ! ssgGenerator.isEmpty() && ! adpcmLayers.isEmpty(),
                          "YM2608 unified FM patch, operator, SSG, or ADPCM module is missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty(),
                          "YM2608 should retire the detached profile destination");
        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Six FM + Three SSG Lanes"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Shared FM Patch"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Shared Operator Matrix"
                              && editor.getModuleTitleTextForLayoutTest(4) == "Shared SSG Generator"
                              && editor.getModuleTitleTextForLayoutTest(5) == "Rhythm + ADPCM Layers",
                          "YM2608 module titles should explain FM, SSG, rhythm, and ADPCM ownership");

        std::array<juce::Rectangle<int>, 9> cards {};
        for (size_t lane = 0; lane < cards.size(); ++lane)
        {
            cards[lane] = editor.getSourceChannelBoundsForLayoutTest(lane);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(lane);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(lane);
            widthOk &= expect(cards[lane].getHeight() >= 82
                                  && cards[lane].getWidth() >= 350
                                  && lanes.expanded(2).contains(cards[lane]),
                              "YM2608 FM/SSG lane card should remain readable inside the 3x3 bank");
            widthOk &= expect(! level.isEmpty()
                                  && level.getHeight() >= 8
                                  && cards[lane].expanded(2).contains(level),
                              "YM2608 lane level should remain inside its owning card");
            if (lane < 6u)
                widthOk &= expect(header.startsWith("FM " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("| A"),
                                  "YM2608 FM header should expose lane, stack role, and algorithm");
            else
                widthOk &= expect(header.startsWith("SSG ")
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && (header.contains("Tone") || header.contains("Noise") || header.contains("T+N") || header.contains("Off")),
                                  "YM2608 SSG header should expose lane, stack role, and resolved mixer state");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "YM2608 FM/SSG lane cards should not overlap");

        for (size_t ssg = 0; ssg < 3u; ++ssg)
        {
            const auto mix = editor.getYmChannelMixBoundsForLayoutTest(ssg);
            widthOk &= expect(! mix.isEmpty()
                                  && mix.getHeight() >= 26
                                  && cards[ssg + 6u].expanded(2).contains(mix),
                              "Each YM2608 SSG lane must own its Tone/Noise mix selector");
        }

        const std::array<juce::Rectangle<int>, 5> fmPatchControls {
            editor.getFmAlgorithmBoundsForLayoutTest(),
            editor.getFmAlgorithmPreviewBoundsForLayoutTest(),
            editor.getYmEnvelopeShapeBoundsForLayoutTest(),
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getFmFeedbackBoundsForLayoutTest()
        };
        for (const auto& control : fmPatchControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && fmPatch.expanded(2).contains(control),
                              "YM2608 FM control should remain readable and owned by Shared FM Patch");
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(0),
                          "YM2608 Algorithm Bias should be active for Manual + Preset");

        for (size_t op = 0; op < 4u; ++op)
            widthOk &= expect(! editor.getFmOperatorCardBoundsForLayoutTest(op).isEmpty()
                                  && operators.expanded(2).contains(editor.getFmOperatorCardBoundsForLayoutTest(op)),
                              "YM2608 operator card should remain inside the shared operator matrix");

        widthOk &= expect(ssgGenerator.expanded(2).contains(editor.getSnNoiseModeBoundsForLayoutTest())
                              && ssgGenerator.expanded(2).contains(editor.getEnvelopeDecayBoundsForLayoutTest())
                              && editor.isEnvelopeDecayVisibleForLayoutTest(),
                          "YM2608 shared SSG envelope shape and period should live in Shared SSG Generator");
        widthOk &= expect(adpcmLayers.expanded(2).contains(editor.getSampleFileButtonBoundsForLayoutTest())
                              && adpcmLayers.expanded(2).contains(editor.getSampleFolderButtonBoundsForLayoutTest())
                              && adpcmLayers.expanded(2).contains(editor.getSampleWaveformBoundsForLayoutTest())
                              && editor.getSampleLabelTextForLayoutTest() == "Drum/Hit Layer"
                              && editor.getSampleFileButtonTextForLayoutTest() == "Rhythm"
                              && editor.getSampleFolderButtonTextForLayoutTest() == "ADPCM-B"
                              && editor.getSampleStatusTextForLayoutTest().contains("Drum/Hit only"),
                          "YM2608 ADPCM-A/B conditional layer should remain actionable and explicit");

        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Shared FM/SSG + Stereo Output"
                              && footer.getHeight() == 88,
                          "YM2608 footer should contain only cross-engine, stereo, clock, and output controls");
        for (const auto control : { editor.getNativeSliderBoundsForLayoutTest(2),
                                    editor.getNativeSliderBoundsForLayoutTest(3),
                                    editor.getDmgStereoRouteBoundsForLayoutTest(),
                                    editor.getStereoSpreadBoundsForLayoutTest(),
                                    editor.getClockSliderBoundsForLayoutTest(),
                                    editor.getOutputSliderBoundsForLayoutTest() })
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && footer.expanded(2).contains(control),
                              "YM2608 shared bridge/stereo/clock/output control escaped the compact footer");
        widthOk &= expect(editor.getNativeLabelTextForLayoutTest(2) == "FM Tone + SSG Noise"
                              && editor.getNativeSliderTextForLayoutTest(2).startsWith("N")
                              && editor.getNativeSliderTextForLayoutTest(3).startsWith("V"),
                          "YM2608 shared bridge controls should use FM/SSG and native register vocabulary");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only"),
                          "YM2608 explicit Algorithm should visibly take ownership from Algorithm Bias");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("FM 1 + BD")
                              && editor.getSourceChannelButtonTextForLayoutTest(5).contains("FM 6 + Rim"),
                          "YM2608 Drum recipe should disclose ADPCM-A rhythm ownership on FM lanes 1-6");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t lane = 0; lane < 9u; ++lane)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(lane).containsIgnoreCase("note " + juce::String(static_cast<int>(lane + 1u))),
                              "YM2608 Chip Poly headers should expose all nine allocation lanes");

        editor.showPresetBrowserForLayoutTest();
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isPresetBrowserVisibleForLayoutTest()
                              && editor.isPresetBrowserAboveWorkspaceForLayoutTest()
                              && ! editor.getGlobalPresetBrowserSearchBoundsForLayoutTest().isEmpty(),
                          "YM2608 preset browser should remain open as the sole overlay until dismissed");

        ok &= widthOk;
    }

    return ok;
}

bool checkYm2610UnifiedOpnbLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2610);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2610 chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 0);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto lanes = editor.getModuleBoundsForLayoutTest(1);
        const auto fmPatch = editor.getModuleBoundsForLayoutTest(2);
        const auto operators = editor.getModuleBoundsForLayoutTest(3);
        const auto ssgGenerator = editor.getModuleBoundsForLayoutTest(4);
        const auto adpcmLayers = editor.getModuleBoundsForLayoutTest(5);
        const auto footer = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(editor.getHeight() == expectedEditorYm2610Height
                              && ! lanes.isEmpty() && lanes.getHeight() >= 236,
                          "YM2610 should use its 900px centered two-row FM/SSG surface");
        widthOk &= expect(! fmPatch.isEmpty() && ! operators.isEmpty() && ! ssgGenerator.isEmpty() && ! adpcmLayers.isEmpty(),
                          "YM2610 unified FM patch, operator, SSG, or external ADPCM module is missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty(),
                          "YM2610 should retire the detached profile destination");
        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Four FM + Three SSG Lanes"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Shared FM Patch"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Shared Operator Matrix"
                              && editor.getModuleTitleTextForLayoutTest(4) == "Shared SSG Generator"
                              && editor.getModuleTitleTextForLayoutTest(5) == "External ADPCM-A/B Layers",
                          "YM2610 module titles should explain FM, SSG, and external sample ownership");

        std::array<juce::Rectangle<int>, 7> cards {};
        for (size_t lane = 0; lane < cards.size(); ++lane)
        {
            cards[lane] = editor.getSourceChannelBoundsForLayoutTest(lane);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(lane);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(lane);
            widthOk &= expect(cards[lane].getHeight() >= 90
                                  && cards[lane].getWidth() >= 260
                                  && lanes.expanded(2).contains(cards[lane]),
                              "YM2610 FM/SSG lane card should remain readable inside the 4x2 bank");
            widthOk &= expect(! level.isEmpty()
                                  && level.getHeight() >= 8
                                  && cards[lane].expanded(2).contains(level),
                              "YM2610 lane level should remain inside its owning card");
            if (lane < 4u)
                widthOk &= expect(header.startsWith("FM " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("| A"),
                                  "YM2610 FM header should expose lane, stack role, and algorithm");
            else
                widthOk &= expect(header.startsWith("SSG ")
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && (header.contains("Tone") || header.contains("Noise") || header.contains("T+N") || header.contains("Off")),
                                  "YM2610 SSG header should expose lane, stack role, and resolved mixer state");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "YM2610 FM/SSG lane cards should not overlap");
        widthOk &= expect(std::abs((cards[4].getX() + cards[6].getRight()) / 2 - lanes.getCentreX()) <= 2,
                          "YM2610 SSG A-C should form an intentional centered second row");

        for (size_t ssg = 0; ssg < 3u; ++ssg)
        {
            const auto mix = editor.getYmChannelMixBoundsForLayoutTest(ssg);
            widthOk &= expect(! mix.isEmpty()
                                  && mix.getHeight() >= 26
                                  && cards[ssg + 4u].expanded(2).contains(mix),
                              "Each YM2610 SSG lane must own its Tone/Noise mix selector");
        }

        const std::array<juce::Rectangle<int>, 5> fmPatchControls {
            editor.getFmAlgorithmBoundsForLayoutTest(),
            editor.getFmAlgorithmPreviewBoundsForLayoutTest(),
            editor.getYmEnvelopeShapeBoundsForLayoutTest(),
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getFmFeedbackBoundsForLayoutTest()
        };
        for (const auto& control : fmPatchControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && fmPatch.expanded(2).contains(control),
                              "YM2610 FM control should remain readable and owned by Shared FM Patch");
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(0),
                          "YM2610 Algorithm Bias should be active for Manual + Preset");

        for (size_t op = 0; op < 4u; ++op)
            widthOk &= expect(! editor.getFmOperatorCardBoundsForLayoutTest(op).isEmpty()
                                  && operators.expanded(2).contains(editor.getFmOperatorCardBoundsForLayoutTest(op)),
                              "YM2610 operator card should remain inside the shared operator matrix");

        widthOk &= expect(ssgGenerator.expanded(2).contains(editor.getSnNoiseModeBoundsForLayoutTest())
                              && ssgGenerator.expanded(2).contains(editor.getEnvelopeDecayBoundsForLayoutTest())
                              && editor.isEnvelopeDecayVisibleForLayoutTest(),
                          "YM2610 shared SSG envelope shape and period should live in Shared SSG Generator");
        widthOk &= expect(adpcmLayers.expanded(2).contains(editor.getSampleFileButtonBoundsForLayoutTest())
                              && adpcmLayers.expanded(2).contains(editor.getSampleFolderButtonBoundsForLayoutTest())
                              && adpcmLayers.expanded(2).contains(editor.getSampleWaveformBoundsForLayoutTest())
                              && editor.getSampleLabelTextForLayoutTest() == "Drum/Hit Layers"
                              && editor.getSampleFileButtonTextForLayoutTest() == "ADPCM-A"
                              && editor.getSampleFolderButtonTextForLayoutTest() == "ADPCM-B"
                              && editor.getSampleStatusTextForLayoutTest().contains("Drum/Hit only")
                              && editor.getSampleStatusTextForLayoutTest().contains("A empty")
                              && editor.getSampleStatusTextForLayoutTest().contains("B empty"),
                          "YM2610 external ADPCM empty state should remain actionable, conditional, and explicit");

        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Shared FM/SSG + Stereo Output"
                              && footer.getHeight() == 88,
                          "YM2610 footer should contain only cross-engine, stereo, clock, and output controls");
        for (const auto control : { editor.getNativeSliderBoundsForLayoutTest(2),
                                    editor.getNativeSliderBoundsForLayoutTest(3),
                                    editor.getDmgStereoRouteBoundsForLayoutTest(),
                                    editor.getStereoSpreadBoundsForLayoutTest(),
                                    editor.getClockSliderBoundsForLayoutTest(),
                                    editor.getOutputSliderBoundsForLayoutTest() })
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && footer.expanded(2).contains(control),
                              "YM2610 shared bridge/stereo/clock/output control escaped the compact footer");
        widthOk &= expect(editor.getNativeLabelTextForLayoutTest(2) == "FM Tone + SSG Noise"
                              && editor.getNativeSliderTextForLayoutTest(2).startsWith("N")
                              && editor.getNativeSliderTextForLayoutTest(3).startsWith("V"),
                          "YM2610 shared bridge controls should use FM/SSG and native register vocabulary");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only"),
                          "YM2610 explicit Algorithm should visibly take ownership from Algorithm Bias");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("FM 1 + ADPCM-A1")
                              && editor.getSourceChannelButtonTextForLayoutTest(3).contains("FM 4 + ADPCM-A4")
                              && editor.getSourceChannelButtonTextForLayoutTest(4).contains("SSG A + ADPCM-A5")
                              && editor.getSourceChannelButtonTextForLayoutTest(5).contains("SSG B + ADPCM-A6")
                              && ! editor.getSourceChannelButtonTextForLayoutTest(6).contains("ADPCM-A"),
                          "YM2610 Drum recipe should disclose exact ADPCM-A segment ownership on the first six lanes");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t lane = 0; lane < 7u; ++lane)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(lane).containsIgnoreCase("note " + juce::String(static_cast<int>(lane + 1u))),
                              "YM2610 Chip Poly headers should expose all seven allocation lanes");

        editor.showPresetBrowserForLayoutTest();
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isPresetBrowserVisibleForLayoutTest()
                              && editor.isPresetBrowserAboveWorkspaceForLayoutTest()
                              && ! editor.getGlobalPresetBrowserSearchBoundsForLayoutTest().isEmpty(),
                          "YM2610 preset browser should remain open as the sole overlay until dismissed");

        ok &= widthOk;
    }

    return ok;
}

bool checkYm2610bUnifiedOpnb2Layout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::ym2610b);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: YM2610B chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 0);

        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto lanes = editor.getModuleBoundsForLayoutTest(1);
        const auto fmPatch = editor.getModuleBoundsForLayoutTest(2);
        const auto operators = editor.getModuleBoundsForLayoutTest(3);
        const auto ssgGenerator = editor.getModuleBoundsForLayoutTest(4);
        const auto adpcmLayers = editor.getModuleBoundsForLayoutTest(5);
        const auto footer = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(editor.getHeight() == expectedEditorYm2610bHeight
                              && ! lanes.isEmpty() && lanes.getHeight() >= 304,
                          "YM2610B should use its 900px three-row FM/SSG surface");
        widthOk &= expect(! fmPatch.isEmpty() && ! operators.isEmpty() && ! ssgGenerator.isEmpty() && ! adpcmLayers.isEmpty(),
                          "YM2610B unified FM patch, operator, SSG, or external ADPCM module is missing");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty(),
                          "YM2610B should retire the detached profile destination");
        widthOk &= expect(editor.getModuleTitleTextForLayoutTest(1) == "Six FM + Three SSG Lanes"
                              && editor.getModuleTitleTextForLayoutTest(2) == "Shared FM Patch"
                              && editor.getModuleTitleTextForLayoutTest(3) == "Shared Operator Matrix"
                              && editor.getModuleTitleTextForLayoutTest(4) == "Shared SSG Generator"
                              && editor.getModuleTitleTextForLayoutTest(5) == "External ADPCM-A/B Layers",
                          "YM2610B module titles should explain FM, SSG, and external sample ownership");

        std::array<juce::Rectangle<int>, 9> cards {};
        for (size_t lane = 0; lane < cards.size(); ++lane)
        {
            cards[lane] = editor.getSourceChannelBoundsForLayoutTest(lane);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(lane);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(lane);
            widthOk &= expect(cards[lane].getHeight() >= 82
                                  && cards[lane].getWidth() >= 350
                                  && lanes.expanded(2).contains(cards[lane]),
                              "YM2610B FM/SSG lane card should remain readable inside the 3x3 bank");
            widthOk &= expect(! level.isEmpty()
                                  && level.getHeight() >= 8
                                  && cards[lane].expanded(2).contains(level),
                              "YM2610B lane level should remain inside its owning card");
            if (lane < 6u)
                widthOk &= expect(header.startsWith("FM " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && header.contains("| A"),
                                  "YM2610B FM header should expose lane, stack role, and algorithm");
            else
                widthOk &= expect(header.startsWith("SSG ")
                                      && header.contains("stack note " + juce::String(static_cast<int>(lane + 1u)))
                                      && (header.contains("Tone") || header.contains("Noise") || header.contains("T+N") || header.contains("Off")),
                                  "YM2610B SSG header should expose lane, stack role, and resolved mixer state");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "YM2610B FM/SSG lane cards should not overlap");

        for (size_t ssg = 0; ssg < 3u; ++ssg)
        {
            const auto mix = editor.getYmChannelMixBoundsForLayoutTest(ssg);
            widthOk &= expect(! mix.isEmpty()
                                  && mix.getHeight() >= 26
                                  && cards[ssg + 6u].expanded(2).contains(mix),
                              "Each YM2610B SSG lane must own its Tone/Noise mix selector");
        }

        for (const auto control : { editor.getFmAlgorithmBoundsForLayoutTest(),
                                    editor.getFmAlgorithmPreviewBoundsForLayoutTest(),
                                    editor.getYmEnvelopeShapeBoundsForLayoutTest(),
                                    editor.getNativeSliderBoundsForLayoutTest(0),
                                    editor.getFmFeedbackBoundsForLayoutTest() })
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && fmPatch.expanded(2).contains(control),
                              "YM2610B FM control should remain readable and owned by Shared FM Patch");
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(0),
                          "YM2610B Algorithm Bias should be active for Manual + Preset");

        for (size_t op = 0; op < 4u; ++op)
            widthOk &= expect(! editor.getFmOperatorCardBoundsForLayoutTest(op).isEmpty()
                                  && operators.expanded(2).contains(editor.getFmOperatorCardBoundsForLayoutTest(op)),
                              "YM2610B operator card should remain inside the shared operator matrix");

        widthOk &= expect(ssgGenerator.expanded(2).contains(editor.getSnNoiseModeBoundsForLayoutTest())
                              && ssgGenerator.expanded(2).contains(editor.getEnvelopeDecayBoundsForLayoutTest())
                              && editor.isEnvelopeDecayVisibleForLayoutTest(),
                          "YM2610B shared SSG envelope shape and period should live in Shared SSG Generator");
        widthOk &= expect(adpcmLayers.expanded(2).contains(editor.getSampleFileButtonBoundsForLayoutTest())
                              && adpcmLayers.expanded(2).contains(editor.getSampleFolderButtonBoundsForLayoutTest())
                              && adpcmLayers.expanded(2).contains(editor.getSampleWaveformBoundsForLayoutTest())
                              && editor.getSampleLabelTextForLayoutTest() == "Drum/Hit Layers"
                              && editor.getSampleFileButtonTextForLayoutTest() == "ADPCM-A"
                              && editor.getSampleFolderButtonTextForLayoutTest() == "ADPCM-B"
                              && editor.getSampleStatusTextForLayoutTest().contains("Drum/Hit only")
                              && editor.getSampleStatusTextForLayoutTest().contains("A empty")
                              && editor.getSampleStatusTextForLayoutTest().contains("B empty"),
                          "YM2610B external ADPCM empty state should remain actionable, conditional, and explicit");

        widthOk &= expect(editor.getGlobalStripLabelTextForLayoutTest() == "Shared FM/SSG + Stereo Output"
                              && footer.getHeight() == 88,
                          "YM2610B footer should contain only cross-engine, stereo, clock, and output controls");
        for (const auto control : { editor.getNativeSliderBoundsForLayoutTest(2),
                                    editor.getNativeSliderBoundsForLayoutTest(3),
                                    editor.getDmgStereoRouteBoundsForLayoutTest(),
                                    editor.getStereoSpreadBoundsForLayoutTest(),
                                    editor.getClockSliderBoundsForLayoutTest(),
                                    editor.getOutputSliderBoundsForLayoutTest() })
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && footer.expanded(2).contains(control),
                              "YM2610B shared bridge/stereo/clock/output control escaped the compact footer");
        widthOk &= expect(editor.getNativeLabelTextForLayoutTest(2) == "FM Tone + SSG Noise"
                              && editor.getNativeSliderTextForLayoutTest(2).startsWith("N")
                              && editor.getNativeSliderTextForLayoutTest(3).startsWith("V"),
                          "YM2610B shared bridge controls should use FM/SSG and native register vocabulary");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(0)
                              && editor.getNativeLabelTextForLayoutTest(0).contains("Manual + Preset only"),
                          "YM2610B explicit Algorithm should visibly take ownership from Algorithm Bias");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 5);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("FM 1 + ADPCM-A1")
                              && editor.getSourceChannelButtonTextForLayoutTest(5).contains("FM 6 + ADPCM-A6")
                              && ! editor.getSourceChannelButtonTextForLayoutTest(6).contains("ADPCM-A")
                              && ! editor.getSourceChannelButtonTextForLayoutTest(8).contains("ADPCM-A"),
                          "YM2610B Drum recipe should disclose ADPCM-A1-6 ownership only on FM1-6");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t lane = 0; lane < 9u; ++lane)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(lane).containsIgnoreCase("note " + juce::String(static_cast<int>(lane + 1u))),
                              "YM2610B Chip Poly headers should expose all nine allocation lanes");

        editor.showPresetBrowserForLayoutTest();
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isPresetBrowserVisibleForLayoutTest()
                              && editor.isPresetBrowserAboveWorkspaceForLayoutTest()
                              && ! editor.getGlobalPresetBrowserSearchBoundsForLayoutTest().isEmpty(),
                          "YM2610B preset browser should remain open as the sole overlay until dismissed");

        ok &= widthOk;
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
        const auto envelopePanel = mode == chipper::ChipMode::paula
            ? editor.getPerformanceBoundsForLayoutTest()
            : editor.getModuleBoundsForLayoutTest(3);
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

bool checkHuc6280UnifiedLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::huc6280);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: HuC6280 chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto sourceDeck = editor.getModuleBoundsForLayoutTest(1);
        const auto performance = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! sourceDeck.isEmpty() && sourceDeck.getHeight() >= 440,
                          "HuC6280 unified voice deck should reserve space for six voices and their LFO relationship");
        widthOk &= expect(performance.getHeight() >= 180,
                          "HuC6280 shared performance/output strip should expose two readable rows");
        for (const auto retiredModule : { 0u, 2u, 3u, 4u, 5u })
            widthOk &= expect(editor.getModuleBoundsForLayoutTest(retiredModule).isEmpty(),
                              "HuC6280 should not retain detached generic modules outside the unified voice deck");

        const auto lfoBounds = editor.getDmgStereoRouteBoundsForLayoutTest();
        widthOk &= expect(! lfoBounds.isEmpty()
                              && lfoBounds.getHeight() >= 24
                              && sourceDeck.expanded(2).contains(lfoBounds),
                          "HuC6280 Ch 2 to Ch 1 LFO must live with the voice cards");
        widthOk &= expect(editor.isDmgStereoRouteSegmentVisibleForLayoutTest()
                              && editor.getDmgStereoRouteLabelTextForLayoutTest() == "Ch 2 -> Ch 1 Pitch LFO",
                          "HuC6280 LFO relationship should use chip-specific wording");
        widthOk &= expect(editor.getDmgStereoRouteValueTextForLayoutTest().contains("independent voices")
                              && ! editor.getDmgStereoRouteValueTextForLayoutTest().contains("NR51"),
                          "HuC6280 LFO readout should describe the voice relationship rather than DMG routing");

        widthOk &= expect(editor.getSourceWaveSelectorItemTextForLayoutTest(0, 4) == "Grain",
                          "HuC6280 channels 1-4 should name choice 4 as a generated grain wave");
        widthOk &= expect(editor.getSourceWaveSelectorItemTextForLayoutTest(4, 4) == "Noise",
                          "HuC6280 channels 5-6 should expose the hardware-noise choice");

        std::array<juce::Rectangle<int>, 6> cards {};
        for (size_t channel = 0; channel < cards.size(); ++channel)
        {
            cards[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto wave = editor.getSourceWaveSelectorBoundsForLayoutTest(channel);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(channel);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(channel);

            widthOk &= expect(cards[channel].getHeight() >= 136 && cards[channel].getHeight() <= 144,
                              "HuC6280 source cards should use the dedicated readable height");
            widthOk &= expect(! wave.isEmpty() && wave.getHeight() >= 28 && cards[channel].expanded(2).contains(wave),
                              "HuC6280 wave selector should be owned by its voice card");
            widthOk &= expect(! level.isEmpty() && level.getHeight() >= 16 && cards[channel].expanded(2).contains(level),
                              "HuC6280 level trim should be owned by its voice card");
            widthOk &= expect(header.startsWith("Ch " + juce::String(static_cast<int>(channel + 1u)) + " |"),
                              "HuC6280 voice header should name its channel and role");
        }
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(1).contains("LFO source"),
                          "HuC6280 channel 2 should advertise its LFO-source role even while independent");
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(4).contains("Wave RAM + noise")
                              && editor.getSourceChannelButtonTextForLayoutTest(5).contains("Wave RAM + noise"),
                          "HuC6280 upper channels should advertise their wave/noise architecture");
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "HuC6280 voice cards should not overlap");

        const std::array<juce::Rectangle<int>, 7> sharedControls {
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getNativeSliderBoundsForLayoutTest(1),
            editor.getNativeSliderBoundsForLayoutTest(2),
            editor.getNativeSliderBoundsForLayoutTest(3),
            editor.getEnvelopeDecayBoundsForLayoutTest(),
            editor.getStereoSpreadBoundsForLayoutTest(),
            editor.getOutputSliderBoundsForLayoutTest()
        };
        for (const auto& control : sharedControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && performance.expanded(2).contains(control),
                              "HuC6280 shared controls should remain readable and owned by the shared strip");
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern width off"),
                          "HuC6280 zero-width readout should identify the modern centered-mono convenience");

        widthOk &= setPlainParameter(processor, chipper::parameters::id::stereoSpread, 1.0f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern six-lane")
                              && editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("native balance not modeled"),
                          "HuC6280 full-width readout should not pretend to expose native balance registers");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::dmgStereoRoute, 3);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("LFO target")
                              && editor.getSourceChannelButtonTextForLayoutTest(1).contains("LFO source - muted"),
                          "HuC6280 active LFO should expose channel 1 target and channel 2 muted-source roles");
        widthOk &= expect(editor.getDmgStereoRouteValueTextForLayoutTest().contains("Deep")
                              && editor.getDmgStereoRouteValueTextForLayoutTest().contains("Ch 2 wave -> Ch 1 pitch")
                              && editor.getDmgStereoRouteValueTextForLayoutTest().contains("Ch 2 muted"),
                          "HuC6280 active LFO readout should explain its rate/depth relationship and voice cost");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t channel = 0; channel < cards.size(); ++channel)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(channel).contains("Note " + juce::String(static_cast<int>(channel + 1u))),
                              "HuC6280 Chip Poly headers should expose all six allocation lanes");

        ok &= widthOk;
    }

    return ok;
}

bool checkNamcoWsgUnifiedLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::namcoWsg);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: Namco WSG chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        widthOk &= expect(editor.getHeight() == expectedEditorNamcoWsgHeight,
                          "Namco WSG should use its dedicated compact editor height");
        const auto voiceDeck = editor.getModuleBoundsForLayoutTest(1);
        const auto performance = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! voiceDeck.isEmpty() && voiceDeck.getHeight() >= 340,
                          "Namco WSG unified voice bank should reserve two readable card rows");
        widthOk &= expect(performance.getHeight() >= 180,
                          "Namco WSG shared lane-motion/output strip should expose two readable rows");
        for (const auto retiredModule : { 0u, 2u, 3u, 4u, 5u })
            widthOk &= expect(editor.getModuleBoundsForLayoutTest(retiredModule).isEmpty(),
                              "Namco WSG should not retain detached generic modules");

        std::array<juce::Rectangle<int>, 8> cards {};
        for (size_t lane = 0; lane < cards.size(); ++lane)
        {
            cards[lane] = editor.getSourceChannelBoundsForLayoutTest(lane);
            const auto wave = editor.getSourceWaveSelectorBoundsForLayoutTest(lane);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(lane);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(lane);
            widthOk &= expect(cards[lane].getHeight() >= 136 && cards[lane].getHeight() <= 144,
                              "Namco WSG lane cards should use the dedicated readable height");
            widthOk &= expect(! wave.isEmpty() && wave.getHeight() >= 28 && cards[lane].expanded(2).contains(wave),
                              "Namco WSG per-lane wave selector should be owned by its card");
            widthOk &= expect(! level.isEmpty() && level.getHeight() >= 16 && cards[lane].expanded(2).contains(level),
                              "Namco WSG per-lane 4-bit level should be owned by its card");
            widthOk &= expect(header.startsWith("Lane " + juce::String(static_cast<int>(lane + 1u)) + " | 4-bit Wave RAM | V")
                                  && header.endsWith("/15"),
                              "Namco WSG header should identify lane, Wave RAM depth, and resolved 4-bit volume");
            widthOk &= expect(editor.getSourceWaveSelectorItemTextForLayoutTest(lane, 4) == "Steps",
                              "Namco WSG lane selectors should keep the Steps Wave RAM template");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "Namco WSG lane cards should not overlap");

        const std::array<juce::Rectangle<int>, 7> sharedControls {
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getNativeSliderBoundsForLayoutTest(1),
            editor.getNativeSliderBoundsForLayoutTest(2),
            editor.getNativeSliderBoundsForLayoutTest(3),
            editor.getEnvelopeDecayBoundsForLayoutTest(),
            editor.getStereoSpreadBoundsForLayoutTest(),
            editor.getOutputSliderBoundsForLayoutTest()
        };
        for (const auto& control : sharedControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && performance.expanded(2).contains(control),
                              "Namco WSG shared controls should be readable and owned by the shared strip");

        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(2)
                              && editor.getNativeLabelTextForLayoutTest(2).containsIgnoreCase("select Pulse")
                              && editor.getNativeValueLabelTextForLayoutTest(2).containsIgnoreCase("no Pulse lanes"),
                          "Namco WSG Pulse Width should disclose when no lane uses the Pulse template");
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern width off"),
                          "Namco WSG zero-width readout should identify centered mono as a modern-width state");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 3);
        widthOk &= setPlainParameter(processor, chipper::parameters::id::macroControl3, 1.0f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(2)
                              && editor.getNativeLabelTextForLayoutTest(2) == "Pulse Width"
                              && editor.getNativeValueLabelTextForLayoutTest(2).contains("Pulse high 28/32"),
                          "Namco WSG Pulse Width should activate and show the generated high-sample count for Pulse lanes");

        widthOk &= setPlainParameter(processor, chipper::parameters::id::stereoSpread, 1.0f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern eight-lane")
                              && editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("native pan not modeled"),
                          "Namco WSG full-width readout should distinguish modern spread from unmodeled hardware routing");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t lane = 0; lane < cards.size(); ++lane)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(lane).contains("Note " + juce::String(static_cast<int>(lane + 1u))),
                              "Namco WSG Chip Poly headers should expose all eight allocation lanes");

        ok &= widthOk;
    }

    return ok;
}

bool checkSccUnifiedLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::scc);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: SCC chip mode choice unavailable\n";
        return false;
    }

    auto ok = true;
    for (const auto width : { 1240, expectedEditorMinimumWidth })
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(width, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        widthOk &= expect(editor.getHeight() == expectedEditorSccHeight,
                          "SCC should use its dedicated compact editor height");
        const auto voiceDeck = editor.getModuleBoundsForLayoutTest(1);
        const auto topologySummary = editor.getModuleSummaryBoundsForLayoutTest(1);
        const auto performance = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! voiceDeck.isEmpty() && voiceDeck.getHeight() >= 340,
                          "SCC unified wave bank should reserve two readable card rows");
        widthOk &= expect(editor.isModuleSummaryVisibleForLayoutTest(1)
                              && ! topologySummary.isEmpty()
                              && voiceDeck.expanded(2).contains(topologySummary)
                              && editor.getModuleSummaryTextForLayoutTest(1).containsIgnoreCase("enhanced")
                              && editor.getModuleSummaryTextForLayoutTest(1).containsIgnoreCase("sharing is not modeled"),
                          "SCC wave bank should disclose the enhanced five-wave topology and unmodeled original sharing");
        widthOk &= expect(performance.getHeight() >= 180,
                          "SCC shared wave-stack/output strip should expose two readable rows");
        for (const auto retiredModule : { 0u, 2u, 3u, 4u, 5u })
            widthOk &= expect(editor.getModuleBoundsForLayoutTest(retiredModule).isEmpty(),
                              "SCC should not retain detached generic modules");

        std::array<juce::Rectangle<int>, 5> cards {};
        for (size_t channel = 0; channel < cards.size(); ++channel)
        {
            cards[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto wave = editor.getSourceWaveSelectorBoundsForLayoutTest(channel);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(channel);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(channel);
            widthOk &= expect(cards[channel].getHeight() >= 136 && cards[channel].getHeight() <= 144,
                              "SCC channel cards should keep the dedicated readable height");
            widthOk &= expect(! cards[channel].intersects(topologySummary),
                              "SCC topology disclosure should not be covered by channel cards");
            widthOk &= expect(! wave.isEmpty() && wave.getHeight() >= 28 && cards[channel].expanded(2).contains(wave),
                              "SCC per-channel wave selector should be owned by its card");
            widthOk &= expect(! level.isEmpty() && level.getHeight() >= 16 && cards[channel].expanded(2).contains(level),
                              "SCC per-channel 4-bit level should be owned by its card");
            widthOk &= expect(header.startsWith("Ch " + juce::String(static_cast<int>(channel + 1u)) + " | 32-byte Wave RAM | V")
                                  && header.endsWith("/15"),
                              "SCC header should identify channel, Wave RAM depth, and resolved 4-bit volume");
            widthOk &= expect(editor.getSourceWaveSelectorItemTextForLayoutTest(channel, 4) == "Steps",
                              "SCC channel selectors should keep the Steps Wave RAM template");
        }
        for (size_t left = 0; left < cards.size(); ++left)
            for (size_t right = left + 1u; right < cards.size(); ++right)
                widthOk &= expect(! cards[left].intersects(cards[right]),
                                  "SCC channel cards should not overlap");
        widthOk &= expect(std::abs((cards[3].getX() + cards[4].getRight()) / 2 - voiceDeck.getCentreX()) <= 2,
                          "SCC channels 4 and 5 should form an intentional centered second row");

        const std::array<juce::Rectangle<int>, 7> sharedControls {
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getNativeSliderBoundsForLayoutTest(1),
            editor.getNativeSliderBoundsForLayoutTest(2),
            editor.getNativeSliderBoundsForLayoutTest(3),
            editor.getEnvelopeDecayBoundsForLayoutTest(),
            editor.getStereoSpreadBoundsForLayoutTest(),
            editor.getOutputSliderBoundsForLayoutTest()
        };
        for (const auto& control : sharedControls)
            widthOk &= expect(! control.isEmpty()
                                  && control.getHeight() >= 16
                                  && performance.expanded(2).contains(control),
                              "SCC shared controls should be readable and owned by the shared strip");

        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(1)
                              && editor.getNativeLabelTextForLayoutTest(1).containsIgnoreCase("choose Zap / Jump")
                              && editor.getNativeValueLabelTextForLayoutTest(1).containsIgnoreCase("Zap / Jump only"),
                          "SCC Gesture Pitch should disclose its recipe-specific ownership");
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(2)
                              && editor.getNativeLabelTextForLayoutTest(2).containsIgnoreCase("select Pulse")
                              && editor.getNativeValueLabelTextForLayoutTest(2).containsIgnoreCase("no Pulse channels"),
                          "SCC Pulse Width should disclose when no channel uses the Pulse template");
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern width off"),
                          "SCC zero-width readout should identify centered mono as a modern-width state");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::macro, 7);
        widthOk &= setPlainParameter(processor, chipper::parameters::id::macroControl2, 1.0f);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 3);
        widthOk &= setPlainParameter(processor, chipper::parameters::id::macroControl3, 1.0f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(1)
                              && editor.getNativeLabelTextForLayoutTest(1) == "Gesture Pitch"
                              && editor.getNativeValueLabelTextForLayoutTest(1).contains("Ch 1 +12 st"),
                          "SCC Gesture Pitch should activate for the Sweep Zap recipe");
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(2)
                              && editor.getNativeLabelTextForLayoutTest(2) == "Pulse Width"
                              && editor.getNativeValueLabelTextForLayoutTest(2).contains("Pulse high 28/32"),
                          "SCC Pulse Width should activate and show the generated high-byte count for Pulse channels");

        widthOk &= setPlainParameter(processor, chipper::parameters::id::stereoSpread, 1.0f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern five-channel")
                              && editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("native SCC pan not modeled"),
                          "SCC full-width readout should distinguish modern spread from unmodeled hardware routing");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t channel = 0; channel < cards.size(); ++channel)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(channel).contains("Note " + juce::String(static_cast<int>(channel + 1u))),
                              "SCC Chip Poly headers should expose all five allocation channels");

        ok &= widthOk;
    }

    return ok;
}

bool checkSpc700UnifiedSamplerLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::spc700);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: SPC700 chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto voicesModule = editor.getModuleBoundsForLayoutTest(1);
        const auto generatedModule = editor.getModuleBoundsForLayoutTest(2);
        const auto shapingModule = editor.getModuleBoundsForLayoutTest(3);
        const auto sampleBank = editor.getSampleBankBoundsForLayoutTest();
        const auto performance = editor.getPerformanceBoundsForLayoutTest();

        widthOk &= expect(editor.getHeight() == expectedEditorSpc700Height,
                          "SPC700 editor lost the height required for its complete sample-voice path");
        widthOk &= expect(! voicesModule.isEmpty()
                              && ! generatedModule.isEmpty()
                              && ! shapingModule.isEmpty()
                              && ! sampleBank.isEmpty()
                              && ! performance.isEmpty(),
                          "SPC700 unified voice, source, shaping, bank, and output regions must remain visible");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty(),
                          "SPC700 must not restore detached profile or motion destinations");

        const auto generatedShape = editor.getWaveShapeBoundsForLayoutTest();
        const auto noiseSource = editor.getSnNoiseModeBoundsForLayoutTest();
        if (! editor.isWaveShapeSegmentVisibleForLayoutTest()
            || ! editor.isSnNoiseModeSegmentVisibleForLayoutTest()
            || generatedShape.isEmpty()
            || noiseSource.isEmpty()
            || ! generatedModule.expanded(2).contains(generatedShape)
            || ! generatedModule.expanded(2).contains(noiseSource)
            || generatedShape.intersects(noiseSource)
            || generatedShape.getWidth() < 220
            || noiseSource.getWidth() < 220
            || generatedShape.getHeight() < 24
            || noiseSource.getHeight() < 24)
        {
            std::cerr << "editor_size_smoke: SPC700 generated shape and NON noise source must be visible, separate, and owned by Generated Source at width "
                      << editorWidth << ": shape " << generatedShape.toString()
                      << " noise " << noiseSource.toString()
                      << " module " << generatedModule.toString() << '\n';
            widthOk = false;
        }

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 3> shapingControls {{
            { editor.getYmEnvelopeShapeBoundsForLayoutTest(), "Envelope Shape" },
            { editor.getNativeSliderBoundsForLayoutTest(1), "Pitch / PMON" },
            { editor.getEnvelopeDecayBoundsForLayoutTest(), "ADSR / Gain Speed" }
        }};
        widthOk &= expect(editor.isYmEnvelopeShapeSegmentVisibleForLayoutTest(),
                          "SPC700 Envelope Shape choices must remain visible");
        for (size_t control = 0; control < shapingControls.size(); ++control)
        {
            const auto& [bounds, name] = shapingControls[control];
            if (bounds.isEmpty()
                || ! shapingModule.expanded(2).contains(bounds)
                || bounds.getWidth() < (control == 0u ? 220 : 88)
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: SPC700 " << name
                          << " is missing or escaped Voice Shaping at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " module " << shapingModule.toString() << '\n';
                widthOk = false;
            }
            for (size_t other = control + 1u; other < shapingControls.size(); ++other)
            {
                if (bounds.intersects(shapingControls[other].first))
                {
                    std::cerr << "editor_size_smoke: SPC700 shaping controls overlap at width "
                              << editorWidth << ": " << name << ' ' << bounds.toString()
                              << " and " << shapingControls[other].second << ' '
                              << shapingControls[other].first.toString() << '\n';
                    widthOk = false;
                }
            }
        }

        std::array<juce::Rectangle<int>, 8> voices {};
        for (size_t voice = 0; voice < voices.size(); ++voice)
        {
            voices[voice] = editor.getSourceChannelBoundsForLayoutTest(voice);
            const auto sampleSelector = editor.getSourceWaveSelectorBoundsForLayoutTest(voice);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(voice);
            if (voices[voice].isEmpty()
                || ! voicesModule.expanded(2).contains(voices[voice])
                || sampleSelector.isEmpty()
                || ! voices[voice].expanded(2).contains(sampleSelector)
                || sampleSelector.getHeight() < 28
                || sampleSelector.getWidth() < 180
                || level.isEmpty()
                || ! voices[voice].expanded(2).contains(level)
                || level.getWidth() < 180
                || level.getHeight() < 10)
            {
                std::cerr << "editor_size_smoke: SPC700 voice " << (voice + 1u)
                          << " lost its source-owned sample pin or level at width " << editorWidth
                          << ": voice " << voices[voice].toString()
                          << " sample " << sampleSelector.toString()
                          << " level " << level.toString() << '\n';
                widthOk = false;
            }
        }
        widthOk &= expect(voices[0].getY() == voices[1].getY()
                              && voices[0].getY() == voices[2].getY()
                              && voices[0].getY() == voices[3].getY()
                              && voices[4].getY() == voices[5].getY()
                              && voices[4].getY() == voices[6].getY()
                              && voices[4].getY() == voices[7].getY()
                              && voices[4].getY() > voices[0].getBottom(),
                          "SPC700 voices must remain a contained four-by-two voice matrix");
        widthOk &= expect(voices[4].getBottom() <= voicesModule.getBottom()
                              && voicesModule.getBottom() <= sampleBank.getY(),
                          "SPC700 second voice row must not overrun the sample bank");

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 7> bankControls {{
            { editor.getSampleFileButtonBoundsForLayoutTest(), "File" },
            { editor.getSampleFolderButtonBoundsForLayoutTest(), "Folder" },
            { editor.getSampleBankButtonBoundsForLayoutTest(), "Bank" },
            { editor.getSamplePlaybackModeBoundsForLayoutTest(), "Playback" },
            { editor.getSampleSlotBoundsForLayoutTest(), "Manual Slot" },
            { editor.getSampleRootBoundsForLayoutTest(), "Map Root" },
            { editor.getSampleLoopToggleBoundsForLayoutTest(), "Loop While Held" }
        }};
        for (const auto& [bounds, name] : bankControls)
        {
            if (bounds.isEmpty()
                || ! sampleBank.expanded(2).contains(bounds)
                || bounds.getHeight() < 24
                || bounds.getWidth() < 64)
            {
                std::cerr << "editor_size_smoke: SPC700 sample-bank control " << name
                          << " is missing, cramped, or not bank-owned at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " bank " << sampleBank.toString() << '\n';
                widthOk = false;
            }
        }

        const auto waveform = editor.getSampleWaveformBoundsForLayoutTest();
        const auto loopStart = editor.getSampleLoopStartBoundsForLayoutTest();
        const auto loopEnd = editor.getSampleLoopEndBoundsForLayoutTest();
        if (waveform.isEmpty()
            || loopStart.isEmpty()
            || loopEnd.isEmpty()
            || ! sampleBank.expanded(2).contains(waveform)
            || ! sampleBank.expanded(2).contains(loopStart)
            || ! sampleBank.expanded(2).contains(loopEnd)
            || waveform.getWidth() < 560
            || waveform.getHeight() < 108
            || loopStart.getWidth() < 240
            || loopEnd.getWidth() < 240
            || loopStart.getY() <= waveform.getBottom()
            || loopEnd.getY() <= waveform.getBottom()
            || loopStart.intersects(loopEnd))
        {
            std::cerr << "editor_size_smoke: SPC700 waveform and loop range must remain readable and bank-owned at width "
                      << editorWidth << ": waveform " << waveform.toString()
                      << " start " << loopStart.toString()
                      << " end " << loopEnd.toString()
                      << " bank " << sampleBank.toString() << '\n';
            widthOk = false;
        }

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 5> performanceControls {{
            { editor.getNativeSliderBoundsForLayoutTest(0), "Voice Spread" },
            { editor.getNativeSliderBoundsForLayoutTest(2), "Echo Color" },
            { editor.getNativeSliderBoundsForLayoutTest(3), "Voice Volume" },
            { editor.getStereoSpreadBoundsForLayoutTest(), "Stereo Spread" },
            { editor.getOutputSliderBoundsForLayoutTest(), "Output" }
        }};
        for (size_t control = 0; control < performanceControls.size(); ++control)
        {
            const auto& [bounds, name] = performanceControls[control];
            if (bounds.isEmpty()
                || ! performance.expanded(2).contains(bounds)
                || bounds.getWidth() < 88
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: SPC700 shared " << name
                          << " escaped Voice Mix + Echo + Output at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " performance " << performance.toString() << '\n';
                widthOk = false;
            }
            for (size_t other = control + 1u; other < performanceControls.size(); ++other)
            {
                if (bounds.intersects(performanceControls[other].first))
                {
                    std::cerr << "editor_size_smoke: SPC700 performance controls overlap at width "
                              << editorWidth << ": " << name << ' ' << bounds.toString()
                              << " and " << performanceControls[other].second << ' '
                              << performanceControls[other].first.toString() << '\n';
                    widthOk = false;
                }
            }
        }

        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);
    return ok;
}

bool checkPaulaUnifiedTrackerLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::paula);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: Paula chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto channelsModule = editor.getModuleBoundsForLayoutTest(1);
        const auto sampleBank = editor.getSampleBankBoundsForLayoutTest();
        const auto performance = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(editor.getHeight() == expectedEditorPaulaHeight,
                          "Paula editor lost the height required for its complete tracker path");
        widthOk &= expect(! channelsModule.isEmpty() && ! sampleBank.isEmpty() && ! performance.isEmpty(),
                          "Paula DMA channels, sample memory, and shared playback/output must remain visible");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(2).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(3).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty(),
                          "Paula must not restore detached profile, sample, envelope, or motion destinations");

        std::array<juce::Rectangle<int>, 4> channels {};
        for (size_t channel = 0; channel < channels.size(); ++channel)
        {
            channels[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto shape = editor.getSourceWaveSelectorBoundsForLayoutTest(channel);
            const auto sample = editor.getPaulaSourceSampleSelectorBoundsForLayoutTest(channel);
            const auto level = editor.getSourceLevelBoundsForLayoutTest(channel);
            const auto header = editor.getSourceChannelButtonTextForLayoutTest(channel);
            const auto expectedPan = channel == 0u || channel == 3u ? "L" : "R";
            if (channels[channel].isEmpty()
                || ! channelsModule.expanded(2).contains(channels[channel])
                || shape.isEmpty()
                || sample.isEmpty()
                || level.isEmpty()
                || ! channels[channel].expanded(2).contains(shape)
                || ! channels[channel].expanded(2).contains(sample)
                || ! channels[channel].expanded(2).contains(level)
                || shape.getHeight() < 28
                || sample.getHeight() < 28
                || level.getHeight() < 16
                || shape.getWidth() < 360
                || sample.getWidth() < 360
                || level.getWidth() < 360
                || sample.getY() <= shape.getBottom()
                || level.getY() <= sample.getBottom()
                || level.getY() - sample.getBottom() > 16
                || ! header.contains("Ch " + juce::String(static_cast<int>(channel + 1u)))
                || ! header.contains(expectedPan)
                || ! header.contains("V")
                || ! header.containsIgnoreCase("Loop"))
            {
                std::cerr << "editor_size_smoke: Paula channel " << (channel + 1u)
                          << " lost its pan, sample-source, volume, or lifetime path at width " << editorWidth
                          << ": channel " << channels[channel].toString()
                          << " shape " << shape.toString()
                          << " sample " << sample.toString()
                          << " level " << level.toString()
                          << " header " << header << '\n';
                widthOk = false;
            }
        }
        widthOk &= expect(channels[0].getY() == channels[1].getY()
                              && channels[2].getY() == channels[3].getY()
                              && channels[2].getY() > channels[0].getBottom()
                              && channels[2].getBottom() <= channelsModule.getBottom(),
                          "Paula channels must remain a contained two-by-two L/R/R/L matrix");

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 6> bankControls {{
            { editor.getSampleFileButtonBoundsForLayoutTest(), "File" },
            { editor.getSampleFolderButtonBoundsForLayoutTest(), "Folder" },
            { editor.getSampleBankButtonBoundsForLayoutTest(), "Bank" },
            { editor.getSamplePlaybackModeBoundsForLayoutTest(), "Playback" },
            { editor.getSampleSlotBoundsForLayoutTest(), "Manual Slot" },
            { editor.getSampleRootBoundsForLayoutTest(), "Map Root" }
        }};
        for (const auto& [bounds, name] : bankControls)
        {
            if (bounds.isEmpty()
                || ! sampleBank.expanded(2).contains(bounds)
                || bounds.getHeight() < 28
                || bounds.getWidth() < 72)
            {
                std::cerr << "editor_size_smoke: Paula sample-memory control " << name
                          << " is missing, cramped, or escaped its bank at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " bank " << sampleBank.toString() << '\n';
                widthOk = false;
            }
        }
        const auto waveform = editor.getSampleWaveformBoundsForLayoutTest();
        widthOk &= expect(! waveform.isEmpty()
                              && waveform.getWidth() >= 560
                              && waveform.getHeight() >= 108
                              && sampleBank.expanded(2).contains(waveform),
                          "Paula sample waveform must remain a useful, bank-owned editor");

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 9> sharedControls {{
            { editor.getNativeSliderBoundsForLayoutTest(0), "Channel Spread" },
            { editor.getNativeSliderBoundsForLayoutTest(1), "Period Motion" },
            { editor.getNativeSliderBoundsForLayoutTest(2), "Loop Tendency" },
            { editor.getDmgStereoRouteBoundsForLayoutTest(), "Loop Mode" },
            { editor.getNativeSliderBoundsForLayoutTest(3), "Channel Volume" },
            { editor.getEnvelopeDecayBoundsForLayoutTest(), "Tracker Amp Env" },
            { editor.getSnNoiseModeBoundsForLayoutTest(), "Output Filter" },
            { editor.getStereoSpreadBoundsForLayoutTest(), "Stereo Spread" },
            { editor.getOutputSliderBoundsForLayoutTest(), "Output" }
        }};
        widthOk &= expect(editor.isDmgStereoRouteSegmentVisibleForLayoutTest()
                              && editor.getDmgStereoRouteLabelTextForLayoutTest() == "Loop Mode"
                              && editor.isSnNoiseModeSegmentVisibleForLayoutTest(),
                          "Paula Loop Mode and Output Filter choices must remain visibly labeled");
        for (size_t control = 0; control < sharedControls.size(); ++control)
        {
            const auto& [bounds, name] = sharedControls[control];
            if (bounds.isEmpty()
                || ! performance.expanded(2).contains(bounds)
                || bounds.getWidth() < 88
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: Paula shared " << name
                          << " escaped Tracker Playback + Paula Output at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " performance " << performance.toString() << '\n';
                widthOk = false;
            }
            for (size_t other = control + 1u; other < sharedControls.size(); ++other)
            {
                if (bounds.intersects(sharedControls[other].first))
                {
                    std::cerr << "editor_size_smoke: Paula shared controls overlap at width "
                              << editorWidth << ": " << name << ' ' << bounds.toString()
                              << " and " << sharedControls[other].second << ' '
                              << sharedControls[other].first.toString() << '\n';
                    widthOk = false;
                }
            }
        }

        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("modern mono"),
                          "Paula zero Stereo Spread must be disclosed as a modern centered collapse");
        widthOk &= setPlainParameter(processor, chipper::parameters::id::stereoSpread, 1.0f);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getStereoSpreadValueTextForLayoutTest().containsIgnoreCase("authentic L/R/R/L"),
                          "Paula full Stereo Spread must identify the authentic hardware pan layout");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        editor.runEditorUpdateForLayoutTest();
        for (size_t channel = 0; channel < channels.size(); ++channel)
            widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(channel).contains("Note " + juce::String(static_cast<int>(channel + 1u))),
                              "Paula Chip Poly must identify each independent note lane in its channel header");

        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);
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
            case chipper::ChipMode::sn76489:
                expectedMacroSliders = {};
                break;
            case chipper::ChipMode::sid:
                // Voice 1 pulse width is owned by the Voice 1 card alongside
                // the independent Voice 2/3 pulse-width controls.
                expectedMacroSliders = { 1, 3 };
                break;
            case chipper::ChipMode::spc700:
            case chipper::ChipMode::paula:
                expectedMacroSliders = { 0, 2, 3 };
                break;
            case chipper::ChipMode::ym2149:
                expectedMacroSliders = { 0, 1 };
                break;
            case chipper::ChipMode::saa1099:
                // Noise Clock is owned by the dual shared-generator block.
                expectedMacroSliders = { 0, 1, 3 };
                break;
            case chipper::ChipMode::pokey:
                // Distortion Bias is owned by Shared AUDC Texture + Gate.
                expectedMacroSliders = { 0, 1, 3 };
                break;
            case chipper::ChipMode::pcSpeaker:
                // Every speaker-shaping control belongs to the one hardware path.
                expectedMacroSliders = {};
                break;
            case chipper::ChipMode::zxSpectrumBeeper:
                // Every EAR/MIC beeper control belongs to the one ULA path.
                expectedMacroSliders = {};
                break;
            case chipper::ChipMode::ym2612:
                // The complete shared four-operator patch lives in its owning
                // module; the compact footer is clock/output only.
                expectedMacroSliders = {};
                break;
            case chipper::ChipMode::opl3:
                // Topology and the complete shared OPL patch live together;
                // the compact footer is clock/output only.
                expectedMacroSliders = {};
                break;
            case chipper::ChipMode::ym2151:
                // Eight channels, the complete shared OPM patch, channel-8
                // noise, and LFO/pan routing all live above the compact footer.
                expectedMacroSliders = {};
                break;
            case chipper::ChipMode::ym2203:
                // Algorithm Bias and Feedback belong to Shared FM Patch;
                // only controls that bridge FM and SSG remain in the footer.
                expectedMacroSliders = { 2, 3 };
                break;
            case chipper::ChipMode::ym2608:
                // Algorithm Bias and Feedback belong to Shared FM Patch;
                // FM/SSG tone and level bridge the two engines in the footer.
                expectedMacroSliders = { 2, 3 };
                break;
            case chipper::ChipMode::ym2610:
                // Algorithm Bias and Feedback belong to Shared FM Patch;
                // FM/SSG tone and level bridge the two engines in the footer.
                expectedMacroSliders = { 2, 3 };
                break;
            case chipper::ChipMode::ym2610b:
                // Algorithm Bias and Feedback belong to Shared FM Patch;
                // FM/SSG tone and level bridge the two engines in the footer.
                expectedMacroSliders = { 2, 3 };
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

            if (compactMacroCell && mode != chipper::ChipMode::sn76489 && ! valueBounds.isEmpty())
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
            const auto feedbackOwnerBounds = mode == chipper::ChipMode::ym2612 || mode == chipper::ChipMode::ym2151 || mode == chipper::ChipMode::ym2203 || mode == chipper::ChipMode::ym2608 || mode == chipper::ChipMode::ym2610 || mode == chipper::ChipMode::ym2610b
                ? editor.getModuleBoundsForLayoutTest(2)
                : performanceBounds;
            if (feedbackBounds.isEmpty()
                || ! feedbackOwnerBounds.expanded(2).contains(feedbackBounds)
                || feedbackBounds.getWidth() < 96
                || feedbackBounds.getHeight() < 20)
            {
                std::cerr << "editor_size_smoke: FM feedback menu is not readable/owned by its patch surface for mode "
                          << chipper::parameters::chipModeChoices()[chipMode]
                          << ": feedback " << feedbackBounds.toString()
                          << " owner " << feedbackOwnerBounds.toString() << '\n';
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

bool checkSaa1099GroupedLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::saa1099);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: SAA1099 chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto channelsModule = editor.getModuleBoundsForLayoutTest(1);
        const auto generatorsModule = editor.getModuleBoundsForLayoutTest(2);
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();

        widthOk &= expect(editor.getHeight() == expectedEditorSaa1099Height,
                          "SAA1099 editor lost its compact chip-specific height");
        widthOk &= expect(! channelsModule.isEmpty() && ! generatorsModule.isEmpty(),
                          "SAA1099 channel and shared-generator modules must both remain visible");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(3).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(5).isEmpty(),
                          "SAA1099 must not restore detached profile, envelope, motion, or output panels");

        std::array<juce::Rectangle<int>, 6> channelBounds {};
        for (size_t channel = 0; channel < channelBounds.size(); ++channel)
        {
            channelBounds[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto levelBounds = editor.getSourceLevelBoundsForLayoutTest(channel);
            if (channelBounds[channel].isEmpty()
                || ! channelsModule.expanded(2).contains(channelBounds[channel])
                || levelBounds.isEmpty()
                || ! channelBounds[channel].expanded(2).contains(levelBounds)
                || levelBounds.getWidth() < 96
                || levelBounds.getHeight() < 10)
            {
                std::cerr << "editor_size_smoke: SAA1099 channel " << (channel + 1u)
                          << " lost its readable, source-owned level lane at width " << editorWidth
                          << ": channel " << channelBounds[channel].toString()
                          << " level " << levelBounds.toString()
                          << " module " << channelsModule.toString() << '\n';
                widthOk = false;
            }
        }

        const auto topRowY = channelBounds[0].getY();
        const auto bottomRowY = channelBounds[3].getY();
        widthOk &= expect(channelBounds[1].getY() == topRowY && channelBounds[2].getY() == topRowY,
                          "SAA1099 channels 1-3 must remain one visible generator group");
        widthOk &= expect(channelBounds[4].getY() == bottomRowY && channelBounds[5].getY() == bottomRowY,
                          "SAA1099 channels 4-6 must remain one visible generator group");
        widthOk &= expect(bottomRowY > channelBounds[0].getBottom(),
                          "SAA1099 generator groups must remain spatially separated");

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 4> generatorControls {{
            { editor.getSnNoiseModeBoundsForLayoutTest(), "Noise Mode" },
            { editor.getNativeSliderBoundsForLayoutTest(2), "Noise Clock" },
            { editor.getYmEnvelopeShapeBoundsForLayoutTest(), "Envelope Shape" },
            { editor.getEnvelopeDecayBoundsForLayoutTest(), "Envelope Speed" }
        }};
        for (size_t control = 0; control < generatorControls.size(); ++control)
        {
            const auto& [bounds, name] = generatorControls[control];
            if (bounds.isEmpty()
                || ! generatorsModule.expanded(2).contains(bounds)
                || bounds.getWidth() < 96
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: SAA1099 " << name
                          << " is missing or escaped Shared Generators at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " module " << generatorsModule.toString() << '\n';
                widthOk = false;
            }

            for (size_t other = control + 1u; other < generatorControls.size(); ++other)
            {
                if (bounds.intersects(generatorControls[other].first))
                {
                    std::cerr << "editor_size_smoke: SAA1099 shared controls overlap at width "
                              << editorWidth << ": " << name << ' ' << bounds.toString()
                              << " and " << generatorControls[other].second << ' '
                              << generatorControls[other].first.toString() << '\n';
                    widthOk = false;
                }
            }
        }

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 6> performanceControls {{
            { editor.getNativeSliderBoundsForLayoutTest(0), "Channel Spread" },
            { editor.getNativeSliderBoundsForLayoutTest(1), "Pitch Motion" },
            { editor.getNativeSliderBoundsForLayoutTest(3), "Channel Level" },
            { editor.getStereoSpreadBoundsForLayoutTest(), "Stereo Spread" },
            { editor.getClockSliderBoundsForLayoutTest(), "Clock" },
            { editor.getOutputSliderBoundsForLayoutTest(), "Output" }
        }};
        for (const auto& [bounds, name] : performanceControls)
        {
            if (bounds.isEmpty()
                || ! performanceBounds.expanded(2).contains(bounds)
                || bounds.getWidth() < 72
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: SAA1099 " << name
                          << " is missing or escaped Performance + Output at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                widthOk = false;
            }
        }

        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);
    return ok;
}

bool checkPokeyRelationshipLayout()
{
    const auto chipChoice = chipModeChoiceFor(chipper::ChipMode::pokey);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: POKEY chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto channelsModule = editor.getModuleBoundsForLayoutTest(1);
        const auto relationshipsModule = editor.getModuleBoundsForLayoutTest(2);
        const auto textureModule = editor.getModuleBoundsForLayoutTest(3);
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(! channelsModule.isEmpty() && ! relationshipsModule.isEmpty() && ! textureModule.isEmpty(),
                          "POKEY channel, AUDCTL relationship, and shared texture modules must remain visible");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(5).isEmpty(),
                          "POKEY must not restore detached profile, motion, or output panels");

        std::array<juce::Rectangle<int>, 4> channels {};
        for (size_t channel = 0; channel < channels.size(); ++channel)
        {
            channels[channel] = editor.getSourceChannelBoundsForLayoutTest(channel);
            const auto trim = editor.getSourceLevelBoundsForLayoutTest(channel);
            if (channels[channel].isEmpty()
                || ! channelsModule.expanded(2).contains(channels[channel])
                || trim.isEmpty()
                || ! channels[channel].expanded(2).contains(trim)
                || trim.getWidth() < 96
                || trim.getHeight() < 10)
            {
                std::cerr << "editor_size_smoke: POKEY channel " << (channel + 1u)
                          << " lost its source-owned post-AUDV trim at width " << editorWidth
                          << ": channel " << channels[channel].toString()
                          << " trim " << trim.toString() << '\n';
                widthOk = false;
            }
        }
        widthOk &= expect(channels[0].getY() == channels[1].getY()
                              && channels[2].getY() == channels[3].getY()
                              && channels[2].getY() > channels[0].getBottom(),
                          "POKEY channels must remain a 1+2 / 3+4 pair matrix");
        widthOk &= expect(channels[0].getX() == channels[2].getX()
                              && channels[1].getX() == channels[3].getX(),
                          "POKEY channels must preserve vertical 3-to-1 and 4-to-2 filter relationships");

        const auto pairingBounds = editor.getDmgStereoRouteBoundsForLayoutTest();
        const auto filterBounds = editor.getYmEnvelopeShapeBoundsForLayoutTest();
        if (pairingBounds.isEmpty()
            || filterBounds.isEmpty()
            || ! relationshipsModule.expanded(2).contains(pairingBounds)
            || ! relationshipsModule.expanded(2).contains(filterBounds)
            || pairingBounds.intersects(filterBounds)
            || pairingBounds.getWidth() < 240
            || filterBounds.getWidth() < 240)
        {
            std::cerr << "editor_size_smoke: POKEY AUDCTL Pairing and Filter must be readable, separate, and owned by Relationships at width "
                      << editorWidth << ": pairing " << pairingBounds.toString()
                      << " filter " << filterBounds.toString()
                      << " module " << relationshipsModule.toString() << '\n';
            widthOk = false;
        }

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 3> textureControls {{
            { editor.getWaveShapeBoundsForLayoutTest(), "Distortion Code" },
            { editor.getNativeSliderBoundsForLayoutTest(2), "Distortion Bias" },
            { editor.getEnvelopeDecayBoundsForLayoutTest(), "AUDV Gate" }
        }};
        for (size_t control = 0; control < textureControls.size(); ++control)
        {
            const auto& [bounds, name] = textureControls[control];
            if (bounds.isEmpty()
                || ! textureModule.expanded(2).contains(bounds)
                || bounds.getWidth() < 96
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: POKEY " << name
                          << " is missing or escaped Shared AUDC Texture + Gate at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " module " << textureModule.toString() << '\n';
                widthOk = false;
            }
            for (size_t other = control + 1u; other < textureControls.size(); ++other)
            {
                if (bounds.intersects(textureControls[other].first))
                {
                    std::cerr << "editor_size_smoke: POKEY shared texture controls overlap at width "
                              << editorWidth << ": " << name << ' ' << bounds.toString()
                              << " and " << textureControls[other].second << ' '
                              << textureControls[other].first.toString() << '\n';
                    widthOk = false;
                }
            }
        }

        const std::array<juce::Rectangle<int>, 6> performanceControls {{
            editor.getNativeSliderBoundsForLayoutTest(0),
            editor.getNativeSliderBoundsForLayoutTest(1),
            editor.getNativeSliderBoundsForLayoutTest(3),
            editor.getStereoSpreadBoundsForLayoutTest(),
            editor.getClockSliderBoundsForLayoutTest(),
            editor.getOutputSliderBoundsForLayoutTest()
        }};
        for (const auto& bounds : performanceControls)
        {
            if (bounds.isEmpty()
                || ! performanceBounds.expanded(2).contains(bounds)
                || bounds.getWidth() < 72
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: POKEY performance/output control escaped its compact strip at width "
                          << editorWidth << ": control " << bounds.toString()
                          << " performance " << performanceBounds.toString() << '\n';
                widthOk = false;
            }
        }

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 0);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.isNativeSliderEnabledForLayoutTest(2),
                          "POKEY Distortion Bias should be active while Distortion Code follows the preset");
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::waveShape, 2);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(! editor.isNativeSliderEnabledForLayoutTest(2),
                          "POKEY explicit Distortion Code should disable the preset-only Distortion Bias");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::playMode, 1);
        widthOk &= setChoiceParameter(processor, chipper::parameters::id::dmgStereoRoute, 1);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("8-bit note 1")
                              && editor.getSourceChannelButtonTextForLayoutTest(3).contains("8-bit note 4"),
                          "POKEY unpaired Chip Poly should expose four independent note lanes");

        widthOk &= setChoiceParameter(processor, chipper::parameters::id::dmgStereoRoute, 2);
        editor.runEditorUpdateForLayoutTest();
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(0).contains("16-bit note 1"),
                          "POKEY 1+2 pairing should present channel 1 as the 16-bit note lane");
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(1).contains("high byte")
                              && ! editor.isSourceChannelButtonEnabledForLayoutTest(1),
                          "POKEY 1+2 pairing should visibly consume and disable channel 2 as the high byte");
        widthOk &= expect(editor.getSourceChannelButtonTextForLayoutTest(2).contains("8-bit note 2")
                              && editor.getSourceChannelButtonTextForLayoutTest(3).contains("8-bit note 3"),
                          "POKEY 1+2 pairing should renumber the remaining Chip Poly lanes truthfully");

        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);
    return ok;
}

bool checkOneBitHardwarePathLayout(chipper::ChipMode mode,
                                   const char* chipName,
                                   const std::array<const char*, 3>& sourceLabelTokens,
                                   const std::array<const char*, 6>& controlNames)
{
    const auto chipChoice = chipModeChoiceFor(mode);
    if (chipChoice < 0)
    {
        std::cerr << "editor_size_smoke: " << chipName << " chip mode choice unavailable\n";
        return false;
    }

    auto checkAtWidth = [&](int editorWidth)
    {
        ChipperAudioProcessor processor;
        auto widthOk = setChoiceParameter(processor, chipper::parameters::id::chipMode, chipChoice);
        ChipperAudioProcessorEditor editor(processor);
        editor.setSize(editorWidth, expectedHeightForChipMode(chipChoice));
        editor.runEditorUpdateForLayoutTest();

        const auto sourceModule = editor.getModuleBoundsForLayoutTest(1);
        const auto pathModule = editor.getModuleBoundsForLayoutTest(2);
        const auto performanceBounds = editor.getPerformanceBoundsForLayoutTest();
        widthOk &= expect(editor.getHeight() == expectedHeightForChipMode(chipChoice),
                          "One-bit hardware path should retain its chip-specific compact instrument height");
        widthOk &= expect(! sourceModule.isEmpty() && ! pathModule.isEmpty(),
                          "One-bit source and complete hardware path must remain visible");
        widthOk &= expect(editor.getModuleBoundsForLayoutTest(0).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(3).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(4).isEmpty()
                              && editor.getModuleBoundsForLayoutTest(5).isEmpty(),
                          "One-bit chip must not restore detached profile, envelope, motion, or output panels");

        const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(0);
        const auto levelBounds = editor.getSourceLevelBoundsForLayoutTest(0);
        if (sourceBounds.isEmpty()
            || ! sourceModule.expanded(2).contains(sourceBounds)
            || levelBounds.isEmpty()
            || ! sourceBounds.expanded(2).contains(levelBounds)
            || levelBounds.getWidth() < 320
            || levelBounds.getHeight() < 10)
        {
            std::cerr << "editor_size_smoke: " << chipName << " lost its single source-owned level lane at width "
                      << editorWidth << ": source " << sourceBounds.toString()
                      << " level " << levelBounds.toString() << '\n';
            widthOk = false;
        }

        const auto sourceLabel = editor.getSourceChannelButtonTextForLayoutTest(0);
        auto sourceLabelOk = true;
        for (const auto* token : sourceLabelTokens)
            sourceLabelOk &= sourceLabel.containsIgnoreCase(token);
        widthOk &= expect(sourceLabelOk,
                          "One-bit source card must name the real hardware output path");

        const std::array<std::pair<juce::Rectangle<int>, const char*>, 6> pathControls {{
            { editor.getWaveShapeBoundsForLayoutTest(), controlNames[0] },
            { editor.getNativeSliderBoundsForLayoutTest(0), controlNames[1] },
            { editor.getNativeSliderBoundsForLayoutTest(1), controlNames[2] },
            { editor.getNativeSliderBoundsForLayoutTest(2), controlNames[3] },
            { editor.getNativeSliderBoundsForLayoutTest(3), controlNames[4] },
            { editor.getEnvelopeDecayBoundsForLayoutTest(), controlNames[5] }
        }};
        for (size_t control = 0; control < pathControls.size(); ++control)
        {
            const auto& [bounds, name] = pathControls[control];
            const auto minimumWidth = control == 0u ? 480 : 96;
            if (bounds.isEmpty()
                || ! pathModule.expanded(2).contains(bounds)
                || bounds.getWidth() < minimumWidth
                || bounds.getHeight() < 18)
            {
                std::cerr << "editor_size_smoke: " << chipName << ' ' << name
                          << " is missing or escaped the one hardware path at width " << editorWidth
                          << ": control " << bounds.toString()
                          << " module " << pathModule.toString() << '\n';
                widthOk = false;
            }
            for (size_t other = control + 1u; other < pathControls.size(); ++other)
            {
                if (bounds.intersects(pathControls[other].first))
                {
                    std::cerr << "editor_size_smoke: " << chipName << " path controls overlap at width "
                              << editorWidth << ": " << name << ' ' << bounds.toString()
                              << " and " << pathControls[other].second << ' '
                              << pathControls[other].first.toString() << '\n';
                    widthOk = false;
                }
            }
        }

        const auto clockBounds = editor.getClockSliderBoundsForLayoutTest();
        const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();
        if (performanceBounds.isEmpty()
            || clockBounds.isEmpty()
            || outputBounds.isEmpty()
            || ! performanceBounds.expanded(2).contains(clockBounds)
            || ! performanceBounds.expanded(2).contains(outputBounds)
            || clockBounds.intersects(outputBounds)
            || clockBounds.getWidth() < 240
            || outputBounds.getWidth() < 240)
        {
            std::cerr << "editor_size_smoke: " << chipName << " Clock + Output strip is missing, cramped, or overlapping at width "
                      << editorWidth << ": clock " << clockBounds.toString()
                      << " output " << outputBounds.toString()
                      << " performance " << performanceBounds.toString() << '\n';
            widthOk = false;
        }

        return widthOk;
    };

    auto ok = checkAtWidth(1240);
    ok &= checkAtWidth(expectedEditorMinimumWidth);
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
        const auto voicesModuleBounds = editor.getModuleBoundsForLayoutTest(1);
        const auto filterModuleBounds = editor.getModuleBoundsForLayoutTest(2);
        const auto profileModuleBounds = editor.getModuleBoundsForLayoutTest(0);
        const auto motionModuleBounds = editor.getModuleBoundsForLayoutTest(4);
        const auto interactionBounds = editor.getSnNoiseModeBoundsForLayoutTest();
        const auto modelBounds = editor.getDmgStereoRouteBoundsForLayoutTest();
        const auto filterModeBounds = editor.getYmEnvelopeShapeBoundsForLayoutTest();
        const auto filterRoutingBounds = editor.getSidFilterRoutingBoundsForLayoutTest();
        const auto cutoffBounds = editor.getNativeSliderBoundsForLayoutTest(2);
        const auto resonanceBounds = editor.getStereoSpreadBoundsForLayoutTest();
        const auto duplicatePulseWidthBounds = editor.getNativeSliderBoundsForLayoutTest(0);
        const auto detuneBounds = editor.getNativeSliderBoundsForLayoutTest(1);
        const auto sustainBounds = editor.getNativeSliderBoundsForLayoutTest(3);
        const auto clockBounds = editor.getClockSliderBoundsForLayoutTest();
        const auto outputBounds = editor.getOutputSliderBoundsForLayoutTest();

        if (! profileModuleBounds.isEmpty() || ! motionModuleBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: SID should not reserve single-control Profile/Motion destinations at width "
                      << editorWidth << ": profile " << profileModuleBounds.toString()
                      << " motion " << motionModuleBounds.toString() << '\n';
            widthOk = false;
        }

        for (size_t voice = 0; voice < 3; ++voice)
        {
            const auto sourceBounds = editor.getSourceChannelBoundsForLayoutTest(voice);
            const auto waveBounds = editor.getSidVoiceWaveBoundsForLayoutTest(voice);
            const auto pulseWidthBounds = editor.getSidVoicePulseWidthBoundsForLayoutTest(voice);
            if (sourceBounds.isEmpty()
                || ! voicesModuleBounds.expanded(2).contains(sourceBounds)
                || ! sourceBounds.expanded(2).contains(waveBounds)
                || ! sourceBounds.expanded(2).contains(pulseWidthBounds))
            {
                std::cerr << "editor_size_smoke: SID voice " << (voice + 1u)
                          << " lost owned waveform/pulse-width controls at width " << editorWidth
                          << ": source " << sourceBounds.toString() << " wave " << waveBounds.toString()
                          << " pulse width " << pulseWidthBounds.toString() << '\n';
                widthOk = false;
            }
        }

        if (interactionBounds.isEmpty() || ! voicesModuleBounds.expanded(2).contains(interactionBounds))
        {
            std::cerr << "editor_size_smoke: SID oscillator interaction is not owned by the voice block at width "
                      << editorWidth << ": interaction " << interactionBounds.toString()
                      << " voices " << voicesModuleBounds.toString() << '\n';
            widthOk = false;
        }

        for (const auto control : { cutoffBounds, resonanceBounds, modelBounds, filterModeBounds, filterRoutingBounds })
        {
            if (control.isEmpty() || ! filterModuleBounds.expanded(2).contains(control))
            {
                std::cerr << "editor_size_smoke: SID filter/model block is incomplete at width "
                          << editorWidth << ": control " << control.toString()
                          << " filter " << filterModuleBounds.toString() << '\n';
                widthOk = false;
            }
        }

        if (! duplicatePulseWidthBounds.isEmpty())
        {
            std::cerr << "editor_size_smoke: SID Voice 1 pulse width is still duplicated in the footer at width "
                      << editorWidth << ": " << duplicatePulseWidthBounds.toString() << '\n';
            widthOk = false;
        }

        for (const auto control : { detuneBounds, sustainBounds, clockBounds, outputBounds })
        {
            if (control.isEmpty() || ! performanceBounds.expanded(2).contains(control))
            {
                std::cerr << "editor_size_smoke: SID performance/output footer is incomplete at width "
                          << editorWidth << ": control " << control.toString()
                          << " footer " << performanceBounds.toString() << '\n';
                widthOk = false;
            }
        }

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
        const auto minimumPerformanceHeight = (mode == chipper::ChipMode::ym2612
                                                || mode == chipper::ChipMode::opl3
                                                || mode == chipper::ChipMode::ym2151
                                                || mode == chipper::ChipMode::ym2203
                                                || mode == chipper::ChipMode::ym2608
                                                || mode == chipper::ChipMode::ym2610
                                                || mode == chipper::ChipMode::ym2610b)
            ? 80
            : 100;
        if (editor.getPerformanceBoundsForLayoutTest().getHeight() < minimumPerformanceHeight
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
    ok &= checkOpl3UnifiedTopologyLayout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2612);
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2151);
    ok &= checkYm2151UnifiedOpmLayout();
    ok &= checkYm2413UnifiedOpllLayout();
    ok &= checkYm2203UnifiedOpnLayout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2203);
    ok &= checkYm2608UnifiedOpnaLayout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2608);
    ok &= checkYm2610UnifiedOpnbLayout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2610);
    ok &= checkYm2610bUnifiedOpnb2Layout();
    ok &= checkFourOperatorFmOperatorSurfaceLayout(chipper::ChipMode::ym2610b);
    ok &= checkHuc6280UnifiedLayout();
    ok &= checkNamcoWsgUnifiedLayout();
    ok &= checkSccUnifiedLayout();
    ok &= checkSamplerSourceDeck(chipper::ChipMode::spc700);
    ok &= checkSamplerSourceDeck(chipper::ChipMode::paula);
    ok &= checkSamplerBankLayout(chipper::ChipMode::spc700);
    ok &= checkSamplerBankLayout(chipper::ChipMode::paula);
    ok &= checkSpc700UnifiedSamplerLayout();
    ok &= checkPaulaUnifiedTrackerLayout();
    ok &= checkNesDmcAndPerformanceLayout();
    ok &= checkPerformanceMacroSliderLayout();
    ok &= checkSaa1099GroupedLayout();
    ok &= checkPokeyRelationshipLayout();
    ok &= checkOneBitHardwarePathLayout(chipper::ChipMode::pcSpeaker,
                                        "PC Speaker",
                                        { "PIT ch2", "0x61", "speaker" },
                                        { "Speaker Mode", "Pulse Width", "Pitch Motion", "Click Grit", "Speaker Level", "Gate Decay" });
    ok &= checkOneBitHardwarePathLayout(chipper::ChipMode::zxSpectrumBeeper,
                                        "ZX Spectrum Beeper",
                                        { "ULA $FE", "EAR bit 4", "MIC bit 3" },
                                        { "Beeper Mode", "Duty + Border", "Pitch Motion", "MIC Grit", "Beeper Level", "Gate Decay" });
    ok &= checkSidAdsrLayout();
    ok &= checkCompactChipLayouts();
    ok &= checkPresetRoleFilterLayout();
    ok &= checkGlobalPresetBrowserWorkflow();
    ok &= checkChipSwitchPreservesEditorSettings();
    ok &= checkUnifiedEditorContract();
    ok &= checkWorkflowTools();

    return ok ? 0 : 1;
}
