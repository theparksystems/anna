#include "TopNavBarComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

TopNavBarComponent::TopNavBarComponent()
{
    titleLabel.setText ("ANNA", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions { 15.0f, juce::Font::bold });
    titleLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::accent());
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    hintLabel.setText ("SAMPLE WORKSTATION  |  Space play  |  1-5 tabs  |  Ctrl+E export  |  Drop audio",
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
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff20252f), bounds.getX(), bounds.getY(),
                                             SamprLookAndFeel::background().darker (0.2f), bounds.getX(), bounds.getBottom(),
                                             false));
    g.fillRect (bounds);
    g.setColour (SamprLookAndFeel::accent().withAlpha (0.30f));
    g.fillRect (0, getHeight() - 2, getWidth(), 1);
    SamprLookAndFeel::paintHorizontalDivider (g, getHeight() - 1, getWidth());
}

void TopNavBarComponent::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    fullscreenButton.setBounds (area.removeFromRight (116));
    area.removeFromRight (8);
    titleLabel.setBounds (area.removeFromLeft (76));
    hintLabel.setBounds (area);
}

} // namespace sampr
