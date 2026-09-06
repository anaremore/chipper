#include "PluginProcessor.h"
#include "Parameters.h"
#include "UI/ChipperChoiceControl.h"
#include "UI/ChipperSsgChannel.h"
#include "UI/ChipperFmEditor.h"

#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (! condition) std::cerr << "ui_choice_smoke: " << message << '\n';
    return condition;
}
struct ParameterEvents final : juce::AudioProcessorParameter::Listener
{
    void parameterValueChanged(int, float) override { ++values; }
    void parameterGestureChanged(int, bool starting) override { starting ? ++begins : ++ends; }
    int values = 0, begins = 0, ends = 0;
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    ChipperAudioProcessor processor;
    auto& state = processor.getValueTreeState();
    auto* parameter = state.getParameter(chipper::parameters::id::pulse2Duty);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f));
    bool ok = true;
    ParameterEvents events;
    parameter->addListener(&events);
    {
        ChipperChoiceControl control;
        control.bind(state, chipper::parameters::id::pulse2Duty, { "Preset", "12.5%", "25%", "50%", "75%" });
        control.setSegmented(true);
        control.setSize(198, 54);
        control.setResolvedValue("25%");
        ok &= expect(control.displayText().contains("25%") && control.selectedChoice() == 0,
                     "inherited duty should show its effective value");
        ok &= expect(events.values == 0 && events.begins == 0 && events.ends == 0,
                     "showing an inherited value must not write or gesture the parameter");
        ok &= expect(control.showingSegments() && control.choiceBounds(1).getWidth() >= 44,
                     "four duties should remain readable in a compact source card");
        ok &= expect(! control.choiceBounds(0).intersects(control.choiceBounds(1)),
                     "inheritance and native choices need separate rows");
        control.selectChoice(4);
        ok &= expect(parameter->convertFrom0to1(parameter->getValue()) == 4.0f
                         && events.begins == 1 && events.ends == 1,
                     "a native selection should send exactly one complete host gesture");
        control.setResolvedValue("50%");
        ok &= expect(control.displayText() == "75%", "resolved updates must not override an explicit choice");
        parameter->setValueNotifyingHost(parameter->convertTo0to1(2.0f));
        ok &= expect(control.selectedChoice() == 2 && control.displayText() == "25%",
                     "host automation should update the displayed native selection");
        const auto saved = state.copyState();
        control.selectChoice(0);
        state.replaceState(saved);
        ok &= expect(control.selectedChoice() == 2, "state restore should update an existing choice component");
        control.setSize(130, 28);
        ok &= expect(! control.showingSegments() && control.menu().getBounds() == control.getLocalBounds(),
                     "short cards should fall back to a full-size menu");
        control.selectChoice(0);
        control.setResolvedValue("12.5%");
        ok &= expect(control.menu().getText().contains("12.5%") && control.menu().getSelectedId() == 1,
                     "an already selected inherited menu item must refresh its visible text and preserve its ID");
        state.copyState();
        processor.getUndoManager().clearUndoHistory();
        control.selectChoice(4);
        state.copyState();
        control.selectChoice(3);
        state.copyState();
        processor.getUndoManager().undo();
        ok &= expect(control.selectedChoice() == 4, "each choice must be a separate undoable edit");
        processor.getUndoManager().redo();
        ok &= expect(control.selectedChoice() == 3, "redo must restore the selected native value");
        control.bind(state, chipper::parameters::id::ymChannelAMix, { "Follow", "Tone", "Noise", "Both", "Off" });
        parameter->setValueNotifyingHost(parameter->convertTo0to1(1.0f));
        ok &= expect(control.selectedChoice() == 0, "rebinding must detach the previous parameter");
    }
    parameter->removeListener(&events);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(0.0f)); // destroyed attachment must be detached

    chipper::PatchConfig patch;
    patch.control4 = 0.5f;
    for (const auto mode : { chipper::ChipMode::ym2149, chipper::ChipMode::ym2203,
                            chipper::ChipMode::ym2608, chipper::ChipMode::ym2610, chipper::ChipMode::ym2610b })
    {
        ChipperSsgChannel channel;
        channel.bind(state, 0);
        channel.updatePatch(mode, patch);
        ok &= expect(channel.routing().displayText().contains(ChipperSsgChannel::effectiveRoute(mode, patch, 0)),
                     "every AY/OPN variant should display resolved inherited routing");
        patch.ymChannelAMix = 2;
        patch.ymChannelBMix = 1;
        patch.ymChannelCMix = 4;
        ok &= expect(ChipperSsgChannel::effectiveRoute(mode, patch, 0) == "Noise"
                         && ChipperSsgChannel::effectiveRoute(mode, patch, 1) == "Tone"
                         && ChipperSsgChannel::effectiveRoute(mode, patch, 2) == "Off",
                     "routing labels should follow the native mixer bits on every SSG chip");
        ok &= expect(ChipperSsgChannel::sharedNoiseDestinations(mode, patch) == "Noise routed to: A",
                     "shared noise should identify only channels with enabled noise routing");
        patch.ymChannelAMix = patch.ymChannelBMix = patch.ymChannelCMix = 0;
    }
    for (const auto mode : { chipper::ChipMode::ym2612, chipper::ChipMode::ym2151,
                            chipper::ChipMode::opl3, chipper::ChipMode::ym2413, chipper::ChipMode::nesVrc7 })
    {
        const auto menu = ChipperFmEditor::createEnvelopeMenu(mode, { 0, 1, 2, 3 });
        ok &= expect(menu.getNumItems() == 4, "envelopes should expose four navigable fields");
        const bool fourBit = mode == chipper::ChipMode::opl3 || mode == chipper::ChipMode::ym2413 || mode == chipper::ChipMode::nesVrc7;
        int field = 0;
        for (juce::PopupMenu::MenuItemIterator it(menu); it.next(); ++field)
        {
            const auto& item = it.getItem();
            ok &= expect(item.subMenu != nullptr, "each envelope field should have its own native choice submenu");
            if (! item.subMenu) continue;
            const int expectedCount = fourBit || field == 3 ? 17 : 33;
            ok &= expect(item.subMenu->getNumItems() == expectedCount, "each chip should offer only its native envelope range");
            if (field == 2)
                ok &= expect(item.text.startsWith(fourBit ? "Sustain level" : "Sustain rate"),
                             "OPL/OPLL sustain level must not be presented as an OPN sustain rate");
            int choice = 0;
            for (juce::PopupMenu::MenuItemIterator values(*item.subMenu); values.next(); ++choice)
                ok &= expect(values.getItem().itemID == field * 100 + choice + 1,
                             "submenu choices must retain existing parameter dispatch IDs");
        }
    }
    const auto clampedMenu = ChipperFmEditor::createEnvelopeMenu(chipper::ChipMode::ym2413, { 32, 32, 32, 16 });
    for (juce::PopupMenu::MenuItemIterator fields(clampedMenu); fields.next();)
    {
        ok &= expect(fields.getItem().text.endsWith("15"), "switching from OPN should display the effective four-bit envelope value");
    }
    return ok ? 0 : 1;
}
