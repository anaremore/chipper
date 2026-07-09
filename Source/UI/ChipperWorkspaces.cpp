#include "ChipperWorkspaces.h"

#include <algorithm>

namespace
{
constexpr std::array<const char*, 9> sourceEnableIds {
    chipper::parameters::id::source1Enabled,
    chipper::parameters::id::source2Enabled,
    chipper::parameters::id::source3Enabled,
    chipper::parameters::id::source4Enabled,
    chipper::parameters::id::source5Enabled,
    chipper::parameters::id::source6Enabled,
    chipper::parameters::id::source7Enabled,
    chipper::parameters::id::source8Enabled,
    chipper::parameters::id::source9Enabled
};

constexpr std::array<const char*, 9> sourceLevelIds {
    chipper::parameters::id::source1Level,
    chipper::parameters::id::source2Level,
    chipper::parameters::id::source3Level,
    chipper::parameters::id::source4Level,
    chipper::parameters::id::source5Level,
    chipper::parameters::id::source6Level,
    chipper::parameters::id::source7Level,
    chipper::parameters::id::source8Level,
    chipper::parameters::id::source9Level
};

constexpr std::array<const char*, 4> macroIds {
    chipper::parameters::id::macroControl1,
    chipper::parameters::id::macroControl2,
    chipper::parameters::id::macroControl3,
    chipper::parameters::id::macroControl4
};

constexpr std::array<const char*, 8> perVoiceSampleIds {
    chipper::parameters::id::spc700Voice1SampleSlot,
    chipper::parameters::id::spc700Voice2SampleSlot,
    chipper::parameters::id::spc700Voice3SampleSlot,
    chipper::parameters::id::spc700Voice4SampleSlot,
    chipper::parameters::id::spc700Voice5SampleSlot,
    chipper::parameters::id::spc700Voice6SampleSlot,
    chipper::parameters::id::spc700Voice7SampleSlot,
    chipper::parameters::id::spc700Voice8SampleSlot
};

constexpr std::array<const char*, 8> perVoiceWaveIds {
    chipper::parameters::id::waveShape,
    chipper::parameters::id::sidVoice2WaveShape,
    chipper::parameters::id::sidVoice3WaveShape,
    chipper::parameters::id::pulse2Duty,
    chipper::parameters::id::dmgWaveLevel,
    chipper::parameters::id::snNoiseMode,
    chipper::parameters::id::ymEnvelopeShape,
    chipper::parameters::id::dmgStereoRoute
};

constexpr std::array<chipper::ChipParameterRole, 9> sourceEnableRoles {
    chipper::ChipParameterRole::source1Enabled,
    chipper::ChipParameterRole::source2Enabled,
    chipper::ChipParameterRole::source3Enabled,
    chipper::ChipParameterRole::source4Enabled,
    chipper::ChipParameterRole::source5Enabled,
    chipper::ChipParameterRole::source6Enabled,
    chipper::ChipParameterRole::source7Enabled,
    chipper::ChipParameterRole::source8Enabled,
    chipper::ChipParameterRole::source9Enabled
};

constexpr std::array<chipper::ChipParameterRole, 9> sourceLevelRoles {
    chipper::ChipParameterRole::source1Level,
    chipper::ChipParameterRole::source2Level,
    chipper::ChipParameterRole::source3Level,
    chipper::ChipParameterRole::source4Level,
    chipper::ChipParameterRole::source5Level,
    chipper::ChipParameterRole::source6Level,
    chipper::ChipParameterRole::source7Level,
    chipper::ChipParameterRole::source8Level,
    chipper::ChipParameterRole::source9Level
};

constexpr std::array<chipper::ChipParameterRole, 4> macroRoles {
    chipper::ChipParameterRole::macroControl1,
    chipper::ChipParameterRole::macroControl2,
    chipper::ChipParameterRole::macroControl3,
    chipper::ChipParameterRole::macroControl4
};

void configureSectionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(14.0f, juce::Font::bold));
}

void drawPanel(juce::Graphics& graphics,
               juce::Rectangle<int> bounds,
               const ChipperWorkspaceTheme& theme,
               float radius = 5.0f)
{
    if (bounds.isEmpty())
        return;

    graphics.setColour(theme.panel);
    graphics.fillRoundedRectangle(bounds.toFloat(), radius);
    graphics.setColour(theme.outline);
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), radius, 1.0f);
}

juce::String joinedLines(const std::vector<std::string>& values)
{
    juce::String result;
    for (const auto& value : values)
    {
        if (result.isNotEmpty())
            result << "\n";
        result << "- " << value;
    }
    return result;
}

juce::String sourceDetailSummary(const chipper::ui::ChipUiProfile& profile)
{
    switch (profile.family)
    {
        case chipper::ui::ChipUiFamily::consoleApu:
            return "Enable and trim this lane precisely. Chip-owned waveform, register, and expansion controls remain in Edit.";
        case chipper::ui::ChipUiFamily::psg:
            return "Enable and trim this channel precisely. Shared tone, noise, and envelope registers remain in Edit.";
        case chipper::ui::ChipUiFamily::fm:
            return "Enable and trim this channel precisely. Algorithm and operator settings are shared patch resources in Edit.";
        case chipper::ui::ChipUiFamily::sampler:
            return "Enable and trim this voice precisely. Sample assignment, loop, and mapping controls remain in Edit.";
        case chipper::ui::ChipUiFamily::wavetable:
            return "Enable and trim this lane precisely. Per-lane Wave RAM shape and shared modulation remain in Edit.";
        case chipper::ui::ChipUiFamily::oneBit:
            return "Enable and trim the output lane precisely. Timing and behavior controls remain in Edit.";
    }

    return "Enable and trim this source precisely. Open Edit for its chip-native controls.";
}

