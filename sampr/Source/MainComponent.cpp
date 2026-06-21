#include "MainComponent.h"

#include "App/SampleImportService.h"
#include "Model/NoteEvent.h"

#include <cmath>

MainComponent::MainComponent()
    : sampleManager (audioEngine),
      patternStore (projectModel, sampleManager),
      projectController (projectModel, sampleManager, patternStore),
      sampleBrowser (sampleManager),
      sliceEditor (sampleManager),
      stepSequencer (patternStore),
      pianoRoll (patternStore, sampleManager),
      arrangementTimeline (projectController, patternStore)
{
    formatManager.registerBasicFormats();
    setAudioChannels (2, 2);

    for (const auto& device : juce::MidiInput::getAvailableDevices())
        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);

    deviceManager.addMidiInputDeviceCallback ({}, this);

    sampleManager.setChangeCallback ([this]
    {
        refreshSampleViews();
        publishPatternSnapshot();
    });

    patternStore.setUndoCheckpoint ([this]
    {
        projectController.beginEdit ("Pattern edit");
    });

    patternStore.setChangeCallback ([this]
    {
        stepSequencer.refresh();
        pianoRoll.refresh();
        arrangementTimeline.refresh();
        publishPatternSnapshot();
    });

    projectController.setChangeCallback ([this]
    {
        refreshProjectViews();
    });

    arrangementTimeline.setChangeCallback ([this]
    {
        publishPatternSnapshot();
        updateStatusLabel();
    });

    stepSequencer.setChangeCallback ([this]
    {
        publishPatternSnapshot();
    });

    pianoRoll.setChangeCallback ([this]
    {
        publishPatternSnapshot();
    });

    patternTabs.addTab ("Step Seq", juce::Colour (0xff1a2030), &stepSequencer, false);
    patternTabs.addTab ("Piano Roll", juce::Colour (0xff1a2030), &pianoRoll, false);
    patternTabs.addTab ("Arrangement", juce::Colour (0xff1a2030), &arrangementTimeline, false);
    patternTabs.setCurrentTabIndex (0);

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

    sliceEditor.setTriggerCallback ([this] { triggerSelectedSlice(); });
    sliceEditor.setParameterChangedCallback ([this] { refreshSampleViews(); });

    playPauseButton.onClick = [this] { toggleTransport(); };
    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    saveProjectButton.onClick = [this] { saveProjectToDisk(); };
    loadProjectButton.onClick = [this] { loadProjectFromDisk(); };
    undoButton.onClick = [this]
    {
        if (projectController.undo())
            refreshProjectViews();
    };
    redoButton.onClick = [this]
    {
        if (projectController.redo())
            refreshProjectViews();
    };
    songModeButton.onClick = [this]
    {
        projectController.setPlaybackMode (songModeButton.getToggleState()
                                             ? sampr::PlaybackMode::song
                                             : sampr::PlaybackMode::pattern);
        lastSongPlaybackPatternIndex = -1;
        publishPatternSnapshot();
        updateStatusLabel();
    };

    auto configureSlider = [] (juce::Slider& slider, double min, double max, double value, int decimals)
    {
        slider.setRange (min, max, 0.01);
        slider.setValue (value, juce::dontSendNotification);
        slider.setNumDecimalPlacesToDisplay (decimals);
        slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 64, 22);
    };

    configureSlider (bpmSlider, 60.0, 200.0, projectModel.getSettings().bpm, 0);
    configureSlider (gainSlider, 0.0, 1.5, projectModel.getSettings().masterGain, 2);
    configureSlider (panSlider, -1.0, 1.0, 0.0, 2);

    bpmSlider.onValueChange = [this]
    {
        projectModel.getSettings().bpm = bpmSlider.getValue();
        pushFloatCommand (sampr::AudioCommandType::setBpm, static_cast<float> (bpmSlider.getValue()));
        publishPatternSnapshot();
        updateStatusLabel();
    };

    gainSlider.onValueChange = [this]
    {
        projectModel.getSettings().masterGain = static_cast<float> (gainSlider.getValue());
        pushFloatCommand (sampr::AudioCommandType::setMasterGain, static_cast<float> (gainSlider.getValue()));
    };

    bpmLabel.setText ("BPM", juce::dontSendNotification);
    gainLabel.setText ("Gain", juce::dontSendNotification);
    panLabel.setText ("Pan", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setFont (juce::FontOptions { 12.0f });

    songModeButton.setToggleState (projectModel.getSettings().playbackMode == sampr::PlaybackMode::song,
                                   juce::dontSendNotification);

    for (auto* child : { &sampleBrowser, &waveformView, &sliceEditor, &patternTabs,
                         &playPauseButton, &audioSettingsButton, &saveProjectButton, &loadProjectButton,
                         &undoButton, &redoButton, &midiRecordButton, &songModeButton,
                         &bpmSlider, &gainSlider, &panSlider,
                         &bpmLabel, &gainLabel, &panLabel, &statusLabel })
        addAndMakeVisible (child);

    pushFloatCommand (sampr::AudioCommandType::setBpm, static_cast<float> (bpmSlider.getValue()));
    pushFloatCommand (sampr::AudioCommandType::setMasterGain, static_cast<float> (gainSlider.getValue()));
    pushBoolCommand (sampr::AudioCommandType::setTestToneEnabled, false);
    audioEngine.setSequencerEnabled (true);
    audioEngine.setNoteSequencerEnabled (true);

    setSize (1024, 900);
    publishPatternSnapshot();
    updateStatusLabel();
    startTimerHz (60);
}

