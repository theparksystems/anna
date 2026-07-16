#include "ReverbEditorPanel.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

ReverbEditorPanel::ReverbEditorPanel (PatternStore& store)
    : patternStore (store)
{
    headerLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    enabledButton.onClick = [this]
    {
        if (syncing || channelIndex < 0)
            return;

        patternStore.setChannelReverbEnabled (channelIndex, enabledButton.getToggleState());
        notifyChanged();
    };

    for (auto* slider : { &roomSlider, &dampingSlider, &wetSlider, &drySlider, &widthSlider })
    {
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        slider->setRange (0.0, 1.0, 0.0);
        slider->onValueChange = [this] { handleChanged(); };
    }

    addAndMakeVisible (headerLabel);
    addAndMakeVisible (enabledButton);
    addAndMakeVisible (roomLabel);
    addAndMakeVisible (dampingLabel);
    addAndMakeVisible (wetLabel);
    addAndMakeVisible (dryLabel);
    addAndMakeVisible (widthLabel);
    addAndMakeVisible (roomSlider);
    addAndMakeVisible (dampingSlider);
    addAndMakeVisible (wetSlider);
    addAndMakeVisible (drySlider);
    addAndMakeVisible (widthSlider);
}

void ReverbEditorPanel::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void ReverbEditorPanel::setChannel (int rowIndex)
{
    channelIndex = rowIndex;
    syncFromStore();
}

void ReverbEditorPanel::clearChannel()
{
    channelIndex = -1;
}

void ReverbEditorPanel::refreshFromStore()
{
    syncFromStore();
}

void ReverbEditorPanel::syncFromStore()
{
    if (channelIndex < 0)
        return;

    const auto& pattern = patternStore.getCurrentPattern();

    if (! juce::isPositiveAndBelow (channelIndex, static_cast<int> (pattern.rows.size())))
    {
        clearChannel();
        return;
    }

    const auto& row = pattern.rows[static_cast<size_t> (channelIndex)];
    const auto& reverb = row.channelReverb;

    syncing = true;
    headerLabel.setText ("Reverb — " + row.label, juce::dontSendNotification);
    enabledButton.setToggleState (reverb.enabled, juce::dontSendNotification);
    roomSlider.setValue (reverb.roomSize, juce::dontSendNotification);
    dampingSlider.setValue (reverb.damping, juce::dontSendNotification);
    wetSlider.setValue (reverb.wetLevel, juce::dontSendNotification);
    drySlider.setValue (reverb.dryLevel, juce::dontSendNotification);
    widthSlider.setValue (reverb.width, juce::dontSendNotification);
    syncing = false;
}

void ReverbEditorPanel::handleChanged()
{
    if (syncing || channelIndex < 0)
        return;

    ChannelReverbState state;
    state.enabled = enabledButton.getToggleState();
    state.roomSize = static_cast<float> (roomSlider.getValue());
    state.damping = static_cast<float> (dampingSlider.getValue());
    state.wetLevel = static_cast<float> (wetSlider.getValue());
    state.dryLevel = static_cast<float> (drySlider.getValue());
    state.width = static_cast<float> (widthSlider.getValue());
    patternStore.setChannelReverb (channelIndex, state);
    notifyChanged();
}

void ReverbEditorPanel::notifyChanged()
{
    if (onChange != nullptr)
        onChange();
}

void ReverbEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
}

void ReverbEditorPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto header = area.removeFromTop (22);
    headerLabel.setBounds (header.removeFromLeft (juce::jmax (140, header.getWidth() / 2)));
    enabledButton.setBounds (header.removeFromRight (48));
    area.removeFromTop (4);

    auto layoutRow = [&area] (juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (22);
        label.setBounds (row.removeFromLeft (44));
        slider.setBounds (row);
        area.removeFromTop (4);
    };

    layoutRow (roomLabel, roomSlider);
    layoutRow (dampingLabel, dampingSlider);
    layoutRow (wetLabel, wetSlider);
    layoutRow (dryLabel, drySlider);
    layoutRow (widthLabel, widthSlider);
}

} // namespace sampr