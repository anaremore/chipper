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

chipper::ChipParameterRole sourceLevelRole(size_t index)
{
    static constexpr std::array<chipper::ChipParameterRole, 4> roles {
        chipper::ChipParameterRole::source1Level,
        chipper::ChipParameterRole::source2Level,
        chipper::ChipParameterRole::source3Level,
        chipper::ChipParameterRole::source4Level
    };

    return roles[std::min(index, roles.size() - 1u)];
}

juce::String choiceTooltip(const chipper::ChipParameterSpec& spec, size_t choiceIndex)
{
    if (choiceIndex < spec.choices.size() && ! spec.choices[choiceIndex].help.empty())
        return juce::String(spec.choices[choiceIndex].help);

    return juce::String(spec.help);
}

juce::String withMidiCc(juce::String text, const char* parameterId)
{
    const auto controller = chipper::parameters::midiControllerForParameterId(parameterId);
    if (controller < 0)
        return text;

    const auto ccText = juce::String("MIDI CC ") + juce::String(controller);
    return text.isEmpty() ? ccText : text + "\n" + ccText;
}

juce::String withMidiCcForRole(juce::String text, chipper::ChipParameterRole role)
{
    return withMidiCc(text, chipper::parameters::parameterIdForChipParameterRole(role));
}

juce::String midiCcRangeLabel()
{
    const auto& mappings = chipper::parameters::midiCcMappings();
    auto minController = 127;
    auto maxController = 0;

    for (const auto& mapping : mappings)
    {
        minController = std::min(minController, mapping.controller);
        maxController = std::max(maxController, mapping.controller);
    }

    return juce::String("MIDI CC ") + juce::String(minController) + "-" + juce::String(maxController);
}

