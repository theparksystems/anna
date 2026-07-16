#include "MainComponent.h"

#include "../App/MvpFlow.h"
#include "../App/SampleImportService.h"
#include "../Audio/OfflineExporter.h"
#include "../Model/NoteEvent.h"
#include "../Persistence/BeatSerializer.h"
#include "../UI/SamprLookAndFeel.h"

#include <cmath>
#include <memory>

namespace
{
constexpr int kTabStepSeq = 0;
constexpr int kTabPianoRoll = 1;
constexpr int kTabArrangement = 2;
constexpr int kTabFxRack = 3;
constexpr int kTabAssistant = 4;

juce::String makeSafeFileStem (juce::String name)
{
    name = name.trim();
    if (name.isEmpty())
        name = "ANNA Track";

    for (const auto c : juce::String ("<>:\"/\\|?*"))
        name = name.replaceCharacter (c, '-');

    return name;
}

juce::File getUniqueChildFile (const juce::File& folder,
                               const juce::String& stem,
                               const juce::String& extension)
{
    auto file = folder.getChildFile (stem + extension);

    for (int i = 2; file.existsAsFile(); ++i)
        file = folder.getChildFile (stem + " " + juce::String (i) + extension);

    return file;
}

juce::String quoteCommandArg (juce::String value)
{
    value = value.replace ("\"", "\\\"");
    return "\"" + value + "\"";
}

juce::File findSourceRoot()
{
    juce::Array<juce::File> candidates;
    const auto envRoot = juce::SystemStats::getEnvironmentVariable ("ANNA_SOURCE_ROOT", {});
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    if (envRoot.isNotEmpty())
        candidates.add (juce::File (envRoot));

    candidates.add (cwd);
    candidates.add (cwd.getChildFile ("anna"));
    candidates.add (exe.getParentDirectory());
    candidates.add (exe.getParentDirectory().getParentDirectory().getParentDirectory().getChildFile ("anna"));
    candidates.add (exe.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory().getChildFile ("anna"));
    candidates.add (juce::File ("C:\\anna\\anna"));

    for (const auto& candidate : candidates)
    {
        if (candidate.getChildFile ("tools").getChildFile ("youtube_sample_ingest.py").existsAsFile())
            return candidate;
    }

    return {};
}

juce::String getLastNonEmptyLine (const juce::String& text)
{
    juce::StringArray lines;
    lines.addLines (text);

    for (int i = lines.size() - 1; i >= 0; --i)
    {
        const auto line = lines[i].trim();
        if (line.isNotEmpty())
            return line;
    }

    return {};
}

juce::String buildSampledFromText (const sampr::ProjectModel& project)
{
    juce::String text;

    for (const auto& ref : project.getSampleRefs())
    {
        if (! ref.origin.hasSource())
            continue;

        const auto title = ref.origin.sourceTitle.isNotEmpty() ? ref.origin.sourceTitle : ref.displayName;
        text << "- " << title;

        if (ref.origin.sourceAuthor.isNotEmpty())
            text << " by " << ref.origin.sourceAuthor;

        if (ref.origin.sourceUrl.isNotEmpty())
            text << " - " << ref.origin.sourceUrl;

        text << "\n";
    }

    if (text.isEmpty())
        text = "No external samples with source metadata were used.\n";

    return text;
}
}

class MainComponent::AsyncExportJob final : private juce::Thread
{
public:
    AsyncExportJob (MainComponent& ownerIn,
                    juce::File destinationIn,
                    sampr::OfflineExportOptions optionsIn,
                    bool wasPlayingIn)
        : juce::Thread ("ANNA Export"),
          owner (ownerIn),
          ownerPtr (&ownerIn),
          destination (std::move (destinationIn)),
          options (optionsIn),
          wasPlaying (wasPlayingIn)
    {
    }

    ~AsyncExportJob() override
    {
        requestCancel();
        signalThreadShouldExit();
        stopThread (30000);
    }

    void start()
    {
        startThread (juce::Thread::Priority::normal);
    }

    void requestCancel() noexcept
    {
        cancelRequested.store (true, std::memory_order_release);
    }

private:
    void run() override
    {
        owner.exportProgress.store (0.0f, std::memory_order_relaxed);

        const auto result = sampr::OfflineExporter::exportToWav (
            destination,
            owner.audioEngine,
            owner.patternStore,
            owner.projectController,
            owner.projectModel,
            options,
            [this] (float progress)
            {
                owner.exportProgress.store (juce::jlimit (0.0f, 1.0f, progress),
                                            std::memory_order_relaxed);
            },
            [this]
            {
                return cancelRequested.load (std::memory_order_acquire) || threadShouldExit();
            });

        juce::MessageManager::callAsync ([safeOwner = ownerPtr, result, file = destination, wasPlaying = wasPlaying]
        {
            if (safeOwner != nullptr)
                safeOwner->finishAsyncExport (file, result, wasPlaying);
        });
    }

    MainComponent& owner;
    juce::Component::SafePointer<MainComponent> ownerPtr;
    juce::File destination;
    sampr::OfflineExportOptions options;
    bool wasPlaying = false;
    std::atomic<bool> cancelRequested { false };
};

class MainComponent::AsyncYouTubeImportJob final : private juce::Thread
{
public:
    AsyncYouTubeImportJob (MainComponent& ownerIn, juce::String urlIn, juce::String formatIn)
        : juce::Thread ("ANNA YouTube Import"),
          ownerPtr (&ownerIn),
          url (std::move (urlIn)),
          format (std::move (formatIn))
    {
    }

    ~AsyncYouTubeImportJob() override
    {
        signalThreadShouldExit();
        stopThread (30000);
    }

    void start()
    {
        startThread (juce::Thread::Priority::normal);
    }

private:
    void run() override
    {
        const auto sourceRoot = findSourceRoot();

        if (sourceRoot == juce::File())
        {
            finish ({}, "Could not locate tools\\youtube_sample_ingest.py.");
            return;
        }

        const auto script = sourceRoot.getChildFile ("tools").getChildFile ("youtube_sample_ingest.py");
        const auto outputFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                      .getChildFile ("ANNA YouTube Imports");

        if (! outputFolder.exists() && ! outputFolder.createDirectory())
        {
            finish ({}, "Could not create import folder:\n" + outputFolder.getFullPathName());
            return;
        }

        juce::StringArray launchers;
        const auto envPython = juce::SystemStats::getEnvironmentVariable ("ANNA_PYTHON", {});
        if (envPython.isNotEmpty())
            launchers.add (quoteCommandArg (envPython));
        launchers.add (quoteCommandArg ("C:\\Users\\User\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\python\\python.exe"));
        launchers.add ("py -3");
        launchers.add ("python");
        juce::String lastOutput;

        for (const auto& launcher : launchers)
        {
            if (threadShouldExit())
                return;

            const auto command = launcher
                + " " + quoteCommandArg (script.getFullPathName())
                + " " + quoteCommandArg (url)
                + " --out " + quoteCommandArg (outputFolder.getFullPathName())
                + " --format " + format
                + " --json-output";

            juce::ChildProcess process;

            if (! process.start (command))
            {
                lastOutput = "Could not start: " + launcher;
                continue;
            }

            while (process.isRunning())
            {
                if (threadShouldExit())
                    return;

                lastOutput << process.readAllProcessOutput();
                wait (100);
            }

            lastOutput << process.readAllProcessOutput();
            const auto exitCode = process.getExitCode();

            if (exitCode != 0)
                continue;

            const auto parsed = juce::JSON::parse (getLastNonEmptyLine (lastOutput));

            if (! parsed.isObject())
                finish ({}, "Import completed but ANNA could not read the converter result.\n" + lastOutput);
            else
                finish (juce::File (parsed.getProperty ("audio", juce::String()).toString()), {});

            return;
        }

        finish ({}, "YouTube import failed.\n\nCheck the converter details below. The source may be unavailable, blocked by YouTube, or missing a local conversion dependency.\n\n" + lastOutput);
    }

