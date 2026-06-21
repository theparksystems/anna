#include "SampleImportService.h"

namespace sampr
{

namespace
{
    const juce::StringArray& supportedExtensions()
    {
        static const juce::StringArray extensions { ".wav", ".aiff", ".aif", ".flac", ".ogg", ".mp3" };
        return extensions;
    }

    bool hasAudioExtension (const juce::File& file)
    {
        return supportedExtensions().contains (file.getFileExtension().toLowerCase());
    }
}

juce::String SampleImportService::getSupportedFileChooserPatterns()
{
    juce::String patterns;

    for (const auto& ext : supportedExtensions())
        patterns << "*" << ext << ";";

    return patterns.dropLastCharacters (1);
}

bool SampleImportService::isSupportedAudioFile (const juce::File& file)
{
    return file.existsAsFile() && hasAudioExtension (file);
}

SampleImportService::ImportResult SampleImportService::importFiles (
    const juce::StringArray& paths,
    ProjectModel& project,
    SampleManager& samples,
    juce::AudioFormatManager& formats)
{
    ImportResult result;

    for (const auto& path : paths)
    {
        const juce::File file (path);

        if (! isSupportedAudioFile (file))
            continue;

        const auto assetId = samples.loadFromFile (file, formats);

        if (! assetId.has_value())
        {
            ++result.failedCount;
            result.lastError = "Could not decode: " + file.getFullPathName();
            continue;
        }

        if (auto* asset = samples.getAsset (*assetId); asset != nullptr && asset->refId.isEmpty())
            asset->refId = project.allocateSampleRefId();

        ++result.loadedCount;
    }

    return result;
}

} // namespace sampr