#include "TopNavBarComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

TopNavBarComponent::TopNavBarComponent()
{
    titleLabel.setText ("ANNA", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions { 18.0f, juce::Font::bold });
    titleLabel.setColour (juce::Label::textColourId, SamprLookAndFeel::accent());
    titleLabel.setJustificationType (juce::Justification::centredLeft);

    hintLabel.setText ("PRODUCER WORKSTATION  |  Sample  |  Slice  |  Sequence  |  Arrange  |  Export",
                       juce::dontSendNotification);
    hintLabel.setFont (juce::FontOptions { 11.5f, juce::Font::bold });
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
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff242b37), bounds.getX(), bounds.getY(),
                                             juce::Colour (0xff0d1118), bounds.getX(), bounds.getBottom(),
                                             false));
    g.fillRect (bounds);
    g.setGradientFill (juce::ColourGradient (SamprLookAndFeel::accent().withAlpha (0.52f), 0.0f, static_cast<float> (getHeight() - 2),
                                             SamprLookAndFeel::accentCool().withAlpha (0.42f), static_cast<float> (getWidth()), static_cast<float> (getHeight() - 2),
                                             false));
    g.fillRect (0, getHeight() - 2, getWidth(), 1);
    SamprLookAndFeel::paintHorizontalDivider (g, getHeight() - 1, getWidth());
}

void TopNavBarComponent::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    fullscreenButton.setBounds (area.removeFromRight (122));
    area.removeFromRight (8);
    titleLabel.setBounds (area.removeFromLeft (92));
    hintLabel.setBounds (area);
}

} // namespace sampr
