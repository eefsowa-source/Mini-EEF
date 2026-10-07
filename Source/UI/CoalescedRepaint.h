#pragma once

#include <JuceHeader.h>

// One reusable AsyncUpdaterMessage. trigger() from the message thread sets
// shouldDeliver; the message thread later runs handleAsyncUpdate once, even
// if several controls requested a repaint before the callback.
class CoalescedRepaint : private juce::AsyncUpdater
{
public:
    ~CoalescedRepaint() override { cancelPendingUpdate(); }

    void trigger (juce::Component& component)
    {
        target = &component;
        triggerAsyncUpdate();
    }

private:
    void handleAsyncUpdate() override
    {
        if (target != nullptr)
            target->repaint();
    }

    juce::Component* target = nullptr;
};