MainComponent::~MainComponent()
{
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

void MainComponent::timerCallback()
{
    const auto enginePlaying = audioEngine.isTransportPlaying();

    if (enginePlaying != transportPlaying)
    {
        transportPlaying = enginePlaying;
        playPauseButton.setButtonText (transportPlaying ? "Pause" : "Play");
    }

    stepSequencer.setCurrentStepIndex (audioEngine.getCurrentSequencerStep());
    pianoRoll.setPlayheadBeats (audioEngine.getPlayheadBeats (bpmSlider.getValue()));
    sliceEditor.syncFromSelectedSlice();
    updateStatusLabel();
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

    if (midiRecordButton.getToggleState() && transportPlaying)
    {
        if (patternTabs.getCurrentTabIndex() == 1)
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

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12151b));
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::FontOptions { 20.0f, juce::Font::bold });
    g.drawText ("SAMPR — Sampler + Patterns + Arrangement", getLocalBounds().removeFromTop (36),
                juce::Justification::centred, true);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (40);

    auto topRow = area.removeFromTop (30);
    playPauseButton.setBounds (topRow.removeFromLeft (90));
    topRow.removeFromLeft (8);
    saveProjectButton.setBounds (topRow.removeFromLeft (64));
    topRow.removeFromLeft (6);
    loadProjectButton.setBounds (topRow.removeFromLeft (64));
    topRow.removeFromLeft (6);
    undoButton.setBounds (topRow.removeFromLeft (64));
    topRow.removeFromLeft (6);
    redoButton.setBounds (topRow.removeFromLeft (64));
    topRow.removeFromLeft (8);
    audioSettingsButton.setBounds (topRow.removeFromLeft (120));
    topRow.removeFromLeft (8);
    songModeButton.setBounds (topRow.removeFromLeft (110));
    topRow.removeFromLeft (8);
    midiRecordButton.setBounds (topRow.removeFromLeft (130));
    topRow.removeFromLeft (12);
    bpmLabel.setBounds (topRow.removeFromLeft (40));
    bpmSlider.setBounds (topRow.removeFromLeft (110));
    topRow.removeFromLeft (8);
    gainLabel.setBounds (topRow.removeFromLeft (40));
    gainSlider.setBounds (topRow.removeFromLeft (90));
    topRow.removeFromLeft (8);
    panLabel.setBounds (topRow.removeFromLeft (34));
    panSlider.setBounds (topRow);

    area.removeFromTop (8);

    const auto statusHeight = 56;
    const auto patternEditorHeight = 300;
    statusLabel.setBounds (area.removeFromBottom (statusHeight));
    area.removeFromBottom (6);
    patternTabs.setBounds (area.removeFromBottom (patternEditorHeight));
    area.removeFromBottom (8);

    auto browserWidth = 180;
    sampleBrowser.setBounds (area.removeFromLeft (browserWidth));
    area.removeFromLeft (8);

    auto editorWidth = 260;
    sliceEditor.setBounds (area.removeFromRight (editorWidth));
    area.removeFromRight (8);
    waveformView.setBounds (area);
}

