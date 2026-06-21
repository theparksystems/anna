#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

namespace sampr
{

class TopNavBarComponent final : public juce::Component
{
public:
    using FullscreenToggleCallback = std::function<void()>;

    TopNavBarComponent();

    void setFullscreenToggleCallback (FullscreenToggleCallback callback);
    void setFullscreenState (bool isFullscreen);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::TextButton fullscreenButton { "Fullscreen" };

    FullscreenToggleCallback onFullscreenToggle;
};

} // namespace sampr