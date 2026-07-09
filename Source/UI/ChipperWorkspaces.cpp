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
    addAndMakeVisible(macroSectionLabel);
    addAndMakeVisible(outputSectionLabel);

    auto& state = audioProcessor.getValueTreeState();
    for (size_t i = 0; i < sourceButtons.size(); ++i)
    {
        auto& button = sourceButtons[i];
        button.setClickingTogglesState(true);
        button.setWantsKeyboardFocus(true);
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
        slider.setComponentID("play.source" + juce::String(static_cast<int>(i + 1u)) + ".level");
        addAndMakeVisible(slider);
        sourceLevelAttachments[i] = std::make_unique<SliderAttachment>(state, sourceLevelIds[i], slider);
    }

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
        addAndMakeVisible(slider);
        macroAttachments[i] = std::make_unique<SliderAttachment>(state, macroIds[i], slider);
    }

    outputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
    outputSlider.setTextValueSuffix(" dB");
    outputSlider.setWantsKeyboardFocus(true);
    outputSlider.setComponentID("play.output");
    addAndMakeVisible(outputSlider);
    outputAttachment = std::make_unique<SliderAttachment>(state, chipper::parameters::id::outputDb, outputSlider);
}

void ChipperPlayWorkspace::paint(juce::Graphics& graphics)
{
    graphics.fillAll(theme.background);
    drawPanel(graphics, sourcePanelBounds, theme);
    drawPanel(graphics, macroPanelBounds, theme);
    drawPanel(graphics, outputPanelBounds, theme);

    for (size_t i = 0; i < visibleSourceCount; ++i)
    {
        graphics.setColour(theme.sourceCard);
        graphics.fillRoundedRectangle(sourceCardBounds[i].toFloat(), 4.0f);
        graphics.setColour(theme.outline);
        graphics.drawRoundedRectangle(sourceCardBounds[i].toFloat().reduced(0.5f), 4.0f, 1.0f);
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
    sourceSectionLabel.setBounds(sourceArea.removeFromTop(24));
    sourceArea.removeFromTop(6);
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
        sourceButtons[i].setBounds(card.removeFromTop(28));
        card.removeFromTop(8);
        sourceLevelLabels[i].setBounds(card.removeFromTop(16));
        sourceLevelSliders[i].setBounds(card.removeFromTop(std::min(28, card.getHeight())).reduced(0, 2));
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
    summaryLabel.setText(juce::String(descriptor.displayName) + " essentials · "
                             + juce::String(uiProfile.familyLabel.data()) + " · "
                             + juce::String(static_cast<int>(uiProfile.visibleSourceCount)) + " sources. Open Edit for chip-native detail or Inspect for evidence.",
                         juce::dontSendNotification);

    titleLabel.setColour(juce::Label::textColourId, theme.primary);
    summaryLabel.setColour(juce::Label::textColourId, theme.mutedText);
    for (auto* label : { &sourceSectionLabel, &macroSectionLabel, &outputSectionLabel })
        label->setColour(juce::Label::textColourId, theme.primary);

    for (size_t i = 0; i < sourceButtons.size(); ++i)
    {
        const auto visible = i < visibleSourceCount;
        const auto* spec = visible ? chipper::parameterSpecFor(mode, sourceEnableRoles[i]) : nullptr;
        const auto label = spec != nullptr ? juce::String(spec->label) : "Source " + juce::String(static_cast<int>(i + 1u));
        sourceButtons[i].setButtonText(label);
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

void ChipperPlayWorkspace::focusInitialControl()
{
    if (visibleSourceCount > 0)
        sourceButtons[0].grabKeyboardFocus();
    else
        macroSliders[0].grabKeyboardFocus();
}

juce::Rectangle<int> ChipperPlayWorkspace::sourceButtonBoundsForTest(size_t index) const
{
    return index < sourceButtons.size() ? sourceButtons[index].getBounds() : juce::Rectangle<int> {};
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
