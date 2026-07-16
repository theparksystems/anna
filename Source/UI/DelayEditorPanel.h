#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"

namespace sampr
{

class DelayEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit DelayEditorPanel (PatternStore& store);

    void setChangeCallback (ChangeCallback callback);
    void setChannel (int rowIndex);
    void clearChannel();
    void refreshFromStore();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void syncFromStore();
    void notifyChanged();
    void handleChanged();

    PatternStore& patternStore;
    ChangeCallback onChange;
    int channelIndex = -1;
    bool syncing = false;

    juce::Label headerLabel;
    juce::ToggleButton enabledButton { "On" };
    juce::Slider timeSlider;
    juce::Slider feedbackSlider;
    juce::Slider mixSlider;
    juce::Label timeLabel { {}, "Time" };
    juce::Label feedbackLabel { {}, "Fb" };
    juce::Label mixLabel { {}, "Mix" };
};

} // namespace sampr