#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"

namespace sampr
{

class ColorEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit ColorEditorPanel (PatternStore& store);

    void setChangeCallback (ChangeCallback callback);
    void setChannel (int rowIndex);
    void clearChannel();
    void refreshFromStore();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void syncFromStore();
    void handleChanged();
    void notifyChanged();
    void configureSlider (juce::Slider& slider, double min, double max, double interval, const juce::String& suffix);

    PatternStore& patternStore;
    ChangeCallback onChange;
    int channelIndex = -1;
    bool syncing = false;

    juce::Label headerLabel;
    juce::ToggleButton enabledButton { "On" };
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;
    juce::Slider driveSlider;
    juce::Slider widthSlider;
    juce::Slider chorusSlider;
    juce::Slider phaserSlider;
    juce::Slider pumpSlider;
    juce::Slider ceilingSlider;

    juce::Label lowCutLabel { {}, "Low Cut" };
    juce::Label highCutLabel { {}, "High Cut" };
    juce::Label driveLabel { {}, "Drive" };
    juce::Label widthLabel { {}, "Width" };
    juce::Label chorusLabel { {}, "Chorus" };
    juce::Label phaserLabel { {}, "Phaser" };
    juce::Label pumpLabel { {}, "Pump" };
    juce::Label ceilingLabel { {}, "Limit" };
};

} // namespace sampr
