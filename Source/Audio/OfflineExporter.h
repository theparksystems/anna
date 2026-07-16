#pragma once

#include <functional>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Model/PatternStore.h"
#include "../Model/ProjectController.h"
#include "../Model/ProjectModel.h"
#include "AudioEngine.h"

namespace sampr
{

struct OfflineExportOptions
{
    double sampleRate = 44100.0;
    int blockSize = 512;
    int bitDepth = 24;
    float outputGainDb = 0.0f;
    bool renderArrangement = false;
    PlaybackMode playbackMode = PlaybackMode::pattern;
};

struct OfflineExportResult
{
    bool success = false;
    juce::String errorMessage;
    juce::int64 samplesWritten = 0;
};

class OfflineExporter
{
public:
    using ProgressCallback = std::function<void (float progress)>;
    using CancelCallback = std::function<bool()>;

    static OfflineExportResult exportToWav (const juce::File& destination,
                                            AudioEngine& engine,
                                            PatternStore& patternStore,
                                            ProjectController& projectController,
                                            const ProjectModel& project,
                                            const OfflineExportOptions& options,
                                            ProgressCallback progress = nullptr,
                                            CancelCallback shouldCancel = nullptr);

private:
    static uint64_t computeRenderLengthSamples (const ProjectModel& project,
                                                const OfflineExportOptions& options,
                                                double bpm,
                                                double sampleRate,
                                                int patternIndex);

    static void publishSnapshotsForPosition (PatternStore& patternStore,
                                             AudioEngine& engine,
                                             ProjectController& projectController,
                                             const ProjectModel& project,
                                             double bpm,
                                             double sampleRate,
                                             uint64_t samplePosition,
                                             const OfflineExportOptions& options);
};

} // namespace sampr
