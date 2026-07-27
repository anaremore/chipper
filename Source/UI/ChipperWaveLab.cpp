#include "ChipperWaveLab.h"

#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr auto waveLabFocusOrderBase = 340;

uint8_t scaledSample(uint8_t sample, uint8_t sourceMaximum, uint8_t targetMaximum)
{
    if (sourceMaximum == 0u)
        return 0u;

    const auto normalized = static_cast<double>(sample) / static_cast<double>(sourceMaximum);
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(normalized * targetMaximum)),
                                           0,
                                           static_cast<int>(targetMaximum)));
}
}

ChipperWaveLab::WaveCanvas::WaveCanvas()
{
    setName("Wave RAM drawing canvas");
    setComponentID("chipper-wave-lab-canvas");
    setDescription("Draw the selected native 32-sample Wave RAM lane. Arrow keys move and edit samples.");
    setTooltip("Draw the selected 32-sample native Wave RAM lane. Left/Right selects a sample; Up/Down edits it.");
    setWantsKeyboardFocus(true);
    setExplicitFocusOrder(waveLabFocusOrderBase + 5);
}

void ChipperWaveLab::WaveCanvas::setTheme(const Theme& newTheme)
{
    theme = newTheme;
    repaint();
}

void ChipperWaveLab::WaveCanvas::setSamples(const chipper::WavetableLane& newSamples,
                                            uint8_t newMaximumSampleValue,
                                            bool isCustom)
{
    const auto changed = samples != newSamples
        || maximumSampleValue != std::max<uint8_t>(1u, newMaximumSampleValue)
        || custom != isCustom;
    samples = newSamples;
    maximumSampleValue = std::max<uint8_t>(1u, newMaximumSampleValue);
    custom = isCustom;
    if (changed)
        repaint();
}

void ChipperWaveLab::WaveCanvas::setSampleForTest(size_t sampleIndex, uint8_t value)
{
    if (sampleIndex >= samples.size())
        return;

    const auto previous = samples;
    samples[sampleIndex] = std::min(value, maximumSampleValue);
    keyboardSample = sampleIndex;
    publishIfChanged(previous);
}

juce::Rectangle<float> ChipperWaveLab::WaveCanvas::plotBounds() const
{
    return getLocalBounds().toFloat().reduced(9.0f, 8.0f);
}

size_t ChipperWaveLab::WaveCanvas::sampleIndexForX(float x) const
{
    const auto plot = plotBounds();
    if (plot.isEmpty())
        return 0u;

    const auto normalized = std::clamp((x - plot.getX()) / std::max(1.0f, plot.getWidth()), 0.0f, 0.999999f);
    return std::min(samples.size() - 1u,
                    static_cast<size_t>(std::floor(normalized * static_cast<float>(samples.size()))));
}

uint8_t ChipperWaveLab::WaveCanvas::sampleValueForY(float y) const
{
    const auto plot = plotBounds();
    if (plot.isEmpty())
        return 0u;

    const auto normalized = std::clamp((plot.getBottom() - y) / std::max(1.0f, plot.getHeight()), 0.0f, 1.0f);
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(normalized * maximumSampleValue)),
                                           0,
                                           static_cast<int>(maximumSampleValue)));
}

void ChipperWaveLab::WaveCanvas::publishIfChanged(const chipper::WavetableLane& previous)
{
    if (samples == previous)
        return;

    custom = true;
    repaint();
    if (onSamplesChanged)
        onSamplesChanged(samples);
}

void ChipperWaveLab::WaveCanvas::applyPointer(float x, float y)
{
    const auto previous = samples;
    const auto sampleIndex = sampleIndexForX(x);
    const auto sampleValue = sampleValueForY(y);
    keyboardSample = sampleIndex;

    if (lastDragSample < 0 || static_cast<size_t>(lastDragSample) == sampleIndex)
    {
        samples[sampleIndex] = sampleValue;
    }
    else
    {
        const auto start = lastDragSample;
        const auto finish = static_cast<int>(sampleIndex);
        const auto startValue = static_cast<int>(samples[static_cast<size_t>(start)]);
        const auto distance = std::abs(finish - start);
        const auto direction = finish > start ? 1 : -1;
        for (auto step = 1; step <= distance; ++step)
        {
            const auto amount = static_cast<double>(step) / static_cast<double>(distance);
            const auto interpolated = static_cast<int>(std::lround(startValue
                + (static_cast<int>(sampleValue) - startValue) * amount));
            samples[static_cast<size_t>(start + direction * step)] = static_cast<uint8_t>(
                std::clamp(interpolated, 0, static_cast<int>(maximumSampleValue)));
        }
    }

    lastDragSample = static_cast<int>(sampleIndex);
    publishIfChanged(previous);
}

