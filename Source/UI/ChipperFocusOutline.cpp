#include "ChipperFocusOutline.h"

ChipperFocusOutline::ChipperFocusOutline(juce::Component& ownerToUse)
    : owner(ownerToUse)
{
    setInterceptsMouseClicks(false, false);
    setComponentID("accessibility.focusOutline");
    juce::Desktop::getInstance().addFocusChangeListener(this);
}

ChipperFocusOutline::~ChipperFocusOutline()
{
    juce::Desktop::getInstance().removeFocusChangeListener(this);
}

void ChipperFocusOutline::setColour(juce::Colour colourToUse)
{
    focusColour = colourToUse;
    repaint();
}

void ChipperFocusOutline::globalFocusChanged(juce::Component* focusedComponent)
{
    focusedBounds = {};
    if (focusedComponent != nullptr && (&owner == focusedComponent || owner.isParentOf(focusedComponent)))
    {
        auto* target = focusedComponent;
        while (target->getParentComponent() != nullptr
               && target->getParentComponent() != &owner
               && target->getComponentID().isEmpty()
               && target->getName().isEmpty())
            target = target->getParentComponent();

        focusedBounds = getLocalArea(target, target->getLocalBounds()).expanded(3).getIntersection(getLocalBounds());
        toFront(false);
    }
    repaint();
}

void ChipperFocusOutline::paint(juce::Graphics& graphics)
{
    if (focusedBounds.isEmpty())
        return;

    graphics.setColour(juce::Colours::black.withAlpha(0.72f));
    graphics.drawRoundedRectangle(focusedBounds.toFloat().reduced(0.5f), 4.0f, 4.0f);
    graphics.setColour(focusColour);
    graphics.drawRoundedRectangle(focusedBounds.toFloat().reduced(1.0f), 4.0f, 2.0f);
}
