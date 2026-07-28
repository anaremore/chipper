#include "ChipperFmEditor.h"

#include <algorithm>

ChipperFmEditor::ChipperFmEditor(Controls controlsToUse)
    : controls(controlsToUse)
{
    setInterceptsMouseClicks(false, true);
    setComponentID("fm.operatorEditor");
}

void ChipperFmEditor::attachControls()
{
    for (size_t i = 0; i < operatorCount; ++i)
    {
        addAndMakeVisible(controls.names[i]);
        addAndMakeVisible(controls.registerReadouts[i]);
        addAndMakeVisible(controls.levelReadouts[i]);
        addAndMakeVisible(controls.levelSliders[i]);
        addAndMakeVisible(controls.multipliers[i]);
        addAndMakeVisible(controls.envelopes[i]);
        addAndMakeVisible(controls.detunes[i]);
    }
}

void ChipperFmEditor::configure(chipper::ChipMode modeToUse, size_t visibleOperators, bool editableOperators)
{
    mode = modeToUse;
    operatorRows = std::min(operatorCount, visibleOperators);
    editable = editableOperators;
    gridLayout = editable && operatorRows == operatorCount;
    twoOperatorGrid = editable && operatorRows == 2u;
    resized();
    repaint();
}

void ChipperFmEditor::setTheme(juce::Colour panel,
                               juce::Colour card,
                               juce::Colour outline,
                               juce::Colour primary,
                               juce::Colour accent,
                               juce::Colour text,
                               juce::Colour mutedText)
{
    panelColour = panel;
    cardColour = card;
    outlineColour = outline;
    primaryColour = primary;
    accentColour = accent;
    textColour = text;
    mutedTextColour = mutedText;
    repaint();
}

void ChipperFmEditor::paint(juce::Graphics& graphics)
{
    if (! gridLayout && ! twoOperatorGrid && mode != chipper::ChipMode::opl3)
        return;

    for (size_t i = 0; i < operatorRows; ++i)
    {
        const auto bounds = operatorCards[i];
        if (bounds.isEmpty())
            continue;
        const auto carrier = controls.names[i].getText().endsWith(" C")
            || ((mode == chipper::ChipMode::ym2413 || mode == chipper::ChipMode::nesVrc7) && i == 1u);
        graphics.setColour(cardColour);
        graphics.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        graphics.setColour(carrier ? primaryColour : accentColour);
        graphics.fillRoundedRectangle(bounds.withWidth(3).toFloat(), 2.0f);
        graphics.setColour((carrier ? primaryColour : outlineColour).withAlpha(carrier ? 0.9f : 1.0f));
        graphics.drawRoundedRectangle(operatorCards[i].toFloat().reduced(0.5f), 4.0f, carrier ? 1.2f : 1.0f);
    }
}

void ChipperFmEditor::resized()
{
    operatorCards.fill({});
    if (gridLayout)
        layoutFourOperatorGrid();
    else if (twoOperatorGrid)
        layoutTwoOperatorGrid();
    else
        layoutCompactRows();
}

void ChipperFmEditor::clearControlBounds(size_t index)
{
    controls.names[index].setBounds({});
    controls.registerReadouts[index].setBounds({});
    controls.levelReadouts[index].setBounds({});
    controls.levelSliders[index].setBounds({});
    controls.multipliers[index].setBounds({});
    controls.envelopes[index].setBounds({});
    controls.detunes[index].setBounds({});
}

void ChipperFmEditor::layoutFourOperatorGrid()
{
    auto area = getLocalBounds();
    constexpr auto gap = 6;
    const auto cardWidth = std::max(0, (area.getWidth() - gap) / 2);
    const auto cardHeight = std::max(0, (area.getHeight() - gap) / 2);
    for (size_t i = 0; i < operatorCount; ++i)
    {
        const auto row = static_cast<int>(i / 2u);
        const auto column = static_cast<int>(i % 2u);
        operatorCards[i] = {
            area.getX() + column * (cardWidth + gap),
            area.getY() + row * (cardHeight + gap),
            cardWidth,
            cardHeight
        };

        auto content = operatorCards[i].reduced(8, 3);
        auto header = content.removeFromTop(std::min(15, content.getHeight()));
        controls.names[i].setBounds(header.removeFromLeft(std::min(82, header.getWidth())));
        controls.levelReadouts[i].setBounds(header);
        content.removeFromTop(std::min(1, content.getHeight()));
        auto registerRow = content.removeFromBottom(std::min(14, content.getHeight()));
        if (mode == chipper::ChipMode::ym2151)
        {
            auto detuneRow = content.removeFromBottom(std::min(18, content.getHeight()));
            controls.detunes[i].setBounds(detuneRow);
            content.removeFromBottom(std::min(3, content.getHeight()));
        }
        else
        {
            controls.detunes[i].setBounds({});
        }
        controls.registerReadouts[i].setBounds(registerRow.reduced(2, 0));
        content.removeFromBottom(std::min(1, content.getHeight()));

        auto controlsRow = content;
        const auto envelopeWidth = std::min(46, controlsRow.getWidth());
        controls.envelopes[i].setBounds(controlsRow.removeFromRight(envelopeWidth));
        controlsRow.removeFromRight(std::min(4, controlsRow.getWidth()));
        const auto multiplierWidth = std::min(54, controlsRow.getWidth());
        controls.multipliers[i].setBounds(controlsRow.removeFromRight(multiplierWidth));
        controlsRow.removeFromRight(std::min(6, controlsRow.getWidth()));
        controls.levelSliders[i].setBounds(controlsRow.reduced(0, 1));
    }
}

