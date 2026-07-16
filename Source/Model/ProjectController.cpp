#include "ProjectController.h"

#include <algorithm>
#include <cmath>

namespace sampr
{

ProjectController::ProjectController (ProjectModel& model,
                                      SampleManager& samples,
                                      PatternStore& patterns)
    : project (model),
      sampleManager (samples),
      patternStore (patterns)
{
}

void ProjectController::setChangeCallback (ChangeCallback callback)
{
    changeCallback = std::move (callback);
}

void ProjectController::notifyChanged()
{
    if (changeCallback != nullptr)
        changeCallback();
}

void ProjectController::beginEdit (const juce::String& description)
{
    undoHistory.saveUndoPoint (project);
    pendingEditDescription = description;
    editInProgress = true;
}

void ProjectController::endEdit()
{
    editInProgress = false;
    juce::ignoreUnused (pendingEditDescription);
    pendingEditDescription.clear();
    notifyChanged();
}

bool ProjectController::undo()
{
    if (! undoHistory.undo (project))
        return false;

    remapPatternAssetIds();
    patternStore.onProjectStructureChanged();
    notifyChanged();
    return true;
}

bool ProjectController::redo()
{
    if (! undoHistory.redo (project))
        return false;

    remapPatternAssetIds();
    patternStore.onProjectStructureChanged();
    notifyChanged();
    return true;
}

bool ProjectController::saveProject (const juce::File& file, juce::String& errorOut)
{
    syncSampleRefsFromManager();

    if (! ProjectSerializer::saveToFile (project, file, errorOut))
        return false;

    currentProjectFile = file;
    return true;
}

bool ProjectController::loadProject (const juce::File& file,
                                     juce::AudioFormatManager& formatManager,
                                     juce::String& errorOut)
{
    const auto result = ProjectSerializer::loadFromFile (file);

    if (! result.success)
    {
        errorOut = result.errorMessage;
        return false;
    }

    sampleManager.clear();
    const auto refMap = sampleManager.loadSamplesFromRefs (result.model.getSampleRefs(),
                                                           result.projectFile,
                                                           formatManager);

    applyLoadedModel (result.model.clone(), result.projectFile);
    sampleManager.remapPatternAssetIds (project, refMap);
    undoHistory.clear();
    notifyChanged();
    return true;
}

void ProjectController::syncSampleRefsFromManager()
{
    for (const auto assetId : sampleManager.getAssetIds())
    {
        if (auto* asset = sampleManager.getAsset (assetId); asset != nullptr && asset->refId.isEmpty())
            asset->refId = project.allocateSampleRefId();
    }

    sampleManager.exportSampleRefs (project.getSampleRefs(), currentProjectFile);
}

void ProjectController::remapPatternAssetIds()
{
    const auto refMap = sampleManager.buildRefIdToAssetMap();
    sampleManager.remapPatternAssetIds (project, refMap);
}

void ProjectController::applyLoadedModel (ProjectModel&& loaded, const juce::File& projectFile)
{
    project.assignFrom (loaded);
    currentProjectFile = projectFile;
    patternStore.onProjectStructureChanged();
}

ClipId ProjectController::addArrangementClip (PatternId patternId, double startBar, int lengthBars)
{
    if (static_cast<int> (project.getArrangement().size()) >= ProjectModel::kMaxArrangementClips)
        return kInvalidClipId;

    if (project.findPatternById (patternId) == nullptr)
        return kInvalidClipId;

    beginEdit ("Add arrangement clip");

    ArrangementClip clip;
    clip.id = project.allocateClipId();
    clip.patternId = patternId;
    clip.startBar = juce::jmax (0.0, startBar);
    clip.lengthBars = juce::jlimit (1, 64, lengthBars);
    clip.colour = colourForClip (patternId);

    if (const auto* pattern = project.findPatternById (patternId))
        clip.name = pattern->name;

    project.getArrangement().push_back (clip);
    endEdit();
    return clip.id;
}

void ProjectController::removeArrangementClip (ClipId clipId)
{
    auto& clips = project.getArrangement();
    const auto it = std::find_if (clips.begin(), clips.end(),
                                  [clipId] (const ArrangementClip& c) { return c.id == clipId; });

    if (it == clips.end())
        return;

    beginEdit ("Remove arrangement clip");
    clips.erase (it);
    endEdit();
}

void ProjectController::moveArrangementClip (ClipId clipId, double newStartBar)
{
    if (auto* clip = project.findClipById (clipId))
    {
        const auto start = juce::jmax (0.0, newStartBar);

        if (std::abs (clip->startBar - start) < 0.0001)
            return;

        beginEdit ("Move arrangement clip");
        clip->startBar = start;
        endEdit();
    }
}

void ProjectController::previewArrangementClipMove (ClipId clipId, double newStartBar)
{
    if (auto* clip = project.findClipById (clipId))
        clip->startBar = juce::jmax (0.0, newStartBar);
}

void ProjectController::setClipLength (ClipId clipId, int lengthBars)
{
    if (auto* clip = project.findClipById (clipId))
    {
        beginEdit ("Resize arrangement clip");
        clip->lengthBars = juce::jlimit (1, 64, lengthBars);
        endEdit();
    }
}

std::optional<int> ProjectController::getActivePatternIndexForSongBar (double songBar) const noexcept
{
    if (const auto* clip = getActiveClipForSongBar (songBar))
        return project.findPatternIndexById (clip->patternId);

    return std::nullopt;
}

const ArrangementClip* ProjectController::getActiveClipForSongBar (double songBar) const noexcept
{
    const ArrangementClip* best = nullptr;
    double bestStart = -1.0;

    for (const auto& clip : project.getArrangement())
    {
        const auto endBar = clip.startBar + static_cast<double> (clip.lengthBars);

        if (songBar >= clip.startBar && songBar < endBar && clip.startBar >= bestStart)
        {
            best = &clip;
            bestStart = clip.startBar;
        }
    }

    return best;
}

void ProjectController::setPlaybackMode (PlaybackMode mode)
{
    if (project.getSettings().playbackMode == mode)
        return;

    beginEdit ("Change playback mode");
    project.getSettings().playbackMode = mode;
    endEdit();
}

juce::Colour ProjectController::colourForClip (PatternId patternId) const
{
    static constexpr juce::uint32 colours[] {
        0xff4cc2ff, 0xffff6b6b, 0xffa3e635, 0xffffb347,
        0xffc084fc, 0xff22d3ee, 0xfff472b6, 0xff94a3b8
    };

    const auto index = project.findPatternIndexById (patternId);
    const auto colourIndex = index < 0 ? 0 : index;
    return juce::Colour (colours[static_cast<size_t> (colourIndex) % 8]);
}

} // namespace sampr
