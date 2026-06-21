#include "TransportBarComponent.h"

#include "SamprLookAndFeel.h"

namespace sampr
{

TransportBarComponent::TransportBarComponent()
{
    playButton.setTooltip ("Play (Space)");
    pauseButton.setTooltip ("Pause (Space)");
    stopButton.setTooltip ("Stop (Esc)");
    recordButton.setTooltip ("Record MIDI while playing. Step Seq: steps. Piano Roll: notes.");
    exportButton.setTooltip ("Export WAV (Ctrl+E)");
    settingsButton.setTooltip ("Audio device settings");
    bpmSlider.setTooltip ("Project tempo");
    metronomeButton.setTooltip ("Click on playback");
    loopButton.setTooltip ("Loop playback region");

    playButton.onClick = [this] { if (onPlay) onPlay(); };
    pauseButton.onClick = [this] { if (onPause) onPause(); };
    stopButton.onClick = [this] { if (onStop) onStop(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };
    settingsButton.onClick = [this] { if (onSettings) onSettings(); };

    recordButton.onClick = [this]
    {
        if (onRecord)
            onRecord (recordButton.getToggleState());
    };

    bpmSlider.setRange (60.0, 200.0, 0.1);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 64, 22);
    bpmLabel.setText ("BPM", juce::dontSendNotification);

    loopStartSlider.setRange (0.0, 256.0, 0.25);
    loopEndSlider.setRange (0.25, 256.0, 0.25);
    loopStartSlider.setValue (0.0, juce::dontSendNotification);
    loopEndSlider.setValue (16.0, juce::dontSendNotification);
    loopStartSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
    loopEndSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
    loopStartLabel.setText ("Loop In", juce::dontSendNotification);
    loopEndLabel.setText ("Loop Out", juce::dontSendNotification);

    auto notify = [this]
    {
        if (onValueChange)
            onValueChange();
    };

    bpmSlider.onValueChange = notify;
    metronomeButton.onClick = notify;
    loopButton.onClick = [this]
    {
        updateLoopControlStates();
        if (onValueChange)
            onValueChange();
    };
    loopStartSlider.onValueChange = notify;
    loopEndSlider.onValueChange = notify;

    positionLabel.setJustificationType (juce::Justification::centredLeft);
    positionLabel.setFont (juce::FontOptions { 12.0f });

    addAndMakeVisible (playButton);
    addAndMakeVisible (pauseButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (recordButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (bpmLabel);
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (positionLabel);
    addAndMakeVisible (metronomeButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (loopStartLabel);
    addAndMakeVisible (loopStartSlider);
    addAndMakeVisible (loopEndLabel);
    addAndMakeVisible (loopEndSlider);

    updateLoopControlStates();
}

void TransportBarComponent::updateLoopControlStates()
{
    const auto loopOn = loopButton.getToggleState();
    loopStartLabel.setEnabled (loopOn);
    loopEndLabel.setEnabled (loopOn);
    loopStartSlider.setEnabled (loopOn);
    loopEndSlider.setEnabled (loopOn);
}

void TransportBarComponent::setPlayCallback (PlayCallback callback) { onPlay = std::move (callback); }
void TransportBarComponent::setPauseCallback (PauseCallback callback) { onPause = std::move (callback); }
void TransportBarComponent::setStopCallback (StopCallback callback) { onStop = std::move (callback); }
void TransportBarComponent::setRecordCallback (RecordCallback callback) { onRecord = std::move (callback); }
void TransportBarComponent::setExportCallback (ExportCallback callback) { onExport = std::move (callback); }
void TransportBarComponent::setSettingsCallback (SettingsCallback callback) { onSettings = std::move (callback); }
void TransportBarComponent::setValueChangeCallback (ValueChangeCallback callback) { onValueChange = std::move (callback); }

void TransportBarComponent::setPlaying (bool playing)
{
    playButton.setEnabled (! playing);
    pauseButton.setEnabled (playing);
}

void TransportBarComponent::setPaused (bool paused)
{
    juce::ignoreUnused (paused);
}

void TransportBarComponent::setRecording (bool recording)
{
    recordButton.setToggleState (recording, juce::dontSendNotification);
}

void TransportBarComponent::setRecordEnabled (bool enabled)
{
    recordButton.setEnabled (enabled);

    if (! enabled)
        recordButton.setToggleState (false, juce::dontSendNotification);
}

bool TransportBarComponent::isRecording() const
{
    return recordButton.getToggleState();
}

void TransportBarComponent::setBpm (double bpm)
{
    bpmSlider.setValue (bpm, juce::dontSendNotification);
}

void TransportBarComponent::setPositionText (const juce::String& text)
{
    positionLabel.setText (text, juce::dontSendNotification);
}

double TransportBarComponent::getBpm() const { return bpmSlider.getValue(); }
bool TransportBarComponent::isMetronomeEnabled() const { return metronomeButton.getToggleState(); }
bool TransportBarComponent::isLoopEnabled() const { return loopButton.getToggleState(); }
double TransportBarComponent::getLoopStartBeats() const { return loopStartSlider.getValue(); }
double TransportBarComponent::getLoopEndBeats() const { return loopEndSlider.getValue(); }

void TransportBarComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    g.fillAll (SamprLookAndFeel::background());
    SamprLookAndFeel::paintHorizontalDivider (g, 0, bounds.getWidth());
}

void TransportBarComponent::resized()
{
    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    row.alignItems = juce::FlexBox::AlignItems::center;
    row.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    const juce::FlexItem::Margin margin { 0, 2, 0, 0 };

    row.items.add (juce::FlexItem (playButton).withWidth (36).withHeight (28).withMargin (margin));
    row.items.add (juce::FlexItem (pauseButton).withWidth (36).withHeight (28).withMargin (margin));
    row.items.add (juce::FlexItem (stopButton).withWidth (36).withHeight (28).withMargin (margin));
    row.items.add (juce::FlexItem (recordButton).withWidth (72).withHeight (28).withMargin (margin));
    row.items.add (juce::FlexItem (exportButton).withWidth (88).withHeight (28).withMargin (margin));
    row.items.add (juce::FlexItem (settingsButton).withWidth (56).withHeight (28).withMargin (margin));
    row.items.add (juce::FlexItem (bpmLabel).withWidth (34).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (bpmSlider).withWidth (120).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (metronomeButton).withWidth (84).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (loopButton).withWidth (52).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (loopStartLabel).withWidth (52).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (loopStartSlider).withWidth (88).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (loopEndLabel).withWidth (58).withHeight (24).withMargin (margin));
    row.items.add (juce::FlexItem (loopEndSlider).withMinWidth (88.0f).withHeight (24).withFlex (1.0f).withMargin (margin));
    row.items.add (juce::FlexItem (positionLabel).withMinWidth (140.0f).withHeight (24).withFlex (0.8f));

    row.performLayout (getLocalBounds().reduced (4, 2));
}

} // namespace sampr