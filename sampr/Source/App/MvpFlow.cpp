#include "MvpFlow.h"

namespace sampr
{

void MvpFlow::publishCurrentPattern (PatternStore& patterns,
                                    AudioEngine& engine,
                                    ProjectController& project,
                                    double bpm,
                                    double sampleRate,
                                    PlaybackMode mode,
                                    int fallbackPatternIndex)
{
    if (sampleRate <= 0.0)
        return;

    if (mode == PlaybackMode::song)
    {
        const auto beats = engine.getPlayheadBeats (bpm);
        const auto songBar = beats / 4.0;

        if (const auto active = project.getActivePatternIndexForSongBar (songBar))
        {
            patterns.publishSnapshotForPattern (*active, engine, bpm, sampleRate);
            return;
        }

        engine.setPatternSnapshot (std::make_shared<PatternSnapshot>());
        engine.setNoteSnapshot (std::make_shared<NoteSnapshot>());
        return;
    }

    juce::ignoreUnused (fallbackPatternIndex);
    patterns.publishSnapshot (engine, bpm, sampleRate);
}

int MvpFlow::activePatternIndexForPlayback (ProjectController& project,
                                            PlaybackMode mode,
                                            double songBar,
                                            int editorPatternIndex)
{
    if (mode == PlaybackMode::song)
    {
        if (const auto active = project.getActivePatternIndexForSongBar (songBar))
            return *active;
    }

    return editorPatternIndex;
}

} // namespace sampr