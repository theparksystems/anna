#pragma once

#include <atomic>
#include <array>
#include <memory>

#include "AudioDiagnostics.h"
#include "NoteSnapshot.h"
#include "SampleRegistry.h"
#include "VoicePool.h"

namespace sampr
{

class NoteScheduler
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setSnapshot (SharedNoteSnapshot snapshot) noexcept;
    void setEnabled (bool shouldEnable) noexcept;
    void setMasterGain (float gain) noexcept;

    void process (uint64_t blockStartSamplePosition,
                  int numSamples,
                  bool transportPlaying,
                  VoicePool& voicePool,
                  const SampleRegistry& registry,
                  AudioDiagnostics& diagnostics) noexcept;

    double getPlayheadBeats (uint64_t transportSamplePosition, double bpm, double sampleRate) const noexcept;

private:
    std::atomic<SharedNoteSnapshot> snapshot;
    std::atomic<bool> enabled { true };
    std::atomic<float> masterGain { 1.0f };

    std::array<uint64_t, kMaxScheduledNotes> lastTriggeredLoop {};
    double deviceSampleRate = 44100.0;
};

} // namespace sampr