    void finish (juce::File audioFile, juce::String errorMessage)
    {
        juce::MessageManager::callAsync ([safeOwner = ownerPtr,
                                          file = std::move (audioFile),
                                          error = std::move (errorMessage)]
        {
            if (safeOwner != nullptr)
                safeOwner->finishYouTubeImport (file, error);
        });
    }

    juce::Component::SafePointer<MainComponent> ownerPtr;
    juce::String url;
    juce::String format;
};

MainComponent::MainComponent()
    : sampleManager (audioEngine),
      patternStore (projectModel, sampleManager),
      projectController (projectModel, sampleManager, patternStore),
      mixerPanel (patternStore, audioEngine),
      sampleBrowser (sampleManager),
      sliceEditor (sampleManager),
      stepSequencer (patternStore),
      pianoRoll (patternStore, sampleManager),
      arrangementTimeline (projectController, patternStore),
      fxWorkspace (patternStore),
      assistantPanel (assistantClient, patternStore, projectModel, sampleManager, audioEngine)
{
    formatManager.registerBasicFormats();
    setAudioChannels (2, 2);
    setWantsKeyboardFocus (true);

    chopHintLabel.setText ("Click waveform to add slice  |  Right-click marker to delete  |  Drag edges to trim  |  Double-click for editor",
                           juce::dontSendNotification);
    chopHintLabel.setFont (juce::FontOptions { 10.5f });
    chopHintLabel.setColour (juce::Label::textColourId, sampr::SamprLookAndFeel::textMuted());
    chopHintLabel.setJustificationType (juce::Justification::centredLeft);

    chopArea.addAndMakeVisible (chopHintLabel);
    chopArea.addAndMakeVisible (waveformView);
    chopArea.addAndMakeVisible (sliceEditorButton);
    centralEditor.addAndMakeVisible (chopArea);
    centralEditor.addAndMakeVisible (patternTabs);
    addAndMakeVisible (centralEditor);

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);

    deviceManager.addMidiInputDeviceCallback ({}, this);

    const auto& settings = projectModel.getSettings();
    transportBar.setBpm (settings.bpm);
    mixerState.syncFromProjectSettings (settings.bpm,
                                        settings.masterGain,
                                        settings.metronomeEnabled,
                                        settings.loopEnabled,
                                        settings.loopStartBeats,
                                        settings.loopEndBeats);

    sampleManager.setChangeCallback ([this]
    {
        refreshSampleViews();
        mixerPanel.refresh();
        publishPatternSnapshot();
    });

    patternStore.setUndoCheckpoint ([this] { projectController.beginEdit ("Pattern edit"); });

    patternStore.setChangeCallback ([this]
    {
        stepSequencer.refresh();
        pianoRoll.refresh();
        arrangementTimeline.refresh();
        mixerPanel.refresh();
        fxWorkspace.refreshFromStore();
        assistantPanel.refreshChannelList();
        publishPatternSnapshot();
    });

    projectController.setChangeCallback ([this] { refreshProjectViews(); });

    arrangementTimeline.setChangeCallback ([this]
    {
        publishPatternSnapshot();
        updateStatusLabel();
    });

    stepSequencer.setChangeCallback ([this] { publishPatternSnapshot(); });
    pianoRoll.setChangeCallback ([this] { publishPatternSnapshot(); });
    mixerPanel.setChangeCallback ([this] { publishPatternSnapshot(); });
    mixerPanel.setFxOpenCallback ([this] (int rowIndex)
    {
        patternTabs.setCurrentTabIndex (kTabFxRack);
        fxWorkspace.setChannel (rowIndex);
        focusActiveEditorTab();
    });
    mixerPanel.setAskCallback ([this] (int rowIndex)
    {
        const auto& pattern = patternStore.getCurrentPattern();
        if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
            return;

        const auto& row = pattern.rows[static_cast<size_t> (rowIndex)];
        const auto label = row.label.isNotEmpty() ? row.label : ("channel " + juce::String (rowIndex + 1));
        openAssistantWithContext (sampr::ContextScope::channel,
                                  rowIndex,
                                  "Analyze " + label + " - what could make this track sound better?");
    });
    fxWorkspace.setChangeCallback ([this] { publishPatternSnapshot(); });
    fxWorkspace.setAskCallback ([this] (int rowIndex)
    {
        const auto& pattern = patternStore.getCurrentPattern();
        if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
            return;

        const auto& row = pattern.rows[static_cast<size_t> (rowIndex)];
        const auto label = row.label.isNotEmpty() ? row.label : ("channel " + juce::String (rowIndex + 1));
        openAssistantWithContext (sampr::ContextScope::channel,
                                  rowIndex,
                                  "Review FX settings for " + label + " - any issues with color, filtering, drive, width, compression, delay, or reverb?");
    });
    mixerPanel.setMasterGainChangeCallback ([this]
    {
        projectModel.getSettings().masterGain = static_cast<float> (mixerPanel.getMasterGain());
        pushFloatCommand (sampr::AudioCommandType::setMasterGain, projectModel.getSettings().masterGain);
    });
    mixerPanel.setMasterGain (settings.masterGain);

    patternTabs.addTab ("Step Seq (1)", sampr::SamprLookAndFeel::panel(), &stepSequencer, false);
    patternTabs.addTab ("Piano Roll (2)", sampr::SamprLookAndFeel::panel(), &pianoRoll, false);
    patternTabs.addTab ("Arrangement (3)", sampr::SamprLookAndFeel::panel(), &arrangementTimeline, false);
    patternTabs.addTab ("FX Rack (4)", sampr::SamprLookAndFeel::panel(), &fxWorkspace, false);
    patternTabs.addTab ("Assistant (5)", sampr::SamprLookAndFeel::panel(), &assistantPanel, false);
    patternTabs.setCurrentTabIndex (0);

    patternTabs.getTabbedButtonBar().addChangeListener (this);

    sampleBrowser.setLoadRequestedCallback ([this] { loadSampleFromDisk(); });
    sampleBrowser.setSelectionCallback ([this] (sampr::AssetId)
    {
        refreshSampleViews();
        sliceEditor.syncFromSelectedSlice();
    });

    waveformView.setSliceClickedCallback ([this] (int sliceIndex)
    {
        sampleManager.selectSlice (sampleManager.getSelectedAssetId(), sliceIndex);
        sliceEditor.syncFromSelectedSlice();
        refreshSampleViews();
    });

    waveformView.setAddSliceCallback ([this] (int samplePosition)
    {
        sampleManager.addSliceAtSample (sampleManager.getSelectedAssetId(), samplePosition);
        sliceEditor.syncFromSelectedSlice();
        refreshSampleViews();
    });

    waveformView.setRemoveSliceCallback ([this] (int sliceIndex)
    {
        sampleManager.removeSlice (sampleManager.getSelectedAssetId(), sliceIndex);
        sliceEditor.syncFromSelectedSlice();
        refreshSampleViews();
    });

    waveformView.setOpenSliceEditorCallback ([this] { showSliceEditorPopup(); });
    sliceEditorButton.setTooltip ("Open slice editor — pitch, time stretch, auto-slice, and bake");
    sliceEditorButton.onClick = [this] { showSliceEditorPopup(); };

    waveformView.setSliceTrimChangedCallback ([this] (int sliceIndex, int startSample, int endSample)
    {
        const auto assetId = sampleManager.getSelectedAssetId();
        auto* asset = sampleManager.getAsset (assetId);

        if (asset == nullptr || ! juce::isPositiveAndBelow (sliceIndex, static_cast<int> (asset->slices.size())))
            return;

        const auto& slice = asset->slices[static_cast<size_t> (sliceIndex)];

        if (startSample != slice.startSample)
        {
            sampleManager.updateSliceRange (assetId, sliceIndex, startSample, slice.endSample);
        }
        else if (endSample != slice.endSample)
        {
            if (sliceIndex + 1 < static_cast<int> (asset->slices.size()))
            {
                const auto& nextSlice = asset->slices[static_cast<size_t> (sliceIndex + 1)];
                sampleManager.updateSliceRange (assetId, sliceIndex + 1, endSample, nextSlice.endSample);
            }
            else
            {
                sampleManager.updateSliceRange (assetId, sliceIndex, slice.startSample, endSample);
            }
        }

        sliceEditor.syncFromSelectedSlice();
        refreshSampleViews();
    });

    topNavBar.setFullscreenToggleCallback ([this] { toggleFullscreen(); });

    sliceEditor.setTriggerCallback ([this] { triggerSelectedSlice(); });
    sliceEditor.setParameterChangedCallback ([this] { refreshSampleViews(); });
    sliceEditor.setAutoSliceCallback ([this]
    {
        refreshSampleViews();
        sliceEditor.syncFromSelectedSlice();
        publishPatternSnapshot();
    });
    sliceEditor.setOpenFxCallback ([this]
    {
        const auto assetId = sampleManager.getSelectedAssetId();
        const auto* asset = sampleManager.getAsset (assetId);

        if (asset == nullptr)
        {
            showUserMessage ("Select a sample slice first.");
            return;
        }

        const auto sliceIndex = asset->selectedSliceIndex;
        const auto& pattern = patternStore.getCurrentPattern();

        for (int i = 0; i < static_cast<int> (pattern.rows.size()); ++i)
        {
            const auto& row = pattern.rows[static_cast<size_t> (i)];

            if (row.assetId == assetId && row.sliceIndex == sliceIndex)
            {
                patternTabs.setCurrentTabIndex (kTabFxRack);
                fxWorkspace.setChannel (i);
                focusActiveEditorTab();
                showUserMessage ("FX opened for selected slice channel.");
                return;
            }
        }

        showUserMessage ("Add this slice to the sequencer, then open FX.");
    });

    transportBar.setPlayCallback ([this]
    {
        transportPlaying = true;
        pushBoolCommand (sampr::AudioCommandType::transportPlay, true);
        transportBar.setPlaying (true);
        updateStatusLabel();
    });

    transportBar.setPauseCallback ([this]
    {
        transportPlaying = false;
        pushBoolCommand (sampr::AudioCommandType::transportPause, false);
        transportBar.setPlaying (false);
        updateStatusLabel();
    });

    transportBar.setStopCallback ([this]
    {
        transportPlaying = false;
        pushBoolCommand (sampr::AudioCommandType::transportStop, false);
        transportBar.setPlaying (false);
        lastSongPlaybackPatternIndex = -1;
        updateStatusLabel();
    });

    transportBar.setRecordCallback ([this] (bool recording)
    {
        transportBar.setRecording (recording);

        if (recording && ! transportPlaying)
            showUserMessage ("Record armed — press Play to capture MIDI steps or piano roll notes.");
        else if (! recording)
            showUserMessage ("Record off.");

        updateStatusLabel();
    });

    transportBar.setExportCallback ([this] { exportAudioToDisk(); });
    transportBar.setSoundCloudExportCallback ([this] { exportSoundCloudReady(); });
    transportBar.setSettingsCallback ([this] { showAudioSettings(); });
    transportBar.setValueChangeCallback ([this] { applyTransportSettings(); });

    saveProjectButton.onClick = [this] { saveProjectToDisk(); };
    loadProjectButton.onClick = [this] { loadProjectFromDisk(); };
    saveBeatButton.onClick = [this] { saveBeatToDisk(); };
    loadBeatButton.onClick = [this] { loadBeatFromDisk(); };
    undoButton.onClick = [this]
    {
        if (projectController.undo())
            refreshProjectViews();
        updateToolbarState();
    };
    redoButton.onClick = [this]
    {
        if (projectController.redo())
            refreshProjectViews();
        updateToolbarState();
    };

    saveProjectButton.setTooltip ("Save project (Ctrl+S)");
    loadProjectButton.setTooltip ("Load project (Ctrl+O)");
    saveBeatButton.setTooltip ("Save current pattern as beat preset");
    loadBeatButton.setTooltip ("Load beat preset into current pattern");
    askPatternButton.setTooltip ("Ask Gemma about the current pattern mix");
    askPatternButton.onClick = [this]
    {
        openAssistantWithContext (sampr::ContextScope::pattern,
                                  fxWorkspace.getChannelIndex(),
                                  "Analyze this pattern — which channels sound flat, muddy, or unbalanced?");
    };
    undoButton.setTooltip ("Undo (Ctrl+Z)");
    redoButton.setTooltip ("Redo (Ctrl+Y)");
    songModeButton.setTooltip ("Song Mode plays the Arrangement timeline instead of looping the current pattern.");
    youtubeImportButton.setTooltip ("Search YouTube, paste a source URL, convert audio, and import it with metadata.");
    songModeButton.onClick = [this]
    {
        const auto songMode = songModeButton.getToggleState();

        if (songMode && ! ensureArrangementHasPlayableClip())
        {
            songModeButton.setToggleState (false, juce::dontSendNotification);
            showUserMessage ("Song Mode needs a pattern before it can play.");
            updateStatusLabel();
            return;
        }

        stopTransport();
        projectController.setPlaybackMode (songMode ? sampr::PlaybackMode::song
                                                    : sampr::PlaybackMode::pattern);
        lastSongPlaybackPatternIndex = -1;
        publishPatternSnapshot();

        if (songMode)
        {
            patternTabs.setCurrentTabIndex (kTabArrangement);
            focusActiveEditorTab();
            showUserMessage ("Song Mode on — edit and play clips in the Arrangement tab.");
        }
        else
        {
            showUserMessage ("Pattern Mode — looping the current pattern.");
        }

        updateStatusLabel();
    };
    youtubeImportButton.onClick = [this] { showYouTubeImportPopup(); };

    songModeButton.setToggleState (settings.playbackMode == sampr::PlaybackMode::song,
                                   juce::dontSendNotification);

    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setFont (juce::FontOptions { 12.0f });

    addAndMakeVisible (topNavBar);
    addAndMakeVisible (transportBar);
    addAndMakeVisible (mixerPanel);
    addAndMakeVisible (sampleBrowser);
    addAndMakeVisible (saveProjectButton);
    addAndMakeVisible (loadProjectButton);
    addAndMakeVisible (saveBeatButton);
    addAndMakeVisible (loadBeatButton);
    addAndMakeVisible (askPatternButton);
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    addAndMakeVisible (songModeButton);
    addAndMakeVisible (youtubeImportButton);
    addAndMakeVisible (statusLabel);

    applyTransportSettings();
    pushBoolCommand (sampr::AudioCommandType::setTestToneEnabled, false);
    audioEngine.setSequencerEnabled (true);
    audioEngine.setNoteSequencerEnabled (true);

    setupKeyboardShortcuts();
    updateToolbarState();

    publishPatternSnapshot();
    assistantClient.checkHealth ([this] (bool online, const juce::String& detail)
    {
        juce::ignoreUnused (online, detail);
        updateStatusLabel();
    });

    updateStatusLabel();
    startTimerHz (60);
}

