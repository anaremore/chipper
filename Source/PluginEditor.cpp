#include "PluginEditor.h"

#include "ChipperBuildInfo.h"
#include "Engine/ChipDescriptors.h"
#include "Presets.h"

#include <algorithm>
#include <cmath>

namespace
{

int chipModeChoiceIndex(chipper::ChipMode mode)
{
    switch (mode)
    {
        case chipper::ChipMode::nes: return 0;
        case chipper::ChipMode::dmg: return 1;
        case chipper::ChipMode::sid: return 2;
        case chipper::ChipMode::ym2149: return 3;
        case chipper::ChipMode::sn76489: return 4;
        case chipper::ChipMode::ym2612: return 5;
        case chipper::ChipMode::opl3: return 6;
        case chipper::ChipMode::spc700: return 7;
        case chipper::ChipMode::pokey: return 8;
        case chipper::ChipMode::paula: return 9;
        case chipper::ChipMode::huc6280: return 10;
        case chipper::ChipMode::namcoWsg: return 11;
        case chipper::ChipMode::ym2151: return 12;
        case chipper::ChipMode::ym2413: return 13;
        case chipper::ChipMode::scc: return 14;
        case chipper::ChipMode::arcade: return 15;
        case chipper::ChipMode::custom: return 16;
    }

    return 0;
}

int accuracyChoiceIndex(chipper::AccuracyMode mode)
{
    switch (mode)
    {
        case chipper::AccuracyMode::inspired: return 0;
        case chipper::AccuracyMode::hybrid: return 1;
        case chipper::AccuracyMode::authentic: return 2;
    }

    return 1;
}

int macroChoiceIndex(chipper::MacroKind macro)
{
    const auto order = chipper::macroOrder();
    const auto iter = std::find(order.begin(), order.end(), macro);
    return iter == order.end() ? 0 : static_cast<int>(std::distance(order.begin(), iter));
}

int playModeChoiceIndex(chipper::PlayMode mode)
{
    return mode == chipper::PlayMode::chipPoly ? 1 : 0;
}

chipper::ChipParameterRole macroControlRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 4> roles {
        chipper::ChipParameterRole::macroControl1,
        chipper::ChipParameterRole::macroControl2,
        chipper::ChipParameterRole::macroControl3,
        chipper::ChipParameterRole::macroControl4
    };

    return roles[std::min(index, roles.size() - 1u)];
}

chipper::ChipParameterRole sourceRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 4> roles {
        chipper::ChipParameterRole::source1Enabled,
        chipper::ChipParameterRole::source2Enabled,
        chipper::ChipParameterRole::source3Enabled,
        chipper::ChipParameterRole::source4Enabled
    };

    return roles[std::min(index, roles.size() - 1u)];
}

juce::String choiceTooltip(const chipper::ChipParameterSpec& spec, size_t choiceIndex)
{
    if (choiceIndex < spec.choices.size() && ! spec.choices[choiceIndex].help.empty())
        return juce::String(spec.choices[choiceIndex].help);

    return juce::String(spec.help);
}

