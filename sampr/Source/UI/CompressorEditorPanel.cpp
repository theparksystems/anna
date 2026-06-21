#include "CompressorEditorPanel.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

CompressorEditorPanel::CompressorEditorPanel (PatternStore& store)
    : patternStore (store)
{
    headerLabel.setFont (juce::FontOptions { 12.0f, juce::Font::bold });
    enabledButton.onClick = [this]
    {
        if (syncing || channelIndex < 0)
            return;

        patternStore.setChannelCompressorEnabled (channelIndex, enabledButton.getToggleState());
        notifyChanged();
    };

    configureSlider (thresholdSlider, -60.0, 0.0, " dB");
    configureSlider (ratioSlider, 1.0, 20.0, ":1");
    configureSlider (attackSlider, 0.1, 200.0, " ms");
    configureSlider (releaseSlider, 1.0, 1000.0, " ms");
    configureSlider (makeupSlider, 0.0, 24.0, " dB");

    for (auto* slider : { &thresholdSlider, &ratioSlider, &attackSlider, &releaseSlider, &makeupSlider })
        slider->onValueChange = [this] { handleChanged(); };

    addAndMakeVisible (headerLabel);
    addAndMakeVisible (enabledButton);
    addAndMakeVisible (thresholdLabel);
    addAndMakeVisible (ratioLabel);
    addAndMakeVisible (attackLabel);
    addAndMakeVisible (releaseLabel);
    addAndMakeVisible (makeupLabel);
    addAndMakeVisible (thresholdSlider);
    addAndMakeVisible (ratioSlider);
    addAndMakeVisible (attackSlider);
    addAndMakeVisible (releaseSlider);
    addAndMakeVisible (makeupSlider);
}

void CompressorEditorPanel::setChangeCallback (ChangeCallback callback)
{
    onChange = std::move (callback);
}

void CompressorEditorPanel::configureSlider (juce::Slider& slider, double min, double max, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    slider.setRange (min, max, 0.0);
    slider.setTextValueSuffix (suffix);
}

void CompressorEditorPanel::setChannel (int rowIndex)
{
    channelIndex = rowIndex;
    syncFromStore();
}

void CompressorEditorPanel::clearChannel()
{
    channelIndex = -1;
}

void CompressorEditorPanel::refreshFromStore()
{
    syncFromStore();
}

void CompressorEditorPanel::syncFromStore()
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
    const auto& comp = row.channelCompressor;

    syncing = true;
    headerLabel.setText ("Compressor — " + row.label, juce::dontSendNotification);
    enabledButton.setToggleState (comp.enabled, juce::dontSendNotification);
    thresholdSlider.setValue (comp.thresholdDb, juce::dontSendNotification);
    ratioSlider.setValue (comp.ratio, juce::dontSendNotification);
    attackSlider.setValue (comp.attackMs, juce::dontSendNotification);
    releaseSlider.setValue (comp.releaseMs, juce::dontSendNotification);
    makeupSlider.setValue (comp.makeupGainDb, juce::dontSendNotification);
    syncing = false;
}

void CompressorEditorPanel::handleChanged()
{
    if (syncing || channelIndex < 0)
        return;

    ChannelCompressorState state;
    state.enabled = enabledButton.getToggleState();
    state.thresholdDb = static_cast<float> (thresholdSlider.getValue());
    state.ratio = static_cast<float> (ratioSlider.getValue());
    state.attackMs = static_cast<float> (attackSlider.getValue());
    state.releaseMs = static_cast<float> (releaseSlider.getValue());
    state.makeupGainDb = static_cast<float> (makeupSlider.getValue());
    patternStore.setChannelCompressor (channelIndex, state);
    notifyChanged();
}

void CompressorEditorPanel::notifyChanged()
{
    if (onChange != nullptr)
        onChange();
}

void CompressorEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (SamprLookAndFeel::panel());
}

void CompressorEditorPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto header = area.removeFromTop (22);
    headerLabel.setBounds (header.removeFromLeft (juce::jmax (160, header.getWidth() / 2)));
    enabledButton.setBounds (header.removeFromRight (48));
    area.removeFromTop (4);

    auto layoutRow = [&area] (juce::Label& label, juce::Slider& slider)
    {
        auto row = area.removeFromTop (22);
        label.setBounds (row.removeFromLeft (44));
        slider.setBounds (row);
        area.removeFromTop (4);
    };

    layoutRow (thresholdLabel, thresholdSlider);
    layoutRow (ratioLabel, ratioSlider);
    layoutRow (attackLabel, attackSlider);
    layoutRow (releaseLabel, releaseSlider);
    layoutRow (makeupLabel, makeupSlider);
}

} // namespace sampr