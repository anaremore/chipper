#include "ChipperMotionLab.h"

#include "Engine/ChipDescriptors.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto motionFocusOrderBase = 400;
constexpr std::array<const char*, 6> templateLabels {
    "Clear", "Major", "Minor", "Rise", "Fall", "Pulse"
};
constexpr std::array<chipper::MotionTemplate, 6> templateKinds {
    chipper::MotionTemplate::init,
    chipper::MotionTemplate::majorArp,
    chipper::MotionTemplate::minorArp,
    chipper::MotionTemplate::rise,
    chipper::MotionTemplate::fall,
    chipper::MotionTemplate::pulse
};

juce::String stringFromView(std::string_view text)
{
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

void drawPanel(juce::Graphics& graphics,
               juce::Rectangle<int> bounds,
               juce::Colour fill,
               juce::Colour outline,
               float radius = 6.0f)
{
    if (bounds.isEmpty())
        return;

    graphics.setColour(fill);
    graphics.fillRoundedRectangle(bounds.toFloat(), radius);
    graphics.setColour(outline);
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), radius, 1.0f);
}
}

ChipperMotionLab::StepColumn::StepColumn()
{
    setName("Tracker motion step");
    setComponentID("motion.step");

    stepLabel.setJustificationType(juce::Justification::centred);
    stepLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(stepLabel);

    const auto configureFieldLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };
    configureFieldLabel(pitchLabel, "PITCH");
    configureFieldLabel(levelLabel, "LEVEL");
    configureFieldLabel(gateLabel, "GATE");
    configureFieldLabel(noiseLabel, "NOISE PERIOD");

    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 22);
    pitchSlider.setRange(chipper::motionMinimumPitch, chipper::motionMaximumPitch, 1.0);
    pitchSlider.setNumDecimalPlacesToDisplay(0);
    pitchSlider.setScrollWheelEnabled(false);
    pitchSlider.setName("Step pitch offset");
    pitchSlider.setDescription("Semitone offset from the held MIDI note.");
    pitchSlider.setTooltip("Pitch offset in semitones (-24 to +24).");
    pitchSlider.setWantsKeyboardFocus(true);
    addAndMakeVisible(pitchSlider);

    levelSlider.setSliderStyle(juce::Slider::LinearVertical);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 22);
    levelSlider.setRange(0.0, static_cast<double>(chipper::motionMaximumLevel), 1.0);
    levelSlider.setNumDecimalPlacesToDisplay(0);
    levelSlider.setScrollWheelEnabled(false);
    levelSlider.setName("Step output level");
    levelSlider.setDescription("Post-chip step level from zero to fifteen.");
    levelSlider.setTooltip("Step output level (0 to 15).");
    levelSlider.setWantsKeyboardFocus(true);
    addAndMakeVisible(levelSlider);

    gateBox.addItem("Hold", 1);
    gateBox.addItem("Trig", 2);
    gateBox.addItem("Cut", 3);
    gateBox.setName("Step gate");
    gateBox.setDescription("Hold, retrigger, or cut the note at this step.");
    gateBox.setTooltip("Hold continues the voice, Trig retriggers it, and Cut silences it.");
    gateBox.setWantsKeyboardFocus(true);
    addAndMakeVisible(gateBox);

    noiseBox.addItem("Preset", 1);
    for (int period = 0; period < 32; ++period)
        noiseBox.addItem(juce::String(period), period + 2);
    noiseBox.setName("YM2149 shared noise period");
    noiseBox.setTooltip("Native register 6 period (0-31); lower is faster. Preset restores the patch. Enable Noise and route it in the A/B/C source cards. Hold changes timbre without retriggering.");
    noiseBox.setWantsKeyboardFocus(true);
    addAndMakeVisible(noiseBox);

    const auto changed = [this]
    {
        if (! updating && onChanged)
            onChanged();
    };
    pitchSlider.onValueChange = changed;
    levelSlider.onValueChange = changed;
    gateBox.onChange = changed;
    noiseBox.onChange = changed;
}