juce::String midiCcMapTooltip()
{
    juce::String text("Fixed MIDI CC map\n");

    for (const auto& mapping : chipper::parameters::midiCcMappings())
        text += juce::String("CC ") + juce::String(mapping.controller) + " - " + juce::String(mapping.label) + "\n";

    text += "\nAll current plugin parameters have default MIDI CC control. Choice/register controls quantize to legal chip values.";
    return text;
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
    chipModeBox.setTooltip(withMidiCc("Selects the chip model or planned chip family.", chipper::parameters::id::chipMode));
    accuracyBox.setTooltip(withMidiCc("Selects the requested accuracy tier for the active core.", chipper::parameters::id::accuracy));
    presetBox.setTooltip("Applies a factory preset for the selected chip mode.");
    macroBox.setTooltip(withMidiCc("Applies a chip-specific musical register template.", chipper::parameters::id::macro));
    playModeBox.setTooltip(withMidiCc("Chooses how incoming notes use the chip channels inside one patch.", chipper::parameters::id::playMode));

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
    clockSlider.setTooltip(withMidiCc("Optional chip clock override. Zero uses the documented default for the selected mode.", chipper::parameters::id::clockHz));
    clockAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::clockHz, clockSlider);

    addLabeledSlider(outputSlider, outputLabel, "Output");
    outputSlider.setTextValueSuffix(" dB");
    outputSlider.setTooltip(withMidiCc("Final plugin output level.", chipper::parameters::id::outputDb));
    outputAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::outputDb, outputSlider);

    addLabeledSlider(stereoSpreadSlider, stereoSpreadLabel, "Stereo Spread");
    stereoSpreadSlider.setNumDecimalPlacesToDisplay(2);
    stereoSpreadSlider.setTooltip(withMidiCc("Modern stereo spread for chips that expose it; zero preserves mono output.", chipper::parameters::id::stereoSpread));
    stereoSpreadAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::stereoSpread, stereoSpreadSlider);

    stereoSpreadValueLabel.setJustificationType(juce::Justification::centredLeft);
    stereoSpreadValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    stereoSpreadValueLabel.setFont(juce::FontOptions(11.0f));
    stereoSpreadValueLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(stereoSpreadValueLabel);

    addLabeledSlider(envelopeDecaySlider, envelopeDecayLabel, "Envelope Decay");
    envelopeDecaySlider.setNumDecimalPlacesToDisplay(2);
    envelopeDecaySlider.setTooltip(withMidiCc("Maps musical decay to the active chip's hardware envelope period. Zero preserves the macro's original envelope/level behavior.", chipper::parameters::id::envelopeDecay));
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

    dmgWaveLevelLabel.setText("Wave Level", juce::dontSendNotification);
    dmgWaveLevelLabel.setJustificationType(juce::Justification::centredLeft);
    dmgWaveLevelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    dmgWaveLevelLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    dmgWaveLevelLabel.setVisible(false);
    addAndMakeVisible(dmgWaveLevelLabel);

    dmgWaveLevelValueLabel.setJustificationType(juce::Justification::centredLeft);
    dmgWaveLevelValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    dmgWaveLevelValueLabel.setFont(juce::FontOptions(11.0f));
    dmgWaveLevelValueLabel.setMinimumHorizontalScale(0.75f);
    dmgWaveLevelValueLabel.setVisible(false);
    addAndMakeVisible(dmgWaveLevelValueLabel);

    const std::array<const char*, dmgWaveLevelCount> dmgWaveLevelLabels { "Macro", "Mute", "100%", "50%", "25%" };
    for (size_t i = 0; i < dmgWaveLevelButtons.size(); ++i)
    {
        auto& button = dmgWaveLevelButtons[i];
        button.setButtonText(dmgWaveLevelLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("DMG NR32 wave level: ") + dmgWaveLevelLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::dmgWaveLevel, static_cast<int>(i));
            updateLiveControlReadouts();
        };
        button.setVisible(false);
        addAndMakeVisible(button);
    }

    dmgStereoRouteLabel.setText("Stereo Route", juce::dontSendNotification);
    dmgStereoRouteLabel.setJustificationType(juce::Justification::centredLeft);
    dmgStereoRouteLabel.setColour(juce::Label::textColourId, juce::Colour(0xffd9e1e8));
    dmgStereoRouteLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    dmgStereoRouteLabel.setVisible(false);
    addAndMakeVisible(dmgStereoRouteLabel);

    dmgStereoRouteValueLabel.setJustificationType(juce::Justification::centredLeft);
    dmgStereoRouteValueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
    dmgStereoRouteValueLabel.setFont(juce::FontOptions(11.0f));
    dmgStereoRouteValueLabel.setMinimumHorizontalScale(0.75f);
    dmgStereoRouteValueLabel.setVisible(false);
    addAndMakeVisible(dmgStereoRouteValueLabel);

    const std::array<const char*, dmgStereoRouteCount> dmgStereoRouteLabels { "Macro", "Both", "Left", "Right", "Split" };
    for (size_t i = 0; i < dmgStereoRouteButtons.size(); ++i)
    {
        auto& button = dmgStereoRouteButtons[i];
        button.setButtonText(dmgStereoRouteLabels[i]);
        button.setClickingTogglesState(false);
        button.setTooltip(juce::String("DMG NR51 stereo route: ") + dmgStereoRouteLabels[i]);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c33));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xfff0c94d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdbe8e5));
        button.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff101414));
        button.onClick = [this, i]()
        {
            setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, static_cast<int>(i));
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

    midiCcLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    midiCcLabel.setJustificationType(juce::Justification::centred);
    midiCcLabel.setColour(juce::Label::textColourId, juce::Colour(0xff56c7d8));
    midiCcLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff18252d));
    midiCcLabel.setText(midiCcRangeLabel(), juce::dontSendNotification);
    midiCcLabel.setTooltip(midiCcMapTooltip());
    addAndMakeVisible(midiCcLabel);

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
    const std::array<const char*, sourceChannelCount> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level
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

        auto& slider = sourceLevelSliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour(juce::Slider::trackColourId, juce::Colour(0xfff7d85a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff56c7d8));
        slider.setTooltip(withMidiCc("Source level trim.", sourceLevelIds[i]));
        slider.setVisible(false);
        addAndMakeVisible(slider);
        sourceLevelAttachments[i] = std::make_unique<SliderAttachment>(state, sourceLevelIds[i], slider);

        auto& valueLabel = sourceLevelValueLabels[i];
        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbc4));
        valueLabel.setFont(juce::FontOptions(10.0f));
        valueLabel.setMinimumHorizontalScale(0.70f);
        valueLabel.setTooltip(withMidiCc("Source level trim.", sourceLevelIds[i]));
        valueLabel.setVisible(false);
        addAndMakeVisible(valueLabel);
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
    const auto sourceCardWidth = (sourcePanel.getWidth() - (sourceGap * static_cast<int>(sourceChannelBounds.size() - 1u))) / static_cast<int>(sourceChannelBounds.size());
    const auto sourceCardHeight = sourcePanel.getHeight();
    for (size_t i = 0; i < sourceChannelBounds.size(); ++i)
    {
        sourceChannelBounds[i] = {
            sourcePanel.getX() + (static_cast<int>(i) * (sourceCardWidth + sourceGap)),
            sourcePanel.getY(),
            sourceCardWidth,
            sourceCardHeight
        };
        auto sourceCard = sourceChannelBounds[i].reduced(8, 4);
        sourceChannelButtons[i].setBounds(sourceCard.removeFromTop(std::min(18, sourceCard.getHeight())));
        sourceCard.removeFromTop(2);
        sourceLevelSliders[i].setBounds(sourceCard.removeFromTop(std::min(14, sourceCard.getHeight())).reduced(0, 1));
        sourceCard.removeFromTop(1);
        sourceLevelValueLabels[i].setBounds(sourceCard.removeFromTop(std::min(12, sourceCard.getHeight())));
    }

    auto tonePanel = moduleBounds[2].reduced(12, 9);
    tonePanel.removeFromTop(20);
    tonePanel.removeFromTop(30);
    tonePanel.removeFromTop(4);
    auto primaryTonePanel = tonePanel;
    auto secondaryTonePanel = tonePanel;
    if (displayedMode == chipper::ChipMode::dmg || displayedMode == chipper::ChipMode::sid)
    {
        primaryTonePanel = tonePanel.removeFromTop(58);
        tonePanel.removeFromTop(6);
        secondaryTonePanel = tonePanel;
    }

    placeWaveShapeSegment(primaryTonePanel);
    placeDmgWaveLevelSegment(secondaryTonePanel);
    placeYmEnvelopeShapeSegment(displayedMode == chipper::ChipMode::sid ? secondaryTonePanel : primaryTonePanel);

    auto motionPanel = moduleBounds[4].reduced(12, 9);
    motionPanel.removeFromTop(20);
    motionPanel.removeFromTop(30);
    motionPanel.removeFromTop(4);
    placeSnNoiseModeSegment(displayedMode == chipper::ChipMode::sid ? motionPanel : primaryTonePanel);

    auto envelopePanel = moduleBounds[3].reduced(12, 9);
    envelopePanel.removeFromTop(20);
    envelopePanel.removeFromTop(30);
    envelopePanel.removeFromTop(4);
    auto envelopeDecayPanel = envelopePanel;
    envelopeDecayPanel.removeFromBottom(6);
    placeLabeledSliderWithReadout(envelopeDecaySlider, envelopeDecayLabel, envelopeDecayValueLabel, envelopeDecayPanel);

    auto outputPanel = moduleBounds[5].reduced(12, 9);
    outputPanel.removeFromTop(20);
    outputPanel.removeFromTop(30);
    outputPanel.removeFromTop(4);
    outputPanel.removeFromBottom(6);
    placeLabeledSliderWithReadout(stereoSpreadSlider, stereoSpreadLabel, stereoSpreadValueLabel, outputPanel);

    auto profilePanel = moduleBounds[0].reduced(12, 9);
    profilePanel.removeFromTop(20);
    profilePanel.removeFromTop(30);
    profilePanel.removeFromTop(4);
    placeDmgStereoRouteSegment(displayedMode == chipper::ChipMode::sid ? profilePanel : outputPanel);

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
    midiCcLabel.setBounds(footer.removeFromRight(136));
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