void ChipperWaveLab::WaveCanvas::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    dragging = true;
    lastDragSample = -1;
    applyPointer(event.position.x, event.position.y);
}

void ChipperWaveLab::WaveCanvas::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging)
        applyPointer(event.position.x, event.position.y);
}

void ChipperWaveLab::WaveCanvas::mouseUp(const juce::MouseEvent&)
{
    dragging = false;
    lastDragSample = -1;
}

bool ChipperWaveLab::WaveCanvas::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)
    {
        keyboardSample = keyboardSample == 0u ? samples.size() - 1u : keyboardSample - 1u;
        repaint();
        return true;
    }
    if (key == juce::KeyPress::rightKey)
    {
        keyboardSample = (keyboardSample + 1u) % samples.size();
        repaint();
        return true;
    }
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const auto previous = samples;
        const auto delta = key == juce::KeyPress::upKey ? 1 : -1;
        samples[keyboardSample] = static_cast<uint8_t>(std::clamp(
            static_cast<int>(samples[keyboardSample]) + delta,
            0,
            static_cast<int>(maximumSampleValue)));
        publishIfChanged(previous);
        return true;
    }
    if (key == juce::KeyPress::homeKey || key == juce::KeyPress::endKey)
    {
        const auto previous = samples;
        samples[keyboardSample] = key == juce::KeyPress::homeKey ? 0u : maximumSampleValue;
        publishIfChanged(previous);
        return true;
    }
    return false;
}

void ChipperWaveLab::WaveCanvas::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(theme.canvas);
    graphics.fillRoundedRectangle(bounds, 4.0f);
    graphics.setColour(theme.outline);
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    const auto plot = plotBounds();
    if (plot.isEmpty())
        return;

    graphics.setColour(theme.outline.withAlpha(0.52f));
    for (auto row = 0; row <= 4; ++row)
    {
        const auto y = plot.getY() + plot.getHeight() * static_cast<float>(row) / 4.0f;
        graphics.drawHorizontalLine(static_cast<int>(std::lround(y)), plot.getX(), plot.getRight());
    }
    for (size_t sample = 0; sample <= samples.size(); ++sample)
    {
        const auto x = plot.getX() + plot.getWidth() * static_cast<float>(sample) / static_cast<float>(samples.size());
        graphics.setColour(theme.outline.withAlpha(sample % 4u == 0u ? 0.52f : 0.20f));
        graphics.drawVerticalLine(static_cast<int>(std::lround(x)), plot.getY(), plot.getBottom());
    }

    const auto sampleWidth = plot.getWidth() / static_cast<float>(samples.size());
    const auto yForSample = [this, plot](uint8_t sample)
    {
        return plot.getBottom()
            - (static_cast<float>(sample) / static_cast<float>(maximumSampleValue)) * plot.getHeight();
    };

    juce::Path fill;
    juce::Path line;
    const auto firstX = plot.getX() + sampleWidth * 0.5f;
    const auto firstY = yForSample(samples.front());
    fill.startNewSubPath(plot.getX(), plot.getBottom());
    fill.lineTo(plot.getX(), firstY);
    line.startNewSubPath(firstX, firstY);
    for (size_t sample = 0; sample < samples.size(); ++sample)
    {
        const auto left = plot.getX() + sampleWidth * static_cast<float>(sample);
        const auto right = left + sampleWidth;
        const auto y = yForSample(samples[sample]);
        if (sample > 0u)
            line.lineTo(left, yForSample(samples[sample - 1u]));
        line.lineTo(left + sampleWidth * 0.5f, y);
        fill.lineTo(left, y);
        fill.lineTo(right, y);
    }
    fill.lineTo(plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    const auto waveColour = custom ? theme.accent : theme.primary;
    graphics.setColour(waveColour.withAlpha(custom ? 0.24f : 0.16f));
    graphics.fillPath(fill);
    graphics.setColour(waveColour.withAlpha(0.22f));
    graphics.strokePath(line, juce::PathStrokeType(4.0f));
    graphics.setColour(waveColour);
    graphics.strokePath(line, juce::PathStrokeType(1.5f));

    const auto markerX = plot.getX() + sampleWidth * (static_cast<float>(keyboardSample) + 0.5f);
    const auto markerY = yForSample(samples[keyboardSample]);
    graphics.setColour(theme.text);
    graphics.fillEllipse(markerX - 3.5f, markerY - 3.5f, 7.0f, 7.0f);
    if (hasKeyboardFocus(true))
    {
        graphics.setColour(theme.accent.withAlpha(0.82f));
        graphics.drawRoundedRectangle(bounds.reduced(2.0f), 3.0f, 1.5f);
    }

    graphics.setColour(theme.mutedText);
    graphics.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    graphics.drawText(juce::String(static_cast<int>(maximumSampleValue)),
                      plot.withHeight(12.0f).translated(4.0f, -1.0f),
                      juce::Justification::topLeft,
                      false);
    graphics.drawText("0",
                      plot.withTop(plot.getBottom() - 12.0f).translated(4.0f, 0.0f),
                      juce::Justification::bottomLeft,
                      false);
}