template <typename ButtonArray>
void layoutSegmentedButtons(ButtonArray& buttons, juce::Rectangle<int> bounds, size_t visibleCount)
{
    const auto count = std::max<size_t>(1u, std::min(visibleCount, buttons.size()));
    const auto gap = 4;
    auto remainingWidth = std::max(1, bounds.getWidth() - (gap * static_cast<int>(count - 1u)));
    auto x = bounds.getX();

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        if (i >= count)
        {
            buttons[i].setBounds({});
            continue;
        }

        const auto remainingButtons = static_cast<int>(count - i);
        const auto width = std::max(1, remainingWidth / remainingButtons);
        buttons[i].setBounds({ x, bounds.getY(), width, bounds.getHeight() });
        x += width + gap;
        remainingWidth -= width;
    }
}

} // namespace

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor)
{
    setSize(1080, 800);

    auto& state = audioProcessor.getValueTreeState();

    titleLabel.setText("Chipper", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff7d85a));
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    coreReadinessLabel.setJustificationType(juce::Justification::centred);
    coreReadinessLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    coreReadinessLabel.setColour(juce::Label::textColourId, juce::Colour(0xff101414));
    coreReadinessLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff56c7d8));
    addAndMakeVisible(coreReadinessLabel);

    chipModeBox.addItemList(chipper::parameters::chipModeChoices(), 1);
    accuracyBox.addItemList(chipper::parameters::accuracyChoices(), 1);
    presetBox.setTextWhenNothingSelected("Factory Preset");
    macroBox.addItemList(chipper::parameters::macroChoices(), 1);
    playModeBox.addItemList(chipper::parameters::playModeChoices(), 1);
    chipModeBox.setTooltip("Selects the chip model or planned chip family.");
    accuracyBox.setTooltip("Selects the requested accuracy tier for the active core.");
    presetBox.setTooltip("Applies a factory preset for the selected chip mode.");
    macroBox.setTooltip("Applies a chip-specific musical register template.");
    playModeBox.setTooltip("Chooses how incoming notes use the chip channels inside one patch.");

    const std::array<const char*, 5> headerNames { "Preset", "Chip Mode", "Accuracy", "Macro", "Play Mode" };
    for (size_t i = 0; i < headerControlLabels.size(); ++i)
    {
        headerControlLabels[i].setText(headerNames[i], juce::dontSendNotification);
        headerControlLabels[i].setJustificationType(juce::Justification::centredLeft);
        headerControlLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        headerControlLabels[i].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(headerControlLabels[i]);
    }

    addAndMakeVisible(presetBox);
    addAndMakeVisible(chipModeBox);
    addAndMakeVisible(accuracyBox);
    addAndMakeVisible(macroBox);
    addAndMakeVisible(playModeBox);

    chipModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::chipMode, chipModeBox);
    accuracyAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::accuracy, accuracyBox);
    macroAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::macro, macroBox);
    playModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::playMode, playModeBox);
    macroBox.onChange = [this]()
    {
        applySelectedMacroTemplate();
    };
    presetBox.onChange = [this]()
    {
        applySelectedPreset();
    };

    addLabeledSlider(clockSlider, clockLabel, "Clock");
    clockSlider.setTextValueSuffix(" Hz");
    clockSlider.setSkewFactor(0.35);
    clockSlider.setTooltip("Optional chip clock override. Zero uses the documented default for the selected mode.");
    clockAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::clockHz, clockSlider);

    addLabeledSlider(outputSlider, outputLabel, "Output");
    outputSlider.setTextValueSuffix(" dB");
    outputSlider.setTooltip("Final plugin output level.");
    outputAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::outputDb, outputSlider);

    addLabeledSlider(envelopeDecaySlider, envelopeDecayLabel, "Envelope Decay");
    envelopeDecaySlider.setNumDecimalPlacesToDisplay(2);
    envelopeDecaySlider.setTooltip("Maps musical decay to the active chip's hardware envelope period. Zero preserves the macro's original envelope/level behavior.");
    envelopeDecayAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::envelopeDecay, envelopeDecaySlider);

    envelopeDecayValueLabel.setJustificationType(juce::Justification::centredLeft);
    envelopeDecayValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    envelopeDecayValueLabel.setFont(juce::FontOptions(11.0f));
    envelopeDecayValueLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(envelopeDecayValueLabel);

    const std::array<const char*, 4> ids {
        chipper::parameters::id::macroControl1,
        chipper::parameters::id::macroControl2,
        chipper::parameters::id::macroControl3,
        chipper::parameters::id::macroControl4
    };

    for (size_t i = 0; i < nativeSliders.size(); ++i)
    {
        nativeGroupLabels[i].setJustificationType(juce::Justification::centredLeft);
        nativeGroupLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
        nativeGroupLabels[i].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(nativeGroupLabels[i]);

        addLabeledSlider(nativeSliders[i], nativeLabels[i], "Native Control");
        nativeSliders[i].setNumDecimalPlacesToDisplay(2);
        nativeAttachments[i] = std::make_unique<SliderAttachment>(state, ids[i], nativeSliders[i]);
    }

    for (auto& valueLabel : controlValueLabels)
    {
        valueLabel.setJustificationType(juce::Justification::centredLeft);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        valueLabel.setFont(juce::FontOptions(11.0f));
        valueLabel.setMinimumHorizontalScale(0.75f);
        addAndMakeVisible(valueLabel);
    }

    const std::array<const char*, 4> dutyLabels { "12.5%", "25%", "50%", "75%" };
    for (size_t i = 0; i < pulseDutyButtons.size(); ++i)
    {
        auto& button = pulseDutyButtons[i];
        button.setButtonText(dutyLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("Pulse duty ") + dutyLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            const auto value = static_cast<float>(i) / static_cast<float>(pulseDutyButtons.size() - 1u);
            setParameterValueFromUi(chipper::parameters::id::macroControl1, value);
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    const std::array<const char*, toneNoiseMixCount> toneNoiseLabels { "Noise", "Tone", "Both" };
    for (size_t i = 0; i < toneNoiseMixButtons.size(); ++i)
    {
        auto& button = toneNoiseMixButtons[i];
        button.setButtonText(toneNoiseLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("YM2149 tone/noise mixer: ") + toneNoiseLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            const auto value = static_cast<float>(i) / static_cast<float>(toneNoiseMixButtons.size() - 1u);
            setParameterValueFromUi(chipper::parameters::id::macroControl4, value);
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    waveShapeLabel.setText("Wave Shape", juce::dontSendNotification);
    waveShapeLabel.setJustificationType(juce::Justification::centredLeft);
    waveShapeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    waveShapeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    waveShapeLabel.setVisible(false);
    addAndMakeVisible(waveShapeLabel);

    waveShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    waveShapeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    waveShapeValueLabel.setFont(juce::FontOptions(11.0f));
    waveShapeValueLabel.setMinimumHorizontalScale(0.75f);
    waveShapeValueLabel.setVisible(false);
    addAndMakeVisible(waveShapeValueLabel);

    const std::array<const char*, waveShapeCount> waveLabels { "RAM", "Tri", "Saw", "Pulse", "Steps" };
    for (size_t i = 0; i < waveShapeButtons.size(); ++i)
    {
        auto& button = waveShapeButtons[i];
        button.setButtonText(waveLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("DMG Wave RAM shape: ") + waveLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::waveShape, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    ymEnvelopeShapeLabel.setText("Envelope Shape", juce::dontSendNotification);
    ymEnvelopeShapeLabel.setJustificationType(juce::Justification::centredLeft);
    ymEnvelopeShapeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    ymEnvelopeShapeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    ymEnvelopeShapeLabel.setVisible(false);
    addAndMakeVisible(ymEnvelopeShapeLabel);

    ymEnvelopeShapeValueLabel.setJustificationType(juce::Justification::centredLeft);
    ymEnvelopeShapeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    ymEnvelopeShapeValueLabel.setFont(juce::FontOptions(11.0f));
    ymEnvelopeShapeValueLabel.setMinimumHorizontalScale(0.75f);
    ymEnvelopeShapeValueLabel.setVisible(false);
    addAndMakeVisible(ymEnvelopeShapeValueLabel);

    const std::array<const char*, ymEnvelopeShapeCount> ymEnvelopeLabels { "Fixed", "Fall", "Rise", "Saw", "Tri" };
    for (size_t i = 0; i < ymEnvelopeShapeButtons.size(); ++i)
    {
        auto& button = ymEnvelopeShapeButtons[i];
        button.setButtonText(ymEnvelopeLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("YM2149 envelope shape: ") + ymEnvelopeLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    snNoiseModeLabel.setText("Noise Mode", juce::dontSendNotification);
    snNoiseModeLabel.setJustificationType(juce::Justification::centredLeft);
    snNoiseModeLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    snNoiseModeLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    snNoiseModeLabel.setVisible(false);
    addAndMakeVisible(snNoiseModeLabel);

    snNoiseModeValueLabel.setJustificationType(juce::Justification::centredLeft);
    snNoiseModeValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    snNoiseModeValueLabel.setFont(juce::FontOptions(11.0f));
    snNoiseModeValueLabel.setMinimumHorizontalScale(0.75f);
    snNoiseModeValueLabel.setVisible(false);
    addAndMakeVisible(snNoiseModeValueLabel);

    const std::array<const char*, snNoiseModeCount> snNoiseLabels { "Macro", "P-Lo", "P-Hi", "W-Lo", "W-T3" };
    for (size_t i = 0; i < snNoiseModeButtons.size(); ++i)
    {
        auto& button = snNoiseModeButtons[i];
        button.setButtonText(snNoiseLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("SN76489 noise register mode: ") + snNoiseLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    statusLabel.setFont(juce::FontOptions(13.0f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f2a34));
    statusLabel.setText("Loading chip core...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    buildLabel.setFont(juce::FontOptions(11.0f));
    buildLabel.setJustificationType(juce::Justification::centredRight);
    buildLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7f9099));
    buildLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f2a34));
    buildLabel.setText(juce::String("Build ") + chipper::build::label, juce::dontSendNotification);
    buildLabel.setTooltip(juce::String("Built ") + chipper::build::builtAtUtc + " from " + chipper::build::gitState + " source");
    addAndMakeVisible(buildLabel);

    chipSummaryLabel.setFont(juce::FontOptions(14.0f));
    chipSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    chipSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    addAndMakeVisible(chipSummaryLabel);

    for (size_t i = 0; i < moduleTitleLabels.size(); ++i)
    {
        moduleNumberLabels[i].setText(juce::String(static_cast<int>(i + 1)), juce::dontSendNotification);
        moduleNumberLabels[i].setFont(juce::FontOptions(13.0f, juce::Font::bold));
        moduleNumberLabels[i].setJustificationType(juce::Justification::centred);
        moduleNumberLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xff101414));
        moduleNumberLabels[i].setColour(juce::Label::backgroundColourId, juce::Colour(0xfff0c94d));
        addAndMakeVisible(moduleNumberLabels[i]);

        moduleTitleLabels[i].setFont(juce::FontOptions(15.0f, juce::Font::bold));
        moduleTitleLabels[i].setJustificationType(juce::Justification::centredLeft);
        moduleTitleLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xfff0c94d));
        addAndMakeVisible(moduleTitleLabels[i]);

        moduleSummaryLabels[i].setFont(juce::FontOptions(12.0f));
        moduleSummaryLabels[i].setJustificationType(juce::Justification::centredLeft);
        moduleSummaryLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffc8d4dc));
        moduleSummaryLabels[i].setMinimumHorizontalScale(0.80f);
        addAndMakeVisible(moduleSummaryLabels[i]);

        for (auto& itemLabel : moduleItemLabels[i])
        {
            itemLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            itemLabel.setJustificationType(juce::Justification::centredLeft);
            itemLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdbe8e5));
            itemLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff243038));
            itemLabel.setMinimumHorizontalScale(0.75f);
            addAndMakeVisible(itemLabel);
        }
    }

    const std::array<const char*, sourceChannelCount> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled
    };

    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        auto& button = sourceChannelButtons[i];
        button.setClickingTogglesState(true);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff203a30));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff93a2aa));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffe6f2f0));
        button.setVisible(false);
        addAndMakeVisible(button);
        sourceEnableAttachments[i] = std::make_unique<ButtonAttachment>(state, sourceIds[i], button);
    }

    globalStripLabel.setText("Global Performance Controls", juce::dontSendNotification);
    globalStripLabel.setJustificationType(juce::Justification::centredLeft);
    globalStripLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0c94d));
    globalStripLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(globalStripLabel);

    macroSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    macroSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    macroSummaryLabel.setFont(juce::FontOptions(12.0f));
    macroSummaryLabel.setMinimumHorizontalScale(0.60f);
    addAndMakeVisible(macroSummaryLabel);

    updateDescriptorText();
    updateLiveControlReadouts();
    startTimerHz(8);
}

void ChipperAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101414));
    g.setColour(juce::Colour(0xff314047));
    g.drawHorizontalLine(84, 16.0f, static_cast<float>(getWidth() - 16));
    g.drawHorizontalLine(getHeight() - 62, 16.0f, static_cast<float>(getWidth() - 16));

    for (const auto& bounds : moduleBounds)
    {
        if (bounds.isEmpty())
            continue;

        const auto panel = bounds.toFloat();
        g.setColour(juce::Colour(0xff182125));
        g.fillRoundedRectangle(panel, 6.0f);
        g.setColour(juce::Colour(0xff34474c));
        g.drawRoundedRectangle(panel.reduced(0.5f), 6.0f, 1.0f);
    }

    if (usesSourceChannelSurface(displayedMode))
    {
        for (const auto& bounds : sourceChannelBounds)
        {
            if (bounds.isEmpty())
                continue;

            const auto card = bounds.toFloat();
            g.setColour(juce::Colour(0xff202c33));
            g.fillRoundedRectangle(card, 4.0f);
            g.setColour(juce::Colour(0xff56c7d8).withAlpha(0.70f));
            g.drawRoundedRectangle(card.reduced(0.5f), 4.0f, 1.0f);
        }
    }

    if (! globalStripBounds.isEmpty())
    {
        const auto strip = globalStripBounds.toFloat();
        g.setColour(juce::Colour(0xff171c20));
        g.fillRoundedRectangle(strip, 6.0f);
        g.setColour(juce::Colour(0xff344047));
        g.drawRoundedRectangle(strip.reduced(0.5f), 6.0f, 1.0f);
    }
}

void ChipperAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto top = area.removeFromTop(62);
    const auto placeHeaderCombo = [this](size_t index, juce::ComboBox& comboBox, juce::Rectangle<int> bounds)
    {
        headerControlLabels[index].setBounds(bounds.removeFromTop(16));
        comboBox.setBounds(bounds.reduced(0, 4));
    };

    titleLabel.setBounds(top.removeFromLeft(122));
    top.removeFromLeft(8);
    placeHeaderCombo(0, presetBox, top.removeFromLeft(210));
    top.removeFromLeft(8);
    placeHeaderCombo(1, chipModeBox, top.removeFromLeft(210));
    top.removeFromLeft(8);
    placeHeaderCombo(2, accuracyBox, top.removeFromLeft(112));
    top.removeFromLeft(8);
    placeHeaderCombo(3, macroBox, top.removeFromLeft(136));
    top.removeFromLeft(8);
    placeHeaderCombo(4, playModeBox, top.removeFromLeft(136));
    top.removeFromLeft(8);
    coreReadinessLabel.setBounds(top.removeFromTop(44).reduced(0, 12));

    area.removeFromTop(10);
    chipSummaryLabel.setBounds(area.removeFromTop(34));
    area.removeFromTop(10);

    auto modules = area.removeFromTop(390);
    const auto gap = 10;
    const auto columnWidth = (modules.getWidth() - gap) / 2;
    const auto rowHeight = (modules.getHeight() - (gap * 2)) / 3;

    for (size_t i = 0; i < moduleBounds.size(); ++i)
    {
        const auto row = static_cast<int>(i / 2);
        const auto column = static_cast<int>(i % 2);
        const auto x = modules.getX() + (column * (columnWidth + gap));
        const auto y = modules.getY() + (row * (rowHeight + gap));
        moduleBounds[i] = { x, y, columnWidth, rowHeight };

        auto panel = moduleBounds[i].reduced(12, 9);
        auto titleRow = panel.removeFromTop(20);
        moduleNumberLabels[i].setBounds(titleRow.removeFromLeft(22));
        titleRow.removeFromLeft(6);
        moduleTitleLabels[i].setBounds(titleRow);
        moduleSummaryLabels[i].setBounds(panel.removeFromTop(30));
        panel.removeFromTop(4);

        const auto itemGap = 6;
        const auto itemWidth = (panel.getWidth() - itemGap) / 2;
        const auto itemHeight = 20;
        for (size_t item = 0; item < moduleItemLabels[i].size(); ++item)
        {
            const auto itemRow = static_cast<int>(item / 2);
            const auto itemColumn = static_cast<int>(item % 2);
            moduleItemLabels[i][item].setBounds({
                panel.getX() + (itemColumn * (itemWidth + itemGap)),
                panel.getY() + (itemRow * (itemHeight + itemGap)),
                itemWidth,
                itemHeight
            });
        }
    }

    auto sourcePanel = moduleBounds[1].reduced(12, 9);
    sourcePanel.removeFromTop(20);
    sourcePanel.removeFromTop(30);
    sourcePanel.removeFromTop(4);
    const auto sourceGap = 6;
    const auto sourceCardWidth = (sourcePanel.getWidth() - sourceGap) / 2;
    const auto sourceCardHeight = (sourcePanel.getHeight() - sourceGap) / 2;
    for (size_t i = 0; i < sourceChannelBounds.size(); ++i)
    {
        const auto row = static_cast<int>(i / 2);
        const auto column = static_cast<int>(i % 2);
        sourceChannelBounds[i] = {
            sourcePanel.getX() + (column * (sourceCardWidth + sourceGap)),
            sourcePanel.getY() + (row * (sourceCardHeight + sourceGap)),
            sourceCardWidth,
            sourceCardHeight
        };
        sourceChannelButtons[i].setBounds(sourceChannelBounds[i].reduced(8, 1));
    }

    auto tonePanel = moduleBounds[2].reduced(12, 9);
    tonePanel.removeFromTop(20);
    tonePanel.removeFromTop(30);
    tonePanel.removeFromTop(4);
    placeWaveShapeSegment(tonePanel);
    placeSnNoiseModeSegment(tonePanel);

    auto envelopePanel = moduleBounds[3].reduced(12, 9);
    envelopePanel.removeFromTop(20);
    envelopePanel.removeFromTop(30);
    envelopePanel.removeFromTop(4);
    auto envelopeDecayPanel = envelopePanel;
    auto ymEnvelopeShapePanel = envelopePanel;
    envelopeDecayPanel.removeFromBottom(6);
    placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);
    placeYmEnvelopeShapeSegment(ymEnvelopeShapePanel);

    area.removeFromTop(12);
    globalStripBounds = area.removeFromTop(190);
    auto strip = globalStripBounds.reduced(12, 8);
    auto stripHeader = strip.removeFromTop(20);
    globalStripLabel.setBounds(stripHeader.removeFromLeft(160));
    stripHeader.removeFromLeft(10);
    macroSummaryLabel.setBounds(stripHeader);
    strip.removeFromTop(4);

    const auto controlGap = 10;
    const auto controlColumnWidth = (strip.getWidth() - (controlGap * 2)) / 3;
    const auto controlRowHeight = (strip.getHeight() - controlGap) / 2;

    std::array<juce::Rectangle<int>, 6> controlCells;
    for (size_t i = 0; i < controlCells.size(); ++i)
    {
        const auto row = static_cast<int>(i / 3);
        const auto column = static_cast<int>(i % 3);
        controlCells[i] = {
            strip.getX() + (column * (controlColumnWidth + controlGap)),
            strip.getY() + (row * (controlRowHeight + controlGap)),
            controlColumnWidth,
            controlRowHeight
        };
    }

    placeGroupedSlider(nativeSliders[0], nativeGroupLabels[0], nativeLabels[0], controlValueLabels[0], controlCells[0]);
    placePulseDutySegment(controlCells[0]);
    placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], controlCells[1]);
    placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[2]);
    placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], controlCells[3]);
    placeToneNoiseMixSegment(controlCells[3]);
    placeLabeledSliderWithReadout(clockSlider, clockLabel, controlValueLabels[4], controlCells[4]);
    placeLabeledSliderWithReadout(outputSlider, outputLabel, controlValueLabels[5], controlCells[5]);

    auto footer = getLocalBounds().reduced(16).removeFromBottom(44);
    buildLabel.setBounds(footer.removeFromRight(190));
    footer.removeFromRight(8);
    statusLabel.setBounds(footer);
}