void ChipperMotionLab::StepColumn::setTheme(const Theme& newTheme)
{
    theme = newTheme;
    for (auto* label : { &stepLabel, &pitchLabel, &levelLabel, &gateLabel, &noiseLabel })
        label->setColour(juce::Label::textColourId, theme.text);

    for (auto* slider : { &pitchSlider, &levelSlider })
    {
        slider->setColour(juce::Slider::trackColourId, theme.primary);
        slider->setColour(juce::Slider::thumbColourId, theme.accent);
        slider->setColour(juce::Slider::textBoxTextColourId, theme.text);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, theme.panel);
        slider->setColour(juce::Slider::textBoxOutlineColourId, theme.outline);
    }
    repaint();
}

void ChipperMotionLab::StepColumn::setStep(size_t index,
                                           const chipper::MotionStep& newStep,
                                           bool shouldBeInPatternLength,
                                           bool shouldBeActive, bool shouldShowNativeNoise)
{
    updating = true;
    stepIndex = index;
    nativeNoise = shouldShowNativeNoise;
    noiseLabel.setVisible(nativeNoise);
    noiseBox.setVisible(nativeNoise);
    noiseBox.setEnabled(shouldBeInPatternLength);
    noiseBox.setSelectedId(static_cast<int>(newStep.ymNoisePeriod) + 1, juce::dontSendNotification);
    inPatternLength = shouldBeInPatternLength;
    const auto focusBase = motionFocusOrderBase + 20 + static_cast<int>(stepIndex * 4u);
    pitchSlider.setComponentID("motion.step." + juce::String(static_cast<int>(stepIndex + 1u)) + ".pitch");
    pitchSlider.setExplicitFocusOrder(focusBase);
    levelSlider.setComponentID("motion.step." + juce::String(static_cast<int>(stepIndex + 1u)) + ".level");
    levelSlider.setExplicitFocusOrder(focusBase + 1);
    gateBox.setComponentID("motion.step." + juce::String(static_cast<int>(stepIndex + 1u)) + ".gate");
    gateBox.setExplicitFocusOrder(focusBase + 2);
    noiseBox.setComponentID("motion.step." + juce::String(static_cast<int>(stepIndex + 1u)) + ".noise");
    noiseBox.setExplicitFocusOrder(focusBase + 3);
    active = shouldBeActive;
    stepLabel.setText("STEP " + juce::String(static_cast<int>(stepIndex + 1u))
                          + (active ? "  >"
                                    : (inPatternLength ? juce::String() : juce::String("  OFF"))),
                      juce::dontSendNotification);
    pitchSlider.setValue(static_cast<double>(newStep.pitch), juce::dontSendNotification);
    levelSlider.setValue(static_cast<double>(newStep.level), juce::dontSendNotification);
    gateBox.setSelectedId(static_cast<int>(newStep.gate) + 1, juce::dontSendNotification);
    pitchSlider.setEnabled(inPatternLength);
    levelSlider.setEnabled(inPatternLength);
    gateBox.setEnabled(inPatternLength);
    setAlpha(inPatternLength ? 1.0f : 0.52f);
    setDescription("Tracker motion step " + juce::String(static_cast<int>(stepIndex + 1u))
                   + (inPatternLength ? " is inside the pattern length." : " is outside the current pattern length."));
    updating = false;
    resized();
    repaint();
}

chipper::MotionStep ChipperMotionLab::StepColumn::step() const
{
    chipper::MotionStep result;
    result.pitch = static_cast<int8_t>(std::lround(pitchSlider.getValue()));
    result.level = static_cast<uint8_t>(std::lround(levelSlider.getValue()));
    result.gate = static_cast<chipper::MotionGate>(std::clamp(gateBox.getSelectedId() - 1, 0, 2));
    result.ymNoisePeriod = nativeNoise ? static_cast<uint8_t>(std::clamp(noiseBox.getSelectedId() - 1, 0, 32)) : 0;
    return result;
}

