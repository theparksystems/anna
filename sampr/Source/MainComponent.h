#pragma once

#include <limits>

#include <juce_audio_utils/juce_audio_utils.h>

#include "Audio/AudioCommand.h"
#include "Audio/AudioEngine.h"
#include "Model/PatternStore.h"
#include "Model/ProjectController.h"
#include "Model/ProjectModel.h"
#include "Model/SampleManager.h"
#include "UI/ArrangementTimelineComponent.h"
#include "UI/PianoRollComponent.h"
#include "UI/SampleBrowserComponent.h"
#include "UI/SliceEditorPanel.h"
#include "UI/StepSequencerComponent.h"
#include "UI/WaveformComponent.h"

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer,
                            private juce::MidiInputCallback
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void refreshSampleViews();
    void refreshProjectViews();
    void publishPatternSnapshot();
    void loadSampleFromDisk();
    void saveProjectToDisk();
    void loadProjectFromDisk();
    void triggerSelectedSlice();
    void triggerPatternRow (int rowIndex, float velocity);
    void toggleTransport();
    void showAudioSettings();
    void updateStatusLabel();
    void pushBoolCommand (sampr::AudioCommandType type, bool value);
    void pushFloatCommand (sampr::AudioCommandType type, float value);
    void recordPianoNote (int pitch, float velocity);

    sampr::ProjectModel projectModel;
    sampr::AudioEngine audioEngine;
    sampr::SampleManager sampleManager;
    sampr::PatternStore patternStore;
    sampr::ProjectController projectController;

    sampr::SampleBrowserComponent sampleBrowser;
    sampr::WaveformComponent waveformView;
    sampr::SliceEditorPanel sliceEditor;
    sampr::StepSequencerComponent stepSequencer;
    sampr::PianoRollComponent pianoRoll;
    sampr::ArrangementTimelineComponent arrangementTimeline;
    juce::TabbedComponent patternTabs { juce::TabbedButtonBar::TabsAtTop };

    juce::TextButton playPauseButton { "Play" };
    juce::TextButton audioSettingsButton { "Audio Settings" };
    juce::TextButton saveProjectButton { "Save" };
    juce::TextButton loadProjectButton { "Load" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::ToggleButton midiRecordButton { "MIDI Record" };
    juce::ToggleButton songModeButton { "Song Mode" };
    juce::Slider bpmSlider;
    juce::Slider gainSlider;
    juce::Slider panSlider;
    juce::Label bpmLabel;
    juce::Label gainLabel;
    juce::Label panLabel;
    juce::Label statusLabel;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool transportPlaying = false;
    double deviceSampleRate = 44100.0;
    int lastSongPlaybackPatternIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};