void ChipperAudioProcessorEditor::timerCallback()
{
    updateDescriptorText();
    updateLiveControlReadouts();
    statusLabel.setText(audioProcessor.currentCoreStatus(), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::addLabeledSlider(juce::Slider& slider, juce::Label& label, const juce::String& fallbackText)
{
    label.setText(fallbackText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 24);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
    addAndMakeVisible(slider);
}

void ChipperAudioProcessorEditor::placeLabeledSlider(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(18));
    slider.setBounds(bounds.reduced(0, 2));
}

void ChipperAudioProcessorEditor::placeGroupedSlider(juce::Slider& slider,
                                                     juce::Label& groupLabel,
                                                     juce::Label& label,
                                                     juce::Label& valueLabel,
                                                     juce::Rectangle<int> bounds)
{
    groupLabel.setBounds(bounds.removeFromTop(13));
    label.setBounds(bounds.removeFromTop(17));
    slider.setBounds(bounds.removeFromTop(26).reduced(0, 1));
    valueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeLabeledSliderWithReadout(juce::Slider& slider,
                                                                juce::Label& label,
                                                                juce::Label& valueLabel,
                                                                juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(20));
    slider.setBounds(bounds.removeFromTop(30).reduced(0, 2));
    valueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placePulseDutySegment(juce::Rectangle<int> bounds)
{
    bounds.removeFromTop(30);
    pulseDutySegmentBounds = bounds.removeFromTop(26).reduced(0, 1);
    layoutSegmentedButtons(pulseDutyButtons, pulseDutySegmentBounds, pulseDutyButtons.size());
}

void ChipperAudioProcessorEditor::placeWaveShapeSegment(juce::Rectangle<int> bounds)
{
    waveShapeLabel.setBounds(bounds.removeFromTop(18));
    waveShapeSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(waveShapeButtons, waveShapeSegmentBounds, waveShapeButtons.size());

    waveShapeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeYmEnvelopeShapeSegment(juce::Rectangle<int> bounds)
{
    ymEnvelopeShapeLabel.setBounds(bounds.removeFromTop(18));
    ymEnvelopeShapeSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(ymEnvelopeShapeButtons, ymEnvelopeShapeSegmentBounds, ymEnvelopeShapeButtons.size());

    ymEnvelopeShapeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeSnNoiseModeSegment(juce::Rectangle<int> bounds)
{
    snNoiseModeLabel.setBounds(bounds.removeFromTop(18));
    snNoiseModeSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(snNoiseModeButtons, snNoiseModeSegmentBounds, snNoiseModeButtons.size());

    snNoiseModeValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeToneNoiseMixSegment(juce::Rectangle<int> bounds)
{
    bounds.removeFromTop(30);
    toneNoiseMixSegmentBounds = bounds.removeFromTop(26).reduced(0, 1);
    layoutSegmentedButtons(toneNoiseMixButtons, toneNoiseMixSegmentBounds, toneNoiseMixButtons.size());
}

float ChipperAudioProcessorEditor::parameterValue(const char* parameterId) const
{
    if (const auto* value = audioProcessor.getValueTreeState().getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}

void ChipperAudioProcessorEditor::setParameterValueFromUi(const char* parameterId, float plainValue)
{
    if (auto* parameter = audioProcessor.getValueTreeState().getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(std::clamp(plainValue, 0.0f, 1.0f));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void ChipperAudioProcessorEditor::setPlainParameterValueFromUi(const char* parameterId, float plainValue)
{
    if (auto* parameter = audioProcessor.getValueTreeState().getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(plainValue);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void ChipperAudioProcessorEditor::setChoiceParameterFromUi(const char* parameterId, int choiceIndex)
{
    if (auto* parameter = audioProcessor.getValueTreeState().getParameter(parameterId))
    {
        const auto normalized = parameter->convertTo0to1(static_cast<float>(choiceIndex));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        parameter->endChangeGesture();
    }
}

void ChipperAudioProcessorEditor::updateMacroChoices(chipper::ChipMode mode)
{
    const auto order = chipper::macroOrder();
    const auto selected = std::clamp(static_cast<int>(std::round(parameterValue(chipper::parameters::id::macro))),
                                     0,
                                     static_cast<int>(order.size() - 1u));

    const juce::ScopedValueSetter<bool> suppress(suppressMacroTemplateApply, true);
    macroBox.clear(juce::dontSendNotification);
    for (size_t i = 0; i < order.size(); ++i)
    {
        const auto& templ = chipper::macroTemplateFor(mode, order[i]);
        const auto label = templ.label.empty() ? juce::String(chipper::toString(order[i])) : juce::String(templ.label);
        macroBox.addItem(label, static_cast<int>(i + 1u));
    }

    macroBox.setSelectedItemIndex(selected, juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updatePresetChoices(chipper::ChipMode mode)
{
    const juce::ScopedValueSetter<bool> suppress(suppressPresetApply, true);
    displayedPresets = chipper::presetsForChip(mode);
    presetBox.clear(juce::dontSendNotification);

    for (size_t i = 0; i < displayedPresets.size(); ++i)
    {
        const auto& preset = *displayedPresets[i];
        presetBox.addItem(juce::String(preset.category) + " / " + juce::String(preset.name), static_cast<int>(i + 1u));
    }

    if (displayedPresets.empty())
    {
        presetBox.setText("No factory presets", juce::dontSendNotification);
        presetBox.setTooltip("No factory presets are active for this planned chip mode yet.");
    }
    else
    {
        presetBox.setSelectedId(0, juce::dontSendNotification);
        presetBox.setTextWhenNothingSelected("Factory Preset");
        presetBox.setTooltip("Applies a factory preset for the selected chip mode.");
    }
}

void ChipperAudioProcessorEditor::updateSegmentedControlSpecs(chipper::ChipMode mode)
{
    const auto applyChoices = [](auto& buttons, const chipper::ChipParameterSpec* spec)
    {
        if (spec == nullptr)
            return;

        for (size_t i = 0; i < buttons.size(); ++i)
        {
            if (i >= spec->choices.size())
                continue;

            buttons[i].setButtonText(spec->choices[i].label);
            buttons[i].setTooltip(choiceTooltip(*spec, i));
        }
    };

    applyChoices(pulseDutyButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::macroControl1));
    applyChoices(toneNoiseMixButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::macroControl4));

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::waveShape))
    {
        waveShapeLabel.setText(spec->label, juce::dontSendNotification);
        waveShapeLabel.setTooltip(spec->help);
        waveShapeValueLabel.setTooltip(spec->help);
        applyChoices(waveShapeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::ymEnvelopeShape))
    {
        ymEnvelopeShapeLabel.setText(spec->label, juce::dontSendNotification);
        ymEnvelopeShapeLabel.setTooltip(spec->help);
        ymEnvelopeShapeValueLabel.setTooltip(spec->help);
        applyChoices(ymEnvelopeShapeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::snNoiseMode))
    {
        snNoiseModeLabel.setText(spec->label, juce::dontSendNotification);
        snNoiseModeLabel.setTooltip(spec->help);
        snNoiseModeValueLabel.setTooltip(spec->help);
        applyChoices(snNoiseModeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::envelopeDecay))
    {
        envelopeDecayLabel.setText(spec->label, juce::dontSendNotification);
        envelopeDecayLabel.setTooltip(spec->help);
        envelopeDecaySlider.setTooltip(spec->help);
        envelopeDecayValueLabel.setTooltip(spec->help);
    }
}

void ChipperAudioProcessorEditor::applySelectedMacroTemplate()
{
    if (suppressMacroTemplateApply)
        return;

    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    if (! chipper::descriptorFor(mode).implemented)
        return;

    const auto selected = macroBox.getSelectedItemIndex();
    if (selected < 0)
        return;

    const auto order = chipper::macroOrder();
    const auto macroIndex = static_cast<size_t>(std::clamp(selected, 0, static_cast<int>(order.size() - 1u)));
    const auto& templ = chipper::macroTemplateFor(mode, order[macroIndex]);
    const std::array<const char*, 4> ids {
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

    const juce::ScopedValueSetter<bool> suppress(suppressMacroTemplateApply, true);
    for (size_t i = 0; i < ids.size(); ++i)
        setParameterValueFromUi(ids[i], templ.controls[i]);
    for (size_t i = 0; i < sourceIds.size(); ++i)
        setParameterValueFromUi(sourceIds[i], templ.sourceEnabled[i] ? 1.0f : 0.0f);
    setParameterValueFromUi(chipper::parameters::id::envelopeDecay, templ.envelopeDecay);
    setChoiceParameterFromUi(chipper::parameters::id::waveShape, templ.waveShape);
    setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, templ.ymEnvelopeShape);
    setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, templ.snNoiseMode);

    const juce::ScopedValueSetter<bool> suppressPreset(suppressPresetApply, true);
    presetBox.setSelectedId(0, juce::dontSendNotification);
    presetBox.setTextWhenNothingSelected("Factory Preset");
    presetBox.setTooltip("Applies a factory preset for the selected chip mode.");

    updateLiveControlReadouts();
}

void ChipperAudioProcessorEditor::applySelectedPreset()
{
    if (suppressPresetApply)
        return;

    const auto selected = presetBox.getSelectedItemIndex();
    if (selected < 0 || static_cast<size_t>(selected) >= displayedPresets.size())
        return;

    applyFactoryPreset(*displayedPresets[static_cast<size_t>(selected)]);
}

void ChipperAudioProcessorEditor::applyFactoryPreset(const chipper::PresetInfo& preset)
{
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

    const juce::ScopedValueSetter<bool> suppressMacro(suppressMacroTemplateApply, true);
    setChoiceParameterFromUi(chipper::parameters::id::chipMode, chipModeChoiceIndex(preset.chip));
    setChoiceParameterFromUi(chipper::parameters::id::accuracy, accuracyChoiceIndex(preset.accuracy));
    setChoiceParameterFromUi(chipper::parameters::id::macro, macroChoiceIndex(preset.macro));
    setChoiceParameterFromUi(chipper::parameters::id::playMode, playModeChoiceIndex(preset.playMode));

    for (size_t i = 0; i < controlIds.size(); ++i)
        setParameterValueFromUi(controlIds[i], preset.controls[i]);

    for (size_t i = 0; i < sourceIds.size(); ++i)
        setParameterValueFromUi(sourceIds[i], preset.sourceEnabled[i] ? 1.0f : 0.0f);

    setParameterValueFromUi(chipper::parameters::id::envelopeDecay, preset.envelopeDecay);
    setChoiceParameterFromUi(chipper::parameters::id::waveShape, preset.waveShape);
    setChoiceParameterFromUi(chipper::parameters::id::ymEnvelopeShape, preset.ymEnvelopeShape);
    setChoiceParameterFromUi(chipper::parameters::id::snNoiseMode, preset.snNoiseMode);
    setPlainParameterValueFromUi(chipper::parameters::id::clockHz, 0.0f);
    setPlainParameterValueFromUi(chipper::parameters::id::outputDb, preset.outputDb);

    presetBox.setTooltip(juce::String(preset.name) + ": " + juce::String(preset.note));
    updateDescriptorText();
    updateLiveControlReadouts();
}

chipper::PatchConfig ChipperAudioProcessorEditor::currentUiPatch(chipper::ChipMode mode,
                                                                 float control1,
                                                                 float control2,
                                                                 float control3,
                                                                 float control4,
                                                                 int waveShape,
                                                                 int ymEnvelopeShape,
                                                                 int snNoiseMode) const
{
    const auto macroChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::macro)));
    const auto playModeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode)));

    return chipper::makePatchConfig(
        mode,
        chipper::parameters::macroFromChoice(macroChoice),
        control1,
        control2,
        control3,
        control4,
        chipper::parameters::playModeFromChoice(playModeChoice),
        {
            parameterValue(chipper::parameters::id::source1Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source2Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source3Enabled) >= 0.5f,
            parameterValue(chipper::parameters::id::source4Enabled) >= 0.5f
        },
        parameterValue(chipper::parameters::id::envelopeDecay),
        waveShape,
        ymEnvelopeShape,
        snNoiseMode);
}

bool ChipperAudioProcessorEditor::usesPulseDutySegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::macroControl1,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesWaveShapeSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::waveShape,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesYmEnvelopeShapeSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::ymEnvelopeShape,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesSnNoiseModeSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::snNoiseMode,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesToneNoiseMixSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::macroControl4,
                                            chipper::ControlSurface::segmentedChoice);
}