ChipperWaveLab::ChipperWaveLab(ChipperAudioProcessor& owner)
    : processor(owner),
      mode(chipper::ChipMode::nes)
{
    setName("Wave Lab");
    setComponentID("chipper-wave-lab");
    setDescription("Native 32-sample Wave RAM editor with copy, paste, audio import, reset, and project recall.");

    titleLabel.setText("WAVE LAB", juce::dontSendNotification);
    titleLabel.setName("Wave Lab");
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    laneBox.setName("Wave Lab lane");
    laneBox.setComponentID("chipper-wave-lab-lane");
    laneBox.setTooltip("Choose the native Wave RAM lane to edit.");
    laneBox.setWantsKeyboardFocus(true);
    laneBox.setExplicitFocusOrder(waveLabFocusOrderBase);
    laneBox.onChange = [this]
    {
        const auto selected = laneBox.getSelectedId() - 1;
        if (selected >= 0)
        {
            selectedLane = static_cast<size_t>(selected);
            refresh();
        }
    };
    addAndMakeVisible(laneBox);

    statusLabel.setName("Wave Lab status");
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::FontOptions(11.0f));
    statusLabel.setMinimumHorizontalScale(0.65f);
    addAndMakeVisible(statusLabel);

    const auto prepareButton = [this](juce::TextButton& button,
                                      const char* id,
                                      const char* tooltip,
                                      int focusOffset)
    {
        button.setName(button.getButtonText());
        button.setComponentID(id);
        button.setTooltip(tooltip);
        button.setWantsKeyboardFocus(true);
        button.setExplicitFocusOrder(waveLabFocusOrderBase + focusOffset);
        addAndMakeVisible(button);
    };
    prepareButton(copyButton, "chipper-wave-lab-copy", "Copy this native 32-sample lane.", 1);
    prepareButton(pasteButton, "chipper-wave-lab-paste", "Paste and rescale a copied lane to this chip's native depth.", 2);
    prepareButton(importButton, "chipper-wave-lab-import", "Import WAV or AIFF audio and resample it to one 32-step cycle.", 3);
    prepareButton(resetButton, "chipper-wave-lab-reset", "Return this lane to its generated selector-backed wave.", 4);

    copyButton.onClick = [this] { copyLane(); };
    pasteButton.onClick = [this] { pasteLane(); };
    importButton.onClick = [this] { chooseAudioFile(); };
    resetButton.onClick = [this] { resetLane(); };

    canvas.onSamplesChanged = [this](const auto& lane) { commitLane(lane); };
    addAndMakeVisible(canvas);

    setMode(mode);
}

