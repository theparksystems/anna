#include "DelayEditorPanel.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

DelayEditorPanel::DelayEditorPanel (PatternStore& store)
    : patternStore (store)
{
    headerLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    enabledButton.onClick = [this]
    {
        if (syncing || channelIndex < 0)
            return;

        patternStore.setChannelDelayEnabled (channelIndex, enabledButton.getToggleState());
        notifyChanged();
    };

    timeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    timeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    timeSlider.setRange (1.0, 2000.0, 0.0);
    timeSlider.setTextValueSuffix (" ms");

    feedbackSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    feedbackSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    feedbackSlider.setRange (0.0, 0.95, 0.0);

    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    mixSlider.setRange (0.0, 1.0, 0.0);

    for (auto* slider : { &timeSlider, &feedbackSlider, &mixSlider })
        slider->onValueChange = [this] { handleChanged(); };

    addAndMakeVisible (headerLabel);
    addAndMakeVisible (enabledButton);
    addAndMakeVisible (timeLabel);
    addAndMakeVisible (feedbackLabel);
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (timeSlider);
    addAndMakeVisible (feedbackSlider);
    addAndMakeVisible (mixSlider);
}

void DelayEditorPanel::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void DelayEditorPanel::setChannel (int rowIndex)
{
    channelIndex = rowIndex;
    syncFromStore();
}

void DelayEditorPanel::clearChannel()
{
    channelIndex = -1;
}

void DelayEditorPanel::refreshFromStore()
{
    syncFromStore();
}

void DelayEditorPanel::syncFromStore()
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
    const auto& delay = row.channelDelay;

    syncing = true;
    headerLabel.setText ("Delay — " + row.label, juce::dontSendNotification);
    enabledButton.setToggleState (delay.enabled, juce::dontSendNotification);
    timeSlider.setValue (delay.timeMs, juce::dontSendNotification);
    feedbackSlider.setValue (delay.feedback, juce::dontSendNotification);
    mixSlider.setValue (delay.mix, juce::dontSendNotification);
    syncing = false;
}

void DelayEditorPanel::handleChanged()
{
    if (syncing || channelIndex < 0)
        return;

    ChannelDelayState state;
    state.enabled = enabledButton.getToggleState();
    state.timeMs = static_cast<float> (timeSlider.getValue());
    state.feedback = static_cast<float> (feedbackSlider.getValue());
    state.mix = static_cast<float> (mixSlider.getValue());
    patternStore.setChannelDelay (channelIndex, state);
    notifyChanged();
}

void DelayEditorPanel::notifyChanged()
{
    if (onChange != nullptr)
        onChange();
}

void DelayEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
}

void DelayEditorPanel::resized()
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

    layoutRow (timeLabel, timeSlider);
    layoutRow (feedbackLabel, feedbackSlider);
    layoutRow (mixLabel, mixSlider);
}

} // namespace sampr