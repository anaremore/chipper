#include "PluginEditor.h"

#include "Engine/ChipDescriptors.h"

ChipperAudioProcessorEditor::ChipperAudioProcessorEditor(ChipperAudioProcessor& processor)
    : AudioProcessorEditor(processor),
      audioProcessor(processor)
{
    setSize(760, 520);

    auto& state = audioProcessor.getValueTreeState();

    titleLabel.setText("Chipper", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff7d85a));
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    chipModeBox.addItemList(chipper::parameters::chipModeChoices(), 1);
    accuracyBox.addItemList(chipper::parameters::accuracyChoices(), 1);
    macroBox.addItemList(chipper::parameters::macroChoices(), 1);
    chipModeBox.setTooltip("Selects the chip model or planned chip family.");
    accuracyBox.setTooltip("Selects the requested accuracy tier for the active core.");
    macroBox.setTooltip("Applies a chip-specific musical register template.");
    addAndMakeVisible(chipModeBox);
    addAndMakeVisible(accuracyBox);
    addAndMakeVisible(macroBox);

    chipModeAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::chipMode, chipModeBox);
    accuracyAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::accuracy, accuracyBox);
    macroAttachment = std::make_unique<ComboBoxAttachment>(state, chipper::parameters::id::macro, macroBox);

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

    updateDescriptorText();
    startTimerHz(8);
}

void ChipperAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11161d));
    g.setColour(juce::Colour(0xff26323f));
    g.drawHorizontalLine(70, 16.0f, static_cast<float>(getWidth() - 16));
    g.drawHorizontalLine(getHeight() - 62, 16.0f, static_cast<float>(getWidth() - 16));
}

void ChipperAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto top = area.removeFromTop(46);
    titleLabel.setBounds(top.removeFromLeft(180));
    chipModeBox.setBounds(top.removeFromLeft(220).reduced(0, 6));
    top.removeFromLeft(8);
    accuracyBox.setBounds(top.removeFromLeft(150).reduced(0, 6));
    top.removeFromLeft(8);
    macroBox.setBounds(top.removeFromLeft(150).reduced(0, 6));

    area.removeFromTop(16);
    chipSummaryLabel.setBounds(area.removeFromTop(38));
    area.removeFromTop(10);

    auto utility = area.removeFromTop(76);
    clockSlider.setBounds(utility.removeFromLeft((getWidth() - 40) / 2).reduced(8, 0));
    outputSlider.setBounds(utility.reduced(8, 0));

    area.removeFromTop(18);

    auto controls = area.removeFromTop(230);
    auto row1 = controls.removeFromTop(104);
    nativeSliders[0].setBounds(row1.removeFromLeft((getWidth() - 48) / 2).reduced(8, 0));
    nativeSliders[1].setBounds(row1.reduced(8, 0));
    controls.removeFromTop(16);
    auto row2 = controls.removeFromTop(104);
    nativeSliders[2].setBounds(row2.removeFromLeft((getWidth() - 48) / 2).reduced(8, 0));
    nativeSliders[3].setBounds(row2.reduced(8, 0));

    clockLabel.setBounds(clockSlider.getBounds().withHeight(22).translated(0, -24));
    outputLabel.setBounds(outputSlider.getBounds().withHeight(22).translated(0, -24));
    for (size_t i = 0; i < nativeSliders.size(); ++i)
        nativeLabels[i].setBounds(nativeSliders[i].getBounds().withHeight(22).translated(0, -24));

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
