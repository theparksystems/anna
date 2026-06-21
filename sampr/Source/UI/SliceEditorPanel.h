#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/SampleManager.h"

namespace sampr
{

class SliceEditorPanel final : public juce::Component
{
public:
    using TriggerCallback = std::function<void()>;
    using ParameterChangedCallback = std::function<void()>;

    explicit SliceEditorPanel (SampleManager& manager);

    void setTriggerCallback (TriggerCallback callback);
    void setParameterChangedCallback (ParameterChangedCallback callback);
    void syncFromSelectedSlice();

    void resized() override;

private:
    void configureSlider (juce::Slider& slider, double min, double max, double value, const juce::String& suffix);
    void onParameterChanged();
    void updateBakeStatus();

    SampleManager& sampleManager;

    juce::Label titleLabel;
    juce::Label bakeStatusLabel;
    juce::Slider pitchSlider;
    juce::Slider timeSlider;
    juce::Label pitchLabel;
    juce::Label timeLabel;
    juce::ToggleButton loopButton { "Loop slice" };
    juce::ToggleButton preRenderButton { "Pre-render (Rubber Band)" };
    juce::TextButton bakeButton { "Bake Stretch" };
    juce::TextButton triggerButton { "Trigger Slice" };

    TriggerCallback onTrigger;
    ParameterChangedCallback onParameterChanged;
    bool syncing = false;
};

} // namespace sampr