bool isSamplerMode(chipper::ChipMode mode)
{
    return mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula;
}

bool isWavetableMode(chipper::ChipMode mode)
{
    return mode == chipper::ChipMode::huc6280 || mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc;
}

const char* sourceAssetParameterId(chipper::ChipMode mode, size_t index)
{
    const auto safeIndex = std::min(index, perVoiceSampleIds.size() - 1u);
    return isSamplerMode(mode) ? perVoiceSampleIds[safeIndex] : perVoiceWaveIds[safeIndex];
}

juce::StringArray waveChoicesFor(chipper::ChipMode mode)
{
    if (mode == chipper::ChipMode::huc6280)
        return { "Preset", "Ramp", "Tri", "Square", "Noise" };
    return { "Preset", "Ramp", "Tri", "Pulse", "Steps" };
}
}

ChipperPlayWorkspace::ChipperPlayWorkspace(ChipperAudioProcessor& processor)
    : audioProcessor(processor)
{
    setOpaque(true);
    titleLabel.setText("PLAY", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    summaryLabel.setFont(juce::FontOptions(12.0f));
    summaryLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(summaryLabel);

    configureSectionLabel(sourceSectionLabel, "Sources");
    configureSectionLabel(macroSectionLabel, "Musical controls");
    configureSectionLabel(outputSectionLabel, "Output");
    addAndMakeVisible(sourceSectionLabel);
    addAndMakeVisible(relationshipMap);
    addAndMakeVisible(macroSectionLabel);
    addAndMakeVisible(outputSectionLabel);

    auto& state = audioProcessor.getValueTreeState();
    for (size_t i = 0; i < sourceButtons.size(); ++i)
    {
        auto& selector = sourceSelectButtons[i];
        selector.setClickingTogglesState(false);
        selector.setWantsKeyboardFocus(true);
        selector.setExplicitFocusOrder(100 + static_cast<int>(i * 3u));
        selector.setComponentID("play.source" + juce::String(static_cast<int>(i + 1u)) + ".select");
        selector.onClick = [this, i]() { selectSource(i); };
        addAndMakeVisible(selector);

        auto& button = sourceButtons[i];
        button.setClickingTogglesState(true);
        button.setWantsKeyboardFocus(true);
        button.setExplicitFocusOrder(101 + static_cast<int>(i * 3u));
        button.setComponentID("play.source" + juce::String(static_cast<int>(i + 1u)) + ".enabled");
        addAndMakeVisible(button);
        sourceButtonAttachments[i] = std::make_unique<ButtonAttachment>(state, sourceEnableIds[i], button);

        auto& label = sourceLevelLabels[i];
        label.setText("Level", juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(label);

        auto& slider = sourceLevelSliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setWantsKeyboardFocus(true);
        slider.setExplicitFocusOrder(102 + static_cast<int>(i * 3u));
        slider.setComponentID("play.source" + juce::String(static_cast<int>(i + 1u)) + ".level");
        addAndMakeVisible(slider);
        sourceLevelAttachments[i] = std::make_unique<SliderAttachment>(state, sourceLevelIds[i], slider);
    }

    detailTitleLabel.setJustificationType(juce::Justification::centredLeft);
    detailTitleLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(detailTitleLabel);

    detailSummaryLabel.setJustificationType(juce::Justification::centredLeft);
    detailSummaryLabel.setFont(juce::FontOptions(11.0f));
    detailSummaryLabel.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(detailSummaryLabel);

    detailEnableButton.setButtonText("Enabled");
    detailEnableButton.setWantsKeyboardFocus(true);
    detailEnableButton.setComponentID("play.sourceDetail.enabled");
    detailEnableButton.setExplicitFocusOrder(140);
    addAndMakeVisible(detailEnableButton);

    detailLevelLabel.setText("Selected level", juce::dontSendNotification);
    detailLevelLabel.setJustificationType(juce::Justification::centredLeft);
    detailLevelLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    addAndMakeVisible(detailLevelLabel);

    detailLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    detailLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 24);
    detailLevelSlider.setWantsKeyboardFocus(true);
    detailLevelSlider.setComponentID("play.sourceDetail.level");
    detailLevelSlider.setExplicitFocusOrder(141);
    addAndMakeVisible(detailLevelSlider);

    detailAssetLabel.setJustificationType(juce::Justification::centredLeft);
    detailAssetLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    addAndMakeVisible(detailAssetLabel);

    detailAssetBox.setWantsKeyboardFocus(true);
    detailAssetBox.setComponentID("play.sourceDetail.asset");
    detailAssetBox.setExplicitFocusOrder(142);
    detailAssetBox.onChange = [this]
    {
        if (detailAssetAttachment != nullptr && detailAssetBox.getSelectedItemIndex() >= 0)
            detailAssetAttachment->setValueAsCompleteGesture(static_cast<float>(detailAssetBox.getSelectedItemIndex()));
    };
    addAndMakeVisible(detailAssetBox);

    detailAssetStatusLabel.setJustificationType(juce::Justification::centredLeft);
    detailAssetStatusLabel.setFont(juce::FontOptions(9.5f));
    detailAssetStatusLabel.setMinimumHorizontalScale(0.65f);
    addAndMakeVisible(detailAssetStatusLabel);

    detailOpenEditButton.setWantsKeyboardFocus(true);
    detailOpenEditButton.setComponentID("play.sourceDetail.openEdit");
    detailOpenEditButton.setExplicitFocusOrder(143);
    detailOpenEditButton.onClick = [this]
    {
        if (onOpenEditRequested)
            onOpenEditRequested();
    };
    addAndMakeVisible(detailOpenEditButton);

    for (size_t i = 0; i < macroSliders.size(); ++i)
    {
        auto& label = macroLabels[i];
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        addAndMakeVisible(label);

        auto& slider = macroSliders[i];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 24);
        slider.setWantsKeyboardFocus(true);
        slider.setComponentID("play.macro" + juce::String(static_cast<int>(i + 1u)));
        slider.setExplicitFocusOrder(150 + static_cast<int>(i));
        addAndMakeVisible(slider);
        macroAttachments[i] = std::make_unique<SliderAttachment>(state, macroIds[i], slider);
    }

    outputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
    outputSlider.setTextValueSuffix(" dB");
    outputSlider.setWantsKeyboardFocus(true);
    outputSlider.setComponentID("play.output");
    outputSlider.setExplicitFocusOrder(160);
    addAndMakeVisible(outputSlider);
    outputAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::outputDb, outputSlider);
}

