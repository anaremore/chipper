#include "ChipperFmEditor.h"
#include "Parameters.h"

#include <algorithm>

juce::PopupMenu ChipperFmEditor::createEnvelopeMenu(chipper::ChipMode mode, const std::array<int, 4>& selections)
{
    const bool fourBitRates = mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2413
        || mode == chipper::ChipMode::nesVrc7;
    const std::array<juce::StringArray, 4> choices {
        chipper::parameters::fmOperatorAttackRateChoices(), chipper::parameters::fmOperatorDecayRateChoices(),
        chipper::parameters::fmOperatorSustainRateChoices(), chipper::parameters::fmOperatorReleaseRateChoices() };
    const std::array<juce::String, 4> names { "Attack rate", "Decay rate", fourBitRates ? "Sustain level" : "Sustain rate", "Release rate" };
    const std::array<juce::String, 4> prefixes { "AR ", "DR ", fourBitRates ? "SL " : "D2R ", "RR " };
    juce::PopupMenu menu;
    for (size_t field = 0; field < choices.size(); ++field)
    {
        juce::PopupMenu values;
        const int count = fourBitRates ? std::min(17, choices[field].size()) : choices[field].size();
        // A wider OPN override may remain stored when switching to OPL/OPLL.
        // Show the native clamped value without rewriting the shared parameter.
        const int selected = std::clamp(selections[field], 0, count - 1);
        for (int choice = 0; choice < count; ++choice)
            values.addItem(static_cast<int>(field) * 100 + choice + 1,
                           choice == 0 ? juce::String("Follow") : prefixes[field] + choices[field][choice],
                           true, choice == selected);
        menu.addSubMenu(names[field] + ": " + choices[field][selected], values);
    }
    return menu;
}

ChipperFmEditor::ChipperFmEditor(juce::AudioProcessorValueTreeState& state)
{
    setInterceptsMouseClicks(false, true);
    setComponentID("fm.operatorEditor");
    static const std::array<const char*, operatorCount> levelIds {
        chipper::parameters::id::fmOperator1Level, chipper::parameters::id::fmOperator2Level,
        chipper::parameters::id::fmOperator3Level, chipper::parameters::id::fmOperator4Level };
    static const std::array<const char*, operatorCount> multiplierIds {
        chipper::parameters::id::fmOperator1Multiplier, chipper::parameters::id::fmOperator2Multiplier,
        chipper::parameters::id::fmOperator3Multiplier, chipper::parameters::id::fmOperator4Multiplier };
    for (size_t i = 0; i < operatorCount; ++i)
    {
        controls.levelSliders[i].setSliderStyle(juce::Slider::LinearHorizontal);
        controls.levelSliders[i].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        levelAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, levelIds[i], controls.levelSliders[i]);
        controls.multipliers[i].bind(state, multiplierIds[i], chipper::parameters::fmOperatorMultiplierChoices());
    }
    attachControls();
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
    for (auto& multiplier : controls.multipliers)
        multiplier.setTheme(card, accent, outline, text, text);
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

        auto content = operatorCards[i].reduced(8, 2);
        auto header = content.removeFromTop(std::min(22, content.getHeight()));
        controls.names[i].setBounds(header.removeFromLeft(std::min(82, header.getWidth())));
        controls.levelReadouts[i].setBounds(header.removeFromRight(std::min(80, header.getWidth())));
        controls.levelSliders[i].setBounds(header.reduced(4, 2));
        content.removeFromTop(std::min(1, content.getHeight()));
        auto registerRow = content.removeFromBottom(std::min(mode == chipper::ChipMode::opl3 ? 14 : 16, content.getHeight()));
        if (mode == chipper::ChipMode::opl3)
        {
            auto detuneRow = content.removeFromBottom(std::min(26, content.getHeight()));
            controls.detunes[i].setBounds(detuneRow);
            content.removeFromBottom(std::min(1, content.getHeight()));
        }
        else
        {
            controls.detunes[i].setBounds({});
        }
        controls.registerReadouts[i].setBounds(registerRow.reduced(2, 0));
        content.removeFromBottom(std::min(1, content.getHeight()));

        auto controlsRow = content.removeFromTop(std::min(30, content.getHeight()));
        if (mode == chipper::ChipMode::ym2151)
        {
            controls.detunes[i].setBounds(controlsRow.removeFromRight(std::min(92, controlsRow.getWidth())));
            controlsRow.removeFromRight(std::min(6, controlsRow.getWidth()));
            controls.envelopes[i].setBounds(controlsRow.removeFromRight(std::min(76, controlsRow.getWidth())));
            controls.envelopes[i].setButtonText("Envelope");
            controlsRow.removeFromRight(std::min(6, controlsRow.getWidth()));
            controls.multipliers[i].setBounds(controlsRow);
            continue;
        }
        const auto envelopeWidth = std::max(0, (controlsRow.getWidth() - 6) / 2);
        controls.envelopes[i].setBounds(controlsRow.removeFromRight(envelopeWidth));
        controlsRow.removeFromRight(std::min(6, controlsRow.getWidth()));
        controls.multipliers[i].setBounds(controlsRow);
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
        const auto envelopeWidth = std::min(92, controlsRow.getWidth());
        controls.envelopes[i].setBounds(controlsRow.removeFromRight(envelopeWidth));
        controlsRow.removeFromRight(std::min(6, controlsRow.getWidth()));
        const auto multiplierWidth = std::min(114, controlsRow.getWidth());
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