void ChipperAudioProcessorEditor::placeDmgWaveLevelSegment(juce::Rectangle<int> bounds)
{
    dmgWaveLevelLabel.setBounds(bounds.removeFromTop(18));
    dmgWaveLevelSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(dmgWaveLevelButtons, dmgWaveLevelSegmentBounds, dmgWaveLevelButtons.size());

    dmgWaveLevelValueLabel.setBounds(bounds);
}

void ChipperAudioProcessorEditor::placeDmgStereoRouteSegment(juce::Rectangle<int> bounds)
{
    dmgStereoRouteLabel.setBounds(bounds.removeFromTop(18));
    dmgStereoRouteSegmentBounds = bounds.removeFromTop(28).reduced(0, 1);
    layoutSegmentedButtons(dmgStereoRouteButtons, dmgStereoRouteSegmentBounds, dmgStereoRouteButtons.size());

    dmgStereoRouteValueLabel.setBounds(bounds);
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
            buttons[i].setTooltip(withMidiCcForRole(choiceTooltip(*spec, i), spec->role));
        }
    };

    applyChoices(pulseDutyButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::macroControl1));
    applyChoices(toneNoiseMixButtons, chipper::parameterSpecFor(mode, chipper::ChipParameterRole::macroControl4));

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::waveShape))
    {
        waveShapeLabel.setText(spec->label, juce::dontSendNotification);
        waveShapeLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        waveShapeValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(waveShapeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgWaveLevel))
    {
        dmgWaveLevelLabel.setText(spec->label, juce::dontSendNotification);
        dmgWaveLevelLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        dmgWaveLevelValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(dmgWaveLevelButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgStereoRoute))
    {
        dmgStereoRouteLabel.setText(spec->label, juce::dontSendNotification);
        dmgStereoRouteLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        dmgStereoRouteValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(dmgStereoRouteButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::ymEnvelopeShape))
    {
        ymEnvelopeShapeLabel.setText(spec->label, juce::dontSendNotification);
        ymEnvelopeShapeLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        ymEnvelopeShapeValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(ymEnvelopeShapeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::snNoiseMode))
    {
        snNoiseModeLabel.setText(spec->label, juce::dontSendNotification);
        snNoiseModeLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        snNoiseModeValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        applyChoices(snNoiseModeButtons, spec);
    }

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::envelopeDecay))
    {
        envelopeDecayLabel.setText(spec->label, juce::dontSendNotification);
        envelopeDecayLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        envelopeDecaySlider.setTooltip(withMidiCcForRole(spec->help, spec->role));
        envelopeDecayValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
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
    const std::array<const char*, 4> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level
    };

    const juce::ScopedValueSetter<bool> suppress(suppressMacroTemplateApply, true);
    for (size_t i = 0; i < ids.size(); ++i)
        setParameterValueFromUi(ids[i], templ.controls[i]);
    for (size_t i = 0; i < sourceIds.size(); ++i)
        setParameterValueFromUi(sourceIds[i], templ.sourceEnabled[i] ? 1.0f : 0.0f);
    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setParameterValueFromUi(sourceLevelIds[i], 1.0f);
    setParameterValueFromUi(chipper::parameters::id::envelopeDecay, templ.envelopeDecay);
    setParameterValueFromUi(chipper::parameters::id::stereoSpread, templ.stereoSpread);
    setChoiceParameterFromUi(chipper::parameters::id::waveShape, templ.waveShape);
    setChoiceParameterFromUi(chipper::parameters::id::dmgWaveLevel, 0);
    setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, templ.dmgStereoRoute);
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
    const std::array<const char*, 4> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level
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

    for (size_t i = 0; i < sourceLevelIds.size(); ++i)
        setParameterValueFromUi(sourceLevelIds[i], 1.0f);

    setParameterValueFromUi(chipper::parameters::id::envelopeDecay, preset.envelopeDecay);
    setParameterValueFromUi(chipper::parameters::id::stereoSpread, preset.stereoSpread);
    setChoiceParameterFromUi(chipper::parameters::id::waveShape, preset.waveShape);
    setChoiceParameterFromUi(chipper::parameters::id::dmgWaveLevel, 0);
    setChoiceParameterFromUi(chipper::parameters::id::dmgStereoRoute, preset.dmgStereoRoute);
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
                                                                 int dmgWaveLevel,
                                                                 int dmgStereoRoute,
                                                                 int ymEnvelopeShape,
                                                                 int snNoiseMode,
                                                                 float stereoSpread) const
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
        {
            parameterValue(chipper::parameters::id::source1Level),
            parameterValue(chipper::parameters::id::source2Level),
            parameterValue(chipper::parameters::id::source3Level),
            parameterValue(chipper::parameters::id::source4Level)
        },
        stereoSpread,
        parameterValue(chipper::parameters::id::envelopeDecay),
        waveShape,
        dmgWaveLevel,
        dmgStereoRoute,
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