void ChipperPlayWorkspace::paint(juce::Graphics& graphics)
{
    graphics.fillAll(theme.background);
    drawPanel(graphics, sourcePanelBounds, theme);
    drawPanel(graphics, macroPanelBounds, theme);
    drawPanel(graphics, outputPanelBounds, theme);
    if (usesMasterDetail)
        drawPanel(graphics, sourceDetailPanelBounds, theme, 4.0f);

    for (size_t i = 0; i < visibleSourceCount; ++i)
    {
        const auto selected = usesMasterDetail && i == selectedSourceIndex;
        graphics.setColour(selected ? theme.primary.withAlpha(0.16f) : theme.sourceCard);
        graphics.fillRoundedRectangle(sourceCardBounds[i].toFloat(), 4.0f);
        graphics.setColour(selected ? theme.accent : theme.outline);
        graphics.drawRoundedRectangle(sourceCardBounds[i].toFloat().reduced(0.5f), 4.0f, selected ? 1.5f : 1.0f);
    }
}

void ChipperPlayWorkspace::resized()
{
    auto area = getLocalBounds();
    auto heading = area.removeFromTop(42);
    titleLabel.setBounds(heading.removeFromLeft(76));
    summaryLabel.setBounds(heading);
    area.removeFromTop(8);

    const auto sourceHeight = std::clamp(static_cast<int>(std::round(area.getHeight() * 0.62)), 310, 408);
    sourcePanelBounds = area.removeFromTop(std::min(sourceHeight, area.getHeight()));
    area.removeFromTop(10);
    const auto bottomWidth = area.getWidth();
    const auto outputWidth = std::clamp(bottomWidth / 4, 250, 310);
    macroPanelBounds = area.removeFromLeft(std::max(0, bottomWidth - outputWidth - 10));
    area.removeFromLeft(std::min(10, area.getWidth()));
    outputPanelBounds = area;

    auto sourceArea = sourcePanelBounds.reduced(14, 10);
    auto sourceHeader = sourceArea.removeFromTop(24);
    sourceSectionLabel.setBounds(sourceHeader.removeFromLeft(std::min(92, sourceHeader.getWidth())));
    sourceHeader.removeFromLeft(std::min(8, sourceHeader.getWidth()));
    relationshipMap.setBounds(sourceHeader);
    sourceArea.removeFromTop(6);
    sourceDetailPanelBounds = {};
    if (usesMasterDetail)
    {
        const auto detailHeight = usesAssetDetail ? 118 : 82;
        sourceDetailPanelBounds = sourceArea.removeFromBottom(std::min(detailHeight, sourceArea.getHeight()));
        sourceArea.removeFromBottom(std::min(8, sourceArea.getHeight()));
    }
    const auto columns = chipper::ui::profileFor(displayedMode).playSourceColumns;
    const auto rows = std::max(1, static_cast<int>((visibleSourceCount + static_cast<size_t>(columns) - 1u) / static_cast<size_t>(columns)));
    constexpr auto cardGap = 8;
    const auto cardWidth = (sourceArea.getWidth() - (cardGap * (columns - 1))) / columns;
    const auto cardHeight = (sourceArea.getHeight() - (cardGap * (rows - 1))) / rows;
    for (size_t i = 0; i < sourceCardBounds.size(); ++i)
    {
        if (i >= visibleSourceCount)
        {
            sourceCardBounds[i] = {};
            sourceButtons[i].setBounds({});
            sourceSelectButtons[i].setBounds({});
            sourceLevelLabels[i].setBounds({});
            sourceLevelSliders[i].setBounds({});
            continue;
        }

        const auto row = static_cast<int>(i) / columns;
        const auto column = static_cast<int>(i) % columns;
        sourceCardBounds[i] = {
            sourceArea.getX() + (column * (cardWidth + cardGap)),
            sourceArea.getY() + (row * (cardHeight + cardGap)),
            cardWidth,
            cardHeight
        };
        auto card = sourceCardBounds[i].reduced(8, 7);
        if (usesMasterDetail)
        {
            auto sourceHeading = card.removeFromTop(28);
            sourceButtons[i].setBounds(sourceHeading.removeFromRight(std::min(48, sourceHeading.getWidth())));
            sourceHeading.removeFromRight(std::min(5, sourceHeading.getWidth()));
            sourceSelectButtons[i].setBounds(sourceHeading);
        }
        else
        {
            sourceSelectButtons[i].setBounds({});
            sourceButtons[i].setBounds(card.removeFromTop(28));
        }
        card.removeFromTop(8);
        sourceLevelLabels[i].setBounds(card.removeFromTop(16));
        sourceLevelSliders[i].setBounds(card.removeFromTop(std::min(28, card.getHeight())).reduced(0, 2));
    }

    if (usesMasterDetail)
    {
        auto detail = sourceDetailPanelBounds.reduced(12, 8);
        auto actions = detail.removeFromRight(std::min(330, detail.getWidth() / 2));
        detail.removeFromRight(std::min(12, detail.getWidth()));
        auto asset = juce::Rectangle<int> {};
        if (usesAssetDetail)
        {
            asset = detail.removeFromRight(std::min(310, detail.getWidth() / 2));
            detail.removeFromRight(std::min(12, detail.getWidth()));
        }
        detailTitleLabel.setBounds(detail.removeFromTop(22));
        detailSummaryLabel.setBounds(detail);

        detailEnableButton.setBounds(actions.removeFromLeft(std::min(94, actions.getWidth())).reduced(0, 7));
        actions.removeFromLeft(std::min(10, actions.getWidth()));
        detailLevelLabel.setBounds(actions.removeFromTop(18));
        detailLevelSlider.setBounds(actions.removeFromTop(std::min(32, actions.getHeight())).reduced(0, 2));

        if (usesAssetDetail)
        {
            detailAssetLabel.setBounds(asset.removeFromTop(17));
            detailAssetBox.setBounds(asset.removeFromTop(std::min(28, asset.getHeight())));
            asset.removeFromTop(std::min(3, asset.getHeight()));
            detailAssetStatusLabel.setBounds(asset.removeFromTop(std::min(18, asset.getHeight())));
            asset.removeFromTop(std::min(4, asset.getHeight()));
            detailOpenEditButton.setBounds(asset.removeFromTop(std::min(26, asset.getHeight())));
        }
        else
        {
            detailAssetLabel.setBounds({});
            detailAssetBox.setBounds({});
            detailAssetStatusLabel.setBounds({});
            detailOpenEditButton.setBounds({});
        }
    }
    else
    {
        detailTitleLabel.setBounds({});
        detailSummaryLabel.setBounds({});
        detailEnableButton.setBounds({});
        detailLevelLabel.setBounds({});
        detailLevelSlider.setBounds({});
        detailAssetLabel.setBounds({});
        detailAssetBox.setBounds({});
        detailAssetStatusLabel.setBounds({});
        detailOpenEditButton.setBounds({});
    }

    auto macroArea = macroPanelBounds.reduced(14, 10);
    macroSectionLabel.setBounds(macroArea.removeFromTop(24));
    macroArea.removeFromTop(4);
    constexpr auto macroGap = 8;
    const auto macroCellWidth = (macroArea.getWidth() - macroGap) / 2;
    const auto macroCellHeight = (macroArea.getHeight() - macroGap) / 2;
    for (size_t i = 0; i < macroSliders.size(); ++i)
    {
        const auto row = static_cast<int>(i / 2u);
        const auto column = static_cast<int>(i % 2u);
        auto cell = juce::Rectangle<int> {
            macroArea.getX() + (column * (macroCellWidth + macroGap)),
            macroArea.getY() + (row * (macroCellHeight + macroGap)),
            macroCellWidth,
            macroCellHeight
        };
        macroLabels[i].setBounds(cell.removeFromTop(20));
        macroSliders[i].setBounds(cell.removeFromTop(std::min(30, cell.getHeight())).reduced(0, 2));
    }

    auto outputArea = outputPanelBounds.reduced(14, 10);
    outputSectionLabel.setBounds(outputArea.removeFromTop(24));
    outputArea.removeFromTop(8);
    outputSlider.setBounds(outputArea.removeFromTop(std::min(32, outputArea.getHeight())).reduced(0, 2));
}

