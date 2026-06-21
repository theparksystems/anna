#include "TopNavBarComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

TopNavBarComponent::TopNavBarComponent()
{
    titleLabel.setText ("SAMPR", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions { 13.0f, juce::Font::bold });
    titleLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textPrimary());
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    hintLabel.setText ("Space play  |  1-4 tabs  |  Ctrl+S/O save/load  |  Drop audio to import",
                       juce::dontSendNotification);
    hintLabel.setFont (juce::FontOptions { 11.0f });
    hintLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::textMuted());
    hintLabel.setJustificationType (juce::Justification::centredLeft);

    fullscreenButton.setTooltip ("Toggle fullscreen");
    fullscreenButton.onClick = [this]
    {
        if (onFullscreenToggle != nullptr)
            onFullscreenToggle();
    };

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (hintLabel);
    addAndMakeVisible (fullscreenButton);
}

void TopNavBarComponent::setFullscreenToggleCallback (FullscreenToggleCallback callback)
{
    onFullscreenToggle = std::move (callback);
}

void TopNavBarComponent::setFullscreenState (bool isFullscreen)
{
    fullscreenButton.setButtonText (isFullscreen ? "Exit Fullscreen" : "Fullscreen");
}

void TopNavBarComponent::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::background());
    SamprLookAndFeel::paintHorizontalDivider (g, getHeight() - 1, getWidth());
}

void TopNavBarComponent::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    fullscreenButton.setBounds (area.removeFromRight (108));
    area.removeFromRight (8);
    titleLabel.setBounds (area.removeFromLeft (56));
    hintLabel.setBounds (area);
}

} // namespace sampr