juce::String ChipperAudioProcessorEditor::macroTemplateReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    const auto& templ = chipper::macroTemplateFor(mode, patch.macro);
    const auto label = templ.label.empty() ? juce::String(chipper::toString(patch.macro)) : juce::String(templ.label);

    if (! chipper::descriptorFor(mode).implemented)
        return label + ": planned mapping";

    if (mode == chipper::ChipMode::nes)
        return label + " -> " + pulseDutyReadout(mode, patch.control1) + " | " + nesNoiseModeReadout(patch) + " | " + nesFocusReadout(patch.control4);

    if (mode == chipper::ChipMode::dmg)
        return label + " -> " + pulseDutyReadout(mode, patch.control1) + " | " + dmgNoiseModeReadout(patch) + " | " + dmgEnvelopeReadout(patch.control4);

    if (mode == chipper::ChipMode::ym2149)
        return label + " -> " + ymSpreadReadout(patch.control1) + " | " + ymNoiseReadout(patch.control3) + " | " + ymToneNoiseReadout(patch.control4);

    if (mode == chipper::ChipMode::sn76489)
        return label + " -> " + snStackReadout(patch.control1) + " | " + snNoiseModeReadout(patch) + " | " + snLevelReadout(patch.control4);

    return label + ": " + juce::String(templ.help);
}

juce::String ChipperAudioProcessorEditor::pulseDutyReadout(chipper::ChipMode mode, float value) const
{
    static constexpr std::array<const char*, 4> nesDutyLabels { "12.5% narrow pulse", "25% hollow pulse", "50% square", "75% inverted pulse" };
    static constexpr std::array<const char*, 4> dmgDutyLabels { "12.5% thin pulse", "25% narrow pulse", "50% square", "75% wide pulse" };
    const auto index = static_cast<size_t>(std::clamp(static_cast<int>(std::round(value * 3.0f)), 0, 3));
    return mode == chipper::ChipMode::dmg ? dmgDutyLabels[index] : nesDutyLabels[index];
}

juce::String ChipperAudioProcessorEditor::waveShapeReadout(int choice) const
{
    switch (std::clamp(choice, 0, 4))
    {
        case 1: return "Writes 32-sample triangle into Wave RAM";
        case 2: return "Writes 32-sample saw ramp into Wave RAM";
        case 3: return "Writes 50% pulse into Wave RAM";
        case 4: return "Writes stepped 4-level table into Wave RAM";
        case 0:
        default:
            return "RAM: preserve current/register-trace Wave RAM";
    }
}

juce::String ChipperAudioProcessorEditor::ymEnvelopeShapeReadout(int choice) const
{
    switch (std::clamp(choice, 0, 4))
    {
        case 1: return "Register 13 = 0x09, fall then hold low";
        case 2: return "Register 13 = 0x0D, rise then hold high";
        case 3: return "Register 13 = 0x08, repeating saw down";
        case 4: return "Register 13 = 0x0E, repeating triangle";
        case 0:
        default:
            return "Fixed volume registers; envelope bit off";
    }
}

juce::String ChipperAudioProcessorEditor::snNoiseRegisterLabel(uint8_t noiseControl) const
{
    const auto rate = noiseControl & 0x03u;
    const auto kind = (noiseControl & 0x04u) != 0 ? "white" : "periodic";
    const auto rateText = [rate]() -> const char*
    {
        switch (rate)
        {
            case 0: return "/512";
            case 1: return "/1024";
            case 2: return "/2048";
            case 3: return "tone 3 clock";
            default: return "/512";
        }
    }();

    return juce::String(kind) + " " + rateText;
}

juce::String ChipperAudioProcessorEditor::noiseModeReadout(chipper::ChipMode mode, const chipper::PatchConfig& patch) const
{
    if (mode == chipper::ChipMode::nes)
        return nesNoiseModeReadout(patch);

    if (mode == chipper::ChipMode::dmg)
        return dmgNoiseModeReadout(patch);

    return snNoiseModeReadout(patch);
}

