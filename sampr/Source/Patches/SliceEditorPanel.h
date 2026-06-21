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
    using AutoSliceCallback = std::function<void()>;

    explicit SliceEditorPanel (SampleManager& manager);

    void setTriggerCallback (TriggerCallback callback);
    void setParameterChangedCallback (ParameterChangedCallback callback);
    void setAutoSliceCallback (AutoSliceCallback callback);
    void syncFromSelectedSlice();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void configureSlider (juce::Slider& slider, double min, double max, double value, const juce::String& suffix);
    void handleParameterChanged();
    void handleAutoSlice();
    void updateBakeStatus();
    StretchProcessingMode modeFromCombo() const;
    void setComboFromMode (StretchProcessingMode mode);

    SampleManager& sampleManager;

    juce::Label titleLabel;
    juce::Label bakeStatusLabel;
    juce::Slider pitchSlider;
    juce::Slider timeSlider;
    juce::Slider sensitivitySlider;
    juce::Label pitchLabel;
    juce::Label timeLabel;
    juce::Label sensitivityLabel;
    juce::ToggleButton loopButton { "Loop slice" };
    juce::ComboBox processingModeBox;
    juce::TextButton autoSliceButton { "Auto Slice" };
    juce::TextButton bakeButton { "Bake Stretch" };
    juce::TextButton triggerButton { "Trigger Slice" };

    TriggerCallback onTrigger;
    ParameterChangedCallback parameterChangedCallback;
    AutoSliceCallback autoSliceCallback;
    bool syncing = false;
};

} // namespace sampr