MainComponent::~MainComponent()
{
    youtubeImportJob.reset();
    exportJob.reset();
    patternTabs.getTabbedButtonBar().removeChangeListener (this);
    deviceManager.removeMidiInputDeviceCallback ({}, this);
    sampleManager.clear();
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    deviceSampleRate = sampleRate;
    audioEngine.prepare (sampleRate, samplesPerBlockExpected);
    publishPatternSnapshot();
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    audioEngine.render (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    audioEngine.release();
}

void MainComponent::applyTransportSettings()
{
    auto& settings = projectModel.getSettings();
    settings.bpm = transportBar.getBpm();
    settings.metronomeEnabled = transportBar.isMetronomeEnabled();
    settings.loopEnabled = transportBar.isLoopEnabled();
    settings.loopStartBeats = transportBar.getLoopStartBeats();
    settings.loopEndBeats = transportBar.getLoopEndBeats();

    pushFloatCommand (sampr::AudioCommandType::setBpm, static_cast<float> (settings.bpm));
    pushBoolCommand (sampr::AudioCommandType::setMetronomeEnabled, settings.metronomeEnabled);
    pushBoolCommand (sampr::AudioCommandType::setLoopEnabled, settings.loopEnabled);
    pushFloatCommand (sampr::AudioCommandType::setLoopStartBeats, static_cast<float> (settings.loopStartBeats));
    pushFloatCommand (sampr::AudioCommandType::setLoopEndBeats, static_cast<float> (settings.loopEndBeats));
    pushFloatCommand (sampr::AudioCommandType::setMasterGain, settings.masterGain);

    mixerState.syncFromProjectSettings (settings.bpm,
                                        settings.masterGain,
                                        settings.metronomeEnabled,
                                        settings.loopEnabled,
                                        settings.loopStartBeats,
                                        settings.loopEndBeats);
    publishPatternSnapshot();
    updateStatusLabel();
}

void MainComponent::timerCallback()
{
    const auto enginePlaying = audioEngine.isTransportPlaying();

    if (enginePlaying != transportPlaying)
    {
        transportPlaying = enginePlaying;
        transportBar.setPlaying (enginePlaying);
    }

    const auto bpm = transportBar.getBpm();
    const auto beats = audioEngine.getPlayheadBeats (bpm);
    const auto bars = beats / 4.0;
    const auto loopText = transportBar.isLoopEnabled()
                              ? juce::String ("  loop ") + juce::String (transportBar.getLoopStartBeats(), 1)
                                + "-" + juce::String (transportBar.getLoopEndBeats(), 1)
                              : juce::String();
    transportBar.setPositionText ("Bar " + juce::String (bars + 1.0, 2)
                                  + "  Beat " + juce::String (beats, 2) + loopText);

    stepSequencer.setCurrentStepIndex (audioEngine.getCurrentSequencerStep());
    pianoRoll.setPlayheadBeats (beats);
    sliceEditor.syncFromSelectedSlice();
    updateToolbarState();

    transportBar.setRecordEnabled (! patternStore.getCurrentPattern().rows.empty());

    if (userMessageTicks > 0)
        --userMessageTicks;

    if (audioWarningTicks > 0)
        --audioWarningTicks;

    if (++assistantHealthTicks >= 300)
    {
        assistantHealthTicks = 0;

        if (! assistantClient.isBusy())
        {
            assistantClient.checkHealth ([this] (bool, const juce::String&)
            {
                assistantPanel.updateOnlineStatus();
                updateStatusLabel();
            });
        }
    }

    updateStatusLabel();
    drainAudioDiagnostics();
}

void MainComponent::setupKeyboardShortcuts()
{
    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();

    if (exportInProgress)
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey && exportJob != nullptr)
        {
            exportJob->requestCancel();
            showUserMessage ("Cancelling export...");
            return true;
        }

        showUserMessage ("Export in progress - press Esc to cancel.");
        return true;
    }

    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayPause();
        return true;
    }

    if (key == juce::KeyPress::F11Key)
    {
        toggleFullscreen();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::escapeKey)
    {
        stopTransport();
        return true;
    }

    if (mods.isCtrlDown() && key.getTextCharacter() == 's')
    {
        saveProjectToDisk();
        return true;
    }

    if (mods.isCtrlDown() && key.getTextCharacter() == 'o')
    {
        loadProjectFromDisk();
        return true;
    }

    if (mods.isCtrlDown() && key.getTextCharacter() == 'e')
    {
        exportAudioToDisk();
        return true;
    }

    if (mods.isCtrlDown() && mods.isShiftDown() && key.getTextCharacter() == 'l')
    {
        loadSampleFromDisk();
        return true;
    }

    if (mods.isCtrlDown() && key.getTextCharacter() == 'z')
    {
        if (projectController.undo())
            refreshProjectViews();
        updateToolbarState();
        return true;
    }

    if (mods.isCtrlDown() && (key.getTextCharacter() == 'y'
                              || (mods.isShiftDown() && key.getTextCharacter() == 'z')))
    {
        if (projectController.redo())
            refreshProjectViews();
        updateToolbarState();
        return true;
    }

    if (key.getTextCharacter() == 'r' && ! mods.isCtrlDown())
    {
        transportBar.setRecording (! transportBar.isRecording());
        updateStatusLabel();
        return true;
    }

    if (key.getTextCharacter() == '1') { patternTabs.setCurrentTabIndex (kTabStepSeq); focusActiveEditorTab(); return true; }
    if (key.getTextCharacter() == '2') { patternTabs.setCurrentTabIndex (kTabPianoRoll); focusActiveEditorTab(); return true; }
    if (key.getTextCharacter() == '3') { patternTabs.setCurrentTabIndex (kTabArrangement); focusActiveEditorTab(); return true; }
    if (key.getTextCharacter() == '4') { patternTabs.setCurrentTabIndex (kTabFxRack); focusActiveEditorTab(); return true; }
    if (key.getTextCharacter() == '5') { patternTabs.setCurrentTabIndex (kTabAssistant); focusActiveEditorTab(); return true; }

    return false;
}

bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (sampr::SampleImportService::canImportPath ({ path }))
            return true;

    return false;
}

void MainComponent::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (files, x, y);
    dragSamplesActive = true;
    repaint();
}

void MainComponent::fileDragExit (const juce::StringArray& files)
{
    juce::ignoreUnused (files);
    dragSamplesActive = false;
    repaint();
}

void MainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);
    dragSamplesActive = false;
    importSampleFiles (files);
    repaint();
}

void MainComponent::togglePlayPause()
{
    if (transportPlaying)
    {
        transportPlaying = false;
        pushBoolCommand (sampr::AudioCommandType::transportPause, false);
        transportBar.setPlaying (false);
    }
    else
    {
        transportPlaying = true;
        pushBoolCommand (sampr::AudioCommandType::transportPlay, true);
        transportBar.setPlaying (true);
    }

    updateStatusLabel();
}

void MainComponent::stopTransport()
{
    transportPlaying = false;
    pushBoolCommand (sampr::AudioCommandType::transportStop, false);
    transportBar.setPlaying (false);
    lastSongPlaybackPatternIndex = -1;
    updateStatusLabel();
}

void MainComponent::updateToolbarState()
{
    const auto canEdit = ! exportInProgress;

    saveProjectButton.setEnabled (canEdit);
    loadProjectButton.setEnabled (canEdit);
    saveBeatButton.setEnabled (canEdit);
    loadBeatButton.setEnabled (canEdit);
    askPatternButton.setEnabled (canEdit);
    songModeButton.setEnabled (canEdit);
    youtubeImportButton.setEnabled (canEdit && ! youtubeImportInProgress);
    sampleBrowser.setEnabled (canEdit);
    sliceEditorButton.setEnabled (canEdit);
    patternTabs.setEnabled (canEdit);
    transportBar.setEnabled (canEdit);

    undoButton.setEnabled (canEdit && projectController.canUndo());
    redoButton.setEnabled (canEdit && projectController.canRedo());
}

