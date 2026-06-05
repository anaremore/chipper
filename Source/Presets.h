#pragma once

#include <JuceHeader.h>

namespace chipper
{

struct PresetInfo
{
    juce::String category;
    juce::String name;
    juce::String note;
};

const std::vector<PresetInfo>& presetCatalog();

} // namespace chipper
