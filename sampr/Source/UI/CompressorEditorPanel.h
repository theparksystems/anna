#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"
#include "../Plugins/FxTypes.h"

namespace sampr
{

class CompressorEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit CompressorEditorPanel (PatternStore& store);

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
    void configureSlider (juce::Slider& slider, double min, double max, const juce::String& suffix);

    PatternStore& patternStore;
    ChangeCallback onChange;
    int channelIndex = -1;
    bool syncing = false;

    juce::Label headerLabel;
    juce::ToggleButton enabledButton { "On" };
    juce::Slider thresholdSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider makeupSlider;
    juce::Label thresholdLabel { {}, "Thr" };
    juce::Label ratioLabel { {}, "Ratio" };
    juce::Label attackLabel { {}, "Atk" };
    juce::Label releaseLabel { {}, "Rel" };
    juce::Label makeupLabel { {}, "Gain" };
};

} // namespace sampr