void MainComponent::handleIncomingMidiMessage (juce::MidiInput* source,
                                               const juce::MidiMessage& message)
{
    juce::ignoreUnused (source);

    if (! message.isNoteOn())
        return;

    const auto note = message.getNoteNumber();
    const auto velocity = message.getFloatVelocity();
    const auto& pattern = patternStore.getCurrentPattern();

    if (transportBar.isRecording() && transportPlaying)
    {
        if (patternTabs.getCurrentTabIndex() == kTabPianoRoll)
        {
            recordPianoNote (note, velocity);
            return;
        }

        const auto step = audioEngine.getCurrentSequencerStep();

        if (step >= 0)
        {
            const auto row = note - 36;

            if (juce::isPositiveAndBelow (row, static_cast<int> (pattern.rows.size())))
            {
                patternStore.activateStep (row, step, velocity);
                stepSequencer.refresh();
                publishPatternSnapshot();
            }
        }

        return;
    }

    const auto row = note - 36;

    if (juce::isPositiveAndBelow (row, static_cast<int> (pattern.rows.size())))
        triggerPatternRow (row, velocity);
}

void MainComponent::toggleFullscreen()
{
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        window->setFullScreen (! window->isFullScreen());
        topNavBar.setFullscreenState (window->isFullScreen());
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d1118), bounds.getX(), bounds.getY(),
                                             juce::Colour (0xff050608), bounds.getX(), bounds.getBottom(),
                                             false));
    g.fillRect (bounds);
    g.setGradientFill (juce::ColourGradient (sampr::SamprLookAndFeel::accent().withAlpha (0.10f), bounds.getX(), bounds.getY(),
                                             sampr::SamprLookAndFeel::accentCool().withAlpha (0.07f), bounds.getRight(), bounds.getY(),
                                             false));
    g.fillRect (bounds.withHeight (72.0f));

    if (dragSamplesActive)
    {
        auto dropBounds = getLocalBounds().reduced (2);
        g.setColour (sampr::SamprLookAndFeel::accent().withAlpha (0.16f));
        g.fillRect (dropBounds);
        g.setColour (sampr::SamprLookAndFeel::accent());
        g.drawRect (dropBounds, 1);
        g.setFont (juce::FontOptions { 14.0f, juce::Font::bold });
        g.drawText ("Drop audio files to import samples",
                    dropBounds,
                    juce::Justification::centred);
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (4);
    topNavBar.setBounds (area.removeFromTop (38));

    auto projectRow = area.removeFromTop (32);
    saveProjectButton.setBounds (projectRow.removeFromLeft (60));
    projectRow.removeFromLeft (4);
    loadProjectButton.setBounds (projectRow.removeFromLeft (60));
    projectRow.removeFromLeft (4);
    saveBeatButton.setBounds (projectRow.removeFromLeft (76));
    projectRow.removeFromLeft (4);
    loadBeatButton.setBounds (projectRow.removeFromLeft (76));
    projectRow.removeFromLeft (4);
    askPatternButton.setBounds (projectRow.removeFromLeft (88));
    projectRow.removeFromLeft (4);
    undoButton.setBounds (projectRow.removeFromLeft (60));
    projectRow.removeFromLeft (4);
    redoButton.setBounds (projectRow.removeFromLeft (60));
    projectRow.removeFromLeft (8);
    songModeButton.setBounds (projectRow.removeFromLeft (104));
    projectRow.removeFromLeft (6);
    youtubeImportButton.setBounds (projectRow.removeFromLeft (82));
    area.removeFromTop (4);

    statusLabel.setBounds (area.removeFromBottom (46));
    area.removeFromBottom (4);
    transportBar.setBounds (area.removeFromBottom (38));
    area.removeFromBottom (4);
    const auto mixerHeight = juce::jlimit (150, 244, area.getHeight() / 4);
    mixerPanel.setBounds (area.removeFromBottom (mixerHeight));
    area.removeFromBottom (4);

    sampleBrowser.setBounds (area.removeFromLeft (220));
    area.removeFromLeft (4);
    centralEditor.setBounds (area);

    auto editor = centralEditor.getLocalBounds();
    const auto chopHeight = juce::jlimit (96, 150, editor.getHeight() / 4);
    auto chopBounds = editor.removeFromTop (chopHeight);
    editor.removeFromTop (4);
    patternTabs.setBounds (editor);

    chopArea.setBounds (chopBounds);
    auto chop = chopArea.getLocalBounds();
    auto toolbar = chop.removeFromTop (24);
    sliceEditorButton.setBounds (toolbar.removeFromRight (96).reduced (2));
    toolbar.removeFromRight (6);
    chopHintLabel.setBounds (toolbar.reduced (2, 0));
    waveformView.setBounds (chop);
}

void MainComponent::refreshProjectViews()
{
    stepSequencer.refresh();
    pianoRoll.refresh();
    arrangementTimeline.refresh();
    mixerPanel.refresh();
    fxWorkspace.refreshFromStore();
    refreshSampleViews();
    publishPatternSnapshot();
    updateStatusLabel();
}

void MainComponent::publishPatternSnapshot()
{
    const auto bpm = transportBar.getBpm();

    if (deviceSampleRate <= 0.0)
        return;

    if (projectController.getPlaybackMode() == sampr::PlaybackMode::song)
    {
        const auto beats = audioEngine.getPlayheadBeats (bpm);
        arrangementTimeline.setSongPlayheadBar (beats / 4.0);
    }

    sampr::MvpFlow::publishCurrentPattern (patternStore,
                                           audioEngine,
                                           projectController,
                                           bpm,
                                           deviceSampleRate,
                                           projectController.getPlaybackMode(),
                                           patternStore.getCurrentPatternIndex());
}

void MainComponent::refreshSampleViews()
{
    sampleBrowser.refresh();

    const auto assetId = sampleManager.getSelectedAssetId();
    const auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
    {
        waveformView.clear();
        return;
    }

    waveformView.setPeaks (asset->peaks);
    waveformView.setSliceMarkers (asset->slices, asset->selectedSliceIndex, asset->numSamples);
}

void MainComponent::importSampleFiles (const juce::StringArray& paths)
{
    const auto result = sampr::SampleImportService::importFiles (
        paths, projectModel, sampleManager, formatManager);

    if (result.loadedCount > 0)
    {
        patternStore.syncRowsFromLoadedAssets();
        refreshProjectViews();
        sliceEditor.syncFromSelectedSlice();
        showUserMessage ("Imported " + juce::String (result.loadedCount) + " sample(s).");
    }

    if (result.failedCount > 0)
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Import failed",
            result.lastError + "\n(" + juce::String (result.failedCount) + " file(s) failed)");
    }
}

void MainComponent::startYouTubeImport (const juce::String& url, const juce::String& format)
{
    if (youtubeImportInProgress || youtubeImportJob != nullptr)
    {
        showUserMessage ("YouTube import already running.");
        return;
    }

    if (! url.startsWithIgnoreCase ("http"))
    {
        showUserMessage ("Paste a valid YouTube URL first.");
        return;
    }

    youtubeImportInProgress = true;
    updateToolbarState();
    showUserMessage ("YouTube import started.");

    if (youtubeImportDialog != nullptr)
        youtubeImportDialog->exitModalState (0);

    youtubeImportJob = std::make_unique<AsyncYouTubeImportJob> (*this, url, format);
    youtubeImportJob->start();
}

void MainComponent::finishYouTubeImport (const juce::File& audioFile, const juce::String& errorMessage)
{
    youtubeImportJob.reset();
    youtubeImportInProgress = false;
    updateToolbarState();

    if (errorMessage.isNotEmpty())
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "YouTube import failed",
            errorMessage);
        showUserMessage ("YouTube import failed.");
        return;
    }

    if (! audioFile.existsAsFile())
    {
        showUserMessage ("YouTube import finished but no audio file was found.");
        return;
    }

    juce::StringArray paths;
    paths.add (audioFile.getFullPathName());
    importSampleFiles (paths);
    showUserMessage ("Imported YouTube sample with metadata.");
}

