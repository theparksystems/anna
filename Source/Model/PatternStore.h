#pragma once

#include <functional>
#include <vector>

#include "../Audio/AudioEngine.h"
#include "../Audio/FxSnapshot.h"
#include "../Audio/NoteSnapshot.h"
#include "../Audio/PatternSnapshot.h"
#include "../Plugins/FxTypes.h"
#include "NoteEvent.h"
#include "Pattern.h"
#include "ProjectModel.h"
#include "SampleManager.h"

namespace sampr
{

class PatternStore
{
public:
    using ChangeCallback = std::function<void()>;

    PatternStore (ProjectModel& project, SampleManager& samples);

    void setChangeCallback (ChangeCallback callback);
    void setUndoCheckpoint (std::function<void()> callback);
    void onProjectStructureChanged();

    Pattern& getCurrentPattern();
    const Pattern& getCurrentPattern() const;

    int getCurrentPatternIndex() const noexcept;
    int getPatternCount() const noexcept { return static_cast<int> (project.getPatterns().size()); }
    const Pattern& getPattern (int index) const;

    void selectPattern (int index);
    void addPattern();
    void duplicateCurrentPattern();
    void setPatternName (int index, const juce::String& name);

    void setNumSteps (int numSteps);
    void toggleStep (int rowIndex, int stepIndex);
    void activateStep (int rowIndex, int stepIndex, float velocity);
    void setStepVelocity (int rowIndex, int stepIndex, float velocity);
    void setStepProbability (int rowIndex, int stepIndex, float probability);

    void setChannelGain (int rowIndex, float gain);
    void setChannelPan (int rowIndex, float pan);
    void setChannelMute (int rowIndex, bool muted);
    void setChannelSolo (int rowIndex, bool solo);
    void toggleChannelMute (int rowIndex);
    void toggleChannelSolo (int rowIndex);

    ChannelEqState getChannelEq (int rowIndex) const;
    void setChannelEqEnabled (int rowIndex, bool enabled);
    void setChannelEqBand (int rowIndex, EqBandKind band, float frequencyHz, float gainDb, float q);

    ChannelCompressorState getChannelCompressor (int rowIndex) const;
    void setChannelCompressorEnabled (int rowIndex, bool enabled);
    void setChannelCompressor (int rowIndex, const ChannelCompressorState& state);

    ChannelColorState getChannelColor (int rowIndex) const;
    void setChannelColorEnabled (int rowIndex, bool enabled);
    void setChannelColor (int rowIndex, const ChannelColorState& state);

    ChannelDelayState getChannelDelay (int rowIndex) const;
    void setChannelDelayEnabled (int rowIndex, bool enabled);
    void setChannelDelay (int rowIndex, const ChannelDelayState& state);

    ChannelReverbState getChannelReverb (int rowIndex) const;
    void setChannelReverbEnabled (int rowIndex, bool enabled);
    void setChannelReverb (int rowIndex, const ChannelReverbState& state);

    Pattern exportCurrentPattern() const;
    bool importPatternIntoCurrent (const Pattern& imported, bool replaceRows);

    int addRowFromAsset (AssetId assetId, int sliceIndex = 0);
    int addRowFromSelectedAsset();
    void removeRow (int rowIndex);
    void clearRow (int rowIndex);
    void clearPattern();
    void clearNotes();
    void syncRowsFromLoadedAssets();

    void setLengthBars (int bars);
    NoteId addNote (const NoteEvent& prototype);
    void updateNote (const NoteEvent& note);
    void deleteNote (NoteId noteId);
    NoteEvent* findNote (NoteId noteId);
    const NoteEvent* findNote (NoteId noteId) const;

    void publishSnapshot (AudioEngine& engine, double bpm, double sampleRate);
    void publishSnapshotForPattern (int patternIndex, AudioEngine& engine, double bpm, double sampleRate);

    int getCurrentStepForUi (const AudioEngine& engine) const noexcept;

private:
    void ensureStepArrays (Pattern& pattern);
    SharedPatternSnapshot buildSnapshotForPattern (int patternIndex, double bpm, double sampleRate) const;
    SharedNoteSnapshot buildNoteSnapshotForPattern (int patternIndex, double bpm, double sampleRate) const;
    SharedFxSnapshot buildFxSnapshotForPattern (int patternIndex) const;
    void notifyChanged();
    void saveCheckpoint();
    juce::String refIdForAsset (AssetId assetId) const;

    ProjectModel& project;
    SampleManager& sampleManager;
    ChangeCallback changeCallback;
    std::function<void()> undoCheckpoint;
};

} // namespace sampr