void ChipperPlayWorkspace::refresh(chipper::ChipMode mode, const ChipperWorkspaceTheme& themeToUse)
{
    displayedMode = mode;
    theme = themeToUse;
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto uiProfile = chipper::ui::profileFor(mode);
    visibleSourceCount = std::min(sourceCount, uiProfile.visibleSourceCount);
    usesMasterDetail = uiProfile.usesMasterDetailSources;
    usesAssetDetail = uiProfile.sampler || uiProfile.wavetable;
    if (visibleSourceCount == 0u)
        selectedSourceIndex = 0u;
    else
        selectedSourceIndex = std::min(selectedSourceIndex, visibleSourceCount - 1u);
    summaryLabel.setText(juce::String(descriptor.displayName) + " essentials / "
                             + juce::String(uiProfile.familyLabel.data()) + " / "
                             + juce::String(static_cast<int>(uiProfile.visibleSourceCount)) + " sources. Open Edit for chip-native detail or Inspect for evidence.",
                         juce::dontSendNotification);

    titleLabel.setColour(juce::Label::textColourId, theme.primary);
    summaryLabel.setColour(juce::Label::textColourId, theme.mutedText);
    for (auto* label : { &sourceSectionLabel, &macroSectionLabel, &outputSectionLabel })
        label->setColour(juce::Label::textColourId, theme.primary);
    relationshipMap.setMode(mode);
    relationshipMap.setTheme(theme.accent, theme.primary, theme.outline, theme.text, theme.mutedText, theme.background);

    for (size_t i = 0; i < sourceButtons.size(); ++i)
    {
        const auto visible = i < visibleSourceCount;
        const auto* spec = visible ? chipper::parameterSpecFor(mode, sourceEnableRoles[i]) : nullptr;
        const auto label = spec != nullptr ? juce::String(spec->label) : "Source " + juce::String(static_cast<int>(i + 1u));
        sourceSelectButtons[i].setButtonText(label);
        sourceSelectButtons[i].setName("Select " + label);
        sourceSelectButtons[i].setTooltip("Select " + label + " for precise source editing");
        sourceSelectButtons[i].setToggleState(usesMasterDetail && i == selectedSourceIndex, juce::dontSendNotification);
        sourceSelectButtons[i].setVisible(visible && usesMasterDetail);
        sourceSelectButtons[i].setColour(juce::TextButton::buttonColourId, theme.sourceCard);
        sourceSelectButtons[i].setColour(juce::TextButton::buttonOnColourId, theme.primary.withAlpha(0.34f));
        sourceSelectButtons[i].setColour(juce::TextButton::textColourOffId, theme.text);
        sourceSelectButtons[i].setColour(juce::TextButton::textColourOnId, theme.text);
        sourceButtons[i].setButtonText(usesMasterDetail ? "ON" : label);
        sourceButtons[i].setName(label + " enabled");
        sourceButtons[i].setTooltip(spec != nullptr ? juce::String(spec->help) : label + " enable");
        sourceButtons[i].setVisible(visible);
        sourceLevelLabels[i].setVisible(visible);
        sourceLevelSliders[i].setVisible(visible);
        sourceLevelSliders[i].setName(label + " level");
        sourceLevelSliders[i].setTooltip(label + " level trim");
        sourceButtons[i].setColour(juce::TextButton::buttonColourId, theme.sourceCard);
        sourceButtons[i].setColour(juce::TextButton::buttonOnColourId, theme.primary);
        sourceButtons[i].setColour(juce::TextButton::textColourOffId, theme.text);
        sourceButtons[i].setColour(juce::TextButton::textColourOnId, theme.darkText);
        sourceLevelLabels[i].setColour(juce::Label::textColourId, theme.accent);
        sourceLevelSliders[i].setColour(juce::Slider::trackColourId, theme.primary);
        sourceLevelSliders[i].setColour(juce::Slider::thumbColourId, theme.accent);
        sourceLevelSliders[i].setColour(juce::Slider::backgroundColourId, theme.outline.darker(0.35f));
    }

    for (auto* label : { &detailTitleLabel, &detailLevelLabel, &detailAssetLabel })
        label->setColour(juce::Label::textColourId, theme.primary);
    detailSummaryLabel.setColour(juce::Label::textColourId, theme.mutedText);
    detailAssetStatusLabel.setColour(juce::Label::textColourId, theme.mutedText);
    detailEnableButton.setColour(juce::ToggleButton::textColourId, theme.text);
    detailEnableButton.setColour(juce::ToggleButton::tickColourId, theme.accent);
    detailEnableButton.setColour(juce::ToggleButton::tickDisabledColourId, theme.outline);
    detailLevelSlider.setColour(juce::Slider::trackColourId, theme.primary);
    detailLevelSlider.setColour(juce::Slider::thumbColourId, theme.accent);
    detailLevelSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    detailLevelSlider.setColour(juce::Slider::textBoxBackgroundColourId, theme.background);
    detailLevelSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.outline);
    detailAssetBox.setColour(juce::ComboBox::backgroundColourId, theme.sourceCard);
    detailAssetBox.setColour(juce::ComboBox::textColourId, theme.text);
    detailAssetBox.setColour(juce::ComboBox::outlineColourId, theme.outline);
    detailAssetBox.setColour(juce::ComboBox::arrowColourId, theme.text);
    detailOpenEditButton.setColour(juce::TextButton::buttonColourId, theme.sourceCard);
    detailOpenEditButton.setColour(juce::TextButton::textColourOffId, theme.text);
    detailTitleLabel.setVisible(usesMasterDetail);
    detailSummaryLabel.setVisible(usesMasterDetail);
    detailEnableButton.setVisible(usesMasterDetail);
    detailLevelLabel.setVisible(usesMasterDetail);
    detailLevelSlider.setVisible(usesMasterDetail);
    detailAssetLabel.setVisible(usesAssetDetail);
    detailAssetBox.setVisible(usesAssetDetail);
    detailAssetStatusLabel.setVisible(usesAssetDetail);
    detailOpenEditButton.setVisible(usesAssetDetail);
    bindSelectedSource();

    for (size_t i = 0; i < macroSliders.size(); ++i)
    {
        const auto* spec = chipper::parameterSpecFor(mode, macroRoles[i]);
        const auto label = spec != nullptr ? juce::String(spec->label) : "Control " + juce::String(static_cast<int>(i + 1u));
        macroLabels[i].setText(label, juce::dontSendNotification);
        macroLabels[i].setColour(juce::Label::textColourId, theme.text);
        macroSliders[i].setName(label);
        macroSliders[i].setTooltip(spec != nullptr ? juce::String(spec->help) : label);
        macroSliders[i].setColour(juce::Slider::trackColourId, theme.primary);
        macroSliders[i].setColour(juce::Slider::thumbColourId, theme.accent);
        macroSliders[i].setColour(juce::Slider::textBoxTextColourId, theme.text);
        macroSliders[i].setColour(juce::Slider::textBoxBackgroundColourId, theme.background);
        macroSliders[i].setColour(juce::Slider::textBoxOutlineColourId, theme.outline);
    }

    outputSlider.setColour(juce::Slider::trackColourId, theme.primary);
    outputSlider.setColour(juce::Slider::thumbColourId, theme.accent);
    outputSlider.setColour(juce::Slider::textBoxTextColourId, theme.text);
    outputSlider.setColour(juce::Slider::textBoxBackgroundColourId, theme.background);
    outputSlider.setColour(juce::Slider::textBoxOutlineColourId, theme.outline);
    resized();
    repaint();
}