bool ChipperAudioProcessorEditor::usesDmgWaveLevelSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgWaveLevel,
                                            chipper::ControlSurface::segmentedChoice);
}

bool ChipperAudioProcessorEditor::usesDmgStereoRouteSegment(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::dmgStereoRoute,
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
        return label + " -> " + pulseDutyReadout(mode, patch.control1) + " | " + dmgWaveLevelReadout(patch) + " | " + dmgStereoRouteReadout(patch);

    if (mode == chipper::ChipMode::ym2149)
        return label + " -> " + ymSpreadReadout(patch.control1) + " | " + ymNoiseReadout(patch.control3) + " | " + ymToneNoiseReadout(patch.control4);

    if (mode == chipper::ChipMode::sn76489)
        return label + " -> " + snStackReadout(patch.control1) + " | " + snNoiseModeReadout(patch) + " | " + snLevelReadout(patch.control4);

    if (mode == chipper::ChipMode::sid)
        return label + " -> " + sidModelReadout(patch) + " | " + waveShapeReadout(mode, patch.waveShape) + " | " + sidFilterModeReadout(patch) + " | " + sidModModeReadout(patch);

    return label + ": " + juce::String(templ.help);
}

juce::String ChipperAudioProcessorEditor::pulseDutyReadout(chipper::ChipMode mode, float value) const
{
    static constexpr std::array<const char*, 4> nesDutyLabels { "12.5% narrow pulse", "25% hollow pulse", "50% square", "75% inverted pulse" };
    static constexpr std::array<const char*, 4> dmgDutyLabels { "12.5% thin pulse", "25% narrow pulse", "50% square", "75% wide pulse" };
    const auto index = static_cast<size_t>(std::clamp(static_cast<int>(std::round(value * 3.0f)), 0, 3));
    return mode == chipper::ChipMode::dmg ? dmgDutyLabels[index] : nesDutyLabels[index];
}

