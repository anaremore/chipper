#include "ChipperWorkflowBar.h"

#include <algorithm>
#include <numeric>

ChipperWorkflowBar::ChipperWorkflowBar()
{
    static constexpr std::array<const char*, 8> labels {
        "Undo", "Redo", "A", "B", "Copy", "Paste", "Init", "Vary"
    };
    static constexpr std::array<const char*, 8> names {
        "Undo last sound edit",
        "Redo last sound edit",
        "Audition sound slot A",
        "Audition sound slot B",
        "Copy current chip sound",
        "Paste copied chip sound",
        "Initialize a sound section",
        "Create a safe variation"
    };
    static constexpr std::array<const char*, 8> help {
        "Undo the last parameter edit or workflow action.",
        "Redo the last undone parameter edit or workflow action.",
        "Store the current B sound and audition A.",
        "Store the current A sound and audition B.",
        "Copy all automatable parameters for this chip.",
        "Paste a copied sound only when it belongs to the current chip.",
        "Reset Sources, Musical controls, Output, or the whole chip.",
        "Gently vary musical macros and active source levels without changing chip-native configuration."
    };

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        button.setButtonText(labels[i]);
        button.setComponentID("workflow." + juce::String(labels[i]).toLowerCase());
        button.setName(names[i]);
        button.setTooltip(help[i]);
        button.setWantsKeyboardFocus(true);
        button.setExplicitFocusOrder(static_cast<int>(30 + i));
        addAndMakeVisible(button);
    }

    buttons[2].setClickingTogglesState(true);
    buttons[3].setClickingTogglesState(true);
    buttons[2].setRadioGroupId(0x57464142);
    buttons[3].setRadioGroupId(0x57464142);

    buttons[0].onClick = [this] { if (onUndo) onUndo(); };
    buttons[1].onClick = [this] { if (onRedo) onRedo(); };
    buttons[2].onClick = [this] { if (onSlotA) onSlotA(); };
    buttons[3].onClick = [this] { if (onSlotB) onSlotB(); };
    buttons[4].onClick = [this] { if (onCopy) onCopy(); };
    buttons[5].onClick = [this] { if (onPaste) onPaste(); };
    buttons[6].onClick = [this] { if (onInit) onInit(); };
    buttons[7].onClick = [this] { if (onVary) onVary(); };

    setState(false, false, false, 0);
}

void ChipperWorkflowBar::resized()
{
    auto area = getLocalBounds();
    constexpr auto gap = 3;
    constexpr std::array<int, 8> idealWidths { 46, 46, 28, 28, 44, 48, 40, 44 };
    const auto availableForButtons = std::max(0, area.getWidth() - gap * static_cast<int>(buttons.size() - 1u));
    const auto idealTotal = std::accumulate(idealWidths.begin(), idealWidths.end(), 0);

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const auto isLast = i + 1u == buttons.size();
        const auto scaledWidth = idealTotal > 0
            ? std::max(24, availableForButtons * idealWidths[i] / idealTotal)
            : 24;
        buttons[i].setBounds(isLast ? area : area.removeFromLeft(std::min(scaledWidth, area.getWidth())));
        if (! isLast)
            area.removeFromLeft(std::min(gap, area.getWidth()));
    }
}

void ChipperWorkflowBar::setTheme(juce::Colour primary,
                                  juce::Colour accent,
                                  juce::Colour outline,
                                  juce::Colour text,
                                  juce::Colour mutedText,
                                  juce::Colour darkText)
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        auto& button = buttons[i];
        button.setColour(juce::TextButton::buttonColourId,
                         outline.darker(0.45f).interpolatedWith(accent, i >= 6u ? 0.14f : 0.06f));
        button.setColour(juce::TextButton::buttonOnColourId, primary);
        button.setColour(juce::TextButton::textColourOffId, button.isEnabled() ? text : mutedText);
        button.setColour(juce::TextButton::textColourOnId, darkText);
    }
}

void ChipperWorkflowBar::setState(bool canUndo, bool canRedo, bool canPaste, int activeSlot)
{
    buttons[0].setEnabled(canUndo);
    buttons[1].setEnabled(canRedo);
    buttons[5].setEnabled(canPaste);
    buttons[2].setToggleState(activeSlot == 0, juce::dontSendNotification);
    buttons[3].setToggleState(activeSlot == 1, juce::dontSendNotification);
}

juce::Rectangle<int> ChipperWorkflowBar::buttonBoundsForTest(size_t index) const
{
    return index < buttons.size() ? buttons[index].getBounds() : juce::Rectangle<int> {};
}
