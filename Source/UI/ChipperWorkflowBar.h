#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>

class ChipperWorkflowBar final : public juce::Component
{
public:
    ChipperWorkflowBar();

    void resized() override;
    void setTheme(juce::Colour primary,
                  juce::Colour accent,
                  juce::Colour outline,
                  juce::Colour text,
                  juce::Colour mutedText,
                  juce::Colour darkText);
    void setState(bool canUndo, bool canRedo, bool canPaste, int activeSlot);

    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void()> onSlotA;
    std::function<void()> onSlotB;
    std::function<void()> onCopy;
    std::function<void()> onPaste;
    std::function<void()> onInit;
    std::function<void()> onVary;

    juce::Rectangle<int> buttonBoundsForTest(size_t index) const;

private:
    std::array<juce::TextButton, 8> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipperWorkflowBar)
};