void ChipperMotionLab::StepColumn::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds();
    auto fill = inPatternLength ? theme.card : theme.panel.darker(0.18f);
    if (active)
        fill = fill.interpolatedWith(theme.accent, 0.14f);
    drawPanel(graphics,
              bounds,
              fill,
              active ? theme.accent : theme.outline,
              5.0f);
    if (active)
    {
        graphics.setColour(theme.accent);
        graphics.fillRoundedRectangle(bounds.withHeight(4).toFloat(), 2.0f);
    }
    if (! inPatternLength)
    {
        graphics.setColour(theme.background.withAlpha(0.30f));
        graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f);
    }
}

void ChipperMotionLab::StepColumn::resized()
{
    auto area = getLocalBounds().reduced(8, 7);
    stepLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(3);

    constexpr auto labelHeight = 15;
    constexpr auto gateHeight = 30;
    constexpr auto gap = 4;
    const auto fixedHeight = labelHeight * 3 + gateHeight + gap * 4 + (nativeNoise ? labelHeight + gateHeight + gap : 0);
    const auto slidersHeight = std::max(80, area.getHeight() - fixedHeight);
    const auto pitchHeight = std::max(46, slidersHeight * 55 / 100);
    const auto levelHeight = std::max(42, slidersHeight - pitchHeight);

    pitchLabel.setBounds(area.removeFromTop(labelHeight));
    pitchSlider.setBounds(area.removeFromTop(std::min(pitchHeight, area.getHeight())));
    area.removeFromTop(std::min(gap, area.getHeight()));
    levelLabel.setBounds(area.removeFromTop(std::min(labelHeight, area.getHeight())));
    levelSlider.setBounds(area.removeFromTop(std::min(levelHeight, area.getHeight())));
    area.removeFromTop(std::min(gap, area.getHeight()));
    gateLabel.setBounds(area.removeFromTop(std::min(labelHeight, area.getHeight())));
    gateBox.setBounds(area.removeFromTop(std::min(gateHeight, area.getHeight())));
    noiseLabel.setBounds({});
    noiseBox.setBounds({});
    if (nativeNoise)
    {
        area.removeFromTop(gap);
        noiseLabel.setBounds(area.removeFromTop(labelHeight));
        noiseBox.setBounds(area.removeFromTop(gateHeight));
    }
}