void MainComponent::refreshProjectViews()
{
    stepSequencer.refresh();
    pianoRoll.refresh();
    arrangementTimeline.refresh();
    refreshSampleViews();
    publishPatternSnapshot();
    updateStatusLabel();
}

void MainComponent::publishPatternSnapshot()
{
    const auto bpm = bpmSlider.getValue();

    if (deviceSampleRate <= 0.0)
        return;

    auto patternIndex = patternStore.getCurrentPatternIndex();

    if (projectController.getPlaybackMode() == sampr::PlaybackMode::song)
    {
        const auto beats = audioEngine.getPlayheadBeats (bpm);
        const auto songBar = beats / 4.0;
        arrangementTimeline.setSongPlayheadBar (songBar);

        if (const auto activeIndex = projectController.getActivePatternIndexForSongBar (songBar))
        {
            patternIndex = *activeIndex;
            lastSongPlaybackPatternIndex = patternIndex;
            patternStore.publishSnapshotForPattern (patternIndex, audioEngine, bpm, deviceSampleRate);
            return;
        }

        lastSongPlaybackPatternIndex = -1;
        audioEngine.setPatternSnapshot (std::make_shared<sampr::PatternSnapshot>());
        audioEngine.setNoteSnapshot (std::make_shared<sampr::NoteSnapshot>());
        return;
    }

    lastSongPlaybackPatternIndex = -1;
    patternStore.publishSnapshot (audioEngine, bpm, deviceSampleRate);
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

void MainComponent::loadSampleFromDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select audio files",
        juce::File {},
        sampr::SampleImportService::getSupportedFileChooserPatterns());

    const auto flags = juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles
                       | juce::FileBrowserComponent::canSelectMultipleItems;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        for (const auto& file : chooser.getResults())
        {
            if (! file.existsAsFile())
                continue;

            const auto assetId = sampleManager.loadFromFile (file, formatManager);

            if (! assetId.has_value())
            {
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon,
                    "Load failed",
                    "Could not decode: " + file.getFullPathName());
                continue;
            }

            if (auto* asset = sampleManager.getAsset (*assetId); asset != nullptr && asset->refId.isEmpty())
                asset->refId = projectModel.allocateSampleRefId();
        }

        patternStore.syncRowsFromLoadedAssets();
        stepSequencer.refresh();
        pianoRoll.refresh();
        refreshSampleViews();
        sliceEditor.syncFromSelectedSlice();
        publishPatternSnapshot();
        updateStatusLabel();
    });
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
    command.velocity = velocity;
    command.gain = static_cast<float> (gainSlider.getValue());
    command.pan = static_cast<float> (panSlider.getValue());
    command.pitchSemitones = sampleManager.resolvePlaybackPitchSemitones (row.assetId, row.sliceIndex);
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
        }
        return;
    }

    sampr::AudioCommand command;
    command.type = sampr::AudioCommandType::triggerVoice;
    command.sampleId = *sampleId;
    command.velocity = 1.0f;
    command.gain = static_cast<float> (gainSlider.getValue());
    command.pan = static_cast<float> (panSlider.getValue());
    command.pitchSemitones = sampleManager.resolvePlaybackPitchSemitones (assetId, sliceIndex);
    command.loop = sampleManager.resolvePlaybackLoop (assetId, sliceIndex);
    audioEngine.pushCommand (command);
}

