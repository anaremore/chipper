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
    }
}

void ChipperFmEditor::configure(chipper::ChipMode modeToUse, size_t visibleOperators, bool editableOperators)
{
    mode = modeToUse;
    operatorRows = std::min(operatorCount, visibleOperators);
    editable = editableOperators;
    gridLayout = editable && operatorRows == operatorCount;
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
    if (! gridLayout)
        return;

    for (size_t i = 0; i < operatorRows; ++i)
    {
        const auto bounds = operatorCards[i];
        if (bounds.isEmpty())
            continue;
        const auto carrier = controls.names[i].getText().endsWith(" C");
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

void ChipperFmEditor::layoutCompactRows()
{
    auto bounds = getLocalBounds();
    const auto rowGap = 4;
    const auto rowHeight = operatorRows > 0
        ? std::clamp((bounds.getHeight() - rowGap * (static_cast<int>(operatorRows) - 1)) / static_cast<int>(operatorRows), 13, 22)
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
        controls.names[i].setBounds(row.removeFromLeft(std::min(48, row.getWidth())));
        row.removeFromLeft(std::min(5, row.getWidth()));
        if (editable)
        {
            controls.levelReadouts[i].setBounds(row.removeFromLeft(std::min(38, row.getWidth())));
            row.removeFromLeft(std::min(4, row.getWidth()));
            controls.levelSliders[i].setBounds(row.removeFromLeft(std::min(78, row.getWidth())).reduced(0, 1));
            row.removeFromLeft(std::min(4, row.getWidth()));
            controls.multipliers[i].setBounds(row.removeFromLeft(std::min(50, row.getWidth())));
            row.removeFromLeft(std::min(4, row.getWidth()));
            controls.envelopes[i].setBounds(row.removeFromLeft(std::min(46, row.getWidth())));
            row.removeFromLeft(std::min(4, row.getWidth()));
        }
        else
        {
            controls.levelReadouts[i].setBounds({});
            controls.levelSliders[i].setBounds({});
            controls.multipliers[i].setBounds({});
            controls.envelopes[i].setBounds({});
        }
        controls.registerReadouts[i].setBounds(row.reduced(2, 0));
        bounds.removeFromTop(std::min(rowGap, bounds.getHeight()));
    }
}
