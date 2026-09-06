#pragma once

#include "ChipperChoiceControl.h"
#include "Engine/ChipCore.h"

// The AY-compatible mixer is the same hardware block in AY and OPN chips.
// Source enable/level remain in the surrounding source card; this component
// owns the native routing parameter and explains its effective register state.
class ChipperSsgChannel final : public juce::Component
{
public:
    void bind(juce::AudioProcessorValueTreeState&, size_t channel);
    void updatePatch(chipper::ChipMode mode, const chipper::PatchConfig&);
    void resized() override { choice.setBounds(getLocalBounds()); }
    void setHelp(const juce::String& name, const juce::String& help) { choice.setHelp(name, help); }
    void setTheme(juce::Colour background, juce::Colour accent, juce::Colour outline,
                  juce::Colour text, juce::Colour darkText)
    { choice.setTheme(background, accent, outline, text, darkText); }
    ChipperChoiceControl& routing() noexcept { return choice; }
    const ChipperChoiceControl& routing() const noexcept { return choice; }
    static juce::String effectiveRoute(chipper::ChipMode mode, const chipper::PatchConfig&, size_t channel);
    static juce::String sharedNoiseDestinations(chipper::ChipMode mode, const chipper::PatchConfig&);
    std::function<void()> onUserChange;

private:
    size_t channelIndex = 0;
    ChipperChoiceControl choice;
};
