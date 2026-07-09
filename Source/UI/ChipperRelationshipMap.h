#pragma once

#include <JuceHeader.h>

#include "Engine/ChipCore.h"

#include <utility>
#include <vector>

class ChipperRelationshipMap final : public juce::Component
{
public:
    ChipperRelationshipMap();

    void paint(juce::Graphics& graphics) override;
    void setMode(chipper::ChipMode modeToUse);
    void setTheme(juce::Colour native,
                  juce::Colour helper,
                  juce::Colour outline,
                  juce::Colour text,
                  juce::Colour mutedText,
                  juce::Colour background);

    juce::String summaryForTest() const { return nativeSummary + " / " + helperSummary; }

private:
    struct Model
    {
        juce::String nativeSummary;
        juce::String helperSummary;
        juce::StringArray nodes;
        std::vector<std::pair<int, int>> edges;
    };

    static Model modelFor(chipper::ChipMode mode);
    void drawBadge(juce::Graphics& graphics,
                   juce::Rectangle<int> bounds,
                   const juce::String& text,
                   juce::Colour colour) const;

    Model model;
    juce::String nativeSummary;
    juce::String helperSummary;
    juce::Colour nativeColour;
    juce::Colour helperColour;
    juce::Colour outlineColour;
    juce::Colour textColour;
    juce::Colour mutedTextColour;
    juce::Colour backgroundColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperRelationshipMap)
};
