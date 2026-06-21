#include "SliceEditorPanel.h"

#include "../DSP/OnsetDetector.h"
#include "../UI/SamprLookAndFeel.h"
#include "../DSP/RubberBandOfflineProcessor.h"
#include "../DSP/RubberBandStreamProcessor.h"

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
    configureSlider (sensitivitySlider, 0.5, 3.0, 1.0, " x");

    pitchLabel.setText ("Pitch", juce::dontSendNotification);
    timeLabel.setText ("Time", juce::dontSendNotification);
    sensitivityLabel.setText ("Onset", juce::dontSendNotification);

    processingModeBox.addItem ("Pre-render (offline)", 1);
    processingModeBox.addItem ("Real-time stretch", 2);
    processingModeBox.addItem ("Raw playback rate", 3);
    processingModeBox.setSelectedId (1, juce::dontSendNotification);

    pitchSlider.onValueChange = [this] { handleParameterChanged(); };
    timeSlider.onValueChange = [this] { handleParameterChanged(); };
    loopButton.onClick = [this] { handleParameterChanged(); };
    processingModeBox.onChange = [this] { handleParameterChanged(); };

    autoSliceButton.onClick = [this] { handleAutoSlice(); };

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

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (bakeStatusLabel);
    addAndMakeVisible (pitchLabel);
    addAndMakeVisible (pitchSlider);
    addAndMakeVisible (timeLabel);
    addAndMakeVisible (timeSlider);
    addAndMakeVisible (sensitivityLabel);
    addAndMakeVisible (sensitivitySlider);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (processingModeBox);
    addAndMakeVisible (autoSliceButton);
    addAndMakeVisible (bakeButton);
    addAndMakeVisible (triggerButton);

    syncFromSelectedSlice();
}

void SliceEditorPanel::setTriggerCallback (TriggerCallback callback)
{
    onTrigger = std::move (callback);
}

void SliceEditorPanel::setParameterChangedCallback (ParameterChangedCallback callback)
{
    parameterChangedCallback = std::move (callback);
}

void SliceEditorPanel::setAutoSliceCallback (AutoSliceCallback callback)
{
    autoSliceCallback = std::move (callback);
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

StretchProcessingMode SliceEditorPanel::modeFromCombo() const
{
    switch (processingModeBox.getSelectedId())
    {
        case 2:  return StretchProcessingMode::realTimeStream;
        case 3:  return StretchProcessingMode::rawPlaybackRate;
        default: return StretchProcessingMode::preRenderOffline;
    }
}

void SliceEditorPanel::setComboFromMode (StretchProcessingMode mode)
{
    switch (mode)
    {
        case StretchProcessingMode::realTimeStream:
            processingModeBox.setSelectedId (2, juce::dontSendNotification);
            break;
        case StretchProcessingMode::rawPlaybackRate:
            processingModeBox.setSelectedId (3, juce::dontSendNotification);
            break;
        default:
            processingModeBox.setSelectedId (1, juce::dontSendNotification);
            break;
    }
}

void SliceEditorPanel::syncFromSelectedSlice()
{
    syncing = true;

    if (const auto* slice = sampleManager.getSelectedSlice())
    {
        pitchSlider.setValue (slice->pitchSemitones, juce::dontSendNotification);
        timeSlider.setValue (slice->timeRatio, juce::dontSendNotification);
        loopButton.setToggleState (slice->loop, juce::dontSendNotification);
        setComboFromMode (slice->processingMode);
    }
    else
    {
        pitchSlider.setValue (0.0, juce::dontSendNotification);
        timeSlider.setValue (1.0, juce::dontSendNotification);
        loopButton.setToggleState (false, juce::dontSendNotification);
        processingModeBox.setSelectedId (1, juce::dontSendNotification);
    }

    syncing = false;
    updateBakeStatus();
}

void SliceEditorPanel::handleParameterChanged()
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
    sampleManager.setSliceProcessingMode (assetId, sliceIndex, modeFromCombo());

    updateBakeStatus();

    if (modeFromCombo() == StretchProcessingMode::preRenderOffline
        && RubberBandOfflineProcessor::isAvailable())
        sampleManager.requestSelectedSliceBake();

    if (parameterChangedCallback != nullptr)
        parameterChangedCallback();
}

void SliceEditorPanel::handleAutoSlice()
{
    const auto assetId = sampleManager.getSelectedAssetId();

    OnsetDetectorOptions options;
    options.sensitivity = static_cast<float> (sensitivitySlider.getValue());

    const auto count = sampleManager.autoSliceAsset (assetId, options);

    if (count > 0 && autoSliceCallback != nullptr)
        autoSliceCallback();
}

void SliceEditorPanel::updateBakeStatus()
{
    juce::String status;
    const auto mode = modeFromCombo();

    if (mode == StretchProcessingMode::realTimeStream)
    {
        status = RubberBandStreamProcessor::isAvailable()
                     ? "Real-time Rubber Band stretch active"
                     : "Real-time stretch unavailable (Rubber Band not linked)";
    }
    else if (! RubberBandOfflineProcessor::isAvailable())
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

void SliceEditorPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll (SamprLookAndFeel::panel());
    SamprLookAndFeel::paintVerticalDivider (g, 0, 0, bounds.getHeight());
}

void SliceEditorPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
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
    area.removeFromTop (6);

    row = area.removeFromTop (26);
    sensitivityLabel.setBounds (row.removeFromLeft (56));
    sensitivitySlider.setBounds (row);
    area.removeFromTop (8);

    loopButton.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    processingModeBox.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);
    autoSliceButton.setBounds (area.removeFromTop (26));
    area.removeFromTop (8);

    auto buttons = area.removeFromTop (30);
    bakeButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    triggerButton.setBounds (buttons);
}

} // namespace sampr