juce::String ChipperAudioProcessorEditor::nesNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseRegister = chipper::nesNoiseRegisterForPatch(patch);
    const auto registerText = juce::String("$400E=0x") + juce::String::toHexString(static_cast<int>(noiseRegister)).paddedLeft('0', 2).toUpperCase();
    const auto modeText = (noiseRegister & 0x80u) != 0 ? "short-loop" : "long LFSR";
    const auto periodText = juce::String("period ") + juce::String(static_cast<int>(noiseRegister & 0x0fu));
    const auto resolvedText = registerText + ", " + modeText + ", " + periodText;
    return std::clamp(patch.snNoiseMode, 0, 2) == 0 ? juce::String("Macro -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::dmgNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseRegister = chipper::dmgNoiseRegisterForPatch(patch);
    const auto registerText = juce::String("NR43=0x") + juce::String::toHexString(static_cast<int>(noiseRegister)).paddedLeft('0', 2).toUpperCase();
    const auto widthText = (noiseRegister & 0x08u) != 0 ? "7-bit LFSR" : "15-bit LFSR";
    const auto shiftText = juce::String("shift ") + juce::String(static_cast<int>((noiseRegister >> 4u) & 0x0fu));
    const auto divisorText = juce::String("divisor code ") + juce::String(static_cast<int>(noiseRegister & 0x07u));
    const auto resolvedText = registerText + ", " + widthText + ", " + shiftText + ", " + divisorText;
    return std::clamp(patch.snNoiseMode, 0, 2) == 0 ? juce::String("Macro -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::snNoiseModeReadout(const chipper::PatchConfig& patch) const
{
    const auto noiseControl = chipper::sn76489NoiseControlForPatch(patch);
    const auto registerText = juce::String("Reg E=0x") + juce::String::toHexString(static_cast<int>(noiseControl)).toUpperCase();
    const auto resolvedText = registerText + ", " + snNoiseRegisterLabel(noiseControl);
    return patch.snNoiseMode == 0 ? juce::String("Macro -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::nesSweepReadout(float value) const
{
    if (value < 0.18f)
        return "Sweep mostly off";
    if (value < 0.45f)
        return "Small pitch gestures";
    if (value < 0.75f)
        return "Coin / jump bends";
    return "Deep laser sweep";
}

juce::String ChipperAudioProcessorEditor::nesNoiseReadout(float value) const
{
    const auto period = std::clamp(static_cast<int>(std::round((1.0f - value) * 14.0f)), 0, 15);
    if (value < 0.25f)
        return juce::String("Low grit, period ") + juce::String(period);
    if (value < 0.60f)
        return juce::String("Snare noise, period ") + juce::String(period);
    return juce::String("Short hats, period ") + juce::String(period);
}

juce::String ChipperAudioProcessorEditor::nesFocusReadout(float value) const
{
    if (value < 0.33f)
        return "Triangle bass focus";
    if (value > 0.66f)
        return "Pulse stack focus";
    return "Pulse + triangle stack";
}

juce::String ChipperAudioProcessorEditor::dmgSweepReadout(float value) const
{
    const auto shift = std::clamp(static_cast<int>(std::round(value * 7.0f)), 0, 7);
    if (shift == 0)
        return "CH1 sweep mostly off";
    if (shift <= 2)
        return juce::String("Small CH1 sweep, shift ") + juce::String(shift);
    if (shift <= 5)
        return juce::String("Arcade pitch bend, shift ") + juce::String(shift);
    return juce::String("Fast sweep, shift ") + juce::String(shift);
}

juce::String ChipperAudioProcessorEditor::dmgNoiseReadout(float value) const
{
    const auto pitch = std::clamp(static_cast<int>(std::round((1.0f - value) * 7.0f)), 0, 7);
    return juce::String("NR43 clock shift ") + juce::String(pitch) + ", divisor code 2";
}

juce::String ChipperAudioProcessorEditor::dmgEnvelopeReadout(float value) const
{
    const auto level = std::clamp(static_cast<int>(std::round(value * 15.0f)), 1, 15);
    return juce::String("Envelope start ") + juce::String(level) + "/15";
}

juce::String ChipperAudioProcessorEditor::ymSpreadReadout(float value) const
{
    const auto spread = std::clamp(static_cast<int>(std::round(value * 12.0f)), 0, 12);
    return juce::String("A/B/C spread ") + juce::String(spread) + " semitones";
}

juce::String ChipperAudioProcessorEditor::ymMotionReadout(float value) const
{
    if (value < 0.25f)
        return "Minimal pitch motion";
    if (value < 0.65f)
        return "Moderate pitch motion";
    return "Wide arcade pitch motion";
}

juce::String ChipperAudioProcessorEditor::ymNoiseReadout(float value) const
{
    const auto period = std::clamp(static_cast<int>(std::round((1.0f - value) * 30.0f)) + 1, 1, 31);
    return juce::String("Noise period ") + juce::String(period) + "/31";
}

juce::String ChipperAudioProcessorEditor::ymToneNoiseReadout(float value) const
{
    const auto mixer = chipper::ym2149MixerRegisterForControl(value);
    switch (mixer)
    {
        case 0x07u: return "Reg 7=0x07, noise only";
        case 0x00u: return "Reg 7=0x00, tone + noise";
        case 0x38u:
        default:
            return "Reg 7=0x38, tone only";
    }
}

juce::String ChipperAudioProcessorEditor::snStackReadout(float value) const
{
    const auto spread = std::clamp(static_cast<int>(std::round(value * 12.0f)), 0, 12);
    return juce::String("Tone spread ") + juce::String(spread) + " semitones";
}

juce::String ChipperAudioProcessorEditor::snMotionReadout(float value) const
{
    if (value < 0.25f)
        return "Small pitch motion";
    if (value < 0.60f)
        return "Arcade blip bends";
    return "Wide SFX pitch motion";
}

juce::String ChipperAudioProcessorEditor::snLevelReadout(float value) const
{
    const auto attenuation = std::clamp(static_cast<int>(15 - std::round(value * 13.0f)), 0, 15);
    return juce::String("Noise attenuation ") + juce::String(attenuation) + "/15";
}

bool ChipperAudioProcessorEditor::usesSourceChannelSurface(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::source1Enabled,
                                            chipper::ControlSurface::sourceCards);
}

bool ChipperAudioProcessorEditor::usesEnvelopeDecayControl(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::envelopeDecay,
                                            chipper::ControlSurface::slider);
}

void ChipperAudioProcessorEditor::setSourceChannelSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesSourceChannelSurface(mode);
    for (auto& channelButton : sourceChannelButtons)
        channelButton.setVisible(active);

    for (auto& itemLabel : moduleItemLabels[1])
        itemLabel.setVisible(! active && ! itemLabel.getText().isEmpty());

    if (active)
        updateSourceChannelButtons(mode);
}

void ChipperAudioProcessorEditor::setWaveShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesWaveShapeSegment(mode);
    waveShapeLabel.setVisible(active);
    waveShapeValueLabel.setVisible(active);
    for (auto& button : waveShapeButtons)
        button.setVisible(active);

    if (active)
    {
        const auto choice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape)));
        updateWaveShapeButtons(choice, true);
    }
}

void ChipperAudioProcessorEditor::setYmEnvelopeShapeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesYmEnvelopeShapeSegment(mode);
    ymEnvelopeShapeLabel.setVisible(active);
    ymEnvelopeShapeValueLabel.setVisible(active);
    for (auto& button : ymEnvelopeShapeButtons)
        button.setVisible(active);

    if (active)
    {
        const auto choice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape)));
        updateYmEnvelopeShapeButtons(choice, true);
    }
}

void ChipperAudioProcessorEditor::setSnNoiseModeSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesSnNoiseModeSegment(mode);
    snNoiseModeLabel.setVisible(active);
    snNoiseModeValueLabel.setVisible(active);
    for (auto& button : snNoiseModeButtons)
        button.setVisible(active);

    if (active)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))));
        updateSnNoiseModeButtons(mode, patch, true);
    }
}

void ChipperAudioProcessorEditor::setEnvelopeDecayControlVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesEnvelopeDecayControl(mode);
    envelopeDecayLabel.setVisible(active);
    envelopeDecaySlider.setVisible(active);
    envelopeDecayValueLabel.setVisible(active);
    envelopeDecayLabel.setEnabled(active);
    envelopeDecaySlider.setEnabled(active);
    envelopeDecayValueLabel.setEnabled(active);
    envelopeDecayLabel.setAlpha(active ? 1.0f : 0.55f);
    envelopeDecaySlider.setAlpha(active ? 1.0f : 0.55f);
    envelopeDecayValueLabel.setAlpha(active ? 1.0f : 0.55f);

    if (active)
        updateEnvelopeDecayReadout(mode);
}