juce::String ChipperAudioProcessorEditor::waveShapeReadout(chipper::ChipMode mode, int choice) const
{
    if (mode == chipper::ChipMode::sid)
    {
        switch (std::clamp(choice, 0, 4))
        {
            case 1: return "CTRL waveform bit 0x10, triangle";
            case 2: return "CTRL waveform bit 0x20, saw";
            case 3: return "CTRL waveform bit 0x40, pulse";
            case 4: return "CTRL waveform bit 0x80, noise";
            case 0:
            default:
                return "Macro waveform: resolves to SID CTRL bits";
        }
    }

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

juce::String ChipperAudioProcessorEditor::dmgWaveLevelReadout(const chipper::PatchConfig& patch) const
{
    const auto bits = chipper::dmgWaveOutputLevelBitsForPatch(patch, 1.0f, false);
    const auto registerText = juce::String("NR32 bits ") + juce::String(static_cast<int>((bits >> 5u) & 0x03u));
    juce::String resolvedText;
    switch (bits)
    {
        case 0x00u: resolvedText = registerText + ", muted"; break;
        case 0x20u: resolvedText = registerText + ", 100%"; break;
        case 0x60u: resolvedText = registerText + ", 25%"; break;
        case 0x40u:
        default:
            resolvedText = registerText + ", 50%";
            break;
    }

    return patch.dmgWaveLevel == 0 ? juce::String("Macro -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::dmgStereoRouteReadout(const chipper::PatchConfig& patch) const
{
    const auto routeRegister = chipper::dmgStereoRouteRegisterForPatch(patch);
    const auto registerText = juce::String("NR51=0x") + juce::String::toHexString(static_cast<int>(routeRegister)).paddedLeft('0', 2).toUpperCase();
    juce::String routeText;
    switch (routeRegister)
    {
        case 0xffu: routeText = "all channels both sides"; break;
        case 0xf0u: routeText = "all channels left"; break;
        case 0x0fu: routeText = "all channels right"; break;
        case 0x5au: routeText = "P1+Wave left, P2+Noise right"; break;
        default:
            routeText = "custom route bits";
            break;
    }

    const auto resolvedText = registerText + ", " + routeText;
    return patch.dmgStereoRoute == 0 ? juce::String("Macro -> ") + resolvedText : resolvedText;
}

juce::String ChipperAudioProcessorEditor::sidModelReadout(const chipper::PatchConfig& patch) const
{
    const auto model = chipper::sidModelNumberForPatch(patch);
    const auto detail = model == 8580 ? "cleaner/brighter filter profile" : "warmer/rougher filter profile";
    const auto text = juce::String("SID ") + juce::String(model) + ", " + detail;
    return std::clamp(patch.dmgStereoRoute, 0, 2) == 0 ? juce::String("Macro -> ") + text : text;
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

juce::String ChipperAudioProcessorEditor::sidFilterModeReadout(const chipper::PatchConfig& patch) const
{
    const auto modeBits = chipper::sidFilterModeBitsForPatch(patch);
    juce::String resolved;
    switch (modeBits)
    {
        case 0x10u: resolved = "LP"; break;
        case 0x20u: resolved = "BP"; break;
        case 0x40u: resolved = "HP"; break;
        case 0x00u:
        default:
            resolved = "bypass";
            break;
    }

    const auto registerText = juce::String("$D418 bits 0x")
        + juce::String::toHexString(static_cast<int>(modeBits)).paddedLeft('0', 2).toUpperCase();
    const auto text = registerText + ", " + resolved;
    return patch.ymEnvelopeShape == 0 ? juce::String("Macro -> ") + text : text;
}

juce::String ChipperAudioProcessorEditor::sidModModeReadout(const chipper::PatchConfig& patch) const
{
    const auto bits = chipper::sidModulationBitsForPatch(patch, 1);
    juce::String resolved;
    switch (bits)
    {
        case 0x02u: resolved = "sync on voices 2/3"; break;
        case 0x04u: resolved = "ring on voices 2/3"; break;
        case 0x06u: resolved = "sync + ring on voices 2/3"; break;
        case 0x00u:
        default:
            resolved = "off";
            break;
    }

    const auto registerText = juce::String("CTRL bits 0x")
        + juce::String::toHexString(static_cast<int>(bits)).paddedLeft('0', 2).toUpperCase();
    const auto text = registerText + ", " + resolved;
    return patch.snNoiseMode == 0 ? juce::String("Macro -> ") + text : text;
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

    if (mode == chipper::ChipMode::sid)
        return sidModModeReadout(patch);

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

juce::String ChipperAudioProcessorEditor::stereoSpreadReadout(chipper::ChipMode mode, float value) const
{
    const auto spread = std::clamp(value, 0.0f, 1.0f);
    if (mode == chipper::ChipMode::sid)
    {
        const auto resonance = chipper::sidFilterResonanceForControl(spread);
        const auto registerValue = static_cast<int>((resonance << 4u) | 0x07u);
        return juce::String("$D417=0x")
             + juce::String::toHexString(registerValue).paddedLeft('0', 2).toUpperCase()
             + ", resonance " + juce::String(static_cast<int>(resonance)) + "/15";
    }

    if (spread <= 0.01f)
        return "Mono: authentic chip output width";

    const auto percent = static_cast<int>(std::round(spread * 100.0f));
    if (mode == chipper::ChipMode::ym2149)
        return juce::String(percent) + "%: A left, B center, C right";
    if (mode == chipper::ChipMode::sn76489)
        return juce::String(percent) + "%: Tone 1 left, Tone 3 right";

    return juce::String(percent) + "% modern stereo spread";
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
    const auto period = chipper::nesNoisePeriodForControl(value);
    if (value < 0.25f)
        return juce::String("$400E period ") + juce::String(static_cast<int>(period)) + ", low grit";
    if (value < 0.60f)
        return juce::String("$400E period ") + juce::String(static_cast<int>(period)) + ", snare noise";
    return juce::String("$400E period ") + juce::String(static_cast<int>(period)) + ", short hats";
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
    const auto reg = chipper::dmgSweepRegisterForControl(value);
    const auto shift = chipper::dmgSweepShiftForControl(value);
    const auto prefix = juce::String("NR10 0x") + juce::String::toHexString(static_cast<int>(reg)).toUpperCase()
        + juce::String(", shift ") + juce::String(static_cast<int>(shift));
    if (shift == 0)
        return prefix + ", mostly off";
    if (shift <= 2)
        return prefix + ", small sweep";
    if (shift <= 5)
        return prefix + ", pitch bend";
    return prefix + ", fast sweep";
}

juce::String ChipperAudioProcessorEditor::dmgNoiseReadout(float value) const
{
    const auto shift = chipper::dmgNoiseClockShiftForControl(value);
    return juce::String("NR43 clock shift ") + juce::String(static_cast<int>(shift)) + ", divisor code 2";
}

juce::String ChipperAudioProcessorEditor::dmgEnvelopeReadout(float value) const
{
    const auto level = chipper::dmgInitialEnvelopeLevelForControl(value);
    return juce::String("NRx2 initial volume ") + juce::String(static_cast<int>(level)) + "/15";
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
    const auto period = chipper::ym2149NoisePeriodForControl(value);
    return juce::String("Reg 6 period ") + juce::String(static_cast<int>(period)) + "/31";
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
    const auto attenuation = chipper::sn76489NoiseAttenuationForControl(value);
    const auto attenuationText = juce::String("Attenuation reg ") + juce::String(static_cast<int>(attenuation)) + "/15";
    if (attenuation == 15u)
        return attenuationText + ", muted";
    if (attenuation == 0u)
        return attenuationText + ", full level";

    return attenuationText + juce::String(", -") + juce::String(static_cast<int>(attenuation) * 2) + " dB";
}

juce::String ChipperAudioProcessorEditor::sidPulseWidthReadout(float value) const
{
    const auto pulseWidth = chipper::sidPulseWidthForControl(value);
    const auto percent = static_cast<int>(std::round((static_cast<float>(pulseWidth) / 4095.0f) * 100.0f));
    return juce::String("PW ") + juce::String(static_cast<int>(pulseWidth)) + "/4095, " + juce::String(percent) + "%";
}

juce::String ChipperAudioProcessorEditor::sidDetuneReadout(float value) const
{
    const auto semitones = std::clamp(static_cast<int>(std::round(value * 12.0f)), 0, 12);
    return juce::String("Voice spread ") + juce::String(semitones) + " semitones";
}

juce::String ChipperAudioProcessorEditor::sidCutoffReadout(float value) const
{
    const auto cutoff = std::clamp(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 2047.0f)), 0, 2047);
    return juce::String("FC registers ") + juce::String(cutoff) + "/2047";
}

juce::String ChipperAudioProcessorEditor::sidSustainReadout(float value) const
{
    const auto sustain = std::clamp(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 15.0f)), 0, 15);
    return juce::String("SR sustain nibble ") + juce::String(sustain) + "/15";
}

juce::String ChipperAudioProcessorEditor::sourceLevelReadout(size_t index) const
{
    static constexpr std::array<const char*, sourceChannelCount> sourceIds {
        chipper::parameters::id::source1Enabled,
        chipper::parameters::id::source2Enabled,
        chipper::parameters::id::source3Enabled,
        chipper::parameters::id::source4Enabled
    };
    static constexpr std::array<const char*, sourceChannelCount> sourceLevelIds {
        chipper::parameters::id::source1Level,
        chipper::parameters::id::source2Level,
        chipper::parameters::id::source3Level,
        chipper::parameters::id::source4Level
    };

    const auto safeIndex = std::min(index, sourceLevelIds.size() - 1u);
    const auto enabled = parameterValue(sourceIds[safeIndex]) >= 0.5f;
    const auto level = std::clamp(parameterValue(sourceLevelIds[safeIndex]), 0.0f, 1.0f);
    const auto percent = static_cast<int>(std::round(level * 100.0f));

    juce::String text;
    if (! enabled)
        text = "Off / ";

    if (level <= 0.0001f)
        return text + "0% / silent";

    const auto db = 20.0f * std::log10(level);
    return text + juce::String(percent) + "% / " + (std::abs(db) < 0.05f ? juce::String("0 dB") : juce::String(db, 1) + " dB");
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

bool ChipperAudioProcessorEditor::usesStereoSpreadControl(chipper::ChipMode mode) const
{
    return chipper::chipHasParameterSurface(mode,
                                            chipper::ChipParameterRole::stereoSpread,
                                            chipper::ControlSurface::slider);
}

void ChipperAudioProcessorEditor::setSourceChannelSurfaceVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesSourceChannelSurface(mode);
    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        const auto hasSource = chipper::parameterSpecFor(mode, sourceRole(i)) != nullptr;
        const auto visible = active && hasSource;
        sourceChannelButtons[i].setVisible(visible);
        sourceLevelSliders[i].setVisible(visible && chipper::parameterSpecFor(mode, sourceLevelRole(i)) != nullptr);
        sourceLevelValueLabels[i].setVisible(visible && chipper::parameterSpecFor(mode, sourceLevelRole(i)) != nullptr);
    }

    for (auto& itemLabel : moduleItemLabels[1])
        itemLabel.setVisible(! active && ! itemLabel.getText().isEmpty());

    if (active)
        updateSourceChannelButtons(mode);
}