void ChipperPlayWorkspace::selectSource(size_t index)
{
    if (! usesMasterDetail || index >= visibleSourceCount || index == selectedSourceIndex)
        return;

    selectedSourceIndex = index;
    for (size_t i = 0; i < sourceSelectButtons.size(); ++i)
        sourceSelectButtons[i].setToggleState(i == selectedSourceIndex, juce::dontSendNotification);
    bindSelectedSource();
    repaint();
}

void ChipperPlayWorkspace::bindSelectedSource()
{
    detailEnableAttachment.reset();
    detailLevelAttachment.reset();
    detailAssetAttachment.reset();
    if (! usesMasterDetail || selectedSourceIndex >= visibleSourceCount)
        return;

    const auto* enableSpec = chipper::parameterSpecFor(displayedMode, sourceEnableRoles[selectedSourceIndex]);
    const auto* levelSpec = chipper::parameterSpecFor(displayedMode, sourceLevelRoles[selectedSourceIndex]);
    const auto sourceName = enableSpec != nullptr
        ? juce::String(enableSpec->label)
        : "Source " + juce::String(static_cast<int>(selectedSourceIndex + 1u));
    const auto profile = chipper::ui::profileFor(displayedMode);

    detailTitleLabel.setText("Selected: " + sourceName, juce::dontSendNotification);
    detailSummaryLabel.setText(sourceDetailSummary(profile), juce::dontSendNotification);
    detailEnableButton.setName(sourceName + " enabled in selected-source editor");
    detailEnableButton.setTooltip(enableSpec != nullptr ? juce::String(enableSpec->help) : sourceName + " enable");
    detailLevelSlider.setName(sourceName + " precise level");
    detailLevelSlider.setTooltip(levelSpec != nullptr ? juce::String(levelSpec->help) : sourceName + " level trim");

    auto& state = audioProcessor.getValueTreeState();
    detailEnableAttachment = std::make_unique<ButtonAttachment>(state, sourceEnableIds[selectedSourceIndex], detailEnableButton);
    detailLevelAttachment = std::make_unique<SliderAttachment>(state, sourceLevelIds[selectedSourceIndex], detailLevelSlider);

    if (! usesAssetDetail)
        return;

    detailAssetBox.clear(juce::dontSendNotification);
    if (isSamplerMode(displayedMode))
    {
        const auto names = displayedMode == chipper::ChipMode::spc700
            ? audioProcessor.spc700BrrSampleNames()
            : audioProcessor.paulaSampleNames();
        detailAssetLabel.setText("Sample assignment", juce::dontSendNotification);
        detailAssetBox.addItem("Follow shared bank", 1);
        for (int slot = 1; slot <= 32; ++slot)
        {
            const auto loadedName = slot <= names.size() ? names[slot - 1] : juce::String("empty");
            detailAssetBox.addItem("Slot " + juce::String(slot).paddedLeft('0', 2) + " / " + loadedName, slot + 1);
        }
        detailOpenEditButton.setButtonText("Open Sample Bank");
        detailOpenEditButton.setTooltip("Open Edit at the shared sample-bank, mapping, loop, and missing-file recovery controls.");
    }
    else
    {
        detailAssetLabel.setText("Wave RAM shape", juce::dontSendNotification);
        detailAssetBox.addItemList(waveChoicesFor(displayedMode), 1);
        detailOpenEditButton.setButtonText("Open Wave Editor");
        detailOpenEditButton.setTooltip("Open Edit at the per-lane Wave RAM and modulation controls.");
    }
    detailAssetBox.setName(sourceName + " asset assignment");

    if (auto* parameter = state.getParameter(sourceAssetParameterId(displayedMode, selectedSourceIndex)))
    {
        detailAssetAttachment = std::make_unique<juce::ParameterAttachment>(
            *parameter,
            [this](float newValue)
            {
                const auto selected = std::clamp(static_cast<int>(std::round(newValue)), 0, std::max(0, detailAssetBox.getNumItems() - 1));
                detailAssetBox.setSelectedItemIndex(selected, juce::dontSendNotification);
                updateSelectedAssetStatus(newValue);
            },
            nullptr);
        detailAssetAttachment->sendInitialUpdate();
    }
}