void ChipperAudioProcessorEditor::updateSourceChannelButtons(chipper::ChipMode mode)
{
    static const std::array<const char*, sourceChannelCount> nesBigMonoLabels {
        "Pulse 1  |  duty lead",
        "Pulse 2  |  stack / sweep",
        "Triangle | bass body",
        "Noise    | snare / hats"
    };
    static const std::array<const char*, sourceChannelCount> nesChipPolyLabels {
        "Pulse 1  |  note 1",
        "Pulse 2  |  note 2",
        "Triangle | note 3 bass",
        "Noise    | mono SFX layer"
    };
    static const std::array<const char*, sourceChannelCount> dmgBigMonoLabels {
        "Pulse 1  |  sweep voice",
        "Pulse 2  |  support pulse",
        "Wave     |  sample body",
        "Noise    |  handheld grit"
    };
    static const std::array<const char*, sourceChannelCount> dmgChipPolyLabels {
        "Pulse 1  |  note 1",
        "Pulse 2  |  note 2",
        "Wave     |  note 3",
        "Noise    |  SFX layer"
    };
    static const std::array<const char*, sourceChannelCount> ymBigMonoLabels {
        "Channel A | tone lead",
        "Channel B | chord tone",
        "Channel C | bass / stack",
        "Noise     | shared PSG"
    };
    static const std::array<const char*, sourceChannelCount> ymChipPolyLabels {
        "Channel A | note 1",
        "Channel B | note 2",
        "Channel C | note 3",
        "Noise     | shared layer"
    };
    static const std::array<const char*, sourceChannelCount> snBigMonoLabels {
        "Tone 1 | lead tone",
        "Tone 2 | chord tone",
        "Tone 3 | bass / noise clock",
        "Noise  | PSG noise"
    };
    static const std::array<const char*, sourceChannelCount> snChipPolyLabels {
        "Tone 1 | note 1",
        "Tone 2 | note 2",
        "Tone 3 | note 3",
        "Noise  | mono SFX layer"
    };

    const auto playModeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode)));
    const auto playMode = chipper::parameters::playModeFromChoice(playModeChoice);
    const auto* labels = &nesBigMonoLabels;

    if (mode == chipper::ChipMode::nes)
        labels = playMode == chipper::PlayMode::chipPoly ? &nesChipPolyLabels : &nesBigMonoLabels;
    else if (mode == chipper::ChipMode::dmg)
        labels = playMode == chipper::PlayMode::chipPoly ? &dmgChipPolyLabels : &dmgBigMonoLabels;
    else if (mode == chipper::ChipMode::ym2149)
        labels = playMode == chipper::PlayMode::chipPoly ? &ymChipPolyLabels : &ymBigMonoLabels;
    else if (mode == chipper::ChipMode::sn76489)
        labels = playMode == chipper::PlayMode::chipPoly ? &snChipPolyLabels : &snBigMonoLabels;

    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        sourceChannelButtons[i].setButtonText((*labels)[i]);
        if (const auto* spec = chipper::parameterSpecFor(mode, sourceRole(i)))
            sourceChannelButtons[i].setTooltip(juce::String(spec->label) + ": " + juce::String(spec->help));
        else
            sourceChannelButtons[i].setTooltip(juce::String("Enable or mute ") + (*labels)[i]);
    }
}

void ChipperAudioProcessorEditor::updateEnvelopeDecayReadout(chipper::ChipMode mode)
{
    envelopeDecayValueLabel.setText(envelopeDecayReadout(mode, parameterValue(chipper::parameters::id::envelopeDecay)), juce::dontSendNotification);
}

juce::String ChipperAudioProcessorEditor::envelopeDecayReadout(chipper::ChipMode mode, float value) const
{
    if (value <= 0.01f)
        return "Off: macro envelope/level unchanged";

    if (mode == chipper::ChipMode::dmg)
    {
        const auto period = std::clamp(static_cast<int>(std::round(7.0f - (std::clamp(value, 0.0f, 1.0f) * 6.0f))), 1, 7);
        return juce::String("DMG 64 Hz decay, period ") + juce::String(period);
    }

    const auto period = std::clamp(static_cast<int>(std::round(15.0f - (std::clamp(value, 0.0f, 1.0f) * 14.0f))), 1, 15);
    return juce::String("NES envelope decay, period ") + juce::String(period);
}

void ChipperAudioProcessorEditor::updatePulseDutyButtons(float value, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(static_cast<int>(std::round(value * 3.0f)), 0, 3));
    for (size_t i = 0; i < pulseDutyButtons.size(); ++i)
    {
        pulseDutyButtons[i].setVisible(shouldBeVisible);
        pulseDutyButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }
}

void ChipperAudioProcessorEditor::updateToneNoiseMixButtons(float value, bool shouldBeVisible)
{
    const auto mixer = chipper::ym2149MixerRegisterForControl(value);
    const auto selected = mixer == 0x07u ? size_t { 0u } : (mixer == 0x00u ? size_t { 2u } : size_t { 1u });
    layoutSegmentedButtons(toneNoiseMixButtons, toneNoiseMixSegmentBounds, toneNoiseMixButtons.size());
    for (size_t i = 0; i < toneNoiseMixButtons.size(); ++i)
    {
        toneNoiseMixButtons[i].setVisible(shouldBeVisible);
        toneNoiseMixButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }
}

