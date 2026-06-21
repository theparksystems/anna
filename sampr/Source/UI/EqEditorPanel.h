#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Model/PatternStore.h"
#include "../Plugins/EqTypes.h"

namespace sampr
{

class EqEditorPanel final : public juce::Component
{
public:
    using ChangeCallback = std::function<void()>;

    explicit EqEditorPanel (PatternStore& store);

    void setChangeCallback (ChangeCallback callback);
    void setChannel (int rowIndex);
    void clearChannel();
    void refreshFromStore();
    int getChannelIndex() const noexcept { return channelIndex; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct BandControls
    {
        juce::Label title;
        juce::Slider freqSlider;
        juce::Slider gainSlider;
        juce::Slider qSlider;
        juce::Label freqLabel;
        juce::Label gainLabel;
        juce::Label qLabel;
    };

    void setupBand (BandControls& band,
                    const juce::String& title,
                    double minFreq,
                    double maxFreq,
                    EqBandKind kind);
    void configureBandSlider (juce::Slider& slider,
                              double min,
                              double max,
                              const juce::String& suffix);
    void layoutBand (BandControls& band, juce::Rectangle<int> bounds);
    void syncFromStore();
    void notifyChanged();
    void handleBandChanged (EqBandKind band);

    PatternStore& patternStore;
    ChangeCallback onChange;
    int channelIndex = -1;
    bool syncing = false;

    juce::Label headerLabel;
    juce::ToggleButton enabledButton { "EQ On" };
    BandControls lowBand;
    BandControls midBand;
    BandControls highBand;
};

} // namespace sampr