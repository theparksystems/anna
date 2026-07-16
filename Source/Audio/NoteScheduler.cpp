#include "NoteScheduler.h"

#include <limits>

namespace sampr
{

void NoteScheduler::prepare (double sampleRate) noexcept
{
    deviceSampleRate = sampleRate;
    reset();
}

void NoteScheduler::reset() noexcept
{
    lastTriggeredLoop.fill (std::numeric_limits<uint64_t>::max());
}

void NoteScheduler::setSnapshot (SharedNoteSnapshot newSnapshot) noexcept
{
    snapshot.store (std::move (newSnapshot), std::memory_order_release);
    reset();
}

void NoteScheduler::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

void NoteScheduler::setMasterGain (float gain) noexcept
{
    masterGain.store (juce::jlimit (0.0f, 2.0f, gain), std::memory_order_relaxed);
}

double NoteScheduler::getPlayheadBeats (uint64_t transportSamplePosition,
                                        double bpm,
                                        double sampleRate) const noexcept
{
    if (sampleRate <= 0.0 || bpm <= 0.0)
        return 0.0;

    const auto seconds = static_cast<double> (transportSamplePosition) / sampleRate;
    return seconds * bpm / 60.0;
}

void NoteScheduler::process (uint64_t blockStartSamplePosition,
                             int numSamples,
                             bool transportPlaying,
                             VoicePool& voicePool,
                             const SampleRegistry& registry,
                             AudioDiagnostics& diagnostics) noexcept
{
    juce::ignoreUnused (deviceSampleRate);

    if (numSamples <= 0 || ! transportPlaying || ! enabled.load (std::memory_order_relaxed))
        return;

    const auto snap = snapshot.load (std::memory_order_acquire);

    if (snap == nullptr || snap->numNotes <= 0 || snap->loopLengthSamples == 0)
        return;

    const auto loopLength = snap->loopLengthSamples;
    const auto blockEnd = blockStartSamplePosition + static_cast<uint64_t> (numSamples);
    const auto gain = masterGain.load (std::memory_order_relaxed);

    const auto firstLoop = blockStartSamplePosition / loopLength;
    const auto lastLoop = (blockEnd - 1) / loopLength;

    for (uint64_t loopIndex = firstLoop; loopIndex <= lastLoop; ++loopIndex)
    {
        const auto loopStart = loopIndex * loopLength;

        for (int noteIndex = 0; noteIndex < snap->numNotes; ++noteIndex)
        {
            const auto& note = snap->notes[static_cast<size_t> (noteIndex)];

            if (note.sampleId == kInvalidSampleId)
                continue;

            const auto noteSample = loopStart + note.startInPatternSamples;

            if (noteSample < blockStartSamplePosition || noteSample >= blockEnd)
                continue;

            if (lastTriggeredLoop[static_cast<size_t> (noteIndex)] == loopIndex)
                continue;

            voicePool.triggerSample (registry,
                                     note.sampleId,
                                     note.velocity,
                                     gain,
                                     0.0f,
                                     note.pitchSemitones,
                                     note.timeRatio,
                                     false,
                                     note.mode,
                                     -1,
                                     diagnostics);

            lastTriggeredLoop[static_cast<size_t> (noteIndex)] = loopIndex;
        }
    }
}

} // namespace sampr
