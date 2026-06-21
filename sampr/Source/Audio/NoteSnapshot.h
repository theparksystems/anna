#pragma once

#include <array>
#include <memory>

#include "AudioTypes.h"
#include "SampleVoice.h"

namespace sampr
{

inline constexpr int kMaxScheduledNotes = 256;

struct ScheduledNoteEvent
{
    uint64_t startInPatternSamples = 0;
    SampleId sampleId = kInvalidSampleId;
    float pitchSemitones = 0.0f;
    float timeRatio = 1.0f;
    float velocity = 1.0f;
    VoicePlaybackMode mode = VoicePlaybackMode::raw;
};

struct NoteSnapshot
{
    uint64_t loopLengthSamples = 0;
    int numNotes = 0;
    std::array<ScheduledNoteEvent, kMaxScheduledNotes> notes {};
};

using SharedNoteSnapshot = std::shared_ptr<const NoteSnapshot>;

} // namespace sampr