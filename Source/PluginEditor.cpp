#include "PluginEditor.h"

#include "ChipperBuildInfo.h"
#include "Engine/ChipDescriptors.h"

#include <cmath>

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor)
{
    setSize(980, 800);

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
    macroBox.addItemList(chipper::parameters::macroChoices(), 1);
    playModeBox.addItemList(chipper::parameters::playModeChoices(), 1);
    chipModeBox.setTooltip("Selects the chip model or planned chip family.");
    accuracyBox.setTooltip("Selects the requested accuracy tier for the active core.");
    macroBox.setTooltip("Applies a chip-specific musical register template.");
    playModeBox.setTooltip("Chooses how incoming notes use the chip channels inside one patch.");

    const std::array<const char*, 4> headerNames { "Chip Mode", "Accuracy", "Macro", "Play Mode" };
    for (size_t i = 0; i < headerControlLabels.size(); ++i)
    {
        headerControlLabels[i].setText(headerNames[i], juce::dontSendNotification);
        headerControlLabels[i].setJustificationType(juce::Justification::centredLeft);
        headerControlLabels[i].setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        headerControlLabels[i].setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(headerControlLabels[i]);
    }

    addAndMakeVisible(chipModeBox);
    addAndMakeVisible(accuracyBox);
    addAndMakeVisible(macroBox);
    addAndMakeVisible(playModeBox);

    chipModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::chipMode, chipModeBox);
    accuracyAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::accuracy, accuracyBox);
    macroAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::macro, macroBox);
    playModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::playMode, playModeBox);

    addLabeledSlider(clockSlider, clockLabel, "Clock");
    clockSlider.setTextValueSuffix(" Hz");
    clockSlider.setSkewFactor(0.35);
    clockSlider.setTooltip("Optional chip clock override. Zero uses the documented default for the selected mode.");
    clockAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::clockHz, clockSlider);

    addLabeledSlider(outputSlider, outputLabel, "Output");
    outputSlider.setTextValueSuffix(" dB");
    outputSlider.setTooltip("Final plugin output level.");
    outputAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::outputDb, outputSlider);

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

    for (auto& channelLabel : nesChannelLabels)
    {
        channelLabel.setJustificationType(juce::Justification::centredLeft);
        channelLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe6f2f0));
        channelLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff202c33));
        channelLabel.setMinimumHorizontalScale(0.70f);
        channelLabel.setVisible(false);
        addAndMakeVisible(channelLabel);
    }

    globalStripLabel.setText("Global Performance Controls", juce::dontSendNotification);
    globalStripLabel.setJustificationType(juce::Justification::centredLeft);
    globalStripLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0c94d));
    globalStripLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(globalStripLabel);

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

    if (displayedMode == chipper::ChipMode::nes)
    {
        for (const auto& bounds : nesChannelBounds)
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
    titleLabel.setBounds(top.removeFromLeft(132));
    const auto placeHeaderCombo = [this](size_t index, juce::ComboBox& comboBox, juce::Rectangle<int> bounds)
    {
        headerControlLabels[index].setBounds(bounds.removeFromTop(16));
        comboBox.setBounds(bounds.reduced(0, 4));
    };

    placeHeaderCombo(0, chipModeBox, top.removeFromLeft(220));
    top.removeFromLeft(8);
    placeHeaderCombo(1, accuracyBox, top.removeFromLeft(126));
    top.removeFromLeft(8);
    placeHeaderCombo(2, macroBox, top.removeFromLeft(136));
    top.removeFromLeft(8);
    placeHeaderCombo(3, playModeBox, top.removeFromLeft(150));
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

    auto nesPanel = moduleBounds[1].reduced(12, 9);
    nesPanel.removeFromTop(20);
    nesPanel.removeFromTop(30);
    nesPanel.removeFromTop(4);
    const auto nesGap = 6;
    const auto nesCardWidth = (nesPanel.getWidth() - nesGap) / 2;
    const auto nesCardHeight = (nesPanel.getHeight() - nesGap) / 2;
    for (size_t i = 0; i < nesChannelBounds.size(); ++i)
    {
        const auto row = static_cast<int>(i / 2);
        const auto column = static_cast<int>(i % 2);
        nesChannelBounds[i] = {
            nesPanel.getX() + (column * (nesCardWidth + nesGap)),
            nesPanel.getY() + (row * (nesCardHeight + nesGap)),
            nesCardWidth,
            nesCardHeight
        };
        nesChannelLabels[i].setBounds(nesChannelBounds[i].reduced(8, 1));
    }

    area.removeFromTop(12);
    globalStripBounds = area.removeFromTop(190);
    auto strip = globalStripBounds.reduced(12, 8);
    globalStripLabel.setBounds(strip.removeFromTop(20));
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
    placeGroupedSlider(nativeSliders[1], nativeGroupLabels[1], nativeLabels[1], controlValueLabels[1], controlCells[1]);
    placeGroupedSlider(nativeSliders[2], nativeGroupLabels[2], nativeLabels[2], controlValueLabels[2], controlCells[2]);
    placeGroupedSlider(nativeSliders[3], nativeGroupLabels[3], nativeLabels[3], controlValueLabels[3], controlCells[3]);
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

float ChipperAudioProcessorEditor::parameterValue(const char* parameterId) const
{
    if (const auto* value = audioProcessor.getValueTreeState().getRawParameterValue(parameterId))
        return value->load();

    return 0.0f;
}

juce::String ChipperAudioProcessorEditor::nesDutyReadout(float value) const
{
    static constexpr std::array<const char*, 4> dutyLabels { "12.5% narrow pulse", "25% hollow pulse", "50% square", "75% inverted pulse" };
    const auto index = static_cast<size_t>(std::clamp(static_cast<int>(std::round(value * 3.0f)), 0, 3));
    return dutyLabels[index];
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

void ChipperAudioProcessorEditor::setNesChannelSurfaceVisible(bool shouldBeVisible)
{
    for (auto& channelLabel : nesChannelLabels)
        channelLabel.setVisible(shouldBeVisible);

    for (auto& itemLabel : moduleItemLabels[1])
        itemLabel.setVisible(! shouldBeVisible && ! itemLabel.getText().isEmpty());
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

    chipSummaryLabel.setText(descriptor.summary, juce::dontSendNotification);
    coreReadinessLabel.setText(hasLiveCore ? "LIVE" : "PLANNED", juce::dontSendNotification);
    coreReadinessLabel.setTooltip(hasLiveCore
                                      ? "This chip mode is backed by an active internal core."
                                      : "This chip mode is a roadmap surface; controls are inactive until its core is implemented.");
    coreReadinessLabel.setColour(juce::Label::textColourId, hasLiveCore ? juce::Colour(0xff101414) : juce::Colour(0xffd9e1e8));
    coreReadinessLabel.setColour(juce::Label::backgroundColourId, hasLiveCore ? juce::Colour(0xff56c7d8) : juce::Colour(0xff344047));
    globalStripLabel.setText(hasLiveCore ? "Live Chip Controls" : "Planned Control Surface", juce::dontSendNotification);
    accuracyBox.setEnabled(hasLiveCore);
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
        if (i < descriptor.controls.size())
        {
            const auto active = hasLiveCore;
            nativeGroupLabels[i].setText(descriptor.controls[i].group, juce::dontSendNotification);
            nativeLabels[i].setText(descriptor.controls[i].label, juce::dontSendNotification);
            nativeSliders[i].setTooltip(descriptor.controls[i].help);
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
            nativeGroupLabels[i].setVisible(false);
            nativeLabels[i].setVisible(false);
            nativeSliders[i].setVisible(false);
            controlValueLabels[i].setVisible(false);
        }
    }

    controlValueLabels[4].setVisible(true);
    controlValueLabels[5].setVisible(true);
    controlValueLabels[4].setEnabled(hasLiveCore);
    controlValueLabels[5].setEnabled(hasLiveCore);
    controlValueLabels[4].setAlpha(hasLiveCore ? 1.0f : 0.55f);
    controlValueLabels[5].setAlpha(hasLiveCore ? 1.0f : 0.55f);
    setNesChannelSurfaceVisible(mode == chipper::ChipMode::nes);
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

    if (mode == chipper::ChipMode::nes)
    {
        controlValueLabels[0].setText(nesDutyReadout(macroControl1), juce::dontSendNotification);
        controlValueLabels[1].setText(nesSweepReadout(macroControl2), juce::dontSendNotification);
        controlValueLabels[2].setText(nesNoiseReadout(macroControl3), juce::dontSendNotification);
        controlValueLabels[3].setText(nesFocusReadout(macroControl4), juce::dontSendNotification);

        const auto playModeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::playMode)));
        const auto playMode = chipper::parameters::playModeFromChoice(playModeChoice);
        if (playMode == chipper::PlayMode::chipPoly)
        {
            nesChannelLabels[0].setText("Pulse 1  |  note 1", juce::dontSendNotification);
            nesChannelLabels[1].setText("Pulse 2  |  note 2", juce::dontSendNotification);
            nesChannelLabels[2].setText("Triangle | note 3 bass", juce::dontSendNotification);
            nesChannelLabels[3].setText("Noise    | mono SFX layer", juce::dontSendNotification);
        }
        else
        {
            nesChannelLabels[0].setText("Pulse 1  |  duty lead", juce::dontSendNotification);
            nesChannelLabels[1].setText("Pulse 2  |  stack / sweep", juce::dontSendNotification);
            nesChannelLabels[2].setText("Triangle | bass body", juce::dontSendNotification);
            nesChannelLabels[3].setText("Noise    | snare / hats", juce::dontSendNotification);
        }
    }
    else
    {
        controlValueLabels[0].setText(juce::String(macroControl1, 2), juce::dontSendNotification);
        controlValueLabels[1].setText(juce::String(macroControl2, 2), juce::dontSendNotification);
        controlValueLabels[2].setText(juce::String(macroControl3, 2), juce::dontSendNotification);
        controlValueLabels[3].setText(juce::String(macroControl4, 2), juce::dontSendNotification);
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