void ChipperAudioProcessorEditor::setStereoSpreadControlVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesStereoSpreadControl(mode);
    stereoSpreadLabel.setVisible(active);
    stereoSpreadSlider.setVisible(active);
    stereoSpreadValueLabel.setVisible(active);
    stereoSpreadLabel.setEnabled(active);
    stereoSpreadSlider.setEnabled(active);
    stereoSpreadValueLabel.setEnabled(active);
    stereoSpreadLabel.setAlpha(active ? 1.0f : 0.55f);
    stereoSpreadSlider.setAlpha(active ? 1.0f : 0.55f);
    stereoSpreadValueLabel.setAlpha(active ? 1.0f : 0.55f);

    if (const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::stereoSpread))
    {
        stereoSpreadLabel.setText(spec->label, juce::dontSendNotification);
        stereoSpreadLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
        stereoSpreadSlider.setTooltip(withMidiCcForRole(spec->help, spec->role));
        stereoSpreadValueLabel.setTooltip(withMidiCcForRole(spec->help, spec->role));
    }

    if (active)
        updateStereoSpreadReadout(mode);
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

void ChipperAudioProcessorEditor::setDmgWaveLevelSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesDmgWaveLevelSegment(mode);
    dmgWaveLevelLabel.setVisible(active);
    dmgWaveLevelValueLabel.setVisible(active);
    for (auto& button : dmgWaveLevelButtons)
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
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateDmgWaveLevelButtons(patch, true);
    }
}

void ChipperAudioProcessorEditor::setDmgStereoRouteSegmentVisible(chipper::ChipMode mode, bool shouldBeVisible)
{
    const auto active = shouldBeVisible && usesDmgStereoRouteSegment(mode);
    dmgStereoRouteLabel.setVisible(active);
    dmgStereoRouteValueLabel.setVisible(active);
    for (auto& button : dmgStereoRouteButtons)
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
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateDmgStereoRouteButtons(mode, patch, true);
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
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        updateYmEnvelopeShapeButtons(mode, patch, true);
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
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
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
    static const std::array<const char*, sourceChannelCount> sidBigMonoLabels {
        "Voice 1 | lead",
        "Voice 2 | detune",
        "Voice 3 | stack / noise",
        "Ext In  | planned"
    };
    static const std::array<const char*, sourceChannelCount> sidChipPolyLabels {
        "Voice 1 | note 1",
        "Voice 2 | note 2",
        "Voice 3 | note 3",
        "Ext In  | planned"
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
    else if (mode == chipper::ChipMode::sid)
        labels = playMode == chipper::PlayMode::chipPoly ? &sidChipPolyLabels : &sidBigMonoLabels;

    for (size_t i = 0; i < sourceChannelButtons.size(); ++i)
    {
        const auto* spec = chipper::parameterSpecFor(mode, sourceRole(i));
        const auto* levelSpec = chipper::parameterSpecFor(mode, sourceLevelRole(i));
        sourceChannelButtons[i].setButtonText(spec != nullptr ? spec->label : (*labels)[i]);
        if (spec != nullptr)
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(juce::String(spec->label) + ": " + juce::String(spec->help), sourceRole(i)));
        else
            sourceChannelButtons[i].setTooltip(withMidiCcForRole(juce::String("Enable or mute ") + (*labels)[i], sourceRole(i)));

        if (levelSpec != nullptr)
            sourceLevelSliders[i].setTooltip(withMidiCcForRole(juce::String(levelSpec->label) + ": " + juce::String(levelSpec->help), sourceLevelRole(i)));
        else
            sourceLevelSliders[i].setTooltip(withMidiCcForRole(juce::String("Trim level for ") + (*labels)[i], sourceLevelRole(i)));

        sourceLevelValueLabels[i].setTooltip(sourceLevelSliders[i].getTooltip());
        sourceLevelValueLabels[i].setText(sourceLevelReadout(i), juce::dontSendNotification);
    }
}

