#include "PluginEditor.h"

#include "Engine/ChipDescriptors.h"

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor)
{
    setSize(900, 680);

    auto& state = audioProcessor.getValueTreeState();

    titleLabel.setText("Chipper", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff7d85a));
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    chipModeBox.addItemList(chipper::parameters::chipModeChoices(), 1);
    accuracyBox.addItemList(chipper::parameters::accuracyChoices(), 1);
    macroBox.addItemList(chipper::parameters::macroChoices(), 1);
    playModeBox.addItemList(chipper::parameters::playModeChoices(), 1);
    chipModeBox.setTooltip("Selects the chip model or planned chip family.");
    accuracyBox.setTooltip("Selects the requested accuracy tier for the active core.");
    macroBox.setTooltip("Applies a chip-specific musical register template.");
    playModeBox.setTooltip("Chooses how incoming notes use the chip channels inside one patch.");
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
        addLabeledSlider(nativeSliders[i], nativeLabels[i], "Native Control");
        nativeSliders[i].setNumDecimalPlacesToDisplay(2);
        nativeAttachments[i] = std::make_unique<SliderAttachment>(state, ids[i], nativeSliders[i]);
    }

    statusLabel.setFont(juce::FontOptions(13.0f));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1f2a34));
    statusLabel.setText("Loading chip core...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    chipSummaryLabel.setFont(juce::FontOptions(14.0f));
    chipSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    chipSummaryLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    addAndMakeVisible(chipSummaryLabel);

    for (size_t i = 0; i < moduleTitleLabels.size(); ++i)
    {
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

    globalStripLabel.setText("Global Performance Controls", juce::dontSendNotification);
    globalStripLabel.setJustificationType(juce::Justification::centredLeft);
    globalStripLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff0c94d));
    globalStripLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(globalStripLabel);

    updateDescriptorText();
    startTimerHz(8);
}

void ChipperAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101414));
    g.setColour(juce::Colour(0xff314047));
    g.drawHorizontalLine(70, 16.0f, static_cast<float>(getWidth() - 16));
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

    auto top = area.removeFromTop(48);
    titleLabel.setBounds(top.removeFromLeft(132));
    chipModeBox.setBounds(top.removeFromLeft(220).reduced(0, 6));
    top.removeFromLeft(8);
    accuracyBox.setBounds(top.removeFromLeft(126).reduced(0, 6));
    top.removeFromLeft(8);
    macroBox.setBounds(top.removeFromLeft(136).reduced(0, 6));
    top.removeFromLeft(8);
    playModeBox.setBounds(top.removeFromLeft(150).reduced(0, 6));

    area.removeFromTop(10);
    chipSummaryLabel.setBounds(area.removeFromTop(34));
    area.removeFromTop(10);

    auto modules = area.removeFromTop(330);
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
        moduleTitleLabels[i].setBounds(panel.removeFromTop(20));
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

    area.removeFromTop(12);
    globalStripBounds = area.removeFromTop(148);
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

    placeLabeledSlider(nativeSliders[0], nativeLabels[0], controlCells[0]);
    placeLabeledSlider(nativeSliders[1], nativeLabels[1], controlCells[1]);
    placeLabeledSlider(nativeSliders[2], nativeLabels[2], controlCells[2]);
    placeLabeledSlider(nativeSliders[3], nativeLabels[3], controlCells[3]);
    placeLabeledSlider(clockSlider, clockLabel, controlCells[4]);
    placeLabeledSlider(outputSlider, outputLabel, controlCells[5]);

    statusLabel.setBounds(getLocalBounds().reduced(16).removeFromBottom(44));
}

void ChipperAudioProcessorEditor::timerCallback()
{
    updateDescriptorText();
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

void ChipperAudioProcessorEditor::updateDescriptorText()
{
    const auto modeChoice = static_cast<int>(std::round(audioProcessor.getValueTreeState()
                                                            .getRawParameterValue(chipper::parameters::id::chipMode)
                                                            ->load()));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    if (mode == displayedMode)
        return;

    displayedMode = mode;
    const auto& descriptor = chipper::descriptorFor(mode);
    chipSummaryLabel.setText(descriptor.summary, juce::dontSendNotification);

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
            nativeLabels[i].setText(descriptor.controls[i].label, juce::dontSendNotification);
            nativeSliders[i].setTooltip(descriptor.controls[i].help);
        }
        else
        {
            nativeLabels[i].setText("Native Control", juce::dontSendNotification);
            nativeSliders[i].setTooltip({});
        }
    }
}
