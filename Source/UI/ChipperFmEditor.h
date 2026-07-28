#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"

#include <array>

class ChipperFmEditor final : public juce::Component
{
public:
    static constexpr size_t operatorCount = 4;

    struct Controls
    {
        std::array<juce::Label, operatorCount>& names;
        std::array<juce::Label, operatorCount>& registerReadouts;
        std::array<juce::Label, operatorCount>& levelReadouts;
        std::array<juce::Slider, operatorCount>& levelSliders;
        std::array<juce::TextButton, operatorCount>& multipliers;
        std::array<juce::TextButton, operatorCount>& envelopes;
        std::array<juce::TextButton, operatorCount>& detunes;
    };

    explicit ChipperFmEditor(Controls controlsToUse);

    void attachControls();
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void configure(chipper::ChipMode modeToUse, size_t visibleOperators, bool editableOperators);
    void setTheme(juce::Colour panel,
                  juce::Colour card,
                  juce::Colour outline,
                  juce::Colour primary,
                  juce::Colour accent,
                  juce::Colour text,
                  juce::Colour mutedText);

    juce::Rectangle<int> boundsInParentForTest(const juce::Component& component) const
    {
        return component.getBounds().translated(getX(), getY());
    }
    juce::Rectangle<int> cardBoundsForTest(size_t index) const
    {
        return index < operatorCards.size() ? operatorCards[index].translated(getX(), getY()) : juce::Rectangle<int> {};
    }

private:
    void clearControlBounds(size_t index);
    void layoutFourOperatorGrid();
    void layoutTwoOperatorGrid();
    void layoutCompactRows();

    Controls controls;
    chipper::ChipMode mode = chipper::ChipMode::ym2612;
    size_t operatorRows = 0;
    bool editable = false;
    bool gridLayout = false;
    bool twoOperatorGrid = false;
    juce::Colour panelColour;
    juce::Colour cardColour;
    juce::Colour outlineColour;
    juce::Colour primaryColour;
    juce::Colour accentColour;
    juce::Colour textColour;
    juce::Colour mutedTextColour;
    std::array<juce::Rectangle<int>, operatorCount> operatorCards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperFmEditor)
};