void ChipperFmEditor::layoutTwoOperatorGrid()
{
    auto area = getLocalBounds();
    constexpr auto gap = 6;
    const auto cardHeight = std::max(0, (area.getHeight() - gap) / 2);
    for (size_t i = 0; i < operatorCount; ++i)
    {
        if (i >= operatorRows)
        {
            clearControlBounds(i);
            continue;
        }

        operatorCards[i] = {
            area.getX(),
            area.getY() + static_cast<int>(i) * (cardHeight + gap),
            area.getWidth(),
            cardHeight
        };

        auto content = operatorCards[i].reduced(9, 5);
        controls.detunes[i].setBounds({});
        auto header = content.removeFromTop(std::min(16, content.getHeight()));
        controls.names[i].setBounds(header.removeFromLeft(std::min(82, header.getWidth())));
        controls.levelReadouts[i].setBounds(header);
        content.removeFromTop(std::min(3, content.getHeight()));

        auto registerRow = content.removeFromBottom(std::min(17, content.getHeight()));
        controls.registerReadouts[i].setBounds(registerRow.reduced(2, 0));
        content.removeFromBottom(std::min(4, content.getHeight()));

        auto controlsRow = content.removeFromTop(std::min(30, content.getHeight()));
        const auto envelopeWidth = std::min(72, controlsRow.getWidth());
        controls.envelopes[i].setBounds(controlsRow.removeFromRight(envelopeWidth));
        controlsRow.removeFromRight(std::min(6, controlsRow.getWidth()));
        const auto multiplierWidth = std::min(82, controlsRow.getWidth());
        controls.multipliers[i].setBounds(controlsRow.removeFromRight(multiplierWidth));
        controlsRow.removeFromRight(std::min(8, controlsRow.getWidth()));
        controls.levelSliders[i].setBounds(controlsRow.reduced(0, 3));
    }
}

void ChipperFmEditor::layoutCompactRows()
{
    auto bounds = getLocalBounds();
    const auto rowGap = 4;
    const auto maximumRowHeight = mode == chipper::ChipMode::opl3 ? 48 : 22;
    const auto rowHeight = operatorRows > 0
        ? std::clamp((bounds.getHeight() - rowGap * (static_cast<int>(operatorRows) - 1)) / static_cast<int>(operatorRows), 13, maximumRowHeight)
        : 0;
    for (size_t i = 0; i < operatorCount; ++i)
    {
        if (i >= operatorRows)
        {
            clearControlBounds(i);
            continue;
        }
        auto row = bounds.removeFromTop(std::min(rowHeight, bounds.getHeight()));
        operatorCards[i] = row;
        auto content = mode == chipper::ChipMode::opl3 ? row.reduced(8, 4) : row;
        controls.names[i].setBounds(content.removeFromLeft(std::min(48, content.getWidth())));
        controls.detunes[i].setBounds({});
        content.removeFromLeft(std::min(5, content.getWidth()));
        if (editable)
        {
            controls.levelReadouts[i].setBounds(content.removeFromLeft(std::min(38, content.getWidth())));
            content.removeFromLeft(std::min(4, content.getWidth()));
            controls.levelSliders[i].setBounds(content.removeFromLeft(std::min(78, content.getWidth())).reduced(0, 1));
            content.removeFromLeft(std::min(4, content.getWidth()));
            controls.multipliers[i].setBounds(content.removeFromLeft(std::min(50, content.getWidth())));
            content.removeFromLeft(std::min(4, content.getWidth()));
            controls.envelopes[i].setBounds(content.removeFromLeft(std::min(46, content.getWidth())));
            content.removeFromLeft(std::min(4, content.getWidth()));
        }
        else
        {
            controls.levelReadouts[i].setBounds({});
            controls.levelSliders[i].setBounds({});
            controls.multipliers[i].setBounds({});
            controls.envelopes[i].setBounds({});
        }
        controls.registerReadouts[i].setBounds(content.reduced(2, 0));
        bounds.removeFromTop(std::min(rowGap, bounds.getHeight()));
    }
}