void ChipperPlayWorkspace::updateSelectedAssetStatus(float plainValue)
{
    const auto selected = std::max(0, static_cast<int>(std::round(plainValue)));
    if (isSamplerMode(displayedMode))
    {
        if (selected == 0)
        {
            detailAssetStatusLabel.setText("Follows the shared bank, manual slot, or note map.", juce::dontSendNotification);
            return;
        }
        const auto names = displayedMode == chipper::ChipMode::spc700
            ? audioProcessor.spc700BrrSampleNames()
            : audioProcessor.paulaSampleNames();
        if (selected <= names.size())
        {
            detailAssetStatusLabel.setText("Loaded: " + names[selected - 1], juce::dontSendNotification);
            return;
        }
        detailAssetStatusLabel.setText("Missing slot " + juce::String(selected).paddedLeft('0', 2) + ". Open Sample Bank to recover.",
                                       juce::dontSendNotification);
        return;
    }

    const auto choices = waveChoicesFor(displayedMode);
    const auto safeChoice = std::clamp(selected, 0, choices.size() - 1);
    detailAssetStatusLabel.setText(choices[safeChoice] + " regenerates this lane's native Wave RAM.", juce::dontSendNotification);
}

void ChipperPlayWorkspace::focusInitialControl()
{
    if (visibleSourceCount > 0)
        sourceButtons[0].grabKeyboardFocus();
    else
        macroSliders[0].grabKeyboardFocus();
}

