#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "ArrangementClip.h"
#include "Pattern.h"
#include "SampleRefData.h"

namespace sampr
{

enum class PlaybackMode
{
    pattern,
    song,
};

struct ProjectSettings
{
    juce::String projectName { "Untitled" };
    double bpm = 120.0;
    float masterGain = 1.0f;
    int arrangementLengthBars = 64;
    PlaybackMode playbackMode = PlaybackMode::pattern;
    int currentPatternIndex = 0;
    bool metronomeEnabled = false;
    bool loopEnabled = false;
    double loopStartBeats = 0.0;
    double loopEndBeats = 16.0;
};

class ProjectModel
{
public:
    static constexpr int kMaxPatterns = 8;
    static constexpr int kMaxArrangementClips = 128;

    ProjectModel();

    ProjectSettings& getSettings() noexcept { return settings; }
    const ProjectSettings& getSettings() const noexcept { return settings; }

    std::vector<Pattern>& getPatterns() noexcept { return patterns; }
    const std::vector<Pattern>& getPatterns() const noexcept { return patterns; }

    std::vector<ArrangementClip>& getArrangement() noexcept { return arrangement; }
    const std::vector<ArrangementClip>& getArrangement() const noexcept { return arrangement; }

    std::vector<SampleRefData>& getSampleRefs() noexcept { return sampleRefs; }
    const std::vector<SampleRefData>& getSampleRefs() const noexcept { return sampleRefs; }

    PatternId getNextPatternId() const noexcept { return nextPatternId; }
    NoteId getNextNoteId() const noexcept { return nextNoteId; }
    ClipId getNextClipId() const noexcept { return nextClipId; }

    PatternId allocatePatternId() noexcept;
    NoteId allocateNoteId() noexcept;
    ClipId allocateClipId() noexcept;
    juce::String allocateSampleRefId();

    void syncIdGeneratorsFromContent();

    Pattern* findPatternById (PatternId id) noexcept;
    const Pattern* findPatternById (PatternId id) const noexcept;
    int findPatternIndexById (PatternId id) const noexcept;

    ArrangementClip* findClipById (ClipId id) noexcept;
    const ArrangementClip* findClipById (ClipId id) const noexcept;

    int getArrangementEndBar() const noexcept;

    ProjectModel clone() const;
    void assignFrom (const ProjectModel& other);

private:
    ProjectSettings settings;
    std::vector<Pattern> patterns;
    std::vector<ArrangementClip> arrangement;
    std::vector<SampleRefData> sampleRefs;

    PatternId nextPatternId { 1 };
    NoteId nextNoteId { 1 };
    ClipId nextClipId { 1 };
    uint32_t nextSampleRefCounter { 1 };
};

} // namespace sampr