void MainComponent::loadSampleFromDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select audio files",
        juce::File {},
        sampr::SampleImportService::getSupportedFileChooserPatterns());

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        juce::StringArray paths;

        for (const auto& file : chooser.getResults())
            paths.add (file.getFullPathName());

        importSampleFiles (paths);
    });
}

void MainComponent::drainAudioDiagnostics()
{
    sampr::AudioLogCode codes[sampr::AudioDiagnostics::kCapacity];
    const auto count = audioEngine.getDiagnostics().drainCodes (codes, sampr::AudioDiagnostics::kCapacity);

    if (count == 0 && audioEngine.getDroppedCommandCount() == 0)
        return;

    juce::String warning;

    if (const auto dropped = audioEngine.getDroppedCommandCount(); dropped > 0)
        warning << "Dropped " << dropped << " audio command(s). ";

    for (int i = 0; i < count; ++i)
    {
        switch (codes[i])
        {
            case sampr::AudioLogCode::commandQueueFull: warning << "Command queue full. "; break;
            case sampr::AudioLogCode::sampleNotFound:   warning << "Sample not found. "; break;
            case sampr::AudioLogCode::voicePoolExhausted: warning << "Voice pool exhausted. "; break;
            default: break;
        }
    }

    if (warning.isNotEmpty())
    {
        audioWarningText = warning.trim();
        audioWarningTicks = 300;
        DBG (warning);
    }
}

void MainComponent::triggerPatternRow (int rowIndex, float velocity)
{
    const auto& pattern = patternStore.getCurrentPattern();

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (pattern.rows.size())))
        return;

    const auto& row = pattern.rows[static_cast<size_t> (rowIndex)];
    const auto sampleId = sampleManager.resolvePlaybackSampleId (row.assetId, row.sliceIndex);

    if (! sampleId.has_value())
        return;

    sampr::AudioCommand command;
    command.type = sampr::AudioCommandType::triggerVoice;
    command.sampleId = *sampleId;
    command.channelIndex = rowIndex;
    command.velocity = velocity;
    command.gain = row.channelGain * projectModel.getSettings().masterGain;
    command.pan = row.channelPan;
    command.pitchSemitones = sampleManager.resolvePlaybackPitchSemitones (row.assetId, row.sliceIndex);
    command.timeRatio = sampleManager.resolvePlaybackTimeRatio (row.assetId, row.sliceIndex);
    command.playbackMode = sampleManager.resolveVoicePlaybackMode (row.assetId, row.sliceIndex);
    command.loop = false;
    audioEngine.pushCommand (command);
}

void MainComponent::triggerSelectedSlice()
{
    const auto assetId = sampleManager.getSelectedAssetId();
    const auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
        return;

    const auto sliceIndex = asset->selectedSliceIndex;
    const auto sampleId = sampleManager.resolvePlaybackSampleId (assetId, sliceIndex);

    if (! sampleId.has_value())
    {
        if (const auto* slice = sampleManager.getSelectedSlice();
            slice != nullptr && sampr::sliceNeedsRubberBandBake (*slice)
            && slice->processingMode == sampr::StretchProcessingMode::preRenderOffline)
        {
            sampleManager.requestSelectedSliceBake();
            showUserMessage ("Baking stretch for this slice — try Trigger again when complete.");
        }
        else
        {
            showUserMessage ("Slice not ready — check processing mode or open Edit Slices.");
        }

        return;
    }

    sampr::AudioCommand command;
    command.type = sampr::AudioCommandType::triggerVoice;
    command.sampleId = *sampleId;
    command.velocity = 1.0f;
    command.gain = projectModel.getSettings().masterGain;
    command.pan = 0.0f;
    command.pitchSemitones = sampleManager.resolvePlaybackPitchSemitones (assetId, sliceIndex);
    command.timeRatio = sampleManager.resolvePlaybackTimeRatio (assetId, sliceIndex);
    command.playbackMode = sampleManager.resolveVoicePlaybackMode (assetId, sliceIndex);
    command.loop = sampleManager.resolvePlaybackLoop (assetId, sliceIndex);
    audioEngine.pushCommand (command);
}

void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (
        deviceManager, 0, 2, 0, 2, true, true, true, false);
    selector->setSize (500, 450);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle                  = "Audio Settings";
    options.dialogBackgroundColour       = juce::Colour (0xff1a1d24);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = true;
    options.content.setOwned (selector);
    options.launchAsync();
}

void MainComponent::showYouTubeImportPopup()
{
    if (youtubeImportDialog != nullptr)
    {
        youtubeImportDialog->toFront (true);
        return;
    }

    struct YouTubeDialogContent final : public juce::Component
    {
        YouTubeDialogContent (std::function<void (const juce::String&, const juce::String&)> importCallbackIn)
            : importCallback (std::move (importCallbackIn))
        {
            titleLabel.setText ("YouTube Import", juce::dontSendNotification);
            titleLabel.setFont (juce::FontOptions { 15.0f, juce::Font::bold });

            searchLabel.setText ("Search", juce::dontSendNotification);
            urlLabel.setText ("URL", juce::dontSendNotification);
            formatLabel.setText ("Format", juce::dontSendNotification);

            searchEditor.setTextToShowWhenEmpty ("artist, song, sound, drum break", sampr::SamprLookAndFeel::textMuted());
            urlEditor.setTextToShowWhenEmpty ("Paste YouTube URL after choosing a source", sampr::SamprLookAndFeel::textMuted());

            formatBox.addItem ("MP3", 1);
            formatBox.addItem ("WAV", 2);
            formatBox.addItem ("FLAC", 3);
            formatBox.addItem ("M4A", 4);
            formatBox.setSelectedId (1, juce::dontSendNotification);

            rightsButton.setButtonText ("I have rights to use this source");
            rightsButton.onClick = [this] { updateImportState(); };
            urlEditor.onTextChange = [this] { updateImportState(); };

            searchButton.onClick = [this]
            {
                const auto query = searchEditor.getText().trim();
                if (query.isEmpty())
                    return;

                const auto escaped = juce::URL::addEscapeChars (query, true);
                juce::URL ("https://www.youtube.com/results?search_query=" + escaped).launchInDefaultBrowser();
            };

            importButton.onClick = [this]
            {
                if (importCallback == nullptr)
                    return;

                importCallback (urlEditor.getText().trim(), getFormat());
            };

            addAndMakeVisible (titleLabel);
            addAndMakeVisible (searchLabel);
            addAndMakeVisible (searchEditor);
            addAndMakeVisible (searchButton);
            addAndMakeVisible (urlLabel);
            addAndMakeVisible (urlEditor);
            addAndMakeVisible (formatLabel);
            addAndMakeVisible (formatBox);
            addAndMakeVisible (rightsButton);
            addAndMakeVisible (importButton);

            updateImportState();
            setSize (560, 220);
        }

        juce::String getFormat() const
        {
            switch (formatBox.getSelectedId())
            {
                case 2: return "wav";
                case 3: return "flac";
                case 4: return "m4a";
                default: return "mp3";
            }
        }

        void updateImportState()
        {
            importButton.setEnabled (rightsButton.getToggleState()
                                     && urlEditor.getText().trim().startsWithIgnoreCase ("http"));
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12);
            titleLabel.setBounds (area.removeFromTop (24));
            area.removeFromTop (8);

            auto row = area.removeFromTop (28);
            searchLabel.setBounds (row.removeFromLeft (52));
            searchEditor.setBounds (row.removeFromLeft (row.getWidth() - 96));
            row.removeFromLeft (8);
            searchButton.setBounds (row);
            area.removeFromTop (8);

            row = area.removeFromTop (28);
            urlLabel.setBounds (row.removeFromLeft (52));
            urlEditor.setBounds (row);
            area.removeFromTop (8);

            row = area.removeFromTop (28);
            formatLabel.setBounds (row.removeFromLeft (52));
            formatBox.setBounds (row.removeFromLeft (110));
            row.removeFromLeft (12);
            rightsButton.setBounds (row);

            importButton.setBounds (area.removeFromBottom (30).removeFromRight (132));
        }

        juce::Label titleLabel;
        juce::Label searchLabel;
        juce::Label urlLabel;
        juce::Label formatLabel;
        juce::TextEditor searchEditor;
        juce::TextEditor urlEditor;
        juce::TextButton searchButton { "Open Search" };
        juce::ComboBox formatBox;
        juce::ToggleButton rightsButton;
        juce::TextButton importButton { "Convert + Import" };
        std::function<void (const juce::String&, const juce::String&)> importCallback;
    };

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle                  = "YouTube Import";
    options.dialogBackgroundColour       = sampr::SamprLookAndFeel::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.componentToCentreAround      = this;
    options.content.setOwned (new YouTubeDialogContent ([this] (const juce::String& url, const juce::String& format)
    {
        startYouTubeImport (url, format);
    }));
    youtubeImportDialog = options.launchAsync();
}