ChipperMotionLab::ChipperMotionLab(ChipperAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setOpaque(true);
    setName("Motion Lab");
    setComponentID("motion.lab");
    setDescription("Edit the selected chip's eight-step pitch, level, and gate motion pattern.");
    setWantsKeyboardFocus(true);
    setExplicitFocusOrder(motionFocusOrderBase - 1);

    configureLabel(titleLabel, 20.0f, true);
    titleLabel.setText("MOTION LAB", juce::dontSendNotification);
    addAndMakeVisible(titleLabel);

    configureLabel(destinationLabel, 11.5f, true);
    destinationLabel.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(destinationLabel);

    configureLabel(statusLabel, 11.0f, false);
    statusLabel.setMinimumHorizontalScale(0.72f);
    addAndMakeVisible(statusLabel);

    closeButton.setComponentID("motion.close");
    closeButton.setName("Close Motion Lab");
    closeButton.setTooltip("Close Motion Lab and return to the chip editor.");
    closeButton.setWantsKeyboardFocus(true);
    closeButton.setExplicitFocusOrder(motionFocusOrderBase);
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible(closeButton);

    enableButton.setComponentID("motion.enabled");
    enableButton.setName("Enable tracker motion");
    enableButton.setTooltip("Enable this chip's tracker motion in Big Mono. Motion is retained but bypassed in Chip Poly.");
    enableButton.setWantsKeyboardFocus(true);
    enableButton.setExplicitFocusOrder(motionFocusOrderBase + 1);
    enableButton.onClick = [this]
    {
        if (updating)
            return;
        pattern.enabled = enableButton.getToggleState();
        commitPattern();
    };
    addAndMakeVisible(enableButton);

    configureLabel(rateLabel, 10.0f, true);
    rateLabel.setText("RATE", juce::dontSendNotification);
    addAndMakeVisible(rateLabel);
    for (const auto rate : { chipper::MotionRate::eighth,
                             chipper::MotionRate::sixteenth,
                             chipper::MotionRate::thirtySecond,
                             chipper::MotionRate::sixtyFourth })
        rateBox.addItem(stringFromView(chipper::motionRateName(rate)), rateBox.getNumItems() + 1);
    rateBox.setComponentID("motion.rate");
    rateBox.setName("Tracker motion rate");
    rateBox.setTooltip("Musical step rate, synchronized to host tempo when available.");
    rateBox.setWantsKeyboardFocus(true);
    rateBox.setExplicitFocusOrder(motionFocusOrderBase + 2);
    rateBox.onChange = [this]
    {
        if (updating)
            return;
        pattern.rate = rateForId(rateBox.getSelectedId());
        commitPattern();
    };
    addAndMakeVisible(rateBox);

    configureLabel(lengthLabel, 10.0f, true);
    lengthLabel.setText("LENGTH", juce::dontSendNotification);
    addAndMakeVisible(lengthLabel);
    for (int length = 1; length <= static_cast<int>(chipper::motionStepCount); ++length)
        lengthBox.addItem(juce::String(length), length);
    lengthBox.setComponentID("motion.length");
    lengthBox.setName("Tracker motion length");
    lengthBox.setTooltip("Number of active steps before the pattern loops.");
    lengthBox.setWantsKeyboardFocus(true);
    lengthBox.setExplicitFocusOrder(motionFocusOrderBase + 3);
    lengthBox.onChange = [this]
    {
        if (updating)
            return;
        pattern.length = static_cast<uint8_t>(std::clamp(lengthBox.getSelectedId(),
                                                         1,
                                                         static_cast<int>(chipper::motionStepCount)));
        commitPattern();
        updatePatternControls();
    };
    addAndMakeVisible(lengthBox);

    configureLabel(templateLabel, 10.0f, true);
    templateLabel.setText("TEMPLATES", juce::dontSendNotification);
    addAndMakeVisible(templateLabel);

    for (size_t index = 0; index < templateButtons.size(); ++index)
    {
        auto& button = templateButtons[index];
        button.setButtonText(templateLabels[index]);
        button.setComponentID("motion.template." + juce::String(templateLabels[index]).toLowerCase());
        button.setName("Apply " + juce::String(templateLabels[index]) + " motion template");
        button.setTooltip(index == 0u
                              ? "Clear this chip's motion pattern and disable it."
                              : "Apply the " + juce::String(templateLabels[index]) + " eight-step motion template.");
        button.setWantsKeyboardFocus(true);
        button.setExplicitFocusOrder(motionFocusOrderBase + 4 + static_cast<int>(index));
        button.onClick = [this, index] { applyTemplate(templateKinds[index]); };
        addAndMakeVisible(button);
    }

    for (size_t index = 0; index < stepColumns.size(); ++index)
    {
        auto& column = stepColumns[index];
        column.setComponentID("motion.step." + juce::String(static_cast<int>(index + 1u)));
        column.onChanged = [this, index]
        {
            if (updating)
                return;
            pattern.steps[index] = stepColumns[index].step();
            commitPattern();
        };
        column.setExplicitFocusOrder(motionFocusOrderBase + 20 + static_cast<int>(index * 4u));
        addAndMakeVisible(column);
    }

    refresh();
}

void ChipperMotionLab::configureLabel(juce::Label& label, float fontSize, bool bold)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(fontSize, bold ? juce::Font::bold : juce::Font::plain));
}

void ChipperMotionLab::setTheme(const Theme& newTheme)
{
    theme = newTheme;
    for (auto* label : { &titleLabel, &destinationLabel, &statusLabel, &rateLabel, &lengthLabel, &templateLabel })
        label->setColour(juce::Label::textColourId, label == &titleLabel ? theme.primary : theme.text);
    statusLabel.setColour(juce::Label::textColourId, theme.mutedText);

    closeButton.setColour(juce::TextButton::buttonColourId, theme.outline.darker(0.35f));
    closeButton.setColour(juce::TextButton::textColourOffId, theme.text);
    enableButton.setColour(juce::ToggleButton::textColourId, theme.text);
    enableButton.setColour(juce::ToggleButton::tickColourId, theme.primary);
    enableButton.setColour(juce::ToggleButton::tickDisabledColourId, theme.mutedText);
    for (auto& button : templateButtons)
    {
        button.setColour(juce::TextButton::buttonColourId, theme.outline.darker(0.38f).interpolatedWith(theme.accent, 0.08f));
        button.setColour(juce::TextButton::textColourOffId, theme.text);
    }
    for (auto& column : stepColumns)
        column.setTheme(theme);
    repaint();
}

void ChipperMotionLab::setMode(chipper::ChipMode newMode)
{
    if (mode == newMode && lastRevision != std::numeric_limits<uint64_t>::max())
        return;

    mode = newMode;
    lastRevision = std::numeric_limits<uint64_t>::max();
    lastActiveStep = std::numeric_limits<int>::min();
    refresh();
}

void ChipperMotionLab::refresh()
{
    const auto snapshot = processor.motionSnapshot(mode);
    if (snapshot.revision != lastRevision)
    {
        pattern = snapshot.pattern;
        lastRevision = snapshot.revision;
        updatePatternControls();
    }

    updateRuntimeReadout(snapshot.bpm, snapshot.hostTempo, snapshot.bypassedForChipPoly);
    if (snapshot.activeStep != lastActiveStep)
    {
        lastActiveStep = snapshot.activeStep;
        for (size_t index = 0; index < stepColumns.size(); ++index)
            stepColumns[index].setStep(index,
                                       pattern.steps[index],
                                       index < pattern.length,
                                       static_cast<int>(index) == snapshot.activeStep, mode == chipper::ChipMode::ym2149);
    }
}

void ChipperMotionLab::open(chipper::ChipMode newMode)
{
    setMode(newMode);
    setVisible(true);
    toFront(false);
    enableButton.grabKeyboardFocus();
}

void ChipperMotionLab::close()
{
    if (! isVisible())
        return;

    setVisible(false);
    if (onClose)
        onClose();
}

bool ChipperMotionLab::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }
    return juce::Component::keyPressed(key);
}

void ChipperMotionLab::updatePatternControls()
{
    updating = true;
    enableButton.setToggleState(pattern.enabled, juce::dontSendNotification);
    rateBox.setSelectedId(idForRate(pattern.rate), juce::dontSendNotification);
    lengthBox.setSelectedId(static_cast<int>(pattern.length), juce::dontSendNotification);
    for (size_t index = 0; index < stepColumns.size(); ++index)
        stepColumns[index].setStep(index,
                                   pattern.steps[index],
                                   index < pattern.length,
                                   static_cast<int>(index) == lastActiveStep, mode == chipper::ChipMode::ym2149);
    updating = false;
}

void ChipperMotionLab::updateRuntimeReadout(double bpm, bool hostTempo, bool bypassedForChipPoly)
{
    const auto& descriptor = chipper::descriptorFor(mode);
    destinationLabel.setText(juce::String(descriptor.displayName)
                                 + "  ->  "
                                 + stringFromView(chipper::motionDestinationForMode(mode)),
                             juce::dontSendNotification);

    const auto tempoSource = hostTempo ? "Host" : "Fallback";
    auto status = juce::String(tempoSource) + " "
        + juce::String(bpm, 1) + " BPM  |  "
        + stringFromView(chipper::motionRateName(pattern.rate)) + " x "
        + juce::String(static_cast<int>(pattern.length)) + " steps";
    if (bypassedForChipPoly)
        status += "  |  Bypassed in Chip Poly - switch Note Allocation to Big Mono";
    else if (pattern.enabled)
        status += "  |  Running in Big Mono";
    else
        status += "  |  Disabled - pattern stays saved with this chip";
    statusLabel.setText(status, juce::dontSendNotification);
    statusLabel.setTooltip("Motion targets " + destinationLabel.getText()
                           + ". Host tempo is used when available; otherwise Chipper falls back to 120 BPM.");
}

void ChipperMotionLab::commitPattern()
{
    pattern = chipper::sanitizeMotionPattern(pattern);
    processor.setMotionPattern(mode, pattern);
    lastRevision = processor.motionRevision(mode);
    const auto snapshot = processor.motionSnapshot(mode);
    updateRuntimeReadout(snapshot.bpm, snapshot.hostTempo, snapshot.bypassedForChipPoly);
    repaint();
}

void ChipperMotionLab::applyTemplate(chipper::MotionTemplate type)
{
    if (type == chipper::MotionTemplate::init)
    {
        pattern = {};
    }
    else
    {
        const auto rate = pattern.rate;
        const auto length = pattern.length;
        pattern = chipper::motionPatternTemplate(type);
        pattern.rate = rate;
        pattern.length = length;
    }
    commitPattern();
    updatePatternControls();
}

chipper::MotionRate ChipperMotionLab::rateForId(int id) noexcept
{
    switch (id)
    {
        case 1: return chipper::MotionRate::eighth;
        case 2: return chipper::MotionRate::sixteenth;
        case 3: return chipper::MotionRate::thirtySecond;
        case 4: return chipper::MotionRate::sixtyFourth;
        default: return chipper::MotionRate::sixteenth;
    }
}

int ChipperMotionLab::idForRate(chipper::MotionRate rate) noexcept
{
    switch (rate)
    {
        case chipper::MotionRate::eighth: return 1;
        case chipper::MotionRate::sixteenth: return 2;
        case chipper::MotionRate::thirtySecond: return 3;
        case chipper::MotionRate::sixtyFourth: return 4;
    }
    return 2;
}

void ChipperMotionLab::paint(juce::Graphics& graphics)
{
    graphics.fillAll(theme.background);
    drawPanel(graphics, headerPanelBounds, theme.panel, theme.outline);
    drawPanel(graphics, toolbarPanelBounds, theme.panel, theme.outline);
    drawPanel(graphics, stepGridBounds, theme.panel.darker(0.08f), theme.outline);
}

