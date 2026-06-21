#include "ProjectModel.h"

#include <algorithm>

namespace sampr
{

ProjectModel::ProjectModel()
{
    Pattern initial;
    initial.id = allocatePatternId();
    initial.name = "Pattern 1";
    initial.numSteps = 16;
    patterns.push_back (std::move (initial));
}

PatternId ProjectModel::allocatePatternId() noexcept
{
    return nextPatternId++;
}

NoteId ProjectModel::allocateNoteId() noexcept
{
    return nextNoteId++;
}

ClipId ProjectModel::allocateClipId() noexcept
{
    return nextClipId++;
}

juce::String ProjectModel::allocateSampleRefId()
{
    return "sample-" + juce::String (nextSampleRefCounter++);
}

void ProjectModel::syncIdGeneratorsFromContent()
{
    PatternId maxPatternId = 0;
    NoteId maxNoteId = 0;
    ClipId maxClipId = 0;
    uint32_t maxSampleRefCounter = 0;

    for (const auto& pattern : patterns)
        maxPatternId = juce::jmax (maxPatternId, pattern.id);

    for (const auto& pattern : patterns)
    {
        for (const auto& note : pattern.notes)
            maxNoteId = juce::jmax (maxNoteId, note.id);
    }

    for (const auto& clip : arrangement)
        maxClipId = juce::jmax (maxClipId, clip.id);

    for (const auto& ref : sampleRefs)
    {
        if (ref.refId.startsWith ("sample-"))
        {
            const auto suffix = ref.refId.fromFirstOccurrenceOf ("sample-", false, false);
            maxSampleRefCounter = juce::jmax (maxSampleRefCounter,
                                              static_cast<uint32_t> (suffix.getIntValue()));
        }
    }

    nextPatternId = juce::jmax (nextPatternId, maxPatternId + 1);
    nextNoteId = juce::jmax (nextNoteId, maxNoteId + 1);
    nextClipId = juce::jmax (nextClipId, maxClipId + 1);
    nextSampleRefCounter = juce::jmax (nextSampleRefCounter, maxSampleRefCounter + 1);
}

Pattern* ProjectModel::findPatternById (PatternId id) noexcept
{
    const auto it = std::find_if (patterns.begin(), patterns.end(),
                                  [id] (const Pattern& p) { return p.id == id; });
    return it != patterns.end() ? &(*it) : nullptr;
}

const Pattern* ProjectModel::findPatternById (PatternId id) const noexcept
{
    const auto it = std::find_if (patterns.begin(), patterns.end(),
                                  [id] (const Pattern& p) { return p.id == id; });
    return it != patterns.end() ? &(*it) : nullptr;
}

int ProjectModel::findPatternIndexById (PatternId id) const noexcept
{
    for (int i = 0; i < static_cast<int> (patterns.size()); ++i)
        if (patterns[static_cast<size_t> (i)].id == id)
            return i;

    return -1;
}

ArrangementClip* ProjectModel::findClipById (ClipId id) noexcept
{
    const auto it = std::find_if (arrangement.begin(), arrangement.end(),
                                  [id] (const ArrangementClip& c) { return c.id == id; });
    return it != arrangement.end() ? &(*it) : nullptr;
}

const ArrangementClip* ProjectModel::findClipById (ClipId id) const noexcept
{
    const auto it = std::find_if (arrangement.begin(), arrangement.end(),
                                  [id] (const ArrangementClip& c) { return c.id == id; });
    return it != arrangement.end() ? &(*it) : nullptr;
}

int ProjectModel::getArrangementEndBar() const noexcept
{
    int endBar = 0;

    for (const auto& clip : arrangement)
        endBar = juce::jmax (endBar, static_cast<int> (std::ceil (clip.startBar + clip.lengthBars)));

    return juce::jmax (endBar, settings.arrangementLengthBars);
}

ProjectModel ProjectModel::clone() const
{
    ProjectModel copy;
    copy.assignFrom (*this);
    return copy;
}

void ProjectModel::assignFrom (const ProjectModel& other)
{
    settings = other.settings;
    patterns = other.patterns;
    arrangement = other.arrangement;
    sampleRefs = other.sampleRefs;
    nextPatternId = other.nextPatternId;
    nextNoteId = other.nextNoteId;
    nextClipId = other.nextClipId;
    nextSampleRefCounter = other.nextSampleRefCounter;
}

} // namespace sampr