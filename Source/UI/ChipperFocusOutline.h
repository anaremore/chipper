#pragma once

#include <JuceHeader.h>

class ChipperFocusOutline final : public juce::Component,
                                  private juce::FocusChangeListener
{
public:
    explicit ChipperFocusOutline(juce::Component& ownerToUse);
    ~ChipperFocusOutline() override;

    void paint(juce::Graphics& graphics) override;
    void setColour(juce::Colour colourToUse);

    juce::Rectangle<int> focusedBoundsForTest() const noexcept { return focusedBounds; }
    void setFocusedComponentForTest(juce::Component& component) { globalFocusChanged(&component); }

private:
    void globalFocusChanged(juce::Component* focusedComponent) override;

    juce::Component& owner;
    juce::Rectangle<int> focusedBounds;
    juce::Colour focusColour { 0xff56c7d8 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperFocusOutline)
};