void ChipperAudioProcessorEditor::updateWaveShapeButtons(int choice, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(choice, 0, static_cast<int>(waveShapeButtons.size() - 1u)));
    for (size_t i = 0; i < waveShapeButtons.size(); ++i)
    {
        waveShapeButtons[i].setVisible(shouldBeVisible);
        waveShapeButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    waveShapeValueLabel.setVisible(shouldBeVisible);
    waveShapeValueLabel.setText(waveShapeReadout(static_cast<int>(selected)), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateYmEnvelopeShapeButtons(int choice, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(choice, 0, static_cast<int>(ymEnvelopeShapeButtons.size() - 1u)));
    for (size_t i = 0; i < ymEnvelopeShapeButtons.size(); ++i)
    {
        ymEnvelopeShapeButtons[i].setVisible(shouldBeVisible);
        ymEnvelopeShapeButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    ymEnvelopeShapeValueLabel.setVisible(shouldBeVisible);
    ymEnvelopeShapeValueLabel.setText(ymEnvelopeShapeReadout(static_cast<int>(selected)), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateSnNoiseModeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::snNoiseMode);
    const auto choiceCount = spec == nullptr || spec->choices.empty()
                                 ? snNoiseModeButtons.size()
                                 : std::min(snNoiseModeButtons.size(), spec->choices.size());
    const auto selected = static_cast<size_t>(std::clamp(patch.snNoiseMode, 0, static_cast<int>(choiceCount - 1u)));
    layoutSegmentedButtons(snNoiseModeButtons, snNoiseModeSegmentBounds, choiceCount);
    for (size_t i = 0; i < snNoiseModeButtons.size(); ++i)
    {
        const auto visible = shouldBeVisible && i < choiceCount;
        snNoiseModeButtons[i].setVisible(visible);
        snNoiseModeButtons[i].setToggleState(visible && i == selected, juce::dontSendNotification);
    }

    snNoiseModeValueLabel.setVisible(shouldBeVisible);
    snNoiseModeValueLabel.setText(noiseModeReadout(mode, patch), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateDescriptorText()
{
    const auto modeChoice = static_cast<int>(std::round(audioProcessor.getValueTreeState()
                                                            .getRawParameterValue(chipper::parameters::id::chipMode)
                                                            ->load()));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    if (descriptorTextInitialized && mode == displayedMode)
        return;

    descriptorTextInitialized = true;
    displayedMode = mode;
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto hasLiveCore = descriptor.implemented;
    const auto supportsChipPoly = chipper::supportsPlayMode(mode, chipper::PlayMode::chipPoly);

    updateMacroChoices(mode);
    updatePresetChoices(mode);
    chipSummaryLabel.setText(descriptor.summary, juce::dontSendNotification);
    coreReadinessLabel.setText(hasLiveCore ? "LIVE" : "PLANNED", juce::dontSendNotification);
    coreReadinessLabel.setTooltip(hasLiveCore
                                      ? "This chip mode is backed by an active internal core."
                                      : "This chip mode is a roadmap surface; controls are inactive until its core is implemented.");
    coreReadinessLabel.setColour(juce::Label::textColourId, hasLiveCore ? juce::Colour(0xff101414) : juce::Colour(0xffd9e1e8));
    coreReadinessLabel.setColour(juce::Label::backgroundColourId, hasLiveCore ? juce::Colour(0xff56c7d8) : juce::Colour(0xff344047));
    globalStripLabel.setText(hasLiveCore ? "Live Chip Controls" : "Planned Control Surface", juce::dontSendNotification);
    macroSummaryLabel.setVisible(hasLiveCore);
    macroSummaryLabel.setEnabled(hasLiveCore);
    macroSummaryLabel.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    accuracyBox.setEnabled(hasLiveCore);
    presetBox.setEnabled(hasLiveCore && ! displayedPresets.empty());
    macroBox.setEnabled(hasLiveCore);
    playModeBox.setEnabled(hasLiveCore && supportsChipPoly);
    playModeBox.setTooltip(supportsChipPoly
                               ? "Chooses how incoming notes use the chip channels inside one patch."
                               : "Chip Poly is disabled until this chip has tested finite-channel allocation.");
    clockSlider.setEnabled(hasLiveCore);
    clockLabel.setAlpha(hasLiveCore ? 1.0f : 0.55f);
    clockSlider.setAlpha(hasLiveCore ? 1.0f : 0.55f);

    for (size_t i = 0; i < moduleTitleLabels.size(); ++i)
    {
        if (i >= descriptor.modules.size())
            continue;

        const auto& module = descriptor.modules[i];
        moduleTitleLabels[i].setText(module.title, juce::dontSendNotification);
        moduleSummaryLabels[i].setText(module.summary, juce::dontSendNotification);

        for (size_t item = 0; item < moduleItemLabels[i].size(); ++item)
        {
            const auto text = item < module.items.size() ? module.items[item] : std::string {};
            moduleItemLabels[i][item].setText(text, juce::dontSendNotification);
            moduleItemLabels[i][item].setVisible(! text.empty());
        }
    }

    for (size_t i = 0; i < nativeLabels.size(); ++i)
    {
        const auto* spec = chipper::parameterSpecFor(mode, macroControlRole(i));
        const auto hasFallbackControl = i < descriptor.controls.size();
        if (spec != nullptr || hasFallbackControl)
        {
            const auto active = hasLiveCore;
            nativeGroupLabels[i].setText(spec != nullptr ? spec->group : descriptor.controls[i].group, juce::dontSendNotification);
            nativeLabels[i].setText(spec != nullptr ? spec->label : descriptor.controls[i].label, juce::dontSendNotification);
            nativeSliders[i].setTooltip(spec != nullptr ? spec->help : descriptor.controls[i].help);
            controlValueLabels[i].setTooltip(spec != nullptr ? spec->help : descriptor.controls[i].help);
            nativeGroupLabels[i].setVisible(true);
            nativeLabels[i].setVisible(true);
            nativeSliders[i].setVisible(true);
            controlValueLabels[i].setVisible(true);
            nativeGroupLabels[i].setEnabled(active);
            nativeLabels[i].setEnabled(active);
            nativeSliders[i].setEnabled(active);
            controlValueLabels[i].setEnabled(active);
            nativeGroupLabels[i].setAlpha(active ? 1.0f : 0.55f);
            nativeLabels[i].setAlpha(active ? 1.0f : 0.55f);
            nativeSliders[i].setAlpha(active ? 1.0f : 0.55f);
            controlValueLabels[i].setAlpha(active ? 1.0f : 0.55f);
        }
        else
        {
            nativeGroupLabels[i].setText({}, juce::dontSendNotification);
            nativeLabels[i].setText("Native Control", juce::dontSendNotification);
            nativeSliders[i].setTooltip({});
            controlValueLabels[i].setTooltip({});
            nativeGroupLabels[i].setVisible(false);
            nativeLabels[i].setVisible(false);
            nativeSliders[i].setVisible(false);
            controlValueLabels[i].setVisible(false);
        }
    }

    updateSegmentedControlSpecs(mode);
    controlValueLabels[4].setVisible(true);
    controlValueLabels[5].setVisible(true);
    controlValueLabels[4].setEnabled(hasLiveCore);
    controlValueLabels[5].setEnabled(hasLiveCore);
    controlValueLabels[4].setAlpha(hasLiveCore ? 1.0f : 0.55f);
    controlValueLabels[5].setAlpha(hasLiveCore ? 1.0f : 0.55f);
    setSourceChannelSurfaceVisible(mode, usesSourceChannelSurface(mode));
    setWaveShapeSegmentVisible(mode, usesWaveShapeSegment(mode) && hasLiveCore);
    setYmEnvelopeShapeSegmentVisible(mode, usesYmEnvelopeShapeSegment(mode) && hasLiveCore);
    setSnNoiseModeSegmentVisible(mode, usesSnNoiseModeSegment(mode) && hasLiveCore);
    setEnvelopeDecayControlVisible(mode, usesEnvelopeDecayControl(mode) && hasLiveCore);
    const auto hasCustomToneSurface = hasLiveCore && (usesWaveShapeSegment(mode) || usesSnNoiseModeSegment(mode));
    for (auto& itemLabel : moduleItemLabels[2])
        itemLabel.setVisible(! hasCustomToneSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomEnvelopeSurface = hasLiveCore && (usesEnvelopeDecayControl(mode) || usesYmEnvelopeShapeSegment(mode));
    for (auto& itemLabel : moduleItemLabels[3])
        itemLabel.setVisible(! hasCustomEnvelopeSurface && ! itemLabel.getText().isEmpty());
    updatePulseDutyButtons(parameterValue(chipper::parameters::id::macroControl1), usesPulseDutySegment(mode) && hasLiveCore);
    updateLiveControlReadouts();
    repaint();
}

void ChipperAudioProcessorEditor::updateLiveControlReadouts()
{
    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto macroControl1 = parameterValue(chipper::parameters::id::macroControl1);
    const auto macroControl2 = parameterValue(chipper::parameters::id::macroControl2);
    const auto macroControl3 = parameterValue(chipper::parameters::id::macroControl3);
    const auto macroControl4 = parameterValue(chipper::parameters::id::macroControl4);
    const auto waveShape = static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape)));
    const auto ymEnvelopeShape = static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape)));
    const auto snNoiseMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode)));
    const auto patch = currentUiPatch(mode, macroControl1, macroControl2, macroControl3, macroControl4, waveShape, ymEnvelopeShape, snNoiseMode);
    const auto macroText = macroTemplateReadout(mode, patch);
    macroSummaryLabel.setText(macroText, juce::dontSendNotification);
    macroBox.setTooltip(macroText);

    const auto hasPulseDutySegment = usesPulseDutySegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasWaveShapeSegment = usesWaveShapeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasYmEnvelopeShapeSegment = usesYmEnvelopeShapeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasSnNoiseModeSegment = usesSnNoiseModeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasToneNoiseMixSegment = usesToneNoiseMixSegment(mode) && chipper::descriptorFor(mode).implemented;
    nativeSliders[0].setVisible(! hasPulseDutySegment);
    nativeSliders[3].setVisible(! hasToneNoiseMixSegment);
    updatePulseDutyButtons(patch.control1, hasPulseDutySegment);
    updateToneNoiseMixButtons(patch.control4, hasToneNoiseMixSegment);
    updateWaveShapeButtons(waveShape, hasWaveShapeSegment);
    updateYmEnvelopeShapeButtons(ymEnvelopeShape, hasYmEnvelopeShapeSegment);
    updateSnNoiseModeButtons(mode, patch, hasSnNoiseModeSegment);
    updateEnvelopeDecayReadout(mode);

    if (mode == chipper::ChipMode::nes)
    {
        controlValueLabels[0].setText(pulseDutyReadout(mode, patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(nesSweepReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(nesNoiseReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(nesFocusReadout(patch.control4), juce::dontSendNotification);

        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::dmg)
    {
        controlValueLabels[0].setText(pulseDutyReadout(mode, patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(dmgSweepReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(dmgNoiseReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(dmgEnvelopeReadout(patch.control4), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::ym2149)
    {
        controlValueLabels[0].setText(ymSpreadReadout(patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(ymMotionReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(ymNoiseReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(ymToneNoiseReadout(patch.control4), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else if (mode == chipper::ChipMode::sn76489)
    {
        controlValueLabels[0].setText(snStackReadout(patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(snMotionReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(snNoiseModeReadout(patch), juce::dontSendNotification);
        controlValueLabels[3].setText(snLevelReadout(patch.control4), juce::dontSendNotification);
        updateSourceChannelButtons(mode);
    }
    else
    {
        controlValueLabels[0].setText(juce::String(patch.control1, 2), juce::dontSendNotification);
        controlValueLabels[1].setText(juce::String(patch.control2, 2), juce::dontSendNotification);
        controlValueLabels[2].setText(juce::String(patch.control3, 2), juce::dontSendNotification);
        controlValueLabels[3].setText(juce::String(patch.control4, 2), juce::dontSendNotification);
    }

    const auto clock = parameterValue(chipper::parameters::id::clockHz);
    const auto defaultClock = chipper::parameters::defaultClockForMode(mode);
    const auto clockText = clock <= 0.0f
        ? juce::String("Default ") + juce::String(defaultClock / 1000000.0, 2) + " MHz"
        : juce::String("Override ") + juce::String(static_cast<double>(clock) / 1000000.0, 2) + " MHz";
    controlValueLabels[4].setText(clockText, juce::dontSendNotification);

    const auto outputDb = parameterValue(chipper::parameters::id::outputDb);
    controlValueLabels[5].setText(juce::String("Output ") + juce::String(outputDb, 1) + " dB", juce::dontSendNotification);
}
