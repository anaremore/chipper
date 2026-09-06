#include "ChipperChoiceControl.h"

#include <algorithm>
#include <cmath>

ChipperChoiceControl::ChipperChoiceControl()
{
    addAndMakeVisible(choiceMenu);
    addChildComponent(inheritedValue);
    inheritedValue.setFont(juce::FontOptions(11.0f));
    inheritedValue.setJustificationType(juce::Justification::centredRight);
    choiceMenu.onChange = [this] { selectChoice(choiceMenu.getSelectedItemIndex()); };
}

void ChipperChoiceControl::bind(juce::AudioProcessorValueTreeState& state,
                                const juce::String& id, const juce::StringArray& choices,
                                bool firstChoiceInherits)
{
    if (boundId == id && labels == choices && inherits == firstChoiceInherits && attachment)
        return;
    attachment.reset();
    boundId = id;
    labels = choices;
    inherits = firstChoiceInherits;
    selected = std::clamp(selected, 0, std::max(0, labels.size() - 1));
    choiceMenu.clear(juce::dontSendNotification);
    choiceMenu.addItemList(labels, 1);
    buttons.clear();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton>(labels[i]);
        button->setClickingTogglesState(false);
        button->onClick = [this, i] { selectChoice(i); };
        addChildComponent(*button);
        buttons.push_back(std::move(button));
    }
    setHelp(accessibleName, helpText);
    setTheme(backgroundColour, accentColour, outlineColour, textColour, darkTextColour);
    if (auto* parameter = state.getParameter(id))
    {
        attachment = std::make_unique<juce::ParameterAttachment>(*parameter,
            [this](float value) { refreshSelection(value); }, state.undoManager);
        attachment->sendInitialUpdate();
    }
    resized();
}

void ChipperChoiceControl::selectChoice(int index)
{
    if (index < 0 || index >= labels.size() || ! attachment)
        return;
    attachment->setValueAsCompleteGesture(static_cast<float>(index));
    refreshSelection(static_cast<float>(index));
    if (onUserChange)
        onUserChange();
}

void ChipperChoiceControl::refreshSelection(float value)
{
    selected = std::clamp(static_cast<int>(std::round(value)), 0, std::max(0, labels.size() - 1));
    refreshText();
    choiceMenu.setSelectedItemIndex(selected, juce::dontSendNotification);
    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState(static_cast<int>(i) == selected, juce::dontSendNotification);
}

juce::String ChipperChoiceControl::displayText() const
{
    if (labels.isEmpty())
        return {};
    if (inherits && selected == 0 && resolvedValue.isNotEmpty())
        return labels[0] + " " + juce::String::charToString(0x2192) + " " + resolvedValue;
    return labels[selected];
}

void ChipperChoiceControl::refreshText()
{
    if (labels.isEmpty())
        return;
    const auto inheritedText = labels[0] + (resolvedValue.isNotEmpty()
        ? " " + juce::String::charToString(0x2192) + " " + resolvedValue : juce::String());
    choiceMenu.changeItemText(1, inherits ? inheritedText : labels[0]);
    // JUCE does not refresh an already selected item's label when its item
    // text changes. Keep the visible value synchronized without a user event.
    choiceMenu.setText(displayText(), juce::dontSendNotification);
    inheritedValue.setText(selected == 0 ? resolvedValue : juce::String(), juce::dontSendNotification);
    choiceMenu.setTooltip(helpText + (helpText.isEmpty() ? "" : "\n") + "Current: " + displayText());
    inheritedValue.setTooltip(choiceMenu.getTooltip());
}

void ChipperChoiceControl::setResolvedValue(const juce::String& value)
{
    if (resolvedValue == value)
        return;
    resolvedValue = value;
    refreshText();
}

void ChipperChoiceControl::setHelp(const juce::String& name, const juce::String& help)
{
    accessibleName = name;
    helpText = help;
    setName(name);
    choiceMenu.setName(name);
    choiceMenu.setComponentID(boundId);
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        buttons[i]->setName(name + ": " + labels[static_cast<int>(i)]);
        buttons[i]->setTooltip(help + "\n" + labels[static_cast<int>(i)]);
        buttons[i]->setComponentID(boundId + ".choice" + juce::String(static_cast<int>(i)));
    }
    refreshText();
}

void ChipperChoiceControl::setSegmented(bool enabled)
{
    preferSegments = enabled;
    resized();
}

void ChipperChoiceControl::setTheme(juce::Colour background, juce::Colour accent,
                                    juce::Colour outline, juce::Colour text, juce::Colour darkText)
{
    backgroundColour = background;
    accentColour = accent;
    outlineColour = outline;
    textColour = text;
    darkTextColour = darkText;
    choiceMenu.setColour(juce::ComboBox::backgroundColourId, background);
    choiceMenu.setColour(juce::ComboBox::outlineColourId, outline);
    choiceMenu.setColour(juce::ComboBox::textColourId, text);
    choiceMenu.setColour(juce::ComboBox::arrowColourId, text);
    inheritedValue.setColour(juce::Label::textColourId, text);
    for (auto& button : buttons)
    {
        button->setColour(juce::TextButton::buttonColourId, background);
        button->setColour(juce::TextButton::buttonOnColourId, accent);
        button->setColour(juce::TextButton::textColourOffId, text);
        button->setColour(juce::TextButton::textColourOnId, darkText);
    }
}

void ChipperChoiceControl::resized()
{
    const auto nativeCount = labels.size() - (inherits ? 1 : 0);
    segmentsVisible = preferSegments && nativeCount > 0 && nativeCount <= 4
        && getWidth() >= nativeCount * 44 && getHeight() >= (inherits ? 52 : 26);
    choiceMenu.setVisible(! segmentsVisible);
    choiceMenu.setBounds(segmentsVisible ? juce::Rectangle<int>() : getLocalBounds());
    inheritedValue.setVisible(segmentsVisible && inherits);
    inheritedValue.setBounds({});
    for (auto& button : buttons)
    {
        button->setVisible(segmentsVisible);
        button->setBounds({});
    }
    if (! segmentsVisible)
        return;
    auto area = getLocalBounds();
    if (inherits)
    {
        auto inheritedRow = area.removeFromTop(24);
        buttons[0]->setBounds(inheritedRow.removeFromLeft(66));
        inheritedValue.setBounds(inheritedRow.reduced(6, 0));
        area.removeFromTop(3);
    }
    constexpr auto gap = 3;
    const auto width = (area.getWidth() - gap * (nativeCount - 1)) / nativeCount;
    for (int i = inherits ? 1 : 0; i < labels.size(); ++i)
    {
        buttons[static_cast<size_t>(i)]->setBounds(area.removeFromLeft(i + 1 == labels.size() ? area.getWidth() : width));
        area.removeFromLeft(std::min(gap, area.getWidth()));
    }
}

juce::Rectangle<int> ChipperChoiceControl::choiceBounds(int index) const
{
    return segmentsVisible && index >= 0 && index < static_cast<int>(buttons.size())
        ? buttons[static_cast<size_t>(index)]->getBounds() : choiceMenu.getBounds();
}