void ChipperWaveLab::setTheme(Theme newTheme)
{
    theme = newTheme;
    titleLabel.setColour(juce::Label::textColourId, theme.accent);
    statusLabel.setColour(juce::Label::textColourId, theme.mutedText);
    laneBox.setColour(juce::ComboBox::backgroundColourId, theme.canvas);
    laneBox.setColour(juce::ComboBox::textColourId, theme.text);
    laneBox.setColour(juce::ComboBox::outlineColourId, theme.outline);
    for (auto* button : { &copyButton, &pasteButton, &importButton, &resetButton })
    {
        button->setColour(juce::TextButton::buttonColourId, theme.canvas);
        button->setColour(juce::TextButton::buttonOnColourId, theme.primary);
        button->setColour(juce::TextButton::textColourOffId, theme.text);
        button->setColour(juce::TextButton::textColourOnId, theme.text);
    }
    canvas.setTheme(theme);
    repaint();
}

juce::String ChipperWaveLab::laneNoun() const
{
    return mode == chipper::ChipMode::namcoWsg ? "Lane" : "Ch";
}

void ChipperWaveLab::setMode(chipper::ChipMode newMode)
{
    const auto newSpec = chipper::wavetableSpecForMode(newMode);
    const auto changed = newMode != mode || newSpec.laneCount != spec.laneCount;
    mode = newMode;
    spec = newSpec;
    const auto supported = chipper::supportsDirectWavetableEditing(mode);
    setEnabled(supported);
    setVisible(supported);
    if (! changed || spec.laneCount == 0u)
        return;

    selectedLane = std::min(selectedLane, spec.laneCount - 1u);
    laneBox.clear(juce::dontSendNotification);
    for (size_t lane = 0; lane < spec.laneCount; ++lane)
        laneBox.addItem(laneNoun() + " " + juce::String(static_cast<int>(lane + 1u)), static_cast<int>(lane + 1u));
    laneBox.setSelectedId(static_cast<int>(selectedLane + 1u), juce::dontSendNotification);
    refresh();
}

void ChipperWaveLab::selectLane(size_t lane)
{
    if (spec.laneCount == 0u)
        return;

    selectedLane = std::min(lane, spec.laneCount - 1u);
    laneBox.setSelectedId(static_cast<int>(selectedLane + 1u), juce::dontSendNotification);
    refresh();
}

void ChipperWaveLab::updateStatus(const juce::String& prefix)
{
    auto status = prefix;
    if (status.isNotEmpty())
        status << "  |  ";
    status << (currentLaneIsCustom ? "Custom" : "Generated")
           << "  |  32 x " << static_cast<int>(spec.bitDepth) << "-bit"
           << "  |  0-" << static_cast<int>(spec.maximumSampleValue)
           << "  |  saved with project";
    statusLabel.setText(status, juce::dontSendNotification);
    statusLabel.setTooltip(status);
    pasteButton.setEnabled(hasCopiedLane);
    resetButton.setEnabled(currentLaneIsCustom);
}

void ChipperWaveLab::refresh()
{
    if (spec.laneCount == 0u || selectedLane >= spec.laneCount)
        return;

    const auto snapshot = processor.wavetableSnapshot(mode, selectedLane);
    currentLaneIsCustom = snapshot.custom;
    canvas.setSamples(snapshot.samples, snapshot.maximumSampleValue, snapshot.custom);
    updateStatus();
}

void ChipperWaveLab::commitLane(const chipper::WavetableLane& lane)
{
    if (processor.setWavetableLane(mode, selectedLane, lane))
    {
        currentLaneIsCustom = true;
        updateStatus("Live");
    }
}

void ChipperWaveLab::copyLane()
{
    const auto snapshot = processor.wavetableSnapshot(mode, selectedLane);
    copiedLane = snapshot.samples;
    copiedMaximumSampleValue = snapshot.maximumSampleValue;
    hasCopiedLane = true;
    updateStatus("Copied " + laneNoun() + " " + juce::String(static_cast<int>(selectedLane + 1u)));
}

void ChipperWaveLab::pasteLane()
{
    if (! hasCopiedLane || copiedMaximumSampleValue == 0u)
        return;

    chipper::WavetableLane pasted {};
    for (size_t sample = 0; sample < pasted.size(); ++sample)
        pasted[sample] = scaledSample(copiedLane[sample], copiedMaximumSampleValue, spec.maximumSampleValue);
    commitLane(pasted);
    refresh();
    updateStatus("Pasted");
}

void ChipperWaveLab::resetLane()
{
    processor.resetWavetableLane(mode, selectedLane);
    refresh();
    updateStatus("Reset");
}