void MainComponent::showSliceEditorPopup()
{
    if (sliceEditorDialog != nullptr)
    {
        sliceEditorDialog->toFront (true);
        return;
    }

    struct SliceDialogContent final : public juce::Component
    {
        SliceDialogContent (sampr::SliceEditorPanel& panel, std::function<void()> onAskGemma)
            : slicePanel (panel),
              askCallback (std::move (onAskGemma))
        {
            if (slicePanel.getParentComponent() != nullptr)
                slicePanel.getParentComponent()->removeChildComponent (&slicePanel);

            askButton.setTooltip ("Ask Gemma about this slice's pitch, timing, and boundaries");
            askButton.onClick = [this]
            {
                if (askCallback != nullptr)
                    askCallback();
            };

            addAndMakeVisible (slicePanel);
            addAndMakeVisible (askButton);
            setSize (420, 396);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            askButton.setBounds (area.removeFromBottom (28));
            area.removeFromBottom (6);
            slicePanel.setBounds (area);
        }

        sampr::SliceEditorPanel& slicePanel;
        juce::TextButton askButton { "Ask Gemma" };
        std::function<void()> askCallback;
    };

    sliceEditor.syncFromSelectedSlice();

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle                  = "Slice Editor";
    options.dialogBackgroundColour       = sampr::SamprLookAndFeel::background();
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = true;
    options.componentToCentreAround      = this;
    options.content.setOwned (new SliceDialogContent (sliceEditor, [this]
    {
        if (sliceEditorDialog != nullptr)
            sliceEditorDialog->exitModalState (0);

        openAssistantWithContext (sampr::ContextScope::slice,
                                  fxWorkspace.getChannelIndex(),
                                  "Does this slice need pitch or time adjustments for a tighter mix?");
        assistantPanel.setScope (sampr::ContextScope::slice);
    }));
    sliceEditorDialog = options.launchAsync();
}

void MainComponent::openAssistantWithContext (sampr::ContextScope scope,
                                              int channelIndex,
                                              const juce::String& seedMessage)
{
    patternTabs.setCurrentTabIndex (kTabAssistant);
    assistantPanel.openWithPrompt (scope, channelIndex, seedMessage);
    focusActiveEditorTab();
}

void MainComponent::saveBeatToDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save beat preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.anna-beat.json;*.sampr-beat.json");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("anna-beat.json"))
            file = file.withFileExtension (".anna-beat.json");

        juce::String error;
        if (! sampr::BeatSerializer::saveToFile (patternStore.exportCurrentPattern(), file, error))
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Save beat failed", error);
            return;
        }

        showUserMessage ("Beat saved to " + file.getFileName());
    });
}

void MainComponent::loadBeatFromDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load beat preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.anna-beat.json;*.sampr-beat.json");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        const auto result = sampr::BeatSerializer::loadFromFile (file);

        if (! result.success)
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Load beat failed", result.errorMessage);
            return;
        }

        const auto& currentPattern = patternStore.getCurrentPattern();
        const auto beatRows = static_cast<int> (result.pattern.rows.size());
        const auto patternRows = static_cast<int> (currentPattern.rows.size());

        if (beatRows == patternRows)
        {
            applyLoadedBeat (result.pattern);
            return;
        }

        const auto matchedRows = juce::jmin (beatRows, patternRows);
        juce::String message = "Beat \"" + result.pattern.name + "\" has " + juce::String (beatRows)
                               + " channel(s). Your pattern has " + juce::String (patternRows) + ".\n\n"
                               + "Merge will apply steps, notes, and FX to the first "
                               + juce::String (matchedRows) + " matching channel(s) and keep your current samples.";

        if (beatRows > patternRows)
            message << "\n\nChannels " + juce::String (patternRows + 1) + "-" + juce::String (beatRows)
                    + " from the beat will not be loaded.";

        if (patternRows > beatRows)
            message << "\n\nChannels " + juce::String (beatRows + 1) + "-" + juce::String (patternRows)
                    + " will keep their existing content.";

        message << "\n\nContinue?";

        juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                 "Load Beat",
                                                 message,
                                                 this,
                                                 juce::ModalCallbackFunction::create ([this, pattern = result.pattern] (int response)
                                                 {
                                                     if (response == 1)
                                                         applyLoadedBeat (pattern);
                                                 }));
    });
}

void MainComponent::applyLoadedBeat (const sampr::Pattern& pattern)
{
    projectController.beginEdit ("Load beat");
    patternStore.importPatternIntoCurrent (pattern, false);
    refreshProjectViews();

    showUserMessage ("Loaded beat \"" + pattern.name + "\" into the current pattern.");
}

void MainComponent::pushBoolCommand (sampr::AudioCommandType type, bool value)
{
    sampr::AudioCommand command;
    command.type = type;
    command.boolValue = value;
    audioEngine.pushCommand (command);
}

void MainComponent::pushFloatCommand (sampr::AudioCommandType type, float value)
{
    sampr::AudioCommand command;
    command.type = type;
    command.floatValue = value;
    audioEngine.pushCommand (command);
}

void MainComponent::showUserMessage (const juce::String& message)
{
    userMessageText = message;
    userMessageTicks = 360;
    updateStatusLabel();
}

bool MainComponent::ensureArrangementHasPlayableClip()
{
    if (! projectModel.getArrangement().empty())
        return true;

    const auto patternIndex = patternStore.getCurrentPatternIndex();

    if (! juce::isPositiveAndBelow (patternIndex, patternStore.getPatternCount()))
        return false;

    const auto& pattern = patternStore.getPattern (patternIndex);
    const auto clipId = projectController.addArrangementClip (pattern.id, 0.0, juce::jmax (1, pattern.lengthBars));

    if (clipId == sampr::kInvalidClipId)
        return false;

    arrangementTimeline.refresh();
    showUserMessage ("Added current pattern to Arrangement.");
    return true;
}

void MainComponent::focusActiveEditorTab()
{
    if (auto* editor = patternTabs.getCurrentContentComponent())
        editor->grabKeyboardFocus();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &patternTabs.getTabbedButtonBar())
        focusActiveEditorTab();
}

