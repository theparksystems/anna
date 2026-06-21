#pragma once

/**
 * SAMPR MVP integration map (Prompt 9)
 *
 * 1. SampleImportService  → SampleManager (decode + registry)
 * 2. Waveform + SliceEditor → slice metadata on SampleAsset
 * 3. PatternStore         → step seq + piano roll edits (undo via ProjectController)
 * 4. publishSnapshot()    → PatternSnapshot + NoteSnapshot → AudioEngine (atomic)
 * 5. AudioEngine::render  → PatternScheduler + NoteScheduler → VoicePool → Mixer
 * 6. MixerComponent       → per-row gain/pan/mute/solo in PatternRow
 * 7. Arrangement clips    → song mode selects active pattern snapshot
 * 8. OfflineExporter      → renderOffline() → WAV
 *
 * Threading: UI owns ProjectModel; audio thread reads immutable snapshots only.
 * AI hooks: mutate ProjectModel / PatternStore via ProjectController::beginEdit().
 */

#include "../Audio/AudioEngine.h"
#include "../Model/PatternStore.h"
#include "../Model/ProjectController.h"

namespace sampr
{

struct MvpFlow
{
    static void publishCurrentPattern (PatternStore& patterns,
                                       AudioEngine& engine,
                                       ProjectController& project,
                                       double bpm,
                                       double sampleRate,
                                       PlaybackMode mode,
                                       int fallbackPatternIndex);

    static int activePatternIndexForPlayback (ProjectController& project,
                                              PlaybackMode mode,
                                              double songBar,
                                              int editorPatternIndex);
};

} // namespace sampr