void ChipperAudioProcessorEditor::updateEnvelopeDecayReadout(chipper::ChipMode mode)
{
    envelopeDecayValueLabel.setText(envelopeDecayReadout(mode, parameterValue(chipper::parameters::id::envelopeDecay)), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateStereoSpreadReadout(chipper::ChipMode mode)
{
    stereoSpreadValueLabel.setText(stereoSpreadReadout(mode, parameterValue(chipper::parameters::id::stereoSpread)), juce::dontSendNotification);
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

    if (mode == chipper::ChipMode::ym2149)
    {
        const auto period = chipper::ym2149EnvelopePeriodForControl(value);
        if (value <= 0.01f)
            return juce::String("Default register period ") + juce::String(static_cast<int>(period));

        return juce::String("Reg 11/12 period ") + juce::String(static_cast<int>(period));
    }

    if (mode == chipper::ChipMode::sid)
    {
        const auto patch = currentUiPatch(
            mode,
            parameterValue(chipper::parameters::id::macroControl1),
            parameterValue(chipper::parameters::id::macroControl2),
            parameterValue(chipper::parameters::id::macroControl3),
            parameterValue(chipper::parameters::id::macroControl4),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::waveShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape))),
            static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode))),
            parameterValue(chipper::parameters::id::stereoSpread));
        const auto ad = chipper::sidAttackDecayForPatch(patch);
        const auto sr = chipper::sidSustainReleaseForPatch(patch);
        return juce::String("AD=0x") + juce::String::toHexString(static_cast<int>(ad)).paddedLeft('0', 2).toUpperCase()
             + ", SR=0x" + juce::String::toHexString(static_cast<int>(sr)).paddedLeft('0', 2).toUpperCase();
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
    const auto modeChoice = static_cast<int>(std::round(parameterValue(chipper::parameters::id::chipMode)));
    const auto mode = chipper::parameters::chipModeFromChoice(modeChoice);
    const auto selected = static_cast<size_t>(std::clamp(choice, 0, static_cast<int>(waveShapeButtons.size() - 1u)));
    for (size_t i = 0; i < waveShapeButtons.size(); ++i)
    {
        waveShapeButtons[i].setVisible(shouldBeVisible);
        waveShapeButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    waveShapeValueLabel.setVisible(shouldBeVisible);
    waveShapeValueLabel.setText(waveShapeReadout(mode, static_cast<int>(selected)), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateDmgWaveLevelButtons(const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(patch.dmgWaveLevel, 0, static_cast<int>(dmgWaveLevelButtons.size() - 1u)));
    for (size_t i = 0; i < dmgWaveLevelButtons.size(); ++i)
    {
        dmgWaveLevelButtons[i].setVisible(shouldBeVisible);
        dmgWaveLevelButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    dmgWaveLevelValueLabel.setVisible(shouldBeVisible);
    dmgWaveLevelValueLabel.setText(dmgWaveLevelReadout(patch), juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateDmgStereoRouteButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto* spec = chipper::parameterSpecFor(mode, chipper::ChipParameterRole::dmgStereoRoute);
    const auto choiceCount = spec == nullptr || spec->choices.empty()
                                 ? dmgStereoRouteButtons.size()
                                 : std::min(dmgStereoRouteButtons.size(), spec->choices.size());
    const auto selected = static_cast<size_t>(std::clamp(patch.dmgStereoRoute, 0, static_cast<int>(choiceCount - 1u)));
    layoutSegmentedButtons(dmgStereoRouteButtons, dmgStereoRouteSegmentBounds, choiceCount);
    for (size_t i = 0; i < dmgStereoRouteButtons.size(); ++i)
    {
        const auto visible = shouldBeVisible && i < choiceCount;
        dmgStereoRouteButtons[i].setVisible(visible);
        dmgStereoRouteButtons[i].setToggleState(visible && i == selected, juce::dontSendNotification);
    }

    dmgStereoRouteValueLabel.setVisible(shouldBeVisible);
    dmgStereoRouteValueLabel.setText(mode == chipper::ChipMode::sid
                                         ? sidModelReadout(patch)
                                         : dmgStereoRouteReadout(patch),
                                     juce::dontSendNotification);
}

void ChipperAudioProcessorEditor::updateYmEnvelopeShapeButtons(chipper::ChipMode mode, const chipper::PatchConfig& patch, bool shouldBeVisible)
{
    const auto selected = static_cast<size_t>(std::clamp(patch.ymEnvelopeShape, 0, static_cast<int>(ymEnvelopeShapeButtons.size() - 1u)));
    for (size_t i = 0; i < ymEnvelopeShapeButtons.size(); ++i)
    {
        ymEnvelopeShapeButtons[i].setVisible(shouldBeVisible);
        ymEnvelopeShapeButtons[i].setToggleState(shouldBeVisible && i == selected, juce::dontSendNotification);
    }

    ymEnvelopeShapeValueLabel.setVisible(shouldBeVisible);
    ymEnvelopeShapeValueLabel.setText(mode == chipper::ChipMode::sid
                                          ? sidFilterModeReadout(patch)
                                          : ymEnvelopeShapeReadout(static_cast<int>(selected)),
                                      juce::dontSendNotification);
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
            nativeSliders[i].setTooltip(withMidiCcForRole(spec != nullptr ? spec->help : descriptor.controls[i].help, macroControlRole(i)));
            controlValueLabels[i].setTooltip(withMidiCcForRole(spec != nullptr ? spec->help : descriptor.controls[i].help, macroControlRole(i)));
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
    setDmgWaveLevelSegmentVisible(mode, usesDmgWaveLevelSegment(mode) && hasLiveCore);
    setDmgStereoRouteSegmentVisible(mode, usesDmgStereoRouteSegment(mode) && hasLiveCore);
    setYmEnvelopeShapeSegmentVisible(mode, usesYmEnvelopeShapeSegment(mode) && hasLiveCore);
    setSnNoiseModeSegmentVisible(mode, usesSnNoiseModeSegment(mode) && hasLiveCore);
    setEnvelopeDecayControlVisible(mode, usesEnvelopeDecayControl(mode) && hasLiveCore);
    setStereoSpreadControlVisible(mode, usesStereoSpreadControl(mode) && hasLiveCore);
    const auto hasCustomProfileSurface = hasLiveCore && mode == chipper::ChipMode::sid && usesDmgStereoRouteSegment(mode);
    for (auto& itemLabel : moduleItemLabels[0])
        itemLabel.setVisible(! hasCustomProfileSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomToneSurface = hasLiveCore && (usesWaveShapeSegment(mode)
        || usesDmgWaveLevelSegment(mode)
        || (mode != chipper::ChipMode::sid && usesSnNoiseModeSegment(mode))
        || usesYmEnvelopeShapeSegment(mode));
    for (auto& itemLabel : moduleItemLabels[2])
        itemLabel.setVisible(! hasCustomToneSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomEnvelopeSurface = hasLiveCore && usesEnvelopeDecayControl(mode);
    for (auto& itemLabel : moduleItemLabels[3])
        itemLabel.setVisible(! hasCustomEnvelopeSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomMotionSurface = hasLiveCore && mode == chipper::ChipMode::sid && usesSnNoiseModeSegment(mode);
    for (auto& itemLabel : moduleItemLabels[4])
        itemLabel.setVisible(! hasCustomMotionSurface && ! itemLabel.getText().isEmpty());
    const auto hasCustomOutputSurface = hasLiveCore && (usesStereoSpreadControl(mode) || (mode != chipper::ChipMode::sid && usesDmgStereoRouteSegment(mode)));
    for (auto& itemLabel : moduleItemLabels[5])
        itemLabel.setVisible(! hasCustomOutputSurface && ! itemLabel.getText().isEmpty());
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
    const auto dmgWaveLevel = static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgWaveLevel)));
    const auto dmgStereoRoute = static_cast<int>(std::round(parameterValue(chipper::parameters::id::dmgStereoRoute)));
    const auto ymEnvelopeShape = static_cast<int>(std::round(parameterValue(chipper::parameters::id::ymEnvelopeShape)));
    const auto snNoiseMode = static_cast<int>(std::round(parameterValue(chipper::parameters::id::snNoiseMode)));
    const auto stereoSpread = parameterValue(chipper::parameters::id::stereoSpread);
    const auto patch = currentUiPatch(mode, macroControl1, macroControl2, macroControl3, macroControl4, waveShape, dmgWaveLevel, dmgStereoRoute, ymEnvelopeShape, snNoiseMode, stereoSpread);
    const auto macroText = macroTemplateReadout(mode, patch);
    macroSummaryLabel.setText(macroText, juce::dontSendNotification);
    macroBox.setTooltip(withMidiCc(macroText, chipper::parameters::id::macro));

    const auto hasPulseDutySegment = usesPulseDutySegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasWaveShapeSegment = usesWaveShapeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasDmgWaveLevelSegment = usesDmgWaveLevelSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasDmgStereoRouteSegment = usesDmgStereoRouteSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasYmEnvelopeShapeSegment = usesYmEnvelopeShapeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasSnNoiseModeSegment = usesSnNoiseModeSegment(mode) && chipper::descriptorFor(mode).implemented;
    const auto hasToneNoiseMixSegment = usesToneNoiseMixSegment(mode) && chipper::descriptorFor(mode).implemented;
    nativeSliders[0].setVisible(! hasPulseDutySegment);
    nativeSliders[3].setVisible(! hasToneNoiseMixSegment);
    updatePulseDutyButtons(patch.control1, hasPulseDutySegment);
    updateToneNoiseMixButtons(patch.control4, hasToneNoiseMixSegment);
    updateWaveShapeButtons(waveShape, hasWaveShapeSegment);
    updateDmgWaveLevelButtons(patch, hasDmgWaveLevelSegment);
    updateDmgStereoRouteButtons(mode, patch, hasDmgStereoRouteSegment);
    updateYmEnvelopeShapeButtons(mode, patch, hasYmEnvelopeShapeSegment);
    updateSnNoiseModeButtons(mode, patch, hasSnNoiseModeSegment);
    updateEnvelopeDecayReadout(mode);
    updateStereoSpreadReadout(mode);

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
    else if (mode == chipper::ChipMode::sid)
    {
        controlValueLabels[0].setText(sidPulseWidthReadout(patch.control1), juce::dontSendNotification);
        controlValueLabels[1].setText(sidDetuneReadout(patch.control2), juce::dontSendNotification);
        controlValueLabels[2].setText(sidCutoffReadout(patch.control3), juce::dontSendNotification);
        controlValueLabels[3].setText(sidSustainReadout(patch.control4), juce::dontSendNotification);
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