void MainComponent::updateStatusLabel()
{
    juce::String text;
    const auto& pattern = patternStore.getCurrentPattern();
    const auto songMode = projectController.getPlaybackMode() == sampr::PlaybackMode::song;
    const auto modeText = songMode ? "Song" : "Pattern";
    const auto playState = transportPlaying ? "Playing" : "Stopped";

    text << modeText << "  |  " << playState
         << "  |  Pattern: " << pattern.name
         << "  |  Bar " << juce::String (audioEngine.getPlayheadBeats (transportBar.getBpm()) / 4.0 + 1.0, 2);

    if (transportBar.isRecording())
        text << (transportPlaying ? "  |  Recording" : "  |  Record armed");

    text << "\n";

    if (const auto* asset = sampleManager.getAsset (sampleManager.getSelectedAssetId()))
        text << "Sample: " << asset->displayName;
    else
        text << "No sample selected — load or drop audio to begin.";

    if (userMessageTicks > 0 && userMessageText.isNotEmpty())
        text << "  |  " << userMessageText;

    text << "\n";

    if (audioWarningTicks > 0 && audioWarningText.isNotEmpty())
        text << "Warning: " << audioWarningText;
    else if (auto* device = deviceManager.getCurrentAudioDevice())
        text << device->getName() << "  " << juce::String (device->getCurrentSampleRate(), 0) << " Hz";

    text << "  |  Gemma: "
         << (assistantClient.isOnline() ? "online" : "offline");

    if (assistantPanel.isWaitingForResponse())
        text << " (thinking)";

    if (exportInProgress)
        text << "  |  Export: " << juce::String (static_cast<int> (exportProgress.load (std::memory_order_relaxed) * 100.0f)) << "%";

    if (youtubeImportInProgress)
        text << "  |  YouTube import running";

    statusLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::recordPianoNote (int pitch, float velocity)
{
    const auto assetId = sampleManager.getSelectedAssetId();
    const auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
        return;

    const auto loopBeats = static_cast<double> (patternStore.getCurrentPattern().lengthBars) * 4.0;
    const auto playhead = std::fmod (audioEngine.getPlayheadBeats (transportBar.getBpm()), loopBeats);

    sampr::NoteEvent note;
    note.startBeats = playhead;
    note.durationBeats = 0.25;
    note.pitch = pitch;
    note.velocity = velocity;
    note.assetId = assetId;
    note.sampleRefId = asset->refId;
    note.sliceIndex = asset->selectedSliceIndex;

    patternStore.addNote (note);
    pianoRoll.refresh();
    publishPatternSnapshot();
}

void MainComponent::saveProjectToDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save ANNA project",
        projectController.getProjectFile().existsAsFile()
            ? projectController.getProjectFile()
            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.anna.json;*.sampr.json");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("anna.json"))
            file = file.withFileExtension (".anna.json");

        auto& settings = projectModel.getSettings();
        settings.projectName = file.getFileNameWithoutExtension();
        settings.bpm = transportBar.getBpm();
        settings.playbackMode = songModeButton.getToggleState()
                                    ? sampr::PlaybackMode::song
                                    : sampr::PlaybackMode::pattern;
        applyTransportSettings();

        juce::String error;
        if (! projectController.saveProject (file, error))
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Save failed", error);
            return;
        }

        updateStatusLabel();
    });
}

void MainComponent::loadProjectFromDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load ANNA project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.anna.json;*.sampr.json");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        juce::String error;
        if (! projectController.loadProject (file, formatManager, error))
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon, "Load failed", error);
            return;
        }

        const auto& settings = projectModel.getSettings();
        transportBar.setBpm (settings.bpm);
        mixerPanel.setMasterGain (settings.masterGain);
        songModeButton.setToggleState (settings.playbackMode == sampr::PlaybackMode::song,
                                       juce::dontSendNotification);
        applyTransportSettings();
        lastSongPlaybackPatternIndex = -1;
        refreshProjectViews();
        sliceEditor.syncFromSelectedSlice();
    });
}

void MainComponent::exportAudioToDisk()
{
    if (exportInProgress)
    {
        showUserMessage ("Export already in progress.");
        return;
    }

    fileChooser = std::make_unique<juce::FileChooser> (
        "Export WAV",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.wav");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("wav"))
            file = file.withFileExtension (".wav");

        sampr::OfflineExportOptions options;
        options.sampleRate = deviceSampleRate > 0.0 ? deviceSampleRate : 44100.0;
        options.renderArrangement = projectController.getPlaybackMode() == sampr::PlaybackMode::song;
        options.playbackMode = projectController.getPlaybackMode();

        const auto wasPlaying = transportPlaying;
        pushBoolCommand (sampr::AudioCommandType::transportStop, false);
        transportPlaying = false;
        transportBar.setPlaying (false);

        exportProgress.store (0.0f, std::memory_order_relaxed);
        exportInProgress = true;
        updateToolbarState();
        showUserMessage ("Export started.");

        exportJob = std::make_unique<AsyncExportJob> (*this, file, options, wasPlaying);
        exportJob->start();
    });
}

void MainComponent::exportSoundCloudReady()
{
    if (exportInProgress)
    {
        showUserMessage ("Export already in progress.");
        return;
    }

    const auto exportFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                  .getChildFile ("ANNA SoundCloud Uploads");

    if (! exportFolder.exists() && ! exportFolder.createDirectory())
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "SoundCloud export failed",
            "Could not create upload folder:\n" + exportFolder.getFullPathName());
        return;
    }

    auto file = getUniqueChildFile (exportFolder,
                                    makeSafeFileStem (projectModel.getSettings().projectName) + " - SoundCloud",
                                    ".wav");

    sampr::OfflineExportOptions options;
    options.sampleRate = 44100.0;
    options.bitDepth = 16;
    options.outputGainDb = -1.0f;
    options.renderArrangement = projectController.getPlaybackMode() == sampr::PlaybackMode::song;
    options.playbackMode = projectController.getPlaybackMode();

    const auto wasPlaying = transportPlaying;
    pushBoolCommand (sampr::AudioCommandType::transportStop, false);
    transportPlaying = false;
    transportBar.setPlaying (false);

    exportProgress.store (0.0f, std::memory_order_relaxed);
    exportInProgress = true;
    updateToolbarState();
    showUserMessage ("SoundCloud export started.");

    exportJob = std::make_unique<AsyncExportJob> (*this, file, options, wasPlaying);
    exportJob->start();
}

void MainComponent::finishAsyncExport (const juce::File& file,
                                       const sampr::OfflineExportResult& result,
                                       bool wasPlaying)
{
    exportJob.reset();
    exportInProgress = false;
    exportProgress.store (1.0f, std::memory_order_relaxed);

    publishPatternSnapshot();

    if (wasPlaying)
    {
        transportPlaying = true;
        pushBoolCommand (sampr::AudioCommandType::transportPlay, true);
        transportBar.setPlaying (true);
    }

    updateToolbarState();
    updateStatusLabel();

    if (! result.success)
    {
        juce::NativeMessageBox::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon, "Export failed", result.errorMessage);
        return;
    }

    showUserMessage ("Export complete: " + file.getFileName());

    const auto isSoundCloudExport = file.getParentDirectory().getFileName() == "ANNA SoundCloud Uploads";
    if (isSoundCloudExport)
    {
        file.getSiblingFile (file.getFileNameWithoutExtension() + " sampled-from.txt")
            .replaceWithText (buildSampledFromText (projectModel));
        file.revealToUser();
    }

    juce::NativeMessageBox::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        isSoundCloudExport ? "SoundCloud upload file ready" : "Export complete",
        isSoundCloudExport
            ? "Your SoundCloud-ready WAV is in the upload folder.\n\nFormat: 16-bit / 44.1 kHz WAV with -1 dB headroom.\n\n"
              + file.getFullPathName()
            : "Wrote " + juce::String (result.samplesWritten) + " samples to\n" + file.getFullPathName());
}
