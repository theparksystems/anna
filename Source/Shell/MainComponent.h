#pragma once

#include <atomic>

#include <juce_audio_utils/juce_audio_utils.h>

#include "../Audio/AudioCommand.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/OfflineExporter.h"
#include "../Model/MixerState.h"
#include "../Model/PatternStore.h"
#include "../Model/ProjectController.h"
#include "../Model/ProjectModel.h"
#include "../Model/SampleManager.h"
#include "../Assistant/AssistantClient.h"
#include "../UI/ArrangementTimelineComponent.h"
#include "../UI/AssistantPanel.h"
#include "../UI/FxWorkspaceComponent.h"
#include "../UI/MixerComponent.h"
#include "../UI/PianoRollComponent.h"
#include "../UI/SampleBrowserComponent.h"
#include "../Patches/SliceEditorPanel.h"
#include "../Patches/StepSequencerComponent.h"
#include "../UI/TopNavBarComponent.h"
#include "../UI/TransportBarComponent.h"
#include "../UI/WaveformComponent.h"

class MainComponent final : public juce::AudioAppComponent,
                            public juce::DragAndDropContainer,
                            private juce::Timer,
                            private juce::ChangeListener,
                            private juce::MidiInputCallback,
                            private juce::FileDragAndDropTarget
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

private:
    void timerCallback() override;
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    void refreshSampleViews();
    void refreshProjectViews();
    void publishPatternSnapshot();
    void applyTransportSettings();
    void importSampleFiles (const juce::StringArray& paths);
    void loadSampleFromDisk();
    void togglePlayPause();
    void stopTransport();
    void updateToolbarState();
    void setupKeyboardShortcuts();
    void saveProjectToDisk();
    void loadProjectFromDisk();
    void exportAudioToDisk();
    void exportSoundCloudReady();
    void showYouTubeImportPopup();
    void startYouTubeImport (const juce::String& url, const juce::String& format);
    void finishYouTubeImport (const juce::File& audioFile, const juce::String& errorMessage);
    void finishAsyncExport (const juce::File& file,
                            const sampr::OfflineExportResult& result,
                            bool wasPlaying);
    void triggerSelectedSlice();
    void triggerPatternRow (int rowIndex, float velocity);
    void showAudioSettings();
    void showSliceEditorPopup();
    void saveBeatToDisk();
    void loadBeatFromDisk();
    void applyLoadedBeat (const sampr::Pattern& pattern);
    void updateStatusLabel();
    void pushBoolCommand (sampr::AudioCommandType type, bool value);
    void pushFloatCommand (sampr::AudioCommandType type, float value);
    void recordPianoNote (int pitch, float velocity);
    void drainAudioDiagnostics();
    void toggleFullscreen();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void focusActiveEditorTab();
    void showUserMessage (const juce::String& message);
    bool ensureArrangementHasPlayableClip();
    void openAssistantWithContext (sampr::ContextScope scope,
                                   int channelIndex,
                                   const juce::String& seedMessage);

    class AsyncExportJob;
    class AsyncYouTubeImportJob;

    sampr::AssistantClient assistantClient;
    sampr::ProjectModel projectModel;
    sampr::MixerState mixerState;
    sampr::AudioEngine audioEngine;
    sampr::SampleManager sampleManager;
    sampr::PatternStore patternStore;
    sampr::ProjectController projectController;

    sampr::TopNavBarComponent topNavBar;
    sampr::TransportBarComponent transportBar;
    sampr::MixerComponent mixerPanel;
    sampr::SampleBrowserComponent sampleBrowser;
    sampr::WaveformComponent waveformView;
    sampr::SliceEditorPanel sliceEditor;
    sampr::StepSequencerComponent stepSequencer;
    sampr::PianoRollComponent pianoRoll;
    sampr::ArrangementTimelineComponent arrangementTimeline;
    sampr::FxWorkspaceComponent fxWorkspace;
    sampr::AssistantPanel assistantPanel;
    juce::Component centralEditor;
    juce::Component chopArea;
    juce::Label chopHintLabel;
    juce::TextButton sliceEditorButton { "Edit Slices" };
    juce::TabbedComponent patternTabs { juce::TabbedButtonBar::TabsAtTop };

    juce::TextButton saveProjectButton { "Save" };
    juce::TextButton loadProjectButton { "Load" };
    juce::TextButton saveBeatButton { "Save Beat" };
    juce::TextButton loadBeatButton { "Load Beat" };
    juce::TextButton askPatternButton { "Ask Gemma" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::ToggleButton songModeButton { "Song Mode" };
    juce::TextButton youtubeImportButton { "YouTube" };
    juce::Label statusLabel;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<AsyncExportJob> exportJob;
    std::unique_ptr<AsyncYouTubeImportJob> youtubeImportJob;
    juce::Component::SafePointer<juce::DialogWindow> sliceEditorDialog;
    juce::Component::SafePointer<juce::DialogWindow> youtubeImportDialog;

    bool transportPlaying = false;
    double deviceSampleRate = 44100.0;
    int lastSongPlaybackPatternIndex = -1;
    bool dragSamplesActive = false;
    juce::String audioWarningText;
    int audioWarningTicks = 0;
    juce::String userMessageText;
    int userMessageTicks = 0;
    int assistantHealthTicks = 0;
    std::atomic<float> exportProgress { 0.0f };
    bool exportInProgress = false;
    bool youtubeImportInProgress = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
