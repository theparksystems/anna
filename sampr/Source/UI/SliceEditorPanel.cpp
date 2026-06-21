#include "SliceEditorPanel.h"

#include "../DSP/RubberBandOfflineProcessor.h"

namespace sampr
{

SliceEditorPanel::SliceEditorPanel (SampleManager& manager)
    : sampleManager (manager)
{
    titleLabel.setText ("Slice Editor", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions { 15.0f, juce::Font::bold });
    bakeStatusLabel.setFont (juce::FontOptions { 12.0f });

    configureSlider (pitchSlider, -24.0, 24.0, 0.0, " st");
    configureSlider (timeSlider, 0.25, 4.0, 1.0, " x");

    pitchLabel.setText ("Pitch", juce::dontSendNotification);
    timeLabel.setText ("Time", juce::dontSendNotification);

    pitchSlider.onValueChange = [this] { onParameterChanged(); };
    timeSlider.onValueChange = [this] { onParameterChanged(); };

    loopButton.onClick = [this] { onParameterChanged(); };
    preRenderButton.setToggleState (true, juce::dontSendNotification);
    preRenderButton.onClick = [this] { onParameterChanged(); };

    bakeButton.onClick = [this]
    {
        sampleManager.requestSelectedSliceBake();
        updateBakeStatus();
    };

    triggerButton.onClick = [this]
    {
        if (onTrigger != nullptr)
            onTrigger();
    };

    for (auto* child : { &titleLabel, &bakeStatusLabel, &pitchLabel, &pitchSlider,
                         &timeLabel, &timeSlider, &loopButton, &preRenderButton,
                         &bakeButton, &triggerButton })
        addAndMakeVisible (child);

    syncFromSelectedSlice();
}

void SliceEditorPanel::setTriggerCallback (TriggerCallback callback)
{
    onTrigger = std::move (callback);
}

void SliceEditorPanel::setParameterChangedCallback (ParameterChangedCallback callback)
{
    onParameterChanged = std::move (callback);
}

void SliceEditorPanel::configureSlider (juce::Slider& slider,
                                        double min,
                                        double max,
                                        double value,
                                        const juce::String& suffix)
{
    slider.setRange (min, max, 0.01);
    slider.setValue (value, juce::dontSendNotification);
    slider.setTextValueSuffix (suffix);
    slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 72, 22);
}

void SliceEditorPanel::syncFromSelectedSlice()
{
    syncing = true;

    if (const auto* slice = sampleManager.getSelectedSlice())
    {
        pitchSlider.setValue (slice->pitchSemitones, juce::dontSendNotification);
        timeSlider.setValue (slice->timeRatio, juce::dontSendNotification);
        loopButton.setToggleState (slice->loop, juce::dontSendNotification);
        preRenderButton.setToggleState (slice->processingMode == StretchProcessingMode::preRenderOffline,
                                        juce::dontSendNotification);
    }
    else
    {
        pitchSlider.setValue (0.0, juce::dontSendNotification);
        timeSlider.setValue (1.0, juce::dontSendNotification);
        loopButton.setToggleState (false, juce::dontSendNotification);
    }

    syncing = false;
    updateBakeStatus();
}

void SliceEditorPanel::onParameterChanged()
{
    if (syncing)
        return;

    const auto assetId = sampleManager.getSelectedAssetId();
    auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
        return;

    const auto sliceIndex = asset->selectedSliceIndex;
    sampleManager.setSlicePitch (assetId, sliceIndex, static_cast<float> (pitchSlider.getValue()));
    sampleManager.setSliceTimeRatio (assetId, sliceIndex, static_cast<float> (timeSlider.getValue()));
    sampleManager.setSliceLoop (assetId, sliceIndex, loopButton.getToggleState());
    sampleManager.setSliceProcessingMode (assetId, sliceIndex,
                                            preRenderButton.getToggleState()
                                                ? StretchProcessingMode::preRenderOffline
                                                : StretchProcessingMode::rawPlaybackRate);

    updateBakeStatus();

    if (preRenderButton.getToggleState() && RubberBandOfflineProcessor::isAvailable())
        sampleManager.requestSelectedSliceBake();

    if (onParameterChanged != nullptr)
        onParameterChanged();
}

void SliceEditorPanel::updateBakeStatus()
{
    juce::String status;

    if (! RubberBandOfflineProcessor::isAvailable())
    {
        status = "Rubber Band: not linked (raw playback-rate fallback)";
    }
    else if (const auto* slice = sampleManager.getSelectedSlice())
    {
        if (slice->bakeInProgress)
            status = "Rubber Band: baking...";
        else if (slice->bakeReady)
            status = "Rubber Band: baked buffer ready";
        else if (sliceNeedsRubberBandBake (*slice))
            status = "Rubber Band: bake required";
        else
            status = "Rubber Band: no stretch needed";
    }
    else
    {
        status = "No slice selected";
    }

    bakeStatusLabel.setText (status, juce::dontSendNotification);
}

void SliceEditorPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    titleLabel.setBounds (area.removeFromTop (22));
    bakeStatusLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (8);

    auto row = area.removeFromTop (26);
    pitchLabel.setBounds (row.removeFromLeft (56));
    pitchSlider.setBounds (row);
    area.removeFromTop (6);

    row = area.removeFromTop (26);
    timeLabel.setBounds (row.removeFromLeft (56));
    timeSlider.setBounds (row);
    area.removeFromTop (8);

    loopButton.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    preRenderButton.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    auto buttons = area.removeFromTop (30);
    bakeButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    triggerButton.setBounds (buttons);
}

} // namespace sampr