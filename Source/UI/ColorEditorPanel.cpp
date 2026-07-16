#include "ColorEditorPanel.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

ColorEditorPanel::ColorEditorPanel (PatternStore& store)
    : patternStore (store)
{
    headerLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    enabledButton.setTooltip ("Enable the producer color chain: filter, saturation, width, modulation, pump, limiter.");
    enabledButton.onClick = [this]
    {
        if (syncing || channelIndex < 0)
            return;

        patternStore.setChannelColorEnabled (channelIndex, enabledButton.getToggleState());
        notifyChanged();
    };

    configureSlider (lowCutSlider, 20.0, 1000.0, 1.0, " Hz");
    configureSlider (highCutSlider, 1000.0, 20000.0, 1.0, " Hz");
    configureSlider (driveSlider, 0.0, 1.0, 0.01, "");
    configureSlider (widthSlider, 0.0, 2.0, 0.01, " x");
    configureSlider (chorusSlider, 0.0, 1.0, 0.01, "");
    configureSlider (phaserSlider, 0.0, 1.0, 0.01, "");
    configureSlider (pumpSlider, 0.0, 1.0, 0.01, "");
    configureSlider (ceilingSlider, -12.0, 0.0, 0.1, " dB");

    lowCutSlider.setSkewFactorFromMidPoint (120.0);
    highCutSlider.setSkewFactorFromMidPoint (8000.0);

    for (auto* slider : { &lowCutSlider, &highCutSlider, &driveSlider, &widthSlider,
                          &chorusSlider, &phaserSlider, &pumpSlider, &ceilingSlider })
        slider->onValueChange = [this] { handleChanged(); };

    addAndMakeVisible (headerLabel);
    addAndMakeVisible (enabledButton);
    addAndMakeVisible (lowCutLabel);
    addAndMakeVisible (lowCutSlider);
    addAndMakeVisible (highCutLabel);
    addAndMakeVisible (highCutSlider);
    addAndMakeVisible (driveLabel);
    addAndMakeVisible (driveSlider);
    addAndMakeVisible (widthLabel);
    addAndMakeVisible (widthSlider);
    addAndMakeVisible (chorusLabel);
    addAndMakeVisible (chorusSlider);
    addAndMakeVisible (phaserLabel);
    addAndMakeVisible (phaserSlider);
    addAndMakeVisible (pumpLabel);
    addAndMakeVisible (pumpSlider);
    addAndMakeVisible (ceilingLabel);
    addAndMakeVisible (ceilingSlider);
}

void ColorEditorPanel::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void ColorEditorPanel::configureSlider (juce::Slider& slider,
                                        double min,
                                        double max,
                                        double interval,
                                        const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 18);
    slider.setRange (min, max, interval);
    slider.setTextValueSuffix (suffix);
}

void ColorEditorPanel::setChannel (int rowIndex)
{
    channelIndex = rowIndex;
    syncFromStore();
}

void ColorEditorPanel::clearChannel()
{
    channelIndex = -1;
}

void ColorEditorPanel::refreshFromStore()
{
    syncFromStore();
}

void ColorEditorPanel::syncFromStore()
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
    const auto& color = row.channelColor;

    syncing = true;
    headerLabel.setText ("Color - " + row.label, juce::dontSendNotification);
    enabledButton.setToggleState (color.enabled, juce::dontSendNotification);
    lowCutSlider.setValue (color.lowCutHz, juce::dontSendNotification);
    highCutSlider.setValue (color.highCutHz, juce::dontSendNotification);
    driveSlider.setValue (color.drive, juce::dontSendNotification);
    widthSlider.setValue (color.width, juce::dontSendNotification);
    chorusSlider.setValue (color.chorusMix, juce::dontSendNotification);
    phaserSlider.setValue (color.phaserMix, juce::dontSendNotification);
    pumpSlider.setValue (color.pump, juce::dontSendNotification);
    ceilingSlider.setValue (color.limiterCeilingDb, juce::dontSendNotification);
    syncing = false;
}

void ColorEditorPanel::handleChanged()
{
    if (syncing || channelIndex < 0)
        return;

    ChannelColorState state;
    state.enabled = enabledButton.getToggleState();
    state.lowCutHz = static_cast<float> (lowCutSlider.getValue());
    state.highCutHz = static_cast<float> (highCutSlider.getValue());
    state.drive = static_cast<float> (driveSlider.getValue());
    state.width = static_cast<float> (widthSlider.getValue());
    state.chorusMix = static_cast<float> (chorusSlider.getValue());
    state.phaserMix = static_cast<float> (phaserSlider.getValue());
    state.pump = static_cast<float> (pumpSlider.getValue());
    state.limiterCeilingDb = static_cast<float> (ceilingSlider.getValue());
    patternStore.setChannelColor (channelIndex, state);
    notifyChanged();
}

void ColorEditorPanel::notifyChanged()
{
    if (onChange != nullptr)
        onChange();
}

void ColorEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
}

void ColorEditorPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto header = area.removeFromTop (22);
    headerLabel.setBounds (header.removeFromLeft (juce::jmax (160, header.getWidth() / 2)));
    enabledButton.setBounds (header.removeFromRight (48));
    area.removeFromTop (4);

    auto layoutRow = [&area] (juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (22);
        label.setBounds (row.removeFromLeft (62));
        slider.setBounds (row);
        area.removeFromTop (3);
    };

    layoutRow (lowCutLabel, lowCutSlider);
    layoutRow (highCutLabel, highCutSlider);
    layoutRow (driveLabel, driveSlider);
    layoutRow (widthLabel, widthSlider);
    layoutRow (chorusLabel, chorusSlider);
    layoutRow (phaserLabel, phaserSlider);
    layoutRow (pumpLabel, pumpSlider);
    layoutRow (ceilingLabel, ceilingSlider);
}

} // namespace sampr