void MainComponent::toggleTransport()
{
    transportPlaying = ! transportPlaying;
    pushBoolCommand (sampr::AudioCommandType::setTransportPlaying, transportPlaying);
    playPauseButton.setButtonText (transportPlaying ? "Pause" : "Play");
    updateStatusLabel();
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

void MainComponent::updateStatusLabel()
{
    juce::String text;

    if (const auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto bufferMs = 1000.0 * static_cast<double> (device->getCurrentBufferSizeSamples())
                              / device->getCurrentSampleRate();
        text << "Audio: " << device->getName()
             << " @ " << juce::String (device->getCurrentSampleRate(), 0) << " Hz"
             << ", " << juce::String (bufferMs, 1) << " ms\n";
    }

    const auto& pattern = patternStore.getCurrentPattern();
    const auto modeText = projectController.getPlaybackMode() == sampr::PlaybackMode::song ? "Song" : "Pattern";
    text << "Mode: " << modeText
         << "  |  " << projectModel.getSettings().projectName
         << "  |  Pattern: " << pattern.name
         << "  steps:" << pattern.numSteps << "  notes:" << pattern.notes.size()
         << "  clips:" << projectModel.getArrangement().size()
         << "  beat:" << juce::String (audioEngine.getPlayheadBeats (bpmSlider.getValue()), 2)
         << "  voices:" << audioEngine.getActiveVoiceCount() << "\n";

    if (const auto* asset = sampleManager.getAsset (sampleManager.getSelectedAssetId()))
        text << "Sample editor: " << asset->displayName;

    statusLabel.setText (text, juce::dontSendNotification);
}

void MainComponent::recordPianoNote (int pitch, float velocity)
{
    const auto assetId = sampleManager.getSelectedAssetId();
    const auto* asset = sampleManager.getAsset (assetId);

    if (asset == nullptr)
        return;

    const auto loopBeats = static_cast<double> (patternStore.getCurrentPattern().lengthBars) * 4.0;
    const auto playhead = std::fmod (audioEngine.getPlayheadBeats (bpmSlider.getValue()), loopBeats);

    NoteEvent note;
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
        "Save SAMPR project",
        projectController.getProjectFile().existsAsFile()
            ? projectController.getProjectFile()
            : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.sampr.json");

    const auto flags = juce::FileBrowserComponent::saveMode
                       | juce::FileBrowserComponent::canSelectFiles
                       | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();

        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("sampr.json"))
            file = file.withFileExtension (".sampr.json");

        projectModel.getSettings().projectName = file.getFileNameWithoutExtension();
        projectModel.getSettings().bpm = bpmSlider.getValue();
        projectModel.getSettings().masterGain = static_cast<float> (gainSlider.getValue());
        projectModel.getSettings().playbackMode = songModeButton.getToggleState()
                                                      ? sampr::PlaybackMode::song
                                                      : sampr::PlaybackMode::pattern;

        juce::String error;

        if (! projectController.saveProject (file, error))
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Save failed",
                error);
            return;
        }

        updateStatusLabel();
    });
}

void MainComponent::loadProjectFromDisk()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load SAMPR project",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.sampr.json");

    const auto flags = juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (! file.existsAsFile())
            return;

        juce::String error;

        if (! projectController.loadProject (file, formatManager, error))
        {
            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                "Load failed",
                error);
            return;
        }

        const auto& settings = projectModel.getSettings();
        bpmSlider.setValue (settings.bpm, juce::dontSendNotification);
        gainSlider.setValue (settings.masterGain, juce::dontSendNotification);
        songModeButton.setToggleState (settings.playbackMode == sampr::PlaybackMode::song,
                                       juce::dontSendNotification);
        pushFloatCommand (sampr::AudioCommandType::setBpm, static_cast<float> (settings.bpm));
        pushFloatCommand (sampr::AudioCommandType::setMasterGain, settings.masterGain);
        lastSongPlaybackPatternIndex = -1;
        refreshProjectViews();
        sliceEditor.syncFromSelectedSlice();
    });
}