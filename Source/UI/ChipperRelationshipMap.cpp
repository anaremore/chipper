#include "ChipperRelationshipMap.h"

#include "UI/ChipUiModel.h"

#include <algorithm>

ChipperRelationshipMap::ChipperRelationshipMap()
{
    setInterceptsMouseClicks(false, false);
    setComponentID("play.relationshipMap");
    setName("Chip relationships");
}

ChipperRelationshipMap::Model ChipperRelationshipMap::modelFor(chipper::ChipMode mode)
{
    if (mode == chipper::ChipMode::sid)
        return { "Sync/Ring oscillator cycle", "Chipper stereo and motion helpers", { "V1", "V2", "V3" }, { { 0, 1 }, { 1, 2 }, { 2, 0 } } };
    if (mode == chipper::ChipMode::dmg)
        return { "NR51 routes voices to L/R", "Chipper amp helper", { "Voices", "NR51", "L/R" }, { { 0, 1 }, { 1, 2 } } };
    if (mode == chipper::ChipMode::huc6280)
        return { "Channel 2 modulates Channel 1", "Shared Chipper amp envelope", { "Ch 2", "LFO", "Ch 1" }, { { 0, 1 }, { 1, 2 } } };
    if (mode == chipper::ChipMode::namcoWsg || mode == chipper::ChipMode::scc)
        return { "Per-lane Wave RAM into native mix", "Shared Chipper amp envelope", { "Wave RAM", "Lanes", "Mix" }, { { 0, 1 }, { 1, 2 } } };
    if (mode == chipper::ChipMode::spc700 || mode == chipper::ChipMode::paula)
        return { "Sample bank feeds native voices", "Chipper note-map and loop workflow", { "Bank", "Voices", "Output" }, { { 0, 1 }, { 1, 2 } } };
    if (mode == chipper::ChipMode::ym2413 || mode == chipper::ChipMode::nesVrc7)
        return { "Shared two-operator patch", "Chipper macro bias", { "Mod", "Carrier", "Output" }, { { 0, 1 }, { 1, 2 } } };

    const auto profile = chipper::ui::profileFor(mode);
    if (profile.family == chipper::ui::ChipUiFamily::fm)
        return { "Operators route through native algorithm", "Chipper macro bias", { "Operators", "Algorithm", "Output" }, { { 0, 1 }, { 1, 2 } } };
    if (profile.family == chipper::ui::ChipUiFamily::psg)
        return { "Shared noise/envelope feeds channels", "Chipper stereo spread", { "Noise/Env", "Channels", "Mix" }, { { 0, 1 }, { 1, 2 } } };
    if (profile.family == chipper::ui::ChipUiFamily::oneBit)
        return { "Timer gates the one-bit output", "Chipper amp helper", { "Timer", "Gate", "Output" }, { { 0, 1 }, { 1, 2 } } };
    if (profile.nesFamily)
        return { "APU and expansion voices share the native mix", "Chipper amp/gate helpers", { "Voices", "Mix", "Output" }, { { 0, 1 }, { 1, 2 } } };

    return { "Native voices feed the chip mixer", "Chipper musical helpers", { "Voices", "Mix", "Output" }, { { 0, 1 }, { 1, 2 } } };
}

void ChipperRelationshipMap::setMode(chipper::ChipMode modeToUse)
{
    model = modelFor(modeToUse);
    nativeSummary = model.nativeSummary;
    helperSummary = model.helperSummary;
    setName("Native: " + nativeSummary + ". Chipper: " + helperSummary + ".");
    repaint();
}

void ChipperRelationshipMap::setTheme(juce::Colour native,
                                      juce::Colour helper,
                                      juce::Colour outline,
                                      juce::Colour text,
                                      juce::Colour mutedText,
                                      juce::Colour background)
{
    nativeColour = native;
    helperColour = helper;
    outlineColour = outline;
    textColour = text;
    mutedTextColour = mutedText;
    backgroundColour = background;
    repaint();
}

void ChipperRelationshipMap::drawBadge(juce::Graphics& graphics,
                                       juce::Rectangle<int> bounds,
                                       const juce::String& text,
                                       juce::Colour colour) const
{
    graphics.setColour(colour.withAlpha(0.16f));
    graphics.fillRoundedRectangle(bounds.toFloat(), 3.0f);
    graphics.setColour(colour);
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 3.0f, 1.0f);
    graphics.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    graphics.drawText(text, bounds, juce::Justification::centred, true);
}

void ChipperRelationshipMap::paint(juce::Graphics& graphics)
{
    auto area = getLocalBounds();
    if (area.isEmpty() || model.nodes.isEmpty())
        return;

    auto nativeBadge = area.removeFromLeft(std::min(54, area.getWidth()));
    drawBadge(graphics, nativeBadge.reduced(0, 3), "NATIVE", nativeColour);
    area.removeFromLeft(std::min(7, area.getWidth()));

    const auto diagramWidth = std::min(250, std::max(0, area.getWidth() / 3));
    auto diagram = area.removeFromLeft(diagramWidth);
    const auto nodeCount = model.nodes.size();
    std::vector<juce::Rectangle<int>> nodes;
    nodes.reserve(static_cast<size_t>(nodeCount));
    const auto gap = 14;
    const auto nodeWidth = nodeCount > 0 ? std::max(34, (diagram.getWidth() - gap * (nodeCount - 1)) / nodeCount) : 0;
    for (int i = 0; i < nodeCount; ++i)
    {
        nodes.push_back({ diagram.getX() + i * (nodeWidth + gap), diagram.getY() + 3, nodeWidth, std::max(0, diagram.getHeight() - 6) });
    }
    graphics.setColour(nativeColour.withAlpha(0.8f));
    for (const auto [from, to] : model.edges)
    {
        if (from < 0 || to < 0 || from >= static_cast<int>(nodes.size()) || to >= static_cast<int>(nodes.size()))
            continue;
        const auto start = nodes[static_cast<size_t>(from)].getCentre().toFloat();
        const auto end = nodes[static_cast<size_t>(to)].getCentre().toFloat();
        graphics.drawArrow({ start, end }, 1.0f, 5.0f, 4.0f);
    }
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        graphics.setColour(backgroundColour);
        graphics.fillRoundedRectangle(nodes[i].toFloat(), 3.0f);
        graphics.setColour(i + 1u == nodes.size() ? nativeColour : outlineColour);
        graphics.drawRoundedRectangle(nodes[i].toFloat().reduced(0.5f), 3.0f, 1.0f);
        graphics.setColour(textColour);
        graphics.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        graphics.drawText(model.nodes[static_cast<int>(i)], nodes[i].reduced(2, 0), juce::Justification::centred, true);
    }

    area.removeFromLeft(std::min(8, area.getWidth()));
    const auto helperWidth = std::min(62, area.getWidth());
    auto helperBadge = area.removeFromLeft(helperWidth);
    drawBadge(graphics, helperBadge.reduced(0, 3), "CHIPPER", helperColour);
    area.removeFromLeft(std::min(7, area.getWidth()));
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::FontOptions(9.5f));
    graphics.drawText(helperSummary, area, juce::Justification::centredLeft, true);
}
