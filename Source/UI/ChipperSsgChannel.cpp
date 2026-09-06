#include "ChipperSsgChannel.h"
#include "Engine/ChipDescriptors.h"
#include "Parameters.h"

#include <algorithm>

namespace
{
uint8_t mixerFor(chipper::ChipMode mode, const chipper::PatchConfig& patch)
{
    return mode == chipper::ChipMode::ym2149
        ? chipper::ym2149MixerRegisterWithChannelOverrides(patch, chipper::ym2149MixerRegisterForControl(patch.control4))
        : chipper::opnSsgMixerRegisterForPatch(patch);
}
}

void ChipperSsgChannel::bind(juce::AudioProcessorValueTreeState& state, size_t channel)
{
    channelIndex = std::min(channel, size_t { 2 });
    static const std::array<const char*, 3> ids {
        chipper::parameters::id::ymChannelAMix, chipper::parameters::id::ymChannelBMix,
        chipper::parameters::id::ymChannelCMix };
    choice.bind(state, ids[channelIndex], chipper::parameters::ymChannelMixChoices());
    choice.onUserChange = [this] { if (onUserChange) onUserChange(); };
    addAndMakeVisible(choice);
}

juce::String ChipperSsgChannel::effectiveRoute(chipper::ChipMode mode, const chipper::PatchConfig& patch, size_t channel)
{
    if (channel >= 3)
        return {};
    const auto mixer = mixerFor(mode, patch);
    const bool tone = (mixer & (1u << channel)) == 0;
    const bool noise = (mixer & (1u << (channel + 3))) == 0;
    return tone ? (noise ? "Both" : "Tone") : (noise ? "Noise" : "Off");
}

void ChipperSsgChannel::updatePatch(chipper::ChipMode mode, const chipper::PatchConfig& patch)
{
    choice.setResolvedValue(effectiveRoute(mode, patch, channelIndex));
}

juce::String ChipperSsgChannel::sharedNoiseDestinations(chipper::ChipMode mode, const chipper::PatchConfig& patch)
{
    juce::StringArray destinations;
    const auto mixer = mixerFor(mode, patch);
    for (size_t channel = 0; channel < 3; ++channel)
        if ((mixer & (1u << (channel + 3))) == 0)
            destinations.add(juce::String::charToString(static_cast<juce_wchar>('A' + channel)));
    return destinations.isEmpty() ? "Noise routed to: none" : "Noise routed to: " + destinations.joinIntoString(", ");
}
