#include "OfflineExporter.h"

namespace sampr
{

namespace
{
    double beatsForSamplePosition (uint64_t samplePosition, double bpm, double sampleRate)
    {
        if (sampleRate <= 0.0 || bpm <= 0.0)
            return 0.0;

        return static_cast<double> (samplePosition) / sampleRate * bpm / 60.0;
    }
}

uint64_t OfflineExporter::computeRenderLengthSamples (const ProjectModel& project,
                                                      const OfflineExportOptions& options,
                                                      double bpm,
                                                      double sampleRate,
                                                      int patternIndex)
{
    if (options.renderArrangement || options.playbackMode == PlaybackMode::song)
    {
        const auto endBar = project.getArrangementEndBar();
        const auto beats = static_cast<double> (juce::jmax (1, endBar)) * 4.0;
        return static_cast<uint64_t> (beats * sampleRate * 60.0 / bpm);
    }

    if (! juce::isPositiveAndBelow (patternIndex, static_cast<int> (project.getPatterns().size())))
        return 0;

    const auto& pattern = project.getPatterns()[static_cast<size_t> (patternIndex)];
    const auto beats = static_cast<double> (pattern.lengthBars) * 4.0;
    return static_cast<uint64_t> (beats * sampleRate * 60.0 / bpm);
}

void OfflineExporter::publishSnapshotsForPosition (PatternStore& patternStore,
                                                   AudioEngine& engine,
                                                   ProjectController& projectController,
                                                   const ProjectModel& project,
                                                   double bpm,
                                                   double sampleRate,
                                                   uint64_t samplePosition,
                                                   const OfflineExportOptions& options)
{
    if (options.renderArrangement || options.playbackMode == PlaybackMode::song)
    {
        const auto songBar = beatsForSamplePosition (samplePosition, bpm, sampleRate) / 4.0;

        if (const auto activeIndex = projectController.getActivePatternIndexForSongBar (songBar))
        {
            patternStore.publishSnapshotForPattern (*activeIndex, engine, bpm, sampleRate);
            return;
        }

        engine.setPatternSnapshot (std::make_shared<PatternSnapshot>());
        engine.setNoteSnapshot (std::make_shared<NoteSnapshot>());
        return;
    }

    patternStore.publishSnapshot (engine, bpm, sampleRate);
}

OfflineExportResult OfflineExporter::exportToWav (const juce::File& destination,
                                                AudioEngine& engine,
                                                PatternStore& patternStore,
                                                ProjectController& projectController,
                                                const ProjectModel& project,
                                                const OfflineExportOptions& options,
                                                ProgressCallback progress)
{
    OfflineExportResult result;

    if (destination.existsAsFile() && ! destination.deleteFile())
    {
        result.errorMessage = "Could not overwrite destination file.";
        return result;
    }

    const auto bpm = project.getSettings().bpm;
    const auto patternIndex = project.getSettings().currentPatternIndex;
    const auto totalSamples = computeRenderLengthSamples (project, options, bpm, options.sampleRate, patternIndex);

    if (totalSamples == 0)
    {
        result.errorMessage = "Nothing to render.";
        return result;
    }

    engine.setOfflineRenderActive (true);
    engine.pushCommand ({ .type = AudioCommandType::transportStop });
    engine.pushCommand ({ .type = AudioCommandType::setMetronomeEnabled, .boolValue = false });
    engine.pushCommand ({ .type = AudioCommandType::transportPlay });
    engine.prepare (options.sampleRate, options.blockSize);

    publishSnapshotsForPosition (patternStore, engine, projectController, project, bpm,
                                 options.sampleRate, 0, options);

    juce::AudioBuffer<float> buffer (2, options.blockSize);
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer;

    {
        auto stream = destination.createOutputStream();

        if (stream == nullptr)
        {
            result.errorMessage = "Could not open output stream.";
            engine.pushCommand ({ .type = AudioCommandType::transportStop });
            return result;
        }

        writer.reset (wavFormat.createWriterFor (stream.release(),
                                               options.sampleRate,
                                               static_cast<unsigned int> (buffer.getNumChannels()),
                                               24,
                                               {},
                                               0));

        if (writer == nullptr)
        {
            result.errorMessage = "Could not create WAV writer.";
            engine.pushCommand ({ .type = AudioCommandType::transportStop });
            engine.setOfflineRenderActive (false);
            return result;
        }
    }

    uint64_t rendered = 0;
    uint64_t lastSnapshotPosition = 0;

    while (rendered < totalSamples)
    {
        const auto block = static_cast<int> (juce::jmin<uint64_t> (static_cast<uint64_t> (options.blockSize),
                                                                   totalSamples - rendered));

        if (rendered / static_cast<uint64_t> (options.blockSize) != lastSnapshotPosition)
        {
            publishSnapshotsForPosition (patternStore, engine, projectController, project, bpm,
                                         options.sampleRate, rendered, options);
            lastSnapshotPosition = rendered / static_cast<uint64_t> (options.blockSize);
        }

        buffer.clear();
        engine.renderOffline (buffer, 0, block);

        if (! writer->writeFromAudioSampleBuffer (buffer, 0, block))
        {
            result.errorMessage = "Failed while writing audio.";
            engine.pushCommand ({ .type = AudioCommandType::transportStop });
            engine.setOfflineRenderActive (false);
            return result;
        }

        rendered += static_cast<uint64_t> (block);

        if (progress != nullptr)
            progress (static_cast<float> (static_cast<double> (rendered) / static_cast<double> (totalSamples)));
    }

    writer.reset();
    engine.pushCommand ({ .type = AudioCommandType::transportStop });
    engine.setOfflineRenderActive (false);

    result.success = true;
    result.samplesWritten = static_cast<juce::int64> (rendered);
    return result;
}

} // namespace sampr