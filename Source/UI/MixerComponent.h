#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/AudioEngine.h"
#include "../Model/PatternStore.h"

namespace sampr
{

class MixerComponent final : public juce::Component,
                             private juce::Timer
{
public:
    using ChangeCallback = std::function<void()>;
    using FxOpenCallback = std::function<void (int rowIndex)>;
    using AskCallback = std::function<void (int rowIndex)>;

    MixerComponent (PatternStore& patterns, AudioEngine& engine);
    ~MixerComponent() override;

    void setChangeCallback (ChangeCallback callback);
    void setMasterGainChangeCallback (ChangeCallback callback);
    void setFxOpenCallback (FxOpenCallback callback);
    void setAskCallback (AskCallback callback);
    void setMasterGain (float gain);
    float getMasterGain() const;
    void refresh();
    void notifyChange();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class ChannelStrip;

    void timerCallback() override;
    void rebuildStrips();
    void openFxForRow (int rowIndex);
    void openAskForRow (int rowIndex);

    PatternStore& patternStore;
    AudioEngine& audioEngine;
    ChangeCallback onChange;
    ChangeCallback onMasterGainChange;
    FxOpenCallback onFxOpen;
    AskCallback onAsk;

    juce::Viewport stripViewport;
    juce::Component stripContainer;
    std::vector<std::unique_ptr<ChannelStrip>> strips;

    juce::Slider masterGainSlider;
    juce::Label masterLabel;
    juce::Component masterMeter;
    float masterMeterLevel = 0.0f;
};

} // namespace sampr