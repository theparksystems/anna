#pragma once

#include <functional>
#include <unordered_map>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Persistence/ProjectSerializer.h"
#include "PatternStore.h"
#include "ProjectModel.h"
#include "SampleManager.h"
#include "UndoHistory.h"

namespace sampr
{

class ProjectController
{
public:
    using ChangeCallback = std::function<void()>;

    ProjectController (ProjectModel& model,
                       SampleManager& samples,
                       PatternStore& patterns);

    void setChangeCallback (ChangeCallback callback);

    ProjectModel& getModel() noexcept { return project; }
    const ProjectModel& getModel() const noexcept { return project; }

    void beginEdit (const juce::String& description = "Edit");
    void endEdit();

    bool undo();
    bool redo();
    bool canUndo() const noexcept { return undoHistory.canUndo(); }
    bool canRedo() const noexcept { return undoHistory.canRedo(); }

    bool saveProject (const juce::File& file, juce::String& errorOut);
    bool loadProject (const juce::File& file, juce::AudioFormatManager& formatManager, juce::String& errorOut);

    void syncSampleRefsFromManager();
    void remapPatternAssetIds();

    ClipId addArrangementClip (PatternId patternId, double startBar, int lengthBars);
    void removeArrangementClip (ClipId clipId);
    void moveArrangementClip (ClipId clipId, double newStartBar);
    void setClipLength (ClipId clipId, int lengthBars);

    std::optional<int> getActivePatternIndexForSongBar (double songBar) const noexcept;
    const ArrangementClip* getActiveClipForSongBar (double songBar) const noexcept;

    void setPlaybackMode (PlaybackMode mode);
    PlaybackMode getPlaybackMode() const noexcept { return project.getSettings().playbackMode; }

    juce::File getProjectFile() const noexcept { return currentProjectFile; }

private:
    void notifyChanged();
    void applyLoadedModel (ProjectModel&& loaded, const juce::File& projectFile);
    juce::Colour colourForClip (PatternId patternId) const;

    ProjectModel& project;
    SampleManager& sampleManager;
    PatternStore& patternStore;
    UndoHistory undoHistory;
    ChangeCallback changeCallback;

    juce::File currentProjectFile;
    bool editInProgress = false;
    juce::String pendingEditDescription;
};

} // namespace sampr