void ChipperMotionLab::resized()
{
    auto area = getLocalBounds().reduced(10);
    constexpr auto sectionGap = 8;

    headerPanelBounds = area.removeFromTop(std::min(58, area.getHeight()));
    auto header = headerPanelBounds.reduced(12, 8);
    closeButton.setBounds(header.removeFromRight(std::min(82, header.getWidth())));
    header.removeFromRight(std::min(10, header.getWidth()));
    titleLabel.setBounds(header.removeFromLeft(std::min(150, header.getWidth())));
    header.removeFromLeft(std::min(10, header.getWidth()));
    auto destination = header.removeFromTop(std::min(22, header.getHeight()));
    destinationLabel.setBounds(destination);
    statusLabel.setBounds(header);

    area.removeFromTop(std::min(sectionGap, area.getHeight()));
    toolbarPanelBounds = area.removeFromTop(std::min(68, area.getHeight()));
    auto toolbar = toolbarPanelBounds.reduced(10, 7);
    constexpr auto fieldGap = 8;
    enableButton.setBounds(toolbar.removeFromLeft(std::min(112, toolbar.getWidth())).withTrimmedTop(13));
    toolbar.removeFromLeft(std::min(fieldGap, toolbar.getWidth()));

    auto rateArea = toolbar.removeFromLeft(std::min(94, toolbar.getWidth()));
    rateLabel.setBounds(rateArea.removeFromTop(std::min(15, rateArea.getHeight())));
    rateBox.setBounds(rateArea.removeFromTop(std::min(32, rateArea.getHeight())));
    toolbar.removeFromLeft(std::min(fieldGap, toolbar.getWidth()));

    auto lengthArea = toolbar.removeFromLeft(std::min(76, toolbar.getWidth()));
    lengthLabel.setBounds(lengthArea.removeFromTop(std::min(15, lengthArea.getHeight())));
    lengthBox.setBounds(lengthArea.removeFromTop(std::min(32, lengthArea.getHeight())));
    toolbar.removeFromLeft(std::min(12, toolbar.getWidth()));

    auto templateArea = toolbar;
    templateLabel.setBounds(templateArea.removeFromTop(std::min(15, templateArea.getHeight())));
    constexpr auto templateGap = 5;
    const auto totalGaps = templateGap * static_cast<int>(templateButtons.size() - 1u);
    const auto buttonWidth = std::max(42, (templateArea.getWidth() - totalGaps)
                                              / static_cast<int>(templateButtons.size()));
    for (size_t index = 0; index < templateButtons.size(); ++index)
    {
        const auto last = index + 1u == templateButtons.size();
        templateButtons[index].setBounds(last
                                             ? templateArea
                                             : templateArea.removeFromLeft(std::min(buttonWidth, templateArea.getWidth())));
        if (! last)
            templateArea.removeFromLeft(std::min(templateGap, templateArea.getWidth()));
    }

    area.removeFromTop(std::min(sectionGap, area.getHeight()));
    stepGridBounds = area;
    auto grid = stepGridBounds.reduced(8);
    constexpr auto columnGap = 7;
    const auto totalColumnGaps = columnGap * static_cast<int>(stepColumns.size() - 1u);
    const auto columnWidth = std::max(72, (grid.getWidth() - totalColumnGaps)
                                             / static_cast<int>(stepColumns.size()));
    for (size_t index = 0; index < stepColumns.size(); ++index)
    {
        const auto last = index + 1u == stepColumns.size();
        stepColumns[index].setBounds(last
                                         ? grid
                                         : grid.removeFromLeft(std::min(columnWidth, grid.getWidth())));
        if (! last)
            grid.removeFromLeft(std::min(columnGap, grid.getWidth()));
    }
}

juce::Rectangle<int> ChipperMotionLab::templateBoundsForTest(size_t index) const
{
    return index < templateButtons.size() ? templateButtons[index].getBounds() : juce::Rectangle<int> {};
}

juce::String ChipperMotionLab::stepTextForTest(size_t index) const
{
    return index < stepColumns.size() ? stepColumns[index].stepTextForTest() : juce::String();
}

juce::Rectangle<int> ChipperMotionLab::stepBoundsForTest(size_t index) const
{
    return index < stepColumns.size() ? stepColumns[index].getBounds() : juce::Rectangle<int> {};
}

juce::Rectangle<int> ChipperMotionLab::pitchBoundsForTest(size_t index) const
{
    return index < stepColumns.size()
        ? stepColumns[index].pitchBoundsForTest().translated(stepColumns[index].getX(), stepColumns[index].getY())
        : juce::Rectangle<int> {};
}

juce::Rectangle<int> ChipperMotionLab::levelBoundsForTest(size_t index) const
{
    return index < stepColumns.size()
        ? stepColumns[index].levelBoundsForTest().translated(stepColumns[index].getX(), stepColumns[index].getY())
        : juce::Rectangle<int> {};
}

juce::Rectangle<int> ChipperMotionLab::gateBoundsForTest(size_t index) const
{
    return index < stepColumns.size()
        ? stepColumns[index].gateBoundsForTest().translated(stepColumns[index].getX(), stepColumns[index].getY())
        : juce::Rectangle<int> {};
}

void ChipperMotionLab::setLengthForTest(int length)
{
    lengthBox.setSelectedId(std::clamp(length, 1, static_cast<int>(chipper::motionStepCount)),
                            juce::sendNotificationSync);
}

bool ChipperMotionLab::stepEnabledForTest(size_t index) const
{
    return index < stepColumns.size() && stepColumns[index].controlsEnabledForTest();
}
