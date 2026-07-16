#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"

namespace sampr
{

class ReverbEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit ReverbEditorPanel (PatternStore& store);

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
    juce::Slider roomSlider;
    juce::Slider dampingSlider;
    juce::Slider wetSlider;
    juce::Slider drySlider;
    juce::Slider widthSlider;
    juce::Label roomLabel { {}, "Room" };
    juce::Label dampingLabel { {}, "Damp" };
    juce::Label wetLabel { {}, "Wet" };
    juce::Label dryLabel { {}, "Dry" };
    juce::Label widthLabel { {}, "Width" };
};

} // namespace sampr