void ChipperWaveLab::chooseAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser>("Import one cycle into Wave Lab",
                                                       juce::File {},
                                                       "*.wav;*.aif;*.aiff");
    juce::Component::SafePointer<ChipperWaveLab> safeThis(this);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safeThis](const juce::FileChooser& chooser)
                             {
                                 if (safeThis == nullptr)
                                     return;
                                 const auto file = chooser.getResult();
                                 if (file != juce::File {})
                                     safeThis->importAudioFile(file);
                             });
}

void ChipperWaveLab::importAudioFile(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
    {
        updateStatus("Import failed");
        return;
    }

    const auto channelCount = std::clamp(static_cast<int>(reader->numChannels), 1, 2);
    juce::AudioBuffer<float> sampleBuffer(channelCount, 1);
    std::array<float, chipper::wavetableSampleCount> imported {};
    auto peak = 0.0f;
    for (size_t sample = 0; sample < imported.size(); ++sample)
    {
        const auto position = imported.size() > 1u
            ? static_cast<juce::int64>(std::llround(
                static_cast<double>(reader->lengthInSamples - 1)
                * static_cast<double>(sample)
                / static_cast<double>(imported.size() - 1u)))
            : 0;
        sampleBuffer.clear();
        if (! reader->read(&sampleBuffer, 0, 1, position, true, channelCount > 1))
            continue;

        auto value = 0.0f;
        for (auto channel = 0; channel < channelCount; ++channel)
            value += sampleBuffer.getSample(channel, 0);
        value /= static_cast<float>(channelCount);
        imported[sample] = value;
        peak = std::max(peak, std::abs(value));
    }

    chipper::WavetableLane lane {};
    if (peak < 1.0e-6f)
    {
        lane.fill(static_cast<uint8_t>(spec.maximumSampleValue / 2u));
    }
    else
    {
        for (size_t sample = 0; sample < lane.size(); ++sample)
        {
            const auto normalized = std::clamp(imported[sample] / peak, -1.0f, 1.0f);
            lane[sample] = static_cast<uint8_t>(std::clamp(
                static_cast<int>(std::lround((normalized * 0.5f + 0.5f) * spec.maximumSampleValue)),
                0,
                static_cast<int>(spec.maximumSampleValue)));
        }
    }

    commitLane(lane);
    refresh();
    updateStatus("Imported " + file.getFileName());
}

void ChipperWaveLab::setSampleForTest(size_t sampleIndex, uint8_t value)
{
    canvas.setSampleForTest(sampleIndex, value);
}

void ChipperWaveLab::resetForTest()
{
    resetLane();
}

void ChipperWaveLab::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(theme.panel);
    graphics.fillRoundedRectangle(bounds, 6.0f);
    graphics.setColour(theme.outline);
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
}

void ChipperWaveLab::resized()
{
    auto area = getLocalBounds().reduced(12, 9);
    auto header = area.removeFromTop(std::min(28, area.getHeight()));
    titleLabel.setBounds(header.removeFromLeft(std::min(82, header.getWidth())));
    header.removeFromLeft(std::min(8, header.getWidth()));
    laneBox.setBounds(header.removeFromLeft(std::min(128, header.getWidth())));
    header.removeFromLeft(std::min(10, header.getWidth()));

    constexpr auto buttonGap = 6;
    constexpr auto compactButtonWidth = 58;
    constexpr auto importButtonWidth = 68;
    const auto takeButton = [&header](int width)
    {
        return header.removeFromRight(std::min(width, header.getWidth()));
    };
    resetButton.setBounds(takeButton(compactButtonWidth));
    header.removeFromRight(std::min(buttonGap, header.getWidth()));
    importButton.setBounds(takeButton(importButtonWidth));
    header.removeFromRight(std::min(buttonGap, header.getWidth()));
    pasteButton.setBounds(takeButton(compactButtonWidth));
    header.removeFromRight(std::min(buttonGap, header.getWidth()));
    copyButton.setBounds(takeButton(compactButtonWidth));
    header.removeFromRight(std::min(10, header.getWidth()));
    statusLabel.setBounds(header);

    area.removeFromTop(std::min(7, area.getHeight()));
    canvas.setBounds(area);
}