juce::Rectangle<int> ChipperPlayWorkspace::sourceButtonBoundsForTest(size_t index) const
{
    if (index >= sourceButtons.size())
        return {};

    return usesMasterDetail ? sourceSelectButtons[index].getBounds() : sourceButtons[index].getBounds();
}

juce::Rectangle<int> ChipperPlayWorkspace::macroSliderBoundsForTest(size_t index) const
{
    return index < macroSliders.size() ? macroSliders[index].getBounds() : juce::Rectangle<int> {};
}

ChipperInspectWorkspace::ChipperInspectWorkspace()
{
    setOpaque(true);
    titleLabel.setText("INSPECT", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    badgeLabel.setJustificationType(juce::Justification::centred);
    badgeLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    addAndMakeVisible(badgeLabel);

    configureHeading(implementationHeading, "Implementation");
    configureHeading(evidenceHeading, "Verification evidence");
    configureHeading(gapsHeading, "Known gaps");
    configureHeading(controlsHeading, "Control contract");
    for (auto* label : { &implementationHeading, &evidenceHeading, &gapsHeading, &controlsHeading })
        addAndMakeVisible(*label);

    for (auto* editor : { &implementationText, &verificationText, &gapsText, &controlsText })
    {
        configureReadOnlyText(*editor);
        addAndMakeVisible(*editor);
    }
    implementationText.setExplicitFocusOrder(200);
    implementationText.setComponentID("inspect.implementation");
    verificationText.setExplicitFocusOrder(201);
    verificationText.setComponentID("inspect.verification");
    gapsText.setExplicitFocusOrder(202);
    gapsText.setComponentID("inspect.gaps");
    controlsText.setExplicitFocusOrder(203);
    controlsText.setComponentID("inspect.controls");
}

void ChipperInspectWorkspace::configureHeading(juce::Label& label, const juce::String& text)
{
    configureSectionLabel(label, text);
}

void ChipperInspectWorkspace::configureReadOnlyText(juce::TextEditor& editor)
{
    editor.setMultiLine(true, true);
    editor.setReadOnly(true);
    editor.setCaretVisible(false);
    editor.setScrollbarsShown(false);
    editor.setPopupMenuEnabled(true);
    editor.setWantsKeyboardFocus(true);
    editor.setFont(juce::FontOptions(13.0f));
    editor.setBorder(juce::BorderSize<int>(8));
}

void ChipperInspectWorkspace::paint(juce::Graphics& graphics)
{
    graphics.fillAll(theme.background);
    for (const auto bounds : panelBounds)
        drawPanel(graphics, bounds, theme);
}

void ChipperInspectWorkspace::resized()
{
    auto area = getLocalBounds();
    auto heading = area.removeFromTop(42);
    titleLabel.setBounds(heading.removeFromLeft(92));
    badgeLabel.setBounds(heading.removeFromRight(std::min(180, heading.getWidth())).reduced(0, 5));
    area.removeFromTop(8);

    constexpr auto gap = 10;
    const auto columnWidth = (area.getWidth() - gap) / 2;
    const auto rowHeight = (area.getHeight() - gap) / 2;
    panelBounds[0] = { area.getX(), area.getY(), columnWidth, rowHeight };
    panelBounds[1] = { area.getX(), area.getY() + rowHeight + gap, columnWidth, rowHeight };
    panelBounds[2] = { area.getX() + columnWidth + gap, area.getY(), columnWidth, rowHeight };
    panelBounds[3] = { area.getX() + columnWidth + gap, area.getY() + rowHeight + gap, columnWidth, rowHeight };

    const auto placePanel = [](juce::Rectangle<int> bounds, juce::Label& headingLabel, juce::TextEditor& textEditor)
    {
        auto content = bounds.reduced(14, 10);
        headingLabel.setBounds(content.removeFromTop(24));
        content.removeFromTop(5);
        textEditor.setBounds(content);
    };
    placePanel(panelBounds[0], implementationHeading, implementationText);
    placePanel(panelBounds[1], evidenceHeading, verificationText);
    placePanel(panelBounds[2], gapsHeading, gapsText);
    placePanel(panelBounds[3], controlsHeading, controlsText);
}

void ChipperInspectWorkspace::refresh(chipper::ChipMode mode, const ChipperWorkspaceTheme& themeToUse)
{
    theme = themeToUse;
    const auto& descriptor = chipper::descriptorFor(mode);
    const auto visibleSources = chipper::visibleSourceCountForMode(mode);
    const auto nativeSources = chipper::nativeSourceCountForMode(mode);

    titleLabel.setColour(juce::Label::textColourId, theme.primary);
    badgeLabel.setText(descriptor.verification.badge, juce::dontSendNotification);
    badgeLabel.setColour(juce::Label::textColourId, theme.darkText);
    badgeLabel.setColour(juce::Label::backgroundColourId, theme.primary);
    for (auto* heading : { &implementationHeading, &evidenceHeading, &gapsHeading, &controlsHeading })
        heading->setColour(juce::Label::textColourId, theme.primary);

    implementationText.setText(juce::String(descriptor.displayName) + "\n\n"
                                   + juce::String(descriptor.summary) + "\n\n"
                                   + juce::String(descriptor.verification.summary),
                               false);
    verificationText.setText(juce::String(descriptor.verification.evidence) + "\n\n"
                                 + joinedLines(descriptor.verification.verifiedBehaviors),
                             false);
    const auto gaps = joinedLines(descriptor.verification.knownGaps);
    gapsText.setText(gaps.isNotEmpty() ? gaps : "No documented gaps for the currently exposed surface.", false);

    juce::String modules;
    for (const auto& module : descriptor.modules)
    {
        if (module.title.empty())
            continue;
        if (modules.isNotEmpty())
            modules << ", ";
        modules << module.title;
    }
    controlsText.setText("Automatable controls: " + juce::String(static_cast<int>(descriptor.parameters.size()))
                             + "\nVisible sources: " + juce::String(static_cast<int>(visibleSources))
                             + " of " + juce::String(static_cast<int>(nativeSources)) + " native lanes"
                             + "\nChip Poly: " + juce::String(descriptor.supportsChipPoly ? "supported" : "not exposed")
                             + "\nHardware validated: " + juce::String(descriptor.verification.hardwareValidated ? "yes" : "no")
                             + "\nCycle accurate: " + juce::String(descriptor.verification.cycleAccurate ? "yes" : "no")
                             + "\nFixed MIDI map: CC 24-119"
                             + "\n\nModules: " + modules,
                         false);

    for (auto* editor : { &implementationText, &verificationText, &gapsText, &controlsText })
    {
        editor->setColour(juce::TextEditor::backgroundColourId, theme.sourceCard);
        editor->setColour(juce::TextEditor::textColourId, theme.text);
        editor->setColour(juce::TextEditor::outlineColourId, theme.outline);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, theme.accent);
        editor->setColour(juce::TextEditor::highlightColourId, theme.primary.withAlpha(0.45f));
    }
    implementationText.setName("Implementation summary");
    verificationText.setName("Verification evidence");
    gapsText.setName("Known implementation gaps");
    controlsText.setName("Control contract");
    repaint();
}

void ChipperInspectWorkspace::focusInitialControl()
{
    implementationText.grabKeyboardFocus();
}

ChipperWorkspaceDeck::ChipperWorkspaceDeck(ChipperAudioProcessor& processor)
    : playWorkspace(processor)
{
    setOpaque(true);
    playWorkspace.onOpenEditRequested = [this]
    {
        if (onOpenEditRequested)
            onOpenEditRequested();
    };
    addChildComponent(playWorkspace);
    addChildComponent(inspectWorkspace);
}

void ChipperWorkspaceDeck::paint(juce::Graphics& graphics)
{
    graphics.fillAll(theme.background);
}

void ChipperWorkspaceDeck::resized()
{
    playWorkspace.setBounds(getLocalBounds());
    inspectWorkspace.setBounds(getLocalBounds());
}

void ChipperWorkspaceDeck::setWorkspace(ChipperEditorWorkspace workspaceToUse)
{
    selectedWorkspace = workspaceToUse;
    playWorkspace.setVisible(selectedWorkspace == ChipperEditorWorkspace::play);
    inspectWorkspace.setVisible(selectedWorkspace == ChipperEditorWorkspace::inspect);
    setVisible(selectedWorkspace != ChipperEditorWorkspace::edit);
}

void ChipperWorkspaceDeck::refresh(chipper::ChipMode mode, const ChipperWorkspaceTheme& themeToUse)
{
    theme = themeToUse;
    playWorkspace.refresh(mode, theme);
    inspectWorkspace.refresh(mode, theme);
    repaint();
}

void ChipperWorkspaceDeck::focusInitialControl()
{
    if (selectedWorkspace == ChipperEditorWorkspace::play)
        playWorkspace.focusInitialControl();
    else if (selectedWorkspace == ChipperEditorWorkspace::inspect)
        inspectWorkspace.